#include "ejdb_test.h"
#include <CUnit/Basic.h>

#include <sys/types.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#undef CHECK
#define CHECK(rc_)               \
  if (rc_)                     \
  {                            \
    iwlog_ecode_error3(rc_); \
    return 1;                \
  }

#undef print_message
#define print_message(fmt, ...)                                          \
  {                                                                    \
    iwlog_info("Thread[%u] | " fmt, (unsigned int) 0, ## __VA_ARGS__); \
  }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IWKV_NO_TRIM_ON_CLOSE
iwrc get_ejdb(EJDB *db) {
  EJDB_OPTS opts = {
    .kv                         = {
      .path                     = "ejdb_test5.db",
      .oflags                   = IWKV_NO_TRIM_ON_CLOSE | IWKV_TRUNC,
      .wal                      = {
        .savepoint_timeout_sec  = 5,
        .checkpoint_timeout_sec = 10,
        .wal_buffer_sz          = 4096,
        .checkpoint_buffer_sz   = 1024 * 1024
      },
    },
    .no_wal                     = 0,
    .sort_buffer_sz             = 1024 * 1024, // 1M
    .document_buffer_sz         = 16 * 1024    // 16K
  };

  print_message("step-1 ejdb_init");
  iwrc rc = ejdb_init();
  CHECK(rc);

  print_message("step-2 ejdb_open");
  rc = ejdb_open(&opts, db);
  CHECK(rc);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
iwrc show_db_meta(EJDB db) {
  JBL jbl = 0;   // Json document
  iwrc rc = ejdb_get_meta(db, &jbl);
  RCGO(rc, finish);

  fprintf(stderr, "\n");
  rc = jbl_as_json(jbl, jbl_fstream_json_printer, stderr, JBL_PRINT_CODEPOINTS);
  RCGO(rc, finish);
  fprintf(stderr, "\n");
finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
iwrc add_record_large(EJDB db, char *collection, int total) {
  JBL jbl = 0;   // Json document
  int64_t id;    // Document id placeholder
  char json[1024];
  for (int ii = 0; ii < total; ii++) {
    memset(json, 0, sizeof(json));
    sprintf(json,
            "{\"name\":\"Darko\", \"time_\":%d, \"largeValue\": \"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\"}",
            ii);
    iwrc rc = jbl_from_json(&jbl, json);
    RCGO(rc, finish);
    rc = ejdb_put_new(db, collection, jbl, &id);
    RCGO(rc, finish);
    if (jbl) {
      jbl_destroy(&jbl);
      jbl = 0;
    }
  }

finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
  return 0;
}

// delete function /////////////////////////////////////////////////////////////////////////////////////////////////////
iwrc delete_record_large(EJDB db, char *collection, int64_t to, int limit) {
  JQL q = 0;

  char deleteSql[128];
  memset(deleteSql, 0, sizeof(deleteSql));
  sprintf(deleteSql, "(/[time_ <= %lld ]) | del | asc /time_ limit %d count", (long long) to, limit);
  print_message("sql=%s\n", deleteSql);
  iwrc rc = jql_create(&q, collection, deleteSql);
  RCGO(rc, finish);

  EJDB_EXEC ux = {
    .db = db,
    .q  = q,
  };

  rc = ejdb_exec(&ux);
  RCGO(rc, finish);
  print_message("delete successful");
finish:
  if (q) {
    jql_destroy(&q);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
  EJDB db = 0;
  iwrc rc = get_ejdb(&db);
  CHECK(rc);

  char *collection = "collection";

  int addTotal = 20000;
  add_record_large(db, collection, addTotal);

  int64_t time = addTotal - 10000;
  int limit = 10000;
  show_db_meta(db);
  delete_record_large(db, collection, time, limit);
  show_db_meta(db);
  ejdb_close(&db);
  return 0;
}
