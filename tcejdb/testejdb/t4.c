
#include <locale.h>
#include <pthread.h>
#include "ejdb_private.h"

#include "CUnit/Basic.h"

/*
 * CUnit Test Suite
 */

static void eprint(EJDB *jb, int line, const char *func) {
    int ecode = ejdbecode(jb);
    fprintf(stderr, "%d: %s: error: %d: %s\n",
            line, func, ecode, ejdberrmsg(ecode));
}

void testTicket53() {
    EJDB *jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_53", JBOWRITER | JBOCREAT));
    ejdbclose(jb);
    ejdbdel(jb);

    jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_53", JBOWRITER));

    EJCOLL *coll = ejdbcreatecoll(jb, "mycoll", NULL);
    if (!coll) {
        eprint(jb, __LINE__, "testTicket53");
    }
    CU_ASSERT_TRUE(coll);
    ejdbclose(jb);
    ejdbdel(jb);
}

int init_suite(void) {
    return 0;
}

int clean_suite(void) {
    return 0;
}


int main() {
    setlocale(LC_ALL, "en_US.UTF-8");
    CU_pSuite pSuite = NULL;

    /* Initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* Add a suite to the registry */
    pSuite = CU_add_suite("t4", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if (
            (NULL == CU_add_test(pSuite, "testTicket53", testTicket53))

            ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    int ret = CU_get_error() || CU_get_number_of_failures();
    CU_cleanup_registry();
    return ret;
}
