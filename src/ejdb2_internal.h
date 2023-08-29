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
#include "jql.h"
#ifdef JB_HTTP
#include "jbr.h"
#endif
#include "jql_internal.h"

#include <iowow/iwkv.h>
#include <iowow/iwxstr.h>
#include <iowow/iwexfile.h>
#include <iowow/iwutils.h>
#include <iowow/iwhmap.h>
#include <iowow/iwjson_internal.h>

#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <setjmp.h>

#include "ejdb2cfg.h"

#define METADB_ID           1
#define NUMRECSDB_ID        2    // DB for number of records per index/collection
#define KEY_PREFIX_COLLMETA "c." // Full key format: c.<coldbid>
#define KEY_PREFIX_IDXMETA  "i." // Full key format: i.<coldbid>.<idxdbid>

#define ENSURE_OPEN(db_)                  \
  if (!(db_) || !((db_)->open)) {         \
    iwlog_error2("Database is not open");  \
    return IW_ERROR_INVALID_STATE;        \
  }

#define API_RLOCK(db_, rci_) \
  ENSURE_OPEN(db_);  \
  rci_ = pthread_rwlock_rdlock(&(db_)->rwl); \
  if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_WLOCK(db_, rci_) \
  ENSURE_OPEN(db_);  \
  rci_ = pthread_rwlock_wrlock(&(db_)->rwl); \
  if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_WLOCK2(db_, rci_) \
  rci_ = pthread_rwlock_wrlock(&(db_)->rwl); \
  if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_UNLOCK(db_, rci_, rc_)  \
  rci_ = pthread_rwlock_unlock(&(db_)->rwl); \
  if (rci_) IWRC(iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_), rc_)

#define API_COLL_UNLOCK(jbc_, rci_, rc_)                                     \
  do {                                                                    \
    rci_ = pthread_rwlock_unlock(&(jbc_)->rwl);                            \
    if (rci_) IWRC(iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_), rc_);  \
    API_UNLOCK((jbc_)->db, rci_, rc_);                                   \
  } while (0)

struct _JBIDX;
typedef struct _JBIDX*JBIDX;

/** Database collection */
typedef struct _JBCOLL {
  uint32_t    dbid;         /**< IWKV collection database ID */
  const char *name;         /**< Collection name */
  IWDB    cdb;              /**< IWKV collection database */
  EJDB    db;               /**< Main database reference */
  JBL     meta;             /**< Collection meta object */
  JBIDX   idx;              /**< First index in chain */
  int64_t rnum;             /**< Number of records stored in collection */
  pthread_rwlock_t rwl;
  int64_t id_seq;
} *JBCOLL;

/** Database collection index */
struct _JBIDX {
  struct _JBIDX *next;      /**< Next index in chain */
  int64_t  rnum;            /**< Number of records stored in index */
  JBCOLL   jbc;             /**< Owner document collection */
  JBL_PTR  ptr;             /**< Indexed JSON path poiner 0*/
  IWDB     idb;             /**< KV database for this index */
  uint32_t dbid;            /**< IWKV collection database ID */
  ejdb_idx_mode_t mode;     /**< Index mode/type mask */
  iwdb_flags_t    idbf;     /**< Index database flags */
};

/** Pair: collection name, document id */
struct _JBDOCREF {
  int64_t     id;
  const char *coll;
};

struct _EJDB {
  IWKV iwkv;
  IWDB metadb;
  IWDB nrecdb;
#ifdef JB_HTTP
  JBR jbr;
#endif
  IWHMAP *mcolls;
  iwkv_openflags    oflags;
  pthread_rwlock_t  rwl;      /**< Main RWL */
  struct _EJDB_OPTS opts;
  volatile bool     open;
};

struct _JBPHCTX {
  int64_t  id;
  JBCOLL   jbc;
  JBL      jbl;
  IWKV_val oldval;
};

struct _JBEXEC;

typedef iwrc (*JB_SCAN_CONSUMER)(
  struct _JBEXEC *ctx, IWKV_cursor cur, int64_t id,
  int64_t *step, bool *matched, iwrc err);

/**
 * @brief Index scan sorter consumer context
 */
struct _JBSSC {
  iwrc      rc;               /**< RC code used for in `_jb_do_sorting` */
  uint32_t *refs;             /**< Document references array */
  uint32_t  refs_asz;         /**< Document references array allocated size */
  uint32_t  refs_num;         /**< Document references array elements count */
  uint32_t  docs_asz;         /**< Documents array allocated size */
  uint8_t  *docs;             /**< Documents byte array */
  uint32_t  docs_npos;        /**< Next document offset */
  jmp_buf   fatal_jmp;
  IWFS_EXT  sof;              /**< Sort overflow file */
  bool      sof_active;
};

struct _JBMIDX {
  JBIDX       idx;                    /**< Index matched this filter */
  JQP_FILTER *filter;                 /**< Query filter */
  JQP_EXPR   *nexpr;                  /**< Filter node expression */
  JQP_EXPR   *expr1;                  /**< Start index expression (optional) */
  JQP_EXPR   *expr2;                  /**< End index expression (optional) */
  IWKV_cursor_op cursor_init;         /**< Initial index cursor position (optional) */
  IWKV_cursor_op cursor_step;         /**< Next index cursor step */
  bool orderby_support;               /**< Index supported first order-by clause */
};

typedef struct _JBEXEC {
  EJDB_EXEC *ux;           /**< User defined context */
  JBCOLL     jbc;          /**< Collection */

  int64_t istep;
  iwrc (*scanner)(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer);
  uint8_t *jblbuf;            /**< Buffer used to keep currently processed document */
  size_t   jblbufsz;          /**< Size of jblbuf allocated memory */
  bool     sorting;           /**< Resultset sorting needed */
  IWKV_cursor_op cursor_init; /**< Initial index cursor position (optional) */
  IWKV_cursor_op cursor_step; /**< Next index cursor step */
  struct _JBMIDX midx;        /**< Index matching context */
  struct _JBSSC  ssc;         /**< Result set sorting context */

  // JQL joned nodes cache
  IWHMAP *proj_joined_nodes_cache;
  IWPOOL *proj_joined_nodes_pool;
} JBEXEC;


typedef uint8_t jb_coll_acquire_t;
#define JB_COLL_ACQUIRE_WRITE    ((jb_coll_acquire_t) 0x01U)
#define JB_COLL_ACQUIRE_EXISTING ((jb_coll_acquire_t) 0x02U)

// Index selector empiric constants
#define JB_IDX_EMPIRIC_MAX_INOP_ARRAY_SIZE  500
#define JB_IDX_EMPIRIC_MIN_INOP_ARRAY_SIZE  10
#define JB_IDX_EMPIRIC_MAX_INOP_ARRAY_RATIO 200

void jbi_jbl_fill_ikey(JBIDX idx, JBL jbv, IWKV_val *ikey, char numbuf[static IWNUMBUF_SIZE]);
void jbi_jqval_fill_ikey(JBIDX idx, const JQVAL *jqval, IWKV_val *ikey, char numbuf[static IWNUMBUF_SIZE]);
void jbi_node_fill_ikey(JBIDX idx, JBL_NODE node, IWKV_val *ikey, char numbuf[static IWNUMBUF_SIZE]);

iwrc jbi_consumer(struct _JBEXEC *ctx, IWKV_cursor cur, int64_t id, int64_t *step, bool *matched, iwrc err);
iwrc jbi_sorter_consumer(struct _JBEXEC *ctx, IWKV_cursor cur, int64_t id, int64_t *step, bool *matched, iwrc err);
iwrc jbi_full_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer);
iwrc jbi_selection(JBEXEC *ctx);
iwrc jbi_pk_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer);
iwrc jbi_uniq_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer);
iwrc jbi_dup_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer);
bool jbi_node_expr_matched(JQP_AUX *aux, JBIDX idx, IWKV_cursor cur, JQP_EXPR *expr, iwrc *rcp);

iwrc jb_get(EJDB db, const char *coll, int64_t id, jb_coll_acquire_t acm, JBL *jblp);
iwrc jb_put(JBCOLL jbc, JBL jbl, int64_t id);
iwrc jb_del(JBCOLL jbc, JBL jbl, int64_t id);
iwrc jb_cursor_set(JBCOLL jbc, IWKV_cursor cur, int64_t id, JBL jbl);
iwrc jb_cursor_del(JBCOLL jbc, IWKV_cursor cur, int64_t id, JBL jbl);

iwrc jb_collection_join_resolver(int64_t id, const char *coll, JBL *out, JBEXEC *ctx);
int jb_proj_node_cache_cmp(const void *v1, const void *v2);
void jb_proj_node_kvfree(void *key, void *val);
uint32_t jb_proj_node_hash(const void *key);

#endif
