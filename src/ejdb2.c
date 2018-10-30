#include "ejdb2.h"
#include "jql.h"
#include "jql_internal.h"
#include "jbl_internal.h"
#include <iowow/iwkv.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include "khash.h"
#include "ejdb2cfg.h"

#define IWDB_DUP_FLAGS (IWDB_DUP_UINT32_VALS | IWDB_DUP_UINT64_VALS)

#define METADB_ID 1
#define KEY_PREFIX_COLLMETA "col."

#define ENSURE_OPEN(db_) \
  if (!(db_) || !((db_)->open)) return IW_ERROR_INVALID_STATE;

#define API_RLOCK(db_, rci_) \
  ENSURE_OPEN(db_);  \
  rci_ = pthread_rwlock_rdlock(&(db_)->rwl); \
  if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_WLOCK(db_, rci_) \
  ENSURE_OPEN(db_);  \
  rci_ = pthread_rwlock_wrlock(&(db_)->rwl); \
  if (rci_) return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_)

#define API_UNLOCK(db_, rci_, rc_)  \
  rci_ = pthread_rwlock_unlock(&(db_)->rwl); \
  if (rci_) IWRC(iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_), rc_)

#define API_COLL_UNLOCK(jbc_, rci_, rc_)                                     \
  do {                                                                    \
    rci_ = pthread_rwlock_unlock(&(jbc_)->rwl);                            \
    if (rci_) IWRC(iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_), rc_);  \
    API_UNLOCK((jbc_)->db, rci_, rc_);                                   \
  } while(0)

struct _JBIDX;
typedef struct _JBIDX *JBIDX;

/** Database collection */
typedef struct _JBCOLL {
  uint32_t dbid;            /**< IWKV collection database ID */
  const char *name;         /**< Collection name */
  IWDB cdb;                 /**< IWKV collection database */
  EJDB db;                  /**< Main database reference */
  JBL meta;                 /**< Collection meta object */
  JBIDX idx;                /**< First index in chain */
  pthread_rwlock_t rwl;
  uint64_t id_seq;
} *JBCOLL;

/** Database collection index */
struct _JBIDX {
  ejdb_idx_mode_t mode;     /**< Index mode/type mask */
  iwdb_flags_t idbf;        /**< Index database flags */
  JBCOLL jbc;               /**< Owner document collection */
  JBL_PTR ptr;              /**< Indexed JSON path poiner */
  IWDB idb;                 /**< KV database for this index */
  struct _JBIDX *next;      /**< Next index in chain */
};

KHASH_MAP_INIT_STR(JBCOLLM, JBCOLL)

struct _EJDB {
  IWKV iwkv;
  IWDB metadb;
  khash_t(JBCOLLM) *mcolls;
  volatile bool open;
  iwkv_openflags oflags;
  pthread_rwlock_t rwl;       /**< Main RWL */
};

// ---------------------------------------------------------------------------

static iwrc _jb_sync_meta(EJDB db) {
  // todo
  return 0;
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

  rc = iwkv_cursor_open(jbc->cdb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
  RCRET(rc);

  rc = iwkv_cursor_to(cur, IWKV_CURSOR_NEXT);
  if (rc) {
    if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
  } else {
    uint64_t llv;
    IWKV_val key;
    rc = iwkv_cursor_key(cur, &key);
    RCGO(rc, finish);
    memcpy(&llv, key.data, sizeof(llv));
    jbc->id_seq = llv;
  }
finish:
  iwkv_cursor_close(&cur);
  return rc;
}

static void _jb_idx_destroy(JBIDX idx) {
  if (idx->idb) {
    iwkv_db_cache_release(idx->idb);
  }
  if (idx->ptr) {
    free(idx->ptr);
  }
  free(idx);
}

static void _jb_coll_destroy(JBCOLL jbc) {
  if (jbc->meta) {
    jbl_destroy(&jbc->meta);
  }
  if (jbc->cdb) {
    iwkv_db_cache_release(jbc->cdb);
  }
  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    _jb_idx_destroy(idx);
  }
  jbc->idx = 0;
  pthread_rwlock_destroy(&jbc->rwl);
  free(jbc);
}

static iwrc _jb_coll_init(JBCOLL jbc, IWKV_val *meta) {
  int rci;
  iwrc rc = 0;
  pthread_rwlock_init(&jbc->rwl, 0);
  if (meta) {
    rc = jbl_from_buf_keep(&jbc->meta, meta->data, meta->size);
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

static iwrc _jb_coll_add_meta_lr(JBCOLL jbc, binn *list) {
  iwrc rc = 0;
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
  if (!binn_list_add_value(list, meta)) {
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
finish:
  if (meta) binn_free(meta);
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
        _jb_coll_destroy(jbc);
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

static void _jb_db_destroy(EJDB *dbp) {
  EJDB db = *dbp;
  if (db->mcolls) {
    for (khiter_t k = kh_begin(db->mcolls); k != kh_end(db->mcolls); ++k) {
      if (!kh_exist(db->mcolls, k)) continue;
      JBCOLL jbc = kh_val(db->mcolls, k);
      _jb_coll_destroy(jbc);
    }
    kh_destroy(JBCOLLM, db->mcolls);
    db->mcolls = 0;
  }
  if (db->iwkv) {
    iwkv_close(&db->iwkv);
  }
  pthread_rwlock_destroy(&db->rwl);
  free(db);
  *dbp = 0;
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
      char keybuf[JBNUMBUF_SIZE + 4];
      IWKV_val metakey, metaval;

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
      rc = jbl_as_buf(meta, &metaval.data, &metaval.size);
      RCGO(rc, create_finish);

      metakey.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u",  dbid);
      if (metakey.size >= sizeof(keybuf)) {
        rc = IW_ERROR_OVERFLOW;
        goto create_finish;
      }
      metakey.data = keybuf;
      rc = iwkv_put(db->metadb, &metakey, &metaval, IWKV_SYNC);
      RCGO(rc, create_finish);

      jbc->db = db;
      jbc->meta = meta;
      rc = _jb_coll_init(jbc, 0);
      if (rc) {
        iwkv_del(db->metadb, &metakey, IWKV_SYNC);
        goto create_finish;
      }
create_finish:
      if (rc) {
        if (meta) jbl_destroy(&meta);
        if (cdb) iwkv_db_destroy(&cdb);
        if (jbc) {
          jbc->meta = 0; // meta was cleared
          _jb_coll_destroy(jbc);
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

//----------------------- Public API

iwrc ejdb_ensure_index(EJDB db, const char *coll, const char *path, ejdb_idx_mode_t mode) {
  if (!db || !coll || !path) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci, sz;
  JBCOLL jbc;
  JBIDX cidx = 0;
  JBL_PTR ptr = 0;
  bool unique = (mode & EJDB_IDX_UNIQUE);

  switch (mode & (EJDB_IDX_STR | EJDB_IDX_NUM)) {
    case EJDB_IDX_STR:
      break;
    case EJDB_IDX_NUM:
      break;
    default:
      return EJDB_ERROR_INVALID_INDEX_MODE;
  }
  if ((mode & EJDB_IDX_ARR) && (mode & EJDB_IDX_UNIQUE)) {
    // Array indexes cannot be unique
    return EJDB_ERROR_INVALID_INDEX_MODE;
  }

  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);
  rc = jbl_ptr_alloc(path, &ptr);
  RCGO(rc, finish);

  for (JBIDX idx = jbc->idx; idx; idx = idx->next) {
    if ((idx->mode & ~EJDB_IDX_UNIQUE) == (mode & ~EJDB_IDX_UNIQUE) && !jbl_ptr_cmp(idx->ptr, ptr)) {
      if ((idx->mode & EJDB_IDX_UNIQUE) != (mode & EJDB_IDX_UNIQUE)) {
        rc = EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE;
      }
      goto finish;
    }
  }
  cidx = calloc(1, sizeof(*cidx));
  if (!cidx) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  cidx->ptr = ptr;
  ptr = 0;

  cidx->idbf = (mode & EJDB_IDX_NUM) ? IWDB_UINT64_KEYS : 0;
  if (!(mode & EJDB_IDX_UNIQUE)) {
    cidx->idbf |= IWDB_DUP_UINT64_VALS;
  }
  rc = iwkv_new_db(db->iwkv, cidx->idbf, 0, &cidx->idb);
  RCGO(rc, finish);

  // TODO: fill index with collection items data

finish:
  if (rc) {
    if (cidx) {
      if (cidx->idb) {
        iwkv_db_destroy(&cidx->idb);
        cidx->idb = 0;
      }
      _jb_idx_destroy(cidx);
    }
  }
  if (ptr) free(ptr);
  API_COLL_UNLOCK(jbc, rci, rc);
  return rc;
}

iwrc ejdb_put(EJDB db, const char *coll, const JBL jbl, uint64_t *id) {
  if (!jbl) {
    return IW_ERROR_INVALID_ARGS;
  }
  int rci;
  JBCOLL jbc;
  if (id) *id = 0;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);

  uint64_t oid = jbc->id_seq + 1;
  IWKV_val key, val;
  key.data = &oid;
  key.size = sizeof(oid);

  rc = jbl_as_buf(jbl, &val.data, &val.size);
  RCGO(rc, finish);

  rc = iwkv_put(jbc->cdb, &key, &val, 0);
  RCGO(rc, finish);

  // TODO: Update indexes

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
  rc = jbl_from_buf_keep(&jbl, val.data, val.size);
  RCGO(rc, finish);
  *jblp = jbl;

finish:
  if (rc) {
    if (jbl) {
      jbl_destroy(&jbl); // `IWKV_val val` will be freed here
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
  IWKV_val key = {.data = &id, .size = sizeof(id)};
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, true, &jbc);
  RCRET(rc);
  rc = iwkv_del(jbc->cdb, &key, 0);
  RCGO(rc, finish);
  // TODO: Update indexes
finish:
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
  khiter_t k = kh_get(JBCOLLM, db->mcolls, coll);
  if (k != kh_end(db->mcolls)) {
    IWDB cdb;
    jbc = kh_value(db->mcolls, k);
    assert(jbc);
    rc = iwkv_db(db->iwkv, jbc->dbid, IWDB_UINT64_KEYS, &cdb);
    RCGO(rc, finish);

    // TODO: remove indexes

    rc = iwkv_db_destroy(&cdb);
    kh_del(JBCOLLM, db->mcolls, k);
    _jb_coll_destroy(jbc);
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

iwrc ejdb_open(const EJDB_OPTS *opts, EJDB *ejdbp) {
  *ejdbp = 0;
  int rci;
  iwrc rc = ejdb_init();
  RCRET(rc);
  if (!opts || !opts->kv.path || !ejdbp) {
    return IW_ERROR_INVALID_ARGS;
  }
  EJDB db = calloc(1, sizeof(*db));
  if (!db) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
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
  memcpy(&kvopts, &opts->kv, sizeof(opts->kv));
  kvopts.wal.enabled = !opts->no_wal;
  rc = iwkv_open(&kvopts, &db->iwkv);
  RCGO(rc, finish);

  db->oflags = kvopts.oflags;
  rc = _jb_db_meta_load(db);

finish:
  if (rc) {
    _jb_db_destroy(&db);
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
  iwrc rc = _jb_sync_meta(db);
  _jb_db_destroy(ejdbp);
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
    case EJDB_ERROR_INVALID_INDEX_MODE:
      return "Invalid index mode (EJDB_ERROR_INVALID_INDEX_MODE)";
    case EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE:
      return "Index exists but mismatched uniqueness constraint (EJDB_ERROR_MISMATCHED_INDEX_UNIQUENESS_MODE)";
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
