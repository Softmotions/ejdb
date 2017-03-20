
#include "myconf.h"
#include "ejdb_private.h"
#include "CUnit/Basic.h"

void testIssue182() {
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
}

void subTestIssue182() {
    bson b1, b2, b3;

    bson_init(&b1);
    bson_append_int(&b1, "test", 1);
    bson_finish(&b1);
    
    bson_init(&b2);
    bson_append_int(&b2, "test", -1);
    bson_finish(&b2);
    
    bson_init(&b3);
    bson_append_string(&b3, "other", "");
    bson_finish(&b3);
    
    CU_ASSERT_TRUE(bson_compare(bson_data(&b1), bson_data(&b2), "test", 4) > 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(&b1), bson_data(&b3), "test", 4) > 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(&b2), bson_data(&b1), "test", 4) < 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(&b2), bson_data(&b3), "test", 4) > 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(&b3), bson_data(&b1), "test", 4) < 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(&b3), bson_data(&b2), "test", 4) < 0);

    bson_destroy(&b1);
    bson_destroy(&b2);
    bson_destroy(&b3);
}

void testIssue174(void) {
    EJDB *jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt6", JBOWRITER | JBOCREAT | JBOTRUNC));
    EJCOLL *coll1 = ejdbcreatecoll(jb, "issue174_1", NULL);
    CU_ASSERT_EQUAL_FATAL(ejdbecode(jb), TCESUCCESS);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll1);

    EJCOLL *coll2 = ejdbcreatecoll(jb, "issue174_2", NULL);
    CU_ASSERT_EQUAL_FATAL(ejdbecode(jb), TCESUCCESS);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll2);

    EJCOLL *coll3 = ejdbcreatecoll(jb, "issue174_3", NULL);
    CU_ASSERT_EQUAL_FATAL(ejdbecode(jb), TCESUCCESS);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll3);

    ejdbclose(jb);
    ejdbdel(jb);
}

void testIssue169(void) {
    bson_oid_t id;

    EJDB *db = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(db, "dbt6", JBOWRITER|JBOCREAT|JBOTRUNC));
    EJCOLLOPTS opts;
    opts.large = false;
    opts.compressed = false;
    opts.records = 1;
    opts.cachedrecords = 0;

    EJCOLL *coll1 = ejdbcreatecoll(db, "issue169", &opts);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll1);

    bson *b = bson_create();;
    bson_init(b);
    bson_append_string(b, "key", "value");
    bson_finish(b);
    CU_ASSERT_FALSE_FATAL(b->err);

    ejdbsavebson(coll1, b, &id);
    bson_del(b);
    ejdbsyncdb(db);
    ejdbclose(db);

    CU_ASSERT_TRUE_FATAL(ejdbopen(db, "dbt6", JBOREADER));
    EJCOLL *coll2 = ejdbgetcoll(db, "issue169");
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll2);
    CU_ASSERT_PTR_NOT_EQUAL_FATAL(coll2, coll1);

    b = ejdbloadbson(coll2, &id);
    CU_ASSERT_PTR_NOT_NULL_FATAL(b);
    CU_ASSERT_TRUE_FATAL(bson_compare_string("value", bson_data(b), "key") == 0);
    bson_del(b);
    ejdbclose(db);
    ejdbdel(db);
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
            (NULL == CU_add_test(pSuite, "subTestIssue182", subTestIssue182)) ||
            (NULL == CU_add_test(pSuite, "testIssue182", testIssue182)) ||
            (NULL == CU_add_test(pSuite, "testIssue174", testIssue174)) ||
            (NULL == CU_add_test(pSuite, "testIssue169", testIssue169))
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
