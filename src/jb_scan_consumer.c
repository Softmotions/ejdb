#include "ejdb2_internal.h"

iwrc jb_scan_consumer(struct _JBEXEC *ctx, IWKV_cursor cur, uint64_t id, int64_t *step) {
  iwrc rc = 0;
  bool matched;
  struct _JBL jbl;
  IWKV_val kval, val;
  size_t sz;

  if (!cur) { // EOF scan
    return 0;
  }

  *step = 1;
  if (ctx->idx) {
    // TODO: Index used to scan
  }
  rc = iwkv_cursor_val(cur, &val);
  RCRET(rc);

  rc = jbl_from_buf_keep_onstack(&jbl, val.data, val.size);
  RCGO(rc, finish);

  rc = jql_matched(ctx->ux->q, &jbl, &matched);
  RCGO(rc, finish);

  if (matched) {
    struct _EJDOC doc = {
      .id = id,
      .raw = &jbl
    };
    rc = ctx->ux->visitor(ctx->ux, &doc, step);
  }

finish:
  iwkv_val_dispose(&val);
  return rc;
}
