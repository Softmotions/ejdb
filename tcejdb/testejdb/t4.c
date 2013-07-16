
#include "myconf.h"
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
    CU_ASSERT_TRUE(coll != NULL);
    ejdbclose(jb);
    ejdbdel(jb);
}

void testBSONExportImport() {
    EJDB *jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_export", JBOWRITER | JBOCREAT | JBOTRUNC));
    EJCOLL *coll = ejdbcreatecoll(jb, "col1", NULL);
    if (!coll) {
        eprint(jb, __LINE__, "testBSONExportImport");
    }
    CU_ASSERT_TRUE(coll != NULL);
    bson_oid_t oid;

    bson bv1;
    bson_init(&bv1);
    bson_append_int(&bv1, "a", 1);
    bson_append_string(&bv1, "c", "d");
    bson_finish(&bv1);
    ejdbsavebson(coll, &bv1, &oid);
    bson_destroy(&bv1);

    EJCOLLOPTS copts = {0};
    copts.large = true;
    copts.records = 200000;
    coll = ejdbcreatecoll(jb, "col2", &copts);
    if (!coll) {
        eprint(jb, __LINE__, "testBSONExportImport");
    }
    CU_ASSERT_TRUE(coll != NULL);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "f", JBIDXSTR | JBIDXNUM));
    bson_init(&bv1);
    bson_append_int(&bv1, "e", 1);
    bson_append_string(&bv1, "f", "g");
    bson_finish(&bv1);
    ejdbsavebson(coll, &bv1, &oid);
    bson_destroy(&bv1);

    bson_init(&bv1);
    bson_append_int(&bv1, "e", 2);
    bson_append_string(&bv1, "f", "g2");
    bson_finish(&bv1);
    ejdbsavebson(coll, &bv1, &oid);
    bson_destroy(&bv1);


    TCXSTR *log = tcxstrnew();
    TCLIST *cnames = tclistnew();
    tclistpush2(cnames, "col1");
    tclistpush2(cnames, "col2");


    bool rv = ejdbexport(jb, "testBSONExportImport", cnames, 0, log);
    if (!rv) {
        eprint(jb, __LINE__, "testBSONExportImport");
    }
    CU_ASSERT_TRUE(rv);
    ejdbclose(jb);
    ejdbdel(jb);

    //Restore data:

    jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_export", JBOWRITER | JBOCREAT | JBOTRUNC));

    rv = ejdbimport(jb, "testBSONExportImport", cnames, 0, log);
    CU_ASSERT_TRUE(rv);

    tcxstrdel(log);
    ejdbclose(jb);
    ejdbdel(jb);
    tclistdel(cnames);

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
            (NULL == CU_add_test(pSuite, "testTicket53", testTicket53)) ||
            (NULL == CU_add_test(pSuite, "testBSONExportImport", testBSONExportImport))

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
