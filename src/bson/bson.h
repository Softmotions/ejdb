/**
 * @file bson.h
 * @brief BSON Declarations
 */

/*    Copyright 2009-2012 10gen Inc.
 *    Copyright (C) 2012-2015 Softmotions Ltd <info@softmotions.com>
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef BSON_H_
#define BSON_H_

#include "basedefs.h"
#include <stdio.h>
#include <stdint.h>
#if ! defined(__cplusplus)
#include <stdbool.h>
#endif
#include <time.h>
#include "tcutil.h"

#define BSON_IS_NUM_TYPE(atype) (atype == BSON_INT || atype == BSON_LONG || atype == BSON_DOUBLE)
#define BSON_IS_NULL_TYPE(atype) (atype == BSON_UNDEFINED || atype == BSON_NULL)
#define BSON_IS_STRING_TYPE(atype) ((atype) == BSON_STRING || (atype) == BSON_SYMBOL) 

EJDB_EXTERN_C_START

#define BSON_OK 0
#define BSON_ERROR -1

//Maximum field path length allocated on stack
#define BSON_MAX_FPATH_LEN (255)

enum bson_error_t {
    BSON_SIZE_OVERFLOW = 1 /**< Trying to create a BSON object larger than INT_MAX. */
};

enum bson_validity_t {
    BSON_VALID = 0, /**< BSON is valid and UTF-8 compliant. */
    BSON_NOT_UTF8 = (1 << 1), /**< A key or a string is not valid UTF-8. */
    BSON_FIELD_HAS_DOT = (1 << 2), /**< Warning: key contains '.' character. */
    BSON_FIELD_INIT_DOLLAR = (1 << 3), /**< Warning: key starts with '$' character. */
    BSON_ALREADY_FINISHED = (1 << 4), /**< Trying to modify a finished BSON object. */
    BSON_ERROR_ANY = (1 << 5), /**< Unspecified error */
    BSON_NOT_FINISHED = (1 << 6) /**< BSON object not finished */
};

enum bson_binary_subtype_t {
    BSON_BIN_BINARY = 0,
    BSON_BIN_FUNC = 1,
    BSON_BIN_BINARY_OLD = 2, /**< Deprecated */
    BSON_BIN_UUID_OLD = 3, /**< Deprecated */
    BSON_BIN_UUID = 4,
    BSON_BIN_MD5 = 5,
    BSON_BIN_USER = 128
};

enum bson_flags_t {
    BSON_FLAG_QUERY_MODE = 1,
    BSON_FLAG_STACK_ALLOCATED = 1 << 1 /**< If it set BSON data is allocated on stack and realloc should deal with this case */
};

typedef enum {
    BSON_EOO = 0,
    BSON_DOUBLE = 1,
    BSON_STRING = 2,
    BSON_OBJECT = 3,
    BSON_ARRAY = 4,
    BSON_BINDATA = 5,
    BSON_UNDEFINED = 6, /**< Deprecated */
    BSON_OID = 7,
    BSON_BOOL = 8,
    BSON_DATE = 9,
    BSON_NULL = 10,
    BSON_REGEX = 11,
    BSON_DBREF = 12, /**< Deprecated. */
    BSON_CODE = 13,
    BSON_SYMBOL = 14, /**< Deprecated. */
    BSON_CODEWSCOPE = 15,
    BSON_INT = 16,
    BSON_TIMESTAMP = 17,
    BSON_LONG = 18
} bson_type;

typedef int bson_bool_t;

typedef struct {
    const char *cur;
    bson_bool_t first;
} bson_iterator;

typedef struct {
    char *data; /**< Pointer to a block of data in this BSON object. */
    char *cur; /**< Pointer to the current position. */
    int dataSize; /**< The number of bytes allocated to char *data. */
    bson_bool_t finished; /**< When finished, the BSON object can no longer be modified. */
    int stack[32]; /**< A stack used to keep track of nested BSON elements. */
    int stackPos; /**< Index of current stack position. */
    int err; /**< Bitfield representing errors or warnings on this buffer */
    char *errstr; /**< A string representation of the most recent error or warning. */
    int flags;
} bson;

#pragma pack(1)

typedef union {
    char bytes[12];
    int ints[3];
} bson_oid_t;
#pragma pack()

typedef int64_t bson_date_t; /* milliseconds since epoch UTC */

typedef struct {
    int i; /* increment */
    int t; /* time in seconds */
} bson_timestamp_t;


EJDB_EXPORT const char* bson_first_errormsg(bson *bson);


#define BSON_ITERATOR_FROM_BUFFER(_bs_I, _bs_B) \
    (_bs_I)->cur = ((char*) (_bs_B)) + 4;       \
    (_bs_I)->first = 1;

#define BSON_ITERATOR_SUBITERATOR(_bs_I, _bs_S) \
    BSON_ITERATOR_FROM_BUFFER((_bs_S), bson_iterator_value(_bs_I))

#define BSON_ITERATOR_TYPE(_bs_I) \
    ((bson_type) (_bs_I)->cur[0])

#define BSON_ITERATOR_KEY(_bs_I) \
    ((_bs_I)->cur + 1)

#define BSON_ITERATOR_INIT(_bs_I, _bs) \
    (_bs_I)->cur = (_bs)->data + 4; \
    (_bs_I)->first = 1;

#define BSON_ITERATOR_CLONE(_bs_I_S, _bs_I_T) \
    (_bs_I_T)->cur = (_bs_I_S)->cur; \
    (_bs_I_T)->first = (_bs_I_S)->first;

/* --------------------------------
   READING
   ------------------------------ */

EJDB_EXPORT bson* bson_create(void);
EJDB_EXPORT void bson_dispose(bson* b);

/**
 * Size of a BSON object.
 *
 * @param b the BSON object.
 *
 * @return the size.
 */
EJDB_EXPORT int bson_size(const bson *b);
EJDB_EXPORT int bson_size2(const void *bsdata);
EJDB_EXPORT int bson_buffer_size(const bson *b);

/**
 * Return a pointer to the raw buffer stored by this bson object.
 *
 * @param b a BSON object
 */
EJDB_EXPORT const char *bson_data(const bson *b);
EJDB_EXPORT const char* bson_data2(const bson *b, int *bsize);

/**
 * Print a string representation of a BSON object.
 *
 * @param bson the raw data to print.
 * @param depth the depth to recurse the object.x
 */
EJDB_EXPORT void bson_print_raw(const char *bson, int depth);

/**
 * Advance a bson_iterator to the named field.
 *
 * @param it the bson_iterator to use.
 * @param obj the BSON object to use.
 * @param name the name of the field to find.
 *
 * @return the type of the found object or BSON_EOO if it is not found.
 */
EJDB_EXPORT bson_type bson_find(bson_iterator *it, const bson *obj, const char *name);

EJDB_EXPORT bson_type bson_find_from_buffer(bson_iterator *it, const char *buffer, const char *name);

typedef struct { /**< Find field path context */
    const char* fpath;
    int fplen;
    bson_iterator *input;
    int stopos;
    bool stopnestedarr;
    int mpos; /**< Array index of the first matched array field */
    int dpos; /**< Position of `$` in array projection fieldpath. */
} FFPCTX;


/**
 * Advance specified iterator 'it' to field value pointing by 'fieldpath'/
 * Field path string format: 'field1.nestedfield1.nestedfield.2'.
 * If specified field not found BSON_EOO will be returned.
 *
 * @param fieldpath Path specification to the BSON field.
 * @param it the bson_iterator to use.
 * @return the type of the found object or BSON_EOO if it is not found.
 */
EJDB_EXPORT bson_type bson_find_fieldpath_value(const char *fieldpath, bson_iterator *it);
EJDB_EXPORT bson_type bson_find_fieldpath_value2(const char *fpath, int fplen, bson_iterator *it);
EJDB_EXPORT bson_type bson_find_fieldpath_value3(FFPCTX* ffctx);

/**
 * BSON object visitor
 * @param it bson iterator to traverse
 * @param visitor Visitor function
 * @param op Opaque data for visitor
 */
typedef enum {
    BSON_TRAVERSE_ARRAYS_EXCLUDED = 1,
    BSON_TRAVERSE_OBJECTS_EXCLUDED = 1 << 1
} bson_traverse_flags_t;

typedef enum {
    BSON_VCMD_OK = 0,
    BSON_VCMD_TERMINATE = 1,
    BSON_VCMD_SKIP_NESTED = 1 << 1,
    BSON_VCMD_SKIP_AFTER = 1 << 2
} bson_visitor_cmd_t;

typedef bson_visitor_cmd_t(*BSONVISITOR)(const char *ipath, int ipathlen, 
                                         const char *key, int keylen, 
                                         const bson_iterator *it, 
                                         bool after, void *op);

EJDB_EXPORT void bson_visit_fields(bson_iterator *it, 
                                   bson_traverse_flags_t flags, 
                                   BSONVISITOR visitor, void *op);


EJDB_EXPORT bson_iterator* bson_iterator_create(void);
EJDB_EXPORT void bson_iterator_dispose(bson_iterator*);
/**
 * Initialize a bson_iterator.
 *
 * @param i the bson_iterator to initialize.
 * @param bson the BSON object to associate with the iterator.
 */
EJDB_EXPORT void bson_iterator_init(bson_iterator *i, const bson *b);

/**
 * Initialize a bson iterator from a const char* buffer. Note
 * that this is mostly used internally.
 *
 * @param i the bson_iterator to initialize.
 * @param buffer the buffer to point to.
 */
EJDB_EXPORT void bson_iterator_from_buffer(bson_iterator *i, const char *buffer);

/* more returns true for eoo. best to loop with bson_iterator_next(&it) */
/**
 * Check to see if the bson_iterator has more data.
 *
 * @param i the iterator.
 *
 * @return  returns true if there is more data.
 */
EJDB_EXPORT bson_bool_t bson_iterator_more(const bson_iterator *i);

/**
 * Point the iterator at the next BSON object.
 *
 * @param i the bson_iterator.
 *
 * @return the type of the next BSON object.
 */
EJDB_EXPORT bson_type bson_iterator_next(bson_iterator *i);

/**
 * Get the type of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the type of the current BSON object.
 */
EJDB_EXPORT bson_type bson_iterator_type(const bson_iterator *i);

/**
 * Get the key of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the key of the current BSON object.
 */
EJDB_EXPORT const char *bson_iterator_key(const bson_iterator *i);

/**
 * Get the value of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
EJDB_EXPORT const char *bson_iterator_value(const bson_iterator *i);

/* these convert to the right type (return 0 if non-numeric) */
/**
 * Get the double value of the BSON object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
EJDB_EXPORT double bson_iterator_double(const bson_iterator *i);

/**
 * Get the int value of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
EJDB_EXPORT int bson_iterator_int(const bson_iterator *i);

/**
 * Get the long value of the BSON object currently pointed to by the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
EJDB_EXPORT int64_t bson_iterator_long(const bson_iterator *i);

/* return the bson timestamp as a whole or in parts */
/**
 * Get the timestamp value of the BSON object currently pointed to by
 * the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
EJDB_EXPORT bson_timestamp_t bson_iterator_timestamp(const bson_iterator *i);
EJDB_EXPORT int bson_iterator_timestamp_time(const bson_iterator *i);
EJDB_EXPORT int bson_iterator_timestamp_increment(const bson_iterator *i);

/**
 * Get the boolean value of the BSON object currently pointed to by
 * the iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
/* false: boolean false, 0 in any type, or null */
/* true: anything else (even empty strings and objects) */
EJDB_EXPORT bson_bool_t bson_iterator_bool(const bson_iterator *i);

/**
 * Get the double value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
/* these assume you are using the right type */
EJDB_EXPORT double bson_iterator_double_raw(const bson_iterator *i);

/**
 * Get the int value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
EJDB_EXPORT int bson_iterator_int_raw(const bson_iterator *i);

/**
 * Get the long value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
EJDB_EXPORT int64_t bson_iterator_long_raw(const bson_iterator *i);

/**
 * Get the bson_bool_t value of the BSON object currently pointed to by the
 * iterator. Assumes the correct type is used.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
EJDB_EXPORT bson_bool_t bson_iterator_bool_raw(const bson_iterator *i);

/**
 * Get the bson_oid_t value of the BSON object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON object.
 */
EJDB_EXPORT bson_oid_t *bson_iterator_oid(const bson_iterator *i);

/**
 * Get the string value of the BSON object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return  the value of the current BSON object.
 */
/* these can also be used with bson_code and bson_symbol*/
EJDB_EXPORT const char *bson_iterator_string(const bson_iterator *i);

/**
 * Get the string length of the BSON object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the length of the current BSON object.
 */
EJDB_EXPORT int bson_iterator_string_len(const bson_iterator *i);

/**
 * Get the code value of the BSON object currently pointed to by the
 * iterator. Works with bson_code, bson_codewscope, and BSON_STRING
 * returns NULL for everything else.
 *
 * @param i the bson_iterator
 *
 * @return the code value of the current BSON object.
 */
/* works with bson_code, bson_codewscope, and BSON_STRING */
/* returns NULL for everything else */
EJDB_EXPORT const char *bson_iterator_code(const bson_iterator *i);

/**
 * Calls bson_empty on scope if not a bson_codewscope
 *
 * @param i the bson_iterator.
 * @param scope the bson scope.
 */
/* calls bson_empty on scope if not a bson_codewscope */
EJDB_EXPORT void bson_iterator_code_scope(const bson_iterator *i, bson *scope);

/**
 * Get the date value of the BSON object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the date value of the current BSON object.
 */
/* both of these only work with bson_date */
EJDB_EXPORT bson_date_t bson_iterator_date(const bson_iterator *i);

/**
 * Get the time value of the BSON object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the time value of the current BSON object.
 */
EJDB_EXPORT time_t bson_iterator_time_t(const bson_iterator *i);

/**
 * Get the length of the BSON binary object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the length of the current BSON binary object.
 */
EJDB_EXPORT int bson_iterator_bin_len(const bson_iterator *i);

/**
 * Get the type of the BSON binary object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the type of the current BSON binary object.
 */
EJDB_EXPORT char bson_iterator_bin_type(const bson_iterator *i);

/**
 * Get the value of the BSON binary object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON binary object.
 */
EJDB_EXPORT const char *bson_iterator_bin_data(const bson_iterator *i);

/**
 * Get the value of the BSON regex object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator
 *
 * @return the value of the current BSON regex object.
 */
EJDB_EXPORT const char *bson_iterator_regex(const bson_iterator *i);

/**
 * Get the options of the BSON regex object currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator.
 *
 * @return the options of the current BSON regex object.
 */
EJDB_EXPORT const char *bson_iterator_regex_opts(const bson_iterator *i);

/* these work with BSON_OBJECT and BSON_ARRAY */
/**
 * Get the BSON subobject currently pointed to by the
 * iterator.
 *
 * @param i the bson_iterator.
 * @param sub the BSON subobject destination.
 */
EJDB_EXPORT void bson_iterator_subobject(const bson_iterator *i, bson *sub);

/**
 * Get a bson_iterator that on the BSON subobject.
 *
 * @param i the bson_iterator.
 * @param sub the iterator to point at the BSON subobject.
 */
EJDB_EXPORT void bson_iterator_subiterator(const bson_iterator *i, bson_iterator *sub);

/* str must be at least 24 hex chars + null byte */
/**
 * Create a bson_oid_t from a string.
 *
 * @param oid the bson_oid_t destination.
 * @param str a null terminated string comprised of at least 24 hex chars.
 */
EJDB_EXPORT void bson_oid_from_string(bson_oid_t *oid, const char *str);

/**
 * Create a string representation of the bson_oid_t.
 *
 * @param oid the bson_oid_t source.
 * @param str the string representation destination.
 */
EJDB_EXPORT void bson_oid_to_string(const bson_oid_t *oid, char *str);

/**
 * Create a bson_oid object.
 *
 * @param oid the destination for the newly created bson_oid_t.
 */
EJDB_EXPORT void bson_oid_gen(bson_oid_t *oid);

/**
 * Set a function to be used to generate the second four bytes
 * of an object id.
 *
 * @param func a pointer to a function that returns an int.
 */
EJDB_EXPORT void bson_set_oid_fuzz(int ( *func)(void));

/**
 * Set a function to be used to generate the incrementing part
 * of an object id (last four bytes). If you need thread-safety
 * in generating object ids, you should set this function.
 *
 * @param func a pointer to a function that returns an int.
 */
EJDB_EXPORT void bson_set_oid_inc(int ( *func)(void));

/**
 * Get the time a bson_oid_t was created.
 *
 * @param oid the bson_oid_t.
 */
EJDB_EXPORT time_t bson_oid_generated_time(bson_oid_t *oid); /* Gives the time the OID was created */

/* ----------------------------
   BUILDING
   ------------------------------ */


EJDB_EXPORT void bson_append(bson *b, const void *data, int len);

/**
 *  Initialize a new bson object. If not created
 *  with bson_new, you must initialize each new bson
 *  object using this function.
 *
 *  @note When finished, you must pass the bson object to
 *      bson_del( ).
 */
EJDB_EXPORT void bson_init(bson *b);


/**
 * Intialize a new bson object. In query contruction mode allowing
 * dot and dollar chars in field names.
 * @param b
 */
EJDB_EXPORT void bson_init_as_query(bson *b);

/**
 * Initialize a BSON object, and point its data
 * pointer to the provided char*.
 *
 * @param b the BSON object to initialize.
 * @param data the raw BSON data.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_init_data(bson *b, char *data);
EJDB_EXPORT int bson_init_finished_data(bson *b, const char *data);

/**
 * Initialize a BSON object and set its buffer to the given size.
 *
 * @param b the BSON object to initialize.
 * @param size the initial size of the buffer.
 */
EJDB_EXPORT void bson_init_size(bson *b, int size);

EJDB_EXPORT void bson_init_on_stack(bson *b, char *bstack, int mincapacity, int maxonstack);

/**
 * Initialize a BSON object. If not created with bson_new, you must
 * initialize each new bson object using this function. Report
 * problems with memory allocation.
 *
 * @param b the BSON object to initialize.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_safe_init(bson *b);

/**
 * Initialize a BSON object and set its buffer to the given size.
 * Report problems with memory allocation.
 *
 * @param b the BSON object to initialize.
 * @param size the inintial size of the buffer.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_safe_init_size(bson *b, int size);

/**
 * Intialize a BSON object. In query contruction mode allowing dot and
 * dollar chars in field names. Report problems with memory
 * allocation.
 *
 * @param b
 *
 * @return BSON_OK or BSON_ERROR
 */
EJDB_EXPORT int bson_safe_init_as_query(bson *b);

/**
 * Grow a bson object.
 *
 * @param b the bson to grow.
 * @param bytesNeeded the additional number of bytes needed.
 *
 * @return BSON_OK or BSON_ERROR with the bson error object set.
 *   Exits if allocation fails.
 */
EJDB_EXPORT int bson_ensure_space(bson *b, const int bytesNeeded);

/**
 * Finalize a bson object.
 *
 * @param b the bson object to finalize.
 *
 * @return the standard error code. To deallocate memory,
 *   call bson_del on the bson object.
 */
EJDB_EXPORT int bson_finish(bson *b);

/**
 * Destroy a bson object.
 * Clears bson object and frees internal memory buffers held by bson
 * object BUT does not delete bson object itself
 * @param b the bson object to destroy.
 */
EJDB_EXPORT void bson_destroy(bson *b);

/**
 * The bson_del() performs bson_destroy() then frees bson object itself.
 * @param b
 */
EJDB_EXPORT void bson_del(bson *b);

EJDB_EXPORT void bson_reset(bson *b);

/**
 * Returns a pointer to a static empty BSON object.
 *
 * @param obj the BSON object to initialize.
 *
 * @return the empty initialized BSON object.
 */
/* returns pointer to static empty bson object */
EJDB_EXPORT bson *bson_empty(bson *obj);

/**
 * Make a complete copy of the a BSON object.
 * The source bson object must be in a finished
 * state; otherwise, the copy will fail.
 *
 * @param out the copy destination BSON object.
 * @param in the copy source BSON object.
 */
EJDB_EXPORT int bson_copy(bson *out, const bson *in); /* puts data in new buffer. NOOP if out==NULL */

/**
 * Append a previously created bson_oid_t to a bson object.
 *
 * @param b the bson to append to.
 * @param name the key for the bson_oid_t.
 * @param oid the bson_oid_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_oid(bson *b, const char *name, const bson_oid_t *oid);

/**
 * Append a bson_oid_t to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the bson_oid_t.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_new_oid(bson *b, const char *name);

/**
 * Append an int to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the int.
 * @param i the int to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_int(bson *b, const char *name, const int i);

/**
 * Append an long to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the long.
 * @param i the long to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_long(bson *b, const char *name, const int64_t i);

/**
 * Append an double to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the double.
 * @param d the double to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_double(bson *b, const char *name, const double d);

/**
 * Append a string to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the string.
 * @param str the string to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_string(bson *b, const char *name, const char *str);

/**
 * Append len bytes of a string to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the string.
 * @param str the string to append.
 * @param len the number of bytes from str to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_string_n(bson *b, const char *name, const char *str, int len);

/**
 * Append a symbol to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the symbol.
 * @param str the symbol to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_symbol(bson *b, const char *name, const char *str);

/**
 * Append len bytes of a symbol to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the symbol.
 * @param str the symbol to append.
 * @param len the number of bytes from str to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_symbol_n(bson *b, const char *name, const char *str, int len);

/**
 * Append code to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the code.
 * @param str the code to append.
 * @param len the number of bytes from str to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_code(bson *b, const char *name, const char *str);

/**
 * Append len bytes of code to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the code.
 * @param str the code to append.
 * @param len the number of bytes from str to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_code_n(bson *b, const char *name, const char *str, int len);

/**
 * Append code to a bson with scope.
 *
 * @param b the bson to append to.
 * @param name the key for the code.
 * @param str the string to append.
 * @param scope a BSON object containing the scope.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_code_w_scope(bson *b, const char *name, const char *code, const bson *scope);

/**
 * Append len bytes of code to a bson with scope.
 *
 * @param b the bson to append to.
 * @param name the key for the code.
 * @param str the string to append.
 * @param len the number of bytes from str to append.
 * @param scope a BSON object containing the scope.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_code_w_scope_n(bson *b, const char *name, const char *code, int size, const bson *scope);

/**
 * Append binary data to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the data.
 * @param type the binary data type.
 * @param str the binary data.
 * @param len the length of the data.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_binary(bson *b, const char *name, char type, const char *str, int len);

/**
 * Append a bson_bool_t to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the boolean value.
 * @param v the bson_bool_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_bool(bson *b, const char *name, const bson_bool_t v);

/**
 * Append a null value to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the null value.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_null(bson *b, const char *name);

/**
 * Append an undefined value to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the undefined value.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_undefined(bson *b, const char *name);

/**
 * Append a regex value to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the regex value.
 * @param pattern the regex pattern to append.
 * @param the regex options.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_regex(bson *b, const char *name, const char *pattern, const char *opts);

/**
 * Append bson data to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the bson data.
 * @param bson the bson object to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_bson(bson *b, const char *name, const bson *bson);

/**
 * Append a BSON element to a bson from the current point of an iterator.
 *
 * @param b the bson to append to.
 * @param name_or_null the key for the BSON element, or NULL.
 * @param elem the bson_iterator.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_element(bson *b, const char *name_or_null, const bson_iterator *elem);

/**
 * Append a bson_timestamp_t value to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the timestampe value.
 * @param ts the bson_timestamp_t value to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_timestamp(bson *b, const char *name, bson_timestamp_t *ts);
EJDB_EXPORT int bson_append_timestamp2(bson *b, const char *name, int time, int increment);

/* these both append a bson_date */
/**
 * Append a bson_date_t value to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the date value.
 * @param millis the bson_date_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_date(bson *b, const char *name, bson_date_t millis);

/**
 * Append a time_t value to a bson.
 *
 * @param b the bson to append to.
 * @param name the key for the date value.
 * @param secs the time_t to append.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_time_t(bson *b, const char *name, time_t secs);

/**
 * Start appending a new object to a bson.
 *
 * @param b the bson to append to.
 * @param name the name of the new object.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_start_object(bson *b, const char *name);
EJDB_EXPORT int bson_append_start_object2(bson *b, const char *name, int namelen);

/**
 * Start appending a new array to a bson.
 *
 * @param b the bson to append to.
 * @param name the name of the new array.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_start_array(bson *b, const char *name);
EJDB_EXPORT int bson_append_start_array2(bson *b, const char *name, int namelen);

/**
 * Finish appending a new object or array to a bson.
 *
 * @param b the bson to append to.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_finish_object(bson *b);

/**
 * Finish appending a new object or array to a bson. This
 * is simply an alias for bson_append_finish_object.
 *
 * @param b the bson to append to.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_finish_array(bson *b);

EJDB_EXPORT int bson_merge_recursive(const bson *b1, const bson *b2, bson_bool_t overwrite, bson *out);

/**
 * Check duplicate keys
 * @return true if bson contains duplicate keys
 */
EJDB_EXPORT bson_bool_t bson_check_duplicate_keys(const bson *bs);

/**
 * Remove duplicate keys from bson:
 *  - merge objects and arrays with same key:
 *   { a : { b : "value 1" }, a : { c : "value 2" } } -> { a : { b : "value 1", c : "value 2" } }
 *  - keep last value for non object and non array values
 *   { a : "value 1", a : "value 2" } -> { a : "value 2" }
 *
 * Example:
 * {
 *   a : {
 *     b : 1,
 *     c : "c"
 *   },
 *   b : NULL,
 *   c : [
 *     {
 *       a : 1,
 *       b : 2,
 *       a : 0
 *     },
 *     {
 *       a : 0,
 *       b : 1,
 *       c : 3
 *     }
 *   ],
 *   a : {
 *     d : 0,
 *     c : 1
 *   }
 * }
 *
 * =>
 *
 * {
 *   a : {
 *     b : 1,
 *     c : 1,
 *     d : 0
 *   },
 *   b : NULL,
 *   c : [
 *     {
 *       a : 0,
 *       b : 2
 *     },
 *     {
 *       a : 0,
         b : 1,
         c : 3
 *     }
 *   ]
 * }
 */
EJDB_EXPORT void bson_fix_duplicate_keys(const bson *bsi, bson *bso);

EJDB_EXPORT void bson_numstr(char *str, int64_t i);
EJDB_EXPORT int bson_numstrn(char *str, int maxbuf, int64_t i);

//void bson_incnumstr(char *str);

/* Error handling and standard library function over-riding. */
/* -------------------------------------------------------- */

/* bson_err_handlers shouldn't return!!! */
typedef void( *bson_err_handler)(const char *errmsg);
typedef int (*bson_printf_func)(const char *, ...);

EJDB_EXPORT void bson_free(void *ptr);

/**
 * Allocates memory and checks return value, exiting fatally if malloc() fails.
 *
 * @param size bytes to allocate.
 *
 * @return a pointer to the allocated memory.
 *
 * @sa malloc(3)
 */
void *bson_malloc(int size);

/**
 * Changes the size of allocated memory and checks return value,
 * exiting fatally if realloc() fails.
 *
 * @param ptr pointer to the space to reallocate.
 * @param size bytes to allocate.
 *
 * @return a pointer to the allocated memory.
 *
 * @sa realloc()
 */
void *bson_realloc(void *ptr, int size);

/**
 * Set a function for error handling.
 *
 * @param func a bson_err_handler function.
 *
 * @return the old error handling function, or NULL.
 */
EJDB_EXPORT bson_err_handler set_bson_err_handler(bson_err_handler func);

/* does nothing if ok != 0 */
/**
 * Exit fatally.
 *
 * @param ok exits if ok is equal to 0.
 */
void bson_fatal(int ok);

/**
 * Exit fatally with an error message.
 *
 * @param ok exits if ok is equal to 0.
 * @param msg prints to stderr before exiting.
 */
void bson_fatal_msg(int ok, const char *msg);

/**
 * Invoke the error handler, but do not exit.
 *
 * @param b the buffer object.
 */
void bson_builder_error(bson *b);

/**
 * Cast an int64_t to double. This is necessary for embedding in
 * certain environments.
 *
 */
EJDB_EXPORT double bson_int64_to_double(int64_t i64);
EJDB_EXPORT void bson_swap_endian32(void *outp, const void *inp);
EJDB_EXPORT void bson_swap_endian64(void *outp, const void *inp);


/**
 * Append current field from iterator into bson object.
 *
 * @param from
 * @param into
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_field_from_iterator(const bson_iterator *from, bson *into);

/**
 * Append current field value from iterator into bson object under specified string key.
 *
 * @param key Key name.
 * @param from Source iterator value
 * @param into Target bsob object
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_append_field_from_iterator2(const char *key, const bson_iterator *from, bson *into);
EJDB_EXPORT int bson_append_object_from_iterator(const char *key, bson_iterator *from, bson *into);
EJDB_EXPORT int bson_append_array_from_iterator(const char *key, bson_iterator *from, bson *into);


/**
 * Merge bson  `b2` into `b1` saving result the 'out' object.
 * `b1` & `b2` bson must be finished BSONS.
 * Resulting 'out' bson must be allocated and not finished.
 *
 * Nested object skipped and usupported.
 *
 * @param b1 BSON to to be merged in `out`
 * @param b2 Second BSON to to be merged in `out`
 * @param overwrite if `true` all `b1` fields will be overwriten by corresponding `b2` fields
 * @param out
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_merge(const bson *b1, const bson *b2, bson_bool_t overwrite, bson *out);
EJDB_EXPORT int bson_merge2(const void *b1data, const void *b2data, bson_bool_t overwrite, bson *out);

/**
 * Recursive merge bson  `b2` into `b1` saving result the 'out' object.
 * `b1` & `b2` bson must be finished BSONS.
 * Resulting 'out' bson must be allocated and not finished.
 *
 * NOTE. Arrays with same paths joined.
 *
 * @param b1 BSON to to be merged in `out`
 * @param b2 Second BSON to to be merged in `out`
 * @param overwrite if `true` all `b1` fields will be overwriten by corresponding `b2` fields
 * @param out
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_merge_recursive(const bson *b1, const bson *b2, bson_bool_t overwrite, bson *out);
EJDB_EXPORT int bson_merge_recursive2(const void *b1data, const void *b2data, bson_bool_t overwrite, bson *out);

/**
 * Merge bsons.
 * `bsdata2` may contain field path keys (eg: 'foo.bar').
 * 
 * @param bsdata1 BSON data to to be merged in `out`
 * @param bsdata2 Second BSON data to to be merged in `out`
 * @param out Resulting `out` bson must be allocated and not finished.
 *
 * @return BSON_OK or BSON_ERROR.
 */
EJDB_EXPORT int bson_merge_fieldpaths(const void *bsdata1, const void *bsdata2, bson *out);

EJDB_EXPORT int bson_inplace_set_bool(bson_iterator *pos, bson_bool_t val);
EJDB_EXPORT int bson_inplace_set_long(bson_iterator *pos, int64_t val);
EJDB_EXPORT int bson_inplace_set_double(bson_iterator *pos, double val);

typedef struct {
    TCMAP *ifields;     //Required Map of fieldpaths. Map values are a simple boolean bufs.
    bool imode;         //Required If true fpaths will be included. Otherwise fpaths will be excluded from bson.
    const void *bsbuf;  //Required BSON buffer to process.
    bson *bsout;        //Required Allocated output not finished bson* object.
    TCMAP *fkfields;    //Optional: Map (fpath => bson key) used to force specific bson keys for selected fpaths.
    int matched;        //Output: number of matched fieldpaths
    bson *collector;    //Optional: Collector for excluded data (fieldpath -> excluded value)
} BSONSTRIPCTX;

/**
 * Include or exclude fpaths in the specified BSON and put resulting data into `bsout`.
 * On completion it finishes `bsout` object.
 *
 * @param ifields Map of fieldpaths. Map values are a simple boolean bufs.
 * @param imode If true fpaths will be included. Otherwise fpaths will be excluded from bson.
 * @param bsbuf BSON buffer to process.
 * @param bsout Allocated and not finished output bson* object
 * @param matched[out] Number of matched include/exclude fieldpaths.
 * @return BSON error code
 */
EJDB_EXPORT int bson_strip(TCMAP *ifields, bool imode, const void *bsbuf, bson *bsout, int *matched);
EJDB_EXPORT int bson_strip2(BSONSTRIPCTX *sctx);

/**
 * @brief Rename a fields specified by `fields` rename mapping.
 * 
 * This operation unsets both all and new fieldpaths and then sets 
 * new fieldpath values. 
 * 
 * @param fields Rename mapping old `fieldpath` to new `fieldpath`.
 * @param bsbuf BSON buffer to process.
 * @param bsout Allocated and not finished output bson* object
 * @param rencnt A number of fieldpaths actually renamed.
 */
EJDB_EXPORT int bson_rename(TCMAP *fields, const void *bsbuf, bson *bsout, int *rencnt);


/**
 * Compares field path primitive values of two BSONs
 * @param bsdata1 BSON raw data
 * @param bsdata2 BSON raw data
 * @param fpath Field path to the field
 * @param fplen Length of fpath value
 */
EJDB_EXPORT int bson_compare(const void *bsdata1, const void *bsdata2, const char* fpath, int fplen);
EJDB_EXPORT int bson_compare_fpaths(const void *bsdata1, const void *bsdata2, 
                                    const char *fpath1, int fplen1, 
                                    const char *fpath2, int fplen2);
EJDB_EXPORT int bson_compare_it_current(const bson_iterator *it1, const bson_iterator *it2);
EJDB_EXPORT int bson_compare_string(const char* cv, const void *bsdata, const char *fpath);
EJDB_EXPORT int bson_compare_long(const int64_t cv, const void *bsdata, const char *fpath);
EJDB_EXPORT int bson_compare_double(double cv, const void *bsdata, const char *fpath);
EJDB_EXPORT int bson_compare_bool(bson_bool_t cv, const void *bsdata, const char *fpath);


/**
 * Duplicates BSON object.
 * @param src BSON
 * @return Finished copy of src
 */
EJDB_EXPORT bson* bson_dup(const bson *src);


EJDB_EXPORT bson* bson_create_from_iterator(bson_iterator *from);
EJDB_EXPORT bson* bson_create_from_buffer(const void *buf, int bufsz);
EJDB_EXPORT bson* bson_create_from_buffer2(bson *bs, const void *buf, int bufsz);
EJDB_EXPORT void bson_init_with_data(bson *bs, const void *bsdata);

typedef enum {
    BSON_MERGE_ARRAY_ADDSET = 0,
    BSON_MERGE_ARRAY_PULL = 1,
    BSON_MERGE_ARRAY_PUSH = 2
} bson_merge_array_mode;

EJDB_EXPORT int bson_merge_arrays(const void *mbuf, const void *inbuf, 
                                  bson_merge_array_mode mode, 
                                  bool expandall, bson *bsout);
                                  
EJDB_EXPORT bool bson_find_unmerged_arrays(const void *mbuf, const void *inbuf);
EJDB_EXPORT bool bson_find_merged_arrays(const void *mbuf, const void *inbuf, bool expandall);

/**
 * Convert BSON into JSON buffer.
 * 
 * A caller should free an allocated `*buf` 
 * if value of `*buf` is not `NULL` after function completion. 
 * 
 * @param src BSON data
 * @param buf Allocated buffer with resulting JSON data
 * @param sp JSON data length will be stored into
 * @return BSON_OK or BSON_ERROR
 */
EJDB_EXPORT int bson2json(const char *bsdata, char **buf, int *sp);

/**
 * Convert JSON into BSON object.
 * @param jsonstr NULL terminated JSON string
 * @return Allocated BSON object filled with given JSON data or NULL on error
 */
EJDB_EXPORT bson* json2bson(const char *jsonstr);


/**
 * @brief Validate bson object.
 * Set the bs->err bitmask as validation result.
 * 
 * @param bs Bson object to be validated.
 * @param checkdots Check what keys contain dot(.) characters
 * @param checkdollar Check what keys contain dollar($) characters
 * @return BSON_OK if all checks passed otherwise return BSON_ERROR
 */
EJDB_EXPORT int bson_validate(bson *bs, bool checkdots, bool checkdollar);


EJDB_EXTERN_C_END
#endif
