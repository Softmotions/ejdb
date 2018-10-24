#include "ejdb2.h"
#include <CUnit/Basic.h>

int init_suite(void) {
  int rc = ejdb_init();
  return rc;
}

int clean_suite(void) {
  return 0;
}

void ejdb_test1_1() {
  EJDB_OPTS opts = {
    .kv = {
      .path = "ejdb_test1_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };
  EJDB db;
  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_ensure_collection(db, "foo");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // Now reopen databse then load collection
  opts.kv.oflags &= ~IWKV_TRUNC;
  rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_remove_collection(db, "foo");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
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
    (NULL == CU_add_test(pSuite, "ejdb_test1_1", ejdb_test1_1))
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
