#include "jqp.h"
#include "ejdb2_internal.h"
#include <iowow/iwxstr.h>
#include <iowow/iwutils.h>
#include <CUnit/Basic.h>
#include <stdlib.h>

int init_suite(void) {
  int rc = ejdb_init();
  return rc;
}

int clean_suite(void) {
  return 0;
}

void _jql_test1_1(int num, iwrc expected) {
  fprintf(stderr, "%03d.jql\n", num);

  iwrc rc;
  char path[64];
  char path_expected[64];
  JQP_AUX *aux;
  char *data, *edata = 0;
  IWXSTR *res = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(res);

  snprintf(path, sizeof(path), "data%c%03d.jql", IW_PATH_CHR, num);
  snprintf(path_expected, sizeof(path_expected), "data%c%03d.expected.jql", IW_PATH_CHR, num);
  data = iwu_file_read_as_buf(path);
  CU_ASSERT_PTR_NOT_NULL_FATAL(data);

  rc = jqp_aux_create(&aux, data);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jqp_parse(aux);
  CU_ASSERT_EQUAL_FATAL(rc, expected);
  if (expected) {
    goto finish;
  }

  CU_ASSERT_PTR_NOT_NULL_FATAL(aux->query);

  edata = iwu_file_read_as_buf(path_expected);
  CU_ASSERT_PTR_NOT_NULL_FATAL(edata);
  rc = jqp_print_query(aux->query, jbl_xstr_json_printer, res);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  // fprintf(stderr, "%s\n", iwxstr_ptr(res));
  // fprintf(stderr, "%s\n", path_expected);
  // fprintf(stderr, "%s\n", edata);

  //  fprintf(stderr, "%d\n", strcmp(edata, iwxstr_ptr(res)));
  //  FILE *out = fopen("out.txt", "w+");
  //  fprintf(out, "%s", iwxstr_ptr(res));
  //  fclose(out);

  CU_ASSERT_EQUAL_FATAL(strcmp(edata, iwxstr_ptr(res)), 0);

finish:
  if (edata) {
    free(edata);
  }
  free(data);
  iwxstr_destroy(res);
  jqp_aux_destroy(&aux);
}

static void jql_test1_1(void) {
  _jql_test1_1(22, 0);

  for (int i = 0; i <= 10; ++i) {
    _jql_test1_1(i, 0);
  }
  for (int i = 11; i <= 13; ++i) {
    _jql_test1_1(i, JQL_ERROR_QUERY_PARSE);
  }
  for (int i = 14; i <= 22; ++i) {
    _jql_test1_1(i, 0);
  }
}

static void _jql_test1_2(const char *jsondata, const char *q, bool match) {
  JBL jbl;
  JQL jql;
  char *json = iwu_replace_char(strdup(jsondata), '\'', '"');
  CU_ASSERT_PTR_NOT_NULL_FATAL(json);
  iwrc rc = jql_create(&jql, "c1", q);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_from_json(&jbl, json);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  bool m = false;
  rc = jql_matched(jql, jbl, &m);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL_FATAL(m, match);

  jql_destroy(&jql);
  jbl_destroy(&jbl);
  free(json);
}

static void jql_test1_2(void) {
  _jql_test1_2("{}", "/*", true);
  _jql_test1_2("{}", "/**", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/*", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/**", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/bar", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/baz", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/bar and /foo/bar or /foo", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/baz or /foo", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/baz and (/foo/daz or /foo/bar)", false);
  _jql_test1_2("{'foo':{'bar':22}}", "(/boo or /foo) and (/foo/daz or /foo/bar)", true);
  _jql_test1_2("{'foo':{'bar':22, 'bar2':'vvv2'}}", "/foo/bar2", true);

  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar = 22]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar eq 22]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar !eq 22]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar != 22]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar >= 22]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/*/[bar >= 22]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar > 21]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar > 22]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar < 23]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar <= 22]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar < 22]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/*/[bar < 22]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/*/[bar > 20 and bar <= 23]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/*/[bar > 22 and bar <= 23]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/*/[bar > 23 or bar < 23]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/*/[bar < 23 or bar > 23]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[[* = bar] = 22]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[[* = bar] != 23]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* = foo]/[[* = bar] != 23]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* != foo]/[[* = bar] != 23]", false);

  // regexp
  _jql_test1_2("{'foo':{'bar':22}}", "/[* re \"foo\"]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* re fo]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* re ^foo$]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* re ^fo$]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* not re ^fo$]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar re 22]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar re \"2+\"]", true);

  // in
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar in [21, \"22\"]]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/foo/[bar in [21, 23]]", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* in [\"foo\"]]/[bar in [21, 22]]", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/[* not in [\"foo\"]]/[bar in [21, 22]]", false);

  // Array element
  _jql_test1_2("{'tags':['bar', 'foo']}", "/tags/[** in [\"bar\", \"baz\"]]", true);
  _jql_test1_2("{'tags':['bar', 'foo']}", "/tags/[** in [\"zaz\", \"gaz\"]]", false);

  // /**
  _jql_test1_2("{'foo':{'bar':22}}", "/**", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/**/bar", true);
  _jql_test1_2("{'foo':{'bar':22}}", "/**/baz", false);
  _jql_test1_2("{'foo':{'bar':22}}", "/**/**/bar", true);
  _jql_test1_2("{'foo':{'bar':22, 'baz':{'zaz':33}}}", "/foo/**/zaz", true);
  _jql_test1_2("{'foo':{'bar':22, 'baz':{'zaz':33}}}", "/foo/**/[zaz > 30]", true);
  _jql_test1_2("{'foo':{'bar':22, 'baz':{'zaz':33}}}", "/foo/**/[zaz < 30]", false);

  // arr/obj
  _jql_test1_2("{'foo':[1,2]}", "/[foo = [1,2]]", true);
  _jql_test1_2("{'foo':[1,2]}", "/[foo ni 2]", true);
  _jql_test1_2("{'foo':[1,2]}", "/[foo in [[1,2]]]", true);
  _jql_test1_2("{'foo':{'arr':[1,2,3,4]}}", "/foo/[arr = [1,2,3,4]]", true);
  _jql_test1_2("{'foo':{'arr':[1,2,3,4]}}", "/foo/**/[arr = [1,2,3,4]]", true);
  _jql_test1_2("{'foo':{'arr':[1,2,3,4]}}", "/foo/*/[arr = [1,2,3,4]]", false);
  _jql_test1_2("{'foo':{'arr':[1,2,3,4]}}", "/foo/[arr = [1,2,3]]", false);
  _jql_test1_2("{'foo':{'arr':[1,2,3,4]}}", "/foo/[arr = [1,12,3,4]]", false);
  _jql_test1_2("{'foo':{'obj':{'f':'d','e':'j'}}}", "/foo/[obj = {\"e\":\"j\",\"f\":\"d\"}]", true);
  _jql_test1_2("{'foo':{'obj':{'f':'d','e':'j'}}}", "/foo/[obj = {\"e\":\"j\",\"f\":\"dd\"}]", false);

  _jql_test1_2("{'f':22}", "/f", true);
  _jql_test1_2("{'a':'bar'}", "/f | asc /f", false);

  // PK
  _jql_test1_2("{'f':22}", "/=22", true);
  _jql_test1_2("{'f':22}", "@mycoll/=22", true);

  //
  const char *doc
    = "{"
      " 'foo':{"
      "   'bar': {'baz':{'zaz':33}},"
      "   'sas': {'gaz':{'zaz':44, 'zarr':[42]}},"
      "   'arr': [1,2,3,4]"
      " }"
      "}";
  _jql_test1_2(doc, "/foo/sas/gaz/zaz", true);
  _jql_test1_2(doc, "/foo/sas/gaz/[zaz = 44]", true);
  _jql_test1_2(doc, "/**/[zaz = 44]", true);
  _jql_test1_2(doc, "/foo/**/[zaz = 44]", true);
  _jql_test1_2(doc, "/foo/*/*/[zaz = 44]", true);
  _jql_test1_2(doc, "/foo/[arr ni 3]", true);
  _jql_test1_2(doc, "/**/[zarr ni 42]", true);
  _jql_test1_2(doc, "/**/[[* in [\"zarr\"]] in [[42]]]", true);
}

static void _jql_test1_3(bool has_apply_or_project, const char *jsondata, const char *q, const char *eq) {
  JBL jbl;
  JQL jql;
  JBL_NODE out = 0, eqn = 0;
  IWPOOL *pool = iwpool_create(512);

  char *json = iwu_replace_char(strdup(jsondata), '\'', '"');
  CU_ASSERT_PTR_NOT_NULL_FATAL(json);
  char *eqjson = iwu_replace_char(strdup(eq), '\'', '"');
  CU_ASSERT_PTR_NOT_NULL_FATAL(eqjson);
  char *qstr = iwu_replace_char(strdup(q), '\'', '"');
  CU_ASSERT_PTR_NOT_NULL_FATAL(qstr);

  iwrc rc = jql_create(&jql, "c1", qstr);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_from_json(&jbl, json);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  bool m = false;
  rc = jql_matched(jql, jbl, &m);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL_FATAL(m, true);

  bool hapl = jql_has_apply(jql) || jql_has_projection(jql);
  CU_ASSERT_EQUAL_FATAL(hapl, has_apply_or_project);
  if (!hapl) {
    goto finish;
  }

  CU_ASSERT_PTR_NOT_NULL_FATAL(pool);
  rc = jql_apply_and_project(jql, jbl, &out, 0, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(out);

  rc = jbn_from_json(eqjson, &eqn, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  int cmp = jbn_compare_nodes(out, eqn, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL_FATAL(cmp, 0);

finish:
  jql_destroy(&jql);
  jbl_destroy(&jbl);
  free(json);
  free(eqjson);
  free(qstr);
  iwpool_destroy(pool);
}

static void jql_test1_3(void) {
  _jql_test1_3(true, "{'foo':{'bar':22}}",
               "/foo/bar | apply [{'op':'add', 'path':'/baz', 'value':'qux'}]",
               "{'foo':{'bar':22},'baz':'qux'}");

  _jql_test1_3(true, "{'foo':{'bar':22}}",
               "/foo/bar | apply {'baz':'qux'}",
               "{'foo':{'bar':22},'baz':'qux'}");
}

// Test projections
static void jql_test_1_4(void) {
  _jql_test1_3(false, "{'foo':{'bar':22}}", "/** | all", "{'foo':{'bar':22}}");
  _jql_test1_3(false, "{'foo':{'bar':22}}", "/** | all+all + all", "{'foo':{'bar':22}}");
  _jql_test1_3(true, "{'foo':{'bar':22}}", "/** | all - all", "{}");
  _jql_test1_3(true, "{'foo':{'bar':22}}", "/** | all-all +all", "{}");
  _jql_test1_3(true, "{'foo':{'bar':22}}", "/** | /foo/bar", "{'foo':{'bar':22}}");
  _jql_test1_3(true, "{'foo':{'bar':22, 'baz':'gaz'}}", "/** | /foo/bar", "{'foo':{'bar':22}}");
  _jql_test1_3(true, "{'foo':{'bar':22, 'baz':'gaz'}}", "/** | /foo/{daz,bar}", "{'foo':{'bar':22}}");
  _jql_test1_3(true, "{'foo':{'bar':22, 'baz':{'gaz':444, 'zaz':555}}}", "/** | /foo/bar + /foo/baz/zaz",
               "{'foo':{'bar':22, 'baz':{'zaz':555}}}");
  _jql_test1_3(true, "{'foo':{'bar':22, 'baz':{'gaz':444, 'zaz':555}}}", "/** | /foo/bar + /foo/baz/zaz - /*/bar",
               "{'foo':{'baz':{'zaz':555}}}");
  _jql_test1_3(true, "{'foo':{'bar':22, 'baz':{'gaz':444, 'zaz':555}}}", "/** | all + /foo/bar + /foo/baz/zaz - /*/bar",
               "{'foo':{'baz':{'zaz':555}}}");
  _jql_test1_3(true, "{'foo':{'bar':22}}", "/** | /zzz", "{}");
  _jql_test1_3(true, "{'foo':{'bar':22}}", "/** | /fooo", "{}");
  _jql_test1_3(true, "{'foo':{'bar':22},'name':'test'}", "/** | all - /name", "{'foo':{'bar':22}}");
}

// Test placeholder projecttion
static void jql_test_1_5(void) {
  JQL q = 0;
  JBL jbl = 0;
  JBL_NODE n = 0, n2 = 0;
  IWXSTR *xstr = iwxstr_new();
  IWPOOL *pool = iwpool_create_empty();
  CU_ASSERT_PTR_NOT_NULL_FATAL(pool);
  iwrc rc = jql_create(&q, "c1", "/* | /:name+/:?");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_i64(q, 0, 0, 1);
  CU_ASSERT_EQUAL(rc, JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE);

  rc = jql_set_str(q, "name", 0, "foo");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jql_set_str(q, 0, 0, "baz");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_from_json(&jbl, "{\"foo\":1,\"bar\":2,\"baz\":3}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  bool m = false;
  rc = jql_matched(q, jbl, &m);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_TRUE(m && jql_has_projection(q));

  rc = jql_apply_and_project(q, jbl, &n, 0, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  jbn_as_json(n, jbl_xstr_json_printer, xstr, 0);
  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "{\"foo\":1,\"baz\":3}");

  jql_destroy(&q);
  jbl_destroy(&jbl);
  iwpool_destroy(pool);
  iwxstr_destroy(xstr);
}

// https://github.com/Softmotions/ejdb/issues/378
static void jql_test_1_6(void) {
  JQL q = 0;
  iwrc rc = jql_create(&q, "c1", "/* | apply {\"pr\":2.2E1,\"b\":1}");
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jql_destroy(&q);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) {
    return CU_get_error();
  }
  pSuite = CU_add_suite("jql_test1", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if ( 
      (NULL == CU_add_test(pSuite, "jql_test1_1", jql_test1_1))
     || (NULL == CU_add_test(pSuite, "jql_test1_2", jql_test1_2))
     || (NULL == CU_add_test(pSuite, "jql_test1_3", jql_test1_3))
     || (NULL == CU_add_test(pSuite, "jql_test1_4", jql_test_1_4))
     || (NULL == CU_add_test(pSuite, "jql_test1_5", jql_test_1_5))
     || (NULL == CU_add_test(pSuite, "jql_test1_6", jql_test_1_6))
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
