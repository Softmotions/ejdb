#include "ejdb2.h"
#include "jbl.h"
#include "jbldom.h"
#include <iowow/iwxstr.h>
#include <iowow/iwutils.h>
#include <CUnit/Basic.h>
#include "jbl_internal.h"
#include <stdlib.h>

int init_suite(void) {
  int rc = ejdb2_init();
  return rc;
}

int clean_suite(void) {
  return 0;
}

void jbl_test1_1() {
  const char *data = "{\"foo\": \"b\\\"ar\", \"num1\":1223,"
                     "\"n\\\"um2\":10.1226222, "
                     "\"list\":[3,2.1,1,\"one\", \"two\", "
                     "{}, {\"z\":false, \"t\":true}]}";
  JBL jbl;
  iwrc rc = jbl_from_json(&jbl, data) ;
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  
  IWXSTR *xstr = iwxstr_new();
  CU_ASSERT_PTR_NOT_NULL_FATAL(xstr);
  
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, false);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  
  int res = strcmp(iwxstr_ptr(xstr),
                   "{\"foo\":\"b\\\"ar\",\"num1\":1223,\"n\\\"um2\":10.1226222,"
                   "\"list\":[3,2.1,1,\"one\",\"two\",{},{\"z\":false,\"t\":true}]}");
  CU_ASSERT_EQUAL(res, 0);
  
  iwxstr_destroy(xstr);
  jbl_destroy(&jbl);
}

void jbl_test1_2() {
  JBLPTR jp;
  iwrc rc = _jbl_ptr_malloc("/", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 1);
  CU_ASSERT_EQUAL(jp->pos, -1);
  CU_ASSERT_TRUE(*jp->n[0] == '\0')
  free(jp);
  
  rc = _jbl_ptr_malloc("/foo", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 1);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  free(jp);
  
  rc = _jbl_ptr_malloc("/foo/bar", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 2);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "bar"));
  free(jp);
  
  rc = _jbl_ptr_malloc("/foo/bar/0/baz", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 4);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "bar"));
  CU_ASSERT_FALSE(strcmp(jp->n[2], "0"));
  CU_ASSERT_FALSE(strcmp(jp->n[3], "baz"));
  free(jp);
  
  rc = _jbl_ptr_malloc("/foo/b~0ar/0/b~1az", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 4);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "b~ar"));
  CU_ASSERT_FALSE(strcmp(jp->n[2], "0"));
  CU_ASSERT_FALSE(strcmp(jp->n[3], "b/az"));
  free(jp);
  
  rc = _jbl_ptr_malloc("/foo/", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);
  
  rc = _jbl_ptr_malloc("//", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);
  
  rc = _jbl_ptr_malloc("", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);
  
  rc = _jbl_ptr_malloc("~", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  free(jp);
}

void jbl_test1_3() {
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
  char *data =
    iwu_replace_char(
      strdup("{'foo':'bar','foo2':{'foo3':{'foo4':'bar4'},'foo5':'bar5'},"
             "'num1':1,'list1':['one','two',{'three':3}]}"),
      '\'', '"');
  JBL jbl, at, at2;
  char *sval;
  int ival;
  iwrc rc = jbl_from_json(&jbl, data) ;
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
  
  at = (void *)1;
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

void jbl_test1_4() {
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
  char *data =
    iwu_replace_char(
      strdup("{'foo':'bar','foo2':{'foo3':{'foo4':'bar4'},'foo5':'bar5'},"
             "'num1':1,'list1':['one','two',{'three':3}]}"),
      '\'', '"');
  JBL jbl;
  int res = 0;
  
  // Remove ROOT
  JBLPATCH p1[] = {{.op = JBP_REMOVE, .path = "/"}};
  iwrc rc = jbl_from_json(&jbl, data) ;
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_patch2(jbl, p1, sizeof(p1) / sizeof(p1[0]));
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  jbl_destroy(&jbl);
  
  
  // Remove "/foo"
  JBLPATCH p2[] = {{.op = JBP_REMOVE, .path = "/foo"}};
  rc = jbl_from_json(&jbl, data) ;
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_patch2(jbl, p2, sizeof(p2) / sizeof(p2[0]));
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, false);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  res = strcmp(iwxstr_ptr(xstr),
               "{\"foo2\":{\"foo3\":{\"foo4\":\"bar4\"},\"foo5\":\"bar5\"},\"num1\":1,\"list1\":[\"one\",\"two\",{\"three\":3}]}");
  CU_ASSERT_EQUAL(res, 0);
  jbl_destroy(&jbl);
  iwxstr_clear(xstr);
  
  // Remove /foo2/foo3/foo4
  // Remove /list1/1
  JBLPATCH p3[] = {
    {.op = JBP_REMOVE, .path = "/foo2/foo3/foo4"},
    {.op = JBP_REMOVE, .path = "/list1/1"}
  };
  rc = jbl_from_json(&jbl, data) ;
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  rc = jbl_patch2(jbl, p3, sizeof(p3) / sizeof(p3[0]));
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
  
  iwrc rc = jbl_from_json(&jbl, data2) ;
  RCGO(rc, finish);
  
  rc = jbl_patch3(jbl, patch2);
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
void jbl_test1_5() {
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
  apply_patch("{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
              "[{ 'op': 'test', 'path': '/foo2', 'value': {'foo5':'bar5', 'zaz':25, 'foo3':{'foo4':'bar4'}} }]",
              0, xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);
  
  apply_patch("{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
              "[{ 'op': 'test', 'path': '/foo2', 'value': {'foo5':'bar5', 'zaz':25, 'foo3':{'foo41':'bar4'}} }]",
              0, xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, JBL_ERROR_PATCH_TEST_FAILED);
  iwxstr_clear(xstr);
  
  apply_patch("{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
              "[{ 'op': 'test', 'path': '/', 'value': {'num1':1, 'foo2':{'foo3':{'foo4':'bar4'}, 'zaz':25, 'foo5':'bar5'},'list1':['one','two',{'three':3}],'foo':'bar'} }]",
              0, xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);
  
  apply_patch("{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
              "[{ 'op': 'test', 'path': '/list1', 'value':['one','two',{'three':3}] }]",
              0, xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);
  
  apply_patch("{'foo':'bar','foo2':{'zaz':25, 'foo3':{'foo4':'bar4'},'foo5':'bar5'},'num1':1,'list1':['one','two',{'three':3}]}",
              "[{ 'op': 'test', 'path': '/list1', 'value':['two','one',{'three':3}] }]",
              0, xstr, &rc);
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
  //  apply_patch("'/': 9,'~1': 10",
  //              "[{'op': 'test', 'path': '/~01', 'value': 10}]",
  //              0, xstr, &rc);
  //  printf("\n%s\n", iwxstr_ptr(xstr));
  //  iwxstr_clear(xstr);
  
  
  // A.16. Adding an Array Value
  apply_patch("{'foo': ['bar']}",
              "[{ 'op': 'add', 'path': '/foo/-', 'value': ['abc', 'def'] }]",
              "{'foo':['bar',['abc','def']]}", xstr, &rc);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  iwxstr_clear(xstr);
  
  //  printf("\n%s\n", iwxstr_ptr(xstr));
  //  iwxstr_clear(xstr);
  
  iwxstr_destroy(xstr);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
  pSuite = CU_add_suite("jbl_test1", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (
    (NULL == CU_add_test(pSuite, "jbl_test1_1", jbl_test1_1)) ||
    (NULL == CU_add_test(pSuite, "jbl_test1_2", jbl_test1_2)) ||
    (NULL == CU_add_test(pSuite, "jbl_test1_3", jbl_test1_3)) ||
    (NULL == CU_add_test(pSuite, "jbl_test1_4", jbl_test1_4)) ||
    (NULL == CU_add_test(pSuite, "jbl_test1_5", jbl_test1_5))
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
