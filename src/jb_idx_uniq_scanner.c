#include "ejdb2_internal.h"

iwrc jb_idx_uniq_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc = 0;
  size_t sz;
  uint64_t id;
  int64_t step = 1;
  IWKV_cursor cur = 0;
  JQP_QUERY *qp = ctx->ux->q->qp;
  struct _JBMIDX *midx = &ctx->midx;
  JBIDX idx = midx->idx;
  assert(idx);
  JQVAL *rval = jql_unit_to_jqval(qp, midx->expr1->right, &rc);
  RCRET(rc);

  if (midx->cursor_init == IWKV_CURSOR_EQ) {

  }

  //if (ctx->cursor_init == IWKV_CURSOR_EQ) {
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
    //    }
    //return consumer(ctx, 0, 0, 0, rc);
  //}

  IWKV_cursor_op cursor_reverse_step = (ctx->cursor_step == IWKV_CURSOR_NEXT)
                                       ? IWKV_CURSOR_PREV : IWKV_CURSOR_NEXT;

  rc = iwkv_cursor_open(idx->idb, &cur, ctx->cursor_init, 0);
  if (rc == IWKV_ERROR_NOTFOUND) return 0;
  RCRET(rc);

  if (ctx->cursor_init < IWKV_CURSOR_NEXT) { // IWKV_CURSOR_BEFORE_FIRST || IWKV_CURSOR_AFTER_LAST
    rc = iwkv_cursor_to(cur, ctx->cursor_step);
    RCGO(rc, finish);
  }
  do {
    if (step > 0) --step;
    else if (step < 0) ++step;
    if (!step) {
      rc = iwkv_cursor_copy_val(cur, &id, sizeof(id), &sz);
      RCBREAK(rc);
      if (sz != sizeof(id)) {
        rc = IWKV_ERROR_CORRUPTED;
        iwlog_ecode_error3(rc);
        break;
      }
      do {
        step = 1;
        rc = consumer(ctx, cur, id, &step, 0);
        RCBREAK(rc);
      } while (step < 0 && !++step);
    }
  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? ctx->cursor_step : cursor_reverse_step)));
  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;

finish:
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  return consumer(ctx, 0, 0, 0, rc);
}
