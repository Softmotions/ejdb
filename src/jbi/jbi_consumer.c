#include "ejdb2_internal.h"

iwrc jbi_consumer(struct _JBEXEC *ctx, IWKV_cursor cur, int64_t id, int64_t *step, bool *matched, iwrc err) {
  if (!id) { // EOF scan
    return err;
  }

  iwrc rc;
  struct _JBL jbl;
  size_t vsz = 0;
  EJDB_EXEC *ux = ctx->ux;
  IWPOOL *pool = ux->pool;

start:
  {
    if (cur) {
      rc = iwkv_cursor_copy_val(cur, ctx->jblbuf, ctx->jblbufsz, &vsz);
    } else {
      IWKV_val key = {
        .data = &id,
        .size = sizeof(id)
      };
      rc = iwkv_get_copy(ctx->jbc->cdb, &key, ctx->jblbuf, ctx->jblbufsz, &vsz);
    }
    if (rc == IWKV_ERROR_NOTFOUND) {
      rc = 0;
      if (ctx->midx.idx) {
        iwlog_error("Orphaned index entry."
                    "\n\tCollection db: %" PRIu32
                    "\n\tIndex db: %" PRIu32
                    "\n\tEntry id: %" PRId64, ctx->jbc->dbid, ctx->midx.idx->dbid, id);
      } else {
        iwlog_error("Orphaned index entry."
                    "\n\tCollection db: %" PRIu32
                    "\n\tEntry id: %" PRId64, ctx->jbc->dbid, id);
      }
      goto finish;
    }
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
  if (rc || !*matched || (ux->skip && (ux->skip-- > 0))) {
    goto finish;
  }
  if (ctx->istep > 0) {
    --ctx->istep;
  } else if (ctx->istep < 0) {
    ++ctx->istep;
  }
  if (!ctx->istep) {
    JQL q = ux->q;
    ctx->istep = 1;
    struct JQP_AUX *aux = q->aux;
    struct _EJDB_DOC doc = {
      .id  = id,
      .raw = &jbl
    };
    if (aux->apply || aux->apply_placeholder || aux->projection) {
      JBL_NODE root;
      if (!pool) {
        pool = iwpool_create(jbl.bn.size * 2);
        if (!pool) {
          rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
          goto finish;
        }
      }
      rc = jbl_to_node(&jbl, &root, true, pool);
      RCGO(rc, finish);
      doc.node = root;
      if (aux->qmode & JQP_QRY_APPLY_DEL) {
        if (cur) {
          rc = jb_cursor_del(ctx->jbc, cur, id, &jbl);
        } else {
          rc = jb_del(ctx->jbc, &jbl, id);
        }
      } else if (aux->apply || aux->apply_placeholder) {
        struct _JBL sn = { 0 };
        rc = jql_apply(q, root, pool);
        RCGO(rc, finish);
        rc = _jbl_from_node(&sn, root);
        RCGO(rc, finish);
        if (cur) {
          rc = jb_cursor_set(ctx->jbc, cur, id, &sn);
        } else {
          rc = jb_put(ctx->jbc, &sn, id);
        }
        binn_free(&sn.bn);
      }
      RCGO(rc, finish);
      if (aux->projection) {
        rc = jql_project(q, root, pool, ctx);
        RCGO(rc, finish);
      }
    } else if (aux->qmode & JQP_QRY_APPLY_DEL) {
      if (cur) {
        rc = jb_cursor_del(ctx->jbc, cur, id, &jbl);
      } else {
        rc = jb_del(ctx->jbc, &jbl, id);
      }
      RCGO(rc, finish);
    }
    if (!(aux->qmode & JQP_QRY_AGGREGATE)) {
      do {
        ctx->istep = 1;
        rc = ux->visitor(ux, &doc, &ctx->istep);
        RCGO(rc, finish);
      } while (ctx->istep == -1);
    }
    ++ux->cnt;
    *step = ctx->istep > 0 ? 1 : ctx->istep < 0 ? -1 : 0;
    if (--ux->limit < 1) {
      *step = 0;
    }
  } else {
    *step = ctx->istep > 0 ? 1 : ctx->istep < 0 ? -1 : 0; // -V547
  }

finish:
  if (pool && (pool != ctx->ux->pool)) {
    iwpool_destroy(pool);
  }
  return rc;
}
