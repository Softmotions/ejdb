#include "ejdb2_internal.h"

iwrc jb_scan_consumer(struct _JBEXEC *ctx, IWKV_cursor cur, uint64_t id, int64_t *step) {
  iwrc rc;
  bool matched;
  struct _JBL jbl;
  size_t vsz = 0;
  int64_t istep = 1;

  if (!id) { // EOF scan
    return 0;
  }

  *step = 1;
  do {
    if (cur) {
      rc = iwkv_cursor_copy_val(cur, ctx->jblbuf, ctx->jblbufsz, &vsz);
    } else {
      IWKV_val key = {
        .data = &id,
        .size = sizeof(id)
      };
      rc = iwkv_get_copy(ctx->jbc->cdb, &key, ctx->jblbuf, ctx->jblbufsz, &vsz);
    }
    if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
    RCGO(rc, finish);
    if (vsz > ctx->jblbufsz) {
      size_t nsize = MAX(vsz, ctx->jblbufsz * 2);
      void *nbuf = realloc(ctx->jblbuf, nsize);
      if (!nbuf) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto finish;
      }
      ctx->jblbuf = nbuf;
      ctx->jblbufsz = nsize;
      continue;
    }
  } while (0);

  rc = jbl_from_buf_keep_onstack(&jbl, ctx->jblbuf, vsz);
  RCGO(rc, finish);

  rc = jql_matched(ctx->ux->q, &jbl, &matched);
  RCGO(rc, finish);

  if (matched) {
    if (istep > 0) {
      --istep;
    } else if (istep < 0) {
      ++istep;
    }
    if (!istep) {
      struct _EJDOC doc = {
        .id = id,
        .raw = &jbl
      };
      rc = ctx->ux->visitor(ctx->ux, &doc, &istep);
      RCGO(rc, finish);
      *step  = istep > 0 ? 1 : istep < 0 ? -1 : 0;
    }
  }

finish:
  return rc;
}
