#include "ejdb2_internal.h"

#define JB_SOLID_EXPRNUM 127

static void jb_print_index(struct _JBIDX *idx, IWXSTR *xstr) {
  int cnt = 0;
  ejdb_idx_mode_t m = idx->mode;
  if (m & EJDB_IDX_UNIQUE) {
    cnt++;
    iwxstr_cat2(xstr, "UNIQUE");
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
  jb_print_index(mctx->idx, xstr);
  if (mctx->expr1) {
    iwxstr_cat2(xstr, " EXPR1: \'");
    jqp_print_filter_node_expr(mctx->expr1, jbl_xstr_json_printer, xstr);
    iwxstr_cat2(xstr, "\'");
  }
  if (mctx->expr2) {
    iwxstr_cat2(xstr, " EXPR2: \'");
    jqp_print_filter_node_expr(mctx->expr2, jbl_xstr_json_printer, xstr);
    iwxstr_cat2(xstr, "\'");
  }
  if (mctx->cursor_init) {
    iwxstr_cat2(xstr, " INIT: ");
    jb_log_cursor_op(xstr, mctx->cursor_init);
  }
  if (mctx->cursor_step) {
    iwxstr_cat2(xstr, " STEP: ");
    jb_log_cursor_op(xstr, mctx->cursor_step);
  }
  if (mctx->orderby_support) {
    iwxstr_cat2(xstr, " ORDERBY");
  }
  iwxstr_cat2(xstr, "\n");
}

IW_INLINE int jb_idx_expr_op_weight(struct _JBMIDX *midx) {
  jqp_op_t op = midx->expr1->op->value;
  switch (op) {
    case JQP_OP_EQ:
      return 10;
    case JQP_OP_IN:
      //case JQP_OP_NI: todo
      return 9;
    default:
      break;
  }
  if (midx->orderby_support) {
    return 8;
  }
  switch (op) {
    case JQP_OP_GT:
    case JQP_OP_GTE:
      return 7;
    case JQP_OP_LT:
    case JQP_OP_LTE:
      return 6;
    default:
      return 0;
  }
}

static bool jb_is_solid_node_expression(const JQP_NODE *n) {
  JQPUNIT *unit = n->value;
  for (const JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) {
    if (
      expr->op->negate
      || (expr->join && (expr->join->negate || expr->join->value == JQP_JOIN_OR))
      || expr->op->value == JQP_OP_RE) {
      // No negate conditions, No OR, No regexp
      return false;
    }
    JQPUNIT *left = expr->left;
    if (
      left->type == JQP_EXPR_TYPE
      || (left->type == JQP_STRING_TYPE && (left->string.flavour & JQP_STR_STAR))) {
      return false;
    }
  }
  return true;
}

static iwrc jb_compute_index_rules(JBEXEC *ctx, struct _JBMIDX *mctx) {
  JQP_EXPR *expr = mctx->nexpr; // Node expression
  if (!expr) return 0;
  JQP_AUX *aux = ctx->ux->q->qp->aux;

  for (; expr; expr = expr->next) {
    iwrc rc = 0;
    jqp_op_t op = expr->op->value;
    JQVAL *rv = jql_unit_to_jqval(aux, expr->right, &rc);
    RCRET(rc);
    if (expr->left->type != JQP_STRING_TYPE) {
      continue;
    }
    switch (rv->type) {
      case JQVAL_NULL:
      case JQVAL_RE:
      case JQVAL_BINN:
        continue;
      case JQVAL_JBLNODE: {
        if (op != JQP_OP_IN || rv->vnode->type != JBV_ARRAY) {
          continue;
        }
        int vcnt = 0;
        for (JBL_NODE n = rv->vnode->child; n; n = n->next, ++vcnt);
        if (
          vcnt > JB_IDX_EMPIRIC_MIN_INOP_ARRAY_SIZE
          && (vcnt > JB_IDX_EMPIRIC_MAX_INOP_ARRAY_SIZE
              || mctx->idx->rnum < rv->vbinn->count * JB_IDX_EMPIRIC_MAX_INOP_ARRAY_RATIO)
        ) {
          // No index for large IN array | small collection size
          continue;
        }
        break;
      }
      default:
        break;
    }
    switch (op) {
      case JQP_OP_EQ:
        mctx->cursor_init = IWKV_CURSOR_EQ;
        mctx->expr1 = expr;
        mctx->expr2 = 0;
        return 0;
      case JQP_OP_GT:
      case JQP_OP_GTE:
        if (mctx->cursor_init != IWKV_CURSOR_EQ) {
          if (mctx->expr1 && mctx->cursor_init == IWKV_CURSOR_GE) {
            JQVAL *pval = jql_unit_to_jqval(aux, mctx->expr1->right, &rc);
            RCRET(rc);
            int cv = jql_cmp_jqval_pair(pval, rv, &rc);
            RCRET(rc);
            if (cv > 0) {
              break;
            }
          }
          mctx->expr1 = expr;
          mctx->cursor_init = IWKV_CURSOR_GE;
          mctx->cursor_step = IWKV_CURSOR_PREV;
        }
        break;
      case JQP_OP_LT:
      case JQP_OP_LTE:
        if (mctx->expr2) {
          JQVAL *pval = jql_unit_to_jqval(aux, mctx->expr2->right, &rc);
          RCRET(rc);
          int cv = jql_cmp_jqval_pair(pval, rv, &rc);
          RCRET(rc);
          if (cv < 0) {
            break;
          }
        }
        mctx->expr2 = expr;
        break;
      case JQP_OP_IN:
        if (mctx->cursor_init != IWKV_CURSOR_EQ && rv->type >= JQVAL_JBLNODE) {
          mctx->expr1 = expr;
          mctx->expr2 = 0;
          mctx->cursor_init = IWKV_CURSOR_EQ;
        }
        break;
      default:
        continue;
    }
  }

  if (mctx->expr2) {
    if (!mctx->expr1) {
      mctx->expr1 = mctx->expr2;
      mctx->expr2 = 0;
      mctx->cursor_init = IWKV_CURSOR_GE;
      mctx->cursor_step = IWKV_CURSOR_NEXT;
    }
  }

  // Orderby compatibility
  if (mctx->orderby_support) {
    if (aux->orderby_num == 1 && mctx->cursor_init != IWKV_CURSOR_EQ) {
      bool desc = (aux->orderby_ptrs[0]->op & 1) != 0; // Desc sort
      if (desc) {
        if (mctx->cursor_step != IWKV_CURSOR_NEXT) {
          mctx->orderby_support = false;
        }
      } else {
        if (mctx->cursor_step != IWKV_CURSOR_PREV) {
          mctx->orderby_support = false;
        }
      }
      if (!mctx->orderby_support && mctx->expr2) {
        JQP_EXPR *tmp = mctx->expr1;
        mctx->expr1 = mctx->expr2;
        mctx->expr2 = tmp;
        mctx->orderby_support = true;
        if (desc) {
          mctx->cursor_step = IWKV_CURSOR_NEXT;
        } else {
          mctx->cursor_step = IWKV_CURSOR_PREV;
        }
      }
    } else {
      mctx->orderby_support = false;
    }
  }

  return 0;
}

static iwrc jb_collect_indexes(JBEXEC *ctx,
                               const struct JQP_EXPR_NODE *en,
                               struct _JBMIDX marr[static JB_SOLID_EXPRNUM],
                               size_t *snp) {

  iwrc rc = 0;
  if (*snp >=  JB_SOLID_EXPRNUM - 1) {
    return 0;
  }
  if (en->type == JQP_EXPR_NODE_TYPE) {
    struct JQP_EXPR_NODE *cn = en->chain;
    for (; cn; cn = cn->next) {
      if (en->join && (en->join->value == JQP_JOIN_OR || en->join->negate)) {
        return 0;
      }
    }
    for (cn = en->chain; cn; cn = cn->next) {
      rc = jb_collect_indexes(ctx, cn, marr, snp);
      RCRET(rc);
    }
  } else if (en->type == JQP_FILTER_TYPE) {
    int fnc = 0;
    JQP_FILTER *f = (JQP_FILTER *) en;
    for (JQP_NODE *n = f->node; n; n = n->next, ++fnc) {
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
    struct JQP_AUX *aux = ctx->ux->q->qp->aux;
    struct _JBL_PTR *obp = aux->orderby_num ? aux->orderby_ptrs[0] : 0;

    for (struct _JBIDX *idx = ctx->jbc->idx; idx && *snp < JB_SOLID_EXPRNUM; idx = idx->next) {
      struct _JBMIDX mctx = {.filter = f};
      struct _JBL_PTR *ptr = idx->ptr;
      if (ptr->cnt > fnc) continue;

      JQP_EXPR *nexpr = 0;
      int i = 0, j = 0;
      for (JQP_NODE *n = f->node; n && i < ptr->cnt; n = n->next, ++i) {
        nexpr = 0;
        const char *field = 0;
        if (n->ntype == JQP_NODE_FIELD) {
          field = n->value->string.value;
        } else if (n->ntype == JQP_NODE_EXPR) {
          nexpr = &n->value->expr;
          JQPUNIT *left = nexpr->left;
          if (left->type == JQP_STRING_TYPE) {
            field = left->string.value;
          }
        }
        if (!field || strcmp(field, ptr->n[i]) != 0) {
          break;
        }
        if (obp && i == j && i < obp->cnt && !strcmp(ptr->n[i], obp->n[i])) {
          j++;
        }
        // Check for the last iteration and the special `**` case
        if (i == ptr->cnt - 1
            && (idx->idbf & IWDB_COMPOUND_KEYS)
            && n->next && !n->next->next && n->next->ntype == JQP_NODE_EXPR) {
          JQPUNIT *left = n->next->value->expr.left;
          if (left->type == JQP_STRING_TYPE && (left->string.flavour & JQP_STR_DBL_STAR)) {
            i++;
            j++;
            nexpr = &n->next->value->expr;
            break;
          }
        }
      }
      if (i == ptr->cnt && nexpr) {
        mctx.idx = idx;
        mctx.nexpr = nexpr;
        mctx.orderby_support = (i == j);
        rc = jb_compute_index_rules(ctx, &mctx);
        RCRET(rc);
        if (ctx->ux->log) {
          iwxstr_cat2(ctx->ux->log, "[INDEX] MATCHED  ");
          jb_log_index_rules(ctx->ux->log, &mctx);
        }
        marr[*snp] = mctx;
        *snp = *snp + 1;
      }
    }
  }
  return rc;
}

static int jb_idx_cmp(const void *o1, const void *o2) {
  struct _JBMIDX *d1 = (struct _JBMIDX *) o1;
  struct _JBMIDX *d2 = (struct _JBMIDX *) o2;
  assert(d1 && d2);
  int w1 = jb_idx_expr_op_weight(d1);
  int w2 = jb_idx_expr_op_weight(d2);
  if (w2 - w1) {
    return w2 - w1;
  }
  w1 = d1->expr2 != 0;
  w2 = d2->expr2 != 0;
  if (w2 - w1) {
    return w2 - w1;
  }
  if (d1->idx->rnum - d2->idx->rnum) {
    return (d1->idx->rnum - d2->idx->rnum) > 0 ? 1 : -1;
  }
  return (d1->idx->ptr->cnt - d2->idx->ptr->cnt);
}

iwrc jb_idx_selection(JBEXEC *ctx) {
  iwrc rc = 0;
  size_t snp = 0;
  struct JQP_AUX *aux = ctx->ux->q->qp->aux;
  struct _JBMIDX fctx[JB_SOLID_EXPRNUM] = {0};

  ctx->cursor_init = IWKV_CURSOR_BEFORE_FIRST;
  ctx->cursor_step = IWKV_CURSOR_NEXT;

  if (ctx->jbc->idx) { // we have indexes associated with collection
    rc = jb_collect_indexes(ctx, aux->expr, fctx, &snp);
    RCRET(rc);
    if (snp) {
      qsort(fctx, snp, sizeof(fctx[0]), jb_idx_cmp);
      memcpy(&ctx->midx, &fctx[0], sizeof(ctx->midx));
      struct _JBMIDX *midx = &ctx->midx;
      jqp_op_t op = midx->expr1->op->value;
      if (op == JQP_OP_EQ || op == JQP_OP_IN || (op == JQP_OP_GTE && ctx->cursor_init == IWKV_CURSOR_GE)) {
        midx->expr1->prematched = true;
      }
      if (ctx->ux->log) {
        iwxstr_cat2(ctx->ux->log, "[INDEX] SELECTED ");
        jb_log_index_rules(ctx->ux->log, &ctx->midx);
      }
      if (midx->orderby_support && aux->orderby_num == 1) {
        // Turn off final sorting since it supported by natural index scan order
        ctx->sorting = false;
      } else if (aux->orderby_num) {
        ctx->sorting = true;
      }
      goto finish;
    }
  }

  // Index not found:
  if (aux->orderby_num) {
    ctx->sorting = true;
  }

finish:
  return rc;
}
