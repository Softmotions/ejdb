
#include "myconf.h"
#include "ejdb_private.h"
#include "CUnit/Basic.h"

int testIssue182() {
    EJDB *jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt6", JBOWRITER | JBOCREAT | JBOTRUNC));
    EJCOLL *coll = ejdbcreatecoll(jb, "issue182", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson_oid_t oid;
    bson bv1;

    // add one odd record (has to be the first to provoke the segmentation fault)
    bson_init(&bv1);
    bson_append_string(&bv1, "test2", "dd");
    bson_finish(&bv1);
    CU_ASSERT_FALSE_FATAL(bv1.err);
    ejdbsavebson(coll, &bv1, &oid);
    bson_destroy(&bv1);

    // add one record with positive value
    bson_init(&bv1);
    bson_append_int(&bv1, "test", 1);
    bson_finish(&bv1);
    CU_ASSERT_FALSE_FATAL(bv1.err);
    ejdbsavebson(coll, &bv1, &oid);
    bson_destroy(&bv1);

    // add one record with negative value
    bson_init(&bv1);
    bson_append_int(&bv1, "test", -1);
    bson_finish(&bv1);
    CU_ASSERT_FALSE_FATAL(bv1.err);
    ejdbsavebson(coll, &bv1, &oid);
    bson_destroy(&bv1);


    // this query now crashes the db (using -1 sort order does not)
    //  db.find("testcoll", {}, { "$orderby": { "test": 1 } }, function(err, cursor, count) {

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "test", 1);
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    // execute query
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);

    ejdbclose(jb);
    ejdbdel(jb);
    return 0;
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
    pSuite = CU_add_suite("ejdbtest6", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if (
            (NULL == CU_add_test(pSuite, "testIssue182", testIssue182))
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
