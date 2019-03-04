#include "ejdb2_internal.h"

static_assert(IW_VNUMBUFSZ <= JBNUMBUF_SIZE, "IW_VNUMBUFSZ <= JBNUMBUF_SIZE");

static iwrc jb_idx_consume_eq(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  size_t sz;
  uint64_t id;
  int64_t step;
  bool matched;
  struct _JBMIDX *midx = &ctx->midx;
  char buf[JBNUMBUF_SIZE];
  IWKV_val key = {.data = buf};

  jb_idx_jqval_fill_key(jqval, &key);
  if (!key.size) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  }
  iwrc rc = iwkv_get_copy(midx->idx->idb, &key, buf, sizeof(buf), &sz);
  if (rc) {
    if (rc == IWKV_ERROR_NOTFOUND) {
      return consumer(ctx, 0, 0, 0, 0, 0);
    } else {
      return rc;
    }
  }
  IW_READVNUMBUF64_2(buf, id);
  rc = consumer(ctx, 0, id, &step, &matched, 0);
  return consumer(ctx, 0, 0, 0, 0, rc);
}

static iwrc jb_idx_consume_in_node(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  JQVAL jqv;
  size_t sz;
  uint64_t id;
  bool matched;
  char buf[JBNUMBUF_SIZE];

  iwrc rc = 0;
  int64_t step = 1;
  IWKV_val key = {.data = buf};
  struct _JBMIDX *midx = &ctx->midx;
  JBL_NODE nv = jqval->vnode->child;

  if (!nv) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  }
  do {
    jql_node_to_jqval(nv, &jqv);
    if (jqv.type >= JQVAL_I64 && jqv.type <= JQVAL_STR) {
      jb_idx_jqval_fill_key(&jqv, &key);
      if (!key.size) {
        rc = IW_ERROR_ASSERTION;
        iwlog_ecode_error3(rc);
        goto finish;
      }
      rc = iwkv_get_copy(midx->idx->idb, &key, buf, sizeof(buf), &sz);
      if (rc) {
        if (rc == IWKV_ERROR_NOTFOUND) {
          rc = 0;
          continue;
        } else {
          goto finish;
        }
      }
      if (step > 0) --step;
      else if (step < 0) ++step;
      if (!step) {
        IW_READVNUMBUF64_2(buf, id);
        step = 1;
        rc = consumer(ctx, 0, id, &step, &matched, 0);
        RCGO(rc, finish);
      }
    }
  } while (step && (step > 0 ? (nv = nv->next) : (nv = nv->prev)));

finish:
  return consumer(ctx, 0, 0, 0, 0, rc);
}

static iwrc jb_idx_consume_scan(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  size_t sz;
  bool matched;
  IWKV_cursor cur;
  int64_t step = 1;
  char buf[JBNUMBUF_SIZE];
  struct _JBMIDX *midx = &ctx->midx;
  JBIDX idx = midx->idx;
  IWKV_val key = {.data = buf};
  jb_idx_jqval_fill_key(jqval, &key);

  iwrc rc = iwkv_cursor_open(idx->idb, &cur, midx->cursor_init, &key);
  if (rc == IWKV_ERROR_NOTFOUND && (midx->expr1->op->value == JQP_OP_LT || midx->expr1->op->value == JQP_OP_LTE)) {
    iwkv_cursor_close(&cur);
    midx->cursor_init = IWKV_CURSOR_BEFORE_FIRST;
    midx->cursor_step = IWKV_CURSOR_NEXT;
    rc = iwkv_cursor_open(idx->idb, &cur, midx->cursor_init, 0);
    RCGO(rc, finish);
    if (!midx->expr2) { // Fail fast
      midx->expr2 = midx->expr1;
    }
  } else RCRET(rc);

  IWKV_cursor_op cursor_reverse_step = (midx->cursor_step == IWKV_CURSOR_NEXT)
                                       ? IWKV_CURSOR_PREV : IWKV_CURSOR_NEXT;

  if (midx->cursor_init < IWKV_CURSOR_NEXT) { // IWKV_CURSOR_BEFORE_FIRST || IWKV_CURSOR_AFTER_LAST
    rc = iwkv_cursor_to(cur, midx->cursor_step);
    RCGO(rc, finish);
  }
  do {
    if (step > 0) --step;
    else if (step < 0) ++step;
    if (!step) {
      int64_t id;
      rc = iwkv_cursor_copy_val(cur, &buf, IW_VNUMBUFSZ, &sz);
      RCGO(rc, finish);
      if (sz > IW_VNUMBUFSZ) {
        rc = IWKV_ERROR_CORRUPTED;
        iwlog_ecode_error3(rc);
        break;
      }
      IW_READVNUMBUF64_2(buf, id);
      if (midx->expr2 && !midx->expr2->prematched
          && !jb_idx_node_expr_matched(ctx->ux->q->qp->aux, midx->idx, cur, midx->expr2, &rc)) {
        break;
      }
      RCGO(rc, finish);
      step = 1;
      matched = false;
      rc = consumer(ctx, 0, id, &step, &matched, 0);
      RCGO(rc, finish);
      if (!midx->expr1->prematched && matched) {
        // Further scan will always match main index expression
        midx->expr1->prematched = true;
      }
    }
  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? midx->cursor_step : cursor_reverse_step)));

finish:
  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

iwrc jb_idx_uniq_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  JQP_QUERY *qp = ctx->ux->q->qp;
  struct _JBMIDX *midx = &ctx->midx;
  JQVAL *jqval = jql_unit_to_jqval(qp->aux, midx->expr1->right, &rc);
  RCRET(rc);
  switch (midx->expr1->op->value) {
    case JQP_OP_EQ:
      return jb_idx_consume_eq(ctx, jqval, consumer);
    case JQP_OP_IN:
      if (jqval->type == JQVAL_JBLNODE) {
        return jb_idx_consume_in_node(ctx, jqval, consumer);
      } else {
        iwlog_ecode_error3(IW_ERROR_ASSERTION);
        return IW_ERROR_ASSERTION;
      }
      break;
    default:
      break;
  }
  if (midx->expr1->op->value == JQP_OP_GT && jqval->type == JQVAL_I64) {
    JQVAL mjqv;
    memcpy(&mjqv, jqval, sizeof(*jqval));
    mjqv.vi64 = mjqv.vi64 + 1; // Because for index scan we use `IWKV_CURSOR_GE`
    return jb_idx_consume_scan(ctx, &mjqv, consumer);
  } else {
    return jb_idx_consume_scan(ctx, jqval, consumer);
  }
}
