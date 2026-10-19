#pragma once
#ifndef EJDB2_INTERNAL_H
#define EJDB2_INTERNAL_H

/**************************************************************************************************
 * EJDB2
 *
 * MIT License
 *
 * Copyright (c) 2012-2022 Softmotions Ltd <info@softmotions.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *************************************************************************************************/

#include "ejdb2.h"

#include <iowow/iwkv.h>
#include <iowow/iwxstr.h>
#include <iowow/iwexfile.h>
#include <iowow/iwutils.h>
#include <iowow/iwhmap.h>
#include <iowow/iwjson.h>

#include "ejdb2cfg.h"

#ifdef JB_HTTP
#include "jbr.h"
#endif

#include "jql_internal.h"

#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <setjmp.h>

#define METADB_ID           1
#define NUMRECSDB_ID        2    // DB for number of records per index/collection
#define KEY_PREFIX_COLLMETA "c." // Full key format: c.<coldbid>
#define KEY_PREFIX_IDXMETA  "i." // Full key format: i.<coldbid>.<idxdbid>

#define ENSURE_OPEN(db_)                        \
        if (!(db_) || !((db_)->open)) {         \
          iwlog_error2("Database is not open"); \
          return IW_ERROR_INVALID_STATE;        \
        }

#define API_RLOCK(db_, rci_)                       \
        ENSURE_OPEN(db_);                          \
        rci_ = pthread_rwlock_rdlock(&(db_)->rwl); \
        if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_WLOCK(db_, rci_)                       \
        ENSURE_OPEN(db_);                          \
        rci_ = pthread_rwlock_wrlock(&(db_)->rwl); \
        if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_WLOCK2(db_, rci_)                      \
        rci_ = pthread_rwlock_wrlock(&(db_)->rwl); \
        if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_UNLOCK(db_, rci_, rc_)                 \
        rci_ = pthread_rwlock_unlock(&(db_)->rwl); \
        if (rci_) IWRC(iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_), rc_)

#define API_COLL_UNLOCK(jbc_, rci_, rc_)                                       \
        do {                                                                   \
          rci_ = pthread_rwlock_unlock(&(jbc_)->rwl);                          \
          if (rci_) IWRC(iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_), rc_); \
          API_UNLOCK((jbc_)->db, rci_, rc_);                                   \
        } while (0)

struct jbidx;
typedef struct jbidx*JBIDX;

/** Database collection */
typedef struct jbcoll {
  uint32_t      dbid;       /**< IWKV collection database ID */
  const char   *name;       /**< Collection name */
  struct iwdb  *cdb;        /**< IWKV collection database */
  struct ejdb  *db;         /**< Main database reference */
  struct jbl   *meta;       /**< Collection meta object */
  struct jbidx *idx;        /**< First index in chain */
  int64_t       rnum;       /**< Number of records stored in collection */
  pthread_rwlock_t rwl;
  int64_t id_seq;
} *JBCOLL;

/** Database collection index */
struct jbidx {
  struct jbidx *next;   /**< Next index in chain */
  int64_t       rnum;   /**< Number of records stored in index */
  struct jbcoll  *jbc;  /**< Owner document collection */
  struct jbl_ptr *ptr;  /**< Indexed JSON path poiner 0*/
  struct iwdb    *idb;  /**< KV database for this index */
  uint32_t dbid;        /**< IWKV collection database ID */
  ejdb_idx_mode_t mode; /**< Index mode/type mask */
  iwdb_flags_t    idbf; /**< Index database flags */
};

/** Pair: collection name, document id */
struct jbdocref {
  int64_t     id;
  const char *coll;
};

struct ejdb {
  struct iwkv *iwkv;
  struct iwdb *metadb;
  struct iwdb *nrecdb;
#ifdef JB_HTTP
  struct jbr *jbr;
#endif
  struct iwhmap   *mcolls;
  iwkv_openflags   oflags;
  pthread_rwlock_t rwl;      /**< Main RWL */
  struct ejdb_opts opts;
  volatile bool    open;
};

struct _jb_put_handler_ctx {
  int64_t id;
  struct jbcoll  *jbc;
  struct jbl     *jbl;
  struct iwkv_val oldval;
};

struct jbexec;

/**
 * @brief Document scan consumer callback.
 *
 * A scanner invokes this callback for every document id to be processed and once more
 * with `id` set to zero at the end of the scan in order to finalize processing and
 * report the accumulated error.
 *
 * @param ctx     Query execution context. Not zero.
 * @param cur     Index cursor positioned at the scanned index entry,
 *                or zero if the document should be fetched by its `id`.
 * @param id      Document id. Zero means the end of the scan.
 * @param step    [in,out] Number of records to skip before the next callback invocation.
 *                Set to zero to finish the scan or to a negative value to continue
 *                scanning in the reverse direction.
 * @param matched [out] Set to `true` if the document matches the query.
 * @param err     Error accumulated by the scanner. Used only in the final call where `id` is zero.
 * @return `0` on success or an error code.
 */
typedef iwrc (*jb_scan_consumer)(
  struct jbexec *ctx, struct iwkv_cursor *cur, int64_t id,
  int64_t *step, bool *matched, iwrc err);

/**
 * @brief Index scan sorter consumer context
 */
struct jbssc {
  iwrc      rc;               /**< RC code used for in `_jb_do_sorting` */
  uint32_t *refs;             /**< Document references array */
  uint32_t  refs_asz;         /**< Document references array allocated size */
  uint32_t  refs_num;         /**< Document references array elements count */
  uint32_t  docs_asz;         /**< Documents array allocated size */
  uint8_t  *docs;             /**< Documents byte array */
  uint32_t  docs_npos;        /**< Next document offset */
  jmp_buf   fatal_jmp;
  struct iwfs_ext sof;              /**< Sort overflow file */
  bool sof_active;
};

struct jbmidx {
  struct jbidx      *idx;             /**< Index matched this filter */
  struct jqp_filter *filter;          /**< Query filter */
  struct jqp_expr   *nexpr;           /**< Filter node expression */
  struct jqp_expr   *expr1;           /**< Start index expression (optional) */
  struct jqp_expr   *expr2;           /**< End index expression (optional) */
  enum iwkv_cursor_op cursor_init;    /**< Initial index cursor position (optional) */
  enum iwkv_cursor_op cursor_step;    /**< Next index cursor step */
  bool orderby_support;               /**< Index supported first order-by clause */
};

typedef struct jbexec {
  struct ejdb_exec *ux;           /**< User defined context */
  struct jbcoll    *jbc;          /**< Collection */

  int64_t istep;
  iwrc (*scanner)(
    struct jbexec   *ctx,
    jb_scan_consumer consumer); /**< Scanner used to produce the stream of collection documents */
  uint8_t *jblbuf;                 /**< Buffer used to keep currently processed document */
  size_t   jblbufsz;               /**< Size of jblbuf allocated memory */
  bool     sorting;                /**< Resultset sorting needed */
  enum iwkv_cursor_op cursor_init; /**< Initial index cursor position (optional) */
  enum iwkv_cursor_op cursor_step; /**< Next index cursor step */
  struct jbmidx midx;              /**< Index matching context */
  struct jbssc  ssc;               /**< Result set sorting context */

  // JQL joned nodes cache
  struct iwhmap *proj_joined_nodes_cache;
  struct iwpool *proj_joined_nodes_pool;
} JBEXEC;


typedef uint8_t jb_coll_acquire_t;
#define JB_COLL_ACQUIRE_WRITE    ((jb_coll_acquire_t) 0x01U)
#define JB_COLL_ACQUIRE_EXISTING ((jb_coll_acquire_t) 0x02U)

// Index selector empiric constants
#define JB_IDX_EMPIRIC_MAX_INOP_ARRAY_SIZE  500
#define JB_IDX_EMPIRIC_MIN_INOP_ARRAY_SIZE  10
#define JB_IDX_EMPIRIC_MAX_INOP_ARRAY_RATIO 200

/**
 * @brief Builds an index key from a `jbl` document value.
 *
 * The key representation is defined by the index mode (`idx->mode`).
 * The resulting key may reference the document memory or the stack allocated `numbuf`,
 * so it is valid only while they are alive. No memory is allocated.
 *
 * @param idx    Index definition. Not zero.
 * @param jbv    Document value to be indexed. Not zero.
 * @param ikey   [out] Index key. Its `size` and `data` are set to zero if the value cannot be indexed.
 * @param numbuf Scratch buffer of `IWNUMBUF_SIZE` bytes used for numeric conversions.
 */
void jbi_jbl_fill_ikey(struct jbidx *idx, struct jbl *jbv, struct iwkv_val *ikey, char numbuf[static IWNUMBUF_SIZE]);

/**
 * @brief Builds an index key from a query value.
 *
 * Same as `jbi_jbl_fill_ikey()` but the key is produced from the query operand `jqval`.
 *
 * @param idx    Index definition. Not zero.
 * @param jqval  Query value to be converted. Not zero.
 * @param ikey   [out] Index key. Its `size` and `data` are set to zero if the value cannot be indexed.
 * @param numbuf Scratch buffer of `IWNUMBUF_SIZE` bytes used for numeric conversions.
 */
void jbi_jqval_fill_ikey(
  struct jbidx *idx, const struct jqval *jqval, struct iwkv_val *ikey,
  char numbuf[static IWNUMBUF_SIZE]);

/**
 * @brief Builds an index key from a DOM node of a document.
 *
 * Same as `jbi_jbl_fill_ikey()` but the key is produced from the parsed document node
 * without accessing the serialized document data.
 *
 * @param idx    Index definition. Not zero.
 * @param node   Document node to be indexed. Not zero.
 * @param ikey   [out] Index key. Its `size` and `data` are set to zero if the value cannot be indexed.
 * @param numbuf Scratch buffer of `IWNUMBUF_SIZE` bytes used for numeric conversions.
 */
void jbi_node_fill_ikey(
  struct jbidx *idx, struct jbl_node *node, struct iwkv_val *ikey,
  char numbuf[static IWNUMBUF_SIZE]);

/**
 * @brief Scan consumer which evaluates the query and passes matched documents to the visitor.
 *
 * For every scanned document id the document is fetched (from `cur` or by `id`), matched against
 * the query, and optionally updated or deleted by the `apply` clause or served with projection.
 * Documents matching the query are reported to the `ejdb_exec` visitor while respecting the
 * `skip` and `limit` options. Invoked with `id` set to zero at the end of the scan.
 *
 * @param ctx     Query execution context. Not zero.
 * @param cur     Index cursor positioned at the scanned index entry, or zero.
 * @param id      Document id. Zero means the end of the scan.
 * @param step    [in,out] Number of records to skip before the next callback invocation.
 * @param matched [out] Set to `true` if the document matches the query.
 * @param err     Error accumulated by the scanner, reported on the final call.
 * @return `0` on success or an error code.
 */
iwrc jbi_consumer(struct jbexec *ctx, struct iwkv_cursor *cur, int64_t id, int64_t *step, bool *matched, iwrc err);

/**
 * @brief Scan consumer which sorts the result set before it is passed to the visitor.
 *
 * Matched documents are collected into the in-memory sort buffer and, when it overflows, into
 * a temporary overflow file. Once the scan is finished (`id` is zero) the collected documents
 * are sorted according to the `order by` clause and passed to the visitor. If the scanner
 * reports an error the collected data is released without sorting.
 *
 * @param ctx     Query execution context. Not zero.
 * @param cur     Index cursor positioned at the scanned index entry, or zero.
 * @param id      Document id. Zero means the end of the scan.
 * @param step    [in,out] Number of records to skip before the next callback invocation.
 * @param matched [out] Set to `true` if the document matches the query.
 * @param err     Error accumulated by the scanner, reported on the final call.
 * @return `0` on success or an error code.
 */
iwrc jbi_sorter_consumer(
  struct jbexec *ctx, struct iwkv_cursor *cur, int64_t id, int64_t *step, bool *matched,
  iwrc err);

/**
 * @brief Scans all documents of the collection.
 *
 * Uses the collection database cursor positioned by `ctx->cursor_init` and moved by
 * `ctx->cursor_step`, and invokes `consumer` for every document id.
 *
 * @param ctx      Query execution context. Not zero.
 * @param consumer Document scan consumer. Not zero.
 * @return `0` on success or an error code.
 */
iwrc jbi_full_scanner(struct jbexec *ctx, jb_scan_consumer consumer);

/**
 * @brief Selects an index to be used for the query execution.
 *
 * Analyzes the query expressions against the collection indexes and fills `ctx->midx` with
 * the selected index, its boundary expressions and the initial cursor position and step.
 * If no suitable index is found `ctx->midx.idx` is left zero so the full collection scan
 * is used. The result set sorting requirement (`ctx->sorting`) is updated accordingly.
 *
 * @param ctx Query execution context. Not zero.
 * @return `0` on success or an error code.
 */
iwrc jbi_selection(struct jbexec *ctx);

/**
 * @brief Scans documents selected by the primary key expression.
 *
 * Extracts the primary key operand (a single id or an array of ids) from the query AST
 * and invokes `consumer` for each id.
 *
 * @param ctx      Query execution context. Not zero.
 * @param consumer Document scan consumer. Not zero.
 * @return `0` on success or an error code.
 */
iwrc jbi_pk_scanner(struct jbexec *ctx, jb_scan_consumer consumer);

/**
 * @brief Scans documents using the selected unique index.
 *
 * Converts the index boundary expression `ctx->midx.expr1` into a query value and iterates
 * the index database using an equality (`EQ`), `IN` or range scan. Invokes `consumer` for
 * every matched document id.
 *
 * @param ctx      Query execution context with a selected index. Not zero.
 * @param consumer Document scan consumer. Not zero.
 * @return `0` on success or an error code.
 */
iwrc jbi_uniq_scanner(struct jbexec *ctx, jb_scan_consumer consumer);

/**
 * @brief Scans documents using the selected non-unique (compound keys) index.
 *
 * Same as `jbi_uniq_scanner()` but a single index key may reference multiple documents,
 * so duplicate document ids produced by the index scan are filtered out before `consumer`
 * is invoked.
 *
 * @param ctx      Query execution context with a selected index. Not zero.
 * @param consumer Document scan consumer. Not zero.
 * @return `0` on success or an error code.
 */
iwrc jbi_dup_scanner(struct jbexec *ctx, jb_scan_consumer consumer);

/**
 * @brief Checks whether the index key at the cursor position matches the given expression.
 *
 * Reads the key from `cur` and compares it with the right operand of `expr` using the
 * expression operator. Used to filter index ranges which cannot be expressed directly
 * by the cursor position, for example `IN` lists containing multiple values.
 *
 * @param aux  Query auxiliary data. Not zero.
 * @param idx  Index definition. Not zero.
 * @param cur  Index cursor positioned at the entry. Not zero.
 * @param expr Filter expression. Not zero.
 * @param rcp  [out] Error code. Not zero.
 * @return `true` if the index key matches the expression.
 */
bool jbi_node_expr_matched(
  struct jqp_aux     *aux,
  struct jbidx       *idx,
  struct iwkv_cursor *cur,
  struct jqp_expr    *expr,
  iwrc               *rcp);

/**
 * @brief Fetches a document from the collection by its id.
 *
 * Database and collection read locks are acquired and released internally.
 *
 * @param db   Database handle. Not zero.
 * @param coll Collection name. Not zero.
 * @param id   Document id.
 * @param acm  Collection acquisition mode, see `JB_COLL_ACQUIRE_*`.
 * @param jblp [out] Holder of the fetched document. Not zero.
 *             The document is a view of the fetched database value and should be disposed
 *             of by `jbl_destroy()`.
 * @return `0` on success, `IWKV_ERROR_NOTFOUND` if the document is not found,
 *         `IW_ERROR_NOT_EXISTS` if the collection does not exist,
 *         or another error code.
 */
iwrc jb_get(struct ejdb *db, const char *coll, int64_t id, jb_coll_acquire_t acm, struct jbl **jblp);

/**
 * @brief Inserts or replaces a document in the collection.
 *
 * The document is serialized and stored under the given `id`, all collection indexes are
 * updated (entries of the replaced document are removed first) and the collection record
 * counter is adjusted.
 *
 * @note The caller must hold the collection write lock (`JB_COLL_ACQUIRE_WRITE`).
 *
 * @param jbc Collection. Not zero.
 * @param jbl Document to store. Not zero.
 * @param id  Document id.
 * @return `0` on success or an error code.
 */
iwrc jb_put(struct jbcoll *jbc, struct jbl *jbl, int64_t id);

/**
 * @brief Removes a document from the collection.
 *
 * Removes all index entries of the document, deletes the document itself and decrements the
 * collection record counter.
 *
 * @note The caller must hold the collection write lock (`JB_COLL_ACQUIRE_WRITE`).
 *
 * @param jbc Collection. Not zero.
 * @param jbl Document being removed. Not zero.
 * @param id  Document id.
 * @return `0` on success or an error code.
 */
iwrc jb_del(struct jbcoll *jbc, struct jbl *jbl, int64_t id);

/**
 * @brief Same as `jb_put()` but stores the document at the current position of the cursor.
 *
 * Avoids the extra key lookup when the caller already has a cursor positioned at the document.
 *
 * @note The caller must hold the collection write lock (`JB_COLL_ACQUIRE_WRITE`).
 *
 * @param jbc Collection. Not zero.
 * @param cur Collection database cursor positioned at the document. Not zero.
 * @param id  Document id.
 * @param jbl Document to store. Not zero.
 * @return `0` on success or an error code.
 */
iwrc jb_cursor_set(struct jbcoll *jbc, struct iwkv_cursor *cur, int64_t id, struct jbl *jbl);

/**
 * @brief Same as `jb_del()` but deletes the document at the current position of the cursor.
 *
 * Avoids the extra key lookup when the caller already has a cursor positioned at the document.
 *
 * @note The caller must hold the collection write lock (`JB_COLL_ACQUIRE_WRITE`).
 *
 * @param jbc Collection. Not zero.
 * @param cur Collection database cursor positioned at the document. Not zero.
 * @param id  Document id.
 * @param jbl Document being removed. Not zero.
 * @return `0` on success or an error code.
 */
iwrc jb_cursor_del(struct jbcoll *jbc, struct iwkv_cursor *cur, int64_t id, struct jbl *jbl);

/**
 * @brief Creates an empty JSON object in the stack allocated `jbl` document.
 *
 * Unlike `jbl_create_empty_object()` this function does not allocate the document itself.
 * The document payload should be released by `binn_destroy(&jbl->bn)`.
 *
 * @param jbl Document holder. Not zero.
 */
iwrc jb_jbl_create_empty_object(struct jbl *jbl);

/**
 * @brief Creates an empty JSON array in the stack allocated `jbl` document.
 *
 * Unlike `jbl_create_empty_array()` this function does not allocate the document itself.
 * The document payload should be released by `binn_destroy(&jbl->bn)`.
 *
 * @param jbl Document holder. Not zero.
 */
iwrc jb_jbl_create_empty_array(struct jbl *jbl);

/**
 * @brief Finds the value of the document pointed by the rfc6901 `path`.
 *
 * The same as `jbl_at2()` but the result is stored in the stack allocated `jbl` view,
 * so no heap memory is allocated. The `res` document borrows the `jbl` memory and must not
 * be used after the `jbl` document is released or modified.
 *
 * @param jbl Document. Not zero.
 * @param jp  Parsed rfc6901 JSON pointer. Not zero.
 * @param res Holder of the found value view. Set to an empty (`JBV_NULL`) document if the path is not found.
 * @return `true` if the path was found.
 */
bool jb_jbl_at(struct jbl *jbl, struct jbl_ptr *jp, struct jbl *res);

/**
 * @brief Initializes the stack allocated `jbl` document as a read-only view of the serialized data.
 *
 * Unlike `jbl_init_from_buf()` this function neither copies nor validates the data: only the root
 * header of the serialized value is parsed and the document data is borrowed.
 *
 * It should be used only for the data produced or already validated by the database engine.
 *
 * @note The `jbl` document should be initialized (see `JBL_INITIALIZER`).
 * @note The `buf` memory must be kept alive while the document is used.
 *
 * @param jbl   Document holder. Not zero.
 * @param buf   Serialized `struct jbl` (binn) data. Not zero.
 * @param bufsz Number of bytes available in `buf`. An upper bound of the document size is acceptable.
 */
iwrc jb_jbl_wrap_buffer(struct jbl *jbl, void *buf, size_t bufsz);

/**
 * @brief Resolves an `@<collection>/<id>` reference while evaluating projection joins.
 *
 * Fetches the referenced document from another collection and returns it to the JQL
 * projection engine.
 *
 * @param id   Referenced document id.
 * @param coll Referenced collection name. Not zero.
 * @param out  [out] Holder of the fetched document. Not zero.
 *             The document should be disposed of by `jbl_destroy()`.
 * @param ctx  Query execution context providing the database handle. Not zero.
 * @return `0` on success, `IW_ERROR_NOT_EXISTS` or `IWKV_ERROR_NOTFOUND` if the reference
 *         is stale, or another error code.
 */
iwrc jb_collection_join_resolver(int64_t id, const char *coll, struct jbl **out, struct jbexec *ctx);

/**
 * @brief Compares two keys of the projection join cache.
 *
 * Keys are ordered by document id first and by the collection name if the ids are equal.
 *
 * @param v1 First `struct jbdocref` cache key. Not zero.
 * @param v2 Second `struct jbdocref` cache key. Not zero.
 * @return Negative, zero or positive value if `v1` is less than, equal to or greater than `v2`.
 */
int jb_proj_node_cache_cmp(const void *v1, const void *v2);

/**
 * @brief Releases a key of the projection join cache.
 *
 * Only the `struct jbdocref` key is heap allocated; cached values are owned by the node pool.
 *
 * @param key Heap allocated cache key. Can be zero.
 * @param val Cached value. Not used.
 */
void jb_proj_node_kvfree(void *key, void *val);

/**
 * @brief Computes the hash of a projection join cache key.
 *
 * @param key `struct jbdocref` cache key. Not zero.
 * @return Hash value.
 */
uint32_t jb_proj_node_hash(const void *key);

#endif
