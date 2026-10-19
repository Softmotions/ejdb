#include "ejdb_test.h"
#include "iwtests.h"

#include <iowow/iwuuid.h>

static int init_suite(void) {
  iwrc rc = ejdb_init();
  return rc;
}

static void clean_suite(void) {
}

static void free_iwpool(void *ptr, void *op) {
  iwpool_destroy((IWPOOL*) op);
}

static void set_apply_int(struct jql *q, int idx, const char *key, int64_t id) {
  JBL_NODE n;
  IWPOOL *pool = iwpool_create(64);
  iwrc rc = jbn_from_json("{}", &n, pool);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbn_add_item_i64(n, key, id, 0, pool);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_json2(q, 0, idx, n, free_iwpool, pool);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
}

static void ejdb_test4_1(void) {
  struct ejdb_opts opts = {
    .kv = {
      .path = "ejdb_test4_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };

  struct ejdb *db;
  struct jql *q;
  int64_t id = 0;
  struct ejdb_list *list = 0;
  IWXSTR *xstr = iwxstr_new();

  iwrc rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json2(db, "artists", "{'name':'Leonardo Da Vinci', 'years':[1452,1519]}", &id);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = put_json(db, "paintings", "{'name':'Mona Lisa', 'year':1490, 'origin':'Italy'}");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_create(&q, "paintings", "/[name=:?] | apply :?");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_str(q, 0, 0, "Mona Lisa");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  set_apply_int(q, 1, "artist_ref", id);
  rc = ejdb_update(db, q);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  jql_destroy(&q);

  rc = put_json(db, "paintings", "{'name':'Madonna Litta - Madonna And The Child', 'year':1490, 'origin':'Italy'}");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_create(&q, "paintings", "/[name=:?] | apply :?");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jql_set_str(q, 0, 0, "Madonna Litta - Madonna And The Child");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  set_apply_int(q, 1, "artist_ref", id);
  rc = ejdb_update(db, q);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  jql_destroy(&q);
  id = 0;

  rc = jql_create(&q, "paintings", "/* | /artist_ref<artists");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_list4(db, q, 0, 0, &list);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  for (struct ejdb_doc *doc = list->first; doc; doc = doc->next) {
    JBL_NODE n;
    iwxstr_clear(xstr);
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    //fprintf(stderr, "%s\n", iwxstr_ptr(xstr));
    rc = jbn_at(doc->node, "/artist_ref/name", &n);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    IW_TEST_ASSERT_EQUAL(n->type, JBV_STR);
    IW_TEST_ASSERT_NSTR_EQUAL(n->vptr, "Leonardo Da Vinci", n->vsize);

    rc = jbn_at(doc->node, "/year", &n);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    IW_TEST_ASSERT_EQUAL(n->type, JBV_I64);
    IW_TEST_ASSERT_EQUAL(n->vi64, 1490);
  }
  jql_destroy(&q);
  ejdb_list_destroy(&list);


  // Next mode
  rc = jql_create(&q, "paintings", "/* | /{name, artist_ref<artists} - /artist_ref/years/0");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  rc = ejdb_list4(db, q, 0, 0, &list);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  for (struct ejdb_doc *doc = list->first; doc; doc = doc->next) {
    JBL_NODE n;
    iwxstr_clear(xstr);
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    //fprintf(stderr, "%s\n", iwxstr_ptr(xstr));

    rc = jbn_at(doc->node, "/name", &n);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    IW_TEST_ASSERT_EQUAL(n->type, JBV_STR);

    rc = jbn_at(doc->node, "/artist_ref/name", &n);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    IW_TEST_ASSERT_EQUAL(n->type, JBV_STR);
    IW_TEST_ASSERT_NSTR_EQUAL(n->vptr, "Leonardo Da Vinci", n->vsize);

    rc = jbn_at(doc->node, "/artist_ref/years/1", &n); // todo: review
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    IW_TEST_ASSERT_EQUAL(n->type, JBV_I64);
    IW_TEST_ASSERT_EQUAL(n->vi64, 1519);
  }
  jql_destroy(&q);
  ejdb_list_destroy(&list);

  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_destroy(xstr);
}

static void ejdb_test4_2(void) {
  struct ejdb_opts opts = {
    .kv = {
      .path = "ejdb_test4_2.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true
  };

  struct ejdb *db;
  struct jql *q;
  JBL_NODE n, n2;
  int i = 0;
  struct ejdb_list *list = 0;
  IWPOOL *pool = iwpool_create_empty();

  char uuid[IW_UUID_STR_LEN + 1] = { 0 };
  iwu_uuid4_fill(uuid);

  iwrc rc = ejdb_open(&opts, &db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_ensure_index(db, "users", "/uuid", EJDB_IDX_STR | EJDB_IDX_UNIQUE);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_create(&q, "users", "/[uuid = :?] | upsert :?");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_str(q, 0, 0, uuid);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_from_json("{}", &n, pool);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_add_item_str(n, "uuid", uuid, -1, 0, pool);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_add_item_str(n, "name", "a", -1, 0, pool);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_json(q, 0, 1, n);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_update(db, q);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_at(n, "/name", &n2);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  n2->vptr = "b";
  rc = jql_set_json(q, 0, 1, n);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_update(db, q);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  jql_destroy(&q);
  rc = jql_create(&q, "users", "/* | /name");
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  rc = ejdb_list4(db, q, 0, 0, &list);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);

  for (struct ejdb_doc *doc = list->first; doc; doc = doc->next, ++i) {
    IW_TEST_ASSERT_PTR_NOT_NULL_FATAL(doc->node);
    rc = jbn_at(doc->node, "/name", &n2);
    IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
    IW_TEST_ASSERT_STR_EQUAL(n2->vptr, "b");
  }
  IW_TEST_ASSERT_EQUAL(i, 1);

  jql_destroy(&q);
  ejdb_list_destroy(&list);
  iwpool_destroy(pool);
  rc = ejdb_close(&db);
  IW_TEST_ASSERT_EQUAL_FATAL(rc, 0);
}

int main(int argc, char *argv[]) {
  static const struct iw_test_case cases[] = {
    IW_TEST_CASE(ejdb_test4_1),
    IW_TEST_CASE(ejdb_test4_2),
  };
  return iw_test_run(argc, argv, IW_TEST_SUITE("ejdb_test4", cases, init_suite, clean_suite));
}
