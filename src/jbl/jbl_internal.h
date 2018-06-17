#pragma once
#ifndef JBL_INTERNAL_H
#define JBL_INTERNAL_H

#include "binn.h"
#include "nxjson.h"
#include <iowow/iwlog.h>
#include <iowow/iwpool.h>
#include <iowow/iwconv.h>
#include "ejdb2cfg.h"

struct _JBL {
  binn bn;
};

typedef struct _JBLPTR {
  int cnt;          /**< Number of nodes */
  int pos;          /**< Current node position (like a cursor) */
  char *n[1];       /**< Path nodes */
} *JBLPTR;

typedef struct _JBLVCTX {  
  binn *bn;
  void *op;
  void *result;
  bool terminate;
} JBLVCTX;

typedef enum {
  JBL_VCMD_OK = 0,
  JBL_VCMD_TERMINATE = 1,
  JBL_VCMD_SKIP_NESTED = 1 << 1  
} jbl_visitor_cmd_t;

typedef struct _JBLPATCHEXT {
  const JBLPATCH *p;
  JBLPTR path;
  JBLPTR from;
  JBLNODE value;
} JBLPATCHEXT;

typedef struct _JBLDRCTX {
  IWPOOL *pool;
  JBLNODE root;
} JBLDRCTX;


typedef jbl_visitor_cmd_t (*JBLVISITOR)(int lvl, binn *bv, char *key, int idx, JBLVCTX *vctx, iwrc *rc);

iwrc _jbl_ptr_malloc(const char *path, JBLPTR *jpp);

#endif
