#pragma once
#ifndef EJDB2_H
#define EJDB2_H

/**************************************************************************************************
 * EJDB2
 *
 * MIT License
 *
 * Copyright (c) 2012-2021 Softmotions Ltd <info@softmotions.com>
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

#include <ejdb2/iowow/iwkv.h>
#include "jql.h"
#include "jbl.h"

IW_EXTERN_C_START

/**
 * @brief ejdb2 initialization routine.
 * @note Must be called before using any of ejdb API function.
 */
IW_EXPORT WUR iwrc ejdb_init(void);

/** Maximum length of EJDB collection name */
#define EJDB_COLLECTION_NAME_MAX_LEN 255

/**
 * @brief EJDB error codes.
 */
typedef enum {
  _EJDB_ERROR_START = (IW_ERROR_START + 15000UL),
  EJDB_ERROR_INVALID_COLLECTION_NAME,             /**< Invalid collection name */
  EJDB_ERROR_INVALID_COLLECTION_META,             /**< Invalid collection metadata */
  EJDB_ERROR_INVALID_COLLECTION_INDEX_META,       /**< Invalid collection index metadata */
  EJDB_ERROR_INVALID_INDEX_MODE,                  /**< Invalid index mode specified */
  EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE,    /**< Index exists but mismatched uniqueness constraint */
  EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED,    /**< Unique index constraint violated */
  EJDB_ERROR_COLLECTION_NOT_FOUND,                /**< Collection not found */
  EJDB_ERROR_TARGET_COLLECTION_EXISTS,            /**< Target collection exists */
  EJDB_ERROR_PATCH_JSON_NOT_OBJECT,               /**< Patch JSON must be an object (map) */
  _EJDB_ERROR_END,
} ejdb_ecode_t;

/** Index creation mode */
typedef uint8_t ejdb_idx_mode_t;

/** Marks index is unique, no duplicated values allowed. */
#define EJDB_IDX_UNIQUE ((ejdb_idx_mode_t) 0x01U)

/** Index values have string type.
 *  Type conversion will be performed on atempt to save value with other type */
#define EJDB_IDX_STR ((ejdb_idx_mode_t) 0x04U)

/** Index values have signed integer 64 bit wide type.
 *  Type conversion will be performed on atempt to save value with other type */
#define EJDB_IDX_I64 ((ejdb_idx_mode_t) 0x08U)

/** Index value have floating point type.
 *  @note Internally floating point numbers are converted to string
 *        with precision of 6 digits after decimal point.
 */
#define EJDB_IDX_F64 ((ejdb_idx_mode_t) 0x10U)

/**
 * @brief Database handler.
 */
struct _EJDB;
typedef struct _EJDB*EJDB;

/**
 * @brief EJDB HTTP/Websocket Server options.
 */
typedef struct _EJDB_HTTP {
  bool enabled;                 /**< If HTTP/Websocket endpoint enabled. Default: false */
  int  port;                    /**< Listen port number, required */
  const char *bind;             /**< Listen IP/host. Default: `localhost` */
  const char *access_token;     /**< Server access token passed in `X-Access-Token` header. Default: zero */
  size_t      access_token_len; /**< Length of access token string. Default: zero */
  bool blocking;                /**< Block `ejdb_open()` thread until http service finished.
                                     Otherwise HTTP server will be started in background. */
  bool   read_anon;             /**< Allow anonymous read-only database access */
  size_t max_body_size;         /**< Maximum WS/HTTP API body size. Default: 64Mb, Min: 512K */
  bool cors;                    /**< Allow CORS */
} EJDB_HTTP;

/**
 * @brief EJDB open options.
 */
typedef struct _EJDB_OPTS {
  IWKV_OPTS kv;                 /**< IWKV storage options. @see iwkv.h */
  EJDB_HTTP http;               /**< HTTP/Websocket server options */
  bool      no_wal;             /**< Do not use write-ahead-log. Default: false */
  uint32_t  sort_buffer_sz;     /**< Max sorting buffer size. If exceeded an overflow temp file for sorted data will
                                   created.
                                     Default 16Mb, min: 1Mb */
  uint32_t document_buffer_sz;  /**< Initial size of buffer in bytes used to process/store document during query
                                   execution.
                                     Default 64Kb, min: 16Kb */
} EJDB_OPTS;

/**
 * @brief Document representation as result of query execution.
 * @see ejdb_exec()
 */
typedef struct _EJDB_DOC {
  int64_t id;                 /**< Document ID. Not zero. */
  JBL     raw;                /**< JSON document in compact binary form.
                                   Based on [Binn](https://github.com/liteserver/binn) format.
                                   Not zero. */
  JBL_NODE node;              /**< JSON document as in-memory tree. Not zero only if query has `apply` or `projection`
                                 parts.

                                   @warning The lifespan of @ref EJDB_DOC.node will be valid only during the call of
                                      @ref EJDB_EXEC_VISITOR
                                            It is true in all cases EXCEPT:
                                            - @ref EJDB_EXEC.pool is not set by `ejdb_exec()` caller
                                            - One of `ejdb_list()` methods used */

  struct _EJDB_DOC *next;     /**< Reference to next document in result list or zero.
                                   Makes sense only for `ejdb_list()` calls. */
  struct _EJDB_DOC *prev;     /**< Reference to the previous document in result list or zero.
                                   Makes sense only for `ejdb_list()` calls. */
} *EJDB_DOC;

/**
 * @brief Query result as list.
 * Used as result of `ejdb_list()` query functions.
 *
 * @warning Getting result of query as list can be very memory consuming for large collections.
 *          Consider use of `ejdb_exec()` with visitor or set `limit` for query.
 */
typedef struct _EJDB_LIST {
  EJDB     db;          /**< EJDB storage used for query execution. Not zero. */
  JQL      q;           /**< Query executed. Not zero. */
  EJDB_DOC first;       /**< First document in result list. Zero if result set is empty. */
  IWPOOL  *pool;        /**< Memory pool used to store list of documents */
} *EJDB_LIST;

struct _EJDB_EXEC;

/**
 * @brief Visitor for matched documents during query execution.
 *
 * @param ctx Visitor context.
 * @param doc Data in `doc` is valid only during execution of this method, to keep a data for farther
 *        processing you need to copy it.
 * @param step [out] Move forward cursor to given number of steps, `1` by default.
 */
typedef iwrc (*EJDB_EXEC_VISITOR)(struct _EJDB_EXEC *ctx, EJDB_DOC doc, int64_t *step);

/**
 * @brief Query execution context.
 * Passed to `ejdb_exec()` to execute database query.
 */
typedef struct _EJDB_EXEC {
  EJDB db;                    /**< EJDB database object. Required. */
  JQL  q;                     /**< Query object to be executed. Created by `jql_create()` Required. */
  EJDB_EXEC_VISITOR visitor;  /**< Optional visitor to handle documents in result set. */
  void   *opaque;             /**< Optional user data passed to visitor functions. */
  int64_t skip;               /**< Number of records to skip. Takes precedence over `skip` encoded in query. */
  int64_t limit;              /**< Result set size limitation. Zero means no limitations. Takes precedence over `limit`
                                 encoded in query. */
  int64_t cnt;                /**< Number of result documents processed by `visitor` */
  IWXSTR *log;                /**< Optional query execution log buffer. If set major query execution/index selection
                                 steps will be logged into */
  IWPOOL *pool;               /**< Optional pool which can be used in query apply  */
} EJDB_EXEC;

/**
 * @brief Open storage file.
 *
 * Storage can be opened only by one single process at time.
 *
 * @param opts  Operating options. Can be stack allocated or disposed after `ejdb_open()` call.
 * @param [out] ejdbp EJDB storage handle pointer as result.
 */
IW_EXPORT WUR iwrc ejdb_open(const EJDB_OPTS *opts, EJDB *ejdbp);

/**
 * @brief Closes storage and frees up all resources.
 * @param [in,out] ejdbp Pointer to storage handle, will set to zero oncompletion.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT iwrc ejdb_close(EJDB *ejdbp);

/**
 * @brief Executes a query.
 *
 * The `ux` structure should be configured before this call,
 * usually it is a stack allocated object.
 *
 * Query object should be created by `jql_create()`.
 *
 * Example:
 *
 * @code {.c}
 *
 *  JQL q;
 *  iwrc rc = jql_create(&q, "mycollection", "/[firstName=?]");
 *  RCRET(rc);
 *
 *  jql_set_str(q, 0, 0, "Andy"); // Set positional string query parameter
 *
 *  EJDB_EXEC ux = {
 *    .db = db,
 *    .q = q,
 *    .visitor = my_query_results_visitor_func
 *  };
 *  rc = ejdb_exec(&ux);  // Execute query
 *  jql_destroy(&q);      // Destroy query object
 *
 * @endcode
 *
 * Query object can be reused in many `ejdb_exec()` calls
 * with different positional/named parameters.
 *
 * @param [in] ux Query execution params, object state may be changes during query execution.
 *                Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_exec(EJDB_EXEC *ux);

/**
 * @brief Executes a given query and builds a query result as linked list of documents.
 *
 * @warning Getting whole query result as linked list can be memory consuming for large collections.
 *          Consider use of `ejdb_exec()` with visitor or set a positive `limit` value.
 *
 * @param db            Database handle. Not zero.
 * @param q             Query object. Not zero.
 * @param [out] first   First document in result set or zero if no matched documents found.
 * @param limit         Maximum number of documents in result set. Takes precedence over `limit` encoded in query.
 *                      Zero means a `limit` encoded in `q` will be used.
 * @param pool          Memory pool will keep documents allocated in result set. Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_list(EJDB db, JQL q, EJDB_DOC *first, int64_t limit, IWPOOL *pool);

/**
 * @brief Executes a given query `q` then returns `count` of matched documents.
 *
 * @param db           Database handle. Not zero.
 * @param q            Query object. Not zero.
 * @param [out] count  Placeholder for number of matched documents. Not zero.
 * @param limit        Limit of matched rows. Makes sense for update queries.
 *
 */
IW_EXPORT WUR iwrc ejdb_count(EJDB db, JQL q, int64_t *count, int64_t limit);

/**
 * @brief Executes a given query `q` then returns `count` of matched documents.
 *
 * @param db           Database handle. Not zero.
 * @param coll         Name of document collection.
 *                     Can be zero, in what collection name should be encoded in query.
 * @param q            Query text. Not zero.
 * @param [out] count  Placeholder for number of matched documents. Not zero.
 * @param limit        Limit of matched rows. Makes sense for update queries.
 *
 */
IW_EXPORT WUR iwrc ejdb_count2(EJDB db, const char *coll, const char *q, int64_t *count, int64_t limit);

/**
 * @brief Executes update query assuming that query object contains `apply` clause.
 *        Similar to `ejdb_count`.
 *
 * @param db          Database handle. Not zero.
 * @param q           Query object. Not zero.
 */
IW_EXPORT WUR iwrc ejdb_update(EJDB db, JQL q);

/**
 * @brief Executes update query assuming that query object contains `apply` clause.
 *        Similar to `ejdb_count`.
 *
 * @param db        Database handle. Not zero.
 * @param coll         Name of document collection.
 *                     Can be zero, in what collection name should be encoded in query.
 * @param q         Query text. Not zero.
 */
IW_EXPORT WUR iwrc ejdb_update2(EJDB db, const char *coll, const char *q);

/**
 * @brief Executes a given `query` and builds a result as linked list of documents.
 *
 * @note Returned `listp` must be disposed by `ejdb_list_destroy()`
 * @warning Getting whole query result as linked list can be memory consuming for large collections.
 *          Consider use of `ejdb_exec()` with visitor or set a positive `limit` value.
 *
 * @param db            Database handle. Not zero.
 * @param coll          Collection name. If zero, collection name must be encoded in query.
 * @param query         Query text. Not zero.
 * @param limit         Maximum number of documents in result set. Takes precedence over `limit` encoded in query.
 *                      Zero means a `limit` encoded in `query` will be used.
 * @param [out] listp   Holder for query result, should be disposed by `ejdb_list_destroy()`.
 *                      Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_list2(EJDB db, const char *coll, const char *query, int64_t limit, EJDB_LIST *listp);

/**
 * @brief Executes a given `query` and builds a result as linked list of documents.
 *
 * @note Returned `listp` must be disposed by `ejdb_list_destroy()`
 * @warning Getting whole query result as linked list can be memory consuming for large collections.
 *          Consider use of `ejdb_exec()` with visitor or set a positive `limit` value.
 *
 * @param db            Database handle. Not zero.
 * @param coll          Collection name. If zero, collection name must be encoded in query.
 * @param query         Query text. Not zero.
 * @param limit         Maximum number of documents in result set. Takes precedence over `limit` encoded in query.
 * @param log           Optional buffer to collect query execution / index usage info.
 * @param [out] listp   Holder for query result, should be disposed by `ejdb_list_destroy()`.
 *                      Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_list3(
  EJDB db, const char *coll, const char *query, int64_t limit,
  IWXSTR *log, EJDB_LIST *listp);

/**
 * @brief Executes a given query `q` and builds a result as linked list of documents (`listp`).
 *
 * @note Returned `listp` must be disposed by `ejdb_list_destroy()`
 * @warning Getting whole query result as linked list can be memory consuming for large collections.
 *          Consider use of `ejdb_exec()` with visitor  or set a positive `limit` value.
 *
 * @param db           Database handle. Not zero.
 * @param q            Query object. Not zero.
 * @param limit        Maximum number of documents in result set. Takes precedence over `limit` encoded in query.
 * @param log          Optional buffer to collect query execution / index usage info.
 * @param [out] listp  Holder for query result, should be disposed by `ejdb_list_destroy()`.
 *                     Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_list4(EJDB db, JQL q, int64_t limit, IWXSTR *log, EJDB_LIST *listp);

/**
 * @brief Destroy query result set and set `listp` to zero.
 * @param [in,out] listp Can be zero.
 */
IW_EXPORT void ejdb_list_destroy(EJDB_LIST *listp);

/**
 * @brief Apply rfc6902/rfc7396 JSON patch to the document identified by `id`.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param patchjson   JSON patch conformed to rfc6902 or rfc7396 specification.
 * @param id          Document id. Not zero.
 *
 * @return `0` on success.
 *         `IWKV_ERROR_NOTFOUND` if document not found.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_patch(EJDB db, const char *coll, const char *patchjson, int64_t id);

/**
 * @brief Apply rfc6902/rfc7396 JSON patch to the document identified by `id`.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param patch       JSON patch conformed to rfc6902 or rfc7396 specification.
 * @param id          Document id. Not zero.
 *
 * @return `0` on success.
 *         `IWKV_ERROR_NOTFOUND` if document not found.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_patch_jbn(EJDB db, const char *coll, JBL_NODE patch, int64_t id);


/**
 * @brief Apply rfc6902/rfc7396 JSON patch to the document identified by `id`.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param patch       JSON patch conformed to rfc6902 or rfc7396 specification.
 * @param id          Document id. Not zero.
 *
 * @return `0` on success.
 *         `IWKV_ERROR_NOTFOUND` if document not found.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_patch_jbl(EJDB db, const char *coll, JBL patch, int64_t id);

/**
 * @brief Apply JSON merge patch (rfc7396) to the document identified by `id` or
 *        insert new document under specified `id`.
 * @note This is an atomic operation.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param patchjson   JSON merge patch conformed to rfc7396 specification.
 * @param id          Document id. Not zero.
 *
 */
IW_EXPORT WUR iwrc ejdb_merge_or_put(EJDB db, const char *coll, const char *patchjson, int64_t id);

/**
 * @brief Apply JSON merge patch (rfc7396) to the document identified by `id` or
 *        insert new document under specified `id`.
 * @note This is an atomic operation.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param patch       JSON merge patch conformed to rfc7396 specification.
 * @param id          Document id. Not zero.
 *
 */
IW_EXPORT WUR iwrc ejdb_merge_or_put_jbn(EJDB db, const char *coll, JBL_NODE patch, int64_t id);

/**
 * @brief Apply JSON merge patch (rfc7396) to the document identified by `id` or
 *        insert new document under specified `id`.
 * @note This is an atomic operation.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param patch       JSON merge patch conformed to rfc7396 specification.
 * @param id          Document id. Not zero.
 *
 */
IW_EXPORT WUR iwrc ejdb_merge_or_put_jbl(EJDB db, const char *coll, JBL patch, int64_t id);

/**
 * @brief Save a given `jbl` document under specified `id`.
 *
 * @param db        Database handle. Not zero.
 * @param coll      Collection name. Not zero.
 * @param jbl       JSON document. Not zero.
 * @param id        Document identifier. Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_put(EJDB db, const char *coll, JBL jbl, int64_t id);

/**
 * @brief Save a document into `coll` under new identifier.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param jbl         JSON document. Not zero.
 * @param [out] oid   Placeholder for new document id. Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_put_new(EJDB db, const char *coll, JBL jbl, int64_t *oid);

/**
 * @brief Save a document into `coll` under new identifier.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param jbn         JSON document. Not zero.
 * @param [out] oid   Placeholder for new document id. Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT iwrc ejdb_put_new_jbn(EJDB db, const char *coll, JBL_NODE jbn, int64_t *id);

/**
 * @brief Retrieve document identified by given `id` from collection `coll`.
 *
 * @param db          Database handle. Not zero.
 * @param coll        Collection name. Not zero.
 * @param id          Document id. Not zero.
 * @param [out] jblp  Placeholder for document.
 *                    Must be released by `jbl_destroy()`
 *
 * @return `0` on success.
 *         `IWKV_ERROR_NOTFOUND` if document not found.
 *         `IW_ERROR_NOT_EXISTS` if collection `coll` is not exists in db.
 *          Any non zero error codes.
 */
IW_EXPORT WUR iwrc ejdb_get(EJDB db, const char *coll, int64_t id, JBL *jblp);

/**
 * @brief  Remove document identified by given `id` from collection `coll`.
 *
 * @param db    Database handle. Not zero.
 * @param coll  Collection name. Not zero.
 * @param id    Document id. Not zero.
 *
 * @return `0` on success.
 *         `IWKV_ERROR_NOTFOUND` if document not found.
 *          Any non zero error codes.
 */
IW_EXPORT iwrc ejdb_del(EJDB db, const char *coll, int64_t id);

/**
 * @brief Remove collection under the given name `coll`.
 *
 * @param db    Database handle. Not zero.
 * @param coll  Collection name. Not zero.
 *
 * @return `0` on success.
 *          Will return `0` if collection is not found.
 *          Any non zero error codes.
 */
IW_EXPORT iwrc ejdb_remove_collection(EJDB db, const char *coll);

/**
 * @brief Rename collection `coll` to `new_coll`.
 *
 * @param db    Database handle. Not zero.
 * @param coll  Old collection name. Not zero.
 * @param new_coll New collection name.
 * @return `0` on success.
 *          - `EJDB_ERROR_COLLECTION_NOT_FOUND` - if source `coll` is not found.
 *          - `EJDB_ERROR_TARGET_COLLECTION_EXISTS` - if `new_coll` is exists already.
 *          -  Any other non zero error codes.
 */
IW_EXPORT iwrc ejdb_rename_collection(EJDB db, const char *coll, const char *new_coll);

/**
 * @brief Create collection with given name if it has not existed before
 *
 * @param db    Database handle. Not zero.
 * @param coll  Collection name. Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
IW_EXPORT iwrc ejdb_ensure_collection(EJDB db, const char *coll);

/**
 * @brief Create index with specified parameters if it has not existed before.
 *
 * @note Index `path` must be fully specified as rfc6901 JSON pointer
 *       and must not countain unspecified `*`/`**` element in middle sections.
 * @see ejdb_idx_mode_t.
 *
 * Example document:
 *
 * @code
 * {
 *   "address" : {
 *      "street": "High Street"
 *   }
 * }
 * @endcode
 *
 * Create unique index over all street names in nested address object:
 *
 * @code {.c}
 * iwrc rc = ejdb_ensure_index(db, "mycoll", "/address/street", EJDB_IDX_UNIQUE | EJDB_IDX_STR);
 * @endcode
 *
 * @param db    Database handle. Not zero.
 * @param coll  Collection name. Not zero.
 * @param path  rfc6901 JSON pointer to indexed field.
 * @param mode  Index mode.
 *
 * @return `0` on success.
 *         `EJDB_ERROR_INVALID_INDEX_MODE` Invalid `mode` specified
 *         `EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE` trying to create non unique index over existing unique or vice
 * versa.
 *          Any non zero error codes.
 *
 */
IW_EXPORT iwrc ejdb_ensure_index(EJDB db, const char *coll, const char *path, ejdb_idx_mode_t mode);

/**
 * @brief Remove index if it has existed before.
 *
 * @param db    Database handle. Not zero.
 * @param coll  Collection name. Not zero.
 * @param path  rfc6901 JSON pointer to indexed field.
 * @param mode  Index mode.
 *
 * @return `0` on success.
 *          Will return `0` if collection is not found.
 *          Any non zero error codes.
 */
IW_EXPORT iwrc ejdb_remove_index(EJDB db, const char *coll, const char *path, ejdb_idx_mode_t mode);

/**
 * @brief Returns JSON document describind database structure.
 * @note Returned `jblp` must be disposed by `jbl_destroy()`
 *
 * Example database metadata:
 * @code {.json}
 *
 *   {
 *    "version": "2.0.0", // EJDB engine version
 *    "file": "db.jb",    // Path to storage file
 *    "size": 16384,      // Storage file size in bytes
 *    "collections": [    // List of collections
 *     {
 *      "name": "c1",     // Collection name
 *      "dbid": 3,        // Collection database ID
 *      "rnum": 2,        // Number of documents in collection
 *      "indexes": [      // List of collections indexes
 *       {
 *        "ptr": "/n",    // rfc6901 JSON pointer to indexed field
 *        "mode": 8,      // Index mode. Here is EJDB_IDX_I64
 *        "idbf": 96,     // Index flags. See iwdb_flags_t
 *        "dbid": 4,      // Index database ID
 *        "rnum": 2       // Number records stored in index database
 *       }
 *      ]
 *     }
 *    ]
 *   }
 * @endcode
 *
 * @param db          Database handle. Not zero.
 * @param [out] jblp  JSON object describing ejdb storage.
 *                    Must be disposed by `jbl_destroy()`
 */
IW_EXPORT iwrc ejdb_get_meta(EJDB db, JBL *jblp);

/**
 * Creates an online database backup image and copies it into the specified `target_file`.
 * During online backup phase read/write database operations are allowed and not
 * blocked for significant amount of time. Backup finish time is placed into `ts`
 * as number of milliseconds since epoch.
 *
 * Online backup guaranties what all records before `ts` timestamp will
 * be stored in backup image. Later, online backup image can be
 * opened as ordinary database file.
 *
 * @note In order to avoid deadlocks: close all opened database cursors
 * before calling this method or do call in separate thread.
 *
 * @param Database handle. Not zero.
 * @param [out] ts Backup completion timestamp
 * @param target_file backup file path
 */
IW_EXPORT iwrc ejdb_online_backup(EJDB db, uint64_t *ts, const char *target_file);

/**
 * @brief Get access to underlying IWKV storage.
 *        Use it with caution.
 *
 * @param db Database handle. Not zero.
 * @param [out] kvp Placeholder for IWKV storage.
 */
IW_EXPORT iwrc ejdb_get_iwkv(EJDB db, IWKV *kvp);

/**
 * @brief  Return `\0` terminated ejdb2 source GIT revision hash.
 */
IW_EXPORT const char *ejdb_git_revision(void);

/**
 * @brief Return `\0` terminated EJDB version string.
 */
IW_EXPORT const char *ejdb_version_full(void);

/**
 * @brief Return major library version.
 */
IW_EXPORT unsigned int ejdb_version_major(void);

/**
 * @brief Return minor library version.
 */
IW_EXPORT unsigned int ejdb_version_minor(void);

/**
 * @brief Return patch library version.
 */
IW_EXPORT unsigned int ejdb_version_patch(void);

IW_EXTERN_C_END
#endif
