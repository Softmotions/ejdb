#pragma once
#ifndef JQL_INTERNAL_H
#define JQL_INTERNAL_H

#include "jqp.h"

#include <iowow/iwbinn.h>
#include <iowow/iwre.h>
#include <math.h>

/** Query object */
struct jql {
  bool dirty;
  bool matched;
  bool has_negation; /**< Query expression contains negated (`not`) parts */
  struct jqp_query *qp;
  struct jqp_aux   *aux;
  const char       *coll;
  void *opaque;
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
typedef struct jqval {
  jqval_type_t type;
  void (*freefn)(void*, void*);
  void *freefn_op;
  int   refs;
  union {
    JBL_NODE    vnode;
    binn       *vbinn;
    int64_t     vi64;
    double      vf64;
    const char *vstr;
    struct iwre *vre;
    bool vbool;
  };
} JQVAL;

struct jqval* jql_find_placeholder(struct jql *q, const char *name);

struct jqval* jql_unit_to_jqval(struct jqp_aux *aux, union jqp_unit *unit, iwrc *rcp);

bool jql_jqval_as_int(struct jqval *jqval, int64_t *out);

jqval_type_t jql_binn_to_jqval(binn *vbinn, struct jqval *qval);

void jql_node_to_jqval(JBL_NODE jn, struct jqval *qv);

int jql_cmp_jqval_pair(const struct jqval *left, const struct jqval *right, iwrc *rcp);

bool jql_match_jqval_pair(struct jqp_aux *aux, struct jqval *left, struct jqp_op *jqop, struct jqval *right, iwrc *rcp);

#endif
