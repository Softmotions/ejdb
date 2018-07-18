#include "ejdb2.h"
#include "jqp.h"
#include <iowow/iwxstr.h>
#include <iowow/iwutils.h>
#include <CUnit/Basic.h>
#include <stdlib.h>

int init_suite(void) {
  int rc = ejdb2_init();
  return rc;
}

int clean_suite(void) {
  return 0;
}

void _jql_test1_1(int num, iwrc expected) {
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
  if (expected) goto finish;
  
  CU_ASSERT_PTR_NOT_NULL_FATAL(aux->query);
  
  
  edata = iwu_file_read_as_buf(path_expected);
  CU_ASSERT_PTR_NOT_NULL_FATAL(data);
  rc = jqp_print_query(aux->query, jbl_xstr_json_printer, res);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
  
//  fprintf(stderr, "%s\n", edata);
//  fprintf(stderr, "%s\n", iwxstr_ptr(res));
//  fprintf(stderr, "%d\n", strcmp(edata, iwxstr_ptr(res)));
//  FILE *out = fopen("out.txt", "w+");
//  fprintf(out, "%s", iwxstr_ptr(res));
//  fclose(out);

  
  CU_ASSERT_EQUAL(strcmp(edata, iwxstr_ptr(res)), 0);
  
finish:
  if (edata) {
    free(edata);
  }
  free(data);
  iwxstr_destroy(res);
  jqp_aux_destroy(&aux);
}

void jql_test1() {
  for (int i = 1; i <= 10; ++i) {
    _jql_test1_1(i, 0);
  }
  for (int i = 11; i <= 13; ++i) {
    _jql_test1_1(i, JQL_ERROR_QUERY_PARSE);
  }
  for (int i = 14; i <= 16; ++i) {
     _jql_test1_1(i, 0);
  }
}

void jql_test1_2() {
  JBL jbl;
  JQL jql;  
  char *json = iwu_replace_char(strdup("{'foo':{'bar':22}}"), '\'', '"');
  CU_ASSERT_PTR_NOT_NULL_FATAL(json);
  iwrc rc = jql_create(&jql, "/foo/bar");  
  CU_ASSERT_EQUAL_FATAL(rc, 0);  
  rc = jbl_from_json(&jbl, json);
  CU_ASSERT_EQUAL_FATAL(rc, 0);  
  bool m = false;
  rc = jql_matched(jql, jbl, &m);
  CU_ASSERT_EQUAL_FATAL(rc, 0);  
  CU_ASSERT_EQUAL(m, true);
    
  jql_destroy(&jql);
  jbl_destroy(&jbl);
  free(json);
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
    (NULL == CU_add_test(pSuite, "jql_test1_1", jql_test1)) ||
    (NULL == CU_add_test(pSuite, "jql_test1_2", jql_test1_2))
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
