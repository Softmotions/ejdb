#include "ejdb2.h"
#include "ejdb_test.h"
#include <iowow/iwxstr.h>
#include "iwtests.h"

static int init_suite(void) {
  iwrc rc = ejdb_init();
  return rc;
}

static void clean_suite(void) {
}

static void ejdb_test1_3(void) {
  struct ejdb_opts opts = {
    .kv = {
      .path = "ejdb_test1_3.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };
  struct ejdb *db;
  JBL jbl;
  int64_t id = 0;

  iwrc rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json2(db, "c1", "{'foo':'bar'}", &id);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_TRUE_FATAL(id > 0);

  rc = patch_json(db, "c1", "[ { 'op': 'add', 'path': '/baz', 'value': 'qux' } ]", id);
  if (rc) {
    iwlog_ecode_error3(rc);
  }
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  // Now check the result
  rc = ejdb_get(db, "c1", id, &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  IWXSTR *xstr = iwxstr_new();
  IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, 0);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(iwxstr_ptr(xstr), "{\"foo\":\"bar\",\"baz\":\"qux\"}");

  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(xstr);
  jbl_destroy(&jbl);
}

static void ejdb_test1_2(void) {
  struct ejdb_opts opts = {
    .kv = {
      .path = "ejdb_test1_2.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };
  struct ejdb *db;
  JBL jbl, at, meta;
  int64_t llv = 0, llv2;
  iwrc rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_from_json(&jbl, "{\"foo\":22}");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_put_new(db, "foocoll", jbl, &llv);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&jbl);
  IW_TEST_ASSERT_TRUE(llv > 0);

  rc = ejdb_get(db, "foocoll", llv, &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_at(jbl, "/foo", &at);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  llv2 = jbl_get_i64(at);
  IW_TEST_ASSERT_EQUAL(llv2, 22);
  jbl_destroy(&at);
  jbl_destroy(&jbl);

  rc = ejdb_del(db, "foocoll", llv);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_get(db, "foocoll", llv, &jbl);
  IW_TEST_ASSERT_EQUAL(rc, IWKV_ERROR_NOTFOUND);
  IW_TEST_ASSERT_PTR_NULL(jbl);

  rc = jbl_from_json(&jbl, "{\"foo\":22}");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_put_new(db, "foocoll", jbl, &llv);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&jbl);
  IW_TEST_ASSERT_TRUE(llv > 0);

  // Ensure indexes
  rc = ejdb_ensure_index(db, "col1", "/foo/bar", EJDB_IDX_I64 | EJDB_IDX_UNIQUE);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "col1", "/foo/baz", EJDB_IDX_STR);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "col1", "/foo/gaz", EJDB_IDX_STR | EJDB_IDX_UNIQUE);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_get_meta(db, &meta);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  //  fprintf(stderr, "\n");
  //  rc = jbl_as_json(meta, jbl_fstream_json_printer, stderr, JBL_PRINT_PRETTY);
  //  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  //  fprintf(stderr, "\n");

  //    "version": "2.0.0",
  //    "file": "ejdb_test1_2.db",
  //    "size": 8192,
  //    "collections": [
  //      {
  //        "name": "col1",
  //        "dbid": 3,
  //        "indexes": [
  //          {
  //            "ptr": "/foo/bar",
  //            "mode": 9,
  //            "idbf": 2,
  //            "dbid": 4,
  //            "auxdbid": 0
  //          },
  //          {
  //            "ptr": "/foo/baz",
  //            "mode": 4,
  //            "idbf": 8,
  //            "dbid": 5,
  //            "auxdbid": 0
  //          }
  //        ]
  //      },
  //      {
  //        "name": "foocoll",
  //        "dbid": 2,
  //        "indexes": []
  //      }
  //    ]

  rc = jbl_at(meta, "/collections/0/name", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "col1");
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/0/indexes/0/ptr", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "/foo/gaz");
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/1/name", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "foocoll");
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/0/indexes/1/ptr", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "/foo/baz");
  jbl_destroy(&jbl);

  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&meta);

  // Now reopen db and check indexes
  opts.kv.oflags &= ~IWKV_TRUNC;
  rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_get_meta(db, &meta);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_at(meta, "/collections/0/name", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "col1");
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/0/indexes/0/ptr", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "/foo/gaz");
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/1/name", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "foocoll");
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/1/rnum", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_EQUAL(jbl_get_i64(jbl), 1);
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/0/indexes/1/ptr", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "/foo/baz");
  jbl_destroy(&jbl);
  jbl_destroy(&meta);

  rc = ejdb_remove_index(db, "col1", "/foo/baz", EJDB_IDX_STR);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_remove_index(db, "col1", "/foo/gaz", EJDB_IDX_STR);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_get_meta(db, &meta);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_at(meta, "/collections/1/indexes/0/ptr", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATH_NOTFOUND);

  rc = jbl_at(meta, "/collections/2/indexes/0/ptr", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATH_NOTFOUND);

  rc = jbl_at(meta, "/collections/0/indexes/0/ptr", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "/foo/bar");
  jbl_destroy(&jbl);

  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&meta);
}

static void ejdb_test1_1(void) {
  struct ejdb_opts opts = {
    .kv = {
      .path = "ejdb_test1_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };
  struct ejdb *db;
  JBL meta, jbl;
  iwrc rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_ensure_collection(db, "foo");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  // Meta: {
  //  "version": "2.0.0",
  //  "file": "ejdb_test1_1.db",
  //  "size": 8192,
  //  "collections": [
  //    {
  //      "name": "foo",
  //      "dbid": 2
  //    }
  //  ]
  //}
  rc = ejdb_get_meta(db, &meta);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_at(meta, "/file", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "ejdb_test1_1.db");
  jbl_destroy(&jbl);

  rc = jbl_at(meta, "/collections/0/name", &jbl);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  IW_TEST_ASSERT_STR_EQUAL(jbl_get_str(jbl), "foo");
  jbl_destroy(&jbl);
  jbl_destroy(&meta);

  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  // Now reopen database then load collection
  opts.kv.oflags &= ~IWKV_TRUNC;
  rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_remove_collection(db, "foo");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_get_meta(db, &meta);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_at(meta, "/collections/0/name", &jbl); // No collections
  IW_TEST_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATH_NOTFOUND);
  jbl_destroy(&meta);

  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
}

int main(int argc, char *argv[]) {
  static const struct iw_test_case cases[] = {
    IW_TEST_CASE(ejdb_test1_1),
    IW_TEST_CASE(ejdb_test1_2),
    IW_TEST_CASE(ejdb_test1_3),
  };
  return iw_test_run(argc, argv, IW_TEST_SUITE("ejdb_test1", cases, init_suite, clean_suite));
}
