#include "ejdb_test.h"
#include "iwtests.h"

#include <sys/types.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#undef CHECK
#define CHECK(rc_)                 \
        if (rc_)                   \
        {                          \
          iwlog_ecode_error3(rc_); \
          return rc_;              \
        }

#undef print_message
#define print_message(fmt, ...)                                              \
        {                                                                    \
          iwlog_info("Thread[%u] | " fmt, (unsigned int) 0, ## __VA_ARGS__); \
        }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IWKV_NO_TRIM_ON_CLOSE
static iwrc get_ejdb(struct ejdb **db) {
  struct ejdb_opts opts = {
    .kv = {
      .path = "ejdb_test5.db",
      .oflags = IWKV_NO_TRIM_ON_CLOSE | IWKV_TRUNC,
      .wal = {
        .savepoint_timeout_sec = 5,
        .checkpoint_timeout_sec = 10,
        .wal_buffer_sz = 4096,
        .checkpoint_buffer_sz = 1024 * 1024
      },
    },
    .no_wal = 0,
    .sort_buffer_sz = 1024 * 1024,             // 1M
    .document_buffer_sz = 16 * 1024            // 16K
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
static iwrc show_db_meta(struct ejdb *db) {
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
  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static iwrc add_record_large(struct ejdb *db, char *collection, int total) {
  JBL jbl = 0;   // Json document
  int64_t id;    // Document id placeholder
  char json[1024];
  iwrc rc = 0;
  for (int ii = 0; ii < total; ii++) {
    memset(json, 0, sizeof(json));
    sprintf(json,
            "{\"name\":\"Darko\", \"time_\":%d, \"largeValue\": \"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000\"}",
            ii);
    rc = jbl_from_json(&jbl, json);
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
  return rc;
}

// delete function /////////////////////////////////////////////////////////////////////////////////////////////////////
static iwrc delete_record_large(struct ejdb *db, char *collection, int64_t to, int limit) {
  struct jql *q = 0;

  char deleteSql[128];
  memset(deleteSql, 0, sizeof(deleteSql));
  sprintf(deleteSql, "(/[time_ <= %lld ]) | del | asc /time_ limit %d count", (long long) to, limit);
  print_message("sql=%s\n", deleteSql);
  iwrc rc = jql_create(&q, collection, deleteSql);
  RCGO(rc, finish);

  struct ejdb_exec ux = {
    .db = db,
    .q = q,
  };

  rc = ejdb_exec(&ux);
  RCGO(rc, finish);
  print_message("delete successful");
finish:
  if (q) {
    jql_destroy(&q);
  }
  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void ejdb_test5_1(void) {
  struct ejdb *db = 0;
  iwrc rc = get_ejdb(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(db);

  char *collection = "collection";

  int addTotal = 20000;
  rc = add_record_large(db, collection, addTotal);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  int64_t time = addTotal - 10000;
  int limit = 10000;
  rc = show_db_meta(db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = delete_record_large(db, collection, time, limit);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = show_db_meta(db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
}

int main(int argc, char *argv[]) {
  static const struct iw_test_case cases[] = {
    IW_TEST_CASE(ejdb_test5_1),
  };
  return iw_test_run(argc, argv, IW_TEST_SUITE("ejdb_test5", cases, 0, 0));
}
