#pragma once
#ifndef EJDB2_H
#define EJDB2_H

#include <iowow/iwkv.h>
#include "jql.h"

IW_EXTERN_C_START

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
  _EJDB_ERROR_END
} ejdb_ecode_t;

/** Index creation mode */
typedef uint8_t ejdb_idx_mode_t;

/** Marks index is unique, no duplicated values allowed. */
#define EJDB_IDX_UNIQUE     ((ejdb_idx_mode_t) 0x01U)

/** Index values have string type.
 *  Type conversion will be performed on atempt to save value with other type */
#define EJDB_IDX_STR        ((ejdb_idx_mode_t) 0x04U)

/** Index values have signed integer 64 bit wide type.
 *  Type conversion will be performed on atempt to save value with other type */
#define EJDB_IDX_I64        ((ejdb_idx_mode_t) 0x08U)

/** Index value have floating point type.
 *  @note Internally floating point numbers are converted to string
 *        with precision of 6 digits after decimal point.
 */
#define EJDB_IDX_F64        ((ejdb_idx_mode_t) 0x10U)

/**
 * @brief Database handler.
 */
struct _EJDB;
typedef struct _EJDB *EJDB;

/**
 * @brief EJDB HTTP/Websocket Server options.
 */
typedef struct _EJDB_HTTP {
  bool enabled;               /**< If HTTP/Websocket endpoint enabled. Default: false */
  int port;                   /**< Listen port number, required */
  const char *bind;           /**< Listen IP/host. Default: `localhost` */
  const char *access_token;   /**< Server access token passed in `X-Access-Token` header. Default: zero */
  size_t access_token_len;    /**< Length of access token string. Default: zero */
  bool blocking;              /**< Block `ejdb_open()` thread until http service finished.
                                   Otherwise HTTP server will be started in background. */
  bool read_anon;             /**< Allow anonymous read-only database access */
  size_t max_body_size;       /**< Maximum WS/HTTP API body size. Default: 64Mb, Min: 512K */
} EJDB_HTTP;

/**
 * @brief EJDB open options.
 */
typedef struct _EJDB_OPTS {
  IWKV_OPTS kv;                 /**< IWKV storage options. @see iwkv.h */
  EJDB_HTTP http;               /**< HTTP/Websocket server options */
  bool no_wal;                  /**< Do not use write-ahead-log. Default: false */
  uint32_t sort_buffer_sz;      /**< Max sort buffer size. If exeeded an overflow file for sorted data will created.
                                     Default 16Mb, min: 1Mb */
  uint32_t document_buffer_sz;  /**< Initial size of buffer to process/store document during select.
                                     Default 64Kb, min: 16Kb */
} EJDB_OPTS;

/**
 * @brief Document representation as result of query execution.
 * @see ejdb_exec()
 */
typedef struct _EJDB_DOC {
  int64_t id;                 /**< Document ID. Not zero. */
  JBL raw;                    /**< JSON document in compact binary form.
                                   Based on [Binn](https://github.com/liteserver/binn) format.
                                   Not zero. */
  JBL_NODE node;              /**< JSON document as in-memory tree. Not zero only if query has `apply` or `projection` parts.
                                   @warning The lifespan of @ref EJDB_DOC.node will be valid only during the call of @ref EJDB_EXEC_VISITOR
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
 *          Consider use of `ejdb_exec()` or set `limit` for query.
 */
typedef struct _EJDB_LIST {
  EJDB db;              /**< EJDB storage used for query execution. Not zero. */
  JQL q;                /**< Query executed. Not zero. */
  EJDB_DOC first;       /**< First document in result list. Zero if result set is empty. */
  IWPOOL *pool;         /**< Memory pool used to store list of documents */
} *EJDB_LIST;

struct _EJDB_EXEC;

/**
 * @brief Visitor for matched documents during query execution.
 *
 * @param ctx Visitor context
 * @param doc Data in `doc` is valid only during execution of this method, to keep a data for farther
 *        processing you need to copy it.
 * @param step [out] Move forward cursor to given number of steps, `1` by default.
 */
typedef iwrc(*EJDB_EXEC_VISITOR)(struct _EJDB_EXEC *ctx, EJDB_DOC doc, int64_t *step);

/**
 * @brief Query execution context.
 * Passed to `ejdb_exec()` to execute database query.
 */
typedef struct _EJDB_EXEC {
  EJDB db;                    /**< EJDB database object. Required. */
  JQL q;                      /**< Query object to be executed. Required. */
  EJDB_EXEC_VISITOR visitor;  /**< Optional visitor to handle documents in result set. */
  void *opaque;               /**< Optional user data passed to visitor functions. */
  int64_t skip;               /**< Number of records to skip. Takes precedence over `skip` encoded in query. */
  int64_t limit;              /**< Result set size limitation. Zero means no restrictions. Takes precedence over `skip` encoded in query. */
  int64_t cnt;                /**< Number of result documents processed by `visitor` */
  IWXSTR *log;                /**< Optional query execution log buffer. If set major query execution/index selection steps will be logged into */
  IWPOOL *pool;               /**< Optional pool which can be used in query apply  */
} EJDB_EXEC;

IW_EXPORT WUR iwrc ejdb_open(const EJDB_OPTS *opts, EJDB *ejdbp);

IW_EXPORT iwrc ejdb_close(EJDB *ejdbp);

IW_EXPORT WUR iwrc ejdb_exec(EJDB_EXEC *ux);

IW_EXPORT WUR iwrc ejdb_list(EJDB db, JQL q, EJDB_DOC *first, int64_t limit, IWPOOL *pool);

IW_EXPORT WUR iwrc ejdb_list2(EJDB db, const char *coll, const char *query, int64_t limit, EJDB_LIST *listp);

IW_EXPORT WUR iwrc ejdb_list3(EJDB db, const char *coll, const char *query, int64_t limit,
                              IWXSTR *log, EJDB_LIST *listp);

IW_EXPORT WUR iwrc ejdb_list4(EJDB db, JQL q, int64_t limit, IWXSTR *log, EJDB_LIST *listp);

IW_EXPORT void ejdb_list_destroy(EJDB_LIST *listp);

IW_EXPORT WUR iwrc ejdb_patch(EJDB db, const char *coll, const char *patchjson, int64_t id);

IW_EXPORT WUR iwrc ejdb_put(EJDB db, const char *coll, JBL jbl, int64_t id);

IW_EXPORT WUR iwrc ejdb_put_new(EJDB db, const char *coll, JBL jbl, int64_t *oid);

IW_EXPORT WUR iwrc ejdb_get(EJDB db, const char *coll, int64_t id, JBL *jblp);

IW_EXPORT iwrc ejdb_del(EJDB db, const char *coll, int64_t id);

IW_EXPORT iwrc ejdb_remove_collection(EJDB db, const char *coll);

IW_EXPORT iwrc ejdb_ensure_collection(EJDB db, const char *coll);

IW_EXPORT iwrc ejdb_ensure_index(EJDB db, const char *coll, const char *path, ejdb_idx_mode_t mode);

IW_EXPORT iwrc ejdb_remove_index(EJDB db, const char *coll, const char *path, ejdb_idx_mode_t mode);

IW_EXPORT iwrc ejdb_get_meta(EJDB db, JBL *jblp);

IW_EXPORT const char *ejdb_version_full(void);

IW_EXPORT unsigned int ejdb_version_major(void);

IW_EXPORT unsigned int ejdb_version_minor(void);

IW_EXPORT unsigned int ejdb_version_patch(void);

IW_EXPORT WUR iwrc ejdb_init(void);

IW_EXTERN_C_END
#endif
