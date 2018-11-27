#include "ejdb2.h"
#include <CUnit/Basic.h>

int init_suite() {
  int rc = ejdb_init();
  return rc;
}

int clean_suite() {
  return 0;
}

static iwrc put_json(EJDB db, const char *coll, const char *json) {
  const size_t len = strlen(json);
  char buf[len + 1];
  memcpy(buf, json, len);
  buf[len] = '\0';
  for (int i = 0; buf[i]; ++i) {
    if (buf[i] == '\'') buf[i] = '"';
  }
  JBL jbl;
  uint64_t llv;
  iwrc rc = jbl_from_json(&jbl, buf);
  RCRET(rc);
  rc = ejdb_put_new(db, coll, jbl, &llv);
  jbl_destroy(&jbl);
  return rc;
}

void ejdb_test2_1() {
   EJDB_OPTS opts = {
    .kv = {
      .path = "ejdb_test2_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };

  EJDB db;
  uint64_t llv = 0, llv2;
  IWPOOL *pool = 0;
  EJDB_LIST list = 0;

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json(db, "a", "{'f':2}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = put_json(db, "a", "{'f':1}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = put_json(db, "a", "{'f':3}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = put_json(db, "a", "{'a':'foo'}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = put_json(db, "a", "{'a':'gaz'}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = put_json(db, "a", "{'a':'bar'}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list2(db, "a", "/*", 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  ejdb_list_destroy(&list);


  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
  pSuite = CU_add_suite("ejdb_test1", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (
    (NULL == CU_add_test(pSuite, "ejdb_test2_1", ejdb_test2_1))
  ) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  int ret = CU_get_error() || CU_get_number_of_failures();
  CU_cleanup_registry();
  return ret;
}




