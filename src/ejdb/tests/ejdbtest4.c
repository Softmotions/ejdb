
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

void testTicket53(void) {
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

void testBSONExportImport(void) {
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


    bool rv = ejdbexport(jb, "testBSONExportImport", NULL, 0, log);
    if (!rv) {
        eprint(jb, __LINE__, "testBSONExportImport");
    }
    CU_ASSERT_TRUE(rv);

    bson *ometa = ejdbmeta(jb);
    CU_ASSERT_TRUE_FATAL(ometa != NULL);

    ejdbclose(jb);
    ejdbdel(jb);

    //Restore data:

    jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_export", JBOWRITER | JBOCREAT));

    coll = ejdbgetcoll(jb, "col1");
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson_init(&bv1);
    bson_append_int(&bv1, "e", 2);
    bson_finish(&bv1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &bv1, &oid));
    bson_destroy(&bv1);

    rv = ejdbimport(jb, "testBSONExportImport", cnames, JBIMPORTREPLACE, log);
    CU_ASSERT_TRUE(rv);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "Replacing all data in 'col1'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "1 objects imported into 'col1'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "2 objects imported into 'col2'"));

    bson *nmeta = ejdbmeta(jb);
    CU_ASSERT_TRUE_FATAL(nmeta != NULL);

    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.0.name", strlen("collections.0.name")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.0.records", strlen("collections.0.records")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.name", strlen("collections.1.name")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.records", strlen("collections.1.records")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.options.buckets", strlen("collections.1.options.buckets")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.options.large", strlen("collections.1.options.large")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.0.field", strlen("collections.1.indexes.0.field")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.0.type", strlen("collections.1.indexes.0.type")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.0.records", strlen("collections.1.indexes.0.records")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.1.field", strlen("collections.1.indexes.1.field")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.1.type", strlen("collections.1.indexes.1.type")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.1.records", strlen("collections.1.indexes.1.records")) == 0);

    ejdbclose(jb);
    ejdbdel(jb);

    jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_export", JBOWRITER | JBOCREAT | JBOTRUNC));

    coll = ejdbcreatecoll(jb, "col1", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson_init(&bv1);
    bson_append_int(&bv1, "e", 2);
    bson_finish(&bv1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &bv1, &oid));

    EJQ *q = ejdbcreatequery(jb, &bv1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q);
    uint32_t count = 0;
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_EQUAL(count, 1);

    rv = ejdbimport(jb, "testBSONExportImport", NULL, JBIMPORTUPDATE, NULL);
    CU_ASSERT_TRUE(rv);

    coll = ejdbcreatecoll(jb, "col1", NULL);
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_EQUAL(count, 1);

    ejdbquerydel(q);
    bson_destroy(&bv1);
    ejdbclose(jb);
    ejdbdel(jb);

    bson_del(ometa);
    bson_del(nmeta);
    tcxstrdel(log);
    tclistdel(cnames);
}

void testBSONExportImport2(void) {
    EJDB *jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_export", JBOWRITER | JBOCREAT | JBOTRUNC));
    EJCOLL *coll = ejdbcreatecoll(jb, "col1", NULL);
    if (!coll) {
        eprint(jb, __LINE__, "testBSONExportImport2");
    }
    CU_ASSERT_TRUE(coll != NULL);
    bson_oid_t oid;
    const char *log = NULL;

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
        eprint(jb, __LINE__, "testBSONExportImport2");
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

    bson cmd;
    bson_init(&cmd);
    bson_append_start_object(&cmd, "export");
    bson_append_string(&cmd, "path", "testBSONExportImport2");
    bson_append_start_array(&cmd, "cnames");
    bson_append_string(&cmd, "0", "col1");
    bson_append_string(&cmd, "1", "col2");
    bson_append_finish_array(&cmd);
    bson_append_finish_object(&cmd);
    bson_finish(&cmd);
    bson *bret = ejdbcommand(jb, &cmd);
    CU_ASSERT_PTR_NOT_NULL_FATAL(bret);
    bson_destroy(&cmd);

    bson_iterator it;
    bson_iterator_init(&it, bret);
    CU_ASSERT_TRUE(bson_find_fieldpath_value("error", &it) == BSON_EOO);
    bson_iterator_init(&it, bret);
    CU_ASSERT_TRUE(bson_compare_long(0, bson_data(bret), "errorCode") == 0);
    bson_iterator_init(&it, bret);
    CU_ASSERT_TRUE(bson_find_fieldpath_value("log", &it) == BSON_STRING);
    bson_del(bret);

    bson *ometa = ejdbmeta(jb);
    CU_ASSERT_TRUE_FATAL(ometa != NULL);

    ejdbclose(jb);
    ejdbdel(jb);

    //Restore data:
    jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_export", JBOWRITER | JBOCREAT));

    coll = ejdbgetcoll(jb, "col1");
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson_init(&bv1);
    bson_append_int(&bv1, "e", 2);
    bson_finish(&bv1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &bv1, &oid));
    bson_destroy(&bv1);

    bson_init(&cmd);
    bson_append_start_object(&cmd, "import");
    bson_append_string(&cmd, "path", "testBSONExportImport2");
    bson_append_int(&cmd, "mode", JBIMPORTREPLACE);
    bson_append_start_array(&cmd, "cnames");
    bson_append_string(&cmd, "0", "col1");
    bson_append_string(&cmd, "1", "col2");
    bson_append_finish_array(&cmd);
    bson_append_finish_object(&cmd);
    bson_finish(&cmd);
    bret = ejdbcommand(jb, &cmd);
    CU_ASSERT_PTR_NOT_NULL_FATAL(bret);
    bson_destroy(&cmd);

    bson_iterator_init(&it, bret);
    CU_ASSERT_TRUE_FATAL(bson_find_fieldpath_value("log", &it) == BSON_STRING);
    log = bson_iterator_string(&it);

    CU_ASSERT_PTR_NOT_NULL(strstr(log, "Replacing all data in 'col1'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(log, "1 objects imported into 'col1'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(log, "2 objects imported into 'col2'"));
    bson_del(bret);
    log = NULL;

    bson *nmeta = ejdbmeta(jb);
    CU_ASSERT_TRUE_FATAL(nmeta != NULL);

    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.0.name", strlen("collections.0.name")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.0.records", strlen("collections.0.records")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.name", strlen("collections.1.name")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.records", strlen("collections.1.records")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.options.buckets", strlen("collections.1.options.buckets")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.options.large", strlen("collections.1.options.large")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.0.field", strlen("collections.1.indexes.0.field")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.0.type", strlen("collections.1.indexes.0.type")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.0.records", strlen("collections.1.indexes.0.records")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.1.field", strlen("collections.1.indexes.1.field")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.1.type", strlen("collections.1.indexes.1.type")) == 0);
    CU_ASSERT_TRUE(bson_compare(bson_data(ometa), bson_data(nmeta), "collections.1.indexes.1.records", strlen("collections.1.indexes.1.records")) == 0);

    ejdbclose(jb);
    ejdbdel(jb);

    jb = ejdbnew();
    CU_ASSERT_TRUE_FATAL(ejdbopen(jb, "dbt4_export", JBOWRITER | JBOCREAT | JBOTRUNC));

    coll = ejdbcreatecoll(jb, "col1", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson_init(&bv1);
    bson_append_int(&bv1, "e", 2);
    bson_finish(&bv1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &bv1, &oid));

    EJQ *q = ejdbcreatequery(jb, &bv1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q);
    uint32_t count = 0;
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_EQUAL(count, 1);

    bson_init(&cmd);
    bson_append_start_object(&cmd, "import");
    bson_append_string(&cmd, "path", "testBSONExportImport2");
    bson_append_int(&cmd, "mode", JBIMPORTUPDATE);
    bson_append_finish_object(&cmd);
    bson_finish(&cmd);
    bret = ejdbcommand(jb, &cmd);
    CU_ASSERT_PTR_NOT_NULL_FATAL(bret);
    bson_destroy(&cmd);
    bson_del(bret);

    coll = ejdbcreatecoll(jb, "col1", NULL);
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_EQUAL(count, 1);

    ejdbquerydel(q);
    bson_destroy(&bv1);
    ejdbclose(jb);
    ejdbdel(jb);

    bson_del(ometa);
    bson_del(nmeta);
}


void testTicket135(void) {
    bson bo_test;
    bson_init(&bo_test);
    bson_append_int(&bo_test, "myInt", 10);
    bson_append_double(&bo_test, "myDouble", -50.0);
    bson_finish(&bo_test);

    char* buf = NULL;
    int lenght = 0;
    bson2json(bson_data(&bo_test), &buf, &lenght);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
    CU_ASSERT_PTR_NOT_NULL(strstr(buf, "\"myInt\" : 10"));
    CU_ASSERT_PTR_NOT_NULL(strstr(buf, "\"myDouble\" : -50.000000"));
    bson_destroy(&bo_test);
    TCFREE(buf);
}


// We are trying to open TCHDB which is not actuall ejdb database
void testOpenOtherTCHDB(void) {
    EJDB *jb = ejdbnew();
    bool ret = ejdbopen(jb, "dbt4_export_col1", JBOREADER);
    CU_ASSERT_FALSE(ret);
    int ecode = ejdbecode(jb);
    CU_ASSERT_EQUAL(ecode, JBEMETANVALID);
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
    pSuite = CU_add_suite("ejdbtest1", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if (
            (NULL == CU_add_test(pSuite, "testTicket53", testTicket53)) ||
            (NULL == CU_add_test(pSuite, "testBSONExportImport", testBSONExportImport)) ||
            (NULL == CU_add_test(pSuite, "testBSONExportImport2", testBSONExportImport2)) ||
            (NULL == CU_add_test(pSuite, "testTicket135", testTicket135)) ||
            (NULL == CU_add_test(pSuite, "testOpenOtherTCHDB", testOpenOtherTCHDB))
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
