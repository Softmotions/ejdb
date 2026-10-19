#include "ejdb2_internal.h"
#include "sort_r.h"

static void _jbi_scan_sorter_release(struct jbexec *ctx) {
  struct jbssc *ssc = &ctx->ssc;
  free(ssc->refs);
  if (ssc->sof_active) {
    ssc->sof.close(&ssc->sof);
  } else {
    free(ssc->docs);
  }
  memset(ssc, 0, sizeof(*ssc));
}

static int _jbi_jbl_cmp_atomic_values(struct jbl *v1, struct jbl *v2) {
  jbl_type_t t1 = jbl_type(v1);
  jbl_type_t t2 = jbl_type(v2);
  if (t1 != t2) {
    return (int) t1 - (int) t2;
  }
  switch (t1) {
    case JBV_BOOL: {
      int b1 = binn_bool_get(&v1->bn) ? 1 : 0;
      int b2 = binn_bool_get(&v2->bn) ? 1 : 0;
      return b1 - b2;
    }
    case JBV_I64: {
      int64_t vv1 = jbl_get_i64(v1);
      int64_t vv2 = jbl_get_i64(v2);
      return vv1 > vv2 ? 1 : vv1 < vv2 ? -1 : 0;
    }
    case JBV_STR: {
      const char *s1 = jbl_get_str(v1);
      const char *s2 = jbl_get_str(v2);
      return strcmp(s1 ? s1 : "", s2 ? s2 : "");
    }
    case JBV_F64: {
      double vv1 = jbl_get_f64(v1);
      double vv2 = jbl_get_f64(v2);
      return vv1 > vv2 ? 1 : vv1 < vv2 ? -1 : 0;
    }
    default:
      return 0;
  }
}

static int _jbi_scan_sorter_cmp(const void *o1, const void *o2, void *op) {
  int rv = 0;
  iwrc rc;
  uint32_t r1, r2;
  struct jbl d1 = JBL_INITIALIZER, d2 = JBL_INITIALIZER;
  struct jbexec *ctx = op;
  struct jbssc *ssc = &ctx->ssc;
  struct jqp_aux *aux = ctx->ux->q->aux;
  uint8_t *p1, *p2;
  assert(aux->orderby_num > 0);

  memcpy(&r1, o1, sizeof(r1));
  memcpy(&r2, o2, sizeof(r2));

  p1 = ssc->docs + r1 + sizeof(uint64_t) /*id*/;
  p2 = ssc->docs + r2 + sizeof(uint64_t) /*id*/;

  // The documents are validated when they are collected, only the root header is parsed here.
  RCC(rc, finish, jb_jbl_wrap_buffer(&d1, p1, ssc->docs_npos - r1 - sizeof(uint64_t)));
  RCC(rc, finish, jb_jbl_wrap_buffer(&d2, p2, ssc->docs_npos - r2 - sizeof(uint64_t)));

  for (int i = 0; i < aux->orderby_num; ++i) {
    struct jbl v1 = JBL_INITIALIZER, v2 = JBL_INITIALIZER;
    struct jbl_ptr *ptr = aux->orderby_ptrs[i];
    int desc = (ptr->op & 1) ? -1 : 1; // If `-1` do desc sorting
    jb_jbl_at(&d1, ptr, &v1);
    jb_jbl_at(&d2, ptr, &v2);
    rv = _jbi_jbl_cmp_atomic_values(&v1, &v2) * desc;
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

static iwrc _jbi_scan_sorter_apply(struct iwpool *pool, struct jbexec *ctx, struct jql *q, struct ejdb_doc *doc) {
  struct jbl_node *root;
  struct jbl *jbl = doc->raw;
  struct jqp_aux *aux = q->aux;
  iwrc rc = jbl_to_node(jbl, &root, true, pool);
  RCRET(rc);
  doc->node = root;
  if (aux->qmode & JQP_QRY_APPLY_DEL) {
    rc = jb_del(ctx->jbc, jbl, doc->id);
    RCRET(rc);
  } else if (aux->apply || aux->apply_placeholder) {
    struct jbl sn = JBL_INITIALIZER;
    rc = jql_apply(q, root, pool);
    RCRET(rc);
    rc = jbl_fill_from_node(&sn, root);
    RCRET(rc);
    rc = jb_put(ctx->jbc, &sn, doc->id);
    binn_destroy(&sn.bn);
    RCRET(rc);
  }
  if (aux->projection) {
    rc = jql_project(q, root, ctx->ux->pool, ctx);
  }
  return rc;
}

static iwrc _jbi_scan_sorter_do(struct jbexec *ctx) {
  iwrc rc = 0;
  int64_t step = 1, id;
  struct jbl jbl = JBL_INITIALIZER;
  struct ejdb_exec *ux = ctx->ux;
  struct jbssc *ssc = &ctx->ssc;
  uint32_t rnum = ssc->refs_num;
  struct jqp_aux *aux = ux->q->aux;
  struct iwpool *pool = ux->pool;

  if (rnum) {
    if (setjmp(ssc->fatal_jmp)) { // Init error jump
      rc = ssc->rc;
      goto finish;
    }
    if (!ssc->docs) {
      size_t sp;
      RCC(rc, finish, ssc->sof.probe_mmap(&ssc->sof, 0, &ssc->docs, &sp));
    }

    sort_r(ssc->refs, rnum, sizeof(ssc->refs[0]), _jbi_scan_sorter_cmp, ctx);
  }

  for (int64_t i = ux->skip; step && i < rnum && i >= 0; ) {
    uint8_t *rp = ssc->docs + ssc->refs[i];
    memcpy(&id, rp, sizeof(id));
    rp += sizeof(id);
    RCC(rc, finish, jb_jbl_wrap_buffer(&jbl, rp, ssc->docs_npos - ssc->refs[i] - sizeof(id)));

    struct ejdb_doc doc = {
      .id = id,
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
      RCC(rc, finish, _jbi_scan_sorter_apply(pool, ctx, ux->q, &doc));
    } else if (aux->qmode & JQP_QRY_APPLY_DEL) {
      RCC(rc, finish, jb_del(ctx->jbc, &jbl, id));
    }

    if (!(aux->qmode & JQP_QRY_AGGREGATE)) {
      do {
        step = 1;
        RCC(rc, finish, ux->visitor(ux, &doc, &step));
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

static iwrc _jbi_scan_sorter_init(struct jbssc *ssc, off_t initial_size) {
  struct iwfs_ext_opts opts = {
    .initial_size = initial_size,
    .rspolicy = iw_exfile_szpolicy_fibo,
    .file = {
      .path = "jb-",
      .omode = IWFS_OTMP | IWFS_OUNLINK
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
  struct jbexec *ctx, struct iwkv_cursor *cur, int64_t id,
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
  struct jbl jbl = JBL_INITIALIZER;
  struct jbssc *ssc = &ctx->ssc;
  struct ejdb *db = ctx->jbc->db;
  struct iwfs_ext *sof = &ssc->sof;

start:
  {
    if (cur) {
      rc = iwkv_cursor_copy_val(cur, ctx->jblbuf + sizeof(id), ctx->jblbufsz - sizeof(id), &vsz);
    } else {
      struct iwkv_val key = {
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

  rc = jbl_init_from_buf(&jbl, ctx->jblbuf + sizeof(id), vsz, true);
  RCRET(rc);

  rc = jql_matched(ctx->ux->q, &jbl, matched);
  if (!*matched) {
    return 0;
  }

  if (!ssc->refs) {
    ssc->refs_asz = db->opts.document_buffer_sz;
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
          if (rc) {
            ssc->sof.close(&ssc->sof);
            return rc;
          }
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
