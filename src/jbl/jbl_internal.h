#pragma once
#ifndef JBL_INTERNAL_H
#define JBL_INTERNAL_H

#include "jbl.h"
#include "binn.h"
#include <iowow/iwlog.h>
#include <iowow/iwpool.h>
#include <iowow/iwconv.h>
#include "ejdb2cfg.h"

struct _JBL {
  binn bn;
};

typedef struct _JBL_PTR {
  int cnt;          /**< Number of nodes */
  int pos;          /**< Current node position (like a cursor) */
  char *n[1];       /**< Path nodes */
} *JBL_PTR;

typedef struct _JBL_VCTX {
  binn *bn;
  void *op;
  void *result;
  bool terminate;
  IWPOOL *pool;
} JBL_VCTX;

typedef enum {
  JBL_VCMD_OK = 0,
  JBL_VCMD_TERMINATE = 1,
  JBL_VCMD_SKIP_NESTED = 1 << 1
} jbl_visitor_cmd_t;

typedef struct _JBL_PATCHEXT {
  const JBL_PATCH *p;
  JBL_PTR path;
  JBL_PTR from;
} JBL_PATCHEXT;

typedef struct _JBLDRCTX {
  IWPOOL *pool;
  JBL_NODE root;
} JBLDRCTX;

iwrc _jbl_ptr_malloc(const char *path, JBL_PTR *jpp);
iwrc _jbl_write_double(double num, jbl_json_printer pt, void *op);
iwrc _jbl_write_int(int64_t num, jbl_json_printer pt, void *op);
iwrc _jbl_write_string(const char *str, size_t len, jbl_json_printer pt, void *op, jbl_print_flags_t pf);
iwrc _jbl_node_from_binn2(const binn *bn, JBL_NODE *node, IWPOOL *pool);

typedef jbl_visitor_cmd_t (*JBL_VISITOR)(int lvl, binn *bv, char *key, int idx, JBL_VCTX *vctx, iwrc *rc);
iwrc _jbl_visit(binn_iter *iter, int lvl, JBL_VCTX *vctx, JBL_VISITOR visitor);

#endif
