/*
 * File:   t3.c
 * Author: Adamansky Anton <adamansky@gmail.com>
 *
 * Created on Oct 26, 2012, 12:12:45 PM
 */

#include "myconf.h"
#include "ejdb_private.h"
#include "CUnit/Basic.h"
#include <stdlib.h>

/*
 * CUnit Test Suite
 */

static EJDB *jb;
const int RS = 10000;
const int QRS = 100;
static bson* recs;

static void eprint(EJDB *jb, int line, const char *func);

int init_suite(void) {
    assert(QRS < RS);
    jb = ejdbnew();
    if (!ejdbopen(jb, "dbt3", JBOWRITER | JBOCREAT | JBOTRUNC)) {
        return 1;
    }
    srand(tcmstime());
    recs = malloc(RS * sizeof (bson));
    if (!recs) {
        return 1;
    }
    for (int i = 0; i < RS; ++i) {
        bson bs;
        bson_init(&bs);
        bson_append_long(&bs, "ts", tcmstime());
        char str[128];
        int len = 0;
        do {
            len = rand() % 128;
        } while (len <= 0);
        str[0] = 'A' + (i % 26);
        for (int j = 1; j < len; ++j) {
            str[j] = 'a' + rand() % 26;
        }
        str[len] = '\0';
        bson_append_string(&bs, "rstring", str);
        bson_finish(&bs);
        recs[i] = bs;
    }

    return 0;
}

int clean_suite(void) {
    ejdbclose(jb);
    ejdbdel(jb);
    for (int i = 0; i < RS; ++i) {
        bson_destroy(&recs[i]);
    }
    free(recs);
    return 0;
}

void testPerf1() {
    CU_ASSERT_PTR_NOT_NULL_FATAL(jb);
    CU_ASSERT_PTR_NOT_NULL_FATAL(recs);
    EJCOLL *coll = ejdbcreatecoll(jb, "pcoll1", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    unsigned long st = tcmstime();
    for (int i = 0; i < RS; ++i) {
        bson_oid_t oid;
        ejdbsavebson(coll, recs + i, &oid);
    }
    ejdbsyncoll(coll);
    fprintf(stderr, "\ntestPerf1(): SAVED %d BSON OBJECTS, TIME %lu ms\n", RS, tcmstime() - st);

    st = tcmstime();
    uint32_t acount = 0;
    int i;
    for (i = 0; i < QRS; ++i) {
        int idx = rand() % QRS;
        bson *bs = recs + idx;
        assert(bs);
        EJQ *q = ejdbcreatequery(jb, bs, NULL, 0, NULL);
        assert(q);
        uint32_t count;
        ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
        assert(count);
        if (count != 1) {
            fprintf(stderr, "CNT=%u\n", count);
        }
        acount += count;
        ejdbquerydel(q);
    }
    CU_ASSERT_TRUE(i <= acount);
    fprintf(stderr, "testPerf1(): %u QUERIES, TIME: %lu ms, PER QUERY TIME: %lu ms\n",
            i, tcmstime() - st, (unsigned long) ((tcmstime() - st) / QRS));

    st = tcmstime();
    CU_ASSERT_TRUE(ejdbsetindex(coll, "rstring", JBIDXSTR));
    fprintf(stderr, "testPerf1(): SET INDEX 'rstring' TIME: %lu ms\n", tcmstime() - st);

    st = tcmstime();
    acount = 0;
    for (i = 0; i < QRS; ++i) {
        int idx = rand() % QRS;
        bson *bs = recs + idx;
        assert(bs);
        EJQ *q = ejdbcreatequery(jb, bs, NULL, 0, NULL);
        assert(q);
        uint32_t count;
        ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
        assert(count);
        acount += count;
        ejdbquerydel(q);
    }
    CU_ASSERT_TRUE(i <= acount);
    fprintf(stderr, "testPerf1(): %u QUERIES WITH 'rstring' INDEX, TIME: %lu ms, PER QUERY TIME: %lu ms\n",
            i, tcmstime() - st, (unsigned long) ((tcmstime() - st) / QRS));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$set");
    bson_append_int(&bsq1, "intv", 1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    EJQ *q = ejdbcreatequery(jb, &bsq1, NULL, JBQRYCOUNT, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q);
    uint32_t count;
    st = tcmstime(); //$set op
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
    if (ejdbecode(jb) != 0) {
        eprint(jb, __LINE__, "$set test");
        CU_ASSERT_TRUE(false);
    }
    CU_ASSERT_EQUAL(count, RS);
    fprintf(stderr, "testPerf1(): {'$set' : {'intv' : 1}} FOR %u OBJECTS, TIME %lu ms\n", count, tcmstime() - st);
    ejdbquerydel(q);
    bson_destroy(&bsq1);

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$inc");
    bson_append_int(&bsq1, "intv", 1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    q = ejdbcreatequery(jb, &bsq1, NULL, JBQRYCOUNT, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q);
    st = tcmstime(); //$inc op
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
    if (ejdbecode(jb) != 0) {
        eprint(jb, __LINE__, "$inc test");
        CU_ASSERT_TRUE(false);
    }
    CU_ASSERT_EQUAL(count, RS);
    fprintf(stderr, "testPerf1(): {'$inc' : {'intv' : 1}} FOR %u OBJECTS, TIME %lu ms\n", count, tcmstime() - st);
    ejdbquerydel(q);
    bson_destroy(&bsq1);

    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "intv", 2);
    bson_finish(&bsq1);
    q = ejdbcreatequery(jb, &bsq1, NULL, JBQRYCOUNT, NULL);
    ejdbqryexecute(coll, q, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_EQUAL(count, RS);
    ejdbquerydel(q);
    bson_destroy(&bsq1);

    ejdbrmcoll(jb, coll->cname, true);
}

//Race conditions

typedef struct {
    int id;
    EJDB *jb;

} TARGRACE;

static void eprint(EJDB *jb, int line, const char *func) {
    int ecode = ejdbecode(jb);
    fprintf(stderr, "%d: %s: error: %d: %s\n",
            line, func, ecode, ejdberrmsg(ecode));
}

static void *threadrace1(void *_tr) {
    const int iterations = 500;
    TARGRACE *tr = (TARGRACE*) _tr;
    bool err = false;

    bson bq;
    bson_init_as_query(&bq);
    bson_append_int(&bq, "tid", tr->id);
    bson_finish(&bq);

    bson_type bt;
    bson_iterator it;
    void *bsdata;
    bool saved = false;
    int lastcnt = 0;

    EJCOLL *coll = ejdbcreatecoll(jb, "threadrace1", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    EJQ *q = ejdbcreatequery(jb, &bq, NULL, 0, NULL);
    TCXSTR *log = tcxstrnew();

    for (int i = 0; !err && i < iterations; ++i) {
        CU_ASSERT_PTR_NOT_NULL_FATAL(q);
        tcxstrclear(log);

        bson_oid_t oid2;
        bson_oid_t *oid = NULL;
        int cnt = 0;
        uint32_t count;
        TCLIST *res = NULL;

        if (ejdbecode(jb) != 0) {
            eprint(jb, __LINE__, "threadrace1");
            err = true;
            goto ffinish;
        }
        res = ejdbqryexecute(coll, q, &count, 0, log);
        if (ejdbecode(jb) != 0) {
            eprint(jb, __LINE__, "threadrace1.ejdbqryexecute");
            err = true;
            goto ffinish;
        }
        if (count != 1 && saved) {
            fprintf(stderr, "%d:COUNT=%d it=%d\n", tr->id, count, i);
            CU_ASSERT_TRUE(false);
            goto ffinish;
        }
        if (count > 0) {
            bsdata = TCLISTVALPTR(res, 0);
            CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);
            bt = bson_find_from_buffer(&it, bsdata, "cnt");
            CU_ASSERT_EQUAL_FATAL(bt, BSON_INT);
            cnt = bson_iterator_int(&it);
            bt = bson_find_from_buffer(&it, bsdata, "_id");
            CU_ASSERT_EQUAL_FATAL(bt, BSON_OID);
            oid = bson_iterator_oid(&it);
            CU_ASSERT_PTR_NOT_NULL_FATAL(oid);
        }

        bson sbs;
        bson_init(&sbs);
        if (oid) {
            bson_append_oid(&sbs, "_id", oid);
        }
        bson_append_int(&sbs, "tid", tr->id);
        bson_append_int(&sbs, "cnt", ++cnt);
        bson_finish(&sbs);

        if (!ejdbsavebson(coll, &sbs, &oid2)) {
            eprint(jb, __LINE__, "threadrace1.ejdbsavebson");
            err = true;
        }
        saved = true;
        bson_destroy(&sbs);
        lastcnt = cnt;
ffinish:
        if (res) tclistdel(res);
    }
    if (q) ejdbquerydel(q);
    if (log) tcxstrdel(log);
    bson_destroy(&bq);
    CU_ASSERT_EQUAL(lastcnt, iterations);
    //fprintf(stderr, "\nThread %d finished", tr->id);
    return err ? "error" : NULL;
}

#define tnum 50
 
void testRace1() {
    CU_ASSERT_PTR_NOT_NULL_FATAL(jb);
    bool err = false;
    TARGRACE targs[tnum];
    pthread_t threads[tnum];

    EJCOLL *coll = ejdbcreatecoll(jb, "threadrace1", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    if (!ejdbsetindex(coll, "tid", JBIDXNUM)) { //INT INDEX
        eprint(jb, __LINE__, "testRace1");
        err = true;
    }
    if (err) {
        goto finish;
    }

    for (int i = 0; i < tnum; i++) {
        targs[i].jb = jb;
        targs[i].id = i;
        if (pthread_create(threads + i, NULL, threadrace1, targs + i) != 0) {
            eprint(jb, __LINE__, "pthread_create");
            targs[i].id = -1;
            err = true;
        }
    }

    for (int i = 0; i < tnum; i++) {
        if (targs[i].id == -1) continue;
        void *rv;
        if (pthread_join(threads[i], &rv) != 0) {
            eprint(jb, __LINE__, "pthread_join");
            err = true;
        } else if (rv) {
            err = true;
        }
    }

finish:
    CU_ASSERT_FALSE(err);
}

void testRace2() {
    CU_ASSERT_PTR_NOT_NULL_FATAL(jb);
    bool err = false;
    TARGRACE targs[tnum];
    pthread_t threads[tnum];

    ejdbrmcoll(jb, "threadrace1", true);
    EJCOLL *coll = ejdbcreatecoll(jb, "threadrace1", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    if (!ejdbsetindex(coll, "tid", JBIDXDROPALL)) { //NO INDEX
        eprint(jb, __LINE__, "testRace2");
        err = true;
    }
    if (err) {
        goto finish;
    }

    for (int i = 0; i < tnum; i++) {
        targs[i].jb = jb;
        targs[i].id = i;
        if (pthread_create(threads + i, NULL, threadrace1, targs + i) != 0) {
            eprint(jb, __LINE__, "pthread_create");
            targs[i].id = -1;
            err = true;
        }
    }
    for (int i = 0; i < tnum; i++) {
        if (targs[i].id == -1) continue;
        void *rv;
        if (pthread_join(threads[i], &rv) != 0) {
            eprint(jb, __LINE__, "pthread_join");
            err = true;
        } else if (rv) {
            err = true;
        }
    }

finish:
    CU_ASSERT_FALSE(err);
}

void testTransactions1() {
    EJCOLL *coll = ejdbcreatecoll(jb, "trans1", NULL);
    bson bs;
    bson_init(&bs);
    bson_append_string(&bs, "foo", "bar");
    bson_finish(&bs);

    bson_oid_t oid;
    CU_ASSERT_TRUE(ejdbtranbegin(coll));
    ejdbsavebson(coll, &bs, &oid);
    CU_ASSERT_TRUE(ejdbtrancommit(coll));

    bson *bres = ejdbloadbson(coll, &oid);
    CU_ASSERT_PTR_NOT_NULL(bres);
    if (bres) {
        bson_del(bres);
    }

    CU_ASSERT_TRUE(ejdbtranbegin(coll));
    ejdbsavebson(coll, &bs, &oid);
    CU_ASSERT_TRUE(ejdbtranabort(coll));

    bres = ejdbloadbson(coll, &oid);
    CU_ASSERT_PTR_NULL(bres);
    if (bres) {
        bson_del(bres);
    }

    bson_destroy(&bs);
}

int main() {
    setlocale(LC_ALL, "en_US.UTF-8");
    CU_pSuite pSuite = NULL;

    /* Initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* Add a suite to the registry */
    pSuite = CU_add_suite("ejdbtest3", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if (
            (NULL == CU_add_test(pSuite, "testPerf1", testPerf1)) ||
            (NULL == CU_add_test(pSuite, "testRace1", testRace1)) ||
            (NULL == CU_add_test(pSuite, "testRace2", testRace2)) ||
            (NULL == CU_add_test(pSuite, "testTransactions1", testTransactions1))

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
