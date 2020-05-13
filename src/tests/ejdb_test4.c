#include "ejdb_test.h"
#include <CUnit/Basic.h>

int init_suite(void) {
  iwrc rc = ejdb_init();
  return rc;
}

int clean_suite() {
  return 0;
}


static void free_iwpool(void *ptr, void *op) {
  iwpool_destroy((IWPOOL *) op);
}

static void set_apply_int(JQL q, int idx, const char *key, int64_t id) {
  JBL_NODE n;
  IWPOOL *pool = iwpool_create(64);
  iwrc rc = jbn_from_json("{}", &n, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbn_add_item_i64(n, key, id, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_json2(q, 0, idx, n, free_iwpool, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
}

void ejdb_test4_1(void) {
  EJDB_OPTS opts = {
    .kv = {
      .path = "ejdb_test4_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };

  EJDB db;
  JQL q;
  int64_t id = 0;
  EJDB_LIST list = 0;

  IWXSTR *xstr = iwxstr_new();
  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json2(db, "artists", "{'name':'Leonardo Da Vinci'}", &id);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = put_json(db, "paintings", "{'name':'Mona Lisa'}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_create(&q, "paintings", "/[name=:?] | apply :?");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_str(q, 0, 0, "Mona Lisa");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  set_apply_int(q, 1, "artist_ref", id);
  rc = ejdb_update(db, q);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jql_destroy(&q);
  id = 0;


  rc = jql_create(&q, "paintings", "/* | /artist_ref<artists");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_list4(db, q, 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);


  for (EJDB_DOC doc = list->first; doc; doc = doc->next) {
    iwxstr_clear(xstr);
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    fprintf(stderr, "%s\n", iwxstr_ptr(xstr));
  }
  jql_destroy(&q);

  // rc = put_json(db, "artists", "{'name':'Vincent Van Gogh'}");
  // CU_ASSERT_EQUAL_FATAL(rc, 0);
  // rc = put_json(db, "artists", "{'name':'Edvard Munch'}");
  // CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  ejdb_list_destroy(&list);
  iwxstr_destroy(xstr);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
  pSuite = CU_add_suite("ejdb_test3", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (
    (NULL == CU_add_test(pSuite, "ejdb_test4_1", ejdb_test4_1))
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
