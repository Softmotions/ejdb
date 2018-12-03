#include "ejdb2_internal.h"

#define JB_SOLID_EXPRNUM 127

struct IDX_FILTER_CTX {
  JQP_FILTER *filter;   /**< Query filter */
  JBIDX idx;            /**< Index matched this filter */
};

static bool jb_is_solid_node_expression(const JQP_NODE *n) {
  const JQPUNIT *unit = n->value;
  for (const JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) {
    if ((expr->join && expr->join->negate) || expr->op->op == JQP_OP_RE) {
      // No negate conditions, No regexp
      return false;
    }
    JQPUNIT *left = expr->left;
    if (left->type == JQP_EXPR_TYPE
        && left->expr.left->type == JQP_STRING_TYPE
        && (left->expr.left->string.flavour & JQP_STR_STAR)) {
      return false; // TODO: review later
    }
  }
  return true;
}

static void jb_fill_solid_filters(const struct JQP_EXPR_NODE *en,
                                  struct IDX_FILTER_CTX fctx[static JB_SOLID_EXPRNUM],
                                  size_t *snp) {
  if (*snp >= JB_SOLID_EXPRNUM - 1) {
    return;
  }
  if (en->type == JQP_EXPR_NODE_TYPE) {
    struct JQP_EXPR_NODE *cn = en->chain;
    for (; cn; cn = cn->next) {
      if (en->join && en->join->value == JQP_JOIN_OR) {
        return;
      }
    }
    for (cn = en->chain; cn; cn = cn->next) {
      jb_fill_solid_filters(cn, fctx, snp);
    }
  } else if (en->type == JQP_FILTER_TYPE) {
    JQP_FILTER *f  = (JQP_FILTER *) en;
    for (JQP_NODE *n = f->node; n; n = n->next) {
      switch (n->ntype) {
        case JQP_NODE_ANY:
        case JQP_NODE_ANYS:
          return;
        case JQP_NODE_FIELD:
          break;
        case JQP_NODE_EXPR:
          if (!jb_is_solid_node_expression(n)) {
            return;
          }
          break;
      }
    }
    if (*snp < JB_SOLID_EXPRNUM - 1) {
      *snp = *snp + 1;
      fctx[*snp].filter = f;
    }
  }
}

iwrc jb_exec_idx_select(JBEXEC *ctx) {
  struct JQP_AUX *aux = ctx->ux->q->qp->aux;
  struct JQP_EXPR_NODE *expr = aux->expr;
  struct IDX_FILTER_CTX fctx[JB_SOLID_EXPRNUM] = {0};

  ctx->iop_init = IWKV_CURSOR_BEFORE_FIRST;
  ctx->iop_step = IWKV_CURSOR_NEXT;
  ctx->iop_reverse_step = IWKV_CURSOR_PREV;

  if (ctx->jbc->idx) { // we have indexes associated with collection
    size_t snp = 0;
    jb_fill_solid_filters(aux->expr, fctx, &snp);
  }

  if (!ctx->idx) {
    if (aux->orderby_num && ctx->iop_init != IWKV_CURSOR_EQ) {
      ctx->sorting = true;
    }
  }

  return 0;
}
