#include "ejdb2_internal.h"
#include "sort_r.h"

static void _jbi_scan_sorter_release(struct _JBEXEC *ctx) {
  struct _JBSSC *ssc = &ctx->ssc;
  free(ssc->refs);
  if (ssc->sof_active) {
    ssc->sof.close(&ssc->sof);
  } else {
    free(ssc->docs);
  }
  memset(ssc, 0, sizeof(*ssc));
}

static int _jbi_scan_sorter_cmp(const void *o1, const void *o2, void *op) {
  int rv = 0;
  uint32_t r1, r2;
  struct _JBL d1, d2;
  struct _JBEXEC *ctx = op;
  struct _JBSSC *ssc = &ctx->ssc;
  struct JQP_AUX *aux = ctx->ux->q->aux;
  uint8_t *p1, *p2;
  assert(aux->orderby_num > 0);

  memcpy(&r1, o1, sizeof(r1));
  memcpy(&r2, o2, sizeof(r2));

  p1 = ssc->docs + r1 + sizeof(uint64_t) /*id*/;
  p2 = ssc->docs + r2 + sizeof(uint64_t) /*id*/;

  iwrc rc = jbl_from_buf_keep_onstack2(&d1, p1);
  RCGO(rc, finish);
  rc = jbl_from_buf_keep_onstack2(&d2, p2);
  RCGO(rc, finish);

  for (int i = 0; i < aux->orderby_num; ++i) {
    struct _JBL v1 = { 0 };
    struct _JBL v2 = { 0 };
    JBL_PTR ptr = aux->orderby_ptrs[i];
    int desc = (ptr->op & 1) ? -1 : 1; // If `-1` do desc sorting
    _jbl_at(&d1, ptr, &v1);
    _jbl_at(&d2, ptr, &v2);
    rv = _jbl_cmp_atomic_values(&v1, &v2) * desc;
    if (rv) {
      break;
    }
  }

finish:
  if (rc) {
    ssc->rc = rc;
    longjmp(ssc->fatal_jmp, 1);
  }
  return rv;
}

static iwrc _jbi_scan_sorter_apply(IWPOOL *pool, struct _JBEXEC *ctx, JQL q, struct _EJDB_DOC *doc) {
  JBL_NODE root;
  JBL jbl = doc->raw;
  struct JQP_AUX *aux = q->aux;
  iwrc rc = jbl_to_node(jbl, &root, true, pool);
  RCRET(rc);
  doc->node = root;
  if (aux->qmode & JQP_QRY_APPLY_DEL) {
    rc = jb_del(ctx->jbc, jbl, doc->id);
    RCRET(rc);
  } else if (aux->apply || aux->apply_placeholder) {
    struct _JBL sn = { 0 };
    rc = jql_apply(q, root, pool);
    RCRET(rc);
    rc = _jbl_from_node(&sn, root);
    RCRET(rc);
    rc = jb_put(ctx->jbc, &sn, doc->id);
    binn_free(&sn.bn);
    RCRET(rc);
  }
  if (aux->projection) {
    rc = jql_project(q, root, ctx->ux->pool, ctx);
  }
  return rc;
}

static iwrc _jbi_scan_sorter_do(struct _JBEXEC *ctx) {
  iwrc rc = 0;
  int64_t step = 1, id;
  struct _JBL jbl;
  EJDB_EXEC *ux = ctx->ux;
  struct _JBSSC *ssc = &ctx->ssc;
  uint32_t rnum = ssc->refs_num;
  struct JQP_AUX *aux = ux->q->aux;
  IWPOOL *pool = ux->pool;

  if (rnum) {
    if (setjmp(ssc->fatal_jmp)) { // Init error jump
      rc = ssc->rc;
      goto finish;
    }
    if (!ssc->docs) {
      size_t sp;
      rc = ssc->sof.probe_mmap(&ssc->sof, 0, &ssc->docs, &sp);
      RCGO(rc, finish);
    }

    sort_r(ssc->refs, rnum, sizeof(ssc->refs[0]), _jbi_scan_sorter_cmp, ctx);
  }

  for (int64_t i = ux->skip; step && i < rnum && i >= 0; ) {
    uint8_t *rp = ssc->docs + ssc->refs[i];
    memcpy(&id, rp, sizeof(id));
    rp += sizeof(id);
    rc = jbl_from_buf_keep_onstack2(&jbl, rp);
    RCGO(rc, finish);
    struct _EJDB_DOC doc = {
      .id  = id,
      .raw = &jbl
    };
    if (aux->apply || aux->projection) {
      if (!pool) {
        pool = iwpool_create(jbl.bn.size * 2);
        if (!pool) {
          rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
          goto finish;
        }
      }
      rc = _jbi_scan_sorter_apply(pool, ctx, ux->q, &doc);
      RCGO(rc, finish);
    } else if (aux->qmode & JQP_QRY_APPLY_DEL) {
      rc = jb_del(ctx->jbc, &jbl, id);
      RCGO(rc, finish);
    }
    if (!(aux->qmode & JQP_QRY_AGGREGATE)) {
      do {
        step = 1;
        rc = ux->visitor(ux, &doc, &step);
        RCGO(rc, finish);
      } while (step == -1);
    }
    ++ux->cnt;
    i += step;
    if (pool != ux->pool) {
      iwpool_destroy(pool);
      pool = 0;
    }
    if (--ux->limit < 1) {
      break;
    }
  }

finish:
  if (pool != ux->pool) {
    iwpool_destroy(pool);
  }
  _jbi_scan_sorter_release(ctx);
  return rc;
}

static iwrc _jbi_scan_sorter_init(struct _JBSSC *ssc, off_t initial_size) {
  IWFS_EXT_OPTS opts = {
    .initial_size = initial_size,
    .rspolicy     = iw_exfile_szpolicy_fibo,
    .file         = {
      .path       = "jb-",
      .omode      = IWFS_OTMP | IWFS_OUNLINK
    }
  };
  iwrc rc = iwfs_exfile_open(&ssc->sof, &opts);
  RCRET(rc);
  rc = ssc->sof.add_mmap(&ssc->sof, 0, SIZE_T_MAX, 0);
  if (rc) {
    ssc->sof.close(&ssc->sof);
  }
  return rc;
}

iwrc jbi_sorter_consumer(
  struct _JBEXEC *ctx, IWKV_cursor cur, int64_t id,
  int64_t *step, bool *matched, iwrc err) {
  if (!id) {
    // End of scan
    if (err) {
      // In the case of error do not perform sorting just release resources
      _jbi_scan_sorter_release(ctx);
      return err;
    } else {
      return _jbi_scan_sorter_do(ctx);
    }
  }

  iwrc rc;
  size_t vsz = 0;
  struct _JBL jbl;
  struct _JBSSC *ssc = &ctx->ssc;
  EJDB db = ctx->jbc->db;
  IWFS_EXT *sof = &ssc->sof;

start:
  {
    if (cur) {
      rc = iwkv_cursor_copy_val(cur, ctx->jblbuf + sizeof(id), ctx->jblbufsz - sizeof(id), &vsz);
    } else {
      IWKV_val key = {
        .data = &id,
        .size = sizeof(id)
      };
      rc = iwkv_get_copy(ctx->jbc->cdb, &key, ctx->jblbuf + sizeof(id), ctx->jblbufsz - sizeof(id), &vsz);
    }
    if (rc == IWKV_ERROR_NOTFOUND) {
      rc = 0;
    } else {
      RCRET(rc);
    }
    if (vsz + sizeof(id) > ctx->jblbufsz) {
      size_t nsize = MAX(vsz + sizeof(id), ctx->jblbufsz * 2);
      void *nbuf = realloc(ctx->jblbuf, nsize);
      if (!nbuf) {
        return iwrc_set_errno(IW_ERROR_ALLOC, errno);
      }
      ctx->jblbuf = nbuf;
      ctx->jblbufsz = nsize;
      goto start;
    }
  }

  rc = jbl_from_buf_keep_onstack(&jbl, ctx->jblbuf + sizeof(id), vsz);
  RCRET(rc);

  rc = jql_matched(ctx->ux->q, &jbl, matched);
  if (!*matched) {
    return 0;
  }

  if (!ssc->refs) {
    ssc->refs_asz = 64 * 1024; // 64K
    ssc->refs = malloc(db->opts.document_buffer_sz);
    if (!ssc->refs) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    ssc->docs_asz = 128 * 1024; // 128K
    ssc->docs = malloc(ssc->docs_asz);
    if (!ssc->docs) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
  } else if (ssc->refs_asz <= (ssc->refs_num + 1) * sizeof(ssc->refs[0])) {
    ssc->refs_asz *= 2;
    uint32_t *nrefs = realloc(ssc->refs, ssc->refs_asz);
    if (!nrefs) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    ssc->refs = nrefs;
  }

  vsz += sizeof(id);
  memcpy(ctx->jblbuf, &id, sizeof(id));

start2:
  {
    if (ssc->docs) {
      uint32_t rsize = ssc->docs_npos + vsz;
      if (rsize > ssc->docs_asz) {
        ssc->docs_asz = MIN(rsize * 2, db->opts.sort_buffer_sz);
        if (rsize > ssc->docs_asz) {
          size_t sz;
          rc = _jbi_scan_sorter_init(ssc, (ssc->docs_npos + vsz) * 2);
          RCRET(rc);
          rc = sof->write(sof, 0, ssc->docs, ssc->docs_npos, &sz);
          RCRET(rc);
          free(ssc->docs);
          ssc->docs = 0;
          ssc->sof_active = true;
          goto start2;
        } else {
          void *nbuf = realloc(ssc->docs, ssc->docs_asz);
          if (!nbuf) {
            return iwrc_set_errno(IW_ERROR_ALLOC, errno);
          }
          ssc->docs = nbuf;
        }
      }
      memcpy(ssc->docs + ssc->docs_npos, ctx->jblbuf, vsz);
    } else {
      size_t sz;
      rc = sof->write(sof, ssc->docs_npos, ctx->jblbuf, vsz, &sz);
      RCRET(rc);
    }
    ssc->refs[ssc->refs_num++] = ssc->docs_npos;
    ssc->docs_npos += vsz;
  }

  return rc;
}
