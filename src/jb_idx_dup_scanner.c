#include "ejdb2_internal.h"

static iwrc jb_idx_consume_eq(struct _JBEXEC *ctx, JQVAL *rv, JB_SCAN_CONSUMER consumer) {
  size_t sz;
  bool matched;
  IWKV_cursor cur;
  int64_t step = 1;
  char buf[JBNUMBUF_SIZE];
  struct _JBMIDX *midx = &ctx->midx;
  JBIDX idx = midx->idx;
  IWKV_cursor_op cursor_reverse_step = IWKV_CURSOR_NEXT;

  IWKV_val key = {.data = buf};
  jb_idx_jqval_fill_key(rv, &key);
  key.compound = INT64_MIN;

  iwrc rc = iwkv_cursor_open(idx->idb, &cur, IWKV_CURSOR_GE, &key);
  if (rc == IWKV_ERROR_NOTFOUND) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  } else RCRET(rc);

  char *dkey = malloc(key.size + IW_VNUMBUFSZ);
  if (!dkey) return iwrc_set_errno(IW_ERROR_ALLOC, errno);

  do {
    if (step > 0) --step;
    else if (step < 0) ++step;
    if (!step) {
      int vns;
      int64_t id;
      rc = iwkv_cursor_copy_key(cur, dkey, key.size + IW_VNUMBUFSZ, &sz);
      RCGO(rc, finish);
      IW_READVNUMBUF64(dkey, id, vns);
      if (key.size != sz - vns || memcmp(dkey + vns, key.data, key.size) != 0) {
        break;
      }
      do {
        step = 1;
        rc = consumer(ctx, 0, id, &step, &matched, 0);
        RCGO(rc, finish);
      } while (step < 0 && !++step);
    }
  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? midx->cursor_step : cursor_reverse_step)));

finish:
  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
  free(dkey);
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

static iwrc jb_idx_consume_in_node(struct _JBEXEC *ctx, JQVAL *rv, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}

static iwrc jb_idx_consume_scan(struct _JBEXEC *ctx, JQVAL *rv, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}

iwrc jb_idx_dup_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  JQP_QUERY *qp = ctx->ux->q->qp;
  struct _JBMIDX *midx = &ctx->midx;
  JQVAL *rval = jql_unit_to_jqval(qp->aux, midx->expr1->right, &rc);
  RCRET(rc);
  switch (midx->expr1->op->value) {
    case JQP_OP_EQ:
      return jb_idx_consume_eq(ctx, rval, consumer);
    case JQP_OP_IN:
      if (rval->type == JQVAL_JBLNODE) {
        return jb_idx_consume_in_node(ctx, rval, consumer);
      } else {
        iwlog_ecode_error3(IW_ERROR_ASSERTION);
        return IW_ERROR_ASSERTION;
      }
      break;
    default:
      break;
  }
  return jb_idx_consume_scan(ctx, rval, consumer);
}
