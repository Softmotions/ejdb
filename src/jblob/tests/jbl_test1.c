#include "ejdb2.h"
#include "jbl.h"
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
  iwrc rc = jbl_ptr("/", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 1);
  CU_ASSERT_EQUAL(jp->pos, -1);
  CU_ASSERT_TRUE(*jp->n[0] == '\0')
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("/foo", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 1);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("/foo/bar", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 2);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "bar"));
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("/foo/bar/0/baz", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 4);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "bar"));
  CU_ASSERT_FALSE(strcmp(jp->n[2], "0"));
  CU_ASSERT_FALSE(strcmp(jp->n[3], "baz"));
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("/foo/b~0ar/0/b~1az", &jp);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  CU_ASSERT_EQUAL(jp->cnt, 4);
  CU_ASSERT_FALSE(strcmp(jp->n[0], "foo"));
  CU_ASSERT_FALSE(strcmp(jp->n[1], "b~ar"));
  CU_ASSERT_FALSE(strcmp(jp->n[2], "0"));
  CU_ASSERT_FALSE(strcmp(jp->n[3], "b/az"));
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("/foo/", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("//", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  jbl_ptr_destroy(&jp);
  
  rc = jbl_ptr("~", &jp);
  CU_ASSERT_EQUAL(rc, JBL_ERROR_JSON_POINTER);
  jbl_ptr_destroy(&jp);
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
  
  at = (void*)1;
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
    (NULL == CU_add_test(pSuite, "jbl_test1_3", jbl_test1_3))
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
