#include "ejdb2_internal.h"

// ---------------------------------------------------------------------------

static iwrc _jb_put_new_lw(JBCOLL jbc, JBL jbl, int64_t *id);

static const IWKV_val EMPTY_VAL = { 0 };

IW_INLINE iwrc _jb_meta_nrecs_removedb(EJDB db, uint32_t dbid) {
  dbid = IW_HTOIL(dbid);
  IWKV_val key = {
    .size = sizeof(dbid),
    .data = &dbid
  };
  return iwkv_del(db->nrecdb, &key, 0);
}

IW_INLINE iwrc _jb_meta_nrecs_update(EJDB db, uint32_t dbid, int64_t delta) {
  delta = IW_HTOILL(delta);
  dbid = IW_HTOIL(dbid);
  IWKV_val val = {
    .size = sizeof(delta),
    .data = &delta
  };
  IWKV_val key = {
    .size = sizeof(dbid),
    .data = &dbid
  };
  return iwkv_put(db->nrecdb, &key, &val, IWKV_VAL_INCREMENT);
}

static int64_t _jb_meta_nrecs_get(EJDB db, uint32_t dbid) {
  size_t vsz = 0;
  uint64_t ret = 0;
  dbid = IW_HTOIL(dbid);
  IWKV_val key = {
    .size = sizeof(dbid),
    .data = &dbid
  };
  iwkv_get_copy(db->nrecdb, &key, &ret, sizeof(ret), &vsz);
  if (vsz == sizeof(ret)) {
    ret = IW_ITOHLL(ret);
  }
  return (int64_t) ret;
}

static void _jb_idx_release(JBIDX idx) {
  if (idx->idb) {
    iwkv_db_cache_release(idx->idb);
  }
  free(idx->ptr);
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
  struct _JBL imeta;
  JBIDX idx = calloc(1, sizeof(*idx));
  if (!idx) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }

  iwrc rc = jbl_from_buf_keep_onstack(&imeta, mval->data, mval->size);
  RCGO(rc, finish);
  bn = &imeta.bn;

  if (  !binn_object_get_str(bn, "ptr", &ptr)
     || !binn_object_get_uint8(bn, "mode", &idx->mode)
     || !binn_object_get_uint8(bn, "idbf", &idx->idbf)
     || !binn_object_get_uint32(bn, "dbid", &idx->dbid)) {
    rc = EJDB_ERROR_INVALID_COLLECTION_INDEX_META;
    goto finish;
  }
  rc = jbl_ptr_alloc(ptr, &idx->ptr);
  RCGO(rc, finish);

  rc = iwkv_db(jbc->db->iwkv, idx->dbid, idx->idbf, &idx->idb);
  RCGO(rc, finish);
  idx->jbc = jbc;
  idx->rnum = _jb_meta_nrecs_get(jbc->db, idx->dbid);
  idx->next = jbc->idx;
  jbc->idx = idx;

finish:
  if (rc) {
    _jb_idx_release(idx);
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
    if ((key.size > sz) && !strncmp(buf, key.data, sz)) {
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
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
  }

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
  jbc->dbid = (uint32_t) jbl_get_i64(jbv);
  jbl_destroy(&jbv);
  if (!jbc->dbid) {
    return EJDB_ERROR_INVALID_COLLECTION_META;
  }
  rc = iwkv_db(jbc->db->iwkv, jbc->dbid, IWDB_VNUM64_KEYS, &jbc->cdb);
  RCRET(rc);

  jbc->rnum = _jb_meta_nrecs_get(jbc->db, jbc->dbid);

  rc = _jb_coll_load_indexes_lr(jbc);
  RCRET(rc);

  rc = iwkv_cursor_open(jbc->cdb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);
  rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT);
  if (rc) {
    if (rc == IWKV_ERROR_NOTFOUND) {
      rc = 0;
    }
  } else {
    size_t sz;
    rc = iwkv_cursor_copy_key(cur, &jbc->id_seq, sizeof(jbc->id_seq), &sz, 0);
    RCGO(rc, finish);
  }

finish:
  iwkv_cursor_close(&cur);
  return rc;
}

static iwrc _jb_coll_init(JBCOLL jbc, IWKV_val *meta) {
  int rci;
  iwrc rc = 0;

  pthread_rwlockattr_t attr;
  pthread_rwlockattr_init(&attr);
#if defined __linux__ && (defined __USE_UNIX98 || defined __USE_XOPEN2K)
  pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
  pthread_rwlock_init(&jbc->rwl, &attr);
  if (meta) {
    rc = jbl_from_buf_keep(&jbc->meta, meta->data, meta->size, false);
    RCRET(rc);
  }
  if (!jbc->meta) {
    iwlog_error("Collection %s seems to be initialized", jbc->name);
    return IW_ERROR_INVALID_STATE;
  }
  rc = _jb_coll_load_meta_lr(jbc);
  RCRET(rc);

  khiter_t k = kh_put(JBCOLLM, jbc->db->mcolls, jbc->name, &rci);
  if (rci != -1) {
    kh_value(jbc->db->mcolls, k) = jbc;
  } else {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
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

  if (  !binn_object_set_str(meta, "ptr", iwxstr_ptr(xstr))
     || !binn_object_set_uint32(meta, "mode", idx->mode)
     || !binn_object_set_uint32(meta, "idbf", idx->idbf)
     || !binn_object_set_uint32(meta, "dbid", idx->dbid)
     || !binn_object_set_int64(meta, "rnum", idx->rnum)) {
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
  if (  !binn_object_set_str(meta, "name", jbc->name)
     || !binn_object_set_uint32(meta, "dbid", jbc->dbid)
     || !binn_object_set_int64(meta, "rnum", jbc->rnum)) {
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
  binn_free(meta);
  if (ilist) {
    binn_free(ilist);
  }
  return rc;
}

static iwrc _jb_db_meta_load(EJDB db) {
  iwrc rc = 0;
  if (!db->metadb) {
    rc = iwkv_db(db->iwkv, METADB_ID, 0, &db->metadb);
    RCRET(rc);
  }
  if (!db->nrecdb) {
    rc = iwkv_db(db->iwkv, NUMRECSDB_ID, IWDB_VNUM64_KEYS, &db->nrecdb);
    RCRET(rc);
  }

  IWKV_cursor cur;
  rc = iwkv_cursor_open(db->metadb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);
  while (!(rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT))) {
    IWKV_val key, val;
    rc = iwkv_cursor_get(cur, &key, &val);
    RCGO(rc, finish);
    if (!strncmp(key.data, KEY_PREFIX_COLLMETA, sizeof(KEY_PREFIX_COLLMETA) - 1)) {
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
        iwkv_val_dispose(&key);
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
  *dbp = 0;
#ifdef JB_HTTP
  if (db->jbr) {
    IWRC(jbr_shutdown(&db->jbr), rc);
  }
#endif
  if (db->mcolls) {
    for (khiter_t k = kh_begin(db->mcolls); k != kh_end(db->mcolls); ++k) {
      if (!kh_exist(db->mcolls, k)) {
        continue;
      }
      JBCOLL jbc = kh_val(db->mcolls, k);
      _jb_coll_release(jbc);
    }
    kh_destroy(JBCOLLM, db->mcolls);
    db->mcolls = 0;
  }
  if (db->iwkv) {
    IWRC(iwkv_close(&db->iwkv), rc);
  }
  pthread_rwlock_destroy(&db->rwl);

  EJDB_HTTP *http = &db->opts.http;
  if (http->bind) {
    free((void*) http->bind);
  }
  if (http->access_token) {
    free((void*) http->access_token);
  }
  free(db);
  return rc;
}

static iwrc _jb_coll_acquire_keeplock2(EJDB db, const char *coll, jb_coll_acquire_t acm, JBCOLL *jbcp) {
  if (strlen(coll) > EJDB_COLLECTION_NAME_MAX_LEN) {
    return EJDB_ERROR_INVALID_COLLECTION_NAME;
  }
  int rci;
  iwrc rc = 0;
  *jbcp = 0;
  JBCOLL jbc = 0;
  bool wl = acm & JB_COLL_ACQUIRE_WRITE;
  API_RLOCK(db, rci);
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
    if ((db->oflags & IWKV_RDONLY) || (acm & JB_COLL_ACQUIRE_EXISTING)) {
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

      rc = iwkv_new_db(db->iwkv, IWDB_VNUM64_KEYS, &dbid, &cdb);
      RCGO(rc, create_finish);
      jbc = calloc(1, sizeof(*jbc));
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

      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u", dbid);
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
        if (meta) {
          jbl_destroy(&meta);
        }
        if (cdb) {
          iwkv_db_destroy(&cdb);
        }
        if (jbc) {
          jbc->meta = 0; // meta was cleared
          _jb_coll_release(jbc);
        }
      } else {
        rci = wl ? pthread_rwlock_wrlock(&jbc->rwl) : pthread_rwlock_rdlock(&jbc->rwl); // -V522
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

IW_INLINE iwrc _jb_coll_acquire_keeplock(EJDB db, const char *coll, bool wl, JBCOLL *jbcp) {
  return _jb_coll_acquire_keeplock2(db, coll, wl ? JB_COLL_ACQUIRE_WRITE : 0, jbcp);
}

static iwrc _jb_idx_record_add(JBIDX idx, int64_t id, JBL jbl, JBL jblprev) {
  IWKV_val key;
  uint8_t step;
  char vnbuf[IW_VNUMBUFSZ];
  char numbuf[JBNUMBUF_SIZE];

  bool jbv_found, jbvprev_found;
  struct _JBL jbv = { 0 }, jbvprev = { 0 };
  jbl_type_t jbv_type, jbvprev_type;

  iwrc rc = 0;
  IWPOOL *pool = 0;
  int64_t delta = 0; // delta of added/removed index records
  bool compound = idx->idbf & IWDB_COMPOUND_KEYS;

  jbvprev_found = jblprev ? _jbl_at(jblprev, idx->ptr, &jbvprev) : false;
  jbv_found = jbl ? _jbl_at(jbl, idx->ptr, &jbv) : false;

  jbv_type = jbl_type(&jbv);
  jbvprev_type = jbl_type(&jbvprev);

  // Do not index NULLs, OBJECTs, ARRAYs (in `EJDB_IDX_UNIQUE` mode)
  if (  ((jbvprev_type == JBV_OBJECT) || (jbvprev_type <= JBV_NULL))
     || ((jbvprev_type == JBV_ARRAY) && !compound)) {
    jbvprev_found = false;
  }
  if (  ((jbv_type == JBV_OBJECT) || (jbv_type <= JBV_NULL))
     || ((jbv_type == JBV_ARRAY) && !compound)) {
    jbv_found = false;
  }

  if (  compound
     && (jbv_type == jbvprev_type)
     && (jbvprev_type == JBV_ARRAY)) {  // compare next/prev obj arrays
    pool = iwpool_create(1024);
    if (!pool) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    JBL_NODE jbvprev_node, jbv_node;
    rc = jbl_to_node(&jbv, &jbv_node, false, pool);
    RCGO(rc, finish);
    jbv.node = jbv_node;

    rc = jbl_to_node(&jbvprev, &jbvprev_node, false, pool);
    RCGO(rc, finish);
    jbvprev.node = jbvprev_node;

    if (_jbl_compare_nodes(jbv_node, jbvprev_node, &rc) == 0) {
      goto finish; // Arrays are equal or error
    }
  } else if (_jbl_is_eq_atomic_values(&jbv, &jbvprev)) {
    return 0;
  }

  if (jbvprev_found) {               // Remove old index elements
    if (jbvprev_type == JBV_ARRAY) { // TODO: array modification delta?
      JBL_NODE n;
      if (!pool) {
        pool = iwpool_create(1024);
        if (!pool) {
          rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
          RCGO(rc, finish);
        }
      }
      rc = jbl_to_node(&jbvprev, &n, false, pool);
      RCGO(rc, finish);
      for (n = n->child; n; n = n->next) {
        jbi_node_fill_ikey(idx, n, &key, numbuf);
        if (key.size) {
          key.compound = id;
          rc = iwkv_del(idx->idb, &key, 0);
          if (!rc) {
            --delta;
          } else if (rc == IWKV_ERROR_NOTFOUND) {
            rc = 0;
          }
          RCGO(rc, finish);
        }
      }
    } else {
      jbi_jbl_fill_ikey(idx, &jbvprev, &key, numbuf);
      if (key.size) {
        key.compound = id;
        rc = iwkv_del(idx->idb, &key, 0);
        if (!rc) {
          --delta;
        } else if (rc == IWKV_ERROR_NOTFOUND) {
          rc = 0;
        }
        RCGO(rc, finish);
      }
    }
  }

  if (jbv_found) {               // Add index record
    if (jbv_type == JBV_ARRAY) { // TODO: array modification delta?
      JBL_NODE n;
      if (!pool) {
        pool = iwpool_create(1024);
        if (!pool) {
          rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
          RCGO(rc, finish);
        }
      }
      rc = jbl_to_node(&jbv, &n, false, pool);
      RCGO(rc, finish);
      for (n = n->child; n; n = n->next) {
        jbi_node_fill_ikey(idx, n, &key, numbuf);
        if (key.size) {
          key.compound = id;
          rc = iwkv_put(idx->idb, &key, &EMPTY_VAL, IWKV_NO_OVERWRITE);
          if (!rc) {
            ++delta;
          } else if (rc == IWKV_ERROR_KEY_EXISTS) {
            rc = 0;
          } else {
            goto finish;
          }
        }
      }
    } else {
      jbi_jbl_fill_ikey(idx, &jbv, &key, numbuf);
      if (key.size) {
        if (compound) {
          key.compound = id;
          rc = iwkv_put(idx->idb, &key, &EMPTY_VAL, IWKV_NO_OVERWRITE);
          if (!rc) {
            ++delta;
          } else if (rc == IWKV_ERROR_KEY_EXISTS) {
            rc = 0;
          }
        } else {
          IW_SETVNUMBUF64(step, vnbuf, id);
          IWKV_val idval = {
            .data = vnbuf,
            .size = step
          };
          rc = iwkv_put(idx->idb, &key, &idval, IWKV_NO_OVERWRITE);
          if (!rc) {
            ++delta;
          } else if (rc == IWKV_ERROR_KEY_EXISTS) {
            rc = EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED;
            goto finish;
          }
        }
      }
    }
  }

finish:
  if (pool) {
    iwpool_destroy(pool);
  }
  if (delta && !_jb_meta_nrecs_update(idx->jbc->db, idx->dbid, delta)) {
    idx->rnum += delta;
  }
  return rc;
}

IW_INLINE iwrc _jb_idx_record_remove(JBIDX idx, int64_t id, JBL jbl) {
  return _jb_idx_record_add(idx, id, 0, jbl);
}

static iwrc _jb_idx_fill(JBIDX idx) {
  IWKV_cursor cur;
  IWKV_val key, val;
  struct _JBL jbs;
  int64_t llv;
  JBL jbl = &jbs;

  iwrc rc = iwkv_cursor_open(idx->jbc->cdb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  while (!rc) {
    rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT);
    if (rc == IWKV_ERROR_NOTFOUND) {
      rc = 0;
      break;
    }
    rc = iwkv_cursor_get(cur, &key, &val);
    RCBREAK(rc);
    if (!binn_load(val.data, &jbs.bn)) {
      rc = JBL_ERROR_CREATION;
      break;
    }
    memcpy(&llv, key.data, sizeof(llv));
    rc = _jb_idx_record_add(idx, llv, jbl, 0);
    iwkv_kv_dispose(&key, &val);
  }
  IWRC(iwkv_cursor_close(&cur), rc);
  return rc;
}

// Used to avoid deadlocks within a `iwkv_put` context
static iwrc _jb_put_handler_after(iwrc rc, struct _JBPHCTX *ctx) {
  IWKV_val *oldval = &ctx->oldval;
  if (rc) {
    if (oldval->size) {
      iwkv_val_dispose(oldval);
    }
    return rc;
  }
  JBL prev;
  struct _JBL jblprev;
  JBCOLL jbc = ctx->jbc;
  if (oldval->size) {
    rc = jbl_from_buf_keep_onstack(&jblprev, oldval->data, oldval->size);
    RCRET(rc);
    prev = &jblprev;
  } else {
    prev = 0;
  }
  JBIDX fail_idx = 0;
  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    rc = _jb_idx_record_add(idx, ctx->id, ctx->jbl, prev);
    if (rc) {
      fail_idx = idx;
      goto finish;
    }
  }
  if (!prev) {
    _jb_meta_nrecs_update(jbc->db, jbc->dbid, 1);
    jbc->rnum += 1;
  }

finish:
  if (oldval->size) {
    iwkv_val_dispose(oldval);
  }
  if (rc && !oldval->size) {
    // Cleanup on error inserting new record
    IWKV_val key = { .data = &ctx->id, .size = sizeof(ctx->id) };
    for (JBIDX idx = jbc->idx; idx && idx != fail_idx; idx = idx->next) {
      IWRC(_jb_idx_record_remove(idx, ctx->id, ctx->jbl), rc);
    }
    IWRC(iwkv_del(jbc->cdb, &key, 0), rc);
  }
  return rc;
}

static iwrc _jb_put_handler(const IWKV_val *key, const IWKV_val *val, IWKV_val *oldval, void *op) {
  struct _JBPHCTX *ctx = op;
  if (oldval && oldval->size) {
    memcpy(&ctx->oldval, oldval, sizeof(*oldval));
  }
  return 0;
}

static iwrc _jb_exec_scan_init(JBEXEC *ctx) {
  ctx->istep = 1;
  ctx->jblbufsz = ctx->jbc->db->opts.document_buffer_sz;
  ctx->jblbuf = malloc(ctx->jblbufsz);
  if (!ctx->jblbuf) {
    ctx->jblbufsz = 0;
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  struct JQP_AUX *aux = ctx->ux->q->aux;
  if (aux->expr->flags & JQP_EXPR_NODE_FLAG_PK) { // Select by primary key
    ctx->scanner = jbi_pk_scanner;
    if (ctx->ux->log) {
      iwxstr_cat2(ctx->ux->log, "[INDEX] PK");
    }
    return 0;
  }
  iwrc rc = jbi_selection(ctx);
  RCRET(rc);
  if (ctx->midx.idx) {
    if (ctx->midx.idx->idbf & IWDB_COMPOUND_KEYS) {
      ctx->scanner = jbi_dup_scanner;
    } else {
      ctx->scanner = jbi_uniq_scanner;
    }
  } else {
    ctx->scanner = jbi_full_scanner;
    if (ctx->ux->log) {
      iwxstr_cat2(ctx->ux->log, "[INDEX] NO");
    }
  }
  return 0;
}

static void _jb_exec_scan_release(JBEXEC *ctx) {
  if (ctx->proj_joined_nodes_cache) {
    // Destroy projected nodes key
    iwstree_destroy(ctx->proj_joined_nodes_cache);
  }
  if (ctx->proj_joined_nodes_pool) {
    iwpool_destroy(ctx->proj_joined_nodes_pool);
  }
  free(ctx->jblbuf);
}

static iwrc _jb_noop_visitor(struct _EJDB_EXEC *ctx, EJDB_DOC doc, int64_t *step) {
  return 0;
}

IW_INLINE iwrc _jb_put_impl(JBCOLL jbc, JBL jbl, int64_t id) {
  IWKV_val val, key = {
    .data = &id,
    .size = sizeof(id)
  };
  struct _JBPHCTX pctx = {
    .id  = id,
    .jbc = jbc,
    .jbl = jbl
  };
  iwrc rc = jbl_as_buf(jbl, &val.data, &val.size);
  RCRET(rc);
  return _jb_put_handler_after(iwkv_puth(jbc->cdb, &key, &val, 0, _jb_put_handler, &pctx), &pctx);
}

iwrc jb_put(JBCOLL jbc, JBL jbl, int64_t id) {
  return _jb_put_impl(jbc, jbl, id);
}

iwrc jb_cursor_set(JBCOLL jbc, IWKV_cursor cur, int64_t id, JBL jbl) {
  IWKV_val val;
  struct _JBPHCTX pctx = {
    .id  = id,
    .jbc = jbc,
    .jbl = jbl
  };
  iwrc rc = jbl_as_buf(jbl, &val.data, &val.size);
  RCRET(rc);
  return _jb_put_handler_after(iwkv_cursor_seth(cur, &val, 0, _jb_put_handler, &pctx), &pctx);
}

static iwrc _jb_exec_upsert_lw(JBEXEC *ctx) {
  JBL_NODE n;
  int64_t id;
  iwrc rc = 0;
  JBL jbl = 0;
  EJDB_EXEC *ux = ctx->ux;
  JQL q = ux->q;
  if (q->aux->apply_placeholder) {
    JQVAL *pv = jql_find_placeholder(q, q->aux->apply_placeholder);
    if (!pv || (pv->type != JQVAL_JBLNODE) || !pv->vnode) {
      rc = JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE;
      goto finish;
    }
    n = pv->vnode;
  } else {
    n = q->aux->apply;
  }
  if (!n) {
    // Skip silently, nothing to do.
    goto finish;
  }
  RCC(rc, finish, jbl_from_node(&jbl, n));
  RCC(rc, finish, _jb_put_new_lw(ctx->jbc, jbl, &id));

  if (!(q->aux->qmode & JQP_QRY_AGGREGATE)) {
    struct _EJDB_DOC doc = {
      .id   = id,
      .raw  = jbl,
      .node = n
    };
    do {
      ctx->istep = 1;
      RCC(rc, finish, ux->visitor(ux, &doc, &ctx->istep));
    } while (ctx->istep == -1);
  }
  ++ux->cnt;

finish:
  jbl_destroy(&jbl);
  return rc;
}

//----------------------- Public API

iwrc ejdb_exec(EJDB_EXEC *ux) {
  if (!ux || !ux->db || !ux->q) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  iwrc rc = 0;
  if (!ux->visitor) {
    ux->visitor = _jb_noop_visitor;
    ux->q->aux->projection = 0; // Actually we don't need projection if exists
  }
  if (ux->log) {
    // set terminating NULL to current pos of log
    iwxstr_cat(ux->log, 0, 0);
  }
  JBEXEC ctx = {
    .ux = ux
  };
  if (ux->limit < 1) {
    rc = jql_get_limit(ux->q, &ux->limit);
    RCRET(rc);
    if (ux->limit < 1) {
      ux->limit = INT64_MAX;
    }
  }
  if (ux->skip < 1) {
    rc = jql_get_skip(ux->q, &ux->skip);
    RCRET(rc);
  }
  rc = _jb_coll_acquire_keeplock2(ux->db, ux->q->coll,
                                  jql_has_apply(ux->q) ? JB_COLL_ACQUIRE_WRITE : JB_COLL_ACQUIRE_EXISTING,
                                  &ctx.jbc);
  if (rc == IW_ERROR_NOT_EXISTS) {
    return 0;
  } else {
    RCRET(rc);
  }

  rc = _jb_exec_scan_init(&ctx);
  RCGO(rc, finish);
  if (ctx.sorting) {
    if (ux->log) {
      iwxstr_cat2(ux->log, " [COLLECTOR] SORTER\n");
    }
    rc = ctx.scanner(&ctx, jbi_sorter_consumer);
  } else {
    if (ux->log) {
      iwxstr_cat2(ux->log, " [COLLECTOR] PLAIN\n");
    }
    rc = ctx.scanner(&ctx, jbi_consumer);
  }
  RCGO(rc, finish);
  if ((ux->cnt == 0) && jql_has_apply_upsert(ux->q)) {
    // No records found trying to upsert new record
    rc = _jb_exec_upsert_lw(&ctx);
  }

finish:
  _jb_exec_scan_release(&ctx);
  API_COLL_UNLOCK(ctx.jbc, rci, rc);
  jql_reset(ux->q, true, false);
  return rc;
}

struct JB_LIST_VISITOR_CTX {
  EJDB_DOC head;
  EJDB_DOC tail;
};

static iwrc _jb_exec_list_visitor(struct _EJDB_EXEC *ctx, EJDB_DOC doc, int64_t *step) {
  struct JB_LIST_VISITOR_CTX *lvc = ctx->opaque;
  IWPOOL *pool = ctx->pool;
  struct _EJDB_DOC *ndoc = iwpool_alloc(sizeof(*ndoc) + sizeof(*doc->raw) + doc->raw->bn.size, pool);
  if (!ndoc) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  ndoc->id = doc->id;
  ndoc->raw = (void*) (((uint8_t*) ndoc) + sizeof(*ndoc));
  ndoc->raw->node = 0;
  ndoc->node = doc->node;
  ndoc->next = 0;
  ndoc->prev = 0;
  memcpy(&ndoc->raw->bn, &doc->raw->bn, sizeof(ndoc->raw->bn));
  ndoc->raw->bn.ptr = ((uint8_t*) ndoc) + sizeof(*ndoc) + sizeof(*doc->raw);
  memcpy(ndoc->raw->bn.ptr, doc->raw->bn.ptr, doc->raw->bn.size);

  if (!lvc->head) {
    lvc->head = ndoc;
    lvc->tail = ndoc;
  } else {
    lvc->tail->next = ndoc;
    ndoc->prev = lvc->tail;
    lvc->tail = ndoc;
  }
  return 0;
}

static iwrc _jb_list(EJDB db, JQL q, EJDB_DOC *first, int64_t limit, IWXSTR *log, IWPOOL *pool) {
  if (!db || !q || !first || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  struct JB_LIST_VISITOR_CTX lvc = { 0 };
  struct _EJDB_EXEC ux = {
    .db      = db,
    .q       = q,
    .visitor = _jb_exec_list_visitor,
    .pool    = pool,
    .limit   = limit,
    .log     = log,
    .opaque  = &lvc
  };
  rc = ejdb_exec(&ux);
  if (rc) {
    *first = 0;
  } else {
    *first = lvc.head;
  }
  return rc;
}

static iwrc _jb_count(EJDB db, JQL q, int64_t *count, int64_t limit, IWXSTR *log) {
  if (!db || !q || !count) {
    return IW_ERROR_INVALID_ARGS;
  }
  struct _EJDB_EXEC ux = {
    .db    = db,
    .q     = q,
    .limit = limit,
    .log   = log
  };
  iwrc rc = ejdb_exec(&ux);
  *count = ux.cnt;
  return rc;
}

iwrc ejdb_count(EJDB db, JQL q, int64_t *count, int64_t limit) {
  return _jb_count(db, q, count, limit, 0);
}

iwrc ejdb_count2(EJDB db, const char *coll, const char *q, int64_t *count, int64_t limit) {
  JQL jql;
  iwrc rc = jql_create(&jql, coll, q);
  RCRET(rc);
  rc = _jb_count(db, jql, count, limit, 0);
  jql_destroy(&jql);
  return rc;
}

iwrc ejdb_update(EJDB db, JQL q) {
  int64_t count;
  return ejdb_count(db, q, &count, 0);
}

iwrc ejdb_update2(EJDB db, const char *coll, const char *q) {
  int64_t count;
  return ejdb_count2(db, coll, q, &count, 0);
}

iwrc ejdb_list(EJDB db, JQL q, EJDB_DOC *first, int64_t limit, IWPOOL *pool) {
  return _jb_list(db, q, first, limit, 0, pool);
}

iwrc ejdb_list3(EJDB db, const char *coll, const char *query, int64_t limit, IWXSTR *log, EJDB_LIST *listp) {
  if (!listp) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  *listp = 0;
  IWPOOL *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  EJDB_LIST list = iwpool_alloc(sizeof(*list), pool);
  if (!list) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  list->first = 0;
  list->db = db;
  list->pool = pool;
  rc = jql_create(&list->q, coll, query);
  RCGO(rc, finish);
  rc = _jb_list(db, list->q, &list->first, limit, log, list->pool);

finish:
  if (rc) {
    iwpool_destroy(pool);
  } else {
    *listp = list;
  }
  return rc;
}

iwrc ejdb_list4(EJDB db, JQL q, int64_t limit, IWXSTR *log, EJDB_LIST *listp) {
  if (!listp) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  *listp = 0;
  IWPOOL *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  EJDB_LIST list = iwpool_alloc(sizeof(*list), pool);
  if (!list) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  list->q = 0;
  list->first = 0;
  list->db = db;
  list->pool = pool;
  rc = _jb_list(db, q, &list->first, limit, log, list->pool);

finish:
  if (rc) {
    iwpool_destroy(pool);
  } else {
    *listp = list;
  }
  return rc;
}

iwrc ejdb_list2(EJDB db, const char *coll, const char *query, int64_t limit, EJDB_LIST *listp) {
  return ejdb_list3(db, coll, query, limit, 0, listp);
}

void ejdb_list_destroy(EJDB_LIST *listp) {
  if (listp) {
    EJDB_LIST list = *listp;
    if (list) {
      if (list->q) {
        jql_destroy(&list->q);
      }
      if (list->pool) {
        iwpool_destroy(list->pool);
      }
    }
    *listp = 0;
  }
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

  iwrc rc = _jb_coll_acquire_keeplock2(db, coll, JB_COLL_ACQUIRE_WRITE | JB_COLL_ACQUIRE_EXISTING, &jbc);
  RCRET(rc);

  rc = jbl_ptr_alloc(path, &ptr);
  RCGO(rc, finish);

  for (JBIDX idx = jbc->idx, prev = 0; idx; idx = idx->next) {
    if (((idx->mode & ~EJDB_IDX_UNIQUE) == (mode & ~EJDB_IDX_UNIQUE)) && !jbl_ptr_cmp(idx->ptr, ptr)) {
      key.data = keybuf;
      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_IDXMETA "%u" "." "%u", jbc->dbid, idx->dbid);
      if (key.size >= sizeof(keybuf)) {
        rc = IW_ERROR_OVERFLOW;
        goto finish;
      }
      rc = iwkv_del(db->metadb, &key, 0);
      RCGO(rc, finish);
      _jb_meta_nrecs_removedb(db, idx->dbid);
      if (prev) {
        prev->next = idx->next;
      } else {
        jbc->idx = idx->next;
      }
      if (idx->idb) {
        iwkv_db_destroy(&idx->idb);
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
  int rci;
  JBCOLL jbc;
  IWKV_val key, val;
  char keybuf[sizeof(KEY_PREFIX_IDXMETA) + 1 + 2 * JBNUMBUF_SIZE]; // Full key format: i.<coldbid>.<idxdbid>

  JBIDX idx = 0;
  JBL_PTR ptr = 0;
  binn *imeta = 0;

  switch (mode & (EJDB_IDX_STR | EJDB_IDX_I64 | EJDB_IDX_F64)) {
    case EJDB_IDX_STR:
    case EJDB_IDX_I64:
    case EJDB_IDX_F64:
      break;
    default:
      return EJDB_ERROR_INVALID_INDEX_MODE;
  }

  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);
  rc = jbl_ptr_alloc(path, &ptr);
  RCGO(rc, finish);

  for (idx = jbc->idx; idx; idx = idx->next) {
    if (((idx->mode & ~EJDB_IDX_UNIQUE) == (mode & ~EJDB_IDX_UNIQUE)) && !jbl_ptr_cmp(idx->ptr, ptr)) {
      if (idx->mode != mode) {
        rc = EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE;
        idx = 0;
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
  idx->idbf = 0;
  if (mode & EJDB_IDX_I64) {
    idx->idbf |= IWDB_VNUM64_KEYS;
  } else if (mode & EJDB_IDX_F64) {
    idx->idbf |= IWDB_REALNUM_KEYS;
  }
  if (!(mode & EJDB_IDX_UNIQUE)) {
    idx->idbf |= IWDB_COMPOUND_KEYS;
  }
  rc = iwkv_new_db(db->iwkv, idx->idbf, &idx->dbid, &idx->idb);
  RCGO(rc, finish);

  rc = _jb_idx_fill(idx);
  RCGO(rc, finish);

  // save index meta into metadb
  imeta = binn_object();
  if (!imeta) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  if (  !binn_object_set_str(imeta, "ptr", path)
     || !binn_object_set_uint32(imeta, "mode", idx->mode)
     || !binn_object_set_uint32(imeta, "idbf", idx->idbf)
     || !binn_object_set_uint32(imeta, "dbid", idx->dbid)) {
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
      if (idx->idb) {
        iwkv_db_destroy(&idx->idb);
        idx->idb = 0;
      }
      _jb_idx_release(idx);
    }
  }
  free(ptr);
  binn_free(imeta);
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

static iwrc _jb_patch(
  EJDB db, const char *coll, int64_t id, bool upsert,
  const char *patchjson, JBL_NODE patchjbn, JBL patchjbl) {

  int rci;
  JBCOLL jbc;
  struct _JBL sjbl;
  JBL_NODE root, patch;
  JBL ujbl = 0;
  IWPOOL *pool = 0;
  IWKV_val val = { 0 };
  IWKV_val key = {
    .data = &id,
    .size = sizeof(id)
  };

  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCGO(rc, finish);

  rc = iwkv_get(jbc->cdb, &key, &val);
  if (upsert && (rc == IWKV_ERROR_NOTFOUND)) {
    if (patchjson) {
      rc = jbl_from_json(&ujbl, patchjson);
    } else if (patchjbl) {
      ujbl = patchjbl;
    } else if (patchjbn) {
      rc = jbl_from_node(&ujbl, patchjbn);
    } else {
      rc = IW_ERROR_INVALID_ARGS;
    }
    RCGO(rc, finish);
    if (jbl_type(ujbl) != JBV_OBJECT) {
      rc = EJDB_ERROR_PATCH_JSON_NOT_OBJECT;
      goto finish;
    }
    rc = _jb_put_impl(jbc, ujbl, id);
    if (!rc && (jbc->id_seq < id)) {
      jbc->id_seq = id;
    }
    goto finish;
  } else {
    RCGO(rc, finish);
  }

  rc = jbl_from_buf_keep_onstack(&sjbl, val.data, val.size);
  RCGO(rc, finish);

  pool = iwpool_create_empty();
  if (!pool) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  rc = jbl_to_node(&sjbl, &root, false, pool);
  RCGO(rc, finish);

  if (patchjson) {
    rc = jbn_from_json(patchjson, &patch, pool);
  } else if (patchjbl) {
    rc = jbl_to_node(patchjbl, &patch, false, pool);
  } else if (patchjbn) {
    patch = patchjbn;
  } else {
    rc = IW_ERROR_INVALID_ARGS;
  }
  RCGO(rc, finish);

  rc = jbn_patch_auto(root, patch, pool);
  RCGO(rc, finish);

  if (root->type == JBV_OBJECT) {
    rc = jbl_create_empty_object(&ujbl);
    RCGO(rc, finish);
  } else if (root->type == JBV_ARRAY) {
    rc = jbl_create_empty_array(&ujbl);
    RCGO(rc, finish);
  } else {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  rc = jbl_fill_from_node(ujbl, root);
  RCGO(rc, finish);

  rc = _jb_put_impl(jbc, ujbl, id);

finish:
  API_COLL_UNLOCK(jbc, rci, rc);
  if (ujbl != patchjbl) {
    jbl_destroy(&ujbl);
  }
  if (val.data) {
    iwkv_val_dispose(&val);
  }
  iwpool_destroy(pool);
  return rc;
}

static iwrc _jb_wal_lock_interceptor(bool before, void *op) {
  int rci;
  iwrc rc = 0;
  EJDB db = op;
  assert(db);
  if (before) {
    API_WLOCK2(db, rci);
  } else {
    API_UNLOCK(db, rci, rc);
  }
  return rc;
}

iwrc ejdb_patch(EJDB db, const char *coll, const char *patchjson, int64_t id) {
  return _jb_patch(db, coll, id, false, patchjson, 0, 0);
}

iwrc ejdb_patch_jbn(EJDB db, const char *coll, JBL_NODE patch, int64_t id) {
  return _jb_patch(db, coll, id, false, 0, patch, 0);
}

iwrc ejdb_patch_jbl(EJDB db, const char *coll, JBL patch, int64_t id) {
  return _jb_patch(db, coll, id, false, 0, 0, patch);
}

iwrc ejdb_merge_or_put(EJDB db, const char *coll, const char *patchjson, int64_t id) {
  return _jb_patch(db, coll, id, true, patchjson, 0, 0);
}

iwrc ejdb_merge_or_put_jbn(EJDB db, const char *coll, JBL_NODE patch, int64_t id) {
  return _jb_patch(db, coll, id, true, 0, patch, 0);
}

iwrc ejdb_merge_or_put_jbl(EJDB db, const char *coll, JBL patch, int64_t id) {
  return _jb_patch(db, coll, id, true, 0, 0, patch);
}

iwrc ejdb_put(EJDB db, const char *coll, JBL jbl, int64_t id) {
  if (!jbl) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  JBCOLL jbc;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);
  rc = _jb_put_impl(jbc, jbl, id);
  if (!rc && (jbc->id_seq < id)) {
    jbc->id_seq = id;
  }
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

static iwrc _jb_put_new_lw(JBCOLL jbc, JBL jbl, int64_t *id) {
  iwrc rc = 0;
  int64_t oid = jbc->id_seq + 1;
  IWKV_val val, key = {
    .data = &oid,
    .size = sizeof(oid)
  };
  struct _JBPHCTX pctx = {
    .id  = oid,
    .jbc = jbc,
    .jbl = jbl
  };

  RCC(rc, finish, jbl_as_buf(jbl, &val.data, &val.size));
  RCC(rc, finish, _jb_put_handler_after(iwkv_puth(jbc->cdb, &key, &val, 0, _jb_put_handler, &pctx), &pctx));

  jbc->id_seq = oid;
  if (id) {
    *id = oid;
  }

finish:
  return rc;
}

iwrc ejdb_put_new(EJDB db, const char *coll, JBL jbl, int64_t *id) {
  if (!jbl) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  JBCOLL jbc;
  if (id) {
    *id = 0;
  }
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);

  rc = _jb_put_new_lw(jbc, jbl, id);

  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_put_new_jbn(EJDB db, const char *coll, JBL_NODE jbn, int64_t *id) {
  JBL jbl = 0;
  iwrc rc = jbl_from_node(&jbl, jbn);
  RCRET(rc);
  rc = ejdb_put_new(db, coll, jbl, id);
  jbl_destroy(&jbl);
  return rc;
}

iwrc jb_get(EJDB db, const char *coll, int64_t id, jb_coll_acquire_t acm, JBL *jblp) {
  if (!id || !jblp) {
    return IW_ERROR_INVALID_ARGS;
  }
  *jblp = 0;
  int rci;
  JBCOLL jbc;
  JBL jbl = 0;
  IWKV_val val = { 0 };
  IWKV_val key = { .data = &id, .size = sizeof(id) };
  iwrc rc = _jb_coll_acquire_keeplock2(db, coll, acm, &jbc);
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

iwrc ejdb_get(EJDB db, const char *coll, int64_t id, JBL *jblp) {
  return jb_get(db, coll, id, JB_COLL_ACQUIRE_EXISTING, jblp);
}

iwrc ejdb_del(EJDB db, const char *coll, int64_t id) {
  int rci;
  JBCOLL jbc;
  struct _JBL jbl;
  IWKV_val val = { 0 };
  IWKV_val key = { .data = &id, .size = sizeof(id) };
  iwrc rc = _jb_coll_acquire_keeplock2(db, coll, JB_COLL_ACQUIRE_WRITE | JB_COLL_ACQUIRE_EXISTING, &jbc);
  RCRET(rc);

  rc = iwkv_get(jbc->cdb, &key, &val);
  RCGO(rc, finish);

  rc = jbl_from_buf_keep_onstack(&jbl, val.data, val.size);
  RCGO(rc, finish);

  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    IWRC(_jb_idx_record_remove(idx, id, &jbl), rc);
  }
  rc = iwkv_del(jbc->cdb, &key, 0);
  RCGO(rc, finish);
  _jb_meta_nrecs_update(jbc->db, jbc->dbid, -1);
  jbc->rnum -= 1;

finish:
  if (val.data) {
    iwkv_val_dispose(&val);
  }
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc jb_del(JBCOLL jbc, JBL jbl, int64_t id) {
  iwrc rc = 0;
  IWKV_val key = { .data = &id, .size = sizeof(id) };
  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    IWRC(_jb_idx_record_remove(idx, id, jbl), rc);
  }
  rc = iwkv_del(jbc->cdb, &key, 0);
  RCRET(rc);
  _jb_meta_nrecs_update(jbc->db, jbc->dbid, -1);
  jbc->rnum -= 1;
  return rc;
}

iwrc jb_cursor_del(JBCOLL jbc, IWKV_cursor cur, int64_t id, JBL jbl) {
  iwrc rc = 0;
  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    IWRC(_jb_idx_record_remove(idx, id, jbl), rc);
  }
  rc = iwkv_cursor_del(cur, 0);
  RCRET(rc);
  _jb_meta_nrecs_update(jbc->db, jbc->dbid, -1);
  jbc->rnum -= 1;
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

    _jb_meta_nrecs_removedb(db, jbc->dbid);

    for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
      key.data = keybuf;
      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_IDXMETA "%u" "." "%u", jbc->dbid, idx->dbid);
      rc = iwkv_del(jbc->db->metadb, &key, 0);
      RCGO(rc, finish);
      _jb_meta_nrecs_removedb(db, idx->dbid);
    }
    for (JBIDX idx = jbc->idx, nidx; idx; idx = nidx) {
      IWRC(iwkv_db_destroy(&idx->idb), rc);
      idx->idb = 0;
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

iwrc jb_collection_join_resolver(int64_t id, const char *coll, JBL *out, JBEXEC *ctx) {
  assert(out && ctx && coll);
  EJDB db = ctx->jbc->db;
  return jb_get(db, coll, id, JB_COLL_ACQUIRE_EXISTING, out);
}

int jb_proj_node_cache_cmp(const void *v1, const void *v2) {
  const struct _JBDOCREF *r1 = v1;
  const struct _JBDOCREF *r2 = v2;
  int ret = r1->id > r2->id ? 1 : r1->id < r2->id ? -1 : 0;
  if (!ret) {
    return strcmp(r1->coll, r2->coll);
    ;
  }
  return ret;
}

void jb_proj_node_kvfree(void *key, void *val) {
  free(key);
}

iwrc ejdb_rename_collection(EJDB db, const char *coll, const char *new_coll) {
  if (!coll || !new_coll) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  iwrc rc = 0;
  if (db->oflags & IWKV_RDONLY) {
    return IW_ERROR_READONLY;
  }
  IWKV_val key, val;
  JBL nmeta = 0, jbv = 0;
  char keybuf[JBNUMBUF_SIZE + sizeof(KEY_PREFIX_COLLMETA)];

  API_WLOCK(db, rci);

  khiter_t k = kh_get(JBCOLLM, db->mcolls, coll);
  if (k == kh_end(db->mcolls)) {
    rc = EJDB_ERROR_COLLECTION_NOT_FOUND;
    goto finish;
  }
  khiter_t k2 = kh_get(JBCOLLM, db->mcolls, new_coll);
  if (k2 != kh_end(db->mcolls)) {
    rc = EJDB_ERROR_TARGET_COLLECTION_EXISTS;
    goto finish;
  }

  JBCOLL jbc = kh_value(db->mcolls, k);

  rc = jbl_create_empty_object(&nmeta);
  RCGO(rc, finish);

  if (!binn_object_set_str(&nmeta->bn, "name", new_coll)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  if (!binn_object_set_uint32(&nmeta->bn, "id", jbc->dbid)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }

  rc = jbl_as_buf(nmeta, &val.data, &val.size);
  RCGO(rc, finish);
  key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u", jbc->dbid);
  if (key.size >= sizeof(keybuf)) {
    rc = IW_ERROR_OVERFLOW;
    goto finish;
  }
  key.data = keybuf;

  rc = jbl_at(nmeta, "/name", &jbv);
  RCGO(rc, finish);

  const char *new_name = jbl_get_str(jbv);

  rc = iwkv_put(db->metadb, &key, &val, IWKV_SYNC);
  RCGO(rc, finish);

  kh_del(JBCOLLM, db->mcolls, k);
  k2 = kh_put(JBCOLLM, db->mcolls, new_name, &rci);
  if (rci != -1) {
    kh_value(db->mcolls, k2) = jbc;
  } else {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  jbc->name = new_name;
  jbl_destroy(&jbc->meta);
  jbc->meta = nmeta;

finish:
  if (jbv) {
    jbl_destroy(&jbv);
  }
  if (rc) {
    if (nmeta) {
      jbl_destroy(&nmeta);
    }
  }
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
  if (  !binn_object_set_str(&jbl->bn, "file", sfsm.exfile.file.opts.path)
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
    if (!kh_exist(db->mcolls, k)) {
      continue;
    }
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
    if (clist) {
      binn_free(clist);
    }
    jbl_destroy(&jbl);
  } else {
    *jblp = jbl;
  }
  return rc;
}

iwrc ejdb_online_backup(EJDB db, uint64_t *ts, const char *target_file) {
  ENSURE_OPEN(db);
  return iwkv_online_backup(db->iwkv, ts, target_file);
}

iwrc ejdb_get_iwkv(EJDB db, IWKV *kvp) {
  if (!db || !kvp) {
    return IW_ERROR_INVALID_ARGS;
  }
  *kvp = db->iwkv;
  return 0;
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
    db->opts.sort_buffer_sz = 16 * 1024 * 1024; // 16Mb
  }
  if (db->opts.sort_buffer_sz < 1024 * 1024) { // Min 1Mb
    db->opts.sort_buffer_sz = 1024 * 1024;
  }
  if (!db->opts.document_buffer_sz) { // 64Kb
    db->opts.document_buffer_sz = 64 * 1024;
  }
  if (db->opts.document_buffer_sz < 16 * 1024) { // Min 16Kb
    db->opts.document_buffer_sz = 16 * 1024;
  }
  EJDB_HTTP *http = &db->opts.http;
  if (http->bind) {
    http->bind = strdup(http->bind);
  }
  if (http->access_token) {
    http->access_token = strdup(http->access_token);
    if (!http->access_token) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    http->access_token_len = strlen(http->access_token);
  }

  pthread_rwlockattr_t attr;
  pthread_rwlockattr_init(&attr);
#if defined __linux__ && (defined __USE_UNIX98 || defined __USE_XOPEN2K)
  pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
  rci = pthread_rwlock_init(&db->rwl, &attr);
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
  kvopts.wal.wal_lock_interceptor = _jb_wal_lock_interceptor;
  kvopts.wal.wal_lock_interceptor_opaque = db;

  rc = iwkv_open(&kvopts, &db->iwkv);
  RCGO(rc, finish);

  db->oflags = kvopts.oflags;
  rc = _jb_db_meta_load(db);
  RCGO(rc, finish);

  if (db->opts.http.enabled) {
    // Maximum WS/HTTP API body size. Default: 64Mb, Min: 512K
    if (!db->opts.http.max_body_size) {
      db->opts.http.max_body_size = 64 * 1024 * 1024;
    } else if (db->opts.http.max_body_size < 512 * 1024) {
      db->opts.http.max_body_size = 512 * 1024;
    }
  }

#ifdef JB_HTTP
  if (db->opts.http.enabled && !db->opts.http.blocking) {
    rc = jbr_start(db, &db->opts, &db->jbr);
    RCGO(rc, finish);
  }
#endif

finish:
  if (rc) {
    _jb_db_release(&db);
  } else {
    db->open = true;
    *ejdbp = db;
#ifdef JB_HTTP
    if (db->opts.http.enabled && db->opts.http.blocking) {
      rc = jbr_start(db, &db->opts, &db->jbr);
    }
#endif
  }
  return rc;
}

iwrc ejdb_close(EJDB *ejdbp) {
  if (!ejdbp || !*ejdbp) {
    return IW_ERROR_INVALID_ARGS;
  }
  EJDB db = *ejdbp;
  if (!__sync_bool_compare_and_swap(&db->open, 1, 0)) {
    iwlog_error2("Database is closed already");
    return IW_ERROR_INVALID_STATE;
  }
  iwrc rc = _jb_db_release(ejdbp);
  return rc;
}

const char *ejdb_git_revision(void) {
  return EJDB2_GIT_REVISION;
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
  if (!((ecode > _EJDB_ERROR_START) && (ecode < _EJDB_ERROR_END))) {
    return 0;
  }
  switch (ecode) {
    case EJDB_ERROR_INVALID_COLLECTION_META:
      return "Invalid collection metadata (EJDB_ERROR_INVALID_COLLECTION_META)";
    case EJDB_ERROR_INVALID_COLLECTION_INDEX_META:
      return "Invalid collection index metadata (EJDB_ERROR_INVALID_COLLECTION_INDEX_META)";
    case EJDB_ERROR_INVALID_INDEX_MODE:
      return "Invalid index mode specified (EJDB_ERROR_INVALID_INDEX_MODE)";
    case EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE:
      return "Index exists but mismatched uniqueness constraint (EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE)";
    case EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED:
      return "Unique index constraint violated (EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED)";
    case EJDB_ERROR_INVALID_COLLECTION_NAME:
      return "Invalid collection name (EJDB_ERROR_INVALID_COLLECTION_NAME)";
    case EJDB_ERROR_COLLECTION_NOT_FOUND:
      return "Collection not found (EJDB_ERROR_COLLECTION_NOT_FOUND)";
    case EJDB_ERROR_TARGET_COLLECTION_EXISTS:
      return "Target collection exists (EJDB_ERROR_TARGET_COLLECTION_EXISTS)";
    case EJDB_ERROR_PATCH_JSON_NOT_OBJECT:
      return "Patch JSON must be an object (map) (EJDB_ERROR_PATCH_JSON_NOT_OBJECT)";
  }
  return 0;
}

iwrc ejdb_init() {
  static volatile int jb_initialized = 0;
  if (!__sync_bool_compare_and_swap(&jb_initialized, 0, 1)) {
    return 0;  // initialized already
  }
  iwrc rc = iw_init();
  RCRET(rc);
  rc = jbl_init();
  RCRET(rc);
  rc = jql_init();
  RCRET(rc);
#ifdef JB_HTTP
  rc = jbr_init();
  RCRET(rc);
#endif
  return iwlog_register_ecodefn(_ejdb_ecodefn);
}
