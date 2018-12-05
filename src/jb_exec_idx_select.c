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
  if (cnt++) iwxstr_cat2(xstr, "|");
  iwxstr_printf(xstr, "%lld ", idx->rnum);
  jbl_ptr_serialize(idx->ptr, xstr);
}

static void jb_log_cursor_op(IWXSTR *xstr, IWKV_cursor_op op) {
  switch (op) {
    case IWKV_CURSOR_EQ:
      iwxstr_cat2(xstr, "IWKV_CURSOR_EQ");
      break;
    case IWKV_CURSOR_GE:
      iwxstr_cat2(xstr, "IWKV_CURSOR_GE");
      break;
    case IWKV_CURSOR_NEXT:
      iwxstr_cat2(xstr, "IWKV_CURSOR_NEXT");
      break;
    case IWKV_CURSOR_PREV:
      iwxstr_cat2(xstr, "IWKV_CURSOR_PREV");
      break;
    case IWKV_CURSOR_BEFORE_FIRST:
      iwxstr_cat2(xstr, "IWKV_CURSOR_BEFORE_FIRST");
      break;
    case IWKV_CURSOR_AFTER_LAST:
      iwxstr_cat2(xstr, "IWKV_CURSOR_AFTER_LAST");
      break;
  }
}

static void jb_log_index_rules(IWXSTR *xstr, struct _JBMIDX *mctx) {
  iwxstr_printf(xstr, "[INDEX] ");
  jb_print_index(mctx->idx, xstr);
  iwxstr_printf(xstr, " expr=%s", (mctx->nexpr ? "yes" : "no"));
  if (mctx->cursor_init) {
    iwxstr_cat2(xstr, " init=");
    jb_log_cursor_op(xstr, mctx->cursor_init);
  }
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

static iwrc jb_compute_index_rules(JBEXEC *ctx, struct _JBMIDX *mctx) {
  JQP_EXPR *expr = mctx->nexpr; // Node expressoon
  if (!expr) return 0;
  JQP_QUERY *qp = ctx->ux->q->qp;
  iwrc rc = 0;
  for (; expr && !rc; expr = expr->next) {
    jqp_op_t op = expr->op->op;
    JQVAL *rval = jql_unit_to_jqval(qp, expr->right, &rc);
    RCRET(rc);
    switch (rval->type) {
      case JQVAL_NULL:
      case JQVAL_RE:
      case JQVAL_JBLNODE:
      case JQVAL_BINN:
        continue;
      default:
        break;
    }
    if (expr->left->type == JQP_STRING_TYPE) {
      switch (op) {
        case JQP_OP_EQ:
          mctx->cursor_init = IWKV_CURSOR_EQ;
          mctx->cursor_key = rval;
          expr->disabled = true;
          goto finish;
        case JQP_OP_GT:
        case JQP_OP_GTE:
          mctx->cursor_key = rval;
          mctx->cursor_init = IWKV_CURSOR_GE;
          mctx->cursor_step = IWKV_CURSOR_NEXT;
          break;
        case JQP_OP_LT:
        case JQP_OP_LTE:
          if (!mctx->cursor_key) {
            mctx->cursor_key = rval;
            mctx->cursor_init = IWKV_CURSOR_GE;
            mctx->cursor_step = IWKV_CURSOR_PREV;
          }
          break;
        case JQP_OP_IN:
          // TODO:
          break;
        default:
          break;
      }
    }
  }
finish:
  return rc;
}

static iwrc jb_collect_indexes(JBEXEC *ctx,
                               const struct JQP_EXPR_NODE *en,
                               struct _JBMIDX marr[static JB_SOLID_EXPRNUM],
                               size_t *snp) {
  iwrc rc = 0;
  if (*snp >= JB_SOLID_EXPRNUM - 1) {
    return 0;
  }
  if (en->type == JQP_EXPR_NODE_TYPE) {
    struct JQP_EXPR_NODE *cn = en->chain;
    for (; cn; cn = cn->next) {
      if (en->join && en->join->value == JQP_JOIN_OR) {
        return 0;
      }
    }
    for (cn = en->chain; cn; cn = cn->next) {
      rc = jb_collect_indexes(ctx, cn, marr, snp);
      RCRET(rc);
    }
  } else if (en->type == JQP_FILTER_TYPE) {
    int fnc = 0;
    JQP_FILTER *f  = (JQP_FILTER *) en;
    for (JQP_NODE *n = f->node; n; n = n->next) {
      switch (n->ntype) {
        case JQP_NODE_ANY:
        case JQP_NODE_ANYS:
          return 0;
        case JQP_NODE_FIELD:
          break;
        case JQP_NODE_EXPR:
          if (!jb_is_solid_node_expression(n)) {
            return 0;
          }
          break;
      }
    }
    for (JQP_NODE *n = f->node; n; n = n->next) {
      ++fnc;
    }
    // Try to find matched index
    for (struct _JBIDX *idx = ctx->jbc->idx; idx; idx = idx->next) {
      struct _JBMIDX mctx = { .filter = f };
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
        rc = jb_compute_index_rules(ctx, &mctx);
        RCRET(rc);
        if (ctx->ux->log) {
          jb_log_index_rules(ctx->ux->log, &mctx);
        }
        *snp = *snp + 1;
        marr[*snp] = mctx;
      }
    }
  }
  return rc;
}

iwrc jb_exec_idx_select(JBEXEC *ctx) {
  iwrc rc = 0;
  struct JQP_AUX *aux = ctx->ux->q->qp->aux;
  struct JQP_EXPR_NODE *expr = aux->expr;
  struct _JBMIDX fctx[JB_SOLID_EXPRNUM] = {0};

  ctx->cursor_init = IWKV_CURSOR_BEFORE_FIRST;
  ctx->cursor_step = IWKV_CURSOR_NEXT;
  ctx->cursor_reverse_step = IWKV_CURSOR_PREV;

  if (ctx->jbc->idx) { // we have indexes associated with collection
    size_t snp = 0;
    rc = jb_collect_indexes(ctx, aux->expr, fctx, &snp);
  } else {
    if (aux->orderby_num) {
      ctx->sorting = true;
    }
  }
  return rc;
}
