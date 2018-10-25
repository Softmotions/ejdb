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
  EJDB_ERROR_INVALID_COLLECTION_META,
  //
  _EJDB_ERROR_END
} ejdb_ecode;


/**
 * Database handler.
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
typedef struct EJDB_OPTS {
  IWKV_OPTS kv;
  EJDB_HTTP http;
  EJDB_IPC ipc;
  bool no_wal;
} EJDB_OPTS;

typedef struct _EJDOC {
  uint64_t id;
  JBL raw;
  JBL_NODE node;
  struct _EJDB_DOC *next;
  struct _EJDB_DOC *prev;
} *EJDOC;

typedef enum {
  EJDB_VCTL_OK = 0,
  EJDB_VCTL_STOP = 1
} ejdb_vctl_cmd_t;

typedef void (*EJDB_VCTL_FN)(JQL q, ejdb_vctl_cmd_t cmd, int skip);

typedef void (*EJDB_VISITOR_FN)(JQL q, const EJDOC doc, EJDB_VCTL_FN ctl, void *op);

IW_EXPORT WUR iwrc ejdb_open(const EJDB_OPTS *opts, EJDB *ejdbp);

IW_EXPORT iwrc ejdb_close(EJDB *ejdbp);

IW_EXPORT WUR iwrc ejdb_put(EJDB db, const char *coll, const JBL jbl, uint64_t *id);

IW_EXPORT WUR iwrc ejdb_get(EJDB db, const char *coll, uint64_t id, JBL *jblp);

IW_EXPORT WUR iwrc ejdb_exec(EJDB db, JQL q, EJDB_VISITOR_FN visitor, void *op);

IW_EXPORT WUR iwrc ejdb_list(EJDB db, JQL q, EJDOC *first, int limit, IWPOOL *pool);

IW_EXPORT iwrc ejdb_remove(EJDB db, const char *coll, uint64_t id);

IW_EXPORT iwrc ejdb_remove_collection(EJDB db, const char *coll);

IW_EXPORT iwrc ejdb_ensure_collection(EJDB db, const char *coll);

IW_EXPORT iwrc ejdb_get_meta(EJDB db, JBL *jblp);

IW_EXPORT const char* ejdb_version_full(void);

IW_EXPORT unsigned int ejdb_version_major(void);

IW_EXPORT unsigned int ejdb_version_minor(void);

IW_EXPORT unsigned int ejdb_version_patch(void);

IW_EXPORT WUR iwrc ejdb_init(void);

IW_EXTERN_C_END
#endif
