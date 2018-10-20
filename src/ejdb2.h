#pragma once
#ifndef EJDB2_H
#define EJDB2_H

#include <iowow/iwkv.h>
#include "jbl.h"
#include "jql.h"

IW_EXTERN_C_START

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

typedef struct _EJDB_DOC {
  JBL jbl;
  JBL_NODE node;
} *EJDB_DOC;

typedef enum {
  EJDB_VCONTROL_OK = 0,
  EJDB_VCONTROL_STOP = 1,
  EJDB_VCONTROL_SKIP = 2,
} ejdb_vcontrol_cmd_t;

typedef void (*EJDB_VCONTROL_FN)(JQL q, ejdb_vcontrol_cmd_t cmd, int skip);

typedef void (*EJDB_VISITOR_FN)(JQL q, const EJDB_DOC doc, EJDB_VCONTROL_FN ctl, void *op);

IW_EXPORT WUR iwrc ejdb_open(const EJDB_OPTS *opts, EJDB *ejdbp);

IW_EXPORT iwrc ejdb_close(EJDB *ejdbp);

IW_EXPORT iwrc ejdb_query(JQL q, EJDB_VISITOR_FN visitor, void *op);

IW_EXPORT WUR iwrc ejdb_put(const char *coll, const EJDB_DOC doc, uint64_t *id);

IW_EXPORT iwrc ejdb_remove(const char *coll, uint64_t id);

IW_EXPORT WUR iwrc ejdb_init(void);

IW_EXTERN_C_END
#endif
