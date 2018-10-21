#include "ejdb2.h"
#include "jql.h"
#include "jql_internal.h"
#include <iowow/iwkv.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include "khash.h"

#define METADB_ID 1

typedef struct _JBCOLL {
  const char *name;
  EJDB jb;
  IWDB db;
  JBL meta;
  atomic_uint_fast64_t id_seq;
} *JBCOLL;

KHASH_MAP_INIT_STR(JBCOLLM, JBCOLL)

struct _EJDB {
  IWKV iwkv;
  IWDB metadb;
  khash_t(JBCOLLM) *mcolls;
  pthread_rwlock_t rwl;       /**< Main RWL */
};


static iwrc _jb_load_coll(EJDB db, const char *name) {
  // todo
  return 0;
}

static iwrc _jb_load_meta(EJDB db) {
  // todo
  return 0;
}

static iwrc _jb_sync_meta(EJDB db) {
  // todo
  return 0;
}

//----------------------- Public API

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
    if (db->metadb) {
      iwkv_db_destroy(&db->metadb);
    }
    if (db->iwkv) {
      iwkv_close(&db->iwkv);
    }
    pthread_rwlock_destroy(&db->rwl);
    free(db);
  }
  return rc;
}

iwrc ejdb_close(EJDB *ejdbp) {
  if (!ejdbp || !*ejdbp) {
    return IW_ERROR_INVALID_ARGS;
  }
  EJDB db = *ejdbp;
  iwrc rc = _jb_sync_meta(db);
  if (db->metadb) {
    iwkv_db_destroy(&db->metadb);
  }
  if (db->iwkv) {
    iwkv_close(&db->iwkv);
  }
  pthread_rwlock_destroy(&db->rwl);
  free(db);
  *ejdbp = 0;
  return rc;
}

static const char *_ejdb_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _EJDB_ERROR_START && ecode < _EJDB_ERROR_END)) {
    return 0;
  }
  // todo
  return 0;
}

iwrc ejdb_init() {
  static int _jb_initialized = 0;
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
