#include "ejdb2_internal.h"

#define JB_SOLID_EXPRNUM 127

static void jb_print_index(struct _JBIDX *idx, IWXSTR *xstr) {
  int cnt = 0;
  ejdb_idx_mode_t m = idx->mode;
  if (m & EJDB_IDX_UNIQUE) {
    cnt++;
    iwxstr_cat2(xstr, "UNIQUE");
  }
  if (m & EJDB_IDX_ARR) {
    if (cnt++) iwxstr_cat2(xstr, "|");
    iwxstr_cat2(xstr, "ARR");
  }
  if (m & EJDB_IDX_STR) {
    if (cnt++) iwxstr_cat2(xstr, "|");
    iwxstr_cat2(xstr, "STR");
  }
  if (m & EJDB_IDX_I64) {
    if (cnt++) iwxstr_cat2(xstr, "|");
    iwxstr_cat2(xstr, "I64");
  }
  if (m & EJDB_IDX_F64) {
    if (cnt++) iwxstr_cat2(xstr, "|");
    iwxstr_cat2(xstr, "F64");
  }
  jbl_ptr_serialize(idx->ptr, xstr);
}


static bool jb_is_solid_node_expression(const JQP_NODE *n) {
  const JQPUNIT *unit = n->value;
  for (const JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) {
    if ((expr->join && expr->join->negate) || expr->op->op == JQP_OP_RE) {
      // No negate conditions, No regexp
      return false;
    }
    JQPUNIT *left = expr->left;
    if ((left->type == JQP_STRING_TYPE && (left->string.flavour & JQP_STR_STAR))
        || left->type == JQP_EXPR_TYPE) {
      return false;
    }
  }
  return true;
}

static void jb_fill_solid_filters(JBEXEC *ctx,
                                  const struct JQP_EXPR_NODE *en,
                                  struct _JBMIDX marr[static JB_SOLID_EXPRNUM],
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
      jb_fill_solid_filters(ctx, cn, marr, snp);
    }
  } else if (en->type == JQP_FILTER_TYPE) {
    int fnc = 0;
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
    for (JQP_NODE *n = f->node; n; n = n->next) {
      ++fnc;
    }
    struct _JBMIDX mctx = {
      .filter = f
    };
    // Try to find matched index
    for (struct _JBIDX *idx = ctx->jbc->idx; idx && !mctx.idx; idx = idx->next) {
      struct _JBL_PTR *ptr = idx->ptr;
      if (ptr->cnt > fnc) continue;
      JQP_EXPR *nexpr = 0;
      int i = 0;
      for (JQP_NODE *n = f->node; n; n = n->next, ++i) { // Examine matched index
        nexpr = 0;
        const char *field;
        if (n->ntype == JQP_NODE_FIELD) {
          field = n->value->string.value;
        } else if (n->ntype == JQP_NODE_EXPR) {
          nexpr = &n->value->expr;
          JQPUNIT *left = nexpr->left; // Left side of first node expression
          if (left->type == JQP_STRING_TYPE) {
            field = left->string.value;
          }
        }
        if (!field && strcmp(field, ptr->n[i])) {
          break;
        }
      }
      if (i == ptr->cnt) { // Found matched index
        mctx.nexpr = nexpr;
        mctx.idx = idx;
        break;
      }
    }
    if (mctx.idx) {
      *snp = *snp + 1;
      marr[*snp] = mctx;
      if (ctx->ux->log) {
        iwxstr_printf(ctx->ux->log, "[INDEX] Found candidate: ");
        jb_print_index(mctx.idx, ctx->ux->log);
        iwxstr_printf(ctx->ux->log, " EXPR: %s\n", (mctx.nexpr ? "YES" : "NO"));
      }
    }
  }
}

iwrc jb_exec_idx_select(JBEXEC *ctx) {
  struct JQP_AUX *aux = ctx->ux->q->qp->aux;
  struct JQP_EXPR_NODE *expr = aux->expr;
  struct _JBMIDX fctx[JB_SOLID_EXPRNUM] = {0};

  ctx->cursor_init = IWKV_CURSOR_BEFORE_FIRST;
  ctx->cursor_step = IWKV_CURSOR_NEXT;
  ctx->cursor_reverse_step = IWKV_CURSOR_PREV;

  if (ctx->jbc->idx) { // we have indexes associated with collection
    size_t snp = 0;
    jb_fill_solid_filters(ctx, aux->expr, fctx, &snp);
  } else {
    if (aux->orderby_num) {
      ctx->sorting = true;
    }
  }

  return 0;
}
