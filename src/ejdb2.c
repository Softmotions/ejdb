#include "ejdb2.h"
#include "jql.h"
#include "jql_internal.h"
#include <iowow/iwkv.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <assert.h>
#include "khash.h"

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
  atomic_uint_fast64_t id_seq;
} *JBCOLL;

KHASH_MAP_INIT_STR(JBCOLLM, JBCOLL)

struct _EJDB {
  volatile int open;
  IWKV iwkv;
  IWDB metadb;
  khash_t(JBCOLLM) *mcolls;
  pthread_rwlock_t rwl;       /**< Main RWL */
};


// ---------------------------------------------------------------------------


static iwrc _jb_sync_meta(EJDB db) {
  // todo
  return 0;
}

IW_INLINE iwrc _jb_init_coll_meta(JBCOLL coll) {
  JBL jbv;
  JBL jbm = coll->meta;
  iwrc rc = jbl_at(jbm, "/name", &jbv);
  RCRET(rc);
  coll->name = jbl_get_str(jbv);
  if (!coll->name) {
    return EJDB_ERROR_INVALID_COLLECTION_META;
  }
  rc = jbl_at(jbm, "/id", &jbv);
  RCRET(rc);
  coll->dbid = jbl_get_i32(jbv);
  if (!coll->dbid) {
    return EJDB_ERROR_INVALID_COLLECTION_META;
  }
  return rc;
}

static iwrc _jb_init_coll(JBCOLL coll, IWKV_val *meta) {
  int rci = pthread_rwlock_init(&coll->rwl, 0);
  if (rci) {
    return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
  }
  iwrc rc = jbl_from_buf_keep(&coll->meta, meta->data, meta->size);
  RCGO(rc, finish);
  rc = _jb_init_coll_meta(coll);
  RCGO(rc, finish);

  khiter_t k = kh_put(JBCOLLM, coll->db->mcolls, coll->name, &rci);
  if (rci != -1) {
    kh_value(coll->db->mcolls, k) = coll;
  } else {
    RCGO(IW_ERROR_FAIL, finish);
  }
finish:
  if (rc) {
    pthread_rwlock_destroy(&coll->rwl);
  }
  return rc;
}

static void _jb_dispose_coll(JBCOLL coll) {
  if (coll->meta) {
    jbl_destroy(&coll->meta);
  }
  pthread_rwlock_destroy(&coll->rwl);
  free(coll);
}

static iwrc _jb_load_meta(EJDB db) {
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
    if (!strncmp(key.data, KEY_PREFIX_COLLMETA, key.size)) {
      JBCOLL coll = calloc(1, sizeof(*coll));
      coll->db = db;
      rc = _jb_init_coll(coll, &val);
      if (rc) {
        free(coll);
        iwkv_val_dispose(&val);
      }
    } else {
      iwkv_val_dispose(&val);
    }
    iwkv_val_dispose(&key);
  }
  if (rc == IWKV_ERROR_NOTFOUND) {
    rc = 0;
  } else {
    goto finish;
  }

finish:
  iwkv_cursor_close(&cur);

  return rc;
}

static void _jb_dispose_db(EJDB *dbp) {
  EJDB db = *dbp;
  if (db->mcolls) {
    for (khiter_t k = kh_begin(db->mcolls); k != kh_end(db->mcolls); ++k) {
      JBCOLL coll = kh_val(db->mcolls, k);
      _jb_dispose_coll(coll);
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

iwrc _jb_acquire_collection_keeplock(EJDB db, const char *collname, bool wl, JBCOLL *collp) {
  int rci;
  iwrc rc = 0;
  API_RLOCK(db, rci);
  JBCOLL coll;
  khiter_t k = kh_get(JBCOLLM, db->mcolls, collname);
  if (k != kh_end(db->mcolls)) {
    coll = kh_value(db->mcolls, k);
    assert(coll);
    if (wl) {
      rci = pthread_rwlock_wrlock(&coll->rwl);
    } else {
      rci = pthread_rwlock_rdlock(&coll->rwl);
    }
    if (rci) {
      pthread_rwlock_unlock(&db->rwl);
      return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    }
    *collp = coll;
  } else {
    pthread_rwlock_unlock(&db->rwl);
    API_WLOCK(db, rci);
    k = kh_get(JBCOLLM, db->mcolls, collname);
    if (k != kh_end(db->mcolls)) {
      coll = kh_value(db->mcolls, k);
      assert(coll);
      rci = pthread_rwlock_rdlock(&coll->rwl);
      if (rci) {
        pthread_rwlock_unlock(&db->rwl);
        return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
      }
      *collp = coll;
    } else {

      // todo

    }
  }
  return 0;
}

//----------------------- Public API

iwrc ejdb_put(const char *coll, const JBL doc, uint64_t *id) {
  // todo
  return 0;
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

  IWKV_OPTS kvopts;
  memcpy(&kvopts, &opts->kv, sizeof(opts->kv));
  kvopts.wal.enabled = !opts->no_wal;
  rc = iwkv_open(&kvopts, &db->iwkv);
  RCGO(rc, finish);

  rc = iwkv_db(db->iwkv, METADB_ID, 0, &db->metadb);
  RCGO(rc, finish);

  rc = _jb_load_meta(db);

finish:
  if (rc) {
    _jb_dispose_db(&db);
  } else {
    db->open = true;
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
  _jb_dispose_db(ejdbp);
  return rc;
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
