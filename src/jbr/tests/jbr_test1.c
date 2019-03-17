#include "ejdb_test.h"
#include <CUnit/Basic.h>
#include <curl/curl.h>

CURL *curl;

int init_suite() {
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  if (!curl) return 1;
  int rc = ejdb_init();
  return rc;
}

int clean_suite() {
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return 0;
}

static size_t curl_write_xstr(void *contents, size_t size, size_t nmemb, void *op) {
  IWXSTR *xstr = op;
  assert(xstr);
  iwxstr_cat(xstr, contents, size * nmemb);
  return size * nmemb;
}

static void jbr_test1_1() {
  EJDB_OPTS opts = {
    .kv = {
      .path = "jbr_test1_1.db",
      .oflags = IWKV_TRUNC
    },
    .no_wal = true,
    .http = {
      .enabled = true,
      .port = 9292
    }
  };

  long code;
  EJDB db;
  iwrc rc = ejdb_open(&opts, &db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);

  IWXSTR *xstr = iwxstr_new();
  IWXSTR *hstr = iwxstr_new();
  struct curl_slist *headers = curl_slist_append(0, "Content-Type: application/json");


  // Check no element in collection
  CURLcode cc = curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9292/c1/1");
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  cc = curl_easy_perform(curl);
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  CU_ASSERT_EQUAL_FATAL(code, 404);

  // Save a document using POST
  curl_easy_reset(curl);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9292/c1");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"foo\":\"bar\"}");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, xstr);
  cc = curl_easy_perform(curl);
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  CU_ASSERT_EQUAL_FATAL(code, 200);
  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr), "1");

  // Now get document JSON using GET
  curl_easy_reset(curl);
  iwxstr_clear(xstr);
  iwxstr_clear(hstr);
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9292/c1/1");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, hstr);
  cc = curl_easy_perform(curl);
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  CU_ASSERT_EQUAL_FATAL(code, 200);
  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr),
                         "{\n"
                         " \"foo\": \"bar\"\n"
                         "}"
                        );
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(hstr), "content-type:application/json"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(hstr), "content-length:17"));
  CU_ASSERT_EQUAL(iwxstr_size(xstr), 17);

  // PUT document under specific ID
  curl_easy_reset(curl);
  iwxstr_clear(xstr);
  iwxstr_clear(hstr);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9292/c1/33");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"foo\":\"b\nar\"}");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, xstr);
  cc = curl_easy_perform(curl);
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  CU_ASSERT_EQUAL_FATAL(code, 200);

  // Check last doc

  curl_easy_reset(curl);
  iwxstr_clear(xstr);
  iwxstr_clear(hstr);
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9292/c1/33");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, hstr);
  cc = curl_easy_perform(curl);
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  CU_ASSERT_EQUAL_FATAL(code, 200);
  CU_ASSERT_STRING_EQUAL(iwxstr_ptr(xstr),
                         "{\n"
                         " \"foo\": \"b\\nar\"\n"
                         "}"
                        );
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(hstr), "content-type:application/json"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(hstr), "content-length:19"));
  CU_ASSERT_EQUAL(iwxstr_size(xstr), 19);

  // Perform a query
  curl_easy_reset(curl);
  iwxstr_clear(xstr);
  iwxstr_clear(hstr);
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9292/");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "@c1/foo");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, hstr);
  cc = curl_easy_perform(curl);
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  CU_ASSERT_EQUAL_FATAL(code, 200);

  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(xstr), "33\t{\"foo\":\"b\\nar\"}"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(xstr), "1\t{\"foo\":\"bar\"}"));


  // Query with explain
  curl_slist_free_all(headers);
  curl_easy_reset(curl);
  iwxstr_clear(xstr);
  iwxstr_clear(hstr);
  headers = curl_slist_append(0, "X-Hints: explain");
  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9292/");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "@c1/foo");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_write_xstr);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, hstr);
  cc = curl_easy_perform(curl);
  CU_ASSERT_EQUAL_FATAL(cc, 0);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  CU_ASSERT_EQUAL_FATAL(code, 200);
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(xstr), "[INDEX] NO [COLLECTOR] PLAIN"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(xstr), "--------------------"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(xstr), "33\t{\"foo\":\"b\\nar\"}"));
  CU_ASSERT_PTR_NOT_NULL(strstr(iwxstr_ptr(xstr), "1\t{\"foo\":\"bar\"}"));


  iwxstr_destroy(xstr);
  iwxstr_destroy(hstr);
  curl_slist_free_all(headers);
  rc = ejdb_close(&db);
  CU_ASSERT_EQUAL_FATAL(rc, 0);
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) return CU_get_error();
  pSuite = CU_add_suite("jbr_test1", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (
    (NULL == CU_add_test(pSuite, "jbr_test1_1", jbr_test1_1))
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
