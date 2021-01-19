#pragma once
#ifndef JQL_INTERNAL_H
#define JQL_INTERNAL_H

#include "jqp.h"
#include "binn.h"
#include <math.h>


/** Query object */
struct _JQL {
  bool       dirty;
  bool       matched;
  JQP_QUERY *qp;
  JQP_AUX   *aux;
  const char *coll;
  void       *opaque;
};

/** Placeholder value type */
typedef enum {
  JQVAL_NULL,  // Do not reorder
  JQVAL_I64,
  JQVAL_F64,
  JQVAL_STR,
  JQVAL_BOOL,
  JQVAL_RE,
  JQVAL_JBLNODE, // Do not reorder JQVAL_JBLNODE,JQVAL_BINN must be last
  JQVAL_BINN,
} jqval_type_t;

/** Placeholder value */
typedef struct {
  jqval_type_t type;
  void (*freefn)(void*, void*);
  void *freefn_op;
  union {
    JBL_NODE    vnode;
    binn       *vbinn;
    int64_t     vi64;
    double      vf64;
    const char *vstr;
    bool       vbool;
    struct re *vre;
  };
} JQVAL;

JQVAL *jql_find_placeholder(JQL q, const char *name);

JQVAL *jql_unit_to_jqval(JQP_AUX *aux, JQPUNIT *unit, iwrc *rcp);

bool jql_jqval_as_int(JQVAL *jqval, int64_t *out);

jqval_type_t jql_binn_to_jqval(binn *vbinn, JQVAL *qval);

void jql_node_to_jqval(JBL_NODE jn, JQVAL *qv);

int jql_cmp_jqval_pair(const JQVAL *left, const JQVAL *right, iwrc *rcp);

bool jql_match_jqval_pair(JQP_AUX *aux, JQVAL *left, JQP_OP *jqop, JQVAL *right, iwrc *rcp);

#endif
