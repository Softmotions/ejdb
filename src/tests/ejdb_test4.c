#include "ejdb_test.h"
#include <CUnit/Basic.h>

#include <ejdb2/iowow/iwuuid.h>

int init_suite(void) {
  iwrc rc = ejdb_init();
  return rc;
}

int clean_suite() {
  return 0;
}

static void free_iwpool(void *ptr, void *op) {
  iwpool_destroy((IWPOOL*) op);
}

static void set_apply_int(JQL q, int idx, const char *key, int64_t id) {
  JBL_NODE n;
  IWPOOL *pool = iwpool_create(64);
  iwrc rc = jbn_from_json("{}", &n, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbn_add_item_i64(n, key, id, 0, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_json2(q, 0, idx, n, free_iwpool, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
}

static void ejdb_test4_1(void) {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test4_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };

  EJDB db;
  JQL q;
  int64_t id = 0;
  EJDB_LIST list = 0;
  IWXSTR *xstr = iwxstr_new();

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json2(db, "artists", "{'name':'Leonardo Da Vinci', 'years':[1452,1519]}", &id);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json(db, "paintings", "{'name':'Mona Lisa', 'year':1490, 'origin':'Italy'}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_create(&q, "paintings", "/[name=:?] | apply :?");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_str(q, 0, 0, "Mona Lisa");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  set_apply_int(q, 1, "artist_ref", id);
  rc = ejdb_update(db, q);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jql_destroy(&q);

  rc = put_json(db, "paintings", "{'name':'Madonna Litta - Madonna And The Child', 'year':1490, 'origin':'Italy'}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_create(&q, "paintings", "/[name=:?] | apply :?");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_str(q, 0, 0, "Madonna Litta - Madonna And The Child");
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
    JBL_NODE n;
    iwxstr_clear(xstr);
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    //fprintf(stderr, "%s\n", iwxstr_ptr(xstr));
    rc = jbn_at(doc->node, "/artist_ref/name", &n);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_EQUAL(n->type, JBV_STR);
    CU_ASSERT_NSTRING_EQUAL(n->vptr, "Leonardo Da Vinci", n->vsize);

    rc = jbn_at(doc->node, "/year", &n);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_EQUAL(n->type, JBV_I64);
    CU_ASSERT_EQUAL(n->vi64, 1490);
  }
  jql_destroy(&q);
  ejdb_list_destroy(&list);


  // Next mode
  rc = jql_create(&q, "paintings", "/* | /{name, artist_ref<artists} - /artist_ref/years/0");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_list4(db, q, 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  for (EJDB_DOC doc = list->first; doc; doc = doc->next) {
    JBL_NODE n;
    iwxstr_clear(xstr);
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    //fprintf(stderr, "%s\n", iwxstr_ptr(xstr));

    rc = jbn_at(doc->node, "/name", &n);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_EQUAL(n->type, JBV_STR);

    rc = jbn_at(doc->node, "/artist_ref/name", &n);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_EQUAL(n->type, JBV_STR);
    CU_ASSERT_NSTRING_EQUAL(n->vptr, "Leonardo Da Vinci", n->vsize);

    rc = jbn_at(doc->node, "/artist_ref/years/1", &n); // todo: review
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_EQUAL(n->type, JBV_I64);
    CU_ASSERT_EQUAL(n->vi64, 1519);
  }
  jql_destroy(&q);
  ejdb_list_destroy(&list);

  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(xstr);
}

static void ejdb_test4_2(void) {
  EJDB_OPTS opts = {
    .kv       = {
      .path   = "ejdb_test4_2.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal   = true
  };

  EJDB db;
  JQL q;
  JBL_NODE n, n2;
  int i = 0;
  EJDB_LIST list = 0;
  IWPOOL *pool = iwpool_create_empty();

  char uuid[IW_UUID_STR_LEN + 1] = { 0 };
  iwu_uuid4_fill(uuid);

  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "users", "/uuid", EJDB_IDX_STR | EJDB_IDX_UNIQUE);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_create(&q, "users", "/[uuid = :?] | upsert :?");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_str(q, 0, 0, uuid);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_from_json("{}", &n, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_add_item_str(n, "uuid", uuid, -1, 0, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_add_item_str(n, "name", "a", -1, 0, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_json(q, 0, 1, n);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_update(db, q);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_at(n, "/name", &n2);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  n2->vptr = "b";
  rc = jql_set_json(q, 0, 1, n);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_update(db, q);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  jql_destroy(&q);
  rc = jql_create(&q, "users", "/* | /name");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, 0, &list);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  for (EJDB_DOC doc = list->first; doc; doc = doc->next, ++i) {
    CU_ASSERT_PTR_NOT_NULL_FATAL(doc->node);
    rc = jbn_at(doc->node, "/name", &n2);
    CU_ASSERT_EQUAL_FATAL(rc, 0);
    CU_ASSERT_STRING_EQUAL(n2->vptr, "b");
  }
  CU_ASSERT_EQUAL(i, 1);

  jql_destroy(&q);
  ejdb_list_destroy(&list);
  iwpool_destroy(pool);
  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) {
    return CU_get_error();
  }
  pSuite = CU_add_suite("ejdb_test4", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (  (NULL == CU_add_test(pSuite, "ejdb_test4_1", ejdb_test4_1))
     || (NULL == CU_add_test(pSuite, "ejdb_test4_2", ejdb_test4_2))) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  int ret = CU_get_error() || CU_get_number_of_failures();
  CU_cleanup_registry();
  return ret;
}
