#include "ejdb2_internal.h"

// Primary key scanner
iwrc jbi_pk_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc = 0;
  int64_t id, step;
  bool matched;
  struct JQP_AUX *aux = ctx->ux->q->aux;
  assert(aux->expr->flags & JQP_EXPR_NODE_FLAG_PK);
  JQP_EXPR_NODE_PK *pk = (void *) aux->expr;
  assert(pk->argument);
  JQVAL *jqval = jql_unit_to_jqval(aux, pk->argument, &rc);
  RCRET(rc);
  if (jql_jqval_as_int(jqval, &id)) {
    rc = consumer(ctx, 0, id, &step, &matched, 0);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

