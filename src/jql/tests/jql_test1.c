#include "ejdb2.h"
#include "jqp.h"
#include "jbl_internal.h"
#include <CUnit/Basic.h>

int init_suite(void) {
  int rc = ejdb2_init();
  return rc;
}

int clean_suite(void) {
  return 0;
}

void jql_test1() {
  printf("\nJQ parser\n");
  JPQDATA *res = 0;
  JPQAUX aux = {
    .line = 1,
    .col = 1,
    //.buf = "/foo/bar"
    //.buf = "/\"foo\"/\"b ar\""
    //.buf = "/foo and /bar"
    //.buf = "/foo/[bar = \"val\"]"    
    //.buf = "/foo/[barlike22]" err
    //.buf = "/foo/[bar like 33]"
    //.buf = "/foo/[bar not like 33]"
    //.buf = "/foo/[bar=33 and zzz like \"te?s*t\"]"
    //.buf = "/foo/[bar = :placeholder]"
    //.buf = "/foo/[bar = :? and \"baz\" = :?] or /root/**/[fname not like \"John\"]"
    //.buf = "/foo/[bar = []]"
    //.buf = "/foo/[bar = [1, 2,3,{},{\"foo\": \"bar\"}]]"
    //.buf = "/foo/[bar = {}]"
    //.buf = "/tags/[* in [\"sample\", \"foo\"] and * like \"ta*\"]"
    //.buf = "/**/[[* = \"familyName\"] = \"Doe\"]"
    .buf = "@one/**/[familyName like \"D\\n*\"] and /**/family/motnher/[age > 30] not /bar/\"ba z\\\"zz\" | apply {} | all - /**/author/{givenName,familyName}"
  };
  jqp_context_t *ctx = jqp_create(&aux);
  CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);
  int rsz = jqp_parse(ctx, &res);  
  jqp_destroy(ctx);  
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
  pSuite = CU_add_suite("jql_test1", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (
    (NULL == CU_add_test(pSuite, "jql_test1_1", jql_test1))
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
