#include "ejdb_test.h"
#include <CUnit/Basic.h>
#include <stdlib.h>

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
  EJDB_LIST list = 0;
  IWXSTR *log = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(log);
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);


  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "c1", "/f/b", EJDB_IDX_UNIQUE | EJDB_IDX_I64);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  for (int i = 1; i <= 10; ++i) {
    snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":%d},\"n\":%d}", i, i);
    rc = put_json(db, "c1", dbuf);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 1) { // Check unique index constraint violation
      rc = put_json(db, "c1", dbuf);
      CU_ASSERT_EQUAL(rc, EJDB_ERROR_UNIQUE_INDEX_CONSTRAINT_VIOLATED);
    }
    rc = put_json(db, "c2", dbuf);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
  }
  rc = ejdb_ensure_index(db, "c2", "/f/b", EJDB_IDX_UNIQUE | EJDB_IDX_I64);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list3(db, "c1", "/f/[b = 1]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(
    strstr(iwxstr_ptr(log), "[INDEX] MATCHED  UNIQUE|I64|10 /f/b EXPR1: 'b = 1' INIT: IWKV_CURSOR_EQ"));
  CU_ASSERT_PTR_NOT_NULL(
    strstr(iwxstr_ptr(log), "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b = 1' INIT: IWKV_CURSOR_EQ"));
  int i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":1},\"n\":1}");
  }
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 1]
  rc = ejdb_list3(db, "c1", "/f/[b > 1]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "MATCHED  UNIQUE|I64|10 /f/b EXPR1: 'b > 1' INIT: IWKV_CURSOR_GE"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 1' INIT: IWKV_CURSOR_GE"));

  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":2},\"n\":2}");
    } else if (i == 8) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":10},\"n\":10}");
    }
  }
  CU_ASSERT_EQUAL(i, 9);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b >= 2]
  rc = ejdb_list3(db, "c1", "/f/[b >= 3]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":3},\"n\":3}");
    } else if (i == 8) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":10},\"n\":10}");
    }
  }
  CU_ASSERT_EQUAL(i, 8);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b < 9]
  rc = ejdb_list3(db, "c1", "/f/[b < 9]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":8},\"n\":8}");
    } else if (i == 7) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":1},\"n\":1}");
    }
  }
  CU_ASSERT_EQUAL(i, 8);
  ejdb_list_destroy(&list);

  // Q: /f/[b < 11]
  rc = ejdb_list3(db, "c1", "/f/[b < 11]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":10},\"n\":10}");
    } else if (i == 9) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":1},\"n\":1}");
    }
  }
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b < 11 and b >= 4]
  rc = ejdb_list3(db, "c1", "/f/[b < 11 and b >= 4]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b >= 4' EXPR2: 'b < 11' INIT: IWKV_CURSOR_GE"));
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":4},\"n\":4}");
    } else if (i == 6) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":10},\"n\":10}");
    }
  }
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b < 11 and b > 0 or b = 0]
  rc = ejdb_list3(db, "c1", "/f/[b < 11 and b > 0 or b = 0]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 1 and b < 2 and b = 2 and b < 3]
  rc = ejdb_list3(db, "c1", "/f/[b > 1 and b < 2 and b = 2 and b < 3]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b = 2' INIT: IWKV_CURSOR_EQ"));
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 1 and b < 3 and b <= 4]
  rc = ejdb_list3(db, "c1", "/f/[b > 1 and b < 3 and b <= 4]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 1' EXPR2: 'b < 3' INIT: IWKV_CURSOR_GE"));
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":2},\"n\":2}");
  }
  CU_ASSERT_EQUAL(i, 1);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 1] and /f/[b < 3]
  rc = ejdb_list3(db, "c1", "/f/[b > 1] and /f/[b < 3]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] MATCHED  UNIQUE|I64|10 /f/b EXPR1: 'b > 1' INIT: IWKV_CURSOR_GE"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] MATCHED  UNIQUE|I64|10 /f/b EXPR1: 'b < 3' INIT: IWKV_CURSOR_GE"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 1' INIT: IWKV_CURSOR_GE"));
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b != 1] and /f/[b < 3]
  rc = ejdb_list3(db, "c1", "/f/[b != 1] and /f/[b < 3]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b < 3' INIT: IWKV_CURSOR_GE"));
  //fprintf(stderr, "\n%s\n", iwxstr_ptr(log));
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":2},\"n\":2}");
  }
  ejdb_list_destroy(&list);
  iwxstr_clear(log);


  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(log);
  iwxstr_destroy(xstr);
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
