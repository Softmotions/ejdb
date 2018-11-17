#include "ejdb2_internal.h"

static void _jb_release_sorting(struct _JBEXEC *ctx) {
  struct _JBSSC *ssc = &ctx->ssc;
  if (ssc->refs) {
    free(ssc->refs);
  }
  if (ssc->docs) {
    free(ssc->docs);
  }
  if (ssc->sof_active) {
    ssc->sof.close(&ssc->sof);
  }
  memset(ssc, 0, sizeof(*ssc));
}

static iwrc _jb_finish_sorting(struct _JBEXEC *ctx) {
  // TODO: sort docs here
  _jb_release_sorting(ctx);
  return 0;
}

static iwrc _jb_init_soft(struct _JBSSC *ssc, off_t initial_size) {
  IWFS_EXT_OPTS opts = {
    .initial_size = initial_size,
    .rspolicy = iw_exfile_szpolicy_fibo,
    .file = {
      .path = "jb-",
      .omode = IWFS_OTMP | IWFS_OUNLINK
    }
  };
  iwrc rc = iwfs_exfile_open(&ssc->sof, &opts);
  RCRET(rc);
  rc = ssc->sof.add_mmap(&ssc->sof, 0, UINT64_MAX, 0);
  RCGO(rc, finish);

finish:
  if (rc) {
    ssc->sof.close(&ssc->sof);
  }
  return rc;
}

iwrc jb_scan_sorter_consumer(struct _JBEXEC *ctx, IWKV_cursor cur, uint64_t id, int64_t *step) {
  if (!id) { // End of scan
    return _jb_finish_sorting(ctx);
  }
  iwrc rc;
  size_t vsz = 0;
  bool matched;
  struct _JBL jbl;
  struct _JBSSC *ssc = &ctx->ssc;
  EJDB db = ctx->jbc->db;

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
  if (!matched) {
    goto finish;
  }

  if (!ssc->refs) {
    ssc->refs_asz = 64 * 1024; // 64K
    ssc->refs = malloc(db->opts.document_buffer_sz);
    if (!ssc->refs) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    ssc->docs_asz = 255 * 1024; // 255K
    ssc->docs = malloc(ssc->docs_asz);
    if (!ssc->docs) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
  } else if (ssc->refs_asz <= (ssc->refs_num + 1) * sizeof(ssc->refs[0])) {
    ssc->refs_asz *= 2;
    uint32_t *nrefs = realloc(ssc->refs, ssc->refs_asz);
    if (!nrefs) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    ssc->refs = nrefs;
  }

  do {
    if (!ssc->sof_active) {
      if (ssc->docs_npos + vsz > ssc->docs_asz)  {
        ssc->docs_asz = MIN(ssc->docs_asz * 2, db->opts.sort_buffer_sz);
        if (ssc->docs_npos + vsz > ssc->docs_asz) {
          size_t sz;
          rc = _jb_init_soft(ssc, (ssc->docs_npos + vsz) * 2);
          RCGO(rc, finish);
          rc = ssc->sof.write(&ssc->sof, 0, ssc->docs, ssc->docs_npos, &sz);
          RCGO(rc, finish);
          free(ssc->docs);
          ssc->docs = 0;
          ssc->sof_active = true;
          continue;
        } else {
          void *nbuf = realloc(ssc->docs, ssc->docs_asz);
          if (!nbuf) {
            rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
            goto finish;
          }
          ssc->docs = nbuf;
        }
      }
      memcpy((char *) ssc->docs + ssc->docs_npos, ctx->jblbuf, vsz);
    } else {
      size_t sz;
      rc = ssc->sof.write(&ssc->sof, ssc->docs_npos, ctx->jblbuf, vsz, &sz);
      RCGO(rc, finish);
    }
    ssc->refs[ssc->refs_num] = ssc->docs_npos;
    ssc->refs_num++;
    ssc->docs_npos += vsz;
  } while (0);

finish:
  if (rc) {
    _jb_release_sorting(ctx);
  }
  return rc;
}
