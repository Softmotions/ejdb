#include "myconf.h"
#include "bson.h"
#include "CUnit/Basic.h"


/*
 * CUnit Test Suite
 */

int init_suite(void) {
    return 0;
}

int clean_suite(void) {
    return 0;
}

void testCheckDuplicates(void) {
    bson bs, bs2;
    bson_iterator it;
    bson_type bt;
    
    bson_init(&bs);
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "b", 2);
    bson_append_null(&bs, "c");
    bson_append_start_object(&bs, "d");
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "e", 0);
    bson_append_int(&bs, "d", 1);
    bson_append_finish_object(&bs);
    bson_finish(&bs);
    CU_ASSERT_FALSE_FATAL(bs.err);

    CU_ASSERT_FALSE(bson_check_duplicate_keys(&bs));

    bson_destroy(&bs);

    bson_init(&bs);
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "b", 2);
    bson_append_null(&bs, "c");
    bson_append_start_object(&bs, "d");
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "e", 0);
    bson_append_int(&bs, "e", 1);
    bson_append_finish_object(&bs);
    bson_finish(&bs);
    CU_ASSERT_FALSE_FATAL(bs.err);

    CU_ASSERT_TRUE(bson_check_duplicate_keys(&bs));

    bson_init(&bs2);
    bson_fix_duplicate_keys(&bs, &bs2);
    bson_finish(&bs2);

    CU_ASSERT_FALSE(bson_check_duplicate_keys(&bs2));
    BSON_ITERATOR_INIT(&it, &bs2);
    bt = bson_find_fieldpath_value("d.e", &it);
    CU_ASSERT_TRUE(BSON_IS_NUM_TYPE(bt));
    CU_ASSERT_EQUAL(bson_iterator_int(&it), 1);

    bson_destroy(&bs2);

    bson_init(&bs);
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "b", 2);
    bson_append_null(&bs, "c");
    bson_append_start_object(&bs, "d");
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "e", 0);
    bson_append_int(&bs, "d", 1);
    bson_append_finish_object(&bs);
    bson_append_start_array(&bs, "f");
    bson_append_start_object(&bs, "0");
    bson_append_string(&bs, "a", "a");
    bson_append_string(&bs, "b", "b");
    bson_append_int(&bs, "c", 1);
    bson_append_finish_object(&bs);
    bson_append_start_object(&bs, "1");
    bson_append_string(&bs, "a", "a");
    bson_append_string(&bs, "b", "b");
    bson_append_int(&bs, "c", 1);
    bson_append_finish_object(&bs);
    bson_append_finish_array(&bs);
    bson_finish(&bs);
    CU_ASSERT_FALSE_FATAL(bs.err);

    CU_ASSERT_FALSE(bson_check_duplicate_keys(&bs));

    bson_init(&bs);
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "b", 2);
    bson_append_null(&bs, "c");
    bson_append_start_object(&bs, "d");
    bson_append_string(&bs, "a", "a");
    bson_append_int(&bs, "e", 0);
    bson_append_int(&bs, "d", 1);
    bson_append_start_object(&bs, "q");
    bson_append_int(&bs, "w", 0);
    bson_append_finish_object(&bs);
    bson_append_finish_object(&bs);
    bson_append_start_array(&bs, "f");
    bson_append_start_object(&bs, "0");
    bson_append_string(&bs, "a", "a");
    bson_append_string(&bs, "b", "b");
    bson_append_int(&bs, "a", 1);
    bson_append_finish_object(&bs);
    bson_append_start_object(&bs, "1");
    bson_append_string(&bs, "a", "a");
    bson_append_string(&bs, "b", "b");
    bson_append_int(&bs, "c", 1);
    bson_append_finish_object(&bs);
    bson_append_finish_array(&bs);
    bson_append_start_object(&bs, "a");
    bson_append_finish_object(&bs);
    bson_append_start_object(&bs, "d");
    bson_append_start_object(&bs, "q");
    bson_append_int(&bs, "e", 1);
    bson_append_finish_object(&bs);
    bson_append_finish_object(&bs);
    bson_finish(&bs);
    CU_ASSERT_FALSE_FATAL(bs.err);

    CU_ASSERT_TRUE(bson_check_duplicate_keys(&bs));

    bson_init(&bs2);
    bson_fix_duplicate_keys(&bs, &bs2);
    bson_finish(&bs2);

    CU_ASSERT_FALSE(bson_check_duplicate_keys(&bs2));
    BSON_ITERATOR_INIT(&it, &bs2);
    bt = bson_find_fieldpath_value("f.0.a", &it);
    CU_ASSERT_TRUE(BSON_IS_NUM_TYPE(bt));
    CU_ASSERT_EQUAL(bson_iterator_int(&it), 1);
    BSON_ITERATOR_INIT(&it, &bs2);
    bt = bson_find_fieldpath_value("f.1.a", &it);
    CU_ASSERT_TRUE(BSON_IS_STRING_TYPE(bt));
    CU_ASSERT_FALSE(strcmp(bson_iterator_string(&it), "a"));

    BSON_ITERATOR_INIT(&it, &bs2);
    bt = bson_find_fieldpath_value("a", &it);
    CU_ASSERT_EQUAL(bt, BSON_OBJECT);

    BSON_ITERATOR_INIT(&it, &bs2);
    bt = bson_find_fieldpath_value("d.q.w", &it);
    CU_ASSERT_TRUE(BSON_IS_NUM_TYPE(bt));
    CU_ASSERT_EQUAL(bson_iterator_int(&it), 0);
    BSON_ITERATOR_INIT(&it, &bs2);
    bt = bson_find_fieldpath_value("d.q.e", &it);
    CU_ASSERT_TRUE(BSON_IS_NUM_TYPE(bt));
    CU_ASSERT_EQUAL(bson_iterator_int(&it), 1);
}

int main() {
    setlocale(LC_ALL, "en_US.UTF-8");
    CU_pSuite pSuite = NULL;

    /* Initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    /* Add a suite to the registry */
    pSuite = CU_add_suite("bsontest", init_suite, clean_suite);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    /* Add the tests to the suite */
    if ((NULL == CU_add_test(pSuite, "testCheckDuplicates", testCheckDuplicates))
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
