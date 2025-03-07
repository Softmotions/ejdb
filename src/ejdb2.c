#include "ejdb2_internal.h"
#include <iowow/wyhash32.h>

#ifdef IW_BLOCKS
#include <Block.h>
#endif

static iwrc _jb_put_new_lw(struct jbcoll *jbc, struct jbl *jbl, int64_t *id);

static const struct iwkv_val EMPTY_VAL = { 0 };

IW_INLINE iwrc _jb_meta_nrecs_removedb(struct ejdb *db, uint32_t dbid) {
  dbid = IW_HTOIL(dbid);
  struct iwkv_val key = {
    .size = sizeof(dbid),
    .data = &dbid
  };
  return iwkv_del(db->nrecdb, &key, 0);
}

IW_INLINE iwrc _jb_meta_nrecs_update(struct ejdb *db, uint32_t dbid, int64_t delta) {
  delta = IW_HTOILL(delta);
  dbid = IW_HTOIL(dbid);
  struct iwkv_val val = {
    .size = sizeof(delta),
    .data = &delta
  };
  struct iwkv_val key = {
    .size = sizeof(dbid),
    .data = &dbid
  };
  return iwkv_put(db->nrecdb, &key, &val, IWKV_VAL_INCREMENT);
}

static int64_t _jb_meta_nrecs_get(struct ejdb *db, uint32_t dbid) {
  size_t vsz = 0;
  uint64_t ret = 0;
  dbid = IW_HTOIL(dbid);
  struct iwkv_val key = {
    .size = sizeof(dbid),
    .data = &dbid
  };
  iwkv_get_copy(db->nrecdb, &key, &ret, sizeof(ret), &vsz);
  if (vsz == sizeof(ret)) {
    ret = IW_ITOHLL(ret);
  }
  return (int64_t) ret;
}

static void _jb_idx_release(struct jbidx *idx) {
  free(idx->ptr);
  free(idx);
}

static void _jb_coll_release(struct jbcoll *jbc) {
  if (jbc->meta) {
    jbl_destroy(&jbc->meta);
  }
  struct jbidx *nidx;
  for (struct jbidx *idx = jbc->idx; idx; idx = nidx) {
    nidx = idx->next;
    _jb_idx_release(idx);
  }
  jbc->idx = 0;
  pthread_rwlock_destroy(&jbc->rwl);
  free(jbc);
}

static iwrc _jb_coll_load_index_lr(struct jbcoll *jbc, struct iwkv_val *mval) {
  iwrc rc;
  binn *bn;
  char *ptr;
  struct jbl imeta;
  struct jbidx *idx = calloc(1, sizeof(*idx));
  if (!idx) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }

  RCC(rc, finish, jbl_from_buf_keep_onstack(&imeta, mval->data, mval->size));
  bn = &imeta.bn;

  if (  !binn_object_get_str(bn, "ptr", &ptr)
     || !binn_object_get_uint8(bn, "mode", &idx->mode)
     || !binn_object_get_uint8(bn, "idbf", &idx->idbf)
     || !binn_object_get_uint32(bn, "dbid", &idx->dbid)) {
    rc = EJDB_ERROR_INVALID_COLLECTION_INDEX_META;
    goto finish;
  }

  RCC(rc, finish, jbl_ptr_alloc(ptr, &idx->ptr));
  RCC(rc, finish, iwkv_db(jbc->db->iwkv, idx->dbid, idx->idbf, &idx->idb));

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

static iwrc _jb_coll_load_indexes_lr(struct jbcoll *jbc) {
  iwrc rc = 0;
  struct iwkv_cursor *cur;
  struct iwkv_val kval;
  char buf[sizeof(KEY_PREFIX_IDXMETA) + IWNUMBUF_SIZE];
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
    struct iwkv_val key, val;
    RCC(rc, finish, iwkv_cursor_key(cur, &key));
    if ((key.size > sz) && !strncmp(buf, key.data, sz)) {
      iwkv_val_dispose(&key);
      RCC(rc, finish, iwkv_cursor_val(cur, &val));
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

static iwrc _jb_coll_load_meta_lr(struct jbcoll *jbc) {
  struct jbl *jbv;
  struct iwkv_cursor *cur;
  struct jbl *jbm = jbc->meta;

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
    RCC(rc, finish, iwkv_cursor_copy_key(cur, &jbc->id_seq, sizeof(jbc->id_seq), &sz, 0));
  }

finish:
  iwkv_cursor_close(&cur);
  return rc;
}

static iwrc _jb_coll_init(struct jbcoll *jbc, struct iwkv_val *meta) {
  iwrc rc = 0;
  pthread_rwlockattr_t attr;
  pthread_rwlockattr_init(&attr);
#if defined __linux__ && (defined __USE_UNIX98 || defined __USE_XOPEN2K)
  pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_READER_NP);
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

  rc = iwhmap_put(jbc->db->mcolls, (void*) jbc->name, jbc);
  RCRET(rc);

  return rc;
}

static iwrc _jb_idx_add_meta_lr(struct jbidx *idx, binn *list) {
  iwrc rc = 0;
  struct iwxstr *xstr = iwxstr_new();
  if (!xstr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  binn *meta = binn_object();
  if (!meta) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    iwxstr_destroy(xstr);
    return rc;
  }
  RCC(rc, finish, jbl_ptr_serialize(idx->ptr, xstr));

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

static iwrc _jb_coll_add_meta_lr(struct jbcoll *jbc, binn *list) {
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
  for (struct jbidx *idx = jbc->idx; idx; idx = idx->next) {
    RCC(rc, finish, _jb_idx_add_meta_lr(idx, ilist));
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

static iwrc _jb_db_meta_load(struct ejdb *db) {
  iwrc rc = 0;
  if (!db->metadb) {
    rc = iwkv_db(db->iwkv, METADB_ID, 0, &db->metadb);
    RCRET(rc);
  }
  if (!db->nrecdb) {
    rc = iwkv_db(db->iwkv, NUMRECSDB_ID, IWDB_VNUM64_KEYS, &db->nrecdb);
    RCRET(rc);
  }

  struct iwkv_cursor *cur;
  rc = iwkv_cursor_open(db->metadb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);
  while (!(rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT))) {
    struct iwkv_val key, val;
    RCC(rc, finish, iwkv_cursor_get(cur, &key, &val));
    if (!strncmp(key.data, KEY_PREFIX_COLLMETA, sizeof(KEY_PREFIX_COLLMETA) - 1)) {
      struct jbcoll *jbc = calloc(1, sizeof(*jbc));
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

static iwrc _jb_db_release(struct ejdb **dbp) {
  iwrc rc = 0;
  struct ejdb *db = *dbp;
  *dbp = 0;
#ifdef JB_HTTP
  jbr_shutdown_wait(db->jbr);
#endif
  if (db->mcolls) {
    iwhmap_destroy(db->mcolls);
    db->mcolls = 0;
  }
  if (db->iwkv) {
    IWRC(iwkv_close(&db->iwkv), rc);
  }
  pthread_rwlock_destroy(&db->rwl);

  struct ejdb_http *http = &db->opts.http;
  if (http->bind) {
    free((void*) http->bind);
  }
  if (http->access_token) {
    free((void*) http->access_token);
  }
  free(db);
  return rc;
}

static iwrc _jb_coll_acquire_keeplock2(
  struct ejdb *db, const char *coll, jb_coll_acquire_t acm,
  struct jbcoll **jbcp) {
  if (!coll || *coll == '\0' || strlen(coll) > EJDB_COLLECTION_NAME_MAX_LEN) {
    return EJDB_ERROR_INVALID_COLLECTION_NAME;
  }
  int rci;
  iwrc rc = 0;
  *jbcp = 0;
  struct jbcoll *jbc = 0;
  bool wl = acm & JB_COLL_ACQUIRE_WRITE;
  API_RLOCK(db, rci);

  jbc = iwhmap_get(db->mcolls, coll);
  if (jbc) {
    wl ? pthread_rwlock_wrlock(&jbc->rwl) : pthread_rwlock_rdlock(&jbc->rwl);
    *jbcp = jbc;
  } else {
    pthread_rwlock_unlock(&db->rwl); // relock
    if ((db->oflags & IWKV_RDONLY) || (acm & JB_COLL_ACQUIRE_EXISTING)) {
      return IW_ERROR_NOT_EXISTS;
    }
    API_WLOCK(db, rci);
    jbc = iwhmap_get(db->mcolls, coll);
    if (jbc) {
      pthread_rwlock_rdlock(&jbc->rwl);
      *jbcp = jbc;
    } else {
      struct jbl *meta = 0;
      struct iwdb *cdb = 0;
      uint32_t dbid = 0;
      char keybuf[IWNUMBUF_SIZE + sizeof(KEY_PREFIX_COLLMETA)];
      struct iwkv_val key, val;

      RCC(rc, create_finish, iwkv_new_db(db->iwkv, IWDB_VNUM64_KEYS, &dbid, &cdb));
      jbc = calloc(1, sizeof(*jbc));
      if (!jbc) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto create_finish;
      }
      RCC(rc, create_finish, jbl_create_empty_object(&meta));
      if (!binn_object_set_str(&meta->bn, "name", coll)) {
        rc = JBL_ERROR_CREATION;
        goto create_finish;
      }
      if (!binn_object_set_uint32(&meta->bn, "id", dbid)) {
        rc = JBL_ERROR_CREATION;
        goto create_finish;
      }
      RCC(rc, create_finish, jbl_as_buf(meta, &val.data, &val.size));

      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u", dbid);
      if (key.size >= sizeof(keybuf)) {
        rc = IW_ERROR_OVERFLOW;
        goto create_finish;
      }
      key.data = keybuf;
      RCC(rc, create_finish, iwkv_put(db->metadb, &key, &val, IWKV_SYNC));

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

IW_INLINE iwrc _jb_coll_acquire_keeplock(struct ejdb *db, const char *coll, bool wl, struct jbcoll **jbcp) {
  return _jb_coll_acquire_keeplock2(db, coll, wl ? JB_COLL_ACQUIRE_WRITE : 0, jbcp);
}

static iwrc _jb_idx_record_add(struct jbidx *idx, int64_t id, struct jbl *jbl, struct jbl *jblprev) {
  struct iwkv_val key;
  uint8_t step;
  char vnbuf[IW_VNUMBUFSZ];
  char numbuf[IWNUMBUF_SIZE];

  bool jbv_found, jbvprev_found;
  struct jbl jbv = { 0 }, jbvprev = { 0 };
  jbl_type_t jbv_type, jbvprev_type;

  iwrc rc = 0;
  struct iwpool *pool = 0;
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
    struct jbl_node *jbvprev_node, *jbv_node;
    RCC(rc, finish, jbl_to_node(&jbv, &jbv_node, false, pool));
    jbv.node = jbv_node;

    RCC(rc, finish, jbl_to_node(&jbvprev, &jbvprev_node, false, pool));
    jbvprev.node = jbvprev_node;

    if (_jbl_compare_nodes(jbv_node, jbvprev_node, &rc) == 0) {
      goto finish; // Arrays are equal or error
    }
  } else if (_jbl_is_eq_atomic_values(&jbv, &jbvprev)) {
    return 0;
  }

  if (jbvprev_found) {               // Remove old index elements
    if (jbvprev_type == JBV_ARRAY) { // TODO: array modification delta?
      struct jbl_node *n;
      if (!pool) {
        pool = iwpool_create(1024);
        if (!pool) {
          RCC(rc, finish, iwrc_set_errno(IW_ERROR_ALLOC, errno));
        }
      }
      RCC(rc, finish, jbl_to_node(&jbvprev, &n, false, pool));
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
      struct jbl_node *n;
      if (!pool) {
        pool = iwpool_create(1024);
        if (!pool) {
          RCC(rc, finish, iwrc_set_errno(IW_ERROR_ALLOC, errno));
        }
      }
      RCC(rc, finish, jbl_to_node(&jbv, &n, false, pool));
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
          struct iwkv_val idval = {
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

IW_INLINE iwrc _jb_idx_record_remove(struct jbidx *idx, int64_t id, struct jbl *jbl) {
  return _jb_idx_record_add(idx, id, 0, jbl);
}

static iwrc _jb_idx_fill(struct jbidx *idx) {
  struct iwkv_cursor *cur;
  struct iwkv_val key, val;
  struct jbl jbs;
  int64_t llv;
  struct jbl *jbl = &jbs;

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
static iwrc _jb_put_handler_after(iwrc rc, struct _jb_put_handler_ctx *ctx) {
  struct iwkv_val *oldval = &ctx->oldval;
  if (rc) {
    if (oldval->size) {
      iwkv_val_dispose(oldval);
    }
    return rc;
  }
  struct jbl *prev;
  struct jbl jblprev;
  struct jbcoll *jbc = ctx->jbc;
  if (oldval->size) {
    rc = jbl_from_buf_keep_onstack(&jblprev, oldval->data, oldval->size);
    RCRET(rc);
    prev = &jblprev;
  } else {
    prev = 0;
  }
  struct jbidx *fail_idx = 0;
  for (struct jbidx *idx = jbc->idx; idx; idx = idx->next) {
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
    struct iwkv_val key = { .data = &ctx->id, .size = sizeof(ctx->id) };
    for (struct jbidx *idx = jbc->idx; idx && idx != fail_idx; idx = idx->next) {
      IWRC(_jb_idx_record_remove(idx, ctx->id, ctx->jbl), rc);
    }
    IWRC(iwkv_del(jbc->cdb, &key, 0), rc);
  }
  return rc;
}

static iwrc _jb_put_handler(const struct iwkv_val *key, const struct iwkv_val *val, struct iwkv_val *oldval, void *op) {
  struct _jb_put_handler_ctx *ctx = op;
  if (oldval && oldval->size) {
    memcpy(&ctx->oldval, oldval, sizeof(*oldval));
  }
  return 0;
}

static iwrc _jb_exec_scan_init(struct jbexec *ctx) {
  ctx->istep = 1;
  ctx->jblbufsz = ctx->jbc->db->opts.document_buffer_sz;
  ctx->jblbuf = malloc(ctx->jblbufsz);
  if (!ctx->jblbuf) {
    ctx->jblbufsz = 0;
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  struct jqp_aux *aux = ctx->ux->q->aux;
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

static void _jb_exec_scan_release(struct jbexec *ctx) {
  if (ctx->proj_joined_nodes_cache) {
    // Destroy projected nodes key
    iwhmap_destroy(ctx->proj_joined_nodes_cache);
    ctx->proj_joined_nodes_cache = 0;
  }
  if (ctx->proj_joined_nodes_pool) {
    iwpool_destroy(ctx->proj_joined_nodes_pool);
  }
  free(ctx->jblbuf);
}

static iwrc _jb_noop_visitor(struct ejdb_exec *ctx, struct ejdb_doc *doc, int64_t *step) {
  return 0;
}

IW_INLINE iwrc _jb_put_impl(struct jbcoll *jbc, struct jbl *jbl, int64_t id) {
  struct iwkv_val val, key = {
    .data = &id,
    .size = sizeof(id)
  };
  struct _jb_put_handler_ctx pctx = {
    .id = id,
    .jbc = jbc,
    .jbl = jbl
  };
  iwrc rc = jbl_as_buf(jbl, &val.data, &val.size);
  RCRET(rc);
  return _jb_put_handler_after(iwkv_puth(jbc->cdb, &key, &val, 0, _jb_put_handler, &pctx), &pctx);
}

iwrc jb_put(struct jbcoll *jbc, struct jbl *jbl, int64_t id) {
  return _jb_put_impl(jbc, jbl, id);
}

iwrc jb_cursor_set(struct jbcoll *jbc, struct iwkv_cursor *cur, int64_t id, struct jbl *jbl) {
  struct iwkv_val val;
  struct _jb_put_handler_ctx pctx = {
    .id = id,
    .jbc = jbc,
    .jbl = jbl
  };
  iwrc rc = jbl_as_buf(jbl, &val.data, &val.size);
  RCRET(rc);
  return _jb_put_handler_after(iwkv_cursor_seth(cur, &val, 0, _jb_put_handler, &pctx), &pctx);
}

static iwrc _jb_exec_upsert_lw(struct jbexec *ctx) {
  struct jbl_node *n;
  int64_t id;
  iwrc rc = 0;
  struct jbl *jbl = 0;
  struct ejdb_exec *ux = ctx->ux;
  struct jql *q = ux->q;
  if (q->aux->apply_placeholder) {
    struct jqval *pv = jql_find_placeholder(q, q->aux->apply_placeholder);
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
    struct ejdb_doc doc = {
      .id = id,
      .raw = jbl,
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

iwrc ejdb_exec(struct ejdb_exec *ux) {
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
  struct jbexec ctx = {
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

  RCC(rc, finish, _jb_exec_scan_init(&ctx));
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

#ifdef IW_BLOCKS

struct _block_visitor_ctx {
  struct ejdb_visit_block *bctx;
  bool (^visitor)(struct ejdb_doc*);
};

static iwrc _block_visitor(struct ejdb_exec *ux, struct ejdb_doc *doc, int64_t *step) {
  struct _block_visitor_ctx *ctx = ux->opaque;
  ++ctx->bctx->num_visits;
  bool ret = ctx->visitor(doc);
  if (!ret) {
    *step = 0;
  }
  return 0;
}

iwrc ejdb_visit_block2(struct ejdb_visit_block *bctx, bool (^visitor)(struct ejdb_doc*)) {
  bctx->num_visits = 0;
  struct _block_visitor_ctx ctx = {
    .bctx = bctx,
    .visitor = visitor,
  };
  struct ejdb_exec ux = {
    .db = bctx->db,
    .q = bctx->q,
    .visitor = _block_visitor,
    .opaque = &ctx,
    .pool = bctx->pool,
  };
  return ejdb_exec(&ux);
}

iwrc ejdb_visit_block(struct ejdb *db, struct jql *q, bool (^visitor)(struct ejdb_doc*)) {
  return ejdb_visit_block2(&(struct ejdb_visit_block) {
    .db = db,
    .q = q,
  }, visitor);
}

#endif

struct _list_visitor_ctx {
  struct ejdb_doc *head;
  struct ejdb_doc *tail;
};

static iwrc _jb_exec_list_visitor(struct ejdb_exec *ctx, struct ejdb_doc *doc, int64_t *step) {
  struct _list_visitor_ctx *lvc = ctx->opaque;
  struct iwpool *pool = ctx->pool;
  struct ejdb_doc *ndoc = iwpool_alloc(sizeof(*ndoc) + sizeof(*doc->raw) + doc->raw->bn.size, pool);
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

static iwrc _jb_list(
  struct ejdb      *db,
  struct jql       *q,
  struct ejdb_doc **first,
  int64_t           limit,
  struct iwxstr    *log,
  struct iwpool    *pool) {
  if (!db || !q || !first || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  struct _list_visitor_ctx lvc = { 0 };
  struct ejdb_exec ux = {
    .db = db,
    .q = q,
    .visitor = _jb_exec_list_visitor,
    .pool = pool,
    .limit = limit,
    .log = log,
    .opaque = &lvc
  };
  rc = ejdb_exec(&ux);
  if (rc) {
    *first = 0;
  } else {
    *first = lvc.head;
  }
  return rc;
}

static iwrc _jb_count(struct ejdb *db, struct jql *q, int64_t *count, int64_t limit, struct iwxstr *log) {
  if (!db || !q || !count) {
    return IW_ERROR_INVALID_ARGS;
  }
  struct ejdb_exec ux = {
    .db = db,
    .q = q,
    .limit = limit,
    .log = log
  };
  iwrc rc = ejdb_exec(&ux);
  *count = ux.cnt;
  return rc;
}

iwrc ejdb_count(struct ejdb *db, struct jql *q, int64_t *count, int64_t limit) {
  return _jb_count(db, q, count, limit, 0);
}

iwrc ejdb_count2(struct ejdb *db, const char *coll, const char *q, int64_t *count, int64_t limit) {
  struct jql *jql;
  iwrc rc = jql_create(&jql, coll, q);
  RCRET(rc);
  rc = _jb_count(db, jql, count, limit, 0);
  jql_destroy(&jql);
  return rc;
}

iwrc ejdb_update(struct ejdb *db, struct jql *q) {
  int64_t count;
  return ejdb_count(db, q, &count, 0);
}

iwrc ejdb_update2(struct ejdb *db, const char *coll, const char *q) {
  int64_t count;
  return ejdb_count2(db, coll, q, &count, 0);
}

iwrc ejdb_list(struct ejdb *db, struct jql *q, struct ejdb_doc **first, int64_t limit, struct iwpool *pool) {
  return _jb_list(db, q, first, limit, 0, pool);
}

iwrc ejdb_list3(
  struct ejdb       *db,
  const char        *coll,
  const char        *query,
  int64_t            limit,
  struct iwxstr     *log,
  struct ejdb_list **listp) {
  if (!listp) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  *listp = 0;
  struct iwpool *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  struct ejdb_list *list = iwpool_alloc(sizeof(*list), pool);
  if (!list) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  list->first = 0;
  list->db = db;
  list->pool = pool;
  RCC(rc, finish, jql_create(&list->q, coll, query));
  rc = _jb_list(db, list->q, &list->first, limit, log, list->pool);

finish:
  if (rc) {
    iwpool_destroy(pool);
  } else {
    *listp = list;
  }
  return rc;
}

iwrc ejdb_list4(struct ejdb *db, struct jql *q, int64_t limit, struct iwxstr *log, struct ejdb_list **listp) {
  if (!listp) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  *listp = 0;
  struct iwpool *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  struct ejdb_list *list = iwpool_alloc(sizeof(*list), pool);
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

iwrc ejdb_list2(struct ejdb *db, const char *coll, const char *query, int64_t limit, struct ejdb_list **listp) {
  return ejdb_list3(db, coll, query, limit, 0, listp);
}

void ejdb_list_destroy(struct ejdb_list **listp) {
  if (listp) {
    struct ejdb_list *list = *listp;
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

iwrc ejdb_remove_index(struct ejdb *db, const char *coll, const char *path, ejdb_idx_mode_t mode) {
  if (!db || !coll || !path) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  struct jbcoll *jbc;
  struct iwkv_val key;
  struct jbl_ptr *ptr = 0;
  char keybuf[sizeof(KEY_PREFIX_IDXMETA) + 1 + 2UL * IWNUMBUF_SIZE]; // Full key format: i.<coldbid>.<idxdbid>

  iwrc rc = _jb_coll_acquire_keeplock2(db, coll, JB_COLL_ACQUIRE_WRITE | JB_COLL_ACQUIRE_EXISTING, &jbc);
  RCRET(rc);

  RCC(rc, finish, jbl_ptr_alloc(path, &ptr));

  for (struct jbidx *idx = jbc->idx, *prev = 0; idx; idx = idx->next) {
    if (((idx->mode & ~EJDB_IDX_UNIQUE) == (mode & ~EJDB_IDX_UNIQUE)) && !jbl_ptr_cmp(idx->ptr, ptr)) {
      key.data = keybuf;
      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_IDXMETA "%u" "." "%u", jbc->dbid, idx->dbid);
      if (key.size >= sizeof(keybuf)) {
        rc = IW_ERROR_OVERFLOW;
        goto finish;
      }
      RCC(rc, finish, iwkv_del(db->metadb, &key, 0));
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

iwrc ejdb_ensure_index(struct ejdb *db, const char *coll, const char *path, ejdb_idx_mode_t mode) {
  if (!db || !coll || !path) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  struct jbcoll *jbc;
  struct iwkv_val key, val;
  char keybuf[sizeof(KEY_PREFIX_IDXMETA) + 1 + 2UL * IWNUMBUF_SIZE]; // Full key format: i.<coldbid>.<idxdbid>

  struct jbidx *idx = 0;
  struct jbl_ptr *ptr = 0;
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

  RCC(rc, finish, jbl_ptr_alloc(path, &ptr));

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

  RCC(rc, finish, iwkv_new_db(db->iwkv, idx->idbf, &idx->dbid, &idx->idb));
  RCC(rc, finish, _jb_idx_fill(idx));

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
  RCC(rc, finish, iwkv_put(db->metadb, &key, &val, 0));

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
  struct ejdb *db, const char *coll, int64_t id, bool upsert,
  const char *patchjson, struct jbl_node *patchjbn, struct jbl *patchjbl) {
  int rci;
  struct jbcoll *jbc;
  struct jbl sjbl;
  struct jbl_node *root, *patch;
  struct jbl *ujbl = 0;
  struct iwpool *pool = 0;
  struct iwkv_val val = { 0 };
  struct iwkv_val key = {
    .data = &id,
    .size = sizeof(id)
  };

  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);

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

  RCC(rc, finish, jbl_from_buf_keep_onstack(&sjbl, val.data, val.size));

  pool = iwpool_create_empty();
  if (!pool) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  RCC(rc, finish, jbl_to_node(&sjbl, &root, false, pool));

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

  RCC(rc, finish, jbn_patch_auto(root, patch, pool));

  if (root->type == JBV_OBJECT) {
    RCC(rc, finish, jbl_create_empty_object(&ujbl));
  } else if (root->type == JBV_ARRAY) {
    RCC(rc, finish, jbl_create_empty_array(&ujbl));
  } else {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }

  RCC(rc, finish, jbl_fill_from_node(ujbl, root));
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
  struct ejdb *db = op;
  assert(db);
  if (before) {
    API_WLOCK2(db, rci);
  } else {
    API_UNLOCK(db, rci, rc);
  }
  return rc;
}

iwrc ejdb_patch(struct ejdb *db, const char *coll, const char *patchjson, int64_t id) {
  return _jb_patch(db, coll, id, false, patchjson, 0, 0);
}

iwrc ejdb_patch_jbn(struct ejdb *db, const char *coll, struct jbl_node *patch, int64_t id) {
  return _jb_patch(db, coll, id, false, 0, patch, 0);
}

iwrc ejdb_patch_jbl(struct ejdb *db, const char *coll, struct jbl *patch, int64_t id) {
  return _jb_patch(db, coll, id, false, 0, 0, patch);
}

iwrc ejdb_merge_or_put(struct ejdb *db, const char *coll, const char *patchjson, int64_t id) {
  return _jb_patch(db, coll, id, true, patchjson, 0, 0);
}

iwrc ejdb_merge_or_put_jbn(struct ejdb *db, const char *coll, struct jbl_node *patch, int64_t id) {
  return _jb_patch(db, coll, id, true, 0, patch, 0);
}

iwrc ejdb_merge_or_put_jbl(struct ejdb *db, const char *coll, struct jbl *patch, int64_t id) {
  return _jb_patch(db, coll, id, true, 0, 0, patch);
}

iwrc ejdb_put(struct ejdb *db, const char *coll, struct jbl *jbl, int64_t id) {
  if (!jbl) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  struct jbcoll *jbc;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);
  rc = _jb_put_impl(jbc, jbl, id);
  if (!rc && (jbc->id_seq < id)) {
    jbc->id_seq = id;
  }
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_put_jbn(struct ejdb *db, const char *coll, struct jbl_node *jbn, int64_t id) {
  struct jbl *jbl = 0;
  iwrc rc = jbl_from_node(&jbl, jbn);
  RCRET(rc);
  rc = ejdb_put(db, coll, jbl, id);
  jbl_destroy(&jbl);
  return rc;
}

static iwrc _jb_put_new_lw(struct jbcoll *jbc, struct jbl *jbl, int64_t *id) {
  iwrc rc = 0;
  int64_t oid = jbc->id_seq + 1;
  struct iwkv_val val, key = {
    .data = &oid,
    .size = sizeof(oid)
  };
  struct _jb_put_handler_ctx pctx = {
    .id = oid,
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

iwrc ejdb_put_new(struct ejdb *db, const char *coll, struct jbl *jbl, int64_t *id) {
  if (!jbl) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  struct jbcoll *jbc;
  if (id) {
    *id = 0;
  }
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);

  rc = _jb_put_new_lw(jbc, jbl, id);

  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_put_new_jbn(struct ejdb *db, const char *coll, struct jbl_node *jbn, int64_t *id) {
  struct jbl *jbl = 0;
  iwrc rc = jbl_from_node(&jbl, jbn);
  RCRET(rc);
  rc = ejdb_put_new(db, coll, jbl, id);
  jbl_destroy(&jbl);
  return rc;
}

iwrc jb_get(struct ejdb *db, const char *coll, int64_t id, jb_coll_acquire_t acm, struct jbl **jblp) {
  if (!id || !jblp) {
    return IW_ERROR_INVALID_ARGS;
  }
  *jblp = 0;

  int rci;
  struct jbcoll *jbc;
  struct jbl *jbl = 0;
  struct iwkv_val val = { 0 };
  struct iwkv_val key = { .data = &id, .size = sizeof(id) };

  iwrc rc = _jb_coll_acquire_keeplock2(db, coll, acm, &jbc);
  RCRET(rc);

  RCC(rc, finish, iwkv_get(jbc->cdb, &key, &val));
  RCC(rc, finish, jbl_from_buf_keep(&jbl, val.data, val.size, false));

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

iwrc ejdb_get(struct ejdb *db, const char *coll, int64_t id, struct jbl **jblp) {
  return jb_get(db, coll, id, JB_COLL_ACQUIRE_EXISTING, jblp);
}

iwrc ejdb_del(struct ejdb *db, const char *coll, int64_t id) {
  int rci;
  struct jbcoll *jbc;
  struct jbl jbl;
  struct iwkv_val val = { 0 };
  struct iwkv_val key = { .data = &id, .size = sizeof(id) };

  iwrc rc = _jb_coll_acquire_keeplock2(db, coll, JB_COLL_ACQUIRE_WRITE | JB_COLL_ACQUIRE_EXISTING, &jbc);
  RCRET(rc);

  RCC(rc, finish, iwkv_get(jbc->cdb, &key, &val));
  RCC(rc, finish, jbl_from_buf_keep_onstack(&jbl, val.data, val.size));

  for (struct jbidx *idx = jbc->idx; idx; idx = idx->next) {
    IWRC(_jb_idx_record_remove(idx, id, &jbl), rc);
  }

  RCC(rc, finish, iwkv_del(jbc->cdb, &key, 0));
  _jb_meta_nrecs_update(jbc->db, jbc->dbid, -1);
  jbc->rnum -= 1;

finish:
  if (val.data) {
    iwkv_val_dispose(&val);
  }
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc jb_del(struct jbcoll *jbc, struct jbl *jbl, int64_t id) {
  iwrc rc = 0;
  struct iwkv_val key = { .data = &id, .size = sizeof(id) };
  for (struct jbidx *idx = jbc->idx; idx; idx = idx->next) {
    IWRC(_jb_idx_record_remove(idx, id, jbl), rc);
  }
  rc = iwkv_del(jbc->cdb, &key, 0);
  RCRET(rc);
  _jb_meta_nrecs_update(jbc->db, jbc->dbid, -1);
  jbc->rnum -= 1;
  return rc;
}

iwrc jb_cursor_del(struct jbcoll *jbc, struct iwkv_cursor *cur, int64_t id, struct jbl *jbl) {
  iwrc rc = 0;
  for (struct jbidx *idx = jbc->idx; idx; idx = idx->next) {
    IWRC(_jb_idx_record_remove(idx, id, jbl), rc);
  }
  rc = iwkv_cursor_del(cur, 0);
  RCRET(rc);
  _jb_meta_nrecs_update(jbc->db, jbc->dbid, -1);
  jbc->rnum -= 1;
  return rc;
}

iwrc ejdb_ensure_collection(struct ejdb *db, const char *coll) {
  int rci;
  struct jbcoll *jbc;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, false, &jbc);
  RCRET(rc);
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_remove_collection(struct ejdb *db, const char *coll) {
  int rci;
  iwrc rc = 0;
  if (db->oflags & IWKV_RDONLY) {
    return IW_ERROR_READONLY;
  }
  API_WLOCK(db, rci);
  struct jbcoll *jbc;
  struct iwkv_val key;
  char keybuf[sizeof(KEY_PREFIX_IDXMETA) + 1 + 2UL * IWNUMBUF_SIZE]; // Full key format: i.<coldbid>.<idxdbid>

  jbc = iwhmap_get(db->mcolls, coll);
  if (jbc) {
    key.data = keybuf;
    key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u", jbc->dbid);

    RCC(rc, finish, iwkv_del(jbc->db->metadb, &key, IWKV_SYNC));

    _jb_meta_nrecs_removedb(db, jbc->dbid);

    for (struct jbidx *idx = jbc->idx; idx; idx = idx->next) {
      key.data = keybuf;
      key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_IDXMETA "%u" "." "%u", jbc->dbid, idx->dbid);
      RCC(rc, finish, iwkv_del(jbc->db->metadb, &key, 0));
      _jb_meta_nrecs_removedb(db, idx->dbid);
    }
    for (struct jbidx *idx = jbc->idx, *nidx; idx; idx = nidx) {
      IWRC(iwkv_db_destroy(&idx->idb), rc);
      idx->idb = 0;
      nidx = idx->next;
      _jb_idx_release(idx);
    }
    jbc->idx = 0;
    IWRC(iwkv_db_destroy(&jbc->cdb), rc);
    iwhmap_remove(db->mcolls, coll);
  }

finish:
  API_UNLOCK(db, rci, rc);
  return rc;
}

iwrc jb_collection_join_resolver(int64_t id, const char *coll, struct jbl **out, struct jbexec *ctx) {
  assert(out && ctx && coll);
  struct ejdb *db = ctx->jbc->db;
  return jb_get(db, coll, id, JB_COLL_ACQUIRE_EXISTING, out);
}

int jb_proj_node_cache_cmp(const void *v1, const void *v2) {
  const struct jbdocref *r1 = v1;
  const struct jbdocref *r2 = v2;
  int ret = r1->id > r2->id ? 1 : r1->id < r2->id ? -1 : 0;
  if (!ret) {
    return strcmp(r1->coll, r2->coll);
  }
  return ret;
}

void jb_proj_node_kvfree(void *key, void *val) {
  free(key);
}

uint32_t jb_proj_node_hash(const void *key) {
  const struct jbdocref *ref = key;
  return wyhash32(key, sizeof(ref), 0xd31c3939);
}

iwrc ejdb_rename_collection(struct ejdb *db, const char *coll, const char *new_coll) {
  if (!coll || !new_coll) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  iwrc rc = 0;
  if (db->oflags & IWKV_RDONLY) {
    return IW_ERROR_READONLY;
  }
  struct iwkv_val key, val;
  struct jbl *nmeta = 0, *jbv = 0;
  char keybuf[IWNUMBUF_SIZE + sizeof(KEY_PREFIX_COLLMETA)];

  API_WLOCK(db, rci);

  struct jbcoll *jbc = iwhmap_get(db->mcolls, coll);
  if (!jbc) {
    rc = EJDB_ERROR_COLLECTION_NOT_FOUND;
    goto finish;
  }

  if (iwhmap_get(db->mcolls, new_coll)) {
    rc = EJDB_ERROR_TARGET_COLLECTION_EXISTS;
    goto finish;
  }

  RCC(rc, finish, jbl_create_empty_object(&nmeta));
  if (!binn_object_set_str(&nmeta->bn, "name", new_coll)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }

  if (!binn_object_set_uint32(&nmeta->bn, "id", jbc->dbid)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }

  RCC(rc, finish, jbl_as_buf(nmeta, &val.data, &val.size));
  key.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u", jbc->dbid);
  if (key.size >= sizeof(keybuf)) {
    rc = IW_ERROR_OVERFLOW;
    goto finish;
  }
  key.data = keybuf;

  RCC(rc, finish, jbl_at(nmeta, "/name", &jbv));
  const char *new_name = jbl_get_str(jbv);
  RCC(rc, finish, iwkv_put(db->metadb, &key, &val, IWKV_SYNC));
  RCC(rc, finish, iwhmap_rename(db->mcolls, coll, (void*) new_name));

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

iwrc ejdb_get_meta(struct ejdb *db, struct jbl **jblp) {
  int rci;
  *jblp = 0;
  struct jbl *jbl;
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

  struct iwhmap_iter iter;
  iwhmap_iter_init(db->mcolls, &iter);
  while (iwhmap_iter_next(&iter)) {
    struct jbcoll *jbc = (void*) iter.val;
    RCC(rc, finish, _jb_coll_add_meta_lr(jbc, clist));
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

iwrc ejdb_online_backup(struct ejdb *db, uint64_t *ts, const char *target_file) {
  ENSURE_OPEN(db);
  return iwkv_online_backup(db->iwkv, ts, target_file);
}

iwrc ejdb_get_iwkv(struct ejdb *db, IWKV *kvp) {
  if (!db || !kvp) {
    return IW_ERROR_INVALID_ARGS;
  }
  *kvp = db->iwkv;
  return 0;
}

static void _mcolls_map_entry_free(void *key, void *val) {
  if (val) {
    _jb_coll_release(val);
  }
}

iwrc ejdb_open(const struct ejdb_opts *_opts, struct ejdb **ejdbp) {
  *ejdbp = 0;
  int rci;
  iwrc rc = ejdb_init();
  RCRET(rc);
  if (!_opts || !_opts->kv.path || !ejdbp) {
    return IW_ERROR_INVALID_ARGS;
  }

  struct ejdb *db = calloc(1, sizeof(*db));
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
  struct ejdb_http *http = &db->opts.http;
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
  pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_READER_NP);
#endif
  rci = pthread_rwlock_init(&db->rwl, &attr);
  if (rci) {
    rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    free(db);
    return rc;
  }
  RCB(finish, db->mcolls = iwhmap_create_str(_mcolls_map_entry_free));

  struct iwkv_opts kvopts;
  memcpy(&kvopts, &db->opts.kv, sizeof(db->opts.kv));
  kvopts.wal.enabled = !db->opts.no_wal;
  kvopts.wal.wal_lock_interceptor = _jb_wal_lock_interceptor;
  kvopts.wal.wal_lock_interceptor_opaque = db;

  RCC(rc, finish, iwkv_open(&kvopts, &db->iwkv));

  db->oflags = kvopts.oflags;
  RCC(rc, finish, _jb_db_meta_load(db));

  if (db->opts.http.enabled) {
    // Maximum WS/HTTP API body size. Default: 64Mb, Min: 512K
    if (!db->opts.http.max_body_size) {
      db->opts.http.max_body_size = 64UL * 1024 * 1024;
    } else if (db->opts.http.max_body_size < 512UL * 1024) {
      db->opts.http.max_body_size = 512UL * 1024;
    }
  }

#ifdef JB_HTTP
  if (db->opts.http.enabled && !db->opts.http.blocking) {
    RCC(rc, finish, jbr_start(db, &db->opts, &db->jbr));
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

iwrc ejdb_close(struct ejdb **ejdbp) {
  if (!ejdbp || !*ejdbp) {
    return IW_ERROR_INVALID_ARGS;
  }
  struct ejdb *db = *ejdbp;
  if (!__sync_bool_compare_and_swap(&db->open, 1, 0)) {
    iwlog_error2("Database is closed already");
    return IW_ERROR_INVALID_STATE;
  }
  iwrc rc = _jb_db_release(ejdbp);
  return rc;
}

const char* ejdb_git_revision(void) {
  return EJDB2_GIT_REVISION;
}

const char* ejdb_version_full(void) {
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

static const char* _ejdb_ecodefn(locale_t locale, uint32_t ecode) {
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
    default:
      break;
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
