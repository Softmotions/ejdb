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

#define API_COLL_UNLOCK(coll_, rci_, rc_)                                     \
  do {                                                                    \
    rci_ = pthread_rwlock_unlock(&(coll_)->rwl);                            \
    if (rci_) IWRC(iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci_), rc_);  \
    API_UNLOCK((coll_)->db, rci_, rc_);                                   \
  } while(0)


typedef struct _JBCOLL {
  uint32_t dbid;            /**< IWKV database ID */
  const char *name;         /**< Collection name */
  EJDB db;                  /**< Main database reference */
  JBL meta;                 /**< Collection meta object */
  pthread_rwlock_t rwl;
  volatile uint64_t id_seq;
} *JBCOLL;

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

static iwrc _jb_coll_load_meta(JBCOLL jbc) {
  JBL jbv;
  IWDB cdb;
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
  rc = iwkv_db(jbc->db->iwkv, jbc->dbid, IWDB_UINT64_KEYS, &cdb);
  RCRET(rc);

  rc = iwkv_cursor_open(cdb, &cur, IWKV_CURSOR_BEFORE_FIRST, 0);
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

static void _jb_coll_destroy(JBCOLL jbc) {
  if (jbc->meta) {
    jbl_destroy(&jbc->meta);
  }
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
  rc = _jb_coll_load_meta(jbc);
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

  // TODO

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

static iwrc _jb_coll_acquire_keeplock(EJDB db, const char *collname, bool wl, JBCOLL *collp) {
  int rci;
  iwrc rc = 0;
  *collp = 0;
  API_RLOCK(db, rci);
  JBCOLL jbc;
  khiter_t k = kh_get(JBCOLLM, db->mcolls, collname);
  if (k != kh_end(db->mcolls)) {
    jbc = kh_value(db->mcolls, k);
    assert(jbc);
    rci = wl ? pthread_rwlock_wrlock(&jbc->rwl) : pthread_rwlock_rdlock(&jbc->rwl);
    if (rci) {
      rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
      goto finish;
    }
    *collp = jbc;
  } else {
    pthread_rwlock_unlock(&db->rwl); // relock
    if (db->oflags & IWKV_RDONLY) {
      return IW_ERROR_NOT_EXISTS;
    }
    API_WLOCK(db, rci);
    k = kh_get(JBCOLLM, db->mcolls, collname);
    if (k != kh_end(db->mcolls)) {
      jbc = kh_value(db->mcolls, k);
      assert(jbc);
      rci = pthread_rwlock_rdlock(&jbc->rwl);
      if (rci) {
        rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
        goto finish;
      }
      *collp = jbc;
    } else {
      JBL meta = 0;
      IWDB cdb = 0;
      uint32_t dbid = 0;
      char keybuf[JBNUMBUF_SIZE + 4];
      IWKV_val metakey, metaval;

      rc = iwkv_new_db(db->iwkv, IWDB_UINT64_KEYS, &dbid, &cdb);
      RCGO(rc, cfinish);
      JBCOLL jbc = calloc(1, sizeof(*jbc));
      if (!jbc) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto cfinish;
      }
      rc = jbl_create_empty_object(&meta);
      RCGO(rc, cfinish);
      if (!binn_object_set_str(&meta->bn, "name", collname)) {
        rc = JBL_ERROR_CREATION;
        goto cfinish;
      }
      if (!binn_object_set_uint32(&meta->bn, "id", dbid)) {
        rc = JBL_ERROR_CREATION;
        goto cfinish;
      }
      rc = jbl_as_buf(meta, &metaval.data, &metaval.size);
      RCGO(rc, cfinish);

      metakey.size = snprintf(keybuf, sizeof(keybuf), KEY_PREFIX_COLLMETA "%u",  dbid);
      if (metakey.size >= sizeof(keybuf)) {
        rc = IW_ERROR_OVERFLOW;
        goto cfinish;
      }
      metakey.data = keybuf;
      rc = iwkv_put(db->metadb, &metakey, &metaval, IWKV_SYNC);
      RCGO(rc, cfinish);

      jbc->db = db;
      jbc->meta = meta;
      rc = _jb_coll_init(jbc, 0);
      if (rc) {
        iwkv_del(db->metadb, &metakey, IWKV_SYNC);
        goto cfinish;
      }
cfinish:
      if (rc) {
        if (meta) jbl_destroy(&meta);
        if (cdb) iwkv_db_destroy(&cdb);
        if (jbc) {
          jbc->meta = 0;
          _jb_coll_destroy(jbc);
        }
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

iwrc ejdb_put(EJDB db, const char *coll, const JBL doc, uint64_t *id) {
  // todo
  return 0;
}

iwrc ejdb_ensure_collection(EJDB db, const char *coll) {
  int rci;
  JBCOLL jbc;
  iwrc rc = _jb_coll_acquire_keeplock(db, coll, false, &jbc);
  RCRET(rc);
  API_UNLOCK(db, rci, rc);
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
  clist = binn_list();
  if (clist) {
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
      return "Invalid collection metadata";
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
