#include "ejdb2_internal.h"

iwrc jb_scan_consumer(struct _JBEXEC *ctx, IWKV_cursor cur, int64_t id, int64_t *step, bool *matched, iwrc err) {
  if (!id) { // EOF scan
    return err;
  }

  iwrc rc;
  struct _JBL jbl;
  size_t vsz = 0;
  EJDB_EXEC *ux = ctx->ux;
  IWPOOL *pool = ux->pool;
  IWKV_val key = {
    .data = &id,
    .size = sizeof(id)
  };

start: {
    if (cur) {
      rc = iwkv_cursor_copy_val(cur, ctx->jblbuf, ctx->jblbufsz, &vsz);
    } else {
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
      goto start;
    }
  }

  rc = jbl_from_buf_keep_onstack(&jbl, ctx->jblbuf, vsz);
  RCGO(rc, finish);

  rc = jql_matched(ux->q, &jbl, matched);
  if (rc || !*matched || (ux->skip && ux->skip-- > 0)) {
    goto finish;
  }
  if (ctx->istep > 0) {
    --ctx->istep;
  } else if (ctx->istep < 0) {
    ++ctx->istep;
  }

  if (!ctx->istep) {
    JQL q = ux->q;
    struct JQP_AUX *aux = q->qp->aux;
    struct _EJDB_DOC doc = {
      .id = id,
      .raw = &jbl
    };
    if (aux->apply || aux->projection) {
      if (!pool) {
        pool = iwpool_create(1024);
        if (!pool) {
          rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
          goto finish;
        }
      }
      rc = jql_apply(q, &jbl, &doc.node, pool);
      RCGO(rc, finish);
      if (aux->apply && doc.node) {
        binn bv;
        rc = _jbl_from_node(&bv, doc.node);
        RCGO(rc, finish);
        if (bv.writable && bv.dirty) {
          binn_save_header(&bv);
        }
        IWKV_val val = {
          .data = bv.ptr,
          .size = bv.size
        };
        if (cur) {
          rc = iwkv_cursor_set(cur, &val, 0);
        } else {
          rc = iwkv_put(ctx->jbc->cdb, &key, &val, 0);
        }
        binn_free(&bv);
        RCGO(rc, finish);
      }
    }
    ctx->istep = 1;
    rc = ux->visitor(ux, &doc, &ctx->istep);
    RCGO(rc, finish);
    *step  = ctx->istep > 0 ? 1 : ctx->istep < 0 ? -1 : 0;
    if (--ux->limit < 1) {
      *step = 0;
    }
  }

finish:
  if (pool && pool != ctx->ux->pool) {
    iwpool_destroy(pool);
  }
  return rc;
}
