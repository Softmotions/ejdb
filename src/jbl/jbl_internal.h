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
  JBL_NODE node;
};

/**
 * @brief JBL visitor context
 */
typedef struct _JBL_VCTX {
  int pos;          /**< Aux position, not actually used by visitor core */
  binn *bn;         /**< Root node from which started visitor */
  void *op;         /**< Arbitrary opaque data */
  void *result;
  bool terminate;
  IWPOOL *pool;     /**< Pool placeholder, initialization is responsibility of `JBL_VCTX` creator */
} JBL_VCTX;

typedef jbn_visitor_cmd_t jbl_visitor_cmd_t;

typedef struct _JBL_PATCHEXT {
  const JBL_PATCH *p;
  JBL_PTR path;
  JBL_PTR from;
} JBL_PATCHEXT;

typedef struct _JBLDRCTX {
  IWPOOL *pool;
  JBL_NODE root;
} JBLDRCTX;

iwrc _jbl_write_double(double num, jbl_json_printer pt, void *op);
iwrc _jbl_write_int(int64_t num, jbl_json_printer pt, void *op);
iwrc _jbl_write_string(const char *str, size_t len, jbl_json_printer pt, void *op, jbl_print_flags_t pf);
iwrc _jbl_node_from_binn2(const binn *bn, JBL_NODE *node, IWPOOL *pool);
iwrc _jbl_from_node(binn *res, const JBL_NODE node);

typedef jbl_visitor_cmd_t (*JBL_VISITOR)(int lvl, binn *bv, const char *key, int idx, JBL_VCTX *vctx, iwrc *rc);
iwrc _jbl_visit(binn_iter *iter, int lvl, JBL_VCTX *vctx, JBL_VISITOR visitor);

bool _jbl_is_eq_values(JBL v1, JBL v2);

int _jbl_is_cmp_values(JBL v1, JBL v2);

#endif
