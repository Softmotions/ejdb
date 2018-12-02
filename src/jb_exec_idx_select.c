#include "ejdb2_internal.h"

#define JB_SOLID_EXPRNUM 127

//


static void _collect_solid_filters(struct JQP_EXPR_NODE *en, struct JQP_EXPR_NODE *sle, size_t *snp) {
  if (en->type == JQP_EXPR_NODE_TYPE) {
    struct JQP_EXPR_NODE *cn = en->chain;
    for (; cn; cn = cn->next) {
      if (en->join && en->join->value == JQP_JOIN_OR) return;
    }
    for (cn = en->chain; cn; cn = cn->next) {
      _collect_solid_filters(cn, sle, snp);
    }
  } else if (en->type == JQP_FILTER_TYPE) {

  }
}

iwrc jb_exec_idx_select(JBEXEC *ctx) {
  struct JQP_AUX *aux = ctx->ux->q->qp->aux;
  struct JQP_EXPR_NODE *expr = aux->expr;
  struct JQP_EXPR_NODE sle[JB_SOLID_EXPRNUM];

  ctx->iop_init = IWKV_CURSOR_BEFORE_FIRST;
  ctx->iop_step = IWKV_CURSOR_NEXT;
  ctx->iop_reverse_step = IWKV_CURSOR_PREV;

  if (ctx->jbc->idx) { // we have indexes associated with collection
    size_t snp = 0;
    _collect_solid_filters(aux->expr, sle, &snp);
  }

  if (!ctx->idx) {
    if (aux->orderby_num && ctx->iop_init != IWKV_CURSOR_EQ) {
      ctx->sorting = true;
    }
  }

  return 0;
}
