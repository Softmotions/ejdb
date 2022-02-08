#include "ejdb2_internal.h"

static_assert(IW_VNUMBUFSZ <= JBNUMBUF_SIZE, "IW_VNUMBUFSZ <= JBNUMBUF_SIZE");

static iwrc _jbi_consume_eq(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  size_t sz;
  uint64_t id;
  int64_t step;
  bool matched;
  struct _JBMIDX *midx = &ctx->midx;
  char numbuf[JBNUMBUF_SIZE];
  IWKV_val key;

  jbi_jqval_fill_ikey(midx->idx, jqval, &key, numbuf);
  if (!key.size) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  }
  iwrc rc = iwkv_get_copy(midx->idx->idb, &key, numbuf, sizeof(numbuf), &sz);
  if (rc) {
    if (rc == IWKV_ERROR_NOTFOUND) {
      return consumer(ctx, 0, 0, 0, 0, 0);
    } else {
      return rc;
    }
  }
  IW_READVNUMBUF64_2(numbuf, id);
  rc = consumer(ctx, 0, id, &step, &matched, 0);
  return consumer(ctx, 0, 0, 0, 0, rc);
}

static iwrc _jbi_consume_in_node(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  JQVAL jqv;
  size_t sz;
  uint64_t id;
  bool matched;
  char numbuf[JBNUMBUF_SIZE];

  iwrc rc = 0;
  int64_t step = 1;
  IWKV_val key;
  struct _JBMIDX *midx = &ctx->midx;
  JBL_NODE nv = jqval->vnode->child;

  if (!nv) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  }
  do {
    jql_node_to_jqval(nv, &jqv);
    jbi_jqval_fill_ikey(midx->idx, &jqv, &key, numbuf);
    if (!key.size) {
      continue;
    }
    rc = iwkv_get_copy(midx->idx->idb, &key, numbuf, sizeof(numbuf), &sz);
    if (rc) {
      if (rc == IWKV_ERROR_NOTFOUND) {
        rc = 0;
        continue;
      } else {
        goto finish;
      }
    }
    if (step > 0) {
      --step;
    } else if (step < 0) {
      ++step;
    }
    if (!step) {
      IW_READVNUMBUF64_2(numbuf, id);
      step = 1;
      RCC(rc, finish, consumer(ctx, 0, id, &step, &matched, 0));
    }
  } while (step && (step > 0 ? (nv = nv->next) : (nv = nv->prev)));

finish:
  return consumer(ctx, 0, 0, 0, 0, rc);
}

static iwrc _jbi_consume_scan(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  size_t sz;
  IWKV_cursor cur;
  char numbuf[JBNUMBUF_SIZE];

  int64_t step = 1;
  struct _JBMIDX *midx = &ctx->midx;
  JBIDX idx = midx->idx;
  jqp_op_t expr1_op = midx->expr1->op->value;

  IWKV_val key;
  jbi_jqval_fill_ikey(idx, jqval, &key, numbuf);

  iwrc rc = iwkv_cursor_open(idx->idb, &cur, midx->cursor_init, &key);
  if ((rc == IWKV_ERROR_NOTFOUND) && ((expr1_op == JQP_OP_LT) || (expr1_op == JQP_OP_LTE))) {
    iwkv_cursor_close(&cur);
    midx->cursor_init = IWKV_CURSOR_BEFORE_FIRST;
    midx->cursor_step = IWKV_CURSOR_NEXT;
    RCC(rc, finish, iwkv_cursor_open(idx->idb, &cur, midx->cursor_init, 0));
    if (!midx->expr2) { // Fail fast
      midx->expr2 = midx->expr1;
    }
  } else if (rc) {
    goto finish;
  }

  IWKV_cursor_op cursor_reverse_step = (midx->cursor_step == IWKV_CURSOR_NEXT)
                                       ? IWKV_CURSOR_PREV : IWKV_CURSOR_NEXT;

  if (midx->cursor_init < IWKV_CURSOR_NEXT) { // IWKV_CURSOR_BEFORE_FIRST || IWKV_CURSOR_AFTER_LAST
    RCC(rc, finish, iwkv_cursor_to(cur, midx->cursor_step));
  }
  do {
    if (step > 0) {
      --step;
    } else if (step < 0) {
      ++step;
    }
    if (!step) {
      int64_t id;
      bool matched = false;
      RCC(rc, finish, iwkv_cursor_copy_val(cur, &numbuf, IW_VNUMBUFSZ, &sz));
      if (sz > IW_VNUMBUFSZ) {
        rc = IWKV_ERROR_CORRUPTED;
        iwlog_ecode_error3(rc);
        break;
      }
      IW_READVNUMBUF64_2(numbuf, id);
      if (  midx->expr2
         && !midx->expr2->prematched
         && !jbi_node_expr_matched(ctx->ux->q->aux, midx->idx, cur, midx->expr2, &rc)) {
        break;
      }
      if (  (expr1_op == JQP_OP_PREFIX)
         && !jbi_node_expr_matched(ctx->ux->q->aux, midx->idx, cur, midx->expr1, &rc)) {
        break;
      }
      RCGO(rc, finish);

      step = 1;
      RCC(rc, finish, consumer(ctx, 0, id, &step, &matched, 0));
      if (!midx->expr1->prematched && matched && (expr1_op != JQP_OP_PREFIX)) {
        // Further scan will always match the main index expression
        midx->expr1->prematched = true;
      }
    }
  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? midx->cursor_step : cursor_reverse_step)));

finish:
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
  }
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

iwrc _jbi_consume_noxpr_scan(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  size_t sz;
  IWKV_cursor cur;
  char numbuf[JBNUMBUF_SIZE];
  int64_t step = 1;
  struct _JBMIDX *midx = &ctx->midx;
  IWKV_cursor_op cursor_reverse_step = (midx->cursor_step == IWKV_CURSOR_NEXT)
                                       ? IWKV_CURSOR_PREV : IWKV_CURSOR_NEXT;

  RCC(rc, finish, iwkv_cursor_open(midx->idx->idb, &cur, midx->cursor_init, 0));
  if (midx->cursor_init < IWKV_CURSOR_NEXT) { // IWKV_CURSOR_BEFORE_FIRST || IWKV_CURSOR_AFTER_LAST
    RCC(rc, finish, iwkv_cursor_to(cur, midx->cursor_step));
  }
  do {
    if (step > 0) {
      --step;
    } else if (step < 0) {
      ++step;
    }
    if (!step) {
      int64_t id;
      bool matched;
      RCC(rc, finish, iwkv_cursor_copy_val(cur, &numbuf, IW_VNUMBUFSZ, &sz));
      if (sz > IW_VNUMBUFSZ) {
        rc = IWKV_ERROR_CORRUPTED;
        iwlog_ecode_error3(rc);
        break;
      }
      IW_READVNUMBUF64_2(numbuf, id);
      RCGO(rc, finish);
      step = 1;
      RCC(rc, finish, consumer(ctx, 0, id, &step, &matched, 0));
    }
  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? midx->cursor_step : cursor_reverse_step)));

finish:
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
  }
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

iwrc jbi_uniq_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  struct _JBMIDX *midx = &ctx->midx;
  if (!midx->expr1) {
    return _jbi_consume_noxpr_scan(ctx, consumer);
  }
  JQP_QUERY *qp = ctx->ux->q->qp;
  JQVAL *jqval = jql_unit_to_jqval(qp->aux, midx->expr1->right, &rc);
  RCRET(rc);
  switch (midx->expr1->op->value) {
    case JQP_OP_EQ:
      return _jbi_consume_eq(ctx, jqval, consumer);
    case JQP_OP_IN:
      if (jqval->type == JQVAL_JBLNODE) {
        return _jbi_consume_in_node(ctx, jqval, consumer);
      } else {
        iwlog_ecode_error3(IW_ERROR_ASSERTION);
        return IW_ERROR_ASSERTION;
      }
      break;
    default:
      break;
  }
  if ((midx->expr1->op->value == JQP_OP_GT) && (jqval->type == JQVAL_I64)) {
    JQVAL mjqv;
    memcpy(&mjqv, jqval, sizeof(*jqval));
    mjqv.vi64 = mjqv.vi64 + 1; // Because for index scan we use `IWKV_CURSOR_GE`
    return _jbi_consume_scan(ctx, &mjqv, consumer);
  } else {
    return _jbi_consume_scan(ctx, jqval, consumer);
  }
}
