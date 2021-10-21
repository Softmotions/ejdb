#include "ejdb2.h"
#include <ejdb2/iowow/iwxstr.h>
#include <ejdb2/iowow/iwutils.h>

#include "jbl.h"
#include "jbl_internal.h"

#include <CUnit/Basic.h>

int init_suite(void) {
  int rc = ejdb_init();
  return rc;
}

int clean_suite(void) {
  return 0;
}

void _jbl_test1_1(int num, iwrc expected, jbl_print_flags_t pf) {
  iwrc rc;
  char path[64];
  char path_expected[64];
  JBL_NODE node = 0;
  IWPOOL *pool;
  char *data;
  char *edata = 0;
  IWXSTR *res = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(res);

  snprintf(path, sizeof(path), "data%c%03d.json", IW_PATH_CHR, num);
  snprintf(path_expected, sizeof(path_expected), "data%c%03d.expected.json", IW_PATH_CHR, num);
  data = iwu_file_read_as_buf(path);
  CU_ASSERT_PTR_NOT_NULL_FATAL(data);

  pool = iwpool_create(1024);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pool);

  rc = jbn_from_json(data, &node, pool);
  if (rc) {
    iwlog_ecode_error3(rc);
  }
  CU_ASSERT_EQUAL_FATAL(rc, expected);
  CU_ASSERT_PTR_NOT_NULL_FATAL(node);
  if (expected) {
    goto finish;
  }

  rc = jbn_as_json(node, jbl_xstr_json_printer, res, pf);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  edata = iwu_file_read_as_buf(path_expected);
  CU_ASSERT_PTR_NOT_NULL_FATAL(edata);

  FILE *f1 = fopen("f1.txt", "w");
  FILE *f2 = fopen("f2.txt", "w");
  fprintf(f1, "\n%s", edata);
  fprintf(f2, "\n%s", iwxstr_ptr(res));
  fclose(f1);
  fclose(f2);

  fprintf(stderr, "ED %s\n", edata);
  fprintf(stderr, "ED %s\n", iwxstr_ptr(res));

  CU_ASSERT_EQUAL_FATAL(strcmp(edata, iwxstr_ptr(res)), 0);

finish:
  if (edata) {
    free(edata);
  }
  free(data);
  iwpool_destroy(pool);
  iwxstr_destroy(res);
}

void jbl_test1_1() {
  _jbl_test1_1(1, 0, JBL_PRINT_PRETTY);
  _jbl_test1_1(2, 0, JBL_PRINT_PRETTY | JBL_PRINT_CODEPOINTS);
  _jbl_test1_1(3, 0, JBL_PRINT_PRETTY);
  _jbl_test1_1(4, 0, JBL_PRINT_PRETTY);
  _jbl_test1_1(5, 0, JBL_PRINT_PRETTY);
}

void jbl_test1_2() {
  const char *data = "{\"foo\": \"b\\\"ar\", \"num1\":1223,"
                     "\"n\\\"um2\":10.1226222, "
                     "\"list\":[3,2.1,1,\"one\", \"two\", "
                     "{}, {\"z\":false, \"t\":true}]}";
  JBL jbl;
  iwrc rc = jbl_from_json(&jbl, data);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, false);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  int res = strcmp(iwxstr_ptr(xstr),
                   "{\"foo\":\"b\\\"ar\",\"num1\":1223,\"n\\\"um2\":10.1226222,"
                   "\"list\":[3,2.1,1,\"one\",\"two\",{},{\"z\":false,\"t\":true}]}");
  CU_ASSERT_EQUAL(res, 0);
  jbl_destroy(&jbl);

  //
  rc = jbl_from_json(&jbl, "{ ");
  CU_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PARSE_JSON);

  iwxstr_destroy(xstr);
}

void jbl_test1_3() {
  JBL_PTR jp;
  iwrc rc = jbl_ptr_alloc("/", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 1);
  CU_ASSERT_TRUE(*jp->n[0] == '\0')
  free(jp);

  rc = jbl_ptr_alloc("/foo", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 1);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  free(jp);

  rc = jbl_ptr_alloc("/foo/bar", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 2);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "bar"));
  free(jp);

  rc = jbl_ptr_alloc("/foo/bar/0/baz", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 4);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "bar"));
  CU_ASSERT_FALSE(strcmp(jp->n[2], "0"));
  CU_ASSERT_FALSE(strcmp(jp->n[3], "baz"));
  free(jp);

  rc = jbl_ptr_alloc("/foo/b~0ar/0/b~1az", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 4);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "b~ar"));
  CU_ASSERT_FALSE(strcmp(jp->n[2], "0"));
  CU_ASSERT_FALSE(strcmp(jp->n[3], "b/az"));
  free(jp);

  rc = jbl_ptr_alloc("/foo/", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);

  rc = jbl_ptr_alloc("//", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);

  rc = jbl_ptr_alloc("", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);

  rc = jbl_ptr_alloc("~", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);
}

void jbl_test1_4() {
  //  { "foo": "bar",
  //    "foo2": {
  //      "foo3": {
  //        "foo4": "bar4"
  //      },
  //      "foo5": "bar5"
  //    },
  //    "num1": 1,
  //    "list1": ["one", "two", {"three": 3}]
  //  }
  char *data
    = iwu_replace_char(
        strdup("{'foo':'bar','foo2':{'foo3':{'foo4':'bar4'},'foo5':'bar5'},"
               "'num1':1,'list1':['one','two',{'three':3}]}"),
        '\'', '"');
  JBL jbl, at, at2;
  const char *sval;
  int ival;
  iwrc rc = jbl_from_json(&jbl, data);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_at(jbl, "/foo", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);

  sval = jbl_get_str(at);
  CU_ASSERT_PTR_NOT_NULL_FATAL(sval);
  CU_ASSERT_STRING_EQUAL(sval, "bar");
  jbl_destroy(&at);

  rc = jbl_at(jbl, "/foo2/foo3", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  CU_ASSERT_TRUE(at->bn.type == BINN_OBJECT);
  rc = jbl_at(at, "/foo4", &at2);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at2);
  sval = jbl_get_str(at2);
  CU_ASSERT_PTR_NOT_NULL_FATAL(sval);
  CU_ASSERT_STRING_EQUAL(sval, "bar4");
  jbl_destroy(&at2);
  jbl_destroy(&at);

  at = (void*) 1;
  rc = jbl_at(jbl, "/foo2/foo10", &at);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_PATH_NOTFOUND);
  CU_ASSERT_PTR_NULL(at);
  rc = 0;

  rc = jbl_at(jbl, "/foo2/*/foo4", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  sval = jbl_get_str(at);
  CU_ASSERT_PTR_NOT_NULL_FATAL(sval);
  CU_ASSERT_STRING_EQUAL(sval, "bar4");
  jbl_destroy(&at);

  rc = jbl_at(jbl, "/list1/1", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  sval = jbl_get_str(at);
  CU_ASSERT_STRING_EQUAL(sval, "two");
  jbl_destroy(&at);

  rc = jbl_at(jbl, "/list1/2/three", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  ival = jbl_get_i32(at);
  CU_ASSERT_EQUAL(ival, 3);
  jbl_destroy(&at);

  rc = jbl_at(jbl, "/list1/*/three", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  ival = jbl_get_i32(at);
  CU_ASSERT_EQUAL(ival, 3);
  jbl_destroy(&at);

  rc = jbl_at(jbl, "/list1/*/*", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  ival = jbl_get_i32(at);
  CU_ASSERT_EQUAL(ival, 3);
  jbl_destroy(&at);

  jbl_destroy(&jbl);
  free(data);
}

void jbl_test1_5() {
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  //  { "foo": "bar",
  //    "foo2": {
  //      "foo3": {
  //        "foo4": "bar4"
  //      },
  //      "foo5": "bar5"
  //    },
  //    "num1": 1,
  //    "list1": ["one", "two", {"three": 3}]
  //  }
  char *data
    = iwu_replace_char(
        strdup("{'foo':'bar','foo2':{'foo3':{'foo4':'bar4'},'foo5':'bar5'},"
               "'num1':1,'list1':['one','two',{'three':3}]}"),
        '\'', '"');
  JBL jbl;
  int res = 0;

  // Remove ROOT
  JBL_PATCH p1[] = { { .op = JBP_REMOVE, .path = "/" } };
  iwrc rc = jbl_from_json(&jbl, data);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_patch(jbl, p1, sizeof(p1) / sizeof(p1[0]));
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&jbl);


  // Remove "/foo"
  JBL_PATCH p2[] = { { .op = JBP_REMOVE, .path = "/foo" } };
  rc = jbl_from_json(&jbl, data);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_patch(jbl, p2, sizeof(p2) / sizeof(p2[0]));
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, false);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  res = strcmp(iwxstr_ptr(
                 xstr),
               "{\"foo2\":{\"foo3\":{\"foo4\":\"bar4\"},\"foo5\":\"bar5\"},\"num1\":1,\"list1\":[\"one\",\"two\",{\"three\":3}]}");
  CU_ASSERT_EQUAL(res, 0);
  jbl_destroy(&jbl);
  iwxstr_clear(xstr);

  // Remove /foo2/foo3/foo4
  // Remove /list1/1
  JBL_PATCH p3[] = {
    { .op = JBP_REMOVE, .path = "/foo2/foo3/foo4" },
    { .op = JBP_REMOVE, .path = "/list1/1"        }
  };
  rc = jbl_from_json(&jbl, data);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_patch(jbl, p3, sizeof(p3) / sizeof(p3[0]));
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, false);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  res = strcmp(iwxstr_ptr(xstr),
               "{\"foo\":\"bar\",\"foo2\":{\"foo3\":{},\"foo5\":\"bar5\"},\"num1\":1,\"list1\":[\"one\",{\"three\":3}]}");
  CU_ASSERT_EQUAL(res, 0);
  jbl_destroy(&jbl);
  iwxstr_clear(xstr);
  iwxstr_destroy(xstr);
  free(data);
}

void apply_patch(const char *data, const char *patch, const char *result, IWXSTR *xstr, iwrc *rcp) {
  CU_ASSERT_TRUE_FATAL(data && patch && xstr && rcp);
  JBL jbl = 0;
  char *data2 = iwu_replace_char(strdup(data), '\'', '"');
  char *patch2 = iwu_replace_char(strdup(patch), '\'', '"');
  char *result2 = result ? iwu_replace_char(strdup(result), '\'', '"') : 0;
  CU_ASSERT_TRUE_FATAL(data2 && patch2);

  iwrc rc = jbl_from_json(&jbl, data2);
  RCGO(rc, finish);

  rc = jbl_patch_from_json(jbl, patch2);
  RCGO(rc, finish);

  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, false);
  RCGO(rc, finish);

  if (result2) {
    CU_ASSERT_STRING_EQUAL(result2, iwxstr_ptr(xstr));
  }

finish:
  if (data2) {
    free(data2);
  }
  if (patch2) {
    free(patch2);
  }
  if (result2) {
    free(result2);
  }
  if (jbl) {
    jbl_destroy(&jbl);
  }
  *rcp = rc;
}

void apply_merge_patch(const char *data, const char *patch, const char *result, IWXSTR *xstr, iwrc *rcp) {
  CU_ASSERT_TRUE_FATAL(data && patch && xstr && rcp);
  JBL jbl = 0;
  char *data2 = iwu_replace_char(strdup(data), '\'', '"');
  char *patch2 = iwu_replace_char(strdup(patch), '\'', '"');
  char *result2 = result ? iwu_replace_char(strdup(result), '\'', '"') : 0;
  CU_ASSERT_TRUE_FATAL(data2 && patch2);

  iwrc rc = jbl_from_json(&jbl, data2);
  RCGO(rc, finish);

  rc = jbl_merge_patch(jbl, patch2);
  RCGO(rc, finish);

  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, false);
  RCGO(rc, finish);

  if (result2) {
    CU_ASSERT_STRING_EQUAL(result2, iwxstr_ptr(xstr));
  }

finish:
  if (data2) {
    free(data2);
  }
  if (patch2) {
    free(patch2);
  }
  if (result2) {
    free(result2);
  }
  if (jbl) {
    jbl_destroy(&jbl);
  }
  *rcp = rc;
}

// Run tests: https://github.com/json-patch/json-patch-tests/blob/master/spec_tests.json
void jbl_test1_6() {
  iwrc rc;
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  apply_patch("{'foo':'bar','foo2':{'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
              "[{'op':'remove', 'path':'/foo'}]",
              "{'foo2':{'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
              xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // 4.1. add with missing object
  apply_patch("{ 'q': { 'bar': 2 } }",
              "[ {'op': 'add', 'path': '/a/b', 'value': 1} ]",
              0, xstr, &rc);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_PATCH_TARGET_INVALID);
  iwxstr_clear(xstr);

  // A.1.  Adding an Object Member
  apply_patch("{'foo': 'bar'}",
              "[ { 'op': 'add', 'path': '/baz', 'value': 'qux' } ]",
              "{'foo':'bar','baz':'qux'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.2.  Adding an Array Element
  apply_patch("{'foo': [ 'bar', 'baz' ]}",
              "[{ 'op': 'add', 'path': '/foo/1', 'value': 'qux' }]",
              "{'foo':['bar','qux','baz']}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.3.  Removing an Object Member
  apply_patch("{'baz': 'qux','foo': 'bar'}",
              "[{ 'op': 'remove', 'path': '/baz' }]",
              "{'foo':'bar'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.4.  Removing an Array Element
  apply_patch("{'foo': [ 'bar', 'qux', 'baz' ]}",
              "[{ 'op': 'remove', 'path': '/foo/1' }]",
              "{'foo':['bar','baz']}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.5.  Replacing a Value
  apply_patch("{'baz': 'qux','foo': 'bar'}",
              "[{ 'op': 'replace', 'path': '/baz', 'value': 'boo' }]",
              "{'foo':'bar','baz':'boo'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.5.1 #232
  apply_patch("{'a':{'c':'N','s':'F'}}",
              "[{'op':'replace', 'path':'/a/s', 'value':'A'}]",
              "{'a':{'c':'N','s':'A'}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.6.  Moving a Value
  apply_patch("{'foo': {'bar': 'baz','waldo': 'fred'},'qux': {'corge': 'grault'}}",
              "[{ 'op': 'move', 'from': '/foo/waldo', 'path': '/qux/thud' }]",
              "{'foo':{'bar':'baz'},'qux':{'corge':'grault','thud':'fred'}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.7.  Moving an Array Element
  apply_patch("{'foo': [ 'all', 'grass', 'cows', 'eat' ]}",
              "[{ 'op': 'move', 'from': '/foo/1', 'path': '/foo/3' }]",
              "{'foo':['all','cows','eat','grass']}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.8.  Testing a Value: Success
  apply_patch("{'baz': 'qux','foo': [ 'a', 2, 'c' ]}",
              "["
              "{ 'op': 'test', 'path': '/baz', 'value': 'qux' },"
              "{ 'op': 'test', 'path': '/foo/1', 'value': 2 }"
              "]",
              "{'baz':'qux','foo':['a',2,'c']}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.8.  Testing a Value Object
  apply_patch(
    "{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
    "[{ 'op': 'test', 'path': '/foo2', 'value': {'foo5':'bar5', 'zaz':25, 'foo3':{'foo4':'bar4'}} }]",
    0,
    xstr,
    &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_patch(
    "{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
    "[{ 'op': 'test', 'path': '/foo2', 'value': {'foo5':'bar5', 'zaz':25, 'foo3':{'foo41':'bar4'}} }]",
    0,
    xstr,
    &rc);
  CU_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATCH_TEST_FAILED);
  iwxstr_clear(xstr);

  apply_patch(
    "{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
    "[{ 'op': 'test', 'path': '/', 'value': {'num1':1, 'foo2':{'foo3':{'foo4':'bar4'}, 'zaz':25, 'foo5':'bar5'},'list1':['one','two',{'three':3}],'foo':'bar'} }]",
    0,
    xstr,
    &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_patch(
    "{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
    "[{ 'op': 'test', 'path': '/list1', 'value':['one','two',{'three':3}] }]",
    0,
    xstr,
    &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_patch(
    "{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
    "[{ 'op': 'test', 'path': '/list1', 'value':['two','one',{'three':3}] }]",
    0,
    xstr,
    &rc);
  CU_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATCH_TEST_FAILED);
  iwxstr_clear(xstr);

  // A.9.  Testing a Value: Error
  apply_patch("{ 'baz': 'qux'}",
              "[{ 'op': 'test', 'path': '/baz', 'value': 'bar' }]",
              0, xstr, &rc);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_PATCH_TEST_FAILED);
  iwxstr_clear(xstr);

  // A.10.  Adding a nested Member Object
  apply_patch("{'foo': 'bar'}",
              "[{ 'op': 'add', 'path': '/child', 'value': { 'grandchild': { } } }]",
              "{'foo':'bar','child':{'grandchild':{}}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.11.  Ignoring Unrecognized Elements
  apply_patch("{'foo': 'bar'}",
              "[{ 'op': 'add', 'path': '/baz', 'value': 'qux', 'xyz': 123 }]",
              "{'foo':'bar','baz':'qux'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.12.  Adding to a Non-existent Target
  apply_patch("{'foo': 'bar'}",
              "[{ 'op': 'add', 'path': '/baz/bat', 'value': 'qux' }]",
              0, xstr, &rc);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_PATCH_TARGET_INVALID);
  iwxstr_clear(xstr);

  // A.14. ~ Escape Ordering
  apply_patch("{'/': 9,'~1': 10}",
              "[{'op': 'test', 'path': '/~01', 'value': 10}]",
              "{'/':9,'~1':10}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // A.15. Comparing Strings and Numbers
  apply_patch("{'/': 9,'~1': 10}",
              "[{'op': 'test', 'path': '/~01', 'value': '10'}]",
              "{'/':9,'~1':10}", xstr, &rc);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_PATCH_TEST_FAILED);
  iwxstr_clear(xstr);

  // A.16. Adding an Array Value
  apply_patch("{'foo': ['bar']}",
              "[{'op': 'add', 'path': '/foo/-', 'value': ['abc', 'def'] }]",
              "{'foo':['bar',['abc','def']]}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // Apply non standard `increment` patch
  apply_patch("{'foo': 1}",
              "[{'op': 'increment', 'path': '/foo', 'value': 2}]",
              "{'foo':3}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  // Apply non standard `swap` patch
  apply_patch("{'foo': ['bar'], 'baz': {'gaz': 11}}",
              "[{'op': 'swap', 'from': '/foo/0', 'path': '/baz/gaz'}]",
              "{'foo':[11],'baz':{'gaz':'bar'}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_patch("{'foo': ['bar'], 'baz': {'gaz': 11}}",
              "[{'op': 'swap', 'from': '/foo/0', 'path': '/baz/zaz'}]",
              "{'foo':[],'baz':{'gaz':11,'zaz':'bar'}}", xstr, &rc);

  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_patch("{'foo': 1}",
              "[{'op': 'increment', 'path': '/foo', 'value': true}]",
              "{'foo':3}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATCH_INVALID_VALUE);
  iwxstr_clear(xstr);

  // Apply non standard add_create patch
  apply_patch("{'foo': {'bar': 1}}",
              "[{'op': 'add_create', 'path': '/foo/zaz/gaz', 'value': 22}]",
              "{'foo':{'bar':1,'zaz':{'gaz':22}}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_patch("{'foo': {'bar': 1}}",
              "[{'op': 'add_create', 'path': '/foo/bar/gaz', 'value': 22}]",
              "{}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATCH_TARGET_INVALID);
  iwxstr_clear(xstr);

  apply_patch("{'foo': {'bar': 1}}",
              "[{'op': 'add_create', 'path': '/zaz/gaz', 'value': [1,2,3]}]",
              "{'foo':{'bar':1},'zaz':{'gaz':[1,2,3]}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  iwxstr_destroy(xstr);
}

void jbl_test1_7() {
  iwrc rc;
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  // #233
  apply_merge_patch("{'n':'nv'}",
                    "{'a':{'c':'v','d':'k'}}",
                    "{'n':'nv','a':{'c':'v','d':'k'}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':'b'}",
                    "{'a':'c'}",
                    "{'a':'c'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);


  apply_merge_patch("{'a':'b'}",
                    "{'b':'c'}",
                    "{'a':'b','b':'c'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':'b'}",
                    "{'a':null}",
                    "{}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':'b','b':'c'}",
                    "{'a':null}",
                    "{'b':'c'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':['b']}",
                    "{'a':'c'}",
                    "{'a':'c'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':'c'}",
                    "{'a':['b']}",
                    "{'a':['b']}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':{'b':'c'}}",
                    "{'a':{'b':'d','c':null}}",
                    "{'a':{'b':'d'}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':[{'b':'c'}]}",
                    "{'a':[1]}",
                    "{'a':[1]}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("['a','b']",
                    "['c','d']",
                    "['c','d']", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'a':'b'}",
                    "['c']",
                    "['c']", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{'e':null}",
                    "{'a':1}",
                    "{'e':null,'a':1}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("[1,2]",
                    "{'a':'b','c':null}",
                    "{'a':'b'}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);

  apply_merge_patch("{}",
                    "{'a':{'bb':{'ccc':null}}}",
                    "{'a':{'bb':{}}}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);


  iwxstr_destroy(xstr);
}

void jbl_test1_8() {
  JBL jbl, nested, at;
  iwrc rc = jbl_create_empty_object(&jbl);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_create_empty_object(&nested);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_set_int64(nested, "nnum", 2233);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_set_int64(jbl, "mynum", 13223);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_set_string(jbl, "foo", "bar");
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_set_nested(jbl, "nested", nested);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbl_at(jbl, "/mynum", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  CU_ASSERT_EQUAL(jbl_get_i64(at), 13223);
  jbl_destroy(&at);

  rc = jbl_at(jbl, "/foo", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  CU_ASSERT_STRING_EQUAL(jbl_get_str(at), "bar");
  jbl_destroy(&at);

  rc = jbl_at(jbl, "/nested/nnum", &at);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_PTR_NOT_NULL_FATAL(at);
  CU_ASSERT_EQUAL(jbl_get_i64(at), 2233);
  jbl_destroy(&at);

  jbl_destroy(&jbl);
  jbl_destroy(&nested);
}

void jbl_test1_9(void) {
  IWPOOL *pool = iwpool_create(512);
  IWPOOL *cpool = iwpool_create(512);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pool);
  CU_ASSERT_PTR_NOT_NULL_FATAL(cpool);
  const char *data = "{\"foo\": \"b\\\"ar\", \"num1\":1223,"
                     "\"n\\\"um2\":10.1226222, "
                     "\"list\":[3,2.1,1,\"one\" \"two\", "
                     "{}, {\"z\":false, \"arr\":[9,8], \"t\":true}]}";

  JBL_NODE n, cn;
  iwrc rc = jbn_from_json(data, &n, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_clone(n, &cn, cpool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);

  iwpool_destroy(pool);

  rc = jbn_as_json(cn, jbl_xstr_json_printer, xstr, 0);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(
                           xstr),
                         "{\"foo\":\"b\\\"ar\",\"num1\":1223,\"n\\\"um2\":10.1226222,\"list\":[3,2.1,1,\"one\",\"two\",{},{\"z\":false,\"arr\":[9,8],\"t\":true}]}"
                         );

  iwpool_destroy(cpool);
  iwxstr_destroy(xstr);
}

void jbl_test1_10(void) {
  IWPOOL *pool = iwpool_create(512);
  IWPOOL *tpool = iwpool_create(512);
  IWXSTR *xstr = iwxstr_new();

  const char *src_data = "{\"foo\": \"b\\\"ar\", \"num1\":1223,"
                         "\"n\\\"um2\":10.1226222, "
                         "\"list\":[3,2.1,1,\"one\" \"two\", "
                         "{}, {\"z\":false, \"arr\":[9,8], \"t\":true}]}";
  const char *tgt_data = "{\"test\":{\"nested1\":22}}";
  JBL_NODE n1, n2;
  iwrc rc = jbn_from_json(src_data, &n1, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_from_json(tgt_data, &n2, tpool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_copy_path(n1, "/list/6/arr", n2, "/test/nested1", false, false, tpool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_copy_path(n1, "/list/6/t", n2, "/test/t2", false, false, tpool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_copy_path(n1, "/foo", n2, "/bar", false, false, tpool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  iwpool_destroy(pool);

  rc = jbn_as_json(n2, jbl_xstr_json_printer, xstr, 0);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr),
                         "{\"test\":{\"nested1\":[9,8],\"t2\":true},\"bar\":\"b\\\"ar\"}");

  iwpool_destroy(tpool);
  iwxstr_destroy(xstr);
}

void jbl_test1_11(void) {
  IWPOOL *pool = iwpool_create(512);
  IWXSTR *xstr = iwxstr_new();

  const char *src_data = "{\"foo\": \"b\\\"ar\", \"num1\":1223,"
                         "\"n\\\"um2\":10.1226222, "
                         "\"list\":[3,2.1,1,\"one\" \"two\", "
                         "{}, {\"z\":false, \"arr\":[9,8], \"t\":true}]}";
  const char *tgt_data = "{\"test\":{\"nested1\":22}, \"list\":[0,99]}";

  JBL_NODE n1, n2;
  iwrc rc = jbn_from_json(src_data, &n1, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_from_json(tgt_data, &n2, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  const char *paths[] = { "/foo", "/list/1", 0 };
  rc = jbn_copy_paths(n1, n2, paths, false, false, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  rc = jbn_as_json(n2, jbl_xstr_json_printer, xstr, 0);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr),
                         "{\"test\":{\"nested1\":22},\"list\":[0,2.1],\"foo\":\"b\\\"ar\"}");

  iwpool_destroy(pool);
  iwxstr_destroy(xstr);
}

void jbl_test1_12(void) {
  IWPOOL *pool = iwpool_create_empty();
  JBL_NODE n;
  iwrc rc = jbn_from_json("{\"foo\":1.1}", &n, pool);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwpool_destroy(pool);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) {
    return CU_get_error();
  }
  pSuite = CU_add_suite("jbl_test1", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (  (NULL == CU_add_test(pSuite, "jbl_test1_1", jbl_test1_1))
     || (NULL == CU_add_test(pSuite, "jbl_test1_2", jbl_test1_2))
     || (NULL == CU_add_test(pSuite, "jbl_test1_3", jbl_test1_3))
     || (NULL == CU_add_test(pSuite, "jbl_test1_4", jbl_test1_4))
     || (NULL == CU_add_test(pSuite, "jbl_test1_5", jbl_test1_5))
     || (NULL == CU_add_test(pSuite, "jbl_test1_6", jbl_test1_6))
     || (NULL == CU_add_test(pSuite, "jbl_test1_7", jbl_test1_7))
     || (NULL == CU_add_test(pSuite, "jbl_test1_8", jbl_test1_8))
     || (NULL == CU_add_test(pSuite, "jbl_test1_9", jbl_test1_9))
     || (NULL == CU_add_test(pSuite, "jbl_test1_10", jbl_test1_10))
     || (NULL == CU_add_test(pSuite, "jbl_test1_11", jbl_test1_11))
     || (NULL == CU_add_test(pSuite, "jbl_test1_12", jbl_test1_12))
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
