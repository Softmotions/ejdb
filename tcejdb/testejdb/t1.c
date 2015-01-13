/*
 * File:   ejdbtest1.c
 * Author: Adamansky Anton <anton@adamansky.com>
 *
 * Created on Sep 18, 2012, 10:42:19 PM
 */

#include "myconf.h"
#include "ejdb_private.h"
#include "CUnit/Basic.h"


/*
 * CUnit Test Suite
 */

static EJDB *jb;

int init_suite(void) {
    jb = ejdbnew();
    if (!ejdbopen(jb, "dbt1", JBOWRITER | JBOCREAT | JBOTRUNC)) {
        return 1;
    }
    return 0;
}

int clean_suite(void) {
    ejdbrmcoll(jb, "contacts", true);
    ejdbclose(jb);
    ejdbdel(jb);
    return 0;
}

void testTicket102() {
    const char *json = "[0, 1, 2]";
    bson *ret = json2bson(json);
    CU_ASSERT_PTR_NOT_NULL(ret);
    bson_print_raw(bson_data(ret), 0);
    CU_ASSERT_EQUAL(bson_compare_long(0, bson_data(ret), "0"), 0);
    CU_ASSERT_EQUAL(bson_compare_long(1, bson_data(ret), "1"), 0);
    CU_ASSERT_EQUAL(bson_compare_long(2, bson_data(ret), "2"), 0);
	bson_del(ret);
}

void testSaveLoad() {
    CU_ASSERT_PTR_NOT_NULL_FATAL(jb);
    bson_oid_t oid;
    EJCOLL *ccoll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL(ccoll);

    //Save record
    bson a1;
    bson_init(&a1);

    bson_append_string(&a1, "name", "Петров Петр");
    bson_append_string(&a1, "phone", "333-222-333");
    bson_append_int(&a1, "age", 33);
    bson_append_long(&a1, "longage", 0xFFFFFFFFFF01LL);
    bson_append_double(&a1, "doubleage", 0.333333);
    bson_finish(&a1);
    ejdbsavebson(ccoll, &a1, &oid);
    bson_destroy(&a1);


    bson *lbson = ejdbloadbson(ccoll, &oid);
    CU_ASSERT_PTR_NOT_NULL(lbson);
    bson_iterator it1;
    bson_iterator_init(&it1, lbson);

    int btype = bson_iterator_next(&it1);
    CU_ASSERT(btype == BSON_OID);
    btype = bson_iterator_next(&it1);
    CU_ASSERT(btype == BSON_STRING);
    CU_ASSERT(!strcmp("name", bson_iterator_key(&it1)));
    CU_ASSERT(!strcmp("Петров Петр", bson_iterator_string(&it1)));
    btype = bson_iterator_next(&it1);
    CU_ASSERT(btype == BSON_STRING);
    CU_ASSERT(!strcmp("phone", bson_iterator_key(&it1)));
    CU_ASSERT(!strcmp("333-222-333", bson_iterator_string(&it1)));
    btype = bson_iterator_next(&it1);
    CU_ASSERT(btype == BSON_INT);
    CU_ASSERT(!strcmp("age", bson_iterator_key(&it1)));
    CU_ASSERT(33 == bson_iterator_int(&it1));
    btype = bson_iterator_next(&it1);
    CU_ASSERT(btype == BSON_LONG);
    CU_ASSERT(!strcmp("longage", bson_iterator_key(&it1)));
    CU_ASSERT(0xFFFFFFFFFF01LL == bson_iterator_long(&it1));
    btype = bson_iterator_next(&it1);
    CU_ASSERT(btype == BSON_DOUBLE);
    CU_ASSERT(!strcmp("doubleage", bson_iterator_key(&it1)));
    CU_ASSERT_DOUBLE_EQUAL(bson_iterator_double(&it1), 0.3, 0.1);
    btype = bson_iterator_next(&it1);
    CU_ASSERT(btype == BSON_EOO);
    bson_del(lbson);
}

void testBuildQuery1() {
    CU_ASSERT_PTR_NOT_NULL_FATAL(jb);
    /*
     Query = {
        "name" : Петров Петр,
        "age"  : 33,
        "family" : {
            "wife" : {
                "name"  : "Jeniffer",
                "age"   : {"$gt" : 25},
                "phone" : "444-111"
            },
            "children" : [
                {
                    "name" : "Dasha",
                    "age" : {"$in" : [1, 4, 10]}
                }
            ]
         }
     */
    bson q1;
    bson_init_as_query(&q1);
    bson_append_string(&q1, "name", "Петров Петр");
    bson_append_int(&q1, "age", 33);

    bson q1family_wife;
    bson_init_as_query(&q1family_wife);
    bson_append_string(&q1family_wife, "name", "Jeniffer");
    bson_append_start_object(&q1family_wife, "age");
    bson_append_int(&q1family_wife, "$gt", 25);
    bson_append_finish_object(&q1family_wife);

    bson_append_string(&q1family_wife, "phone", "444-111");
    bson_finish(&q1family_wife);

    bson q1family_child;
    bson_init_as_query(&q1family_child);
    bson_append_string(&q1family_child, "name", "Dasha");

    //"age" : {"$in" : [1, 4, 10]}
    bson q1family_child_age_IN;
    bson_init_as_query(&q1family_child_age_IN);
    bson_append_start_array(&q1family_child_age_IN, "$in");
    bson_append_int(&q1family_child_age_IN, "0", 1);
    bson_append_int(&q1family_child_age_IN, "1", 4);
    bson_append_int(&q1family_child_age_IN, "2", 10);
    bson_append_finish_array(&q1family_child_age_IN);
    bson_finish(&q1family_child_age_IN);
    bson_append_bson(&q1family_child, "age", &q1family_child_age_IN);
    bson_finish(&q1family_child);

    bson q1family;
    bson_init_as_query(&q1family);
    bson_append_bson(&q1family, "wife", &q1family_wife);
    bson_append_start_array(&q1family, "children");
    bson_append_bson(&q1family, "0", &q1family_child);
    bson_append_finish_array(&q1family);
    bson_finish(&q1family);

    bson_append_bson(&q1, "family", &q1family);
    bson_finish(&q1);

    CU_ASSERT_FALSE_FATAL(q1.err);
    CU_ASSERT_FALSE_FATAL(q1family.err);
    CU_ASSERT_FALSE_FATAL(q1family_wife.err);
    CU_ASSERT_FALSE_FATAL(q1family_child.err);
    CU_ASSERT_FALSE_FATAL(q1family_child_age_IN.err);

    EJQ *ejq = ejdbcreatequery(jb, &q1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(ejq);

    bson_destroy(&q1);
    bson_destroy(&q1family);
    bson_destroy(&q1family_wife);
    bson_destroy(&q1family_child);
    bson_destroy(&q1family_child_age_IN);

    CU_ASSERT_PTR_NOT_NULL_FATAL(ejq->qflist);
    TCLIST *qmap = ejq->qflist;
    CU_ASSERT_EQUAL(qmap->num, 7);

    for (int i = 0; i < TCLISTNUM(qmap); ++i) {

        const EJQF *qf = TCLISTVALPTR(qmap, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(qf);
        const char* key = qf->fpath;

        switch (i) {
            case 0:
            {
                CU_ASSERT_STRING_EQUAL(key, "name");
                CU_ASSERT_PTR_NOT_NULL(qf);
                CU_ASSERT_STRING_EQUAL(qf->expr, "Петров Петр");
                CU_ASSERT_EQUAL(qf->tcop, TDBQCSTREQ);
                break;
            }
            case 1:
            {
                CU_ASSERT_STRING_EQUAL(key, "age");
                CU_ASSERT_PTR_NOT_NULL(qf);
                CU_ASSERT_STRING_EQUAL(qf->expr, "33");
                CU_ASSERT_EQUAL(qf->tcop, TDBQCNUMEQ);
                break;
            }
            case 2:
            {
                CU_ASSERT_STRING_EQUAL(key, "family.wife.name");
                CU_ASSERT_PTR_NOT_NULL(qf);
                CU_ASSERT_STRING_EQUAL(qf->expr, "Jeniffer");
                CU_ASSERT_EQUAL(qf->tcop, TDBQCSTREQ);
                break;
            }
            case 3:
            {
                CU_ASSERT_STRING_EQUAL(key, "family.wife.age");
                CU_ASSERT_PTR_NOT_NULL(qf);
                CU_ASSERT_STRING_EQUAL(qf->expr, "25");
                CU_ASSERT_EQUAL(qf->tcop, TDBQCNUMGT);
                break;
            }
            case 4:
            {
                CU_ASSERT_STRING_EQUAL(key, "family.wife.phone");
                CU_ASSERT_PTR_NOT_NULL(qf);
                CU_ASSERT_STRING_EQUAL(qf->expr, "444-111");
                CU_ASSERT_EQUAL(qf->tcop, TDBQCSTREQ);
                break;
            }
            case 5:
            {
                CU_ASSERT_STRING_EQUAL(key, "family.children.0.name");
                CU_ASSERT_PTR_NOT_NULL(qf);
                CU_ASSERT_STRING_EQUAL(qf->expr, "Dasha");
                CU_ASSERT_EQUAL(qf->tcop, TDBQCSTREQ);
                break;
            }
            case 6:
            {
                CU_ASSERT_STRING_EQUAL(key, "family.children.0.age");
                CU_ASSERT_PTR_NOT_NULL(qf);
                CU_ASSERT_EQUAL(qf->ftype, BSON_ARRAY);
                TCLIST *al = tclistload(qf->expr, qf->exprsz);
                char* als = tcstrjoin(al, ',');
                CU_ASSERT_STRING_EQUAL(als, "1,4,10");
                TCFREE(als);
                tclistdel(al);
                CU_ASSERT_EQUAL(qf->tcop, TDBQCNUMOREQ);
                break;
            }
        }
    }

    ejdbquerydel(ejq);
}

void testDBOptions() {
    EJCOLLOPTS opts;
    opts.cachedrecords = 10000;
    opts.compressed = true;
    opts.large = true;
    opts.records = 110000;
    EJCOLL *coll = ejdbcreatecoll(jb, "optscoll", &opts);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    TCHDB *hdb = coll->tdb->hdb;
    CU_ASSERT_TRUE(hdb->bnum >= (opts.records * 2 + 1));
    CU_ASSERT_EQUAL(hdb->rcnum, opts.cachedrecords);
    CU_ASSERT_TRUE(hdb->opts & HDBTDEFLATE);
    CU_ASSERT_TRUE(hdb->opts & HDBTLARGE);
    CU_ASSERT_TRUE(ejdbrmcoll(jb, "optscoll", true));
}

int main() {
    setlocale(LC_ALL, "en_US.UTF-8");
    CU_pSuite pSuite = NULL;

    /* Initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* Add a suite to the registry */
    pSuite = CU_add_suite("t1", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if ((NULL == CU_add_test(pSuite, "testSaveLoad", testSaveLoad)) ||
            (NULL == CU_add_test(pSuite, "testBuildQuery1", testBuildQuery1)) ||
            (NULL == CU_add_test(pSuite, "testDBOptions", testDBOptions)) ||
            (NULL == CU_add_test(pSuite, "testTicket102", testTicket102)) 

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
