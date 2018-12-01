#include "ejdb_test.h"
#include <CUnit/Basic.h>

int init_suite() {
  int rc = ejdb_init();
  return rc;
}

int clean_suite() {
  return 0;
}

void ejdb_test3_1() {
  EJDB_OPTS opts = {
    .kv = {
      .path = "ejdb_test3_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };

  EJDB db;
  char dbuf[1024];
  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "c1", "/f", EJDB_IDX_UNIQUE | EJDB_IDX_I64);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  for (int i = 1; i <= 10; ++i) {
    snprintf(dbuf, sizeof(dbuf), "{\"f\":%d, \"n\":%d}", i, i);
    rc = put_json(db, "c1", dbuf);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 1) { // Check unique index constraint violation
      rc = put_json(db, "c1", dbuf);
      CU_ASSERT_EQUAL(rc, EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED);
    }
    rc = put_json(db, "c2", dbuf);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
  }
  rc = ejdb_ensure_index(db, "c2", "/f", EJDB_IDX_UNIQUE | EJDB_IDX_I64);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // TODO:

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
    (NULL == CU_add_test(pSuite, "ejdb_test3_1", ejdb_test3_1))
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
