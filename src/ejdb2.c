#include "ejdb2_internal.h"

// ---------------------------------------------------------------------------

iwrc _jb_idx_ftoa(double val, char buf[static JBNUMBUF_SIZE], size_t *osz) {
  // TODO:
  int sz = snprintf(buf, JBNUMBUF_SIZE, "%.6f", val);
  if (sz >= JBNUMBUF_SIZE) return IW_ERROR_OVERFLOW;
  while (sz > 0 && buf[sz - 1] == '0') buf[sz--] = '\0';
  if (buf[sz] == '.') buf[sz--] = '\0';
  *osz = sz;
  return 0;
}

static void _jb_idx_release(JBIDX idx) {
  if (idx->idb) {
    iwkv_db_cache_release(idx->idb);
  }
  if (idx->auxdb) {
    iwkv_db_cache_release(idx->auxdb);
  }
  if (idx->ptr) {
    free(idx->ptr);
  }
  free(idx);
}

static void _jb_coll_release(JBCOLL jbc) {
  if (jbc->cdb) {
    iwkv_db_cache_release(jbc->cdb);
  }
  if (jbc->meta) {
    jbl_destroy(&jbc->meta);
  }
  JBIDX nidx;
  for (JBIDX idx = jbc->idx; idx; idx = nidx) {
    nidx = idx->next;
    _jb_idx_release(idx);
  }
  jbc->idx = 0;
  pthread_rwlock_destroy(&jbc->rwl);
  free(jbc);
}

static iwrc _jb_coll_load_index_lr(JBCOLL jbc, IWKV_val *mval) {
  binn *bn;
  char *ptr;
  jbl_type_t type;
  struct _JBL imeta;
  JBIDX idx = calloc(1, sizeof(*idx));
  if (!idx) return iwrc_set_errno(IW_ERROR_ALLOC, errno);

  iwrc rc = jbl_from_buf_keep_onstack(&imeta, mval->data, mval->size);
  RCRET(rc);
  bn = &imeta.bn;

  if (!binn_object_get_str(bn, "ptr", &ptr) ||
      !binn_object_get_uint32(bn, "mode", &idx->mode) ||
      !binn_object_get_uint32(bn, "idbf", &idx->idbf) ||
      !binn_object_get_uint32(bn, "dbid", &idx->dbid) ||
      !binn_object_get_uint32(bn, "auxdbid", &idx->auxdbid)) {
    rc = EJDB_ERROR_INVALID_COLLECTION_INDEX_META;
    goto finish;
  }
  rc = jbl_ptr_alloc(ptr, &idx->ptr);
  RCGO(rc, finish);

  rc = iwkv_db(jbc->db->iwkv, idx->dbid, idx->idbf, &idx->idb);
  RCGO(rc, finish);

  if (idx->auxdbid) {
    rc = iwkv_db(jbc->db->iwkv, idx->auxdbid, IWDB_UINT64_KEYS, &idx->auxdb);
    RCGO(rc, finish);
  }

  idx->next = jbc->idx;
  jbc->idx = idx;

finish:
  if (rc) {
    if (idx) {
      _jb_idx_release(idx);
    }
  }
  return rc;
}

static iwrc _jb_coll_load_indexes_lr(JBCOLL jbc) {
  iwrc rc = 0;
  IWKV_cursor cur;
  IWKV_val kval;
  char buf[sizeof(KEY_PREFIX_IDXMETA) + JBNUMBUF_SIZE];
  // Full key format: i.<coldbid>.<idxdbid>
  int sz = snprintf(buf, sizeof(buf), KEY_PREFIX_IDXMETA "%u.", jbc->dbid);
  if (sz >= sizeof(buf)) {
    return IW_ERROR_OVERFLOW;
  }
  kval.data = buf;
  kval.size = sz;
  rc = iwkv_cursor_open(jbc->db->metadb, &cur, IWKV_CURSOR_GE, &kval);
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
    goto finish;
  }
  RCRET(rc);

  do {
    IWKV_val key, val;
    rc = iwkv_cursor_key(cur, &key);
    RCGO(rc, finish);
    if (key.size > sz && !strncmp(buf, key.data, sz)) {
      iwkv_val_dispose(&key);
      rc = iwkv_cursor_val(cur, &val);
      RCGO(rc, finish);
      rc = _jb_coll_load_index_lr(jbc, &val);
      iwkv_val_dispose(&val);
      RCBREAK(rc);
    } else {
      iwkv_val_dispose(&key);
    }
  } while (!(rc = iwkv_cursor_to(cur, IWKV_CURSOR_PREV)));
  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;

finish:
  iwkv_cursor_close(&cur);
  return rc;
}

static iwrc _jb_coll_load_meta_lr(JBCOLL jbc) {
  JBL jbv;
  IWKV_cursor cur;
  JBL jbm = jbc->meta;
  iwrc rc = jbl_at(jbm, "/name", &jbv);
  RCRET(rc);
  jbc->name = jbl_get_str(jbv);
  jbl_destroy(&jbv);
  if (!jbc->name) {
    return EJDB_ERROR_INVALID_COLLECTION_META;
  }
  rc = jbl_at(jbm, "/id", &jbv);
  RCRET(rc);
  jbc->dbid = jbl_get_i64(jbv);
  jbl_destroy(&jbv);
  if (!jbc->dbid) {
    return EJDB_ERROR_INVALID_COLLECTION_META;
  }
  rc = iwkv_db(jbc->db->iwkv, jbc->dbid, IWDB_UINT64_KEYS, &jbc->cdb);
  RCRET(rc);

  rc = _jb_coll_load_indexes_lr(jbc);
  RCRET(rc);

  rc = iwkv_cursor_open(jbc->cdb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);
  rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT);
  if (rc) {
    if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
  } else {
    size_t sz;
    rc = iwkv_cursor_copy_key(cur, &jbc->id_seq, sizeof(jbc->id_seq), &sz);
    RCGO(rc, finish);
  }

finish:
  iwkv_cursor_close(&cur);
  return rc;
}

static iwrc _jb_coll_init(JBCOLL jbc, IWKV_val *meta) {
  int rci;
  iwrc rc = 0;
  pthread_rwlock_init(&jbc->rwl, 0);
  if (meta) {
    rc = jbl_from_buf_keep(&jbc->meta, meta->data, meta->size, false);
    RCRET(rc);
  }
  if (!jbc->meta) {
    return IW_ERROR_INVALID_STATE;
  }
  rc = _jb_coll_load_meta_lr(jbc);
  RCRET(rc);

  khiter_t k = kh_put(JBCOLLM, jbc->db->mcolls, jbc->name, &rci);
  if (rci != -1) {
    kh_value(jbc->db->mcolls, k) = jbc;
  } else {
    return IW_ERROR_FAIL;
  }
  return rc;
}

static iwrc _jb_idx_add_meta_lr(JBIDX idx, binn *list) {
  iwrc rc = 0;
  IWXSTR *xstr = iwxstr_new();
  if (!xstr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  binn *meta = binn_object();
  if (!meta) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    iwxstr_destroy(xstr);
    return rc;
  }
  rc = jbl_ptr_serialize(idx->ptr, xstr);
  RCGO(rc, finish);

  if (!binn_object_set_str(meta, "ptr", iwxstr_ptr(xstr)) ||
      !binn_object_set_uint32(meta, "mode", idx->mode) ||
      !binn_object_set_uint32(meta, "idbf", idx->idbf) ||
      !binn_object_set_uint32(meta, "dbid", idx->dbid) ||
      !binn_object_set_uint32(meta, "auxdbid", idx->auxdbid)) {
    rc = JBL_ERROR_CREATION;
  }
  if (!binn_list_add_object(list, meta)) {
    rc = JBL_ERROR_CREATION;
  }
finish:
  iwxstr_destroy(xstr);
  binn_free(meta);
  return rc;
}

static iwrc _jb_coll_add_meta_lr(JBCOLL jbc, binn *list) {
  iwrc rc = 0;
  binn *ilist = 0;
  binn *meta = binn_object();
  if (!meta) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return rc;
  }
  if (!binn_object_set_str(meta, "name", jbc->name)
      || !binn_object_set_uint32(meta, "dbid", jbc->dbid)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  ilist = binn_list();
  if (!ilist) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    rc = _jb_idx_add_meta_lr(idx, ilist);
    RCGO(rc, finish);
  }
  if (!binn_object_set_list(meta, "indexes", ilist)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  if (!binn_list_add_value(list, meta)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }

finish:
  if (meta) binn_free(meta);
  if (ilist) binn_free(ilist);
  return rc;
}

static iwrc _jb_db_meta_load(EJDB db) {
  iwrc rc = 0;
  if (!db->metadb) {
    rc = iwkv_db(db->iwkv, METADB_ID, 0, &db->metadb);
    RCRET(rc);
  }
  IWKV_cursor cur;
  rc = iwkv_cursor_open(db->metadb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);
  while (!(rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT))) {
    IWKV_val key, val;
    rc = iwkv_cursor_get(cur, &key, &val);
    RCGO(rc, finish);
    if (!strncmp(key.data, KEY_PREFIX_COLLMETA, strlen(KEY_PREFIX_COLLMETA))) {
      JBCOLL jbc = calloc(1, sizeof(*jbc));
      if (!jbc) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        iwkv_val_dispose(&val);
        goto finish;
      }
      jbc->db = db;
      rc = _jb_coll_init(jbc, &val);
      if (rc) {
        _jb_coll_release(jbc);
        iwkv_val_dispose(&val);
        goto finish;
      }
    } else {
      iwkv_val_dispose(&val);
    }
    iwkv_val_dispose(&key);
  }
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
  }

finish:
  iwkv_cursor_close(&cur);
  return rc;
}

static iwrc _jb_db_release(EJDB *dbp) {
  iwrc rc = 0;
  EJDB db = *dbp;
  if (db->mcolls) {
    for (khiter_t k = kh_begin(db->mcolls); k != kh_end(db->mcolls); ++k) {
      if (!kh_exist(db->mcolls, k)) continue;
      JBCOLL jbc = kh_val(db->mcolls, k);
      _jb_coll_release(jbc);
    }
    kh_destroy(JBCOLLM, db->mcolls);
    db->mcolls = 0;
  }
  if (db->iwkv) {
    rc = iwkv_close(&db->iwkv);
  }
  pthread_rwlock_destroy(&db->rwl);
  free(db);
  *dbp = 0;
  return rc;
}

static iwrc _jb_coll_acquire_keeplock(EJDB db, const char *coll, bool wl, JBCOLL *jbcp) {
  int rci;
  iwrc rc = 0;
  *jbcp = 0;
  API_RLOCK(db, rci);
  JBCOLL jbc;
  khiter_t k = kh_get(JBCOLLM, db->mcolls, coll);
  if (k != kh_end(db->mcolls)) {
    jbc = kh_value(db->mcolls, k);
    assert(jbc);
    rci = wl ? pthread_rwlock_wrlock(&jbc->rwl) : pthread_rwlock_rdlock(&jbc->rwl);
    if (rci) {
      rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
      goto finish;
    }
    *jbcp = jbc;
  } else {
    pthread_rwlock_unlock(&db->rwl); // relock
    if (db->oflags & IWKV_RDONLY) {
      return IW_ERROR_NOT_EXISTS;
    }
    API_WLOCK(db, rci);
    k = kh_get(JBCOLLM, db->mcolls, coll);
    if (k != kh_end(db->mcolls)) {
      jbc = kh_value(db->mcolls, k);
      assert(jbc);
      rci = pthread_rwlock_rdlock(&jbc->rwl);
      if (rci) {
        rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
        goto finish;
      }
      *jbcp = jbc;
    } else {
      JBL meta = 0;
      IWDB cdb = 0;
      uint32_t dbid = 0;
      char keybuf[JBNUMBUF_SIZE + sizeof(KEY_PREFIX_COLLMETA)];
      IWKV_val key, val;

      rc = iwkv_new_db(db->iwkv, IWDB_UINT64_KEYS, &dbid, &cdb);
      RCGO(rc, create_finish);
      JBCOLL jbc = calloc(1, sizeof(*jbc));
      if (!jbc) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto create_finish;
      }
      rc = jbl_create_empty_object(&meta);
      RCGO(rc, create_finish);
      if (!binn_object_set_str(&meta->bn, "name", coll)) {
        rc = JBL_ERROR_CREATION;
        goto create_finish;
      }
      if (!binn_object_set_uint32(&meta->bn, "id", dbid)) {
        rc = JBL_ERROR_CREATION;
        goto create_finish;
      }
      rc = jbl_as_buf(meta, &val.data, &val.size);
      RCGO(rc, create_finish);

      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u",  dbid);
      if (key.size >= sizeof(keybuf)) {
        rc = IW_ERROR_OVERFLOW;
        goto create_finish;
      }
      key.data = keybuf;
      rc = iwkv_put(db->metadb, &key, &val, IWKV_SYNC);
      RCGO(rc, create_finish);

      jbc->db = db;
      jbc->meta = meta;
      rc = _jb_coll_init(jbc, 0);
      if (rc) {
        iwkv_del(db->metadb, &key, IWKV_SYNC);
        goto create_finish;
      }
create_finish:
      if (rc) {
        if (meta) jbl_destroy(&meta);
        if (cdb) iwkv_db_destroy(&cdb);
        if (jbc) {
          jbc->meta = 0; // meta was cleared
          _jb_coll_release(jbc);
        }
      } else {
        rci = wl ? pthread_rwlock_wrlock(&jbc->rwl) : pthread_rwlock_rdlock(&jbc->rwl);
        if (rci) {
          rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
          goto finish;
        }
        *jbcp = jbc;
      }
    }
  }

finish:
  if (rc) {
    pthread_rwlock_unlock(&db->rwl);
  }
  return rc;
}

static iwrc _jb_fill_ikey(JBIDX idx, JBL jbv, IWKV_val *ikey, char numbuf[static JBNUMBUF_SIZE]) {
  iwrc rc = 0;
  uint64_t *llu = (void *) numbuf;
  jbl_type_t jbvt = jbl_type(jbv);
  ejdb_idx_mode_t itype = (idx->mode & ~(EJDB_IDX_UNIQUE));
  ikey->size = 0;
  ikey->data = 0;
  switch (itype) {
    case EJDB_IDX_STR:
      switch (jbvt) {
        case JBV_STR:
          ikey->data = jbl_get_str(jbv);
          ikey->size = jbl_size(jbv);
          break;
        case JBV_I64:
          ikey->size = iwitoa(jbl_get_i64(jbv), numbuf, sizeof(JBNUMBUF_SIZE));
          ikey->data = numbuf;
          break;
        case JBV_BOOL:
          if (jbl_get_i32(jbv)) {
            ikey->size = sizeof("true");
            ikey->data = "true";
          } else {
            ikey->size = sizeof("false");
            ikey->data = "false";
          }
          break;
        case JBV_F64:
          rc = _jb_idx_ftoa(jbl_get_f64(jbv), numbuf, &ikey->size);
          RCGO(rc, finish);
          ikey->data = numbuf;
          break;
        default:
          break;
      }
      break;
    case EJDB_IDX_I64:
      ikey->size = sizeof(*llu);
      ikey->data = llu;
      switch (jbvt) {
        case JBV_I64:
        case JBV_F64:
        case JBV_BOOL:
          *llu = jbl_get_i64(jbv);
          break;
        case JBV_STR:
          *llu = iwatoi(jbl_get_str(jbv));
          break;
        default:
          ikey->size = 0;
          ikey->data = 0;
          break;
      }
      break;
    case EJDB_IDX_F64:
      ikey->data = numbuf;
      switch (jbvt) {
        case JBV_F64:
        case JBV_I64:
        case JBV_BOOL:
          rc = _jb_idx_ftoa(jbl_get_f64(jbv), numbuf, &ikey->size);
          RCGO(rc, finish);
          break;
        case JBV_STR:
          rc = _jb_idx_ftoa(iwatof(jbl_get_str(jbv)), numbuf, &ikey->size);
          RCGO(rc, finish);
          break;
        default:
          ikey->size = 0;
          ikey->data = 0;
          break;
      }
      break;
    default:
      break;
  }

finish:
  if (rc) {
    ikey->size = 0;
    ikey->data = 0;
  }
  return rc;
}

static iwrc _jb_idx_record_remove(JBIDX idx, uint64_t id, JBL jbl) {
  JBL jbv;
  IWKV_val ikey;
  char numbuf[JBNUMBUF_SIZE];
  bool isarr = idx->mode & EJDB_IDX_ARR;
  IWKV_val idval = {
    .data = &id,
    .size = sizeof(id)
  };
  iwrc rc = jbl_at2(jbl, idx->ptr, &jbv);
  if (rc == JBL_ERROR_PATH_NOTFOUND) {
    return 0;
  }
  RCRET(rc);

  if (isarr) {
    // TODO: implement
    goto finish;
  }

  rc = _jb_fill_ikey(idx, jbv, &ikey, numbuf);
  RCGO(rc, finish);
  if (ikey.size) {
    if (idx->idbf & IWDB_DUP_FLAGS) {
      rc = iwkv_put(idx->idb, &ikey, &idval, IWKV_DUP_REMOVE | IWKV_DUP_REPORT_EMPTY);
      if (rc == IWKV_RC_DUP_ARRAY_EMPTY) {
        rc = iwkv_del(idx->idb, &ikey, 0);
      }
    } else {
      rc = iwkv_del(idx->idb, &ikey, 0);
    }
    if (rc == IWKV_ERROR_NOTFOUND) {
      rc = 0;
    }
  }

finish:
  if (jbv) {
    jbl_destroy(&jbv);
  }
  return rc;
}

static iwrc _jb_idx_record_add(JBIDX idx, uint64_t id, JBL jbl, JBL jblprev) {
  struct _JBL jbs;
  JBL jbv, jbvprev;
  IWKV_val ikey;
  uint64_t llu;
  char numbuf[JBNUMBUF_SIZE];
  bool isarr = idx->mode & EJDB_IDX_ARR;
  IWKV_val idval = {
    .data = &id,
    .size = sizeof(id)
  };
  iwrc rc = jbl_at2(jbl, idx->ptr, &jbv);
  if (rc == JBL_ERROR_PATH_NOTFOUND) {
    return 0;
  }
  RCRET(rc);

  if (jblprev) {
    rc = jbl_at2(jblprev, idx->ptr, &jbvprev);
    RCGO(rc, finish);
  } else {
    jbvprev = 0;
  }

  if (isarr) {
    // TODO: implement
    goto finish;
  }

  if (jbvprev && _jbl_is_eq_values(jbv, jbvprev)) {
    // Indexed value has not been changed
    goto finish;
  }
  rc = _jb_fill_ikey(idx, jbv, &ikey, numbuf);
  RCGO(rc, finish);
  if (ikey.size) {
    rc = iwkv_put(idx->idb, &ikey, &idval, IWKV_NO_OVERWRITE);
    if (rc == IWKV_ERROR_KEY_EXISTS) {
      rc = EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED;
    }
    RCGO(rc, finish);
  }
  if (jbvprev) {
    // Remove old index val
    rc = _jb_fill_ikey(idx, jbvprev, &ikey, numbuf);
    RCGO(rc, finish);
    if (ikey.size) {
      if (idx->idbf & IWDB_DUP_FLAGS) {
        rc = iwkv_put(idx->idb, &ikey, &idval, IWKV_DUP_REMOVE | IWKV_DUP_REPORT_EMPTY);
        if (rc == IWKV_RC_DUP_ARRAY_EMPTY) {
          rc = iwkv_del(idx->idb, &ikey, 0);
        }
      } else {
        rc = iwkv_del(idx->idb, &ikey, 0);
      }
      if (rc == IWKV_ERROR_NOTFOUND) {
        rc = 0;
      }
    }
  }

finish:
  if (jbv) {
    jbl_destroy(&jbv);
  }
  if (jbvprev) {
    jbl_destroy(&jbvprev);
  }
  return rc;
}

static iwrc _jb_idx_fill(JBIDX idx) {
  IWKV_cursor cur;
  IWKV_val key, val;
  struct _JBL jbs;
  uint64_t llu;
  JBL jbl = &jbs;

  iwrc rc = iwkv_cursor_open(idx->idb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);

  while (!rc) {
    rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT);
    if (rc == IWKV_ERROR_NOTFOUND) {
      rc = 0;
      break;
    }
    rc = iwkv_cursor_get(cur, &key, &val);
    if (rc) break;
    if (!binn_load(val.data, &jbs.bn)) {
      rc = JBL_ERROR_CREATION;
      break;
    }
    memcpy(&llu, key.data, sizeof(llu));
    rc = _jb_idx_record_add(idx, llu, jbl, 0);
    iwkv_kv_dispose(&key, &val);
  }

  IWRC(iwkv_cursor_close(&cur), rc);
  return rc;
}

static iwrc _jb_put_handler(const IWKV_val *key, const IWKV_val *val, IWKV_val *oldval, void *op) {
  iwrc rc = 0;
  struct _JBL jblprev;
  struct _JBPHCTX *ctx = op;
  JBCOLL jbc = ctx->jbc;
  if (oldval) {
    rc = jbl_from_buf_keep_onstack(&jblprev, oldval->data, oldval->size);
    RCRET(rc);
  }
  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    rc = _jb_idx_record_add(idx, ctx->id, ctx->jbl, &jblprev);
    RCGO(rc, finish);
  }
finish:
  if (oldval) {
    iwkv_val_dispose(oldval);
  }
  return rc;
}

static iwrc _jb_exec_index_select(JBEXEC *ctx) {
  return 0;
}

iwrc  _jb_index_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  // TODO:
  return 0;
}

iwrc  _jb_full_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  IWKV_cursor cur;
  int64_t step = 1;
  iwrc rc = iwkv_cursor_open(ctx->jbc->cdb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);

  while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? IWKV_CURSOR_NEXT : IWKV_CURSOR_PREV))) {
    if (step > 0) {
      --step;
    } else {
      ++step;
    }
    if (!step) {
      size_t sz;
      uint64_t id;
      rc = iwkv_cursor_copy_key(cur, &id, sizeof(id), &sz);
      RCBREAK(rc);
      if (sz != sizeof(id)) {
        rc = IW_ERROR_ASSERTION;
        iwlog_ecode_error3(rc);
        break;
      }
      rc = consumer(ctx, cur, id, &step);
      RCBREAK(rc);
    }
  }
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
  }
  iwkv_cursor_close(&cur);
  consumer(ctx, 0, 0, &step);
  return rc;
}

static iwrc _jb_exec_scan_init(JBEXEC *ctx) {
  // Allocate initial space for current document
  ctx->jblbuf = malloc(ctx->jbc->db->opts.document_buffer_sz);
  if (!ctx->jblbuf) return iwrc_set_errno(IW_ERROR_ALLOC, errno);

  ctx->ssc.fh = INVALID_HANDLE_VALUE;

  iwrc rc = _jb_exec_index_select(ctx);
  RCRET(rc);
  if (ctx->idx) {
    ctx->scanner = _jb_index_scanner;
  } else {
    ctx->scanner = _jb_full_scanner;
  }
  return rc;
}

static void _jb_exec_scan_release(JBEXEC *ctx) {
  // TODO:
  if (ctx->jblbuf) {
    free(ctx->jblbuf);
  }
}

//----------------------- Public API

iwrc ejdb_exec(EJDB_EXEC ux) {
  if (!ux || !ux->visitor || !ux->db || !ux->q) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  JBEXEC ctx = {
    .ux = ux
  };
  iwrc rc = _jb_coll_acquire_keeplock(ux->db, ux->q->coll, jql_has_apply(ux->q), &ctx.jbc);
  RCRET(rc);
  rc = _jb_exec_scan_init(&ctx);
  RCGO(rc, finish);
  assert(ctx.scanner);
  if (ctx.sorting) {
    rc = ctx.scanner(&ctx, jb_scan_sorter_consumer);
  } else {
    rc = ctx.scanner(&ctx, jb_scan_consumer);
  }

finish:
  _jb_exec_scan_release(&ctx);
  API_COLL_UNLOCK(ctx.jbc, rci, rc);
  return rc;
}

iwrc ejdb_remove_index(EJDB db, const char *coll, const char *path, ejdb_idx_mode_t mode) {
  if (!db || !coll || !path) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  JBCOLL jbc;
  IWKV_val key;
  JBL_PTR ptr = 0;
  char keybuf[sizeof(KEY_PREFIX_IDXMETA) + 1 + 2 * JBNUMBUF_SIZE]; // Full key format: i.<coldbid>.<idxdbid>
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);

  rc = jbl_ptr_alloc(path, &ptr);
  RCGO(rc, finish);

  for (JBIDX idx = jbc->idx, prev = 0; idx; idx = idx->next) {
    if ((idx->mode & ~EJDB_IDX_UNIQUE) == (mode & ~EJDB_IDX_UNIQUE) && !jbl_ptr_cmp(idx->ptr, ptr)) {
      key.data = keybuf;
      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_IDXMETA "%u" "." "%u", jbc->dbid, idx->dbid);
      if (key.size >= sizeof(keybuf)) {
        rc = IW_ERROR_OVERFLOW;
        goto finish;
      }
      rc = iwkv_del(db->metadb, &key, 0);
      RCGO(rc, finish);
      if (prev) {
        prev->next = idx->next;
      } else {
        jbc->idx = idx->next;
      }
      if (idx->idb) {
        iwkv_db_destroy(&idx->idb);
        idx->idb = 0;
      }
      if (idx->auxdb) {
        iwkv_db_destroy(&idx->auxdb);
        idx->auxdb = 0;
      }
      _jb_idx_release(idx);
      break;
    }
    prev = idx;
  }

finish:
  free(ptr);
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_ensure_index(EJDB db, const char *coll, const char *path, ejdb_idx_mode_t mode) {
  if (!db || !coll || !path) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci, sz;
  JBCOLL jbc;
  IWKV_val key, val;
  char keybuf[sizeof(KEY_PREFIX_IDXMETA) + 1 + 2 * JBNUMBUF_SIZE]; // Full key format: i.<coldbid>.<idxdbid>

  JBIDX idx = 0;
  JBL_PTR ptr = 0;
  binn *imeta = 0;
  bool unique = (mode & EJDB_IDX_UNIQUE);

  switch (mode & (EJDB_IDX_STR | EJDB_IDX_I64 | EJDB_IDX_F64)) {
    case EJDB_IDX_STR:
    case EJDB_IDX_I64:
    case EJDB_IDX_F64:
      break;
    default:
      return EJDB_ERROR_INVALID_INDEX_MODE;
  }
  if ((mode & EJDB_IDX_ARR) && unique) {
    return EJDB_ERROR_INVALID_INDEX_MODE; // Array indexes cannot be unique
  }

  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);
  rc = jbl_ptr_alloc(path, &ptr);
  RCGO(rc, finish);

  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    if ((idx->mode & ~EJDB_IDX_UNIQUE) == (mode & ~EJDB_IDX_UNIQUE) && !jbl_ptr_cmp(idx->ptr, ptr)) {
      if (idx->mode != mode) {
        rc = EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE;
      }
      goto finish;
    }
  }

  idx = calloc(1, sizeof(*idx));
  if (!idx) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  idx->mode = mode;
  idx->jbc = jbc;
  idx->ptr = ptr;
  ptr = 0;
  idx->idbf = (mode & EJDB_IDX_I64) ? IWDB_UINT64_KEYS : 0;
  if (mode & EJDB_IDX_F64) {
    idx->idbf |= IWDB_REALNUM_KEYS;
  }
  if (!(mode & EJDB_IDX_UNIQUE)) {
    idx->idbf |= IWDB_DUP_UINT64_VALS;
  }
  rc = iwkv_new_db(db->iwkv, idx->idbf, &idx->dbid, &idx->idb);
  RCGO(rc, finish);

  if (mode & EJDB_IDX_ARR) { // auxiliary database for array index
    rc = iwkv_new_db(db->iwkv, IWDB_UINT64_KEYS, &idx->auxdbid, &idx->auxdb);
    RCGO(rc, finish);
  }

  rc = _jb_idx_fill(idx);
  RCGO(rc, finish);

  // save index meta into metadb
  imeta = binn_object();
  if (!imeta) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  if (!binn_object_set_str(imeta, "ptr", path) ||
      !binn_object_set_uint32(imeta, "mode", idx->mode) ||
      !binn_object_set_uint32(imeta, "idbf", idx->idbf) ||
      !binn_object_set_uint32(imeta, "dbid", idx->dbid) ||
      !binn_object_set_uint32(imeta, "auxdbid", idx->auxdbid)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }

  key.data = keybuf;
  // Full key format: i.<coldbid>.<idxdbid>
  key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_IDXMETA "%u" "." "%u", jbc->dbid, idx->dbid);
  if (key.size >= sizeof(keybuf)) {
    rc = IW_ERROR_OVERFLOW;
    goto finish;
  }
  val.data = binn_ptr(imeta);
  val.size = binn_size(imeta);
  rc = iwkv_put(db->metadb, &key, &val, 0);
  RCGO(rc, finish);

  idx->next = jbc->idx;
  jbc->idx = idx;

finish:
  if (rc) {
    if (idx) {
      if (idx->auxdb) {
        iwkv_db_destroy(&idx->auxdb);
        idx->auxdb = 0;
      }
      if (idx->idb) {
        iwkv_db_destroy(&idx->idb);
        idx->idb = 0;
      }
      _jb_idx_release(idx);
    }
  }
  if (ptr) free(ptr);
  if (imeta) {
    binn_free(imeta);
  }
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_put(EJDB db, const char *coll, const JBL jbl, uint64_t id) {
  if (!jbl) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  JBCOLL jbc;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);

  IWKV_val val;
  IWKV_val key = {
    .data = &id,
    .size = sizeof(id)
  };
  struct _JBPHCTX pctx = {
    .id = id,
    .jbc = jbc,
    .jbl = jbl
  };

  rc = jbl_as_buf(jbl, &val.data, &val.size);
  RCGO(rc, finish);

  rc = iwkv_puth(jbc->cdb, &key, &val, 0, _jb_put_handler, &pctx);
  RCGO(rc, finish);

finish:
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_put_new(EJDB db, const char *coll, const JBL jbl, uint64_t *id) {
  if (!jbl) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  JBCOLL jbc;
  if (id) *id = 0;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);
  uint64_t oid = jbc->id_seq + 1;

  IWKV_val val;
  IWKV_val key = {
    .data = &oid,
    .size = sizeof(oid)
  };
  struct _JBPHCTX pctx = {
    .id = oid,
    .jbc = jbc,
    .jbl = jbl
  };

  rc = jbl_as_buf(jbl, &val.data, &val.size);
  RCGO(rc, finish);

  rc = iwkv_puth(jbc->cdb, &key, &val, 0, _jb_put_handler, &pctx);
  RCGO(rc, finish);

  jbc->id_seq = oid;
  if (id) {
    *id = oid;
  }

finish:
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_get(EJDB db, const char *coll, uint64_t id, JBL *jblp) {
  if (!id || !jblp) {
    return IW_ERROR_INVALID_ARGS;
  }
  *jblp = 0;
  int rci;
  JBCOLL jbc;
  JBL jbl = 0;
  IWKV_val val = {0};
  IWKV_val key = {.data = &id, .size = sizeof(id)};
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, false, &jbc);
  RCRET(rc);
  rc = iwkv_get(jbc->cdb, &key, &val);
  RCGO(rc, finish);
  rc = jbl_from_buf_keep(&jbl, val.data, val.size, false);
  RCGO(rc, finish);
  *jblp = jbl;

finish:
  if (rc) {
    if (jbl) {
      jbl_destroy(&jbl);
    } else {
      iwkv_val_dispose(&val);
    }
  }
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_remove(EJDB db, const char *coll, uint64_t id) {
  int rci;
  JBCOLL jbc;
  struct _JBL jbl;
  IWKV_val val = {0};
  IWKV_val key = {.data = &id, .size = sizeof(id)};
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);

  rc = iwkv_get(jbc->cdb, &key, &val);
  RCGO(rc, finish);

  rc = jbl_from_buf_keep_onstack(&jbl, val.data, val.size);
  RCGO(rc, finish);

  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    IWRC(_jb_idx_record_remove(idx, id, &jbl), rc);
  }
  RCGO(rc, finish);
  rc = iwkv_del(jbc->cdb, &key, 0);

finish:
  if (val.data) {
    iwkv_val_dispose(&val);
  }
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_ensure_collection(EJDB db, const char *coll) {
  int rci;
  JBCOLL jbc;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, false, &jbc);
  RCRET(rc);
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_remove_collection(EJDB db, const char *coll) {
  int rci;
  iwrc rc = 0;
  if (db->oflags & IWKV_RDONLY) {
    return IW_ERROR_READONLY;
  }
  API_WLOCK(db, rci);
  JBCOLL jbc;
  IWKV_val key;
  char keybuf[sizeof(KEY_PREFIX_IDXMETA) + 1 + 2 * JBNUMBUF_SIZE]; // Full key format: i.<coldbid>.<idxdbid>
  khiter_t k = kh_get(JBCOLLM, db->mcolls, coll);
  if (k != kh_end(db->mcolls)) {
    jbc = kh_value(db->mcolls, k);
    key.data = keybuf;
    key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u", jbc->dbid);
    rc = iwkv_del(jbc->db->metadb, &key, IWKV_SYNC);
    RCGO(rc, finish);
    for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
      key.data = keybuf;
      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_IDXMETA "%u" "." "%u", jbc->dbid, idx->dbid);
      rc = iwkv_del(jbc->db->metadb, &key, 0);
      RCGO(rc, finish);
    }
    for (JBIDX idx = jbc->idx, nidx; idx; idx = nidx) {
      IWRC(iwkv_db_destroy(&idx->idb), rc);
      idx->idb = 0;
      if (idx->auxdb) {
        IWRC(iwkv_db_destroy(&idx->auxdb), rc);
        idx->auxdb = 0;
      }
      nidx = idx->next;
      _jb_idx_release(idx);
    }
    jbc->idx = 0;
    IWRC(iwkv_db_destroy(&jbc->cdb), rc);
    kh_del(JBCOLLM, db->mcolls, k);
    _jb_coll_release(jbc);
  }

finish:
  API_UNLOCK(db, rci, rc);
  return rc;
}

iwrc ejdb_get_meta(EJDB db, JBL *jblp) {
  int rci;
  *jblp = 0;
  JBL jbl;
  iwrc rc = jbl_create_empty_object(&jbl);
  RCRET(rc);
  binn *clist = 0;
  API_RLOCK(db, rci);
  if (!binn_object_set_str(&jbl->bn, "version", ejdb_version_full())) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  IWFS_FSM_STATE sfsm;
  rc = iwkv_state(db->iwkv, &sfsm);
  RCRET(rc);
  if (!binn_object_set_str(&jbl->bn, "file", sfsm.exfile.file.opts.path)
      || !binn_object_set_int64(&jbl->bn, "size", sfsm.exfile.fsize)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  clist = binn_list();
  if (!clist) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  for (khiter_t k = kh_begin(db->mcolls); k != kh_end(db->mcolls); ++k) {
    if (!kh_exist(db->mcolls, k)) continue;
    JBCOLL jbc = kh_val(db->mcolls, k);
    rc = _jb_coll_add_meta_lr(jbc, clist);
    RCGO(rc, finish);
  }
  if (!binn_object_set_list(&jbl->bn, "collections", clist)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  binn_free(clist);
  clist = 0;

finish:
  API_UNLOCK(db, rci, rc);
  if (rc) {
    if (clist) binn_free(clist);
    jbl_destroy(&jbl);
  } else {
    *jblp = jbl;
  }
  return rc;
}

iwrc ejdb_open(const EJDB_OPTS *_opts, EJDB *ejdbp) {
  *ejdbp = 0;
  int rci;
  iwrc rc = ejdb_init();
  RCRET(rc);
  if (!_opts || !_opts->kv.path || !ejdbp) {
    return IW_ERROR_INVALID_ARGS;
  }

  EJDB db = calloc(1, sizeof(*db));
  if (!db) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }

  memcpy(&db->opts, _opts, sizeof(db->opts));
  if (!db->opts.sort_buffer_sz) {
    db->opts.sort_buffer_sz = 16 * 1024 * 1024; // 16Mb, Min 1Mb
  }
  if (db->opts.sort_buffer_sz < 1024 * 1024) {
    db->opts.sort_buffer_sz = 1024 * 1024;
  }
  if (!db->opts.document_buffer_sz) { // 255Kb, Min 64Kb
    db->opts.document_buffer_sz = 255 * 1024;
  }
  if (db->opts.document_buffer_sz < 64 * 1024) {
    db->opts.document_buffer_sz = 64 * 1024;
  }

  rci = pthread_rwlock_init(&db->rwl, 0);
  if (rci) {
    rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    free(db);
    return rc;
  }
  db->mcolls = kh_init(JBCOLLM);
  if (!db->mcolls) {
    rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    goto finish;
  }

  IWKV_OPTS kvopts;
  memcpy(&kvopts, &db->opts.kv, sizeof(db->opts.kv));
  kvopts.wal.enabled = !db->opts.no_wal;
  rc = iwkv_open(&kvopts, &db->iwkv);
  RCGO(rc, finish);

  db->oflags = kvopts.oflags;
  rc = _jb_db_meta_load(db);

finish:
  if (rc) {
    _jb_db_release(&db);
  } else {
    db->open = true;
    *ejdbp = db;
  }
  return rc;
}

iwrc ejdb_close(EJDB *ejdbp) {
  if (!ejdbp || !*ejdbp) {
    return IW_ERROR_INVALID_ARGS;
  }
  EJDB db = *ejdbp;
  if (!__sync_bool_compare_and_swap(&db->open, 1, 0)) {
    return IW_ERROR_INVALID_STATE;
  }
  iwrc rc = _jb_db_release(ejdbp);
  return rc;
}

const char *ejdb_version_full(void) {
  return EJDB2_VERSION;
}

unsigned int ejdb_version_major(void) {
  return EJDB2_VERSION_MAJOR;
}

unsigned int ejdb_version_minor(void) {
  return EJDB2_VERSION_MINOR;
}

unsigned int ejdb_version_patch(void) {
  return EJDB2_VERSION_PATCH;
}

static const char *_ejdb_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _EJDB_ERROR_START && ecode < _EJDB_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case EJDB_ERROR_INVALID_COLLECTION_META:
      return "Invalid collection metadata (EJDB_ERROR_INVALID_COLLECTION_META)";
    case EJDB_ERROR_INVALID_COLLECTION_INDEX_META:
      return "Invalid collection index metadata (EJDB_ERROR_INVALID_COLLECTION_INDEX_META)";
    case EJDB_ERROR_INVALID_INDEX_MODE:
      return "Invalid index mode (EJDB_ERROR_INVALID_INDEX_MODE)";
    case EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE:
      return "Index exists but mismatched uniqueness constraint (EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE)";
    case EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED:
      return "Unique index constraint violated (EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED)";
  }
  return 0;
}

iwrc ejdb_init() {
  static volatile int _jb_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jb_initialized, 0, 1)) {
    return 0;  // initialized already
  }
  iwrc rc = iw_init();
  RCRET(rc);
  rc = jbl_init();
  RCRET(rc);
  rc = jql_init();
  RCRET(rc);
  return iwlog_register_ecodefn(_ejdb_ecodefn);;
}
