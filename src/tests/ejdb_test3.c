#include "ejdb_test.h"
#include <CUnit/Basic.h>

int init_suite() {
  int rc = ejdb_init();
  return rc;
}

int clean_suite() {
  return 0;
}

static void ejdb_test3_1() {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
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

  CU_ASSERT_PTR_NOT_NULL_FATAL(strstr(iwxstr_ptr(log), "[INDEX] MATCHED  UNIQUE|I64|10 /f/b EXPR1: 'b = 1' "
                                      "INIT: IWKV_CURSOR_EQ"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b = 1' "
                                "INIT: IWKV_CURSOR_EQ"));
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
  //if (rc) iwlog_ecode_error3(rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "MATCHED  UNIQUE|I64|10 /f/b EXPR1: 'b > 1' "
                                "INIT: IWKV_CURSOR_GE"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 1' "
                                "INIT: IWKV_CURSOR_GE"));

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
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b >= 4' EXPR2: 'b < 11' "
                                "INIT: IWKV_CURSOR_GE"));
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
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b = 2' "
                                "INIT: IWKV_CURSOR_EQ"));
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 1 and b < 3 and b <= 4]
  rc = ejdb_list3(db, "c1", "/f/[b > 1 and b < 3 and b <= 4]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 1' EXPR2: 'b < 3' "
                                "INIT: IWKV_CURSOR_GE"));
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

  // Q: /f/[b > 1 or b != 1] and /f/[b < 3]
  rc = ejdb_list3(db, "c1", "/f/[b > 1 or b != 1] and /f/[b < 3]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b < 3' "
                                "INIT: IWKV_CURSOR_GE"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[COLLECTOR] PLAIN"));
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":2},\"n\":2}");
  }
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b in [2,1112,4,6]]
  rc = ejdb_list3(db, "c1", "/f/[b in [2,1112,4,6]]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b in [2,1112,4,6]' "
                                "INIT: IWKV_CURSOR_EQ"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[COLLECTOR] PLAIN"));
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":2},\"n\":2}");
    } else if (i == 1) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":4},\"n\":4}");
    } else if (i == 2) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":6},\"n\":6}");
    }
  }
  CU_ASSERT_EQUAL(i, 3);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 1] | asc /f/b
  rc = ejdb_list3(db, "c1", "/f/[b > 1] | asc /f/b", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 1' "
                                "INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_PREV ORDERBY"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[COLLECTOR] PLAIN"));
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

  // Q: /f/[b > 1] | desc /f/b
  rc = ejdb_list3(db, "c1", "/f/[b > 1] | desc /f/b", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 1' "
                                "INIT: IWKV_CURSOR_GE"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[COLLECTOR] SORTER"));
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":10},\"n\":10}");
    } else if (i == 8) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":2},\"n\":2}");
    }
  }
  CU_ASSERT_EQUAL(i, 9);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 2 and b < 10] | asc /f/b
  rc = ejdb_list3(db, "c1", "/f/[b > 2 and b < 10] | asc /f/b", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b > 2' EXPR2: 'b < 10' "
                                "INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_PREV ORDERBY"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[COLLECTOR] PLAIN"));
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":3},\"n\":3}");
    } else if (i == 6) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":9},\"n\":9}");
    }
  }
  CU_ASSERT_EQUAL(i, 7);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b > 2 and b < 10] | desc /f/b
  rc = ejdb_list3(db, "c1", "/f/[b > 2 and b < 10] | desc /f/b", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] SELECTED UNIQUE|I64|10 /f/b EXPR1: 'b < 10' EXPR2: 'b > 2' "
                                "INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_NEXT ORDERBY"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[COLLECTOR] PLAIN"));
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":9},\"n\":9}");
    } else if (i == 6) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":3},\"n\":3}");
    }
  }
  CU_ASSERT_EQUAL(i, 7);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(log);
  iwxstr_destroy(xstr);
}

static void ejdb_test3_2() {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_2.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };

  EJDB db;
  char dbuf[1024];
  EJDB_LIST list = 0;
  int i = 0;
  IWXSTR *log = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(log);
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "a1", "/f/b", EJDB_IDX_I64);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // Data:
  //
  //  {"f":{"b":16777215},"n":9}
  //  {"f":{"b":16777215},"n":7}
  //  {"f":{"b":16777215},"n":5}
  //  {"f":{"b":16777215},"n":3}
  //  {"f":{"b":16777215},"n":1}
  //  {"f":{"b":127},"n":10}
  //  {"f":{"b":127},"n":8}
  //  {"f":{"b":127},"n":6}
  //  {"f":{"b":127},"n":4}
  //  {"f":{"b":127},"n":2}
  //  {"f":{"b":126},"n":12}
  //  {"f":{"b":126},"n":11}
  //

  for (i = 1; i <= 10; ++i) {
    int v = (i % 2) ? 0xffffff : 127;
    //fprintf(stderr, "\n{\"f\":{\"b\":%d},\"n\":%d}", v, i);
    snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":%d},\"n\":%d}", v, i);
    rc = put_json(db, "a1", dbuf);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
  }
  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":%d},\"n\":%d}", 126, 11);
  rc = put_json(db, "a1", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":%d},\"n\":%d}", 126, 12);
  rc = put_json(db, "a1", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // GT
  rc = ejdb_list3(db, "a1", "/f/[b > 127]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] SELECTED I64|12 /f/b EXPR1: 'b > 127' "
                                "INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_PREV"));

  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    if (i == 1) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":16777215},\"n\":1}");
    } else if (i == 9) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":16777215},\"n\":9}");
    }
  }
  CU_ASSERT_EQUAL(i, 6);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // LT
  rc = ejdb_list3(db, "a1", "/f/[b < 127]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "SELECTED I64|12 /f/b EXPR1: 'b < 127' "
                                "INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_NEXT"));

  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 1) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":126},\"n\":12}");
    } else if (i == 2) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":126},\"n\":11}");
    }
  }
  CU_ASSERT_EQUAL(i, 3);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // LT2
  rc = ejdb_list3(db, "a1", "/f/[b < 16777216]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    if (i == 1) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":16777215},\"n\":9}");
    } else if (i == 12) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":126},\"n\":11}");
    }
  }
  CU_ASSERT_EQUAL(i, 13);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // EQ
  rc = ejdb_list3(db, "a1", "/f/[b = 127]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "SELECTED I64|12 /f/b EXPR1: 'b = 127' "
                                "INIT: IWKV_CURSOR_EQ"));

  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    if (i == 1) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":127},\"n\":2}");
    } else if (i == 5) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":127},\"n\":10}");
    }
  }
  CU_ASSERT_EQUAL(i, 6);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // IN
  rc = ejdb_list3(db, "a1", "/f/[b in [333, 16777215, 127, 16777216]]", 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] SELECTED I64|12 /f/b "
                                "EXPR1: 'b in [333,16777215,127,16777216]' "
                                "INIT: IWKV_CURSOR_EQ"));
  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    if (i == 1) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":127},\"n\":2}");
    } else if (i == 5) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":127},\"n\":10}");
    } else if (i == 6) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":16777215},\"n\":1}");
    } else if (i == 10) {
      rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
      CU_ASSERT_EQUAL_FATAL(rc, 0);
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":16777215},\"n\":9}");
    }
  }
  CU_ASSERT_EQUAL(i, 11);

  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(log);
  iwxstr_destroy(xstr);
}

static void ejdb_test3_3() {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_3.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };
  EJDB db;
  char dbuf[1024];

  int i = 0;
  EJDB_LIST list = 0;
  IWXSTR *log = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(log);
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "a2", "/f/b", EJDB_IDX_STR);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  char *data[] = {

    // len: 200
    "Ar4prlJssa2ckf0IpmDuRBZ2b0Q6PtPdTacjWFFuO23CiCjdyfHaliz9JaqK1HFEeaneiMO"
    "7sNh87oDLVkvz7TnOV22v0njqmmd6b8DSfzaCwxFxcqrF7MinjUvJvct1Fr07MJWeG7C6SP"
    "MlUjFQ4jNlds3kUQDP9yxQImH7BkmCqBCisIoh5zar8zSax1Pk7nZSpm1b",

    // len: 115
    "BkMSITU2qJ56xeX3nftUd3g4PuZwo9LY2mTGFtYKrrqhilPiR0UTHrDobstoShELlMHvPx61"
    "KF8qQRPAn4OOUttNtkPE95XsjZQ8PPZW9ruWo1R9UMx",

    // len: 64
    "C1257xkZuJqXhQ5v5eWG8TlwKdCY77DQ0ScLyC3nGDTtC3A8DPDAiVC09EBFTUxp",

    // len: 800
    "D9z1bYv2oEp8n8B0BtY1VI4ezy8adAnPvqno9rdxM7RsMZDcLQyCEJ3vDMFqoJaRNbCtdbHh09"
    "L0UijAR6wmQ87P90eAGKaEvuhoRzhoDZYpa37o7HZrBctcCxGHSrQMR0o1NKOz2vmEvhX6k02M"
    "QopatRrL6jIkr9XXKgekOh6xcVyvUcnr6ttD3tqF0v0QN3ZPnXRCcVYyx0Ot9T6EfZik7HO3QW"
    "jyblg4f4qqohprmGWKO8UfIIsF1gRPPacPc8oJXEFrbJu5NR4LidKpn6ygmTpstGVCanpCq2Yi"
    "pkWrQpC1LkdSh3h2hNMUZbgZDgsvGzocuBuGgnyDd6I91PJjBmbFTXmILkT9t0ApvCGJTrc9aB"
    "Sw5I9CAZRFRqVnRFUr7fA0OfQhN2zCu4d5m1XQg3yh9We4GI1ffhWsbnH4g59HbuAkm9jmz4mp"
    "B6IJUSewlgq6YDgfsPNQXHboBVWAuR9NONIpfpmnU4wjuwI3j3QJAbi81u23QXAWJETvVBRqqU"
    "ZqtqUjwnOCoPvkRV3WhfEHezmN9HTuxxl75WowRXyz8nwUe3xOXadmQwkhyYbWSSrO835XziTj"
    "58e00zGi6eLwXD7xrKC7YeEb7HE4L8eKeEFiM1xC00ySIDkBNoclMxmExPg6kBUpiUT7HAEfaN"
    "AN2VbdWFZsumDQHu3Q5XwrGdiQ6ubLTMuQEIv1IPTZIRTV5TQ59aUdiM6POdLFv9xuDjEgnBYd"
    "MxcP60sNDIakuW2IeabKScwF5yx9PAg7D9K5WVWhpSQuzDFiTSSJPwkQfoWo"
  };

  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":\"%s\"},\"n\":%d}", data[0], 1);
  rc = put_json(db, "a2", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":\"%s\"},\"n\":%d}", data[1], 2);
  rc = put_json(db, "a2", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":\"%s\"},\"n\":%" PRId64 "}", data[1], INT64_MAX - 1);
  rc = put_json(db, "a2", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":\"%s\"},\"n\":%d}", data[2], 3);
  rc = put_json(db, "a2", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":\"%s\"},\"n\":%d}", data[2], 4);
  rc = put_json(db, "a2", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);


  snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":\"%s\"},\"n\":%d}", data[3], 5);
  rc = put_json(db, "a2", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // Q: /f/[b >= data[0]]
  JQL q;
  rc = jql_create(&q, "a2", "/f/[b >= :?]");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_str(q, 0, 0, data[0]);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] SELECTED STR|6 /f/b EXPR1: 'b >= :?' "
                                "INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_PREV"));
  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    JBL jbl1, jbl2;
    rc = jbl_at(doc->raw, "/f/b", &jbl1);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    rc = jbl_at(doc->raw, "/n", &jbl2);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    int64_t llv = jbl_get_i64(jbl2);
    const char *str = jbl_get_str(jbl1);
    switch (i) {
      case 1:
        CU_ASSERT_EQUAL(llv, 1);
        CU_ASSERT_STRING_EQUAL(str, data[0]);
        break;
      case 2:
        CU_ASSERT_EQUAL(llv, 2);
        CU_ASSERT_STRING_EQUAL(str, data[1]);
        break;
      case 3:
        CU_ASSERT_EQUAL(llv, INT64_MAX - 1);
        CU_ASSERT_STRING_EQUAL(str, data[1]);
        break;
      case 4:
        CU_ASSERT_EQUAL(llv, 3);
        CU_ASSERT_STRING_EQUAL(str, data[2]);
        break;
      case 5:
        CU_ASSERT_EQUAL(llv, 4);
        CU_ASSERT_STRING_EQUAL(str, data[2]);
        break;
      case 6:
        CU_ASSERT_EQUAL(llv, 5);
        CU_ASSERT_STRING_EQUAL(str, data[3]);
        break;
    }
    jbl_destroy(&jbl1);
    jbl_destroy(&jbl2);
  }
  CU_ASSERT_EQUAL(i, 7);

  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Q: /f/[b >= data[3]]
  rc = jql_set_str(q, 0, 0, data[3]);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] SELECTED STR|6 /f/b EXPR1: 'b >= :?' "
                                "INIT: IWKV_CURSOR_GE STEP: IWKV_CURSOR_PREV"));

  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    JBL jbl1, jbl2;
    rc = jbl_at(doc->raw, "/f/b", &jbl1);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    rc = jbl_at(doc->raw, "/n", &jbl2);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    int64_t llv = jbl_get_i64(jbl2);
    const char *str = jbl_get_str(jbl1);
    CU_ASSERT_EQUAL(llv, 5);
    CU_ASSERT_STRING_EQUAL(str, data[3]);
    jbl_destroy(&jbl1);
    jbl_destroy(&jbl2);
  }
  CU_ASSERT_EQUAL(i, 2);

  // todo: record update
  // todo: record removal

  ejdb_list_destroy(&list);
  iwxstr_clear(log);
  jql_destroy(&q);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(log);
  iwxstr_destroy(xstr);
}

// Test array index
static void ejdb_test3_4() {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_4.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };
  EJDB db;
  char dbuf[1024];

  int i = 0;
  EJDB_LIST list = 0;
  int64_t docId = 0;

  IWPOOL *pool = iwpool_create(1024);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pool);
  IWXSTR *log = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(log);
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "a3", "/tags", EJDB_IDX_STR);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  snprintf(dbuf, sizeof(dbuf), "{\"tags\": [\"foo\", \"bar\", \"gaz\"],\"n\":%d}", 1);
  rc = put_json(db, "a3", dbuf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  snprintf(dbuf, sizeof(dbuf), "{\"tags\": [\"gaz\", \"zaz\"],\"n\":%d}", 2);
  rc = put_json2(db, "a3", dbuf, &docId);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  JQL q;
  rc = jql_create(&q, "a3", "/tags/[** in :tags]");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // Q:
  JBL_NODE qtags;
  rc = jbn_from_json("[\"zaz\",\"gaz\"]", &qtags, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_json(q, "tags", 0, qtags);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED STR|5 /tags EXPR1: '** in :tags' "
                                "INIT: IWKV_CURSOR_EQ"));
  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 1) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"tags\":[\"foo\",\"bar\",\"gaz\"],\"n\":1}");
    } else if ((i == 2) || (i == 3)) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"tags\":[\"gaz\",\"zaz\"],\"n\":2}");
    }
  }
  CU_ASSERT_EQUAL(i, 4);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);


  // Get
  CU_ASSERT_TRUE(docId > 0);
  JBL jbl;
  rc = ejdb_get(db, "a3", docId, &jbl);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, 0);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&jbl);
  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"tags\":[\"gaz\",\"zaz\"],\"n\":2}");
  rc = put_json2(db, "a3", "{\"tags\": [\"gaz\",\"zaz\"],\"n\":2}", &docId);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // Update {"tags":["gaz","zaz", "boo"], "n":2}
  rc = put_json2(db, "a3", "{\"tags\": [\"gaz\",\"zaz\",\"boo\"],\"n\":2}", &docId);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // Q:
  rc = jbn_from_json("[\"zaz\",\"boo\"]", &qtags, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_json(q, "tags", 0, qtags);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED STR|6 /tags EXPR1: '** in :tags' "
                                "INIT: IWKV_CURSOR_EQ"));
  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"tags\":[\"gaz\",\"zaz\",\"boo\"],\"n\":2}");
  }
  CU_ASSERT_EQUAL(i, 3);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  // Remove last
  rc = ejdb_del(db, "a3", docId);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // G2
  rc = jbn_from_json("[\"gaz\"]", &qtags, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_json(q, "tags", 0, qtags);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log),
                                "[INDEX] SELECTED STR|3 /tags EXPR1: '** in :tags' "
                                "INIT: IWKV_CURSOR_EQ"));
  i = 1;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"tags\":[\"foo\",\"bar\",\"gaz\"],\"n\":1}");
  }
  CU_ASSERT_EQUAL(i, 2);

  ejdb_list_destroy(&list);
  iwxstr_clear(log);


  jql_destroy(&q);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(log);
  iwxstr_destroy(xstr);
  iwpool_destroy(pool);
}

void ejdb_test3_5() {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_5.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };
  EJDB db;
  EJDB_LIST list = 0;
  char dbuf[1024];
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  for (int i = 1; i <= 10; ++i) {
    snprintf(dbuf, sizeof(dbuf), "{\"f\":{\"b\":%d},\"n\":%d}", i, i);
    rc = put_json(db, "c1", dbuf);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
  }

  rc = ejdb_list3(db, "c1", "/f/[b = 2] | del", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  int i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":2},\"n\":2}");
  }
  CU_ASSERT_EQUAL_FATAL(i, 1);
  ejdb_list_destroy(&list);

  // Check if /f/[b = 2] has been deleted
  rc = ejdb_list3(db, "c1", "/f/[b = 2]", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NULL(list->first);
  ejdb_list_destroy(&list);

  // Ensure index on /f/b
  rc = ejdb_ensure_index(db, "c2", "/f/b", EJDB_IDX_UNIQUE | EJDB_IDX_I64);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list3(db, "c1", "/f/[b = 3] | del", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  ejdb_list_destroy(&list);

  // Check if /f/[b = 3] has been deleted
  rc = ejdb_list3(db, "c1", "/f/[b = 3]", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NULL(list->first);
  ejdb_list_destroy(&list);

  rc = ejdb_list3(db, "c1", "/* | asc /f/b", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  i = 0;
  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    iwxstr_clear(xstr);
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    if (i == 0) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":1},\"n\":1}");
    } else if (i == 1) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":4},\"n\":4}");
    } else if (i == 7) {
      CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"f\":{\"b\":10},\"n\":10}");
    }
  }
  ejdb_list_destroy(&list);
  CU_ASSERT_EQUAL_FATAL(i, 8);

  // Remove rest of elements
  rc = ejdb_list3(db, "c1", "/* | del | desc /f/b", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  ejdb_list_destroy(&list);

  // Check coll is empty
  rc = ejdb_list3(db, "c1", "/*", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NULL(list->first);
  ejdb_list_destroy(&list);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(xstr);
}

static void jql_free_str(void *ptr, void *op) {
  if (ptr) {
    free(ptr);
  }
}

void ejdb_test3_6() {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_6.db",
      .oflags = IWKV_TRUNC
    }
  };

  JQL q;
  EJDB db;
  EJDB_LIST list = 0;
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "mycoll", "/foo", EJDB_IDX_UNIQUE | EJDB_IDX_STR);
  CU_ASSERT_EQUAL_FATAL(rc, 0);


  rc = put_json(db, "mycoll", "{\"foo\":\"baz\",\"baz\":\"qux\"}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_create(&q, 0, "@mycoll/[foo re :?]");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_regexp(q, 0, 0, ".*");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(list->first);

  iwxstr_clear(xstr);
  rc = jbl_as_json(list->first->raw, jbl_xstr_json_printer, xstr, 0);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"foo\":\"baz\",\"baz\":\"qux\"}");
  ejdb_list_destroy(&list);

  // Now set regexp again
  rc = jql_set_regexp2(q, 0, 0, strdup(".*"), jql_free_str, 0);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(list->first);

  ejdb_list_destroy(&list);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(xstr);
  jql_destroy(&q);
}

void ejdb_test3_7() {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_7.db",
      .oflags = IWKV_TRUNC
    }
  };
  EJDB db;
  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json(db, "cc1", "{'foo':1}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_rename_collection(db, "cc1", "cc2");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  JBL jbl;
  rc = ejdb_get(db, "cc2", 1, &jbl);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&jbl);

  rc = ejdb_rename_collection(db, "cc1", "cc2");
  CU_ASSERT_EQUAL_FATAL(rc, EJDB_ERROR_COLLECTION_NOT_FOUND);

  rc = ejdb_rename_collection(db, "cc2", "cc2");
  CU_ASSERT_EQUAL_FATAL(rc, EJDB_ERROR_TARGET_COLLECTION_EXISTS);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  opts.kv.oflags = 0;

  rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_get(db, "cc2", 1, &jbl);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&jbl);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
}

void ejdb_test3_8(void) {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_8.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };

  EJDB db;
  JQL q;
  char buf[64];
  JBL_NODE n;

  int64_t id1 = 0, id2 = 0;
  EJDB_LIST list = 0;

  IWPOOL *pool = iwpool_create(255);
  IWXSTR *log = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(log);

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json2(db, "users", "{'name':'Andy'}", &id1);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json2(db, "users", "{'name':'John'}", &id2);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_create(&q, "users", "/=:?");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_i64(q, 0, 0, id1);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(log), "[INDEX] PK [COLLECTOR] PLAIN"));
  CU_ASSERT_PTR_NOT_NULL(list->first);
  CU_ASSERT_PTR_NULL(list->first->next);

  jql_destroy(&q);
  ejdb_list_destroy(&list);
  iwxstr_clear(log);

  rc = jql_create(&q, 0, "@users/=:id");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_str(q, "id", 0, "1");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(list->first);
  CU_ASSERT_PTR_NULL(list->first->next);
  jql_destroy(&q);
  ejdb_list_destroy(&list);

  // matching against PK array
  snprintf(buf, sizeof(buf), "@users/=[%" PRId64 ",%" PRId64 "]", id1, id2);
  rc = jql_create(&q, 0, buf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(list->first);
  CU_ASSERT_PTR_NOT_NULL(list->first->next);
  jql_destroy(&q);
  ejdb_list_destroy(&list);

  // matching against PK array as JSON query paramater
  snprintf(buf, sizeof(buf), "[%" PRId64 ",%" PRId64 "]", id1, id2);
  rc = jbn_from_json(buf, &n, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_create(&q, 0, "@users/=:?");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_json(q, 0, 0, n);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_list4(db, q, 0, log, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL(list->first);
  CU_ASSERT_PTR_NOT_NULL(list->first->next);
  jql_destroy(&q);
  ejdb_list_destroy(&list);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(log);
  iwpool_destroy(pool);
}

static void ejdb_test3_9(void) {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test3_9",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };
  EJDB db;
  EJDB_LIST list = 0;
  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0)

  rc = put_json(db, "zzz", "[[\"one\",\"two\"]]");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json(db, "zzz", "[[\"red\",\"brown\"],[false]]");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list3(db, "zzz", "/*/[** = one]", 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  CU_ASSERT_PTR_NOT_NULL(list->first);
  CU_ASSERT_PTR_NULL(list->first->next);

  ejdb_list_destroy(&list);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) {
    return CU_get_error();
  }
  pSuite = CU_add_suite("ejdb_test3", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (  (NULL == CU_add_test(pSuite, "ejdb_test3_1", ejdb_test3_1))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_2", ejdb_test3_2))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_3", ejdb_test3_3))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_4", ejdb_test3_4))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_5", ejdb_test3_5))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_6", ejdb_test3_6))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_7", ejdb_test3_7))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_8", ejdb_test3_8))
     || (NULL == CU_add_test(pSuite, "ejdb_test3_9", ejdb_test3_9))) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  int ret = CU_get_error() || CU_get_number_of_failures();
  CU_cleanup_registry();
  return ret;
}
