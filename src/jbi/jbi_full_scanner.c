#include "ejdb2_internal.h"

iwrc jbi_full_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  bool matched;
  IWKV_cursor cur;
  int64_t step = 1;
  iwrc rc = iwkv_cursor_open(ctx->jbc->cdb, &cur, ctx->cursor_init, 0);
  RCRET(rc);

  IWKV_cursor_op cursor_reverse_step = (ctx->cursor_step == IWKV_CURSOR_NEXT)
                                       ? IWKV_CURSOR_PREV : IWKV_CURSOR_NEXT;

  while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? ctx->cursor_step : cursor_reverse_step))) {
    if (step > 0) {
      --step;
    } else if (step < 0) {
      ++step;                  // -V547
    }
    if (!step) {
      size_t sz;
      int64_t id;
      rc = iwkv_cursor_copy_key(cur, &id, sizeof(id), &sz, 0);
      RCBREAK(rc);
      if (sz != sizeof(id)) {
        rc = IWKV_ERROR_CORRUPTED;
        iwlog_ecode_error3(rc);
        break;
      }
      step = 1;
      matched = false;
      rc = consumer(ctx, cur, id, &step, &matched, 0);
      RCBREAK(rc);
    }
  }
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
  }
  iwkv_cursor_close(&cur);
  return consumer(ctx, 0, 0, 0, 0, rc);
}
