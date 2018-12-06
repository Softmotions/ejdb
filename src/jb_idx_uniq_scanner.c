#include "ejdb2_internal.h"

static iwrc jb_idx_consume_eq(struct _JBEXEC *ctx, const JQVAL *rval, JB_SCAN_CONSUMER consumer) {
  uint64_t id;
  int64_t step;
  size_t sz;
  struct _JBMIDX *midx = &ctx->midx;
  char buf[JBNUMBUF_SIZE];
  IWKV_val key = { .data = buf };

  jb_idx_jqval_fill_key(rval, &key);
  if (!key.size) {
    return IW_ERROR_ASSERTION;
  }
  iwrc rc = iwkv_get_copy(midx->idx->idb, &key, &id, sizeof(id), &sz);
  if (rc) {
    if (rc == IWKV_ERROR_NOTFOUND) {
      return consumer(ctx, 0, 0, 0, 0);
    } else {
      return rc;
    }
  }
  id = IW_ITOHLL(id);
  rc = consumer(ctx, 0, id, &step, 0);
  return consumer(ctx, 0, 0, 0, rc);
}

static iwrc jb_idx_consume_in_binn(struct _JBEXEC *ctx, JQVAL *rval, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}

static iwrc jb_idx_consume_in_node(struct _JBEXEC *ctx, JQVAL *rval, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}


//    for (int64_t i = 0; step && i < ctx->iop_key_cnt;) {
//      rc = iwkv_get_copy(idx->idb, &ctx->iop_key[i], &id, sizeof(id), &sz);
//      if (rc == IWKV_ERROR_NOTFOUND) continue;
//      RCBREAK(rc);
//      if (sz != sizeof(id)) {
//        rc = IWKV_ERROR_CORRUPTED;
//        iwlog_ecode_error3(rc);
//        break;
//      }
//      if (step > 0) --step;
//      else if (step < 0) ++step;
//      if (!step) {
//        do {
//          step = 1;
//          rc = consumer(ctx, 0, id, &step, 0);
//          RCBREAK(rc);
//        } while (step < 0 && !++step);
//      }
//
//return consumer(ctx, 0, 0, 0, rc);

static iwrc jb_idx_consume_scan(struct _JBEXEC *ctx, JQVAL *rval, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}

//  IWKV_cursor_op cursor_reverse_step = (ctx->cursor_step == IWKV_CURSOR_NEXT)
//                                       ? IWKV_CURSOR_PREV : IWKV_CURSOR_NEXT;
//
//  rc = iwkv_cursor_open(idx->idb, &cur, ctx->cursor_init, 0);
//  if (rc == IWKV_ERROR_NOTFOUND) return 0;
//  RCRET(rc);
//
//  if (ctx->cursor_init < IWKV_CURSOR_NEXT) { // IWKV_CURSOR_BEFORE_FIRST || IWKV_CURSOR_AFTER_LAST
//    rc = iwkv_cursor_to(cur, ctx->cursor_step);
//    RCGO(rc, finish);
//  }
//  do {
//    if (step > 0) --step;
//    else if (step < 0) ++step;
//    if (!step) {
//      rc = iwkv_cursor_copy_val(cur, &id, sizeof(id), &sz);
//      RCBREAK(rc);
//      if (sz != sizeof(id)) {
//        rc = IWKV_ERROR_CORRUPTED;
//        iwlog_ecode_error3(rc);
//        break;
//      }
//      do {
//        step = 1;
//        rc = consumer(ctx, cur, id, &step, 0);
//        RCBREAK(rc);
//      } while (step < 0 && !++step);
//    }
//  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? ctx->cursor_step : cursor_reverse_step)));
//  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
//
//finish:
//  if (cur) {
//    iwkv_cursor_close(&cur);
//  }
//  return consumer(ctx, 0, 0, 0, rc);

iwrc jb_idx_uniq_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  JQP_QUERY *qp = ctx->ux->q->qp;
  struct _JBMIDX *midx = &ctx->midx;
  JQVAL *rval = jql_unit_to_jqval(qp, midx->expr1->right, &rc);
  RCRET(rc);
  switch (midx->expr1->op->value) {
    case JQP_OP_EQ:
      return jb_idx_consume_eq(ctx, rval, consumer);
    case JQP_OP_IN:
      if (rval->type == JQVAL_BINN) {
        return jb_idx_consume_in_binn(ctx, rval, consumer);
      } else if (rval->type == JQVAL_JBLNODE) {
        return jb_idx_consume_in_node(ctx, rval, consumer);
      } else {
        return IW_ERROR_ASSERTION;
      }
      break;
    default:
      break;
  }
  return jb_idx_consume_scan(ctx, rval, consumer);
}
