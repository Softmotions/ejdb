#include "ejdb2.h"
#include "jbl.h"
#include "jbl_internal.h"
#include <CUnit/Basic.h>

#define INT64_FORMAT     PRId64
#define UINT64_FORMAT    PRIu64
#define INT64_HEX_FORMAT PRIx64

typedef unsigned short int     u16;
typedef unsigned int           u32;
typedef unsigned long long int u64;

int init_suite(void) {
  int rc = ejdb_init();
  return rc;
}

int clean_suite(void) {
  return 0;
}

static int CalcAllocation(int needed_size, int alloc_size) {
  int calc_size;
  calc_size = alloc_size;
  while (calc_size < needed_size) {
    calc_size <<= 1;  // same as *= 2
    //calc_size += CHUNK_SIZE;  -- this is slower than the above line, because there are more reallocations
  }
  return calc_size;
}

static BOOL CheckAllocation(binn *item, int add_size) {
  int alloc_size;
  void *ptr;
  if (item->used_size + add_size > item->alloc_size) {
    if (item->pre_allocated) {
      return FALSE;
    }
    alloc_size = CalcAllocation(item->used_size + add_size, item->alloc_size);
    ptr = realloc(item->pbuf, alloc_size);
    if (ptr == NULL) {
      return FALSE;
    }
    item->pbuf = ptr;
    item->alloc_size = alloc_size;
  }
  return TRUE;
}

#define BINN_MAGIC 0x1F22B11F

#define MAX_BINN_HEADER 9        // [1:type][4:size][4:count]
#define MIN_BINN_SIZE   3        // [1:type][1:size][1:count]
#define CHUNK_SIZE      256      // 1024

extern void*(*malloc_fn)(int len);
extern void* (*realloc_fn)(void *ptr, int len);
extern void (*free_fn)(void *ptr);

/*************************************************************************************/

typedef unsigned short int     u16;
typedef unsigned int           u32;
typedef unsigned long long int u64;

/***************************************************************************/

void *memdup(void *src, int size) {
  void *dest;
  if ((src == NULL) || (size <= 0)) {
    return NULL;
  }
  dest = malloc(size);
  if (dest == NULL) {
    return NULL;
  }
  memcpy(dest, src, size);
  return dest;
}

/***************************************************************************/

char *i64toa(int64 value, char *buf, int radix) {
#ifdef _MSC_VER
  return _i64toa(value, buf, radix);
#else
  switch (radix) {
    case 10:
      snprintf(buf, 64, "%" INT64_FORMAT, value);
      break;
    case 16:
      snprintf(buf, 64, "%" INT64_HEX_FORMAT, value);
      break;
    default:
      buf[0] = 0;
  }
  return buf;
#endif
}

/*************************************************************************************/

void pass_int64(int64 a) {

  CU_ASSERT(a == 9223372036854775807);
  CU_ASSERT(a > 9223372036854775806);
}

int64 return_int64() {

  return 9223372036854775807;
}

int64 return_passed_int64(int64 a) {

  return a;
}

/*************************************************************************************/

void test_int64() {
  int64 i64;
  //uint64 b;
  //long long int b;  -- did not work!
  char buf[64];

  printf("testing int64... ");

  pass_int64(9223372036854775807);

  i64 = return_int64();
  CU_ASSERT(i64 == 9223372036854775807);

  /*  do not worked!
     b = 9223372036854775807;
     printf("value of b1=%" G_GINT64_FORMAT "\n", b);
     snprintf(64, buf, "%" G_GINT64_FORMAT, b);
     printf(" value of b2=%s\n", buf);

     ltoa(i64, buf, 10);
     printf(" value of i64=%s\n", buf);
   */

  i64toa(i64, buf, 10);
  //printf(" value of i64=%s\n", buf);
  CU_ASSERT(strcmp(buf, "9223372036854775807") == 0);

  i64 = return_passed_int64(-987654321987654321);
  CU_ASSERT(i64 == -987654321987654321);

  //snprintf(64, buf, "%" G_GINT64_FORMAT, i64);
  i64toa(i64, buf, 10);
  CU_ASSERT(strcmp(buf, "-987654321987654321") == 0);

  printf("OK\n");
}

/*************************************************************************************/

//! this code may not work on processors that does not use the default float standard
//  original name: AlmostEqual2sComplement
BOOL AlmostEqualFloats(float A, float B, int maxUlps) {
  int aInt, bInt, intDiff;
  // Make sure maxUlps is non-negative and small enough that the
  // default NAN won't compare as equal to anything.
  CU_ASSERT(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);
  aInt = *(int*) &A;
  bInt = *(int*) &B;
  // Make aInt lexicographically ordered as a twos-complement int
  if (aInt < 0) {
    aInt = 0x80000000 - aInt;
  }
  if (bInt < 0) {
    bInt = 0x80000000 - bInt;
  }
  intDiff = abs(aInt - bInt);
  if (intDiff <= maxUlps) {
    return TRUE;
  }
  return FALSE;
}

/*************************************************************************************/

#define VERYSMALL (1.0E-150)
#define EPSILON   (1.0E-8)

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

BOOL AlmostEqualDoubles(double a, double b) {
  double absDiff, maxAbs, absA, absB;

  absDiff = fabs(a - b);
  if (absDiff < VERYSMALL) {
    return TRUE;
  }

  absA = fabs(a);
  absB = fabs(b);
  maxAbs = max(absA, absB);
  if ((absDiff / maxAbs) < EPSILON) {
    return TRUE;
  }
  printf("a=%g b=%g\n", a, b);
  return FALSE;
}

/*************************************************************************************/

void test_floating_point_numbers() {
  char buf[256];
  float f1;
  double d1;

  printf("testing floating point... ");

  f1 = 1.25;
  CU_ASSERT(f1 == 1.25);
  d1 = 1.25;
  CU_ASSERT(d1 == 1.25);

  d1 = 0;
  d1 = f1;
  CU_ASSERT(d1 == 1.25);
  f1 = 0;
  f1 = d1;
  CU_ASSERT(f1 == 1.25);

  d1 = 1.234;
  CU_ASSERT(AlmostEqualDoubles(d1, 1.234) == TRUE);
  f1 = d1;
  CU_ASSERT(AlmostEqualFloats(f1, 1.234, 2) == TRUE);

  d1 = 1.2345;
  CU_ASSERT(AlmostEqualDoubles(d1, 1.2345) == TRUE);
  f1 = d1;
  CU_ASSERT(AlmostEqualFloats(f1, 1.2345, 2) == TRUE);


  // from string to number, and back to string

  d1 = atof("1.234");  // converts from string to double
  CU_ASSERT(AlmostEqualDoubles(d1, 1.234) == TRUE);
  f1 = d1;             // converts from double to float
  CU_ASSERT(AlmostEqualFloats(f1, 1.234, 2) == TRUE);

  /*
     sprintf(buf, "%f", d1);  // from double to string
     CU_ASSERT(buf[0] != 0);
     CU_ASSERT(strcmp(buf, "1.234") == 0);
   */

  sprintf(buf, "%g", d1);
  CU_ASSERT(buf[0] != 0);
  CU_ASSERT(strcmp(buf, "1.234") == 0);


  d1 = atof("12.34");
  CU_ASSERT(d1 == 12.34);
  f1 = d1;
  CU_ASSERT(AlmostEqualFloats(f1, 12.34, 2) == TRUE);

  /*
     sprintf(buf, "%f", d1);  // from double to string
     CU_ASSERT(buf[0] != 0);
     CU_ASSERT(strcmp(buf, "12.34") == 0);
   */

  sprintf(buf, "%g", d1);
  CU_ASSERT(buf[0] != 0);
  CU_ASSERT(strcmp(buf, "12.34") == 0);


  d1 = atof("1.234e25");
  CU_ASSERT(AlmostEqualDoubles(d1, 1.234e25) == TRUE);
  f1 = d1;
  CU_ASSERT(AlmostEqualFloats(f1, 1.234e25, 2) == TRUE);

  sprintf(buf, "%g", d1);
  CU_ASSERT(buf[0] != 0);
  //printf("\nbuf=%s\n", buf);
  //CU_ASSERT(strcmp(buf, "1.234e+025") == 0);


  printf("OK\n");
}

/*************************************************************************************/

void test1() {
  static const int fix_size = 512;
  int i = 8768787, blobsize;
  char *ptr, *p2, *ptr2;
  binn *obj1, *list, *map, *obj;  //, *list2=INVALID_BINN, *map2=INVALID_BINN, *obj2=INVALID_BINN;
  binn value;
  // test values
  char vbyte, *pblob;
  signed short vint16;
  unsigned short vuint16;
  signed int vint32;
  unsigned int vuint32;
  signed long long int vint64;
  unsigned long long int vuint64;

  printf("testing binn 1... ");

  // CalcAllocation and CheckAllocation -------------------------------------------------

  CU_ASSERT(CalcAllocation(512, 512) == 512);
  CU_ASSERT(CalcAllocation(510, 512) == 512);
  CU_ASSERT(CalcAllocation(1, 512) == 512);
  CU_ASSERT(CalcAllocation(0, 512) == 512);

  CU_ASSERT(CalcAllocation(513, 512) == 1024);
  CU_ASSERT(CalcAllocation(512 + CHUNK_SIZE, 512) == 1024);
  CU_ASSERT(CalcAllocation(1025, 512) == 2048);
  CU_ASSERT(CalcAllocation(1025, 1024) == 2048);
  CU_ASSERT(CalcAllocation(2100, 1024) == 4096);

  //CU_ASSERT(CheckAllocation(xxx) == xxx);


  // binn_new() ----------------------------------------------------------------------

  // invalid create calls
  CU_ASSERT(binn_new(-1, 0, NULL) == INVALID_BINN);
  CU_ASSERT(binn_new(0, 0, NULL) == INVALID_BINN);
  CU_ASSERT(binn_new(5, 0, NULL) == INVALID_BINN);
  CU_ASSERT(binn_new(BINN_MAP, -1, NULL) == INVALID_BINN);
  ptr = (char*) &obj1;   // create a valid pointer
  CU_ASSERT(binn_new(BINN_MAP, -1, ptr) == INVALID_BINN);
  CU_ASSERT(binn_new(BINN_MAP, MIN_BINN_SIZE - 1, ptr) == INVALID_BINN);

  // first valid create call
  obj1 = binn_new(BINN_LIST, 0, NULL);
  CU_ASSERT(obj1 != INVALID_BINN);

  CU_ASSERT(obj1->header == BINN_MAGIC);
  CU_ASSERT(obj1->type == BINN_LIST);
  CU_ASSERT(obj1->count == 0);
  CU_ASSERT(obj1->pbuf != NULL);
  CU_ASSERT(obj1->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(obj1->used_size == MAX_BINN_HEADER);
  CU_ASSERT(obj1->pre_allocated == FALSE);

  binn_free(obj1);


  // valid create call
  list = binn_new(BINN_LIST, 0, NULL);
  CU_ASSERT(list != INVALID_BINN);

  // valid create call
  map = binn_new(BINN_MAP, 0, NULL);
  CU_ASSERT(map != INVALID_BINN);

  // valid create call
  obj = binn_new(BINN_OBJECT, 0, NULL);
  CU_ASSERT(obj != INVALID_BINN);

  CU_ASSERT(list->header == BINN_MAGIC);
  CU_ASSERT(list->type == BINN_LIST);
  CU_ASSERT(list->count == 0);
  CU_ASSERT(list->pbuf != NULL);
  CU_ASSERT(list->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(list->used_size == MAX_BINN_HEADER);
  CU_ASSERT(list->pre_allocated == FALSE);

  CU_ASSERT(map->header == BINN_MAGIC);
  CU_ASSERT(map->type == BINN_MAP);
  CU_ASSERT(map->count == 0);
  CU_ASSERT(map->pbuf != NULL);
  CU_ASSERT(map->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(map->used_size == MAX_BINN_HEADER);
  CU_ASSERT(map->pre_allocated == FALSE);

  CU_ASSERT(obj->header == BINN_MAGIC);
  CU_ASSERT(obj->type == BINN_OBJECT);
  CU_ASSERT(obj->count == 0);
  CU_ASSERT(obj->pbuf != NULL);
  CU_ASSERT(obj->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(obj->used_size == MAX_BINN_HEADER);
  CU_ASSERT(obj->pre_allocated == FALSE);


  // test create with pre-allocated buffer ----------------------------------------------

  ptr = malloc(fix_size);
  CU_ASSERT(ptr != NULL);

  obj1 = binn_new(BINN_OBJECT, fix_size, ptr);
  CU_ASSERT(obj1 != INVALID_BINN);

  CU_ASSERT(obj1->header == BINN_MAGIC);
  CU_ASSERT(obj1->type == BINN_OBJECT);
  CU_ASSERT(obj1->count == 0);
  CU_ASSERT(obj1->pbuf != NULL);
  CU_ASSERT(obj1->alloc_size == fix_size);
  CU_ASSERT(obj1->used_size == MAX_BINN_HEADER);
  CU_ASSERT(obj1->pre_allocated == TRUE);


  // add values - invalid ---------------------------------------------------------------

  CU_ASSERT(binn_map_set(list, 55001, BINN_INT32, &i, 0) == FALSE);
  CU_ASSERT(binn_object_set(list, "test", BINN_INT32, &i, 0) == FALSE);

  CU_ASSERT(binn_list_add(map, BINN_INT32, &i, 0) == FALSE);
  CU_ASSERT(binn_object_set(map, "test", BINN_INT32, &i, 0) == FALSE);

  CU_ASSERT(binn_list_add(obj, BINN_INT32, &i, 0) == FALSE);
  CU_ASSERT(binn_map_set(obj, 55001, BINN_INT32, &i, 0) == FALSE);

  // invalid type
  CU_ASSERT(binn_list_add(list, -1, &i, 0) == FALSE);
  CU_ASSERT(binn_list_add(list, 0x1FFFF, &i, 0) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, -1, &i, 0) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, 0x1FFFF, &i, 0) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", -1, &i, 0) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", 0x1FFFF, &i, 0) == FALSE);

  // null pointers
  CU_ASSERT(binn_list_add(list, BINN_INT8, NULL, 0) == FALSE);
  CU_ASSERT(binn_list_add(list, BINN_INT16, NULL, 0) == FALSE);
  CU_ASSERT(binn_list_add(list, BINN_INT32, NULL, 0) == FALSE);
  CU_ASSERT(binn_list_add(list, BINN_INT64, NULL, 0) == FALSE);
  //CU_ASSERT(binn_list_add(list, BINN_STRING, NULL, 0) == TRUE);  //*
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT8, NULL, 0) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT16, NULL, 0) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT32, NULL, 0) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT64, NULL, 0) == FALSE);
  //CU_ASSERT(binn_map_set(map, 5501, BINN_STRING, NULL, 0) == TRUE);  //*
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT8, NULL, 0) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT16, NULL, 0) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT32, NULL, 0) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT64, NULL, 0) == FALSE);
  //CU_ASSERT(binn_object_set(obj, "test", BINN_STRING, NULL, 0) == TRUE);  //*

  // blobs with null pointers
  CU_ASSERT(binn_list_add(list, BINN_BLOB, NULL, -1) == FALSE);
  CU_ASSERT(binn_list_add(list, BINN_BLOB, NULL, 10) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_BLOB, NULL, -1) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_BLOB, NULL, 10) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_BLOB, NULL, -1) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_BLOB, NULL, 10) == FALSE);

  // blobs with negative values
  CU_ASSERT(binn_list_add(list, BINN_BLOB, &i, -1) == FALSE);
  CU_ASSERT(binn_list_add(list, BINN_BLOB, &i, -15) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_BLOB, &i, -1) == FALSE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_BLOB, &i, -15) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_BLOB, &i, -1) == FALSE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_BLOB, &i, -15) == FALSE);


  // read values - invalid 1 - empty binns -------------------------------------------

  ptr2 = binn_ptr(list);
  CU_ASSERT(ptr2 != NULL);
  CU_ASSERT(binn_list_get_value(ptr2, 0, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, 1, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, 2, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, -1, &value) == FALSE);

  ptr2 = binn_ptr(map);
  CU_ASSERT(ptr2 != NULL);
  CU_ASSERT(binn_list_get_value(ptr2, 0, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, 1, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, 2, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, -1, &value) == FALSE);


  ptr2 = binn_ptr(obj);
  CU_ASSERT(ptr2 != NULL);
  CU_ASSERT(binn_list_get_value(ptr2, 0, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, 1, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, 2, &value) == FALSE);
  CU_ASSERT(binn_list_get_value(ptr2, -1, &value) == FALSE);

  // add values - valid -----------------------------------------------------------------

  CU_ASSERT(binn_list_add(list, BINN_INT32, &i, 0) == TRUE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT32, &i, 0) == TRUE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT32, &i, 0) == FALSE);       // with the same ID
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT32, &i, 0) == TRUE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT32, &i, 0) == FALSE); // with the same name

  vbyte = (char) 255;
  vint16 = -32000;
  vuint16 = 65000;
  vint32 = -65000000;
  vuint32 = 65000000;
  vint64 = -6500000000000000;
  vuint64 = 6500000000000000;
  blobsize = 150;
  pblob = malloc(blobsize);
  CU_ASSERT(pblob != NULL);
  memset(pblob, 55, blobsize);

  CU_ASSERT(binn_list_add(list, BINN_NULL, 0, 0) == TRUE);                      // second
  CU_ASSERT(binn_list_add(list, BINN_UINT8, &vbyte, 0) == TRUE);                // third
  CU_ASSERT(binn_list_add(list, BINN_INT16, &vint16, 0) == TRUE);               // fourth
  CU_ASSERT(binn_list_add(list, BINN_UINT16, &vuint16, 0) == TRUE);             // fifth
  CU_ASSERT(binn_list_add(list, BINN_INT32, &vint32, 0) == TRUE);               // 6th
  CU_ASSERT(binn_list_add(list, BINN_UINT32, &vuint32, 0) == TRUE);             // 7th
  CU_ASSERT(binn_list_add(list, BINN_INT64, &vint64, 0) == TRUE);               // 8th
  CU_ASSERT(binn_list_add(list, BINN_UINT64, &vuint64, 0) == TRUE);             // 9th
  CU_ASSERT(binn_list_add(list, BINN_STRING, "this is the string", 0) == TRUE); // 10th
  CU_ASSERT(binn_list_add(list, BINN_BLOB, pblob, blobsize) == TRUE);           // 11th

  CU_ASSERT(binn_map_set(map, 99000, BINN_NULL, 0, 0) == TRUE);                      // third
  CU_ASSERT(binn_map_set(map, 99001, BINN_UINT8, &vbyte, 0) == TRUE);                // fourth
  CU_ASSERT(binn_map_set(map, 99002, BINN_INT16, &vint16, 0) == TRUE);               // fifth
  CU_ASSERT(binn_map_set(map, 99003, BINN_UINT16, &vuint16, 0) == TRUE);             // 6th
  CU_ASSERT(binn_map_set(map, 99004, BINN_INT32, &vint32, 0) == TRUE);               // 7th
  CU_ASSERT(binn_map_set(map, 99005, BINN_UINT32, &vuint32, 0) == TRUE);             // 8th
  CU_ASSERT(binn_map_set(map, 99006, BINN_INT64, &vint64, 0) == TRUE);               // 9th
  CU_ASSERT(binn_map_set(map, 99007, BINN_UINT64, &vuint64, 0) == TRUE);             // 10th
  CU_ASSERT(binn_map_set(map, 99008, BINN_STRING, "this is the string", 0) == TRUE); // 11th
  CU_ASSERT(binn_map_set(map, 99009, BINN_BLOB, pblob, blobsize) == TRUE);           // 12th

  CU_ASSERT(binn_object_set(obj, "key0", BINN_NULL, 0, 0) == TRUE);                      // third
  CU_ASSERT(binn_object_set(obj, "key1", BINN_UINT8, &vbyte, 0) == TRUE);                // fourth
  CU_ASSERT(binn_object_set(obj, "key2", BINN_INT16, &vint16, 0) == TRUE);               // fifth
  CU_ASSERT(binn_object_set(obj, "key3", BINN_UINT16, &vuint16, 0) == TRUE);             // 6th
  CU_ASSERT(binn_object_set(obj, "key4", BINN_INT32, &vint32, 0) == TRUE);               // 7th
  CU_ASSERT(binn_object_set(obj, "key5", BINN_UINT32, &vuint32, 0) == TRUE);             // 8th
  CU_ASSERT(binn_object_set(obj, "key6", BINN_INT64, &vint64, 0) == TRUE);               // 9th
  CU_ASSERT(binn_object_set(obj, "key7", BINN_UINT64, &vuint64, 0) == TRUE);             // 10th
  CU_ASSERT(binn_object_set(obj, "key8", BINN_STRING, "this is the string", 0) == TRUE); // 11th
  CU_ASSERT(binn_object_set(obj, "key9", BINN_BLOB, pblob, blobsize) == TRUE);           // 12th

  // blobs with size = 0
  CU_ASSERT(binn_list_add(list, BINN_BLOB, ptr, 0) == TRUE);
  CU_ASSERT(binn_list_add(list, BINN_STRING, "", 0) == TRUE);
  CU_ASSERT(binn_list_add(list, BINN_STRING, "after the empty items", 0) == TRUE);


  // add values to a fixed-size binn (pre-allocated buffer) --------------------------

  CU_ASSERT(binn_list_add(obj1, BINN_INT32, &i, 0) == FALSE);
  CU_ASSERT(binn_map_set(obj1, 55001, BINN_INT32, &i, 0) == FALSE);

  CU_ASSERT(binn_object_set(obj1, "test", BINN_UINT32, &vuint32, 0) == TRUE);
  CU_ASSERT(binn_object_set(obj1, "test", BINN_UINT32, &vuint32, 0) == FALSE);  // with the same name

  CU_ASSERT(binn_object_set(obj1, "key1", BINN_STRING, "this is the value", 0) == TRUE);
  CU_ASSERT(binn_object_set(obj1, "key2", BINN_STRING, "the second value", 0) == TRUE);

  // create a long string buffer to make the test. the string is longer than the available space
  // in the binn.
  ptr2 = malloc(fix_size);
  CU_ASSERT(ptr2 != NULL);
  p2 = ptr2;
  for (i = 0; i < fix_size - 1; i++) {
    *p2 = 'A';
    p2++;
  }
  *p2 = '\0';
  CU_ASSERT(strlen(ptr2) == fix_size - 1);

  CU_ASSERT(binn_object_set(obj1, "v2", BINN_STRING, ptr2,
                            0) == FALSE); // it fails because it uses a pre-allocated memory block

  CU_ASSERT(binn_object_set(obj, "v2", BINN_STRING, ptr2,
                            0) == TRUE); // but this uses a dynamically allocated memory block, so it works with it
  CU_ASSERT(binn_object_set(obj, "Key00", BINN_STRING, "after the big string",
                            0) == TRUE); // and test the 'Key00' against the 'Key0'

  CU_ASSERT(binn_object_set(obj, "list", BINN_LIST, binn_ptr(list), binn_size(list)) == TRUE);
  CU_ASSERT(binn_object_set(obj, "Key10", BINN_STRING, "after the list",
                            0) == TRUE); // and test the 'Key10' against the 'Key1'


  // read values - invalid 2 ------------------------------------------------------------


  // read keys --------------------------------------------------------------------------


  // binn_size - invalid and valid args --------------------------------------------

  CU_ASSERT(binn_size(NULL) == 0);

  CU_ASSERT(binn_size(list) == list->size);
  CU_ASSERT(binn_size(map) == map->size);
  CU_ASSERT(binn_size(obj) == obj->size);
  CU_ASSERT(binn_size(obj1) == obj1->size);


  // destroy them all -------------------------------------------------------------------

  binn_free(list);
  binn_free(map);
  binn_free(obj);
  binn_free(obj1);
  free(pblob);
  free(ptr);
  free(ptr2);


  printf("OK\n");
}

/*************************************************************************************/

void _test2() {
  binn *list = INVALID_BINN, *map = INVALID_BINN, *obj = INVALID_BINN;
  binn value;
  BOOL vbool;
  int blobsize;
  char *pblob, *pstr;
  signed int vint32;
  double vdouble;

  char *str_list = "test list";
  char *str_map = "test map";
  char *str_obj = "test object";

  printf("testing binn 2");

  blobsize = 150;
  pblob = malloc(blobsize);
  CU_ASSERT(pblob != NULL);
  memset(pblob, 55, blobsize);

  CU_ASSERT(list == INVALID_BINN);
  CU_ASSERT(map == INVALID_BINN);
  CU_ASSERT(obj == INVALID_BINN);

  // add values without creating before

  CU_ASSERT(binn_list_add_int32(list, 123) == FALSE);
  CU_ASSERT(binn_map_set_int32(map, 1001, 456) == FALSE);
  CU_ASSERT(binn_object_set_int32(obj, "int", 789) == FALSE);

  // create the structures

  list = binn_list();
  map = binn_map();
  obj = binn_object();

  CU_ASSERT(list != INVALID_BINN);
  CU_ASSERT(map != INVALID_BINN);
  CU_ASSERT(obj != INVALID_BINN);

  // add values without creating before

  CU_ASSERT(binn_list_add_int32(list, 123) == TRUE);
  CU_ASSERT(binn_map_set_int32(map, 1001, 456) == TRUE);
  CU_ASSERT(binn_object_set_int32(obj, "int", 789) == TRUE);

  // check the structures

  CU_ASSERT(list->header == BINN_MAGIC);
  CU_ASSERT(list->type == BINN_LIST);
  CU_ASSERT(list->count == 1);
  CU_ASSERT(list->pbuf != NULL);
  CU_ASSERT(list->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(list->used_size > MAX_BINN_HEADER);
  CU_ASSERT(list->pre_allocated == FALSE);

  CU_ASSERT(map->header == BINN_MAGIC);
  CU_ASSERT(map->type == BINN_MAP);
  CU_ASSERT(map->count == 1);
  CU_ASSERT(map->pbuf != NULL);
  CU_ASSERT(map->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(map->used_size > MAX_BINN_HEADER);
  CU_ASSERT(map->pre_allocated == FALSE);

  CU_ASSERT(obj->header == BINN_MAGIC);
  CU_ASSERT(obj->type == BINN_OBJECT);
  CU_ASSERT(obj->count == 1);
  CU_ASSERT(obj->pbuf != NULL);
  CU_ASSERT(obj->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(obj->used_size > MAX_BINN_HEADER);
  CU_ASSERT(obj->pre_allocated == FALSE);


  // continue adding values

  CU_ASSERT(binn_list_add_double(list, 1.23) == TRUE);
  CU_ASSERT(binn_map_set_double(map, 1002, 4.56) == TRUE);
  CU_ASSERT(binn_object_set_double(obj, "double", 7.89) == TRUE);

  CU_ASSERT(list->count == 2);
  CU_ASSERT(map->count == 2);
  CU_ASSERT(obj->count == 2);

  CU_ASSERT(binn_list_add_bool(list, TRUE) == TRUE);
  CU_ASSERT(binn_map_set_bool(map, 1003, TRUE) == TRUE);
  CU_ASSERT(binn_object_set_bool(obj, "bool", TRUE) == TRUE);

  CU_ASSERT(list->count == 3);
  CU_ASSERT(map->count == 3);
  CU_ASSERT(obj->count == 3);

  CU_ASSERT(binn_list_add_str(list, str_list) == TRUE);
  CU_ASSERT(binn_map_set_str(map, 1004, str_map) == TRUE);
  CU_ASSERT(binn_object_set_str(obj, "text", str_obj) == TRUE);

  CU_ASSERT(list->count == 4);
  CU_ASSERT(map->count == 4);
  CU_ASSERT(obj->count == 4);

  CU_ASSERT(binn_list_add_blob(list, pblob, blobsize) == TRUE);
  CU_ASSERT(binn_map_set_blob(map, 1005, pblob, blobsize) == TRUE);
  CU_ASSERT(binn_object_set_blob(obj, "blob", pblob, blobsize) == TRUE);

  CU_ASSERT(list->count == 5);
  CU_ASSERT(map->count == 5);
  CU_ASSERT(obj->count == 5);

  CU_ASSERT(binn_count(list) == 5);
  CU_ASSERT(binn_count(map) == 5);
  CU_ASSERT(binn_count(obj) == 5);

  CU_ASSERT(binn_size(list) == list->size);
  CU_ASSERT(binn_size(map) == map->size);
  CU_ASSERT(binn_size(obj) == obj->size);

  CU_ASSERT(binn_type(list) == BINN_LIST);
  CU_ASSERT(binn_type(map) == BINN_MAP);
  CU_ASSERT(binn_type(obj) == BINN_OBJECT);


  // try to read them

  // integer

  CU_ASSERT(binn_list_get_value(list, 1, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.allocated == FALSE);

  CU_ASSERT(value.type == BINN_UINT8);
  CU_ASSERT(value.ptr != &value.vuint8);  // it must return a pointer to the byte in the buffer

  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vint == 123);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_map_get_value(map, 1001, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);

  CU_ASSERT(value.type == BINN_UINT16);
  CU_ASSERT(value.ptr == &value.vuint16);

  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vint == 456);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_object_get_value(obj, "int", &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);

  CU_ASSERT(value.type == BINN_UINT16);
  CU_ASSERT(value.ptr == &value.vuint16);

  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vint == 789);

  memset(&value, 0, sizeof(binn));


  // double

  CU_ASSERT(binn_list_get_value(list, 2, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_FLOAT64);
  CU_ASSERT(value.ptr == &value.vint);
  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vdouble == 1.23);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_map_get_value(map, 1002, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_FLOAT64);
  CU_ASSERT(value.ptr == &value.vint);
  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vdouble == 4.56);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_object_get_value(obj, "double", &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_FLOAT64);
  CU_ASSERT(value.ptr == &value.vint);
  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vdouble == 7.89);

  memset(&value, 0, sizeof(binn));


  // bool

  CU_ASSERT(binn_list_get_value(list, 3, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_BOOL);
  CU_ASSERT(value.ptr == &value.vint);
  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vbool == TRUE);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_map_get_value(map, 1003, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_BOOL);
  CU_ASSERT(value.ptr == &value.vint);
  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vbool == TRUE);

  CU_ASSERT(binn_object_get_value(obj, "bool", &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_BOOL);
  CU_ASSERT(value.ptr == &value.vint);
  CU_ASSERT(value.size == 0);
  CU_ASSERT(value.count == 0);
  CU_ASSERT(value.vbool == TRUE);

  memset(&value, 0, sizeof(binn));


  // string

  CU_ASSERT(binn_list_get_value(list, 4, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_STRING);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == strlen(str_list));
  CU_ASSERT(strcmp(value.ptr, str_list) == 0);
  CU_ASSERT(value.count == 0);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_map_get_value(map, 1004, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_STRING);
  CU_ASSERT(value.size == strlen(str_map));
  CU_ASSERT(strcmp(value.ptr, str_map) == 0);
  CU_ASSERT(value.count == 0);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_object_get_value(obj, "text", &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_STRING);
  CU_ASSERT(value.size == strlen(str_obj));
  CU_ASSERT(strcmp(value.ptr, str_obj) == 0);
  CU_ASSERT(value.count == 0);

  memset(&value, 0, sizeof(binn));


  // blob

  CU_ASSERT(binn_list_get_value(list, 5, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_BLOB);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);
  CU_ASSERT(value.count == 0);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_map_get_value(map, 1005, &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_BLOB);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);
  CU_ASSERT(value.count == 0);

  memset(&value, 0, sizeof(binn));

  CU_ASSERT(binn_object_get_value(obj, "blob", &value) == TRUE);

  CU_ASSERT(value.header == BINN_MAGIC);
  CU_ASSERT(value.writable == FALSE);
  CU_ASSERT(value.type == BINN_BLOB);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);
  CU_ASSERT(value.count == 0);

  memset(&value, 0, sizeof(binn));


  // read with other interface

  CU_ASSERT(binn_list_get_int32(list, 1, &vint32) == TRUE);
  CU_ASSERT(vint32 == 123);

  CU_ASSERT(binn_map_get_int32(map, 1001, &vint32) == TRUE);
  CU_ASSERT(vint32 == 456);

  CU_ASSERT(binn_object_get_int32(obj, "int", &vint32) == TRUE);
  CU_ASSERT(vint32 == 789);

  // double

  CU_ASSERT(binn_list_get_double(list, 2, &vdouble) == TRUE);
  CU_ASSERT(vdouble == 1.23);

  CU_ASSERT(binn_map_get_double(map, 1002, &vdouble) == TRUE);
  CU_ASSERT(vdouble == 4.56);

  CU_ASSERT(binn_object_get_double(obj, "double", &vdouble) == TRUE);
  CU_ASSERT(vdouble == 7.89);

  // bool

  CU_ASSERT(binn_list_get_bool(list, 3, &vbool) == TRUE);
  CU_ASSERT(vbool == TRUE);

  CU_ASSERT(binn_map_get_bool(map, 1003, &vbool) == TRUE);
  CU_ASSERT(vbool == TRUE);

  CU_ASSERT(binn_object_get_bool(obj, "bool", &vbool) == TRUE);
  CU_ASSERT(vbool == TRUE);

  // string

  CU_ASSERT(binn_list_get_str(list, 4, &pstr) == TRUE);
  CU_ASSERT(pstr != 0);
  CU_ASSERT(strcmp(pstr, str_list) == 0);

  CU_ASSERT(binn_map_get_str(map, 1004, &pstr) == TRUE);
  CU_ASSERT(pstr != 0);
  CU_ASSERT(strcmp(pstr, str_map) == 0);

  CU_ASSERT(binn_object_get_str(obj, "text", &pstr) == TRUE);
  CU_ASSERT(pstr != 0);
  CU_ASSERT(strcmp(pstr, str_obj) == 0);

  // blob

  value.ptr = 0;
  value.size = 0;
  CU_ASSERT(binn_list_get_blob(list, 5, &value.ptr, &value.size) == TRUE);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);

  value.ptr = 0;
  value.size = 0;
  CU_ASSERT(binn_map_get_blob(map, 1005, &value.ptr, &value.size) == TRUE);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);

  value.ptr = 0;
  value.size = 0;
  CU_ASSERT(binn_object_get_blob(obj, "blob", &value.ptr, &value.size) == TRUE);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);


  // read with other interface

  CU_ASSERT(binn_list_int32(list, 1) == 123);
  CU_ASSERT(binn_map_int32(map, 1001) == 456);
  CU_ASSERT(binn_object_int32(obj, "int") == 789);

  // double

  CU_ASSERT(binn_list_double(list, 2) == 1.23);
  CU_ASSERT(binn_map_double(map, 1002) == 4.56);
  CU_ASSERT(binn_object_double(obj, "double") == 7.89);

  // bool

  CU_ASSERT(binn_list_bool(list, 3) == TRUE);
  CU_ASSERT(binn_map_bool(map, 1003) == TRUE);
  CU_ASSERT(binn_object_bool(obj, "bool") == TRUE);

  // string

  pstr = binn_list_str(list, 4);
  CU_ASSERT(pstr != 0);
  CU_ASSERT(strcmp(pstr, str_list) == 0);

  pstr = binn_map_str(map, 1004);
  CU_ASSERT(pstr != 0);
  CU_ASSERT(strcmp(pstr, str_map) == 0);

  pstr = binn_object_str(obj, "text");
  CU_ASSERT(pstr != 0);
  CU_ASSERT(strcmp(pstr, str_obj) == 0);

  // blob

  value.ptr = binn_list_blob(list, 5, &value.size);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);

  value.ptr = binn_map_blob(map, 1005, &value.size);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);

  value.ptr = binn_object_blob(obj, "blob", &value.size);
  CU_ASSERT(value.ptr != 0);
  CU_ASSERT(value.size == blobsize);
  CU_ASSERT(memcmp(value.ptr, pblob, blobsize) == 0);


  binn_free(list);
  binn_free(map);
  binn_free(obj);


  free(pblob);

  printf("OK\n");
}

void test2() {
  _test2();
}

/*************************************************************************************/

void test4() {
  static const int fix_size = 512;
  int i, id, type, count, size, header_size, blobsize;
  char *ptr, *p2, *pstr, key[256];
  binn *list, *map, *obj, *obj1;
  binn value;
  // test values
  char vbyte, *pblob;
  signed short vint16, *pint16;
  unsigned short vuint16, *puint16;
  signed int vint32, *pint32;
  unsigned int vuint32, *puint32;
  signed long long int vint64, *pint64;
  unsigned long long int vuint64, *puint64;

  printf("testing binn 3... ");

  list = binn_list();
  CU_ASSERT(list != INVALID_BINN);

  map = binn_map();
  CU_ASSERT(map != INVALID_BINN);

  obj = binn_object();
  CU_ASSERT(obj != INVALID_BINN);

  CU_ASSERT(list->header == BINN_MAGIC);
  CU_ASSERT(list->type == BINN_LIST);
  CU_ASSERT(list->count == 0);
  CU_ASSERT(list->pbuf != NULL);
  CU_ASSERT(list->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(list->used_size == MAX_BINN_HEADER);
  CU_ASSERT(list->pre_allocated == FALSE);

  CU_ASSERT(map->header == BINN_MAGIC);
  CU_ASSERT(map->type == BINN_MAP);
  CU_ASSERT(map->count == 0);
  CU_ASSERT(map->pbuf != NULL);
  CU_ASSERT(map->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(map->used_size == MAX_BINN_HEADER);
  CU_ASSERT(map->pre_allocated == FALSE);

  CU_ASSERT(obj->header == BINN_MAGIC);
  CU_ASSERT(obj->type == BINN_OBJECT);
  CU_ASSERT(obj->count == 0);
  CU_ASSERT(obj->pbuf != NULL);
  CU_ASSERT(obj->alloc_size > MAX_BINN_HEADER);
  CU_ASSERT(obj->used_size == MAX_BINN_HEADER);
  CU_ASSERT(obj->pre_allocated == FALSE);


  // test create with pre-allocated buffer ----------------------------------------------

  ptr = malloc(fix_size);
  CU_ASSERT(ptr != NULL);

  obj1 = binn_new(BINN_OBJECT, fix_size, ptr);
  CU_ASSERT(obj1 != INVALID_BINN);

  CU_ASSERT(obj1->header == BINN_MAGIC);
  CU_ASSERT(obj1->type == BINN_OBJECT);
  CU_ASSERT(obj1->count == 0);
  CU_ASSERT(obj1->pbuf != NULL);
  CU_ASSERT(obj1->alloc_size == fix_size);
  CU_ASSERT(obj1->used_size == MAX_BINN_HEADER);
  CU_ASSERT(obj1->pre_allocated == TRUE);


  // add values - invalid ---------------------------------------------------------------


  // read values - invalid 1 - empty binns -------------------------------------------

  ptr = binn_ptr(list);
  CU_ASSERT(ptr != NULL);
  CU_ASSERT(binn_list_read(ptr, 0, &type, &size) == NULL);
  CU_ASSERT(binn_list_read(ptr, 1, &type, &size) == NULL);
  CU_ASSERT(binn_list_read(ptr, 2, &type, &size) == NULL);
  CU_ASSERT(binn_list_read(ptr, -1, &type, &size) == NULL);

  ptr = binn_ptr(map);
  CU_ASSERT(ptr != NULL);
  CU_ASSERT(binn_map_read(ptr, 0, &type, &size) == NULL);
  CU_ASSERT(binn_map_read(ptr, 55001, &type, &size) == NULL);
  CU_ASSERT(binn_map_read(ptr, -1, &type, &size) == NULL);

  ptr = binn_ptr(obj);
  CU_ASSERT(ptr != NULL);
  CU_ASSERT(binn_object_read(ptr, NULL, &type, &size) == NULL);
  CU_ASSERT(binn_object_read(ptr, "", &type, &size) == NULL);
  CU_ASSERT(binn_object_read(ptr, "test", &type, &size) == NULL);


  // add values - valid -----------------------------------------------------------------

  CU_ASSERT(binn_list_add(list, BINN_INT32, &i, 0) == TRUE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT32, &i, 0) == TRUE);
  CU_ASSERT(binn_map_set(map, 5501, BINN_INT32, &i, 0) == FALSE);       // with the same ID
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT32, &i, 0) == TRUE);
  CU_ASSERT(binn_object_set(obj, "test", BINN_INT32, &i, 0) == FALSE); // with the same name

  vbyte = (char) 255;
  vint16 = -32000;
  vuint16 = 65000;
  vint32 = -65000000;
  vuint32 = 65000000;
  vint64 = -6500000000000000;
  vuint64 = 6500000000000000;
  blobsize = 150;
  pblob = malloc(blobsize);
  CU_ASSERT(pblob != NULL);
  memset(pblob, 55, blobsize);

  CU_ASSERT(binn_list_add(list, BINN_NULL, 0, 0) == TRUE);                      // second
  CU_ASSERT(binn_list_add(list, BINN_UINT8, &vbyte, 0) == TRUE);                // third
  CU_ASSERT(binn_list_add(list, BINN_INT16, &vint16, 0) == TRUE);               // fourth
  CU_ASSERT(binn_list_add(list, BINN_UINT16, &vuint16, 0) == TRUE);             // fifth
  CU_ASSERT(binn_list_add(list, BINN_INT32, &vint32, 0) == TRUE);               // 6th
  CU_ASSERT(binn_list_add(list, BINN_UINT32, &vuint32, 0) == TRUE);             // 7th
  CU_ASSERT(binn_list_add(list, BINN_INT64, &vint64, 0) == TRUE);               // 8th
  CU_ASSERT(binn_list_add(list, BINN_UINT64, &vuint64, 0) == TRUE);             // 9th
  CU_ASSERT(binn_list_add(list, BINN_STRING, "this is the string", 0) == TRUE); // 10th
  CU_ASSERT(binn_list_add(list, BINN_BLOB, pblob, blobsize) == TRUE);           // 11th

  CU_ASSERT(binn_map_set(map, 99000, BINN_NULL, 0, 0) == TRUE);                      // third
  CU_ASSERT(binn_map_set(map, 99001, BINN_UINT8, &vbyte, 0) == TRUE);                // fourth
  CU_ASSERT(binn_map_set(map, 99002, BINN_INT16, &vint16, 0) == TRUE);               // fifth
  CU_ASSERT(binn_map_set(map, 99003, BINN_UINT16, &vuint16, 0) == TRUE);             // 6th
  CU_ASSERT(binn_map_set(map, 99004, BINN_INT32, &vint32, 0) == TRUE);               // 7th
  CU_ASSERT(binn_map_set(map, 99005, BINN_UINT32, &vuint32, 0) == TRUE);             // 8th
  CU_ASSERT(binn_map_set(map, 99006, BINN_INT64, &vint64, 0) == TRUE);               // 9th
  CU_ASSERT(binn_map_set(map, 99007, BINN_UINT64, &vuint64, 0) == TRUE);             // 10th
  CU_ASSERT(binn_map_set(map, 99008, BINN_STRING, "this is the string", 0) == TRUE); // 11th
  CU_ASSERT(binn_map_set(map, 99009, BINN_BLOB, pblob, blobsize) == TRUE);           // 12th

  CU_ASSERT(binn_object_set(obj, "key0", BINN_NULL, 0, 0) == TRUE);                      // third
  CU_ASSERT(binn_object_set(obj, "key1", BINN_UINT8, &vbyte, 0) == TRUE);                // fourth
  CU_ASSERT(binn_object_set(obj, "key2", BINN_INT16, &vint16, 0) == TRUE);               // fifth
  CU_ASSERT(binn_object_set(obj, "key3", BINN_UINT16, &vuint16, 0) == TRUE);             // 6th
  CU_ASSERT(binn_object_set(obj, "key4", BINN_INT32, &vint32, 0) == TRUE);               // 7th
  CU_ASSERT(binn_object_set(obj, "key5", BINN_UINT32, &vuint32, 0) == TRUE);             // 8th
  CU_ASSERT(binn_object_set(obj, "key6", BINN_INT64, &vint64, 0) == TRUE);               // 9th
  CU_ASSERT(binn_object_set(obj, "key7", BINN_UINT64, &vuint64, 0) == TRUE);             // 10th
  CU_ASSERT(binn_object_set(obj, "key8", BINN_STRING, "this is the string", 0) == TRUE); // 11th
  CU_ASSERT(binn_object_set(obj, "key9", BINN_BLOB, pblob, blobsize) == TRUE);           // 12th

  // blobs with size = 0
  CU_ASSERT(binn_list_add(list, BINN_BLOB, ptr, 0) == TRUE);
  CU_ASSERT(binn_list_add(list, BINN_STRING, "", 0) == TRUE);
  CU_ASSERT(binn_list_add(list, BINN_STRING, "after the empty items", 0) == TRUE);


  // add values to a fixed-size binn (pre-allocated buffer) --------------------------

  CU_ASSERT(binn_list_add(obj1, BINN_INT32, &i, 0) == FALSE);
  CU_ASSERT(binn_map_set(obj1, 55001, BINN_INT32, &i, 0) == FALSE);

  CU_ASSERT(binn_object_set(obj1, "test", BINN_UINT32, &vuint32, 0) == TRUE);
  CU_ASSERT(binn_object_set(obj1, "test", BINN_UINT32, &vuint32, 0) == FALSE);  // with the same name

  CU_ASSERT(binn_object_set(obj1, "key1", BINN_STRING, "this is the value", 0) == TRUE);
  CU_ASSERT(binn_object_set(obj1, "key2", BINN_STRING, "the second value", 0) == TRUE);

  // create a long string buffer to make the test. the string is longer than the available space
  // in the binn.
  ptr = malloc(fix_size);
  CU_ASSERT(ptr != NULL);
  p2 = ptr;
  for (i = 0; i < fix_size - 1; i++) {
    *p2 = 'A';
    p2++;
  }
  *p2 = '\0';
  CU_ASSERT(strlen(ptr) == fix_size - 1);

  CU_ASSERT(binn_object_set(obj1, "v2", BINN_STRING, ptr,
                            0) == FALSE); // it fails because it uses a pre-allocated memory block

  CU_ASSERT(binn_object_set(obj, "v2", BINN_STRING, ptr,
                            0) == TRUE); // but this uses a dynamically allocated memory block, so it works with it
  CU_ASSERT(binn_object_set(obj, "Key00", BINN_STRING, "after the big string",
                            0) == TRUE); // and test the 'Key00' against the 'Key0'

  free(ptr);
  ptr = 0;

  CU_ASSERT(binn_object_set(obj, "list", BINN_LIST, binn_ptr(list), binn_size(list)) == TRUE);
  CU_ASSERT(binn_object_set(obj, "Key10", BINN_STRING, "after the list",
                            0) == TRUE); // and test the 'Key10' against the 'Key1'


  // read values - invalid 2 ------------------------------------------------------------


  // read keys --------------------------------------------------------------------------

  ptr = binn_ptr(map);
  CU_ASSERT(ptr != NULL);

  CU_ASSERT(binn_map_get_pair(ptr, -1, &id, &value) == FALSE);
  CU_ASSERT(binn_map_get_pair(ptr, 0, &id, &value) == FALSE);

  CU_ASSERT(binn_map_get_pair(ptr, 1, &id, &value) == TRUE);
  CU_ASSERT(id == 5501);
  CU_ASSERT(binn_map_get_pair(ptr, 2, &id, &value) == TRUE);
  CU_ASSERT(id == 99000);
  CU_ASSERT(binn_map_get_pair(ptr, 3, &id, &value) == TRUE);
  CU_ASSERT(id == 99001);
  CU_ASSERT(binn_map_get_pair(ptr, 10, &id, &value) == TRUE);
  CU_ASSERT(id == 99008);
  CU_ASSERT(binn_map_get_pair(ptr, 11, &id, &value) == TRUE);
  CU_ASSERT(id == 99009);


  ptr = binn_ptr(obj);
  CU_ASSERT(ptr != NULL);

  CU_ASSERT(binn_object_get_pair(ptr, -1, key, &value) == FALSE);
  CU_ASSERT(binn_object_get_pair(ptr, 0, key, &value) == FALSE);

  CU_ASSERT(binn_object_get_pair(ptr, 1, key, &value) == TRUE);
  CU_ASSERT(strcmp(key, "test") == 0);
  CU_ASSERT(binn_object_get_pair(ptr, 2, key, &value) == TRUE);
  CU_ASSERT(strcmp(key, "key0") == 0);
  CU_ASSERT(binn_object_get_pair(ptr, 3, key, &value) == TRUE);
  CU_ASSERT(strcmp(key, "key1") == 0);
  CU_ASSERT(binn_object_get_pair(ptr, 10, key, &value) == TRUE);
  CU_ASSERT(strcmp(key, "key8") == 0);
  CU_ASSERT(binn_object_get_pair(ptr, 11, key, &value) == TRUE);
  CU_ASSERT(strcmp(key, "key9") == 0);


  // read values - valid ----------------------------------------------------------------

  ptr = binn_ptr(obj1);
  CU_ASSERT(ptr != NULL);

  type = 0;
  size = 0;
  pstr = binn_object_read(ptr, "key1", &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "this is the value") == 0);

  type = 0;
  size = 0;
  pstr = binn_object_read(ptr, "key2", &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "the second value") == 0);

  type = 0;
  size = 0;
  pint32 = binn_object_read(ptr, "test", &type, &size);
  CU_ASSERT(pint32 != NULL);
  CU_ASSERT(type == BINN_UINT32);
  //CU_ASSERT(size > 0);
  CU_ASSERT(*pint32 == vuint32);


  ptr = binn_ptr(list);
  CU_ASSERT(ptr != NULL);

  type = 0;
  size = 0;
  pstr = binn_list_read(ptr, 2, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_NULL);
  //CU_ASSERT(size > 0);
  //CU_ASSERT(strcmp(pstr, "this is the value") == 0);

  type = 0;
  size = 0;
  p2 = binn_list_read(ptr, 3, &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_UINT8);
  CU_ASSERT(*p2 == vbyte);

  type = 0;
  size = 0;
  pint16 = binn_list_read(ptr, 4, &type, &size);
  CU_ASSERT(pint16 != NULL);
  CU_ASSERT(type == BINN_INT16);
  CU_ASSERT(*pint16 == vint16);

  type = 0;
  size = 0;
  puint16 = binn_list_read(ptr, 5, &type, &size);
  CU_ASSERT(puint16 != NULL);
  CU_ASSERT(type == BINN_UINT16);
  CU_ASSERT(*puint16 == vuint16);

  type = 0;
  size = 0;
  pint32 = binn_list_read(ptr, 6, &type, &size);
  CU_ASSERT(pint32 != NULL);
  CU_ASSERT(type == BINN_INT32);
  CU_ASSERT(*pint32 == vint32);
  // in the second time the value must be the same...
  type = 0;
  size = 0;
  pint32 = binn_list_read(ptr, 6, &type, &size);
  CU_ASSERT(pint32 != NULL);
  CU_ASSERT(type == BINN_INT32);
  CU_ASSERT(*pint32 == vint32);

  type = 0;
  size = 0;
  puint32 = binn_list_read(ptr, 7, &type, &size);
  CU_ASSERT(puint32 != NULL);
  CU_ASSERT(type == BINN_UINT32);
  CU_ASSERT(*puint32 == vuint32);

  type = 0;
  size = 0;
  pint64 = binn_list_read(ptr, 8, &type, &size);
  CU_ASSERT(pint64 != NULL);
  CU_ASSERT(type == BINN_INT64);
  CU_ASSERT(*pint64 == vint64);
  // in the second time the value must be the same...
  type = 0;
  size = 0;
  pint64 = binn_list_read(ptr, 8, &type, &size);
  CU_ASSERT(pint64 != NULL);
  CU_ASSERT(type == BINN_INT64);
  CU_ASSERT(*pint64 == vint64);

  type = 0;
  size = 0;
  puint64 = binn_list_read(ptr, 9, &type, &size);
  CU_ASSERT(puint64 != NULL);
  CU_ASSERT(type == BINN_UINT64);
  CU_ASSERT(*puint64 == vuint64);

  type = 0;
  size = 0;
  pstr = binn_list_read(ptr, 10, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "this is the string") == 0);

  type = 0;
  size = 0;
  p2 = binn_list_read(ptr, 11, &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_BLOB);
  CU_ASSERT(size == blobsize);
  CU_ASSERT(memcmp(p2, pblob, blobsize) == 0);


  ptr = binn_ptr(map);
  CU_ASSERT(ptr != NULL);

  type = 0;
  size = 0;
  pstr = binn_map_read(ptr, 99000, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_NULL);
  //CU_ASSERT(size > 0);
  //CU_ASSERT(strcmp(pstr, "this is the value") == 0);

  type = 0;
  size = 0;
  p2 = binn_map_read(ptr, 99001, &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_UINT8);
  CU_ASSERT(*p2 == vbyte);

  type = 0;
  size = 0;
  pint16 = binn_map_read(ptr, 99002, &type, &size);
  CU_ASSERT(pint16 != NULL);
  CU_ASSERT(type == BINN_INT16);
  CU_ASSERT(*pint16 == vint16);

  type = 0;
  size = 0;
  puint16 = binn_map_read(ptr, 99003, &type, &size);
  CU_ASSERT(puint16 != NULL);
  CU_ASSERT(type == BINN_UINT16);
  CU_ASSERT(*puint16 == vuint16);

  type = 0;
  size = 0;
  pint32 = binn_map_read(ptr, 99004, &type, &size);
  CU_ASSERT(pint32 != NULL);
  CU_ASSERT(type == BINN_INT32);
  CU_ASSERT(*pint32 == vint32);
  // in the second time the value must be the same...
  type = 0;
  size = 0;
  pint32 = binn_map_read(ptr, 99004, &type, &size);
  CU_ASSERT(pint32 != NULL);
  CU_ASSERT(type == BINN_INT32);
  CU_ASSERT(*pint32 == vint32);

  type = 0;
  size = 0;
  puint32 = binn_map_read(ptr, 99005, &type, &size);
  CU_ASSERT(puint32 != NULL);
  CU_ASSERT(type == BINN_UINT32);
  CU_ASSERT(*puint32 == vuint32);

  type = 0;
  size = 0;
  pint64 = binn_map_read(ptr, 99006, &type, &size);
  CU_ASSERT(pint64 != NULL);
  CU_ASSERT(type == BINN_INT64);
  CU_ASSERT(*pint64 == vint64);
  // in the second time the value must be the same...
  type = 0;
  size = 0;
  pint64 = binn_map_read(ptr, 99006, &type, &size);
  CU_ASSERT(pint64 != NULL);
  CU_ASSERT(type == BINN_INT64);
  CU_ASSERT(*pint64 == vint64);

  type = 0;
  size = 0;
  puint64 = binn_map_read(ptr, 99007, &type, &size);
  CU_ASSERT(puint64 != NULL);
  CU_ASSERT(type == BINN_UINT64);
  CU_ASSERT(*puint64 == vuint64);

  type = 0;
  size = 0;
  pstr = binn_map_read(ptr, 99008, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "this is the string") == 0);

  type = 0;
  size = 0;
  p2 = binn_map_read(ptr, 99009, &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_BLOB);
  CU_ASSERT(size == blobsize);
  CU_ASSERT(memcmp(p2, pblob, blobsize) == 0);


  ptr = binn_ptr(obj);
  CU_ASSERT(ptr != NULL);

  type = 0;
  size = 0;
  pstr = binn_object_read(ptr, "key0", &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_NULL);
  //CU_ASSERT(size > 0);
  //CU_ASSERT(strcmp(pstr, "this is the value") == 0);

  type = 0;
  size = 0;
  p2 = binn_object_read(ptr, "key1", &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_UINT8);
  CU_ASSERT(*p2 == vbyte);

  type = 0;
  size = 0;
  pint16 = binn_object_read(ptr, "key2", &type, &size);
  CU_ASSERT(pint16 != NULL);
  CU_ASSERT(type == BINN_INT16);
  CU_ASSERT(*pint16 == vint16);

  type = 0;
  size = 0;
  puint16 = binn_object_read(ptr, "key3", &type, &size);
  CU_ASSERT(puint16 != NULL);
  CU_ASSERT(type == BINN_UINT16);
  CU_ASSERT(*puint16 == vuint16);

  type = 0;
  size = 0;
  pint32 = binn_object_read(ptr, "key4", &type, &size);
  CU_ASSERT(pint32 != NULL);
  CU_ASSERT(type == BINN_INT32);
  CU_ASSERT(*pint32 == vint32);
  // in the second time the value must be the same...
  type = 0;
  size = 0;
  pint32 = binn_object_read(ptr, "key4", &type, &size);
  CU_ASSERT(pint32 != NULL);
  CU_ASSERT(type == BINN_INT32);
  CU_ASSERT(*pint32 == vint32);

  type = 0;
  size = 0;
  puint32 = binn_object_read(ptr, "key5", &type, &size);
  CU_ASSERT(puint32 != NULL);
  CU_ASSERT(type == BINN_UINT32);
  CU_ASSERT(*puint32 == vuint32);

  type = 0;
  size = 0;
  pint64 = binn_object_read(ptr, "key6", &type, &size);
  CU_ASSERT(pint64 != NULL);
  CU_ASSERT(type == BINN_INT64);
  CU_ASSERT(*pint64 == vint64);
  // in the second time the value must be the same...
  type = 0;
  size = 0;
  pint64 = binn_object_read(ptr, "key6", &type, &size);
  CU_ASSERT(pint64 != NULL);
  CU_ASSERT(type == BINN_INT64);
  CU_ASSERT(*pint64 == vint64);

  type = 0;
  size = 0;
  puint64 = binn_object_read(ptr, "key7", &type, &size);
  CU_ASSERT(puint64 != NULL);
  CU_ASSERT(type == BINN_UINT64);
  CU_ASSERT(*puint64 == vuint64);

  type = 0;
  size = 0;
  pstr = binn_object_read(ptr, "key8", &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "this is the string") == 0);

  type = 0;
  size = 0;
  p2 = binn_object_read(ptr, "key9", &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_BLOB);
  CU_ASSERT(size == blobsize);
  CU_ASSERT(memcmp(p2, pblob, blobsize) == 0);

  type = 0;
  size = 0;
  p2 = binn_object_read(ptr, "v2", &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size == fix_size - 1);
  CU_ASSERT(strlen(p2) == fix_size - 1);
  CU_ASSERT(p2[0] == 'A');
  CU_ASSERT(p2[1] == 'A');
  CU_ASSERT(p2[500] == 'A');
  CU_ASSERT(p2[fix_size - 1] == 0);

  type = 0;
  size = 0;
  pstr = binn_object_read(ptr, "key00", &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "after the big string") == 0);

  type = 0;
  size = 0;
  p2 = binn_object_read(ptr, "list", &type, &size);
  CU_ASSERT(p2 != NULL);
  CU_ASSERT(type == BINN_LIST);
  CU_ASSERT(size > 0);
  //
  type = 0;
  size = 0;
  puint64 = binn_list_read(p2, 9, &type, &size);
  CU_ASSERT(puint64 != NULL);
  CU_ASSERT(type == BINN_UINT64);
  CU_ASSERT(*puint64 == vuint64);
  //
  type = 0;
  size = 0;
  pstr = binn_list_read(p2, 10, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "this is the string") == 0);
  //
  type = 0;
  size = 0;
  pstr = binn_list_read(p2, 12, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_BLOB);   // empty blob
  CU_ASSERT(size == 0);
  //
  type = 0;
  size = 0;
  pstr = binn_list_read(p2, 13, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);   // empty string
  CU_ASSERT(size == 0);
  CU_ASSERT(strcmp(pstr, "") == 0);
  //
  type = 0;
  size = 0;
  pstr = binn_list_read(p2, 14, &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "after the empty items") == 0);

  type = 0;
  size = 0;
  pstr = binn_object_read(ptr, "key10", &type, &size);
  CU_ASSERT(pstr != NULL);
  CU_ASSERT(type == BINN_STRING);
  CU_ASSERT(size > 0);
  CU_ASSERT(strcmp(pstr, "after the list") == 0);


  // binn_ptr, IsValidBinnHeader, binn_is_valid...
  // also with invalid/null pointers, with pointers containing invalid data...

  CU_ASSERT(binn_ptr(NULL) == NULL);
  // pointers to invalid data
  //CU_ASSERT(binn_ptr(&type) == NULL);
  //CU_ASSERT(binn_ptr(&size) == NULL);
  //CU_ASSERT(binn_ptr(&count) == NULL);
  //CU_ASSERT(binn_is_valid_header(NULL) == FALSE);

  ptr = binn_ptr(obj);
  CU_ASSERT(ptr != NULL);
  // test the header
  size = 0;
  CU_ASSERT(binn_is_valid_header(ptr, &type, &count, &size, &header_size) == TRUE);
  CU_ASSERT(type == BINN_OBJECT);
  CU_ASSERT(count == 15);
  CU_ASSERT(header_size >= MIN_BINN_SIZE && header_size <= MAX_BINN_HEADER);
  CU_ASSERT(size > MIN_BINN_SIZE);
  CU_ASSERT(size == obj->size);
  // test all the buffer
  CU_ASSERT(binn_is_valid(ptr, &type, &count, &size) == TRUE);
  CU_ASSERT(type == BINN_OBJECT);
  CU_ASSERT(count == 15);
  CU_ASSERT(size > MIN_BINN_SIZE);
  CU_ASSERT(size == obj->size);

  ptr = binn_ptr(map);
  CU_ASSERT(ptr != NULL);
  // test the header
  size = 0;
  CU_ASSERT(binn_is_valid_header(ptr, &type, &count, &size, &header_size) == TRUE);
  CU_ASSERT(type == BINN_MAP);
  CU_ASSERT(count == 11);
  CU_ASSERT(header_size >= MIN_BINN_SIZE && header_size <= MAX_BINN_HEADER);
  CU_ASSERT(size > MIN_BINN_SIZE);
  CU_ASSERT(size == map->size);
  // test all the buffer
  CU_ASSERT(binn_is_valid(ptr, &type, &count, &size) == TRUE);
  CU_ASSERT(type == BINN_MAP);
  CU_ASSERT(count == 11);
  CU_ASSERT(size > MIN_BINN_SIZE);
  CU_ASSERT(size == map->size);

  ptr = binn_ptr(list);
  CU_ASSERT(ptr != NULL);
  // test the header
  size = 0;
  CU_ASSERT(binn_is_valid_header(ptr, &type, &count, &size, &header_size) == TRUE);
  CU_ASSERT(type == BINN_LIST);
  CU_ASSERT(count == 14);
  CU_ASSERT(header_size >= MIN_BINN_SIZE && header_size <= MAX_BINN_HEADER);
  CU_ASSERT(size > MIN_BINN_SIZE);
  CU_ASSERT(size == list->size);
  // test all the buffer
  CU_ASSERT(binn_is_valid(ptr, &type, &count, &size) == TRUE);
  CU_ASSERT(type == BINN_LIST);
  CU_ASSERT(count == 14);
  CU_ASSERT(header_size >= MIN_BINN_SIZE && header_size <= MAX_BINN_HEADER);
  CU_ASSERT(size > MIN_BINN_SIZE);
  CU_ASSERT(size == list->size);


  // binn_size - invalid and valid args --------------------------------------------

  CU_ASSERT(binn_size(NULL) == 0);

  CU_ASSERT(binn_size(list) == list->size);
  CU_ASSERT(binn_size(map) == map->size);
  CU_ASSERT(binn_size(obj) == obj->size);
  CU_ASSERT(binn_size(obj1) == obj1->size);


  // destroy them all -------------------------------------------------------------------

  binn_free(list);
  binn_free(map);
  binn_free(obj);

  printf("OK\n");
}

/*************************************************************************************/

void test_invalid_binn() {

  unsigned char buffers[][20] = {
    { 0xE0 },
    { 0xE0, 0x7E},
    { 0xE0, 0x7E, 0x7F},
    { 0xE0, 0x7E, 0x7F, 0x12},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0x01},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0x7F},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0xFF},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0xFF, 0xFF},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0x01},
    { 0xE0, 0x7E, 0x7F, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x7E, 0xFF},
    { 0xE0, 0x7E, 0xFF, 0x12},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0x01},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0x7F},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0xFF},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0xFF, 0xFF},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0x01},
    { 0xE0, 0x7E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x8E},
    { 0xE0, 0x8E, 0xFF},
    { 0xE0, 0x8E, 0xFF, 0x12},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0x01},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0x7F},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0xFF},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0xFF, 0xFF},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0x01},
    { 0xE0, 0x8E, 0xFF, 0x12, 0x34, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
  };

  int count, size, i;
  unsigned char *ptr;

  puts("testing invalid binn buffers...");

  count = sizeof buffers / sizeof buffers[0];

  for (i = 0; i < count; i++) {
    ptr = buffers[i];
    size = strlen((char*) ptr);
    printf("checking invalid binn #%d   size: %d bytes\n", i, size);
    CU_ASSERT(binn_is_valid_ex(ptr, NULL, NULL, &size) == FALSE);
  }

  puts("OK");
}

int main() {
  CU_pSuite pSuite = NULL;
  if (CUE_SUCCESS != CU_initialize_registry()) {
    return CU_get_error();
  }
  pSuite = CU_add_suite("jbl_test_binn1", init_suite, clean_suite);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  if (  (NULL == CU_add_test(pSuite, "test_int64", test_int64))
     || (NULL == CU_add_test(pSuite, "test_floating_point_numbers", test_floating_point_numbers))
     || (NULL == CU_add_test(pSuite, "test1", test1))
     || (NULL == CU_add_test(pSuite, "test2", test2))
     || (NULL == CU_add_test(pSuite, "test_invalid_binn", test_invalid_binn))) {
    CU_cleanup_registry();
    return CU_get_error();
  }
  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  int ret = CU_get_error() || CU_get_number_of_failures();
  CU_cleanup_registry();
  return ret;
}
