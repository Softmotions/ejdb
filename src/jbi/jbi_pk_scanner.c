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

  switch (jqval->type) {
    case JQVAL_I64:
      id = jqval->vi64;
      break;
    case JQVAL_STR:
      id = iwatoi(jqval->vstr);
      break;
    case JQVAL_F64:
      id = jqval->vf64;
      break;
    case JQVAL_BOOL:
      id = jqval->vbool ? 1 : 0;
      break;
    default:
      goto finish;
  }
  rc = consumer(ctx, 0, id, &step, &matched, 0);
finish:
  return consumer(ctx, 0, 0, 0, 0, rc);
}

