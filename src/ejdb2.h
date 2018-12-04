#pragma once
#ifndef EJDB2_H
#define EJDB2_H

#include <iowow/iwkv.h>
#include "jbl.h"
#include "jql.h"

IW_EXTERN_C_START

/**
 * @brief IWKV error codes.
 */
typedef enum {
  _EJDB_ERROR_START = (IW_ERROR_START + 15000UL),
  EJDB_ERROR_INVALID_COLLECTION_META,             /**< Invalid collection metadata */
  EJDB_ERROR_INVALID_COLLECTION_INDEX_META,       /**< Invalid collection index metadata */
  EJDB_ERROR_INVALID_INDEX_MODE,                  /**< Invalid index mode */
  EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE,    /**< Index exists but mismatched uniqueness constraint */
  EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED,    /**< Unique index constraint violated */
  _EJDB_ERROR_END
} ejdb_ecode;


typedef enum {
  EJDB_IDX_UNIQUE = 1,
  EJDB_IDX_ARR = 1 << 1,
  EJDB_IDX_STR = 1 << 2,
  EJDB_IDX_I64 = 1 << 3,
  EJDB_IDX_F64 = 1 << 4,
} ejdb_idx_mode_t;

/**
 * @brief Database handler.
 */
struct _EJDB;
typedef struct _EJDB *EJDB;

/**
 * @brief EJDB HTTP Server options
 */
typedef struct EJDB_HTTP {
  bool enabled;
  int port;
  const char *bind;
} EJDB_HTTP;

typedef struct EJDB_IPC {
  bool enabled;
  const char *file;
} EJDB_IPC;

/**
 * @brief EJDB open options.
 */
typedef struct _EJDB_OPTS {
  IWKV_OPTS kv;
  EJDB_HTTP http;
  EJDB_IPC ipc;
  bool no_wal;                  /**< Do not use write-ahead-log. Default: false */
  uint32_t sort_buffer_sz;      /**< Max sort buffer size. If exeeded an overflow file for sorted data will created.
                                     Default 16Mb, min: 1Mb */
  uint32_t document_buffer_sz;  /**< Initial size of buffer to process/store document during select.
                                     Default 64Kb, min: 16Kb */
} EJDB_OPTS;

typedef struct _EJDB_DOC {
  uint64_t id;
  JBL raw;
  JBL_NODE node;
  struct _EJDB_DOC *next;
  struct _EJDB_DOC *prev;
} *EJDB_DOC;

typedef struct _EJDB_LIST {
  EJDB db;
  JQL q;
  EJDB_DOC first;
  IWPOOL *pool;
} *EJDB_LIST;

typedef enum {
  EJDB_VCTL_OK = 0,
  EJDB_VCTL_STOP = 1
} ejdb_vctl_cmd_t;

struct _EJDB_EXEC;

/**
 * @param ctx Visitor context
 * @param doc Data in `doc` is valid only during execution of this method, to keep a data for farther
 *        processing you need to copy it.
 * @param step [out] Move forward cursor to given number of steps, `1` by default.
 */
typedef iwrc(*EJDB_EXEC_VISITOR)(struct _EJDB_EXEC *ctx, const EJDB_DOC doc, int64_t *step);
typedef struct _EJDB_EXEC {
  EJDB db;
  JQL q;
  EJDB_EXEC_VISITOR visitor;
  void *opaque;
  int64_t skip;
  int64_t limit;
  IWXSTR *log;
  IWPOOL *pool;               /**< Optional pool which can be used in query apply  */
} EJDB_EXEC;

IW_EXPORT WUR iwrc ejdb_open(const EJDB_OPTS *opts, EJDB *ejdbp);

IW_EXPORT iwrc ejdb_close(EJDB *ejdbp);

IW_EXPORT WUR iwrc ejdb_exec(EJDB_EXEC *ux);

IW_EXPORT WUR iwrc ejdb_list(EJDB db, JQL q, EJDB_DOC *first, int64_t limit, IWPOOL *pool);

IW_EXPORT WUR iwrc ejdb_list2(EJDB db, const char *coll, const char *query, int64_t limit, EJDB_LIST *listp);

IW_EXPORT WUR iwrc ejdb_list3(EJDB db, const char *coll, const char *query, int64_t limit, IWXSTR *log,
                              EJDB_LIST *listp);

IW_EXPORT void ejdb_list_destroy(EJDB_LIST *listp);

IW_EXPORT WUR iwrc ejdb_put(EJDB db, const char *coll, const JBL jbl, uint64_t id);

IW_EXPORT WUR iwrc ejdb_put_new(EJDB db, const char *coll, const JBL jbl, uint64_t *oid);

IW_EXPORT WUR iwrc ejdb_get(EJDB db, const char *coll, uint64_t id, JBL *jblp);

IW_EXPORT iwrc ejdb_remove(EJDB db, const char *coll, uint64_t id);

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
