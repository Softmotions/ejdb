#include "ejdb2_internal.h"

// Primary key scanner
iwrc jbi_pk_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc = 0;
  int64_t id, step;
  bool matched;
  struct JQP_AUX *aux = ctx->ux->q->aux;
  assert(aux->expr->flags & JQP_EXPR_NODE_FLAG_PK);
  JQP_EXPR_NODE_PK *pk = (void*) aux->expr;
  assert(pk->argument);
  JQVAL *jqvp = jql_unit_to_jqval(aux, pk->argument, &rc);
  RCGO(rc, finish);

  if ((jqvp->type == JQVAL_JBLNODE) && (jqvp->vnode->type == JBV_ARRAY)) {
    JQVAL jqv;
    JBL_NODE nv = jqvp->vnode->child;
    if (!nv) {
      goto finish;
    }
    step = 1;
    do {
      jql_node_to_jqval(nv, &jqv);
      if (jql_jqval_as_int(&jqv, &id)) {
        if (step > 0) {
          --step;
        } else if (step < 0) {
          ++step;
        }
        if (!step) {
          step = 1;
          rc = consumer(ctx, 0, id, &step, &matched, 0);
          RCGO(rc, finish);
        }
      }
    } while (step && (step > 0 ? (nv = nv->next) : (nv = nv->prev)));
  } else if (jql_jqval_as_int(jqvp, &id)) {
    rc = consumer(ctx, 0, id, &step, &matched, 0);
  }

finish:
  return consumer(ctx, 0, 0, 0, 0, rc);
}
