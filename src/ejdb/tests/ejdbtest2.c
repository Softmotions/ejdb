/*
 * File:   newcunittest.c
 * Author: Adamansky Anton <adamansky@gmail.com>
 *
 * Created on Oct 1, 2012, 3:13:44 PM
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
    if (!ejdbopen(jb, "dbt2", JBOWRITER | JBOCREAT | JBOTRUNC)) {
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

void testAddData(void) {
    CU_ASSERT_PTR_NOT_NULL_FATAL(jb);
    bson_oid_t oid;
    EJCOLL *ccoll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL(ccoll);

    //Record 1
    bson a1;

    bson_init(&a1);
    bson_append_string(&a1, "name", "Антонов");
    bson_append_string(&a1, "phone", "333-222-333");
    bson_append_int(&a1, "age", 33);
    bson_append_long(&a1, "longscore", 0xFFFFFFFFFF01LL);
    bson_append_double(&a1, "dblscore", 0.333333);
    bson_append_start_object(&a1, "address");
    bson_append_string(&a1, "city", "Novosibirsk");
    bson_append_string(&a1, "country", "Russian Federation");
    bson_append_string(&a1, "zip", "630090");
    bson_append_string(&a1, "street", "Pirogova");
    bson_append_int(&a1, "room", 334);
    bson_append_finish_object(&a1); //EOF address
    bson_append_start_array(&a1, "complexarr");
    bson_append_start_object(&a1, "0");
    bson_append_string(&a1, "key", "title");
    bson_append_string(&a1, "value", "some title");
    bson_append_finish_object(&a1);
    bson_append_start_object(&a1, "1");
    bson_append_string(&a1, "key", "comment");
    bson_append_string(&a1, "value", "some comment");
    bson_append_finish_object(&a1);
    bson_append_finish_array(&a1); //EOF complexarr
    CU_ASSERT_FALSE_FATAL(a1.err);
	bson_append_symbol(&a1, "symbol", "apple");
    bson_finish(&a1);
    ejdbsavebson(ccoll, &a1, &oid);
    bson_destroy(&a1);

    //Record 2
    bson_init(&a1);
    bson_append_string(&a1, "name", "Адаманский");
    bson_append_string(&a1, "phone", "444-123-333");
    bson_append_long(&a1, "longscore", 0xFFFFFFFFFF02LL);
    bson_append_double(&a1, "dblscore", 0.93);
    bson_append_start_object(&a1, "address");
    bson_append_string(&a1, "city", "Novosibirsk");
    bson_append_string(&a1, "country", "Russian Federation");
    bson_append_string(&a1, "zip", "630090");
    bson_append_string(&a1, "street", "Pirogova");
    bson_append_finish_object(&a1);
    bson_append_start_array(&a1, "complexarr");
    bson_append_start_object(&a1, "0");
    bson_append_string(&a1, "key", "title");
    bson_append_string(&a1, "value", "some title");
    bson_append_finish_object(&a1);
    bson_append_start_object(&a1, "1");
    bson_append_string(&a1, "key", "title");
    bson_append_string(&a1, "value", "some other title");
    bson_append_finish_object(&a1);
    bson_append_int(&a1, "2", 333);
    bson_append_finish_array(&a1); //EOF complexarr
    bson_append_start_array(&a1, "labels");
    bson_append_string(&a1, "0", "red");
    bson_append_string(&a1, "1", "green");
    bson_append_string(&a1, "2", "with gap, label");
    bson_append_finish_array(&a1);
    bson_append_start_array(&a1, "drinks");
    bson_append_int(&a1, "0", 4);
    bson_append_long(&a1, "1", 556667);
    bson_append_double(&a1, "2", 77676.22);
    bson_append_finish_array(&a1);
	bson_append_symbol(&a1, "symbol", "application");

    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);
    ejdbsavebson(ccoll, &a1, &oid);
    bson_destroy(&a1);

    //Record 3
    bson_init(&a1);
    bson_append_string(&a1, "name", "Ivanov");
    bson_append_long(&a1, "longscore", 66);
    bson_append_double(&a1, "dblscore", 1.0);
    bson_append_start_object(&a1, "address");
    bson_append_string(&a1, "city", "Petropavlovsk");
    bson_append_string(&a1, "country", "Russian Federation");
    bson_append_string(&a1, "zip", "683042");
    bson_append_string(&a1, "street", "Dalnaya");
    bson_append_finish_object(&a1);
    bson_append_start_array(&a1, "drinks");
    bson_append_int(&a1, "0", 41);
    bson_append_long(&a1, "1", 222334);
    bson_append_double(&a1, "2", 77676.22);
    bson_append_finish_array(&a1);
 	bson_append_symbol(&a1, "symbol", "bison");
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);

    CU_ASSERT_TRUE(ejdbsavebson(ccoll, &a1, &oid));
    bson_destroy(&a1);
}

void testInvalidQueries1(void) {
}

void testSetIndex1(void) {
    EJCOLL *ccoll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(ccoll);
    CU_ASSERT_TRUE(ejdbsetindex(ccoll, "ab.c.d", JBIDXSTR));
    CU_ASSERT_TRUE(ejdbsetindex(ccoll, "ab.c.d", JBIDXSTR | JBIDXNUM));
    CU_ASSERT_TRUE(ejdbsetindex(ccoll, "ab.c.d", JBIDXDROPALL));
    CU_ASSERT_TRUE(ejdbsetindex(ccoll, "address.zip", JBIDXSTR));
    CU_ASSERT_TRUE(ejdbsetindex(ccoll, "name", JBIDXSTR));

    //Insert new record with active index
    //Record 4
    bson a1;
    bson_oid_t oid;
    bson_init(&a1);
    bson_append_string(&a1, "name", "John Travolta");
    bson_append_start_object(&a1, "address");
    bson_append_string(&a1, "country", "USA");
    bson_append_string(&a1, "zip", "4499995");
    bson_append_finish_object(&a1);
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);
    CU_ASSERT_TRUE(ejdbsavebson(ccoll, &a1, &oid));
    bson_destroy(&a1);


    //Update record 4 with active index
    //Record 4
    bson_init(&a1);
    bson_append_oid(&a1, "_id", &oid);
    bson_append_string(&a1, "name", "John Travolta2");
    bson_append_start_object(&a1, "address");
    bson_append_string(&a1, "country", "USA");
    bson_append_string(&a1, "zip", "4499996");
    bson_append_finish_object(&a1);
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);

    CU_ASSERT_TRUE(ejdbsavebson(ccoll, &a1, &oid));
    CU_ASSERT_TRUE(ejdbrmbson(ccoll, &oid));
    bson_destroy(&a1);

    //Save Travolta again
    bson_init(&a1);
    bson_append_oid(&a1, "_id", &oid);
    bson_append_string(&a1, "name", "John Travolta");
    bson_append_start_object(&a1, "address");
    bson_append_string(&a1, "country", "USA");
    bson_append_string(&a1, "zip", "4499996");
    bson_append_string(&a1, "street", "Beverly Hills");
    bson_append_finish_object(&a1);
    bson_append_start_array(&a1, "labels");
    bson_append_string(&a1, "0", "yellow");
    bson_append_string(&a1, "1", "red");
    bson_append_string(&a1, "2", "black");
    bson_append_finish_array(&a1);
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);
    CU_ASSERT_TRUE(ejdbsavebson(ccoll, &a1, &oid));
    bson_destroy(&a1);
}

void testQuery1(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "address.zip", "630090");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    //for (int i = 0; i < TCLISTNUM(q1res); ++i) {
    //    void *bsdata = TCLISTVALPTR(q1res, i);
    //    bson_print_raw(bsdata, 0);
    //}
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'saddress.zip'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);


    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
}

void testQuery2(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "address.zip", "630090");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", -1); //DESC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'saddress.zip'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }
    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
}

void testQuery3(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "address.zip", JBIDXDROPALL));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "address.zip", "630090");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'sname'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 20"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
}

void testQuery4(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "name", JBIDXDROPALL));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "address.zip", "630090");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }
    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
}

void testQuery5(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "labels", "red");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery6(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "labels", JBIDXARR));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "labels", "red");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'alabels'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 5"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"red\" 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("4499996", TCLISTVALPTR(q1res, i), "address.zip"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery7(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "labels", "with gap, label");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'alabels'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 5"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"with gap, label\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery8(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    //"labels" : {"$in" : ["yellow", "green"]}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "labels");
    bson_append_start_array(&bsq1, "$in");
    bson_append_string(&bsq1, "0", "green");
    bson_append_string(&bsq1, "1", "yellow");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'alabels'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 5"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"green\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"yellow\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("yellow", TCLISTVALPTR(q1res, i), "labels.0"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.1"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);


    //todo check hash tokens mode
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "labels", JBIDXDROPALL));

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "labels");
    bson_append_start_array(&bsq1, "$in");

    char nbuff[TCNUMBUFSIZ];
    for (int i = 0; i <= JBINOPTMAPTHRESHOLD; ++i) {
        bson_numstrn(nbuff, TCNUMBUFSIZ, i);
        if (i == 2) {
            bson_append_string(&bsq1, nbuff, "green");
        } else if (i == 8) {
            bson_append_string(&bsq1, nbuff, "yellow");
        } else {
            bson_append_string(&bsq1, nbuff, nbuff);
        }
    }
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "USING HASH TOKENS IN: labels"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("yellow", TCLISTVALPTR(q1res, i), "labels.0"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.1"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery9(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "labels", JBIDXDROPALL));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "labels", "red");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("4499996", TCLISTVALPTR(q1res, i), "address.zip"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("630090", TCLISTVALPTR(q1res, i), "address.zip"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }


    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery10(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "address.street", JBIDXSTR));

    //"address.street" : {"$in" : ["Pirogova", "Beverly Hills"]}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.street");
    bson_append_start_array(&bsq1, "$in");
    bson_append_string(&bsq1, "0", "Pirogova");
    bson_append_string(&bsq1, "1", "Beverly Hills");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'saddress.street'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 6"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 3"));
    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("Beverly Hills", TCLISTVALPTR(q1res, i), "address.street"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("Pirogova", TCLISTVALPTR(q1res, i), "address.street"));
        } else if (i == 2) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("Pirogova", TCLISTVALPTR(q1res, i), "address.street"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }
    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery11(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "address.street", JBIDXDROPALL));

    //"address.street" : {"$in" : ["Pirogova", "Beverly Hills"]}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.street");
    bson_append_start_array(&bsq1, "$in");
    bson_append_string(&bsq1, "0", "Pirogova");
    bson_append_string(&bsq1, "1", "Beverly Hills");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 3"));
    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("Beverly Hills", TCLISTVALPTR(q1res, i), "address.street"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("Pirogova", TCLISTVALPTR(q1res, i), "address.street"));
        } else if (i == 2) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("Pirogova", TCLISTVALPTR(q1res, i), "address.street"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery12(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    //"labels" : {"$in" : ["yellow", "green"]}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "labels");
    bson_append_start_array(&bsq1, "$in");
    bson_append_string(&bsq1, "0", "green");
    bson_append_string(&bsq1, "1", "yellow");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("yellow", TCLISTVALPTR(q1res, i), "labels.0"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.1"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery13(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    //"drinks" : {"$in" : [4, 77676.22]}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "drinks");
    bson_append_start_array(&bsq1, "$in");
    bson_append_int(&bsq1, "0", 4);
    bson_append_double(&bsq1, "1", 77676.22);
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));


    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_long(41, TCLISTVALPTR(q1res, i), "drinks.0"));
            CU_ASSERT_FALSE(bson_compare_long(222334, TCLISTVALPTR(q1res, i), "drinks.1"));
            CU_ASSERT_FALSE(bson_compare_double(77676.22, TCLISTVALPTR(q1res, i), "drinks.2"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_long(4, TCLISTVALPTR(q1res, i), "drinks.0"));
            CU_ASSERT_FALSE(bson_compare_long(556667, TCLISTVALPTR(q1res, i), "drinks.1"));
            CU_ASSERT_FALSE(bson_compare_double(77676.22, TCLISTVALPTR(q1res, i), "drinks.2"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery14(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "drinks", JBIDXARR));

    //"drinks" : {"$in" : [4, 77676.22]}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "drinks");
    bson_append_start_array(&bsq1, "$in");
    bson_append_int(&bsq1, "0", 4);
    bson_append_double(&bsq1, "1", 77676.22);
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'adrinks'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 21"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"4\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"77676.220000\" 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery15(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXNUM));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_double(&bsq1, "dblscore", 0.333333);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'ndblscore'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 8"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery16(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXDROPALL));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_double(&bsq1, "dblscore", 0.333333);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery17(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXNUM));

    //"dblscore" : {"$bt" : [0.95, 0.3]}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_start_array(&bsq1, "$bt");
    bson_append_double(&bsq1, "0", 0.95);
    bson_append_double(&bsq1, "1", 0.333333);
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);


    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", -1); //DESC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'ndblscore'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 13"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    //Second query
    tcxstrclear(log);
    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    ejdbquerydel(q1);

    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXDROPALL));

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_start_array(&bsq1, "$bt");
    bson_append_double(&bsq1, "0", 0.95);
    bson_append_double(&bsq1, "1", 0.333333);
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);


    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", 1); //ASC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery18(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "name", JBIDXARR));

    //{"name" : {$strand : ["Travolta", "John"]}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_start_array(&bsq1, "$strand");
    bson_append_string(&bsq1, "0", "Travolta");
    bson_append_string(&bsq1, "1", "John");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    /*bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", 1); //ASC order on name
    bson_append_finish_object(&bshints);*/
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'aname'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 4"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"John\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"Travolta\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    //Second query
    tcxstrclear(log);
    tclistdel(q1res);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "name", JBIDXDROPALL));

    count = 0;
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    ejdbquerydel(q1);

    //Third query
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "labels", JBIDXARR));

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "labels");
    bson_append_start_array(&bsq1, "$strand");
    bson_append_string(&bsq1, "0", "red");
    bson_append_string(&bsq1, "1", "black");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    count = 0;
    tcxstrclear(log);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery19(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "name", JBIDXARR));

    //{"name" : {$stror : ["Travolta", "Антонов", "John"]}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_start_array(&bsq1, "$stror");
    bson_append_string(&bsq1, "0", "Travolta");
    bson_append_string(&bsq1, "1", "Антонов");
    bson_append_string(&bsq1, "2", "John");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", -1); //DESC order on name
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'aname'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 5"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"John\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"Travolta\" 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"Антонов\" 1"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    //No-index query
    tcxstrclear(log);
    tclistdel(q1res);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "name", JBIDXDROPALL));

    count = 0;
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery20(void) {
    //dblscore
    //{'dblscore' : {'$gte' : 0.93}}
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXNUM));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_double(&bsq1, "$gte", 0.93);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", 1); //ASC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'ndblscore'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 10"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(1.0, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //GT

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_double(&bsq1, "$gt", 0.93);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", 1); //ASC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'ndblscore'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 9"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //NOINDEX
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXDROPALL));

    //NOINDEX GTE
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_double(&bsq1, "$gte", 0.93);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", -1); //DESC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
        } else if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(1.0, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery21(void) {
    //{'dblscore' : {'lte' : 0.93}}
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXNUM));

    //LTE
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_double(&bsq1, "$lte", 0.93);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", -1); //DESC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'ndblscore'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 12"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //LT
    //{'dblscore' : {'$lt' : 0.93}}
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_double(&bsq1, "$lt", 0.93);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", -1); //DESC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'ndblscore'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 11"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    CU_ASSERT_TRUE(ejdbsetindex(contacts, "dblscore", JBIDXDROPALL));

    //NOINDEX GTE
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "dblscore");
    bson_append_double(&bsq1, "$lte", 0.93);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", -1); //DESC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery22(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);
    CU_ASSERT_TRUE(ejdbsetindex(contacts, "address.country", JBIDXSTR));

    //{"address.country" : {$begin : "Ru"}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.country");
    bson_append_string(&bsq1, "$begin", "Ru");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", -1); //DESC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'saddress.country'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(1.0, TCLISTVALPTR(q1res, i), "dblscore"));
            CU_ASSERT_FALSE(bson_compare_string("Russian Federation", TCLISTVALPTR(q1res, i), "address.country"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
            CU_ASSERT_FALSE(bson_compare_string("Russian Federation", TCLISTVALPTR(q1res, i), "address.country"));
        } else if (i == 2) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
            CU_ASSERT_FALSE(bson_compare_string("Russian Federation", TCLISTVALPTR(q1res, i), "address.country"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    CU_ASSERT_TRUE(ejdbsetindex(contacts, "address.country", JBIDXDROPALL));

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.country");
    bson_append_string(&bsq1, "$begin", "R");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "dblscore", -1); //DESC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(1.0, TCLISTVALPTR(q1res, i), "dblscore"));
            CU_ASSERT_FALSE(bson_compare_string("Russian Federation", TCLISTVALPTR(q1res, i), "address.country"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.93, TCLISTVALPTR(q1res, i), "dblscore"));
            CU_ASSERT_FALSE(bson_compare_string("Russian Federation", TCLISTVALPTR(q1res, i), "address.country"));
        } else if (i == 2) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
            CU_ASSERT_FALSE(bson_compare_double(0.333333, TCLISTVALPTR(q1res, i), "dblscore"));
            CU_ASSERT_FALSE(bson_compare_string("Russian Federation", TCLISTVALPTR(q1res, i), "address.country"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery23(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_regex(&bsq1, "name", "(IvaNov$|John\\ TraVolta$)", "i");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", -1); //DESC order on dblscore
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        } else if (i == 1) {
            CU_ASSERT_FALSE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery24(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", -1); //DESC order on name
    bson_append_finish_object(&bshints);
    bson_append_long(&bshints, "$skip", 1);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == TCLISTNUM(q1res) - 1) {
            CU_ASSERT_FALSE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", -1); //DESC order on name
    bson_append_finish_object(&bshints);
    bson_append_long(&bshints, "$skip", 1);
    bson_append_long(&bshints, "$max", 2);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: YES"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == TCLISTNUM(q1res) - 1) {
            CU_ASSERT_FALSE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //No order specified
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_long(&bshints, "$skip", 1);
    bson_append_long(&bshints, "$max", 2);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_long(&bshints, "$skip", 4);
    bson_append_long(&bshints, "$max", 2);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 4"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 0);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 0);

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);


    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "name", 1); //ASC
    bson_append_finish_object(&bshints);
    bson_append_long(&bshints, "$skip", 3);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == TCLISTNUM(q1res) - 1) {
            CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));
        }
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshints);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

}

void testQuery25(void) { //$or
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson obs[2];
    bson_init_as_query(&obs[0]);
    bson_append_string(&obs[0], "name", "Ivanov");
    bson_finish(&obs[0]);
    CU_ASSERT_FALSE_FATAL(obs[0].err);

    bson_init_as_query(&obs[1]);
    bson_append_string(&obs[1], "name", "Антонов");
    bson_finish(&obs[1]);
    CU_ASSERT_FALSE_FATAL(obs[1].err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, obs, 2, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(
            !bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name") ||
            !bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));

    }
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    for (int i = 0; i < 2; ++i) {
        bson_destroy(&obs[i]);
    }
}

void testQuery25_2(void) { //$or alternative
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_array(&bsq1, "$or");

    bson_append_start_object(&bsq1, "0");
    bson_append_string(&bsq1, "name", "Ivanov");
    bson_append_finish_object(&bsq1);

    bson_append_start_object(&bsq1, "1");
    bson_append_string(&bsq1, "name", "Антонов");
    bson_append_finish_object(&bsq1);

    bson_append_finish_array(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(
            !bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name") ||
            !bson_compare_string("Антонов", TCLISTVALPTR(q1res, i), "name"));

    }
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery26(void) { //$not $nin
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    //{'address.city' : {$not : 'Novosibirsk'}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.city");
    bson_append_string(&bsq1, "$not", "Novosibirsk");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(bson_compare_string("Novosibirsk", TCLISTVALPTR(q1res, i), "address.city"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //Double negation {'address.city' : {$not : {'$not' : 'Novosibirsk'}}}
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.city");
    bson_append_start_object(&bsq1, "$not");
    bson_append_string(&bsq1, "$not", "Novosibirsk");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("Novosibirsk", TCLISTVALPTR(q1res, i), "address.city"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //"name" : {"$nin" : ["John Travolta", "Ivanov"]}
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_start_array(&bsq1, "$nin");
    bson_append_string(&bsq1, "0", "John Travolta");
    bson_append_string(&bsq1, "1", "Ivanov");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        CU_ASSERT_TRUE(bson_compare_string("Ivanov", TCLISTVALPTR(q1res, i), "name"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery27(void) { //$exists
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    //{'address.room' : {$exists : true}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.room");
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);


    //{'address.room' : {$exists : true}}
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.room");
    bson_append_bool(&bsq1, "$exists", false);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //{'address.room' : {$not :  {$exists : true}}} is equivalent to {'address.room' : {$exists : false}}
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "address.room");
    bson_append_start_object(&bsq1, "$not");
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAX: 4294967295"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SKIP: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "COUNT ONLY: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ORDER FIELDS: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$OR QUERIES: 0"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FETCH ALL: NO"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FINAL SORTING: NO"));
    CU_ASSERT_EQUAL(count, 3);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 3);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery28(void) { // $gte: 64 bit number
	// TEST for #127: int64_t large numbers
	int64_t int64value = 0xFFFFFFFFFF02LL;
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
	bson_append_start_object(&bsq1, "longscore");
    bson_append_long(&bsq1, "$gte", int64value);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) {
            CU_ASSERT_FALSE(bson_compare_string("444-123-333", TCLISTVALPTR(q1res, i), "phone"));
            CU_ASSERT_FALSE(bson_compare_long(int64value, TCLISTVALPTR(q1res, i), "longscore"));
        } else {
            CU_ASSERT_TRUE(false);
        }
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery29(void) { 
	// #129: Test $begin Query with Symbols
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "symbol");
    bson_append_string(&bsq1, "$begin", "app");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_EQUAL(count, 2);  // should match symbol_info: apple, application
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) CU_ASSERT_FALSE(bson_compare_string("apple", TCLISTVALPTR(q1res, i), "symbol"));
        if (i == 1) CU_ASSERT_FALSE(bson_compare_string("application", TCLISTVALPTR(q1res, i), "symbol"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery30(void) { 
	// #129: Test equal with Symbols
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "symbol", "bison");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_EQUAL(count, 1);  // should match symbol_info: bison
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) CU_ASSERT_FALSE(bson_compare_string("bison", TCLISTVALPTR(q1res, i), "symbol"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testQuery31(void) { 
	// #129: Test $in array Query with Symbols
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "symbol");
    bson_append_start_array(&bsq1, "$in");
    bson_append_string(&bsq1, "0", "apple");
    bson_append_string(&bsq1, "1", "bison");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_EQUAL(count, 2);  // should match symbol_info: apple, bison
    CU_ASSERT_TRUE(TCLISTNUM(q1res) == 2);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        if (i == 0) CU_ASSERT_FALSE(bson_compare_string("apple", TCLISTVALPTR(q1res, i), "symbol"));
        if (i == 1) CU_ASSERT_FALSE(bson_compare_string("bison", TCLISTVALPTR(q1res, i), "symbol"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testOIDSMatching(void) { //OID matching
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    bson_type bt;
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(contacts, q1, &count, 0, log);
    CU_ASSERT_TRUE(count > 0);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) { //first
        char soid[25];
        bson_oid_t *oid;
        void *bsdata = TCLISTVALPTR(q1res, i);
        bson_iterator it2;
        bt = bson_find_from_buffer(&it2, bsdata, JDBIDKEYNAME);
        CU_ASSERT_EQUAL_FATAL(bt, BSON_OID);
        oid = bson_iterator_oid(&it2);
        bson_oid_to_string(oid, soid);
        //fprintf(stderr, "\nOID: %s", soid);

        //OID in string form maching
        bson bsq2;
        bson_init_as_query(&bsq2);

        if (i % 2 == 0) {
            bson_append_string(&bsq2, JDBIDKEYNAME, soid);
        } else {
            bson_append_oid(&bsq2, JDBIDKEYNAME, oid);
        }

        bson_finish(&bsq2);
        CU_ASSERT_FALSE_FATAL(bsq2.err);

        TCXSTR *log2 = tcxstrnew();
        EJQ *q2 = ejdbcreatequery(jb, &bsq2, NULL, 0, NULL);
        TCLIST *q2res = ejdbqryexecute(contacts, q2, &count, 0, log2);
        CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log2), "PRIMARY KEY MATCHING:"));
        CU_ASSERT_EQUAL(count, 1);

        tcxstrdel(log2);
        ejdbquerydel(q2);
        tclistdel(q2res);
        bson_destroy(&bsq2);
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testEmptyFieldIndex(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXDROPALL));

    bson a1;
    bson_oid_t oid;
    bson_init(&a1);
    bson_append_string(&a1, "name", ""); //Empty but indexed field
    CU_ASSERT_FALSE_FATAL(a1.err);
    bson_finish(&a1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &a1, &oid));
    bson_destroy(&a1);
    CU_ASSERT_EQUAL(ejdbecode(coll->jb), 0);

    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXISTR)); //Ignore case string index
    CU_ASSERT_EQUAL(ejdbecode(coll->jb), 0);

    bson_init(&a1);
    bson_append_string(&a1, "name", ""); //Empty but indexed field
    CU_ASSERT_FALSE_FATAL(a1.err);
    bson_finish(&a1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &a1, &oid));
    bson_destroy(&a1);
    CU_ASSERT_EQUAL(ejdbecode(coll->jb), 0);
}

void testICaseIndex(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXISTR)); //Ignore case string index

    //Save one more record
    bson a1;
    bson_oid_t oid;
    bson_init(&a1);
    bson_append_string(&a1, "name", "HeLlo WorlD"); //#1
    CU_ASSERT_FALSE_FATAL(a1.err);
    bson_finish(&a1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &a1, &oid));
    bson_destroy(&a1);
    CU_ASSERT_EQUAL(ejdbecode(coll->jb), 0);

    bson_init(&a1);
    bson_append_string(&a1, "name", "THéÂtRE — театр"); //#2
    CU_ASSERT_FALSE_FATAL(a1.err);
    bson_finish(&a1);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &a1, &oid));
    bson_destroy(&a1);
    CU_ASSERT_EQUAL(ejdbecode(coll->jb), 0);


    //Case insensitive query using index
    // {"name" : {"$icase" : "HellO woRLD"}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_string(&bsq1, "$icase", "HellO woRLD");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_TRUE(count == 1);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'iname'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("HeLlo WorlD", TCLISTVALPTR(q1res, i), "name"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //OK then drop icase index
    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXISTR | JBIDXDROP)); //Ignore case string index

    //Same query:
    //{"name" : {"$icase" : {$in : ["théâtre - театр", "hello world"]}}}
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_start_object(&bsq1, "$icase");
    bson_append_start_array(&bsq1, "$in");
    bson_append_string(&bsq1, "0", "théâtre - театр");
    bson_append_string(&bsq1, "1", "hello world");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_TRUE(count == 2);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(
            !bson_compare_string("HeLlo WorlD", TCLISTVALPTR(q1res, i), "name") ||
            !bson_compare_string("THéÂtRE — театр", TCLISTVALPTR(q1res, i), "name")
        );
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testTicket7(void) { //https://github.com/Softmotions/ejdb/issues/7
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    char xoid[25];
    bson_iterator it;
    bson_type bt;
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    const int onum = 3; //number of saved bsons
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_TRUE_FATAL(count >= onum);

    for (int i = 0; i < TCLISTNUM(q1res) && i < onum; ++i) {
        void *bsdata = TCLISTVALPTR(q1res, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);
    }
    //Now perform $in qry
    //{_id : {$in : ["oid1", "oid2", "oid3"]}}
    bson bsq2;
    bson_init_as_query(&bsq2);
    bson_append_start_object(&bsq2, "_id");
    bson_append_start_array(&bsq2, "$in");
    for (int i = 0; i < onum; ++i) {
        char ibuf[10];
        snprintf(ibuf, 10, "%d", i);
        bson_oid_t *oid = NULL;
        bt = bson_find_from_buffer(&it, TCLISTVALPTR(q1res, i), "_id");
        CU_ASSERT_TRUE_FATAL(bt == BSON_OID);
        oid = bson_iterator_oid(&it);
        CU_ASSERT_PTR_NOT_NULL_FATAL(oid);
        bson_oid_to_string(oid, xoid);
        //fprintf(stderr, "\ni=%s oid=%s", ibuf, xoid);
        if (i % 2 == 0) {
            bson_append_oid(&bsq2, ibuf, oid);
        } else {
            bson_append_string(&bsq2, ibuf, xoid);
        }
    }
    bson_append_finish_array(&bsq2);
    bson_append_finish_object(&bsq2);
    bson_finish(&bsq2);
    CU_ASSERT_FALSE_FATAL(bsq2.err);

    EJQ *q2 = ejdbcreatequery(jb, &bsq2, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q2);
    uint32_t count2 = 0;
    TCXSTR *log2 = tcxstrnew();
    TCLIST *q2res = ejdbqryexecute(coll, q2, &count2, 0, log2);
    //fprintf(stderr, "\n%s", TCXSTRPTR(log2));
    CU_ASSERT_TRUE(count2 == 3);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log2), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log2), "PRIMARY KEY MATCHING: TRUE"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log2), "RS COUNT: 3"));

    for (int i = 0; i < TCLISTNUM(q2res); ++i) {
        bson_oid_t *oid1 = NULL;
        bt = bson_find_from_buffer(&it, TCLISTVALPTR(q2res, i), "_id");
        CU_ASSERT_TRUE_FATAL(bt == BSON_OID);
        oid1 = bson_iterator_oid(&it);
        bool matched = false;
        for (int j = 0; j < TCLISTNUM(q1res); ++j) {
            bson_oid_t *oid2 = NULL;
            bt = bson_find_from_buffer(&it, TCLISTVALPTR(q1res, j), "_id");
            CU_ASSERT_TRUE_FATAL(bt == BSON_OID);
            oid2 = bson_iterator_oid(&it);
            if (!memcmp(oid1, oid2, sizeof (bson_oid_t))) {
                matched = true;
                void *ptr = tclistremove2(q1res, j);
                if (ptr) {
                    TCFREE(ptr);
                }
                break;
            }
        }
        CU_ASSERT_TRUE(matched);
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_destroy(&bsq2);
    tclistdel(q2res);
    tcxstrdel(log2);
    ejdbquerydel(q2);
}

void testTicket8(void) { //https://github.com/Softmotions/ejdb/issues/8
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson bshits1;
    bson_init_as_query(&bshits1);
    bson_append_start_object(&bshits1, "$fields");
    bson_append_int(&bshits1, "_id", 1);
    bson_append_int(&bshits1, "phone", 1);
    bson_append_int(&bshits1, "address.city", 1);
    bson_append_int(&bshits1, "labels", 1);
    bson_append_finish_object(&bshits1);
    bson_finish(&bshits1);
    CU_ASSERT_FALSE_FATAL(bshits1.err);


    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshits1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    //    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
    //        void *bsdata = TCLISTVALPTR(q1res, i);
    //        bson_print_raw(bsdata, 0);
    //    }

    bson_type bt;
    bson_iterator it;
    int ccount = 0;
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        void *bsdata = TCLISTVALPTR(q1res, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);

        if (!bson_compare_string("333-222-333", TCLISTVALPTR(q1res, i), "phone")) {
            ++ccount;
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("_id", &it);
            CU_ASSERT_TRUE(bt == BSON_OID);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("address", &it);
            CU_ASSERT_TRUE(bt == BSON_OBJECT);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("address.city", &it);
            CU_ASSERT_TRUE(bt == BSON_STRING);
            CU_ASSERT_FALSE(strcmp("Novosibirsk", bson_iterator_string(&it)));
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("address.zip", &it);
            CU_ASSERT_TRUE(bt == BSON_EOO);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("age", &it);
            CU_ASSERT_TRUE(bt == BSON_EOO);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("name", &it);
            CU_ASSERT_TRUE(bt == BSON_EOO);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("labels", &it);
            CU_ASSERT_TRUE(bt == BSON_EOO);
        } else if (!bson_compare_string("444-123-333", TCLISTVALPTR(q1res, i), "phone")) {
            ++ccount;
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("_id", &it);
            CU_ASSERT_TRUE(bt == BSON_OID);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("address", &it);
            CU_ASSERT_TRUE(bt == BSON_OBJECT);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("address.city", &it);
            CU_ASSERT_TRUE(bt == BSON_STRING);
            CU_ASSERT_FALSE(strcmp("Novosibirsk", bson_iterator_string(&it)));
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("address.zip", &it);
            CU_ASSERT_TRUE(bt == BSON_EOO);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("age", &it);
            CU_ASSERT_TRUE(bt == BSON_EOO);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("name", &it);
            CU_ASSERT_TRUE(bt == BSON_EOO);
            bson_iterator_from_buffer(&it, bsdata);
            bt = bson_find_fieldpath_value("labels", &it);
            CU_ASSERT_TRUE(bt == BSON_ARRAY);
            CU_ASSERT_FALSE(bson_compare_string("red", bsdata, "labels.0"));
            CU_ASSERT_FALSE(bson_compare_string("green", bsdata, "labels.1"));
            CU_ASSERT_FALSE(bson_compare_string("with gap, label", bsdata, "labels.2"));
        }
    }
    CU_ASSERT_TRUE(ccount == 2);
    bson_destroy(&bshits1);
    tclistdel(q1res);
    tcxstrclear(log);
    ejdbquerydel(q1);

    //NEXT Q
    bson_init_as_query(&bshits1);
    bson_append_start_object(&bshits1, "$fields");
    bson_append_int(&bshits1, "phone", 1);
    bson_append_int(&bshits1, "address.city", 0);
    bson_append_int(&bshits1, "labels", 0);
    bson_append_finish_object(&bshits1);
    bson_finish(&bshits1);
    CU_ASSERT_FALSE_FATAL(bshits1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshits1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(ejdbecode(jb), JBEQINCEXCL);
    ejdbquerydel(q1);
    tcxstrclear(log);
    bson_destroy(&bshits1);
    bson_destroy(&bsq1);

    //NEXT Q
    bson_init_as_query(&bsq1);
    //bson_append_string(&bsq1, "name", "Антонов");
    bson_append_start_object(&bsq1, "address");
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    bson_init_as_query(&bshits1);
    bson_append_start_object(&bshits1, "$fields");
    bson_append_int(&bshits1, "phone", 0);
    bson_append_int(&bshits1, "address.city", 0);
    bson_append_int(&bshits1, "address.room", 0);
    bson_append_int(&bshits1, "labels", 0);
    bson_append_int(&bshits1, "complexarr.0", 0);
    bson_append_int(&bshits1, "_id", 0);
    bson_append_finish_object(&bshits1);
    bson_finish(&bshits1);
    CU_ASSERT_FALSE_FATAL(bshits1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshits1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(ejdbecode(jb), TCESUCCESS);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        void *bsdata = TCLISTVALPTR(q1res, i);
        bson_type bt;
        bson_iterator_from_buffer(&it, bsdata);
        bt = bson_find_fieldpath_value("_id", &it);
        CU_ASSERT_TRUE(bt == BSON_EOO);
        bson_iterator_from_buffer(&it, bsdata);
        bt = bson_find_fieldpath_value("phone", &it);
        CU_ASSERT_TRUE(bt == BSON_EOO);
        bson_iterator_from_buffer(&it, bsdata);
        bt = bson_find_fieldpath_value("address", &it);
        CU_ASSERT_TRUE(bt == BSON_OBJECT);
        bson_iterator_from_buffer(&it, bsdata);
        //bt = bson_find_fieldpath_value("address.country", &it);
        //CU_ASSERT_TRUE(bt == BSON_STRING);
        bson_iterator_from_buffer(&it, bsdata);
        bt = bson_find_fieldpath_value("address.city", &it);
        CU_ASSERT_TRUE(bt == BSON_EOO);
        bson_iterator_from_buffer(&it, bsdata);
        bt = bson_find_fieldpath_value("address.room", &it);
        CU_ASSERT_TRUE(bt == BSON_EOO);
        bson_iterator_from_buffer(&it, bsdata);
        bt = bson_find_fieldpath_value("labels", &it);
        CU_ASSERT_TRUE(bt == BSON_EOO);
        //bson_print_raw(bsdata, 0);
    }

    bson_destroy(&bsq1);
    bson_destroy(&bshits1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

}

void testUpdate1(void) { //https://github.com/Softmotions/ejdb/issues/9
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson_iterator it;

    //q: {name : 'John Travolta', $set : {'labels' : ['black', 'blue'], 'age' : 58}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_start_object(&bsq1, "$set");
    bson_append_start_array(&bsq1, "labels");
    bson_append_string(&bsq1, "0", "black");
    bson_append_string(&bsq1, "1", "blue");
    bson_append_finish_array(&bsq1);
    bson_append_int(&bsq1, "age", 58);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_TRUE(ejdbecode(jb) == 0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_TRUE(ejdbecode(jb) == 0);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(1, count);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        bson_iterator_from_buffer(&it, TCLISTVALPTR(q1res, i));
        CU_ASSERT_TRUE(bson_find_from_buffer(&it, TCLISTVALPTR(q1res, i), "age") == BSON_EOO);
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //q2: {name : 'John Travolta', age: 58}
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_int(&bsq1, "age", 58);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(1, TCLISTNUM(q1res));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: NO"));

    int age = 58;
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_long(age, TCLISTVALPTR(q1res, i), "age"));
        CU_ASSERT_FALSE(bson_compare_string("black", TCLISTVALPTR(q1res, i), "labels.0"));
        CU_ASSERT_FALSE(bson_compare_string("blue", TCLISTVALPTR(q1res, i), "labels.1"));
    }
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //q3: {name : 'John Travolta', '$inc' : {'age' : -1}}
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_start_object(&bsq1, "$inc");
    bson_append_int(&bsq1, "age", -1);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);


    //q4: {name : 'John Travolta', age: 57}
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_int(&bsq1, "age", 57);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(1, TCLISTNUM(q1res));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: NO"));

    --age;
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_long(age, TCLISTVALPTR(q1res, i), "age"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
	
}

void testUpdate2(void) { //https://github.com/Softmotions/ejdb/issues/9
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "age", JBIDXNUM));

    //q: {name : 'John Travolta', '$inc' : {'age' : 1}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_start_object(&bsq1, "$inc");
    bson_append_int(&bsq1, "age", 1);
    bson_append_finish_object(&bsq1);
    bson_append_start_object(&bsq1, "$set");
    bson_append_bool(&bsq1, "visited", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_int(&bsq1, "age", 58);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'nage'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 8"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_bool(true, TCLISTVALPTR(q1res, i), "visited"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
    
    //test nested $inc #120
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_start_object(&bsq1, "$inc");
    bson_append_int(&bsq1, "number.of.coins", 22);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);
    
    //check test nested $inc #120
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL_FATAL(count, 1);
    
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_long(22, TCLISTVALPTR(q1res, i), "number.of.coins"));
    }
    
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testUpdate3(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

	//q5: {name: 'John Travolta', $rename: { "name" : "fullName"}}
    bson bsq1;
    bson_iterator it;

	bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "John Travolta");
    bson_append_start_object(&bsq1, "$rename");
    bson_append_string(&bsq1, "name", "fullName");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(1, count);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));


    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "fullName", "John Travolta");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "fullName"));
        bson_iterator_from_buffer(&it, TCLISTVALPTR(q1res, i));
        CU_ASSERT_TRUE(bson_find_from_buffer(&it, TCLISTVALPTR(q1res, i), "name") == BSON_EOO);
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
	
	//q6: {name: 'John Travolta', $rename: { "fullName" : "name"}}
	bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "fullName", "John Travolta");
    bson_append_start_object(&bsq1, "$rename");
    bson_append_string(&bsq1, "fullName", "name");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(1, count);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "fullName", "John Travolta");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
        bson_iterator_from_buffer(&it, TCLISTVALPTR(q1res, i));
        CU_ASSERT_TRUE(bson_find_from_buffer(&it, TCLISTVALPTR(q1res, i), "fullName") == BSON_EOO);
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
	
}

void testTicket88(void) { //https://github.com/Softmotions/ejdb/issues/88
    EJCOLL *ccoll = ejdbcreatecoll(jb, "ticket88", NULL);
    CU_ASSERT_PTR_NOT_NULL(ccoll);

    bson r;
    bson_oid_t oid;
    for (int i = 0; i < 10; ++i) {
        bson_init(&r);
        bson_append_start_array(&r, "arr1");
        bson_append_start_object(&r, "0");
        bson_append_int(&r, "f1", 1 + i);
        bson_append_finish_object(&r);
        bson_append_start_object(&r, "1");
        bson_append_int(&r, "f1", 2 + i);
        bson_append_finish_object(&r);
        bson_append_finish_array(&r);
        bson_finish(&r);
        CU_ASSERT_TRUE(ejdbsavebson(ccoll, &r, &oid));
        bson_destroy(&r);
    }

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$set");
    bson_append_string(&bsq1, "arr1.0.f2", "x");
    bson_append_int(&bsq1, "arr1.1", 1111);
    bson_append_string(&bsq1, "a.b", "c");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    ejdbqryexecute(ccoll, q1, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_TRUE(ejdbecode(jb) == 0);
    CU_ASSERT_EQUAL(count, 10);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);

    bson bsq2;
    bson_init_as_query(&bsq2);
    bson_append_string(&bsq2, "arr1.0.f2", "x");
    bson_append_int(&bsq2, "arr1.1", 1111);
    bson_append_string(&bsq2, "a.b", "c");
    bson_finish(&bsq2);
    q1 = ejdbcreatequery(jb, &bsq2, NULL, 0, NULL);
    ejdbqryexecute(ccoll, q1, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_EQUAL(count, 10);
    ejdbquerydel(q1);
    bson_destroy(&bsq2);
}

void testTicket89(void) { //https://github.com/Softmotions/ejdb/issues/89
    EJCOLL *ccoll = ejdbcreatecoll(jb, "ticket89", NULL);
    CU_ASSERT_PTR_NOT_NULL(ccoll);

    bson r;
    bson_oid_t oid;

    bson_init(&r);
    //{"test":[["aaa"],["bbb"]]}
    bson_append_start_array(&r, "test");
    bson_append_start_array(&r, "0");
    bson_append_string(&r, "0", "aaa");
    bson_append_finish_array(&r);
    bson_append_start_array(&r, "1");
    bson_append_string(&r, "0", "bbb");
    bson_append_finish_array(&r);
    bson_append_finish_array(&r);
    bson_finish(&r);
    CU_ASSERT_TRUE(ejdbsavebson(ccoll, &r, &oid));
    bson_destroy(&r);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$addToSet");
    bson_append_string(&bsq1, "test.0", "bbb");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    ejdbqryexecute(ccoll, q1, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_TRUE(ejdbecode(jb) == 0);
    CU_ASSERT_EQUAL(count, 1);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "test.0.1", "bbb");
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    ejdbqryexecute(ccoll, q1, &count, JBQRYCOUNT, NULL);
    CU_ASSERT_TRUE(ejdbecode(jb) == 0);
    CU_ASSERT_EQUAL(count, 1);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);
}

void testQueryBool(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_bool(&bsq1, "visited", true);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);


    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_EQUAL(count, 1);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    CU_ASSERT_TRUE(ejdbsetindex(coll, "visited", JBIDXNUM));

    bson_init_as_query(&bsq1);
    bson_append_bool(&bsq1, "visited", true);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'nvisited'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_EQUAL(count, 1);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testDropAll(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXSTR));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "HeLlo WorlD");
    bson_append_bool(&bsq1, "$dropall", true);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);


    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$DROPALL ON:"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'sname'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //Select again
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "HeLlo WorlD");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'sname'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 0"));
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testTokensBegin(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXSTR));

    //q: {'name' : {'$begin' : ['Ада', 'John T']}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_start_array(&bsq1, "$begin");
    bson_append_string(&bsq1, "0", "Ада");
    bson_append_string(&bsq1, "1", "John T");
    bson_append_string(&bsq1, "2", "QWE J");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'sname'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX TCOP: 22"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name") ||
                       !bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //q: {'name' : {'$begin' : ['Ада', 'John T']}}
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_start_array(&bsq1, "$begin");
    bson_append_string(&bsq1, "0", "Ада");
    bson_append_string(&bsq1, "1", "John T");
    bson_append_string(&bsq1, "2", "QWE J");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);


    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXDROPALL));

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'NONE'"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RUN FULLSCAN"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 2"));
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("Адаманский", TCLISTVALPTR(q1res, i), "name") ||
                       !bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testOneFieldManyConditions(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "age");
    bson_append_int(&bsq1, "$lt", 60);
    bson_append_int(&bsq1, "$gt", 50);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(count, 1);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_string("John Travolta", TCLISTVALPTR(q1res, i), "name"));
    }
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testAddToSet(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_append_start_object(&bsq1, "$addToSet");
    bson_append_string(&bsq1, "personal.tags", "tag1");
    bson_append_string(&bsq1, "labels", "green");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //check updated  data
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    \
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("tag1", TCLISTVALPTR(q1res, i), "personal.tags.0"));
        CU_ASSERT_FALSE(bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.0"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //Uppend more vals
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_append_start_object(&bsq1, "$addToSet");
    bson_append_string(&bsq1, "personal.tags", "tag2");
    bson_append_string(&bsq1, "labels", "green");
    //bson_append_int(&bsq1, "scores", 1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);


    //check updated  data
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    \
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        //bson_print_raw(TCLISTVALPTR(q1res, i), 0);
        CU_ASSERT_FALSE(bson_compare_string("tag1", TCLISTVALPTR(q1res, i), "personal.tags.0"));
        CU_ASSERT_FALSE(bson_compare_string("tag2", TCLISTVALPTR(q1res, i), "personal.tags.1"));
        CU_ASSERT_FALSE(bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.0"));
        CU_ASSERT_FALSE(!bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.1"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //Uppend more vals
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_append_start_object(&bsq1, "$inc");
    bson_append_int(&bsq1, "age", -1);
    bson_append_finish_object(&bsq1);
    bson_append_start_object(&bsq1, "$addToSet");
    bson_append_string(&bsq1, "personal.tags", "tag3");
    bson_append_finish_object(&bsq1); //EOF $addToSet
    bson_append_start_object(&bsq1, "$addToSetAll");
    bson_append_start_array(&bsq1, "labels");
    bson_append_string(&bsq1, "0", "red");
    bson_append_string(&bsq1, "1", "black");
    bson_append_string(&bsq1, "2", "green");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1); //EOF $addToSetAll
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    //check again
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    \
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        //bson_print_raw(TCLISTVALPTR(q1res, i), 0);
        CU_ASSERT_FALSE(bson_compare_string("tag1", TCLISTVALPTR(q1res, i), "personal.tags.0"));
        CU_ASSERT_FALSE(bson_compare_string("tag2", TCLISTVALPTR(q1res, i), "personal.tags.1"));
        CU_ASSERT_FALSE(bson_compare_string("tag3", TCLISTVALPTR(q1res, i), "personal.tags.2"));
        CU_ASSERT_FALSE(bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.0"));
        CU_ASSERT_FALSE(bson_compare_string("red", TCLISTVALPTR(q1res, i), "labels.1"));
        CU_ASSERT_FALSE(bson_compare_string("black", TCLISTVALPTR(q1res, i), "labels.2"));
        CU_ASSERT_TRUE(bson_compare_string("green", TCLISTVALPTR(q1res, i), "labels.3"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testTicket123(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket123", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson bs1;
    bson_oid_t oid1;
    bson_init(&bs1);
    bson_append_start_object(&bs1, "abc");
    bson_append_start_array(&bs1, "de");
    bson_append_string(&bs1, "0", "g");
    bson_append_finish_array(&bs1);
    bson_append_start_array(&bs1, "fg");
    bson_append_finish_array(&bs1);
    bson_append_finish_object(&bs1);
    bson_finish(&bs1);

    CU_ASSERT_TRUE_FATAL(ejdbsavebson(coll, &bs1, &oid1));

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$addToSet");
    bson_append_string(&bsq1, "abc.g", "f");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //check updated  data
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("g", TCLISTVALPTR(q1res, i), "abc.de.0"));
        CU_ASSERT_FALSE(bson_compare_string("f", TCLISTVALPTR(q1res, i), "abc.g.0"));
    }

    bson_destroy(&bs1);
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testPush(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket123", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$push");
    bson_append_string(&bsq1, "abc.g", "f");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);
    
    //check updated  data
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("g", TCLISTVALPTR(q1res, i), "abc.de.0"));
        CU_ASSERT_FALSE(bson_compare_string("f", TCLISTVALPTR(q1res, i), "abc.g.0"));
        CU_ASSERT_FALSE(bson_compare_string("f", TCLISTVALPTR(q1res, i), "abc.g.1"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}


void testPushAll(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket123", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$pushAll");
    bson_append_start_array(&bsq1, "abc.g");
    bson_append_string(&bsq1, "0", "h");
    bson_append_string(&bsq1, "1", "h");
    bson_append_int(&bsq1, "2", 11);
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);
    
    //check updated  data
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("g", TCLISTVALPTR(q1res, i), "abc.de.0"));
        CU_ASSERT_FALSE(bson_compare_string("f", TCLISTVALPTR(q1res, i), "abc.g.0"));
        CU_ASSERT_FALSE(bson_compare_string("f", TCLISTVALPTR(q1res, i), "abc.g.1"));
        CU_ASSERT_FALSE(bson_compare_string("h", TCLISTVALPTR(q1res, i), "abc.g.2"));
        CU_ASSERT_FALSE(bson_compare_string("h", TCLISTVALPTR(q1res, i), "abc.g.3"));
        CU_ASSERT_FALSE(bson_compare_long(11, TCLISTVALPTR(q1res, i), "abc.g.4"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testRename2(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket123", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$rename");
    bson_append_string(&bsq1, "abc.g", "abc.f");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);
    
    //check updated  data
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("g", TCLISTVALPTR(q1res, i), "abc.de.0"));
        CU_ASSERT_FALSE(bson_compare_string("f", TCLISTVALPTR(q1res, i), "abc.f.0"));
        CU_ASSERT_FALSE(bson_compare_string("f", TCLISTVALPTR(q1res, i), "abc.f.1"));
        CU_ASSERT_FALSE(bson_compare_string("h", TCLISTVALPTR(q1res, i), "abc.f.2"));
        CU_ASSERT_FALSE(bson_compare_string("h", TCLISTVALPTR(q1res, i), "abc.f.3"));
        CU_ASSERT_FALSE(bson_compare_long(11, TCLISTVALPTR(q1res, i), "abc.f.4"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}


void testPull(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_append_start_object(&bsq1, "$pull");
    bson_append_string(&bsq1, "personal.tags", "tag2");
    bson_append_string(&bsq1, "labels", "green");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "UPDATING MODE: YES"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //check
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "Антонов");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    //fprintf(stderr, "\n\n%s", TCXSTRPTR(log));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        //bson_print_raw(TCLISTVALPTR(q1res, i), 0);
        CU_ASSERT_FALSE(bson_compare_string("tag1", TCLISTVALPTR(q1res, i), "personal.tags.0"));
        CU_ASSERT_FALSE(bson_compare_string("tag3", TCLISTVALPTR(q1res, i), "personal.tags.1"));
        CU_ASSERT_FALSE(bson_compare_string("red", TCLISTVALPTR(q1res, i), "labels.0"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testFindInComplexArray(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "complexarr.key", "title");
    bson_append_string(&bsq1, "complexarr.value", "some title");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 2);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    // 1
    CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, 1), "name"));
    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 1), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 1), "complexarr.0.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //Check matching positional element
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "complexarr.0.key", "title");
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "\n%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 2);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    // 1
    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, 1), "name"));
    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 1), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 1), "complexarr.0.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //Check simple el
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "complexarr.2", 333);
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "\n%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    CU_ASSERT_FALSE(bson_compare_long(333, TCLISTVALPTR(q1res, 0), "complexarr.2"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //Check simple el2
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "complexarr", 333);
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "\n%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    CU_ASSERT_FALSE(bson_compare_long(333, TCLISTVALPTR(q1res, 0), "complexarr.2"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);


    //$exists
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr.key");
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 2);
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);


    //$exists 2
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr.2");
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 1);
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);


    //$exists 3
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr.4");
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 0);
    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testElemMatch(void) {
    // { complexarr: { $elemMatch: { key: 'title', value: 'some title' } } }
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_string(&bsq1, "key", "title");
    bson_append_string(&bsq1, "value", "some title");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL_FATAL(count, 2);
//    fprintf(stderr, "%s", TCXSTRPTR(log));

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    // 1
    CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, 1), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 1), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 1), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("comment", TCLISTVALPTR(q1res, 1), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some comment", TCLISTVALPTR(q1res, 1), "complexarr.1.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_string(&bsq1, "key", "title");
    bson_append_string(&bsq1, "value", "some other title");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_string(&bsq1, "key", "title");
    bson_append_start_object(&bsq1, "value");
    bson_append_string(&bsq1, "$not", "some title");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
//    fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "complexarr.key", "title");
    bson_append_string(&bsq1, "complexarr.value", "some other title");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testNotElemMatch(void) {
    // { complexarr: { $not: { $elemMatch: { key: 'title', value: 'some title' } } } }
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr");
    bson_append_start_object(&bsq1, "$not");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_string(&bsq1, "key", "title");
    bson_append_string(&bsq1, "value", "some title");
    bson_append_finish_object(&bsq1);//$elemMatch
    bson_append_finish_object(&bsq1);//$not
    //include $exists to exclude documents without complexarr
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);//complexarr
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 0);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr");
    bson_append_start_object(&bsq1, "$not");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_string(&bsq1, "key", "title");
    bson_append_string(&bsq1, "value", "some other title");
    bson_append_finish_object(&bsq1);//$elemMatch
    bson_append_finish_object(&bsq1);//$not
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("comment", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some comment", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr");
    bson_append_start_object(&bsq1, "$not");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_string(&bsq1, "key", "comment");
    bson_append_finish_object(&bsq1);//$elemMatch
    bson_append_finish_object(&bsq1);//$not
    bson_append_bool(&bsq1, "$exists", true);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Адаманский", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some other title", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //ensure correct behaviour of double negative
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "complexarr");
    bson_append_start_object(&bsq1, "$not");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_string(&bsq1, "key", "title");
    bson_append_start_object(&bsq1, "value");
    bson_append_string(&bsq1, "$not", "some title");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL_FATAL(count, 1);

    // 0
    CU_ASSERT_FALSE(bson_compare_string("Антонов", TCLISTVALPTR(q1res, 0), "name"));

    CU_ASSERT_FALSE(bson_compare_string("title", TCLISTVALPTR(q1res, 0), "complexarr.0.key"));
    CU_ASSERT_FALSE(bson_compare_string("some title", TCLISTVALPTR(q1res, 0), "complexarr.0.value"));

    CU_ASSERT_FALSE(bson_compare_string("comment", TCLISTVALPTR(q1res, 0), "complexarr.1.key"));
    CU_ASSERT_FALSE(bson_compare_string("some comment", TCLISTVALPTR(q1res, 0), "complexarr.1.value"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testTicket16(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "abcd", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    CU_ASSERT_EQUAL(coll->tdb->inum, 0);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "abcd", JBIDXISTR));
    CU_ASSERT_TRUE(ejdbsetindex(coll, "abcd", JBIDXNUM));
    CU_ASSERT_EQUAL(coll->tdb->inum, 2);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "abcd", JBIDXDROPALL));
    CU_ASSERT_EQUAL(coll->tdb->inum, 0);
}

void testUpsert(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "abcd", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "cde", "fgh"); //sel condition
    bson_append_start_object(&bsq1, "$upsert");
    bson_append_string(&bsq1, "cde", "fgh");
    bson_append_string(&bsq1, "ijk", "lmnp");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        bson_iterator it;
        CU_ASSERT_TRUE(bson_find_from_buffer(&it, TCLISTVALPTR(q1res, i), "_id") == BSON_OID);
        CU_ASSERT_FALSE(bson_compare_string("fgh", TCLISTVALPTR(q1res, i), "cde"));
        CU_ASSERT_FALSE(bson_compare_string("lmnp", TCLISTVALPTR(q1res, i), "ijk"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "cde", "fgh"); //sel condition
    bson_append_start_object(&bsq1, "$upsert");
    bson_append_string(&bsq1, "cde", "fgh");
    bson_append_string(&bsq1, "ijk", "lmnp+");
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    log = tcxstrnew();
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(count, 1);

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "cde", "fgh");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    log = tcxstrnew();
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(count, 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("fgh", TCLISTVALPTR(q1res, i), "cde"));
        CU_ASSERT_FALSE(bson_compare_string("lmnp+", TCLISTVALPTR(q1res, i), "ijk"));
    }

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testPrimitiveCases1(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "abcd", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_EQUAL(count, 1);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "SIMPLE COUNT(*): 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));

    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);

    //$dropall on whole collection
    bson_init_as_query(&bsq1);
    bson_append_bool(&bsq1, "$dropall", true);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    count = 0;
    log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_EQUAL(count, 1);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "VANISH WHOLE COLLECTION ON $dropall"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS COUNT: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "RS SIZE: 0"));

    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testTicket29(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "name", JBIDXARR));

    bson a1;
    bson_init(&a1);
    bson_append_string(&a1, "name", "Hello Мир");
    bson_append_long(&a1, "longscore", 77);
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);

    bson_oid_t oid;
    CU_ASSERT_TRUE(ejdbsavebson(coll, &a1, &oid));
    bson_destroy(&a1);

    //{"name" : {$strand : ["Hello", "Мир"]}}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "name");
    bson_append_start_array(&bsq1, "$strand");
    bson_append_string(&bsq1, "0", "Hello");
    bson_append_string(&bsq1, "1", "Мир");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 1);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "token occurrence: \"Hello\" 1"));

    bson_destroy(&bsq1);
    tclistdel(q1res);
    tcxstrdel(log);
    ejdbquerydel(q1);
}

void testTicket28(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    //{$some:2}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "$some", 2);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_EQUAL(ejdbecode(jb), JBEQERROR);
    CU_ASSERT_PTR_NULL(q1);
    bson_destroy(&bsq1);

    //Second step
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_EQUAL(ejdbecode(jb), TCESUCCESS);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);

    bson_destroy(&bsq1);
    tcxstrdel(log);
    ejdbquerydel(q1);
    tclistdel(q1res);
}

void testTicket38(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket38", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    //R: {a: [ 'b', 'c', 'ddd', 3 ]}
    bson r1;
    bson_init(&r1);
    bson_append_start_array(&r1, "a");
    bson_append_string(&r1, "0", "b");
    bson_append_string(&r1, "1", "c");
    bson_append_string(&r1, "2", "ddd");
    bson_append_int(&r1, "3", 3);
    bson_append_finish_array(&r1);
    bson_finish(&r1);
    CU_ASSERT_FALSE_FATAL(r1.err);

    bson_oid_t oid;
    CU_ASSERT_TRUE(ejdbsavebson(coll, &r1, &oid));
    bson_destroy(&r1);

    //Q: {$pullAll:{a:[3, 2, 'c']}
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_object(&bsq1, "$pullAll");
    bson_append_start_array(&bsq1, "a");
    bson_append_int(&bsq1, "0", 3);
    bson_append_int(&bsq1, "1", 2);
    bson_append_string(&bsq1, "2", "c");
    bson_append_finish_array(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));

    tcxstrdel(log);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);

    //Q: {}
    bson_init_as_query(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        void *bsdata = TCLISTVALPTR(q1res, i);
        bson_iterator it;
        bson_iterator_from_buffer(&it, bsdata);
        bson_type bt = bson_find_fieldpath_value("_id", &it);
        CU_ASSERT_EQUAL(bt, BSON_OID);
        CU_ASSERT_FALSE(bson_compare_string("b", TCLISTVALPTR(q1res, i), "a.0"));
        CU_ASSERT_FALSE(bson_compare_string("ddd", TCLISTVALPTR(q1res, i), "a.1"));
        bt = bson_find_fieldpath_value("a.2", &it);
        CU_ASSERT_EQUAL(bt, BSON_EOO);
        bt = bson_find_fieldpath_value("a.3", &it);
        CU_ASSERT_EQUAL(bt, BSON_EOO);
    }

    tcxstrdel(log);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);
    tclistdel(q1res);
}

void testTicket43(void) {

    EJCOLL *coll = ejdbcreatecoll(jb, "ticket43", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    EJCOLL *rcoll = ejdbcreatecoll(jb, "ticket43_refs", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(rcoll);

    bson a1;
    bson_oid_t oid;
    bson_oid_t ref_oids[2];
    char xoid[25];

    bson_init(&a1);
    bson_append_string(&a1, "name", "n1");
    bson_append_string(&a1, "name2", "n12");
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(rcoll, &a1, &ref_oids[0]));
    bson_destroy(&a1);

    bson_init(&a1);
    bson_append_string(&a1, "name", "n2");
    bson_append_string(&a1, "name2", "n22");
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(rcoll, &a1, &ref_oids[1]));
    bson_destroy(&a1);

    bson_init(&a1);
    bson_append_string(&a1, "name", "c1");
    bson_oid_to_string(&ref_oids[0], xoid);
    bson_append_string(&a1, "refs", xoid);
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(coll, &a1, &oid));
    bson_destroy(&a1);

    bson_init(&a1);
    bson_append_string(&a1, "name", "c2");
    bson_append_start_array(&a1, "arrefs");
    bson_oid_to_string(&ref_oids[0], xoid);
    bson_append_string(&a1, "0", xoid);
    bson_append_oid(&a1, "1", &ref_oids[1]);
    bson_append_finish_array(&a1);
    bson_finish(&a1);
    CU_ASSERT_FALSE_FATAL(a1.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(coll, &a1, &oid));
    bson_destroy(&a1);

    /*
     Assuming fpath contains object id (or its string representation).
     In query results fpath values will be replaced by loaded bson
     objects with matching oids from collectionname

     {..., $do : {fpath : {$join : 'collectionname'}} }
     $fields applied to the joined object:
     {..., $do : {fpath : {$join : 'collectionname'}} }, {$fields : {fpath.jonnedfpath : 1}}
     */

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "c1");
    bson_append_start_object(&bsq1, "$do");
    bson_append_start_object(&bsq1, "refs");
    bson_append_string(&bsq1, "$join", "ticket43_refs");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FIELD: refs HAS $do OPERATION"));
    CU_ASSERT_EQUAL(count, 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("n1", TCLISTVALPTR(q1res, i), "refs.name"));
    }

    tclistdel(q1res);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);
    tcxstrdel(log);

    /////////////////////////////////////////////////////////////////////////////////////

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "c2");
    bson_append_start_object(&bsq1, "$do");
    bson_append_start_object(&bsq1, "arrefs");
    bson_append_string(&bsq1, "$join", "ticket43_refs");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);

    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FIELD: arrefs HAS $do OPERATION"));
    CU_ASSERT_EQUAL(count, 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("c2", TCLISTVALPTR(q1res, i), "name"));
        CU_ASSERT_FALSE(bson_compare_string("n1", TCLISTVALPTR(q1res, i), "arrefs.0.name"));
        CU_ASSERT_FALSE(bson_compare_string("n2", TCLISTVALPTR(q1res, i), "arrefs.1.name"));
    }

    tclistdel(q1res);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);
    tcxstrdel(log);

    /////////////////////////////////////////////////////////////////////////////////////////////////

    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "name", "c2");
    bson_append_start_object(&bsq1, "$do");
    bson_append_start_object(&bsq1, "arrefs");
    bson_append_string(&bsq1, "$join", "ticket43_refs");
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);


    bson bshits1;
    bson_init_as_query(&bshits1);
    bson_append_start_object(&bshits1, "$fields");
    bson_append_int(&bshits1, "arrefs.0.name", 0);
    bson_append_finish_object(&bshits1);
    bson_finish(&bshits1);
    CU_ASSERT_FALSE_FATAL(bshits1.err);

    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshits1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);

    count = 0;
    log = tcxstrnew();
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "FIELD: arrefs HAS $do OPERATION"));
    CU_ASSERT_EQUAL(count, 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        bson_type bt;
        bson_iterator it;
        bson_iterator_from_buffer(&it, TCLISTVALPTR(q1res, i));
        CU_ASSERT_FALSE(bson_compare_string("c2", TCLISTVALPTR(q1res, i), "name"));
        bt = bson_find_fieldpath_value("arrefs.0.name", &it);
        CU_ASSERT_TRUE(bt == BSON_EOO);
        CU_ASSERT_FALSE(bson_compare_string("n2", TCLISTVALPTR(q1res, i), "arrefs.1.name"));
    }

    tclistdel(q1res);
    ejdbquerydel(q1);
    bson_destroy(&bsq1);
    bson_destroy(&bshits1);
    tcxstrdel(log);
}

void testTicket54(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket54", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
    bson b;
    bson_oid_t oid;
    bson_init(&b);
    bson_append_long(&b, "value", -10000000L);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);
    CU_ASSERT_TRUE(ejdbsetindex(coll, "value", JBIDXNUM));
}

void testMetaInfo(void) {
    bson *meta = ejdbmeta(jb);
    CU_ASSERT_PTR_NOT_NULL_FATAL(meta);
    const char *metabsdata = bson_data(meta);
    CU_ASSERT_FALSE(bson_compare_string("dbt2", metabsdata, "file"));
    CU_ASSERT_FALSE(bson_compare_string("contacts", metabsdata, "collections.1.name"));
    CU_ASSERT_FALSE(bson_compare_string("dbt2_contacts", metabsdata, "collections.1.file"));
    CU_ASSERT_FALSE(bson_compare_long(131071, metabsdata, "collections.1.options.buckets"));
    CU_ASSERT_FALSE(bson_compare_long(8, metabsdata, "collections.1.records"));
    bson_del(meta);
}

void testTicket81(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket81", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson b;
    bson_oid_t oid;

    bson_init(&b); //true
    bson_append_int(&b, "z", 33);
    bson_append_int(&b, "a", 1);
    bson_append_int(&b, "b", 3);
    bson_append_int(&b, "c", 10);
    bson_append_int(&b, "d", 7);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson_init(&b); //false
    bson_append_int(&b, "z", 33);
    bson_append_int(&b, "a", 11);
    bson_append_int(&b, "b", 22);
    bson_append_int(&b, "c", 5);
    bson_append_int(&b, "d", 7);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson_init(&b); //true
    bson_append_int(&b, "z", 33);
    bson_append_int(&b, "b", 2);
    bson_append_int(&b, "d", 7);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson_init(&b); //false
    bson_append_int(&b, "z", 22);
    bson_append_int(&b, "a", 1);
    bson_append_int(&b, "b", 3);
    bson_append_int(&b, "c", 10);
    bson_append_int(&b, "d", 7);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    //z=33 AND (a=1 OR b=2) AND (c=5 OR d=7)
    //{z : 33, $and : [ {$or : [{a : 1}, {b : 2}]}, {$or : [{c : 5}, {d : 7}]} ] }
    bson_init_as_query(&b);
    bson_append_int(&b, "z", 33);
    bson_append_start_array(&b, "$and");

    //{$or : [{a : 1}, {b : 2}]}
    bson_append_start_object(&b, "0");
    bson_append_start_array(&b, "$or");
    //{a : 1}
    bson_append_start_object(&b, "0");
    bson_append_int(&b, "a", 1);
    bson_append_finish_object(&b);
    //{b : 2}
    bson_append_start_object(&b, "1");
    bson_append_int(&b, "b", 2);
    bson_append_finish_object(&b);
    bson_append_finish_array(&b);
    bson_append_finish_object(&b); //eof {$or : [{a : 1}, {b : 2}]}

    //{$or : [{c : 5}, {d : 7}]}
    bson_append_start_object(&b, "1");
    bson_append_start_array(&b, "$or");
    //{c : 5}
    bson_append_start_object(&b, "0");
    bson_append_int(&b, "c", 5);
    bson_append_finish_object(&b);
    //{d : 7}
    bson_append_start_object(&b, "1");
    bson_append_int(&b, "d", 7);
    bson_append_finish_object(&b);
    bson_append_finish_array(&b);
    bson_append_finish_object(&b); //eof {$or : [{c : 5}, {d : 7}]}
    bson_append_finish_array(&b); //eof $and
    bson_finish(&b);

    TCXSTR *log = tcxstrnew();
    uint32_t count = 0;
    EJQ *q1 = ejdbcreatequery(jb, &b, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    bson_destroy(&b);

    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    //fprintf(stderr, "%s", TCXSTRPTR(log));
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 2);
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "ACTIVE CONDITIONS: 1"));
    CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "$AND QUERIES: 2"));

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        //z=33 AND (a=1 OR b=2) AND (c=5 OR d=7)
        CU_ASSERT_FALSE(bson_compare_long(33, TCLISTVALPTR(q1res, i), "z"));
        CU_ASSERT_TRUE(!bson_compare_long(1, TCLISTVALPTR(q1res, i), "a") || !bson_compare_long(2, TCLISTVALPTR(q1res, i), "b"));
        CU_ASSERT_TRUE(!bson_compare_long(5, TCLISTVALPTR(q1res, i), "c") || !bson_compare_long(7, TCLISTVALPTR(q1res, i), "d"));
    }
    tclistdel(q1res);
    ejdbquerydel(q1);
    bson_destroy(&b);
    tcxstrdel(log);
}

// $(projection)
// https://github.com/Softmotions/ejdb/issues/15
// http://docs.mongodb.org/manual/reference/projection/positional/#proj._S_

void testDQprojection(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "f_projection", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson b;
    bson_oid_t oid;

    bson_init(&b);
    bson_append_int(&b, "z", 33);
    bson_append_start_array(&b, "arr");
    bson_append_int(&b, "0", 0);
    bson_append_int(&b, "1", 1);
    bson_append_int(&b, "2", 2);
    bson_append_int(&b, "3", 3);
    bson_append_finish_array(&b);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson_init(&b);
    bson_append_int(&b, "z", 33);
    bson_append_start_array(&b, "arr");
    bson_append_int(&b, "0", 3);
    bson_append_int(&b, "1", 2);
    bson_append_int(&b, "2", 1);
    bson_append_int(&b, "3", 0);
    bson_append_finish_array(&b);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson_init(&b);
    bson_append_int(&b, "z", 44);
    bson_append_start_array(&b, "arr");
    bson_append_start_object(&b, "0");
    bson_append_int(&b, "h", 1);
    bson_append_finish_object(&b);
    bson_append_start_object(&b, "1");
    bson_append_int(&b, "h", 2);
    bson_append_finish_object(&b);
    bson_append_finish_array(&b);
    bson_finish(&b);
    //bson_print_raw(bson_data(&b), 0);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

//////// Q1
    bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$fields");
    bson_append_int(&bshints, "arr.$", 1);
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "z", 33);
    bson_append_start_object(&bsq1, "arr");
    bson_append_int(&bsq1, "$gte", 2);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    TCXSTR *log = tcxstrnew();
    uint32_t count;
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 2);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_TRUE(!bson_compare_long(2, TCLISTVALPTR(q1res, i), "arr.0") || !bson_compare_long(3, TCLISTVALPTR(q1res, i), "arr.0"));
    }

    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
    bson_destroy(&bshints);
    bson_destroy(&bsq1);

    /////// Q2
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$fields");
    bson_append_int(&bshints, "arr.$.h", 1);
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "z", 44);
    bson_append_int(&bsq1, "arr.h", 2);
    bson_finish(&bsq1);

    log = tcxstrnew();
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_long(2, TCLISTVALPTR(q1res, i), "arr.0.h"));
    }
    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
    bson_destroy(&bshints);
    bson_destroy(&bsq1);


    /////// Q3
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$fields");
    bson_append_int(&bshints, "arr.$.h", 1);
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);

    //{z: 44, arr: {$elemMatch: {h: 2}} }
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "z", 44);
    bson_append_start_object(&bsq1, "arr");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_int(&bsq1, "h", 2);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    log = tcxstrnew();
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_long(2, TCLISTVALPTR(q1res, i), "arr.0.h"));
    }
    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
    bson_destroy(&bshints);
    bson_destroy(&bsq1);
}

void testTicket96(void) {
    EJCOLL *coll = ejdbgetcoll(jb, "f_projection");
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    //{ $and : [ {arr : {$elemMatch : {h : 2}}} ]
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_start_array(&bsq1, "$and");
    bson_append_start_object(&bsq1, "0");
    bson_append_start_object(&bsq1, "arr");
    bson_append_start_object(&bsq1, "$elemMatch");
    bson_append_int(&bsq1, "h", 2);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_append_finish_object(&bsq1);
    bson_append_finish_array(&bsq1);
    bson_finish(&bsq1);

    CU_ASSERT_EQUAL(bsq1.err, 0);

    TCXSTR *log = tcxstrnew();
    uint32_t count;
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    bson_destroy(&bsq1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);

    tclistdel(q1res);
    ejdbquerydel(q1);
    tcxstrdel(log);
}


// $(query)
// https://github.com/Softmotions/ejdb/issues/15
// http://docs.mongodb.org/manual/reference/projection/positional/#proj._S_

void testDQupdate(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "f_update", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson b;
    bson_oid_t oid;

    bson_init(&b);
    bson_append_int(&b, "z", 33);
    bson_append_start_array(&b, "arr");
    bson_append_int(&b, "0", 0);
    bson_append_int(&b, "1", 1);
    bson_append_int(&b, "2", 2);
    bson_append_int(&b, "3", 3);
    bson_append_finish_array(&b);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "z", 33);
    bson_append_int(&bsq1, "arr", 1);
    bson_append_start_object(&bsq1, "$set");
    bson_append_int(&bsq1, "arr.$", 4);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    TCXSTR *log = tcxstrnew();
    uint32_t count;
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_EQUAL(count, 1);

    ejdbquerydel(q1);
    tcxstrdel(log);
    bson_destroy(&bsq1);

    //Now check It
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "z", 33);
    bson_append_int(&bsq1, "arr", 4);
    bson_finish(&bsq1);

    log = tcxstrnew();
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_EQUAL(count, 1);

    ejdbquerydel(q1);
    tcxstrdel(log);
    bson_destroy(&bsq1);

}

void testDQupdate2(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "f_update", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson b;
    bson_oid_t oid;

    bson_init(&b);
    bson_append_int(&b, "z", 44);
    bson_append_start_array(&b, "arr");
    bson_append_start_object(&b, "0");
    bson_append_int(&b, "h", 1);
    bson_append_finish_object(&b);
    bson_append_start_object(&b, "1");
    bson_append_int(&b, "h", 2);
    bson_append_finish_object(&b);
    bson_append_finish_array(&b);
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "z", 44);
    bson_append_int(&bsq1, "arr.h", 2);
    bson_append_start_object(&bsq1, "$set");
    bson_append_int(&bsq1, "arr.$.h", 4);
    bson_append_int(&bsq1, "arr.$.z", 5);
    bson_append_int(&bsq1, "k", 55);
    bson_append_finish_object(&bsq1);
    bson_finish(&bsq1);

    TCXSTR *log = tcxstrnew();
    uint32_t count;
    EJQ *q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_EQUAL(count, 1);

    ejdbquerydel(q1);
    tcxstrdel(log);
    bson_destroy(&bsq1);

    //Now check It
    bson_init_as_query(&bsq1);
    bson_append_int(&bsq1, "k", 55);
    bson_append_int(&bsq1, "arr.h", 4);
    bson_append_int(&bsq1, "arr.z", 5);
    bson_finish(&bsq1);

    log = tcxstrnew();
    q1 = ejdbcreatequery(jb, &bsq1, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    ejdbqryexecute(coll, q1, &count, JBQRYCOUNT, log);
    CU_ASSERT_EQUAL(count, 1);

    ejdbquerydel(q1);
    tcxstrdel(log);
    bson_destroy(&bsq1);
}

void testTicket99(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket99", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    //create
    bson_oid_t oid;
    bson data;
    bson_init(&data);
    bson_append_start_array(&data, "arr");
    bson_append_start_object(&data, "0");
    bson_append_string(&data, "test0", "value");
    bson_append_finish_object(&data);
    bson_append_finish_array(&data);
    bson_append_finish_array(&data);
    bson_finish(&data);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &data, &oid));
    bson_destroy(&data);

    //set
    bson bsquery;
    bson_init_as_query(&bsquery);
    bson_append_oid(&bsquery, "_id", &oid);
    bson_append_start_object(&bsquery, "$set");
    bson_append_string(&bsquery, "arr.0.test0", "value0");
    bson_append_string(&bsquery, "arr.0.test1", "value1");
    bson_append_string(&bsquery, "arr.0.test2", "value2");
    bson_append_string(&bsquery, "arr.0.test3", "value3");
    bson_append_string(&bsquery, "arr.0.test4", "value4");
    bson_append_finish_object(&bsquery);
    bson_finish(&bsquery);

    uint32_t count = ejdbupdate(coll, &bsquery, 0, 0, 0, 0);
    CU_ASSERT_EQUAL(count, 1);
    bson_destroy(&bsquery);

    bson_init_as_query(&bsquery);
    bson_finish(&bsquery);
    EJQ *q1 = ejdbcreatequery(jb, &bsquery, NULL, 0, NULL);
    bson_destroy(&bsquery);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, NULL);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);

    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_string("value0", TCLISTVALPTR(q1res, i), "arr.0.test0"));
        CU_ASSERT_FALSE(bson_compare_string("value1", TCLISTVALPTR(q1res, i), "arr.0.test1"));
        CU_ASSERT_FALSE(bson_compare_string("value2", TCLISTVALPTR(q1res, i), "arr.0.test2"));
        CU_ASSERT_FALSE(bson_compare_string("value3", TCLISTVALPTR(q1res, i), "arr.0.test3"));
        CU_ASSERT_FALSE(bson_compare_string("value4", TCLISTVALPTR(q1res, i), "arr.0.test4"));
    }

    tclistdel(q1res);
    ejdbquerydel(q1);
}


void testTicket101(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket101", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson b;
    bson_oid_t oid;

    bson_init(&b);
    bson_append_int(&b, "z", 33);
    bson_append_start_object(&b, "obj");
    bson_append_string(&b, "name", "abc");
    bson_append_int(&b, "score", 12);
    bson_append_finish_object(&b); //eof obj
    bson_append_start_array(&b, "arr");
    bson_append_int(&b, "0", 0);
    bson_append_start_object(&b, "1");
    bson_append_string(&b, "name", "cde");
    bson_append_int(&b, "score", 13);
    bson_append_finish_object(&b);
    bson_append_int(&b, "2", 2);
    bson_append_int(&b, "3", 3);
    bson_append_finish_array(&b); //eof arr
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

    bson bsq;
    bson_init_as_query(&bsq);
    bson_append_start_object(&bsq, "$unset");
    bson_append_string(&bsq, "obj.name", "");
    bson_append_bool(&bsq, "arr.1.score", true);
    bson_append_bool(&bsq, "arr.2", true);
    bson_append_finish_object(&bsq);
    bson_finish(&bsq);

    uint32_t count = ejdbupdate(coll, &bsq, 0, 0, 0, 0);
    bson_destroy(&bsq);
    CU_ASSERT_EQUAL(count, 1);


    bson_init_as_query(&bsq);
    bson_finish(&bsq);
    EJQ *q1 = ejdbcreatequery(jb, &bsq, NULL, 0, NULL);
    bson_destroy(&bsq);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, NULL);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);

    bson_type bt;
    bson_iterator it;
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
        CU_ASSERT_FALSE(bson_compare_long(33,  TCLISTVALPTR(q1res, i), "z"));
        CU_ASSERT_FALSE(bson_compare_long(12,  TCLISTVALPTR(q1res, i), "obj.score"));
        bson_iterator_from_buffer(&it, TCLISTVALPTR(q1res, i));
        bt = bson_find_fieldpath_value("obj.name", &it);
        CU_ASSERT_TRUE(bt == BSON_EOO);
        bson_iterator_from_buffer(&it, TCLISTVALPTR(q1res, i));
        bt = bson_find_fieldpath_value("arr.2", &it);
        CU_ASSERT_TRUE(bt == BSON_UNDEFINED);
    }

    tclistdel(q1res);
    ejdbquerydel(q1);
}

void testTicket110(void) {
	EJCOLL *coll = ejdbcreatecoll(jb, "ticket110", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
	
	bson b;
    bson_oid_t oid;

    bson_init(&b);
	bson_append_string(&b, "comment", "One");
	bson_append_string(&b, "status", "New");
	bson_append_int(&b, "xversion", 0);
	bson_append_int(&b, "value",  9855);
	bson_append_string(&b, "title", "Lead 0");
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);
	
	bson_init(&b);
	bson_append_string(&b, "comment", "Two");
	bson_append_string(&b, "status", "New");
	bson_append_int(&b, "xversion", 0);
	bson_append_string(&b, "value",  "18973");
	bson_append_string(&b, "title", "Lead 1");
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

	bson_init(&b);
	bson_append_string(&b, "comment", "Three");
	bson_append_string(&b, "status", "New");
	bson_append_int(&b, "xversion", 0);
	bson_append_string(&b, "value",  "25504");
	bson_append_string(&b, "title", "Lead 2");
    bson_finish(&b);
    CU_ASSERT_TRUE(ejdbsavebson(coll, &b, &oid));
    bson_destroy(&b);

	
	//   
	bson bsq;
    bson_init_as_query(&bsq);
	bson_finish(&bsq);
	CU_ASSERT_FALSE_FATAL(bsq.err);

	//
	bson bshints;
    bson_init_as_query(&bshints);
    bson_append_start_object(&bshints, "$orderby");
    bson_append_int(&bshints, "value", -1);
    bson_append_finish_object(&bshints);
    bson_finish(&bshints);
    CU_ASSERT_FALSE_FATAL(bshints.err);
	
	EJQ *q1 = ejdbcreatequery(jb, &bsq, NULL, 0, &bshints);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
	
	uint32_t count = 0;
    TCXSTR *log = tcxstrnew();
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
	
	bson_destroy(&bsq);
	bson_destroy(&bshints);
	
	tclistdel(q1res);
	tcxstrdel(log);
	ejdbquerydel(q1);
}

// Ticket #14
void testSlice(void) {
	EJCOLL *coll = ejdbgetcoll(jb, "f_projection");
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
	
	bson_iterator it;
	
// { z : 44, $do : { arr : {$slice : 1} } }
	bson bsq;
    bson_init_as_query(&bsq);
    bson_append_int(&bsq, "z", 44);
    bson_append_start_object(&bsq, "$do");
    bson_append_start_object(&bsq, "arr");
    bson_append_int(&bsq, "$slice", 1);
    bson_append_finish_object(&bsq);
    bson_append_finish_object(&bsq);
    bson_finish(&bsq);
    CU_ASSERT_FALSE_FATAL(bsq.err);
	
	EJQ *q1 = ejdbcreatequery(jb, &bsq, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
	
	uint32_t count = 0;
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, NULL);
	//fprintf(stderr, "\n%s", TCXSTRPTR(log));
	CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
		void *bsdata = TCLISTVALPTR(q1res, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);
        CU_ASSERT_FALSE(bson_compare_long(1, bsdata, "arr.0.h"));
		bson_iterator_from_buffer(&it, bsdata);
		CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.1.h", &it) == BSON_EOO);
    }
	tclistdel(q1res);
	bson_destroy(&bsq);
	ejdbquerydel(q1);
	
	// { z : 44, $do : { arr : {$slice : 2} } }
	bson_init_as_query(&bsq);
    bson_append_int(&bsq, "z", 44);
    bson_append_start_object(&bsq, "$do");
    bson_append_start_object(&bsq, "arr");
    bson_append_int(&bsq, "$slice", 2);
    bson_append_finish_object(&bsq);
    bson_append_finish_object(&bsq);
    bson_finish(&bsq);
    CU_ASSERT_FALSE_FATAL(bsq.err);
	
	q1 = ejdbcreatequery(jb, &bsq, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
	
	count = 0;
    q1res = ejdbqryexecute(coll, q1, &count, 0, NULL);
	CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
	CU_ASSERT_EQUAL(count, 1);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
		void *bsdata = TCLISTVALPTR(q1res, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);
        CU_ASSERT_FALSE(bson_compare_long(1, bsdata, "arr.0.h"));
        CU_ASSERT_FALSE(bson_compare_long(2, bsdata, "arr.1.h"));
		bson_iterator_from_buffer(&it, bsdata);
		CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.2.h", &it) == BSON_EOO);
    }
	tclistdel(q1res);
	bson_destroy(&bsq);
	ejdbquerydel(q1);
	
	// { $do : { arr : {$slice : [1, 2]} } }
	bson_init_as_query(&bsq);
    bson_append_start_object(&bsq, "$do");
    bson_append_start_object(&bsq, "arr");
    bson_append_start_array(&bsq, "$slice");
	bson_append_int(&bsq, "0", 1);
	bson_append_int(&bsq, "1", 2);
	bson_append_finish_array(&bsq);
    bson_append_finish_object(&bsq);
    bson_append_finish_object(&bsq);
    bson_finish(&bsq);
    CU_ASSERT_FALSE_FATAL(bsq.err);
	
	q1 = ejdbcreatequery(jb, &bsq, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
	
	count = 0;
    q1res = ejdbqryexecute(coll, q1, &count, 0, NULL);
	CU_ASSERT_EQUAL(TCLISTNUM(q1res), 3);
	CU_ASSERT_EQUAL(count, 3);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
		void *bsdata = TCLISTVALPTR(q1res, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);
		if (i == 0) {
			CU_ASSERT_FALSE(bson_compare_long(1, bsdata, "arr.0"));
			CU_ASSERT_FALSE(bson_compare_long(2, bsdata, "arr.1"));
			bson_iterator_from_buffer(&it, bsdata);
			CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.2", &it) == BSON_EOO);
		} else if (i == 1) {
			CU_ASSERT_FALSE(bson_compare_long(2, bsdata, "arr.0"));
			CU_ASSERT_FALSE(bson_compare_long(1, bsdata, "arr.1"));
			bson_iterator_from_buffer(&it, bsdata);
			CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.2", &it) == BSON_EOO);
		} else if (i == 2) {
			CU_ASSERT_FALSE(bson_compare_long(2, bsdata, "arr.0.h"));
			bson_iterator_from_buffer(&it, bsdata);
			CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.1", &it) == BSON_EOO);
		}
    }
	tclistdel(q1res);
	bson_destroy(&bsq);
	ejdbquerydel(q1);
	
	
	
//{ _id: '54d7a2f07e671e140000001f',
//  z: 33,
//  arr: [ 0, 1, 2, 3 ] }
//{ _id: '54d7a2f07e671e1400000020',
//  z: 33,
//  arr: [ 3, 2, 1, 0 ] }
//{ _id: '54d7a2f07e671e1400000021',
//  z: 44,
//  arr: [ { h: 1 }, { h: 2 } ] } 
	
	// { $do : { arr : {$slice : [-3, 1]} } }
	bson_init_as_query(&bsq);
    bson_append_start_object(&bsq, "$do");
    bson_append_start_object(&bsq, "arr");
    bson_append_start_array(&bsq, "$slice");
	bson_append_int(&bsq, "0", -3);
	bson_append_int(&bsq, "1", 1);
	bson_append_finish_array(&bsq);
    bson_append_finish_object(&bsq);
    bson_append_finish_object(&bsq);
    bson_finish(&bsq);
    CU_ASSERT_FALSE_FATAL(bsq.err);
	
	q1 = ejdbcreatequery(jb, &bsq, NULL, 0, NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
	
	count = 0;
    q1res = ejdbqryexecute(coll, q1, &count, 0, NULL);
	CU_ASSERT_EQUAL(TCLISTNUM(q1res), 3);
	CU_ASSERT_EQUAL(count, 3);
    for (int i = 0; i < TCLISTNUM(q1res); ++i) {
		void *bsdata = TCLISTVALPTR(q1res, i);
        CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);
		if (i == 0) {
			CU_ASSERT_FALSE(bson_compare_long(1, bsdata, "arr.0"));
			bson_iterator_from_buffer(&it, bsdata);
			CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.1", &it) == BSON_EOO);
		} else if (i == 1) {
			CU_ASSERT_FALSE(bson_compare_long(2, bsdata, "arr.0"));
			bson_iterator_from_buffer(&it, bsdata);
			CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.1", &it) == BSON_EOO);
		} else if (i == 2) {
			CU_ASSERT_FALSE(bson_compare_long(2, bsdata, "arr.0.h"));
			bson_iterator_from_buffer(&it, bsdata);
			CU_ASSERT_TRUE(bson_find_fieldpath_value("arr.1", &it) == BSON_EOO);
		}
    }
	tclistdel(q1res);
	bson_destroy(&bsq);
	ejdbquerydel(q1);
}


void testDistinct(void) {
    EJCOLL *contacts = ejdbcreatecoll(jb, "contacts", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(contacts);

    uint32_t count;
    TCXSTR *log;
    bson *q1res; 

    log = tcxstrnew();
    q1res = ejdbqrydistinct(contacts, "address", NULL, NULL, 0, &count, log);

    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 4);
    
    bson_del(q1res);
    
    bson bsq1;
    bson_init_as_query(&bsq1);
    bson_append_string(&bsq1, "address.street", "Pirogova");
    bson_finish(&bsq1);
    CU_ASSERT_FALSE_FATAL(bsq1.err);

    q1res = ejdbqrydistinct(contacts, "address.room", &bsq1, NULL, 0, &count, log);
    
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 1);
    
    bson_del(q1res);

    q1res = ejdbqrydistinct(contacts, "nonexisted", NULL, NULL, 0, &count, log);
    
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1res);
    CU_ASSERT_EQUAL(count, 0);
    
    bson_del(q1res);

    bson_destroy(&bsq1);
    tcxstrdel(log);
}


void testTicket117(void) {
	EJCOLL *coll = ejdbcreatecoll(jb, "ticket117", NULL);
	CU_ASSERT_PTR_NOT_NULL_FATAL(coll);
	
	bson_oid_t oid;
	bson brec;
	
	bson_init(&brec);
    bson_append_string(&brec, "color", "Red");
    bson_finish(&brec);
    CU_ASSERT_FALSE_FATAL(brec.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(coll, &brec, &oid));
    bson_destroy(&brec);
	
	bson_init(&brec);
    bson_append_string(&brec, "color", "Green");
    bson_finish(&brec);
    CU_ASSERT_FALSE_FATAL(brec.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(coll, &brec, &oid));
    bson_destroy(&brec);
	
	CU_ASSERT_TRUE_FATAL(ejdbsetindex(coll, "color", JBIDXSTR));
	
	bson_init(&brec);
    bson_append_string(&brec, "color", "Blue");
    bson_finish(&brec);
    CU_ASSERT_FALSE_FATAL(brec.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(coll, &brec, &oid));
    bson_destroy(&brec);
	
	bson bsq;
    bson_init_as_query(&bsq);
	bson_append_string(&bsq, "color", "Blue");
	bson_finish(&bsq);
	CU_ASSERT_FALSE_FATAL(bsq.err);
	
	TCXSTR *log = tcxstrnew();
	uint32_t count = 0;
	EJQ *q1 = ejdbcreatequery(jb, &bsq, NULL, 0, NULL);
    bson_destroy(&bsq);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, log);
	CU_ASSERT_PTR_NOT_NULL(q1res);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);
	
	for (int i = 0; i < TCLISTNUM(q1res); ++i) {
		void *bsdata = TCLISTVALPTR(q1res, i);
        bson_print_raw(bsdata, 0);
    }
    //fprintf(stderr, "%s", TCXSTRPTR(log));
	CU_ASSERT_PTR_NOT_NULL(strstr(TCXSTRPTR(log), "MAIN IDX: 'scolor'"));
	
	ejdbquerydel(q1);
	tclistdel(q1res);
	tcxstrdel(log);
}

void testTicket148(void) {
    EJCOLL *coll = ejdbcreatecoll(jb, "ticket148", NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(coll);

    bson bs;
    bson_oid_t oid;

    bson_init(&bs);
    bson_finish(&bs);
    CU_ASSERT_FALSE_FATAL(bs.err);
    CU_ASSERT_TRUE_FATAL(ejdbsavebson(coll, &bs, &oid));
    bson_destroy(&bs);

    bson bsq;
    bson_init_as_query(&bsq);
    bson_append_start_object(&bsq, "$set");
    bson_append_int(&bsq, "info.name.par.age", 40);
    bson_append_int(&bsq, "info.name.mot.age", 35);
    bson_append_finish_object(&bsq);
    bson_finish(&bsq);
    CU_ASSERT_FALSE_FATAL(bsq.err);

    uint32_t count = ejdbupdate(coll, &bsq, 0, 0, 0, 0);
    bson_destroy(&bsq);
    CU_ASSERT_EQUAL(count, 1);

    bson_init_as_query(&bsq);
    bson_finish(&bsq);
    EJQ *q1 = ejdbcreatequery(jb, &bsq, NULL, 0, NULL);
    bson_destroy(&bsq);
    CU_ASSERT_PTR_NOT_NULL_FATAL(q1);
    TCLIST *q1res = ejdbqryexecute(coll, q1, &count, 0, NULL);
    CU_ASSERT_EQUAL(TCLISTNUM(q1res), 1);

    void *bsdata = TCLISTVALPTR(q1res, 0);
    CU_ASSERT_PTR_NOT_NULL_FATAL(bsdata);

    bson_iterator it, sit;
    bson_type bt;
    BSON_ITERATOR_FROM_BUFFER(&it, bsdata);
    while((bt = bson_iterator_next(&it)) != BSON_EOO) {
        if (bt == BSON_OID) {
            continue;
        }
        break;
    }
    CU_ASSERT_EQUAL(bt, BSON_OBJECT);
    CU_ASSERT_FALSE(strcmp(BSON_ITERATOR_KEY(&it), "info"));

    BSON_ITERATOR_SUBITERATOR(&it, &sit);
    bt = bson_iterator_next(&sit);
    CU_ASSERT_EQUAL(bt, BSON_OBJECT);
    CU_ASSERT_FALSE(strcmp(BSON_ITERATOR_KEY(&sit), "name"));

    BSON_ITERATOR_SUBITERATOR(&sit, &sit);
    bt = bson_iterator_next(&sit);
    CU_ASSERT_EQUAL(bt, BSON_OBJECT);
    CU_ASSERT_FALSE(strcmp(BSON_ITERATOR_KEY(&sit) , "par") && strcmp(BSON_ITERATOR_KEY(&sit) , "mot"));
    bt = bson_iterator_next(&sit);
    CU_ASSERT_EQUAL(bt, BSON_OBJECT);
    CU_ASSERT_FALSE(strcmp(BSON_ITERATOR_KEY(&sit) , "par") && strcmp(BSON_ITERATOR_KEY(&sit) , "mot"));

    BSON_ITERATOR_FROM_BUFFER(&it, bsdata);
    bt = bson_find_fieldpath_value("info.name.par.age", &it);
    CU_ASSERT_TRUE(BSON_IS_NUM_TYPE(bt));
    CU_ASSERT_EQUAL(bson_iterator_int(&it), 40);

    BSON_ITERATOR_FROM_BUFFER(&it, bsdata);
    bt = bson_find_fieldpath_value("info.name.mot.age", &it);
    CU_ASSERT_TRUE(BSON_IS_NUM_TYPE(bt));
    CU_ASSERT_EQUAL(bson_iterator_int(&it), 35);

    ejdbquerydel(q1);
    tclistdel(q1res);
}


int main() {
    setlocale(LC_ALL, "en_US.UTF-8");
    CU_pSuite pSuite = NULL;

    /* Initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* Add a suite to the registry */
    pSuite = CU_add_suite("ejdbtest2", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if ((NULL == CU_add_test(pSuite, "testAddData", testAddData)) ||
            (NULL == CU_add_test(pSuite, "testInvalidQueries1", testInvalidQueries1)) ||
            (NULL == CU_add_test(pSuite, "testSetIndex1", testSetIndex1)) ||
            (NULL == CU_add_test(pSuite, "testQuery1", testQuery1)) ||
            (NULL == CU_add_test(pSuite, "testQuery2", testQuery2)) ||
            (NULL == CU_add_test(pSuite, "testQuery3", testQuery3)) ||
            (NULL == CU_add_test(pSuite, "testQuery4", testQuery4)) ||
            (NULL == CU_add_test(pSuite, "testQuery5", testQuery5)) ||
            (NULL == CU_add_test(pSuite, "testQuery6", testQuery6)) ||
            (NULL == CU_add_test(pSuite, "testQuery7", testQuery7)) ||
            (NULL == CU_add_test(pSuite, "testQuery8", testQuery8)) ||
            (NULL == CU_add_test(pSuite, "testQuery9", testQuery9)) ||
            (NULL == CU_add_test(pSuite, "testQuery10", testQuery10)) ||
            (NULL == CU_add_test(pSuite, "testQuery11", testQuery11)) ||
            (NULL == CU_add_test(pSuite, "testQuery12", testQuery12)) ||
            (NULL == CU_add_test(pSuite, "testQuery13", testQuery13)) ||
            (NULL == CU_add_test(pSuite, "testQuery14", testQuery14)) ||
            (NULL == CU_add_test(pSuite, "testQuery15", testQuery15)) ||
            (NULL == CU_add_test(pSuite, "testQuery16", testQuery16)) ||
            (NULL == CU_add_test(pSuite, "testQuery17", testQuery17)) ||
            (NULL == CU_add_test(pSuite, "testQuery18", testQuery18)) ||
            (NULL == CU_add_test(pSuite, "testQuery19", testQuery19)) ||
            (NULL == CU_add_test(pSuite, "testQuery20", testQuery20)) ||
            (NULL == CU_add_test(pSuite, "testQuery21", testQuery21)) ||
            (NULL == CU_add_test(pSuite, "testQuery22", testQuery22)) ||
            (NULL == CU_add_test(pSuite, "testQuery23", testQuery23)) ||
            (NULL == CU_add_test(pSuite, "testQuery24", testQuery24)) ||
            (NULL == CU_add_test(pSuite, "testQuery25", testQuery25)) ||
            (NULL == CU_add_test(pSuite, "testQuery25_2", testQuery25_2)) ||
            (NULL == CU_add_test(pSuite, "testQuery26", testQuery26)) ||
            (NULL == CU_add_test(pSuite, "testQuery27", testQuery27)) ||
			(NULL == CU_add_test(pSuite, "testQuery28", testQuery28)) ||
			(NULL == CU_add_test(pSuite, "testQuery29", testQuery29)) ||
			(NULL == CU_add_test(pSuite, "testQuery30", testQuery30)) ||
			(NULL == CU_add_test(pSuite, "testQuery31", testQuery31)) ||
            (NULL == CU_add_test(pSuite, "testOIDSMatching", testOIDSMatching)) ||
            (NULL == CU_add_test(pSuite, "testEmptyFieldIndex", testEmptyFieldIndex)) ||
            (NULL == CU_add_test(pSuite, "testICaseIndex", testICaseIndex)) ||
            (NULL == CU_add_test(pSuite, "testTicket7", testTicket7)) ||
            (NULL == CU_add_test(pSuite, "testTicket8", testTicket8)) ||
            (NULL == CU_add_test(pSuite, "testUpdate1", testUpdate1)) ||
            (NULL == CU_add_test(pSuite, "testUpdate2", testUpdate2)) ||
            (NULL == CU_add_test(pSuite, "testUpdate3", testUpdate3)) ||
            (NULL == CU_add_test(pSuite, "testQueryBool", testQueryBool)) ||
            (NULL == CU_add_test(pSuite, "testDropAll", testDropAll)) ||
            (NULL == CU_add_test(pSuite, "testTokensBegin", testTokensBegin)) ||
            (NULL == CU_add_test(pSuite, "testOneFieldManyConditions", testOneFieldManyConditions)) ||
            (NULL == CU_add_test(pSuite, "testAddToSet", testAddToSet)) ||
            (NULL == CU_add_test(pSuite, "testTicket123", testTicket123)) ||
            (NULL == CU_add_test(pSuite, "testPush", testPush)) ||
            (NULL == CU_add_test(pSuite, "testPushAll", testPushAll)) ||
            (NULL == CU_add_test(pSuite, "testRename2", testRename2)) ||
            (NULL == CU_add_test(pSuite, "testPull", testPull)) ||
            (NULL == CU_add_test(pSuite, "testFindInComplexArray", testFindInComplexArray)) ||
            (NULL == CU_add_test(pSuite, "testElemMatch", testElemMatch)) ||
            (NULL == CU_add_test(pSuite, "testNotElemMatch", testNotElemMatch)) ||
            (NULL == CU_add_test(pSuite, "testTicket16", testTicket16)) ||
            (NULL == CU_add_test(pSuite, "testUpsert", testUpsert)) ||
            (NULL == CU_add_test(pSuite, "testPrimitiveCases1", testPrimitiveCases1)) ||
            (NULL == CU_add_test(pSuite, "testTicket29", testTicket29)) ||
            (NULL == CU_add_test(pSuite, "testTicket28", testTicket28)) ||
            (NULL == CU_add_test(pSuite, "testTicket38", testTicket38)) ||
            (NULL == CU_add_test(pSuite, "testTicket43", testTicket43)) ||
            (NULL == CU_add_test(pSuite, "testTicket54", testTicket54)) ||
            (NULL == CU_add_test(pSuite, "testTicket88", testTicket88)) ||
            (NULL == CU_add_test(pSuite, "testTicket89", testTicket89)) ||
            (NULL == CU_add_test(pSuite, "testTicket81", testTicket81)) ||
            (NULL == CU_add_test(pSuite, "test$projection", testDQprojection)) ||
            (NULL == CU_add_test(pSuite, "test$update", testDQupdate)) ||
            (NULL == CU_add_test(pSuite, "test$update2", testDQupdate2)) ||
            (NULL == CU_add_test(pSuite, "testTicket96", testTicket96)) ||
            (NULL == CU_add_test(pSuite, "testTicket99", testTicket99)) ||
            (NULL == CU_add_test(pSuite, "testTicket101", testTicket101)) || 
            (NULL == CU_add_test(pSuite, "testTicket110", testTicket110)) ||
            (NULL == CU_add_test(pSuite, "testDistinct", testDistinct)) ||
			(NULL == CU_add_test(pSuite, "testSlice", testSlice)) ||
			(NULL == CU_add_test(pSuite, "testTicket117", testTicket117)) ||
            (NULL == CU_add_test(pSuite, "testMetaInfo", testMetaInfo)) ||
            (NULL == CU_add_test(pSuite, "testTicket148", testTicket148))
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
