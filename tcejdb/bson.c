/* bson.c */

/*    Copyright 2009, 2010 10gen Inc.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include "bson.h"
#include "encoding.h"
#include "myconf.h"

const int initialBufferSize = 128;

#define        MIN(a,b) (((a)<(b))?(a):(b))
#define        MAX(a,b) (((a)>(b))?(a):(b))

/* only need one of these */
static const int zero = 0;

/* Custom standard function pointers. */
void *(*bson_malloc_func)(size_t) = MYMALLOC;
void *(*bson_realloc_func)(void *, size_t) = MYREALLOC;
void ( *bson_free_func)(void *) = MYFREE;
#ifdef R_SAFETY_NET
bson_printf_func bson_printf;
#else
bson_printf_func bson_printf = printf;
#endif
bson_fprintf_func bson_fprintf = fprintf;
bson_sprintf_func bson_sprintf = sprintf;

static int _bson_errprintf(const char *, ...);
bson_printf_func bson_errprintf = _bson_errprintf;

/* ObjectId fuzz functions. */
static int ( *oid_fuzz_func)(void) = NULL;
static int ( *oid_inc_func)(void) = NULL;


EJDB_EXPORT const char* bson_first_errormsg(bson *bson) {
    if (bson->errstr) {
        return bson->errstr;
    }
    if (bson->err & BSON_FIELD_HAS_DOT) {
        return "BSON key contains '.' character";
    } else if (bson->err & BSON_FIELD_INIT_DOLLAR) {
        return "BSON key starts with '$' character";
    } else if (bson->err & BSON_ALREADY_FINISHED) {
        return "Trying to modify a finished BSON object";
    } else if (bson->err & BSON_NOT_UTF8) {
        return "A key or a string is not valid UTF-8";
    }
    return "Unspecified BSON error";
}


EJDB_EXPORT void bson_reset(bson *b) {
    b->finished = 0;
    b->stackPos = 0;
    b->err = 0;
    b->errstr = NULL;
    b->flags = 0;
}

static bson_bool_t bson_isnumstr(const char *str, int len);

/* ----------------------------
   READING
   ------------------------------ */

EJDB_EXPORT bson* bson_create(void) {
    return (bson*) bson_malloc(sizeof (bson));
}

EJDB_EXPORT void bson_dispose(bson* b) {
    bson_free(b);
}

EJDB_EXPORT bson *bson_empty(bson *obj) {
    static char *data = "\005\0\0\0\0";
    bson_init_data(obj, data);
    obj->finished = 1;
    obj->err = 0;
    obj->errstr = NULL;
    obj->stackPos = 0;
    obj->flags = 0;
    return obj;
}

EJDB_EXPORT int bson_copy(bson *out, const bson *in) {
    if (!out || !in) return BSON_ERROR;
    if (!in->finished) return BSON_ERROR;
    bson_init_size(out, bson_size(in));
    memcpy(out->data, in->data, bson_size(in));
    out->finished = 1;
    return BSON_OK;
}

int bson_init_data(bson *b, char *data) {
    b->data = data;
    return BSON_OK;
}

EJDB_EXPORT int bson_init_finished_data(bson *b, char *data) {
    bson_init_data(b, data);
    bson_reset(b);
    b->finished = 1;
    return BSON_OK;
}

EJDB_EXPORT int bson_size(const bson *b) {
    int i;
    if (!b || !b->data)
        return 0;
    bson_little_endian32(&i, b->data);
    return i;
}

EJDB_EXPORT int bson_size2(const void *bsdata) {
    int i;
    if (!bsdata)
        return 0;
    bson_little_endian32(&i, bsdata);
    return i;
}

EJDB_EXPORT int bson_buffer_size(const bson *b) {
    return (b->cur - b->data + 1);
}

EJDB_EXPORT const char *bson_data(const bson *b) {
    return (const char *) b->data;
}

EJDB_INLINE char hexbyte(char hex) {
    if (hex >= '0' && hex <= '9')
        return (hex - '0');
    else if (hex >= 'A' && hex <= 'F')
        return (hex - 'A' + 10);
    else if (hex >= 'a' && hex <= 'f')
        return (hex - 'a' + 10);
    else
        return 0x0;
}

EJDB_EXPORT void bson_oid_from_string(bson_oid_t *oid, const char *str) {
    int i;
    for (i = 0; i < 12; i++) {
        oid->bytes[i] = (hexbyte(str[2 * i]) << 4) | hexbyte(str[2 * i + 1]);
    }
}

EJDB_EXPORT void bson_oid_to_string(const bson_oid_t *oid, char *str) {
    static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    int i;
    for (i = 0; i < 12; i++) {
        str[2 * i] = hex[(oid->bytes[i] & 0xf0) >> 4];
        str[2 * i + 1] = hex[ oid->bytes[i] & 0x0f ];
    }
    str[24] = '\0';
}

EJDB_EXPORT void bson_set_oid_fuzz(int ( *func)(void)) {
    oid_fuzz_func = func;
}

EJDB_EXPORT void bson_set_oid_inc(int ( *func)(void)) {
    oid_inc_func = func;
}

EJDB_EXPORT void bson_oid_gen(bson_oid_t *oid) {
    static int incr = 0;
    static int fuzz = 0;
    int i;
    int t = time(NULL);

    if (oid_inc_func)
        i = oid_inc_func();
    else
        i = incr++;

    if (!fuzz) {
        if (oid_fuzz_func)
            fuzz = oid_fuzz_func();
        else {
            srand(t);
            fuzz = rand();
        }
    }

    bson_big_endian32(&oid->ints[0], &t);
    oid->ints[1] = fuzz;
    bson_big_endian32(&oid->ints[2], &i);
}

EJDB_EXPORT time_t bson_oid_generated_time(bson_oid_t *oid) {
    time_t out;
    bson_big_endian32(&out, &oid->ints[0]);

    return out;
}

EJDB_EXPORT void bson_print(FILE *f, const bson *b) {
    bson_print_raw(f, b->data, 0);
}

EJDB_EXPORT void bson_print_raw(FILE *f, const char *data, int depth) {
    bson_iterator i;
    const char *key;
    int temp;
    bson_timestamp_t ts;
    char oidhex[25];
    bson scope;
    bson_iterator_from_buffer(&i, data);

    while (bson_iterator_next(&i)) {
        bson_type t = bson_iterator_type(&i);
        if (t == 0)
            break;
        key = bson_iterator_key(&i);

        for (temp = 0; temp <= depth; temp++)
            bson_fprintf(f, "\t");
        bson_fprintf(f, "%s : %d \t ", key, t);
        switch (t) {
            case BSON_DOUBLE:
                bson_fprintf(f, "%f", bson_iterator_double(&i));
                break;
            case BSON_STRING:
                bson_fprintf(f, "%s", bson_iterator_string(&i));
                break;
            case BSON_SYMBOL:
                bson_fprintf(f, "SYMBOL: %s", bson_iterator_string(&i));
                break;
            case BSON_OID:
                bson_oid_to_string(bson_iterator_oid(&i), oidhex);
                bson_fprintf(f, "%s", oidhex);
                break;
            case BSON_BOOL:
                bson_fprintf(f, "%s", bson_iterator_bool(&i) ? "true" : "false");
                break;
            case BSON_DATE:
                bson_fprintf(f, "%ld", (long int) bson_iterator_date(&i));
                break;
            case BSON_BINDATA:
                bson_fprintf(f, "BSON_BINDATA");
                break;
            case BSON_UNDEFINED:
                bson_fprintf(f, "BSON_UNDEFINED");
                break;
            case BSON_NULL:
                bson_fprintf(f, "BSON_NULL");
                break;
            case BSON_REGEX:
                bson_fprintf(f, "BSON_REGEX: %s", bson_iterator_regex(&i));
                break;
            case BSON_CODE:
                bson_fprintf(f, "BSON_CODE: %s", bson_iterator_code(&i));
                break;
            case BSON_CODEWSCOPE:
                bson_fprintf(f, "BSON_CODE_W_SCOPE: %s", bson_iterator_code(&i));
                /* bson_init( &scope ); */ /* review - stepped on by bson_iterator_code_scope? */
                bson_iterator_code_scope(&i, &scope);
                bson_fprintf(f, "\n\t SCOPE: ");
                bson_print(f, &scope);
                /* bson_destroy( &scope ); */ /* review - causes free error */
                break;
            case BSON_INT:
                bson_fprintf(f, "%d", bson_iterator_int(&i));
                break;
            case BSON_LONG:
                bson_fprintf(f, "%lld", (uint64_t) bson_iterator_long(&i));
                break;
            case BSON_TIMESTAMP:
                ts = bson_iterator_timestamp(&i);
                bson_fprintf(f, "i: %d, t: %d", ts.i, ts.t);
                break;
            case BSON_OBJECT:
            case BSON_ARRAY:
                bson_fprintf(f, "\n");
                bson_print_raw(f, bson_iterator_value(&i), depth + 1);
                break;
            default:
                bson_errprintf("can't print type : %d\n", t);
        }
        bson_fprintf(f, "\n");
    }
}

/* ----------------------------
   ITERATOR
   ------------------------------ */

EJDB_EXPORT bson_iterator* bson_iterator_create(void) {
    return (bson_iterator*) malloc(sizeof ( bson_iterator));
}

EJDB_EXPORT void bson_iterator_dispose(bson_iterator* i) {
    free(i);
}

EJDB_EXPORT void bson_iterator_init(bson_iterator *i, const bson *b) {
    i->cur = b->data + 4;
    i->first = 1;
}

EJDB_EXPORT void bson_iterator_from_buffer(bson_iterator *i, const char *buffer) {
    i->cur = buffer + 4;
    i->first = 1;
}

EJDB_EXPORT bson_type bson_find(bson_iterator *it, const bson *obj, const char *name) {
    bson_iterator_init(it, (bson *) obj);
    while (bson_iterator_next(it)) {
        if (strcmp(name, bson_iterator_key(it)) == 0)
            break;
    }
    return bson_iterator_type(it);
}

EJDB_EXPORT bson_type bson_find_from_buffer(bson_iterator *it, const char *buffer, const char *name) {
    bson_iterator_from_buffer(it, buffer);
    while (bson_iterator_next(it)) {
        if (strcmp(name, bson_iterator_key(it)) == 0)
            break;
    }
    return bson_iterator_type(it);
}

static void bson_visit_fields_impl(bson_traverse_flags_t flags, char* pstack, int curr, bson_iterator *it, BSONVISITOR visitor, void *op) {
    int klen = 0;
    bson_type t;
    bson_visitor_cmd_t vcmd = 0;
    while (!(vcmd & BSON_VCMD_TERMINATE) && (t = bson_iterator_next(it)) != BSON_EOO) {
        const char* key = bson_iterator_key(it);
        klen = strlen(key);
        if (curr + klen > BSON_MAX_FPATH_LEN) {
            continue;
        }
        //PUSH
        if (curr > 0) { //add leading dot
            memset(pstack + curr, '.', 1);
            curr++;
        }
        memcpy(pstack + curr, key, klen);
        curr += klen;
        //Call visitor
        vcmd = visitor(pstack, curr, key, klen, it, false, op);
        if (vcmd & BSON_VCMD_TERMINATE) {
            break;
        }
        if (!(vcmd & BSON_VCMD_SKIP_NESTED) &&
                ((t == BSON_OBJECT && (flags & BSON_TRAVERSE_OBJECTS_EXCLUDED) == 0) ||
                (t == BSON_ARRAY && (flags & BSON_TRAVERSE_ARRAYS_EXCLUDED) == 0))
                ) {
            bson_iterator sit;
            bson_iterator_subiterator(it, &sit);
            bson_visit_fields_impl(flags, pstack, curr, &sit, visitor, op);
        }
        if (!(vcmd & BSON_VCMD_SKIP_AFTER)) {
            vcmd = visitor(pstack, curr, key, klen, it, true, op);
        }
        //POP
        curr -= klen;
        if (curr > 0) {
            curr--; //remove leading dot
        }
    }
}

EJDB_EXPORT void bson_visit_fields(bson_iterator *it, bson_traverse_flags_t flags, BSONVISITOR visitor, void *op) {
    char pstack[BSON_MAX_FPATH_LEN + 1];
    bson_visit_fields_impl(flags, pstack, 0, it, visitor, op);
}

static bson_type bson_find_fieldpath_value_impl(char* pstack, int curr, FFPCTX *ffpctx, bson_iterator *it) {
    int i;
    bson_type t;
    int klen = 0;
    int fplen = ffpctx->fplen;
    const char *fpath = ffpctx->fpath;
    while ((t = bson_iterator_next(it)) != BSON_EOO) {
        const char* key = bson_iterator_key(it);
        klen = strlen(key);
        if (curr + klen > fplen) {
            continue;
        }
        //PUSH
        if (curr > 0) { //add leading dot
            memset(pstack + curr, '.', 1);
            curr++;
        }
        memcpy(pstack + curr, key, klen);
        curr += klen;
        for (i = 0; i < curr && i < fplen && pstack[i] == fpath[i]; ++i);
        if (i == curr && i == fplen) { //Position matched with field path
            ffpctx->stopos = i;
            return t;
        }
        if (i == curr && i < fplen && (t == BSON_OBJECT || t == BSON_ARRAY)) { //Only prefix and we can go into nested objects
            if (ffpctx->stopnestedarr && t == BSON_ARRAY) {
                int p1 = curr;
                while (fpath[p1] == '.' && p1 < fplen) p1++;
                int p2 = p1;
                while (fpath[p2] != '.' && fpath[p2] > '\0' && p2 < fplen) p2++;
                if (!bson_isnumstr(fpath + p1, p2 - p1)) { //next fpath sections is not an array index
                    ffpctx->stopos = i;
                    return t;
                }
            }
            bson_iterator sit;
            bson_iterator_subiterator(it, &sit);
            bson_type st = bson_find_fieldpath_value_impl(pstack, curr, ffpctx, &sit);
            if (st != BSON_EOO) { //Found in nested
                *it = sit;
                return st;
            }
        }
        //POP
        curr -= klen;
        if (curr > 0) {
            curr--; //remove leading dot
        }
    }
    return BSON_EOO;
}

EJDB_EXPORT bson_type bson_find_fieldpath_value(const char *fpath, bson_iterator *it) {
    return bson_find_fieldpath_value2(fpath, strlen(fpath), it);
}

EJDB_EXPORT bson_type bson_find_fieldpath_value2(const char *fpath, int fplen, bson_iterator *it) {
    FFPCTX ffctx = {
        .fpath = fpath,
        .fplen = fplen,
        .input = it,
        .stopnestedarr = false,
        .stopos = 0
    };
    return bson_find_fieldpath_value3(&ffctx);
}

EJDB_EXPORT bson_type bson_find_fieldpath_value3(FFPCTX* ffctx) {
    char pstackstack[BSON_MAX_FPATH_LEN + 1];
    char *pstack;
    if (ffctx->fplen <= BSON_MAX_FPATH_LEN) {
        pstack = pstackstack;
    } else {
        pstack = MYMALLOC((ffctx->fplen + 1) * sizeof (char));
        if (!pstack) {
            return BSON_EOO;
        }
    }
    bson_type bt = bson_find_fieldpath_value_impl(pstack, 0, ffctx, ffctx->input);
    if (pstack != pstackstack) {
        MYFREE(pstack);
    }
    return bt;
}

EJDB_EXPORT bson_bool_t bson_iterator_more(const bson_iterator *i) {
    return *(i->cur);
}

EJDB_EXPORT bson_type bson_iterator_next(bson_iterator *i) {
    int ds;

    if (i->first) {
        i->first = 0;
        return (bson_type) (*i->cur);
    }

    switch (bson_iterator_type(i)) {
        case BSON_EOO:
            return BSON_EOO; /* don't advance */
        case BSON_UNDEFINED:
        case BSON_NULL:
            ds = 0;
            break;
        case BSON_BOOL:
            ds = 1;
            break;
        case BSON_INT:
            ds = 4;
            break;
        case BSON_LONG:
        case BSON_DOUBLE:
        case BSON_TIMESTAMP:
        case BSON_DATE:
            ds = 8;
            break;
        case BSON_OID:
            ds = 12;
            break;
        case BSON_STRING:
        case BSON_SYMBOL:
        case BSON_CODE:
            ds = 4 + bson_iterator_int_raw(i);
            break;
        case BSON_BINDATA:
            ds = 5 + bson_iterator_int_raw(i);
            break;
        case BSON_OBJECT:
        case BSON_ARRAY:
        case BSON_CODEWSCOPE:
            ds = bson_iterator_int_raw(i);
            break;
        case BSON_DBREF:
            ds = 4 + 12 + bson_iterator_int_raw(i);
            break;
        case BSON_REGEX:
        {
            const char *s = bson_iterator_value(i);
            const char *p = s;
            p += strlen(p) + 1;
            p += strlen(p) + 1;
            ds = p - s;
            break;
        }

        default:
        {
            char msg[] = "unknown type: 000000000000";
            bson_numstr(msg + 14, (unsigned) (i->cur[0]));
            bson_fatal_msg(0, msg);
            return 0;
        }
    }

    i->cur += 1 + strlen(i->cur + 1) + 1 + ds;

    return (bson_type) (*i->cur);
}

EJDB_EXPORT bson_type bson_iterator_type(const bson_iterator *i) {
    return (bson_type) i->cur[0];
}

EJDB_EXPORT const char *bson_iterator_key(const bson_iterator *i) {
    return i->cur + 1;
}

EJDB_EXPORT const char *bson_iterator_value(const bson_iterator *i) {
    const char *t = i->cur + 1;
    t += strlen(t) + 1;
    return t;
}

/* types */

EJDB_EXPORT int bson_iterator_int_raw(const bson_iterator *i) {
    int out;
    bson_little_endian32(&out, bson_iterator_value(i));
    return out;
}

EJDB_EXPORT double bson_iterator_double_raw(const bson_iterator *i) {
    double out;
    bson_little_endian64(&out, bson_iterator_value(i));
    return out;
}

EJDB_EXPORT int64_t bson_iterator_long_raw(const bson_iterator *i) {
    int64_t out;
    bson_little_endian64(&out, bson_iterator_value(i));
    return out;
}

EJDB_EXPORT bson_bool_t bson_iterator_bool_raw(const bson_iterator *i) {
    return bson_iterator_value(i)[0];
}

EJDB_EXPORT bson_oid_t *bson_iterator_oid(const bson_iterator *i) {
    return (bson_oid_t *) bson_iterator_value(i);
}

EJDB_EXPORT int bson_iterator_int(const bson_iterator *i) {
    switch (bson_iterator_type(i)) {
        case BSON_INT:
            return bson_iterator_int_raw(i);
        case BSON_LONG:
            return bson_iterator_long_raw(i);
        case BSON_DOUBLE:
            return bson_iterator_double_raw(i);
        case BSON_BOOL:
            return bson_iterator_bool_raw(i) ? 1 : 0;
        default:
            return 0;
    }
}

EJDB_EXPORT double bson_iterator_double(const bson_iterator *i) {
    switch (bson_iterator_type(i)) {
        case BSON_INT:
            return bson_iterator_int_raw(i);
        case BSON_LONG:
        case BSON_DATE:
            return bson_iterator_long_raw(i);
        case BSON_DOUBLE:
            return bson_iterator_double_raw(i);
        case BSON_BOOL:
            return bson_iterator_bool_raw(i) ? 1.0 : 0.0;
        default:
            return 0;
    }
}

EJDB_EXPORT int64_t bson_iterator_long(const bson_iterator *i) {
    switch (bson_iterator_type(i)) {
        case BSON_INT:
            return bson_iterator_int_raw(i);
        case BSON_LONG:
        case BSON_DATE:
            return bson_iterator_long_raw(i);
        case BSON_DOUBLE:
            return bson_iterator_double_raw(i);
        case BSON_BOOL:
            return bson_iterator_bool_raw(i) ? 1 : 0;
        default:
            return 0;
    }
}

static int64_t bson_iterator_long_ext(const bson_iterator *i) {
    switch (bson_iterator_type(i)) {
        case BSON_INT:
            return bson_iterator_int_raw(i);
        case BSON_LONG:
        case BSON_DATE:
        case BSON_TIMESTAMP:
            return bson_iterator_long_raw(i);
        case BSON_DOUBLE:
            return bson_iterator_double_raw(i);
        case BSON_BOOL:
            return bson_iterator_bool_raw(i) ? 1 : 0;
        default:
            return 0;
    }
}

EJDB_EXPORT bson_timestamp_t bson_iterator_timestamp(const bson_iterator *i) {
    bson_timestamp_t ts;
    bson_little_endian32(&(ts.i), bson_iterator_value(i));
    bson_little_endian32(&(ts.t), bson_iterator_value(i) + 4);
    return ts;
}

EJDB_EXPORT int bson_iterator_timestamp_time(const bson_iterator *i) {
    int time;
    bson_little_endian32(&time, bson_iterator_value(i) + 4);
    return time;
}

EJDB_EXPORT int bson_iterator_timestamp_increment(const bson_iterator *i) {
    int increment;
    bson_little_endian32(&increment, bson_iterator_value(i));
    return increment;
}

EJDB_EXPORT bson_bool_t bson_iterator_bool(const bson_iterator *i) {
    switch (bson_iterator_type(i)) {
        case BSON_BOOL:
            return bson_iterator_bool_raw(i);
        case BSON_INT:
            return bson_iterator_int_raw(i) != 0;
        case BSON_LONG:
        case BSON_DATE:
            return bson_iterator_long_raw(i) != 0;
        case BSON_DOUBLE:
            return bson_iterator_double_raw(i) != 0;
        case BSON_EOO:
        case BSON_NULL:
        case BSON_UNDEFINED:
            return 0;
        default:
            return 1;
    }
}

EJDB_EXPORT const char *bson_iterator_string(const bson_iterator *i) {
    switch (bson_iterator_type(i)) {
        case BSON_STRING:
        case BSON_SYMBOL:
            return bson_iterator_value(i) + 4;
        default:
            return "";
    }
}

int bson_iterator_string_len(const bson_iterator *i) {
    return bson_iterator_int_raw(i);
}

EJDB_EXPORT const char *bson_iterator_code(const bson_iterator *i) {
    switch (bson_iterator_type(i)) {
        case BSON_STRING:
        case BSON_CODE:
            return bson_iterator_value(i) + 4;
        case BSON_CODEWSCOPE:
            return bson_iterator_value(i) + 8;
        default:
            return NULL;
    }
}

EJDB_EXPORT void bson_iterator_code_scope(const bson_iterator *i, bson *scope) {
    if (bson_iterator_type(i) == BSON_CODEWSCOPE) {
        int code_len;
        bson_little_endian32(&code_len, bson_iterator_value(i) + 4);
        bson_init_data(scope, (void *) (bson_iterator_value(i) + 8 + code_len));
        bson_reset(scope);
        scope->finished = 1;
    } else {
        bson_empty(scope);
    }
}

EJDB_EXPORT bson_date_t bson_iterator_date(const bson_iterator *i) {
    return bson_iterator_long_raw(i);
}

EJDB_EXPORT time_t bson_iterator_time_t(const bson_iterator *i) {
    return bson_iterator_date(i) / 1000;
}

EJDB_EXPORT int bson_iterator_bin_len(const bson_iterator *i) {
    return ( bson_iterator_bin_type(i) == BSON_BIN_BINARY_OLD)
            ? bson_iterator_int_raw(i) - 4
            : bson_iterator_int_raw(i);
}

EJDB_EXPORT char bson_iterator_bin_type(const bson_iterator *i) {
    return bson_iterator_value(i)[4];
}

EJDB_EXPORT const char *bson_iterator_bin_data(const bson_iterator *i) {
    return ( bson_iterator_bin_type(i) == BSON_BIN_BINARY_OLD)
            ? bson_iterator_value(i) + 9
            : bson_iterator_value(i) + 5;
}

EJDB_EXPORT const char *bson_iterator_regex(const bson_iterator *i) {
    return bson_iterator_value(i);
}

EJDB_EXPORT const char *bson_iterator_regex_opts(const bson_iterator *i) {
    const char *p = bson_iterator_value(i);
    return p + strlen(p) + 1;

}

EJDB_EXPORT void bson_iterator_subobject(const bson_iterator *i, bson *sub) {
    bson_init_data(sub, (char *) bson_iterator_value(i));
    bson_reset(sub);
    sub->finished = 1;
}

EJDB_EXPORT void bson_iterator_subiterator(const bson_iterator *i, bson_iterator *sub) {
    bson_iterator_from_buffer(sub, bson_iterator_value(i));
}

/* ----------------------------
   BUILDING
   ------------------------------ */

static void _bson_init_size(bson *b, int size) {
    if (size == 0) {
        b->data = NULL;
    } else {
        b->data = (char *) bson_malloc(size);
    }
    b->dataSize = size;
    b->cur = b->data + 4;
    bson_reset(b);
}

EJDB_EXPORT void bson_init(bson *b) {
    _bson_init_size(b, initialBufferSize);
}

EJDB_EXPORT void bson_init_as_query(bson *b) {
    bson_init(b);
    b->flags |= BSON_FLAG_QUERY_MODE;
}

void bson_init_size(bson *b, int size) {
    _bson_init_size(b, size);
}

void bson_init_on_stack(bson *b, char *bstack, int mincapacity, int maxonstack) {
    bson_reset(b);
    b->data = (mincapacity < maxonstack) ? bstack : bson_malloc_func(mincapacity);
    b->cur = b->data + 4;
    b->dataSize = (mincapacity < maxonstack) ? maxonstack : mincapacity;
    if (b->data == bstack) {
        b->flags |= BSON_FLAG_STACK_ALLOCATED;
    }
}

void bson_append_byte(bson *b, char c) {
    b->cur[0] = c;
    b->cur++;
}

EJDB_EXPORT void bson_append(bson *b, const void *data, int len) {
    memcpy(b->cur, data, len);
    b->cur += len;
}

void bson_append32(bson *b, const void *data) {
    bson_little_endian32(b->cur, data);
    b->cur += 4;
}

void bson_append64(bson *b, const void *data) {
    bson_little_endian64(b->cur, data);
    b->cur += 8;
}

int bson_ensure_space(bson *b, const int bytesNeeded) {
    int pos = b->cur - b->data;
    char *orig = b->data;
    int new_size;

    if (pos + bytesNeeded <= b->dataSize)
        return BSON_OK;

    new_size = 1.5 * (b->dataSize + bytesNeeded);

    if (new_size < b->dataSize) {
        if ((b->dataSize + bytesNeeded) < INT_MAX)
            new_size = INT_MAX;
        else {
            b->err = BSON_SIZE_OVERFLOW;
            return BSON_ERROR;
        }
    }

    if (b->flags & BSON_FLAG_STACK_ALLOCATED) { //translate stack memory into heap
        char *odata = b->data;
        b->data = bson_malloc_func(new_size);
        if (!b->data) {
            bson_fatal_msg(!!b->data, "malloc() failed");
            return BSON_ERROR;
        }
        if (odata) {
            memcpy(b->data, odata, MIN(new_size, b->dataSize));
        }
        b->flags &= ~BSON_FLAG_STACK_ALLOCATED; //reset this flag
    } else {
        b->data = bson_realloc(b->data, new_size);
    }
    if (!b->data) {
        bson_fatal_msg(!!b->data, "realloc() failed");
        return BSON_ERROR;
    }

    b->dataSize = new_size;
    b->cur += b->data - orig;

    return BSON_OK;
}

EJDB_EXPORT int bson_finish(bson *b) {
    int i;

    if (b->err & BSON_NOT_UTF8)
        return BSON_ERROR;

    if (!b->finished) {
        if (bson_ensure_space(b, 1) == BSON_ERROR) return BSON_ERROR;
        bson_append_byte(b, 0);
        i = b->cur - b->data;
        bson_little_endian32(b->data, &i);
        b->finished = 1;
    }

    return BSON_OK;
}

EJDB_EXPORT void bson_destroy(bson *b) {
    if (b) {
        if (b->data && !(b->flags & BSON_FLAG_STACK_ALLOCATED)) {
            bson_free(b->data);
        }
        b->err = 0;
        b->data = 0;
        b->cur = 0;
        b->finished = 1;
        if (b->errstr) {
            bson_free_func(b->errstr);
            b->errstr = NULL;
        }
    }
}

EJDB_EXPORT void bson_del(bson *b) {
    if (b) {
        bson_destroy(b);
        bson_free(b);
    }
}

static int bson_append_estart2(bson *b, int type, const char *name, int namelen, const int dataSize);

static int bson_append_estart(bson *b, int type, const char *name, const int dataSize) {
    return bson_append_estart2(b, type, name, strlen(name), dataSize);
}

static int bson_append_estart2(bson *b, int type, const char *name, int namelen, const int dataSize) {
    const int len = namelen + 1;

    if (b->finished) {
        b->err |= BSON_ALREADY_FINISHED;
        return BSON_ERROR;
    }

    if (bson_ensure_space(b, 1 + len + dataSize) == BSON_ERROR) {
        return BSON_ERROR;
    }

    if (bson_check_field_name(b, (const char *) name, namelen,
            !(b->flags & BSON_FLAG_QUERY_MODE), !(b->flags & BSON_FLAG_QUERY_MODE)) == BSON_ERROR) {
        bson_builder_error(b);
        return BSON_ERROR;
    }
    bson_append_byte(b, (char) type);
    memcpy(b->cur, name, namelen);
    b->cur += namelen;
    *(b->cur) = '\0';
    b->cur += 1;
    return BSON_OK;
}

/* ----------------------------
   BUILDING TYPES
   ------------------------------ */

EJDB_EXPORT int bson_append_int(bson *b, const char *name, const int i) {
    if (bson_append_estart(b, BSON_INT, name, 4) == BSON_ERROR)
        return BSON_ERROR;
    bson_append32(b, &i);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_long(bson *b, const char *name, const int64_t i) {
    if (bson_append_estart(b, BSON_LONG, name, 8) == BSON_ERROR)
        return BSON_ERROR;
    bson_append64(b, &i);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_double(bson *b, const char *name, const double d) {
    if (bson_append_estart(b, BSON_DOUBLE, name, 8) == BSON_ERROR)
        return BSON_ERROR;
    bson_append64(b, &d);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_bool(bson *b, const char *name, const bson_bool_t i) {
    if (bson_append_estart(b, BSON_BOOL, name, 1) == BSON_ERROR)
        return BSON_ERROR;
    bson_append_byte(b, i != 0);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_null(bson *b, const char *name) {
    if (bson_append_estart(b, BSON_NULL, name, 0) == BSON_ERROR)
        return BSON_ERROR;
    return BSON_OK;
}

EJDB_EXPORT int bson_append_undefined(bson *b, const char *name) {
    if (bson_append_estart(b, BSON_UNDEFINED, name, 0) == BSON_ERROR)
        return BSON_ERROR;
    return BSON_OK;
}

int bson_append_string_base(bson *b, const char *name,
        const char *value, int len, bson_type type) {

    int sl = len + 1;
    if (bson_check_string(b, (const char *) value, sl - 1) == BSON_ERROR)
        return BSON_ERROR;
    if (bson_append_estart(b, type, name, 4 + sl) == BSON_ERROR) {
        return BSON_ERROR;
    }
    bson_append32(b, &sl);
    bson_append(b, value, sl - 1);
    bson_append(b, "\0", 1);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_string(bson *b, const char *name, const char *value) {
    return bson_append_string_base(b, name, value, strlen(value), BSON_STRING);
}

EJDB_EXPORT int bson_append_symbol(bson *b, const char *name, const char *value) {
    return bson_append_string_base(b, name, value, strlen(value), BSON_SYMBOL);
}

EJDB_EXPORT int bson_append_code(bson *b, const char *name, const char *value) {
    return bson_append_string_base(b, name, value, strlen(value), BSON_CODE);
}

EJDB_EXPORT int bson_append_string_n(bson *b, const char *name, const char *value, int len) {
    return bson_append_string_base(b, name, value, len, BSON_STRING);
}

EJDB_EXPORT int bson_append_symbol_n(bson *b, const char *name, const char *value, int len) {
    return bson_append_string_base(b, name, value, len, BSON_SYMBOL);
}

EJDB_EXPORT int bson_append_code_n(bson *b, const char *name, const char *value, int len) {
    return bson_append_string_base(b, name, value, len, BSON_CODE);
}

EJDB_EXPORT int bson_append_code_w_scope_n(bson *b, const char *name,
        const char *code, int len, const bson *scope) {

    int sl, size;
    if (!scope) return BSON_ERROR;
    sl = len + 1;
    size = 4 + 4 + sl + bson_size(scope);
    if (bson_append_estart(b, BSON_CODEWSCOPE, name, size) == BSON_ERROR)
        return BSON_ERROR;
    bson_append32(b, &size);
    bson_append32(b, &sl);
    bson_append(b, code, sl);
    bson_append(b, scope->data, bson_size(scope));
    return BSON_OK;
}

EJDB_EXPORT int bson_append_code_w_scope(bson *b, const char *name, const char *code, const bson *scope) {
    return bson_append_code_w_scope_n(b, name, code, strlen(code), scope);
}

EJDB_EXPORT int bson_append_binary(bson *b, const char *name, char type, const char *str, int len) {
    if (type == BSON_BIN_BINARY_OLD) {
        int subtwolen = len + 4;
        if (bson_append_estart(b, BSON_BINDATA, name, 4 + 1 + 4 + len) == BSON_ERROR)
            return BSON_ERROR;
        bson_append32(b, &subtwolen);
        bson_append_byte(b, type);
        bson_append32(b, &len);
        bson_append(b, str, len);
    } else {
        if (bson_append_estart(b, BSON_BINDATA, name, 4 + 1 + len) == BSON_ERROR)
            return BSON_ERROR;
        bson_append32(b, &len);
        bson_append_byte(b, type);
        bson_append(b, str, len);
    }
    return BSON_OK;
}

EJDB_EXPORT int bson_append_oid(bson *b, const char *name, const bson_oid_t *oid) {
    if (bson_append_estart(b, BSON_OID, name, 12) == BSON_ERROR)
        return BSON_ERROR;
    bson_append(b, oid, 12);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_new_oid(bson *b, const char *name) {
    bson_oid_t oid;
    bson_oid_gen(&oid);
    return bson_append_oid(b, name, &oid);
}

EJDB_EXPORT int bson_append_regex(bson *b, const char *name, const char *pattern, const char *opts) {
    const int plen = strlen(pattern) + 1;
    const int olen = strlen(opts) + 1;
    if (bson_append_estart(b, BSON_REGEX, name, plen + olen) == BSON_ERROR)
        return BSON_ERROR;
    if (bson_check_string(b, pattern, plen - 1) == BSON_ERROR)
        return BSON_ERROR;
    bson_append(b, pattern, plen);
    bson_append(b, opts, olen);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_bson(bson *b, const char *name, const bson *bson) {
    if (!bson) return BSON_ERROR;
    if (bson_append_estart(b, BSON_OBJECT, name, bson_size(bson)) == BSON_ERROR)
        return BSON_ERROR;
    bson_append(b, bson->data, bson_size(bson));
    return BSON_OK;
}

EJDB_EXPORT int bson_append_element(bson *b, const char *name_or_null, const bson_iterator *elem) {
    bson_iterator next = *elem;
    int size;

    bson_iterator_next(&next);
    size = next.cur - elem->cur;

    if (name_or_null == NULL) {
        if (bson_ensure_space(b, size) == BSON_ERROR)
            return BSON_ERROR;
        bson_append(b, elem->cur, size);
    } else {
        int data_size = size - 2 - strlen(bson_iterator_key(elem));
        bson_append_estart(b, elem->cur[0], name_or_null, data_size);
        bson_append(b, bson_iterator_value(elem), data_size);
    }

    return BSON_OK;
}

EJDB_EXPORT int bson_append_timestamp(bson *b, const char *name, bson_timestamp_t *ts) {
    if (bson_append_estart(b, BSON_TIMESTAMP, name, 8) == BSON_ERROR) return BSON_ERROR;

    bson_append32(b, &(ts->i));
    bson_append32(b, &(ts->t));

    return BSON_OK;
}

EJDB_EXPORT int bson_append_timestamp2(bson *b, const char *name, int time, int increment) {
    if (bson_append_estart(b, BSON_TIMESTAMP, name, 8) == BSON_ERROR) return BSON_ERROR;

    bson_append32(b, &increment);
    bson_append32(b, &time);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_date(bson *b, const char *name, bson_date_t millis) {
    if (bson_append_estart(b, BSON_DATE, name, 8) == BSON_ERROR) return BSON_ERROR;
    bson_append64(b, &millis);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_time_t(bson *b, const char *name, time_t secs) {
    return bson_append_date(b, name, (bson_date_t) secs * 1000);
}

EJDB_EXPORT int bson_append_start_object(bson *b, const char *name) {
    if (bson_append_estart(b, BSON_OBJECT, name, 5) == BSON_ERROR) return BSON_ERROR;
    b->stack[ b->stackPos++ ] = b->cur - b->data;
    bson_append32(b, &zero);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_start_object2(bson *b, const char *name, int namelen) {
    if (bson_append_estart2(b, BSON_OBJECT, name, namelen, 5) == BSON_ERROR) return BSON_ERROR;
    b->stack[ b->stackPos++ ] = b->cur - b->data;
    bson_append32(b, &zero);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_start_array(bson *b, const char *name) {
    return bson_append_start_array2(b, name, strlen(name));
}

EJDB_EXPORT int bson_append_start_array2(bson *b, const char *name, int namelen) {
    if (bson_append_estart2(b, BSON_ARRAY, name, namelen, 5) == BSON_ERROR) return BSON_ERROR;
    b->stack[ b->stackPos++ ] = b->cur - b->data;
    bson_append32(b, &zero);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_finish_object(bson *b) {
    char *start;
    int i;
    if (bson_ensure_space(b, 1) == BSON_ERROR) return BSON_ERROR;
    bson_append_byte(b, 0);

    start = b->data + b->stack[ --b->stackPos ];
    i = b->cur - start;
    bson_little_endian32(start, &i);

    return BSON_OK;
}

EJDB_EXPORT double bson_int64_to_double(int64_t i64) {
    return (double) i64;
}

EJDB_EXPORT int bson_append_finish_array(bson *b) {
    return bson_append_finish_object(b);
}

/* Error handling and allocators. */

static bson_err_handler err_handler = NULL;

EJDB_EXPORT bson_err_handler set_bson_err_handler(bson_err_handler func) {
    bson_err_handler old = err_handler;
    err_handler = func;
    return old;
}

EJDB_EXPORT void bson_free(void *ptr) {
    bson_free_func(ptr);
}

EJDB_EXPORT void *bson_malloc(int size) {
    void *p;
    p = bson_malloc_func(size);
    bson_fatal_msg(!!p, "malloc() failed");
    return p;
}

void *bson_realloc(void *ptr, int size) {
    void *p;
    p = bson_realloc_func(ptr, size);
    bson_fatal_msg(!!p, "realloc() failed");
    return p;
}

int _bson_errprintf(const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
#ifndef R_SAFETY_NET
    ret = vfprintf(stderr, format, ap);
#endif
    va_end(ap);

    return ret;
}

/**
 * This method is invoked when a non-fatal bson error is encountered.
 * Calls the error handler if available.
 *
 *  @param
 */
void bson_builder_error(bson *b) {
    if (err_handler)
        err_handler("BSON error.");
}

void bson_fatal(int ok) {
    bson_fatal_msg(ok, "");
}

void bson_fatal_msg(int ok, const char *msg) {
    if (ok)
        return;

    if (err_handler) {
        err_handler(msg);
    }
#ifndef R_SAFETY_NET
    bson_errprintf("error: %s\n", msg);
    exit(-5);
#endif
}


/* Efficiently copy an integer to a string. */
extern const char bson_numstrs[1000][4];

EJDB_EXPORT void bson_numstr(char *str, int64_t i) {
    if (i < 1000)
        memcpy(str, bson_numstrs[i], 4);
    else
        bson_sprintf(str, "%lld", (long long int) i);
}

EJDB_EXPORT int bson_numstrn(char *str, int maxbuf, int64_t i) {
    if (i < 1000 && maxbuf > 4) {
        memcpy(str, bson_numstrs[i], 4);
        return strlen(bson_numstrs[i]);
    } else {
        return snprintf(str, maxbuf, "%lld", (long long int) i);
    }
}

static bson_bool_t bson_isnumstr(const char *str, int len) {
    assert(str);
    bool isnum = false;
    while (len > 0 && *str > '\0' && *str <= ' ') {
        str++;
        len--;
    }
    while (len > 0 && *str >= '0' && *str <= '9') {
        isnum = true;
        str++;
        len--;
    }
    while (len > 0 && *str > '\0' && *str <= ' ') {
        str++;
        len--;
    }
    return (isnum && (*str == '\0' || len == 0));
}

EJDB_EXPORT void bson_swap_endian64(void *outp, const void *inp) {
    const char *in = (const char *) inp;
    char *out = (char *) outp;
    out[0] = in[7];
    out[1] = in[6];
    out[2] = in[5];
    out[3] = in[4];
    out[4] = in[3];
    out[5] = in[2];
    out[6] = in[1];
    out[7] = in[0];

}

EJDB_EXPORT void bson_swap_endian32(void *outp, const void *inp) {
    const char *in = (const char *) inp;
    char *out = (char *) outp;
    out[0] = in[3];
    out[1] = in[2];
    out[2] = in[1];
    out[3] = in[0];
}

EJDB_EXPORT int bson_append_array_from_iterator(const char *key, bson_iterator *from, bson *into) {
    assert(key && from && into);
    bson_type bt;
    bson_append_start_array(into, key);
    while ((bt = bson_iterator_next(from)) != BSON_EOO) {
        bson_append_field_from_iterator(from, into);
    }
    bson_append_finish_array(into);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_object_from_iterator(const char *key, bson_iterator *from, bson *into) {
    assert(key && from && into);
    bson_type bt;
    bson_append_start_object(into, key);
    while ((bt = bson_iterator_next(from)) != BSON_EOO) {
        bson_append_field_from_iterator(from, into);
    }
    bson_append_finish_object(into);
    return BSON_OK;
}

EJDB_EXPORT int bson_append_field_from_iterator2(const char *key, const bson_iterator *from, bson *into) {
    assert(key && from && into);
    bson_type t = bson_iterator_type(from);
    if (t == BSON_EOO || into->finished) {
        return BSON_ERROR;
    }
    switch (t) {
        case BSON_STRING:
        case BSON_SYMBOL:
            bson_append_string(into, key, bson_iterator_string(from));
            break;
        case BSON_CODE:
            bson_append_code(into, key, bson_iterator_code(from));
            break;
        case BSON_INT:
            bson_append_int(into, key, bson_iterator_int_raw(from));
            break;
        case BSON_DOUBLE:
            bson_append_double(into, key, bson_iterator_double_raw(from));
            break;
        case BSON_LONG:
            bson_append_long(into, key, bson_iterator_long_raw(from));
            break;
        case BSON_UNDEFINED:
            bson_append_undefined(into, key);
            break;
        case BSON_NULL:
            bson_append_null(into, key);
            break;
        case BSON_BOOL:
            bson_append_bool(into, key, bson_iterator_bool_raw(from));
            break;
        case BSON_TIMESTAMP:
        {
            bson_timestamp_t ts = bson_iterator_timestamp(from);
            bson_append_timestamp(into, key, &ts);
            break;
        }
        case BSON_DATE:
            bson_append_date(into, key, bson_iterator_date(from));
            break;
        case BSON_REGEX:
            bson_append_regex(into, key, bson_iterator_regex(from), bson_iterator_regex_opts(from));
            break;
        case BSON_OID:
            bson_append_oid(into, key, bson_iterator_oid(from));
            break;
        case BSON_OBJECT:
        {
            bson_iterator sit;
            bson_iterator_subiterator(from, &sit);
            bson_append_start_object(into, key);
            while (bson_iterator_next(&sit) != BSON_EOO) {
                bson_append_field_from_iterator(&sit, into);
            }
            bson_append_finish_object(into);
            break;
        }
        case BSON_ARRAY:
        {
            bson_iterator sit;
            bson_iterator_subiterator(from, &sit);
            bson_append_start_array(into, key);
            while (bson_iterator_next(&sit) != BSON_EOO) {
                bson_append_field_from_iterator(&sit, into);
            }
            bson_append_finish_array(into);
            break;
        }
        case BSON_DBREF:
        case BSON_CODEWSCOPE:
            break;
        default:
            break;
    }
    return BSON_OK;
}

EJDB_EXPORT int bson_append_field_from_iterator(const bson_iterator *from, bson *into) {
    assert(from && into);
    return bson_append_field_from_iterator2(bson_iterator_key(from), from, into);
}

EJDB_EXPORT int bson_merge2(const void *b1data, const void *b2data, bson_bool_t overwrite, bson *out) {
    bson_iterator it1, it2;
    bson_type bt1, bt2;

    bson_iterator_from_buffer(&it1, b1data);
    bson_iterator_from_buffer(&it2, b2data);
    //Append all fields in B1 overwriten by B2
    while ((bt1 = bson_iterator_next(&it1)) != BSON_EOO) {
        const char* k1 = bson_iterator_key(&it1);
        if (overwrite && strcmp(JDBIDKEYNAME, k1) && (bt2 = bson_find_from_buffer(&it2, b2data, k1)) != BSON_EOO) {
            bson_append_field_from_iterator(&it2, out);
        } else {
            bson_append_field_from_iterator(&it1, out);
        }
    }
    bson_iterator_from_buffer(&it1, b1data);
    bson_iterator_from_buffer(&it2, b2data);
    //Append all fields from B2 missing in B1
    while ((bt2 = bson_iterator_next(&it2)) != BSON_EOO) {
        const char* k2 = bson_iterator_key(&it2);
        if ((bt1 = bson_find_from_buffer(&it1, b1data, k2)) == BSON_EOO) {
            bson_append_field_from_iterator(&it2, out);
        }
    }
    return BSON_OK;
}

EJDB_EXPORT int bson_merge(const bson *b1, const bson *b2, bson_bool_t overwrite, bson *out) {
    assert(b1 && b2 && out);
    if (!b1->finished || !b2->finished || out->finished) {
        return BSON_ERROR;
    }
    return bson_merge2(bson_data(b1), bson_data(b2), overwrite, out);
}

EJDB_EXPORT int bson_inplace_set_bool(bson_iterator *pos, bson_bool_t val) {
    assert(pos);
    bson_type bt = bson_iterator_type(pos);
    if (bt != BSON_BOOL) {
        return BSON_ERROR;
    }
    char *t = (char*) pos->cur + 1;
    t += strlen(t) + 1;
    *t = (val != 0);
    return BSON_OK;
}

EJDB_EXPORT int bson_inplace_set_long(bson_iterator *pos, int64_t val) {
    assert(pos);
    bson_type bt = bson_iterator_type(pos);
    if (!BSON_IS_NUM_TYPE(bt)) {
        return BSON_ERROR;
    }
    char *t = (char*) pos->cur + 1;
    t += strlen(t) + 1;
    if (bt == BSON_INT) {
        bson_little_endian32(t, &val);
    } else if (bt == BSON_LONG || bt == BSON_DATE) {
        bson_little_endian64(t, &val);
    } else if (bt == BSON_DOUBLE) {
        double dval = (double) val;
        bson_little_endian64(t, &dval);
    } else {
        return BSON_ERROR;
    }
    return BSON_OK;
}

EJDB_EXPORT int bson_inplace_set_double(bson_iterator *pos, double val) {
    assert(pos);
    bson_type bt = bson_iterator_type(pos);
    if (!BSON_IS_NUM_TYPE(bt)) {
        return BSON_ERROR;
    }
    int64_t ival = (int64_t) val;
    char *t = (char*) pos->cur + 1;
    t += strlen(t) + 1;
    if (bt == BSON_INT) {
        bson_little_endian32(t, &ival);
    } else if (bt == BSON_LONG || bt == BSON_DATE) {
        bson_little_endian64(t, &ival);
    } else if (bt == BSON_DOUBLE) {
        bson_little_endian64(t, &val);
    } else {
        return BSON_ERROR;
    }
    return BSON_OK;
}

EJDB_EXPORT int bson_compare_fpaths(const void *bsdata1, const void *bsdata2, const char *fpath1, int fplen1, const char *fpath2, int fplen2) {
    assert(bsdata1 && bsdata2 && fpath1 && fpath2);
    bson_iterator it1, it2;
    bson_iterator_from_buffer(&it1, bsdata1);
    bson_iterator_from_buffer(&it2, bsdata2);
    bson_find_fieldpath_value2(fpath1, fplen1, &it1);
    bson_find_fieldpath_value2(fpath2, fplen2, &it2);
    return bson_compare_it_current(&it1, &it2);
}

/**
 *
 * Return -1 if value pointing by it1 lesser than from it2.
 * Return  0 if values equal
 * Return  1 if value pointing by it1 greater than from it2.
 * Return -2 if values are not comparable.
 * @param it1
 * @param i
 * @return
 */
EJDB_EXPORT int bson_compare_it_current(const bson_iterator *it1, const bson_iterator *it2) {
    bson_type t1 = bson_iterator_type(it1);
    bson_type t2 = bson_iterator_type(it2);
    if (t1 == BSON_BOOL || t1 == BSON_EOO || t1 == BSON_NULL || t1 == BSON_UNDEFINED) {
        int v1 = bson_iterator_bool(it1);
        int v2 = bson_iterator_bool(it2);
        return (v1 > v2) ? 1 : ((v1 < v2) ? -1 : 0);
    } else if (t1 == BSON_INT || t1 == BSON_LONG || t1 == BSON_DATE || t1 == BSON_TIMESTAMP) {
        int64_t v1 = bson_iterator_long_ext(it1);
        int64_t v2 = bson_iterator_long_ext(it2);
        return (v1 > v2) ? 1 : ((v1 < v2) ? -1 : 0);
    } else if (t1 == BSON_DOUBLE) {
        double v1 = bson_iterator_double_raw(it1);
        double v2 = bson_iterator_double(it2);
        return (v1 > v2) ? 1 : ((v1 < v2) ? -1 : 0);
    } else if (t1 == BSON_STRING || t1 == BSON_SYMBOL) {
        const char* v1 = bson_iterator_string(it1);
        int l1 = bson_iterator_string_len(it1);
        const char* v2 = bson_iterator_string(it2);
        int l2 = (t2 == BSON_STRING || t2 == BSON_SYMBOL) ? bson_iterator_string_len(it2) : strlen(v2);
        int rv;
        TCCMPLEXICAL(rv, v1, l1, v2, l2);
        return rv;
    } else if (t1 == BSON_BINDATA && t2 == BSON_BINDATA) {
        int l1 = bson_iterator_bin_len(it1);
        int l2 = bson_iterator_bin_len(it2);
        return memcmp(bson_iterator_bin_data(it1), bson_iterator_bin_data(it2), MIN(l1, l2));
    } else if (t1 == BSON_OID && t2 == BSON_OID) {
        return memcmp(bson_iterator_oid(it1), bson_iterator_oid(it2), sizeof (bson_oid_t));
    } else if (t1 == t2 && (t1 == BSON_OBJECT || t1 == BSON_ARRAY)) {
        int cv = 0;
        bson_type bt1, bt2;
        bson_iterator sit1, sit2;
        bson_iterator_subiterator(it1, &sit1);
        bson_iterator_subiterator(it2, &sit2);
        while ((bt1 = bson_iterator_next(&sit1)) != BSON_EOO) {
            bt2 = bson_iterator_next(&sit2);
            if (bt2 == BSON_EOO) {
                cv = 1;
                break;
            }
            cv = bson_compare_it_current(&sit1, &sit2);
            if (cv) {
                break;
            }
        }
        if (cv == 0 && bson_iterator_next(&sit2) != BSON_EOO) {
            cv = -1;
        }
        return cv;
    }
    return (t1 - t2);
}

EJDB_EXPORT int bson_compare(const void *bsdata1, const void *bsdata2, const char* fpath, int fplen) {
    return bson_compare_fpaths(bsdata1, bsdata2, fpath, fplen, fpath, fplen);
}

EJDB_EXPORT int bson_compare_string(const char *cv, const void *bsdata, const char *fpath) {
    assert(cv && bsdata && fpath);
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_string(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

EJDB_EXPORT int bson_compare_long(long cv, const void *bsdata, const char *fpath) {
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_long(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

EJDB_EXPORT int bson_compare_double(double cv, const void *bsdata, const char *fpath) {
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_double(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

EJDB_EXPORT int bson_compare_bool(bson_bool_t cv, const void *bsdata, const char *fpath) {
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_bool(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

EJDB_EXPORT bson* bson_dup(const bson *src) {
    assert(src);
    bson *rv = bson_create();
    int s = bson_size(src);
    _bson_init_size(rv, s);
    memmove(rv->data, src->data, s);
    rv->finished = 1;
    return rv;
}

EJDB_EXPORT bson* bson_create_from_buffer(const void* buf, int bufsz) {
    return bson_create_from_buffer2(bson_create(), buf, bufsz);
}

EJDB_EXPORT bson* bson_create_from_buffer2(bson *rv, const void* buf, int bufsz) {
    assert(buf);
    assert(bufsz - 4 > 0);
    bson_init_size(rv, bufsz);
    bson_ensure_space(rv, bufsz - 4);
    bson_append(rv, (char*) buf + 4, bufsz - (4 + 1/*BSON_EOO*/));
    bson_finish(rv);
    return rv;
}

EJDB_EXPORT bool bson_find_merged_array_sets(const void *mbuf, const void *inbuf, bool expandall) {
    assert(mbuf && inbuf);
    bool found = false;
    bson_iterator it, it2;
    bson_type bt, bt2;
    bson_iterator_from_buffer(&it, mbuf);

    while (!found && (bt = bson_iterator_next(&it)) != BSON_EOO) {
        if (expandall && bt != BSON_ARRAY) {
            continue;
        }
        bson_iterator_from_buffer(&it2, inbuf);
        bt2 = bson_find_fieldpath_value(bson_iterator_key(&it), &it2);
        if (bt2 != BSON_ARRAY) {
            continue;
        }
        bson_iterator sit;
        bson_iterator_subiterator(&it2, &sit);
        while (!found && (bt2 = bson_iterator_next(&sit)) != BSON_EOO) {
            if (expandall) {
                bson_iterator sit2;
                bson_iterator_subiterator(&it, &sit2);
                while ((bt2 = bson_iterator_next(&sit2)) != BSON_EOO) {
                    if (!bson_compare_it_current(&sit, &sit2)) {
                        found = true;
                        break;
                    }
                }
            } else {
                if (!bson_compare_it_current(&sit, &it)) {
                    found = true;
                    break;
                }
            }
        }
    }
    return found;
}

EJDB_EXPORT bool bson_find_unmerged_array_sets(const void *mbuf, const void *inbuf) {
    assert(mbuf && inbuf);
    bool allfound = false;
    bson_iterator it, it2;
    bson_type bt, bt2;
    bson_iterator_from_buffer(&it, mbuf);
    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        bson_iterator_from_buffer(&it2, inbuf);
        bt2 = bson_find_fieldpath_value(bson_iterator_key(&it), &it2);
        if (bt2 == BSON_EOO) { //array missing it will be created
            allfound = false;
            break;
        }
        if (bt2 != BSON_ARRAY) { //not an array field
            continue;
        }
        allfound = false;
        bson_iterator sit;
        bson_iterator_subiterator(&it2, &sit);
        while ((bt2 = bson_iterator_next(&sit)) != BSON_EOO) {
            if (!bson_compare_it_current(&sit, &it)) {
                allfound = true;
                break;
            }
        }
        if (!allfound) {
            break;
        }
    }
    return !allfound;
}

typedef struct {
    bson *bsout;
    const void *mbuf;
    int mfields;
    int ecode;
    bool duty;
    bool expandall;
} BSON_MASETS_CTX;

static bson_visitor_cmd_t bson_merge_array_sets_pull_tf(const char *fpath, int fpathlen, const char *key, int keylen, const bson_iterator *it, bool after, void *op) {
    BSON_MASETS_CTX *ctx = op;
    assert(ctx && ctx->mfields >= 0);
    bson_iterator mit;
    bson_type bt = bson_iterator_type(it);
    if (bt != BSON_OBJECT && bt != BSON_ARRAY) { //trivial case
        if (after) {
            return (BSON_VCMD_OK);
        }
        bson_append_field_from_iterator(it, ctx->bsout);
        return (BSON_VCMD_SKIP_AFTER);
    }
    if (bt == BSON_ARRAY) {
        bson_iterator_from_buffer(&mit, ctx->mbuf);
        bt = bson_find_fieldpath_value2(fpath, fpathlen, &mit);
        if (bt == BSON_EOO || (ctx->expandall && bt != BSON_ARRAY)) {
            bson_append_field_from_iterator(it, ctx->bsout);
            return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
        }
        if (ctx->mfields > 0) {
            --ctx->mfields;
        }
        //Find and merge
        bson_iterator ait;
        bson_iterator_subiterator(it, &ait);
        bson_append_start_array(ctx->bsout, key);
        int c = 0;
        bool found = false;
        while ((bt = bson_iterator_next(&ait)) != BSON_EOO) {
            found = false;
            if (ctx->expandall) {
                bson_iterator mitsub;
                bson_iterator_subiterator(&mit, &mitsub);
                while ((bt = bson_iterator_next(&mitsub)) != BSON_EOO) {
                    if (!bson_compare_it_current(&ait, &mitsub)) {
                        found = true;
                        break;
                    }
                }
            } else {
                found = !bson_compare_it_current(&ait, &mit);
            }
            if (!found) {
                char kbuf[TCNUMBUFSIZ];
                bson_numstrn(kbuf, TCNUMBUFSIZ, c++);
                bson_append_field_from_iterator2(kbuf, &ait, ctx->bsout);
            }
        }
        bson_append_finish_array(ctx->bsout);
        return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
    }
    //bt is BSON_OBJECT
    if (!after) {
        bson_append_start_object(ctx->bsout, key);
    } else { //after
        bson_append_finish_object(ctx->bsout);
    }
    return (BSON_VCMD_OK);
}

static bson_visitor_cmd_t bson_merge_array_sets_tf(const char *fpath, int fpathlen, const char *key, int keylen, const bson_iterator *it, bool after, void *op) {
    BSON_MASETS_CTX *ctx = op;
    assert(ctx && ctx->mfields >= 0);
    bson_iterator mit;
    bson_type bt = bson_iterator_type(it);

    if (bt != BSON_OBJECT && bt != BSON_ARRAY) { //trivial case
        if (after) {
            return (BSON_VCMD_OK);
        }
        bson_append_field_from_iterator(it, ctx->bsout);
        return (BSON_VCMD_SKIP_AFTER);
    }
    if (bt == BSON_ARRAY) {
        bson_iterator_from_buffer(&mit, ctx->mbuf);
        bt = bson_find_fieldpath_value2(fpath, fpathlen, &mit);
        if (bt == BSON_EOO || (ctx->expandall && bt != BSON_ARRAY)) {
            bson_append_field_from_iterator(it, ctx->bsout);
            return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
        }
        if (ctx->mfields > 0) {
            --ctx->mfields;
        }
        //Find and merge
        bson_iterator ait;
        bson_iterator_subiterator(it, &ait);
        bson_append_start_array(ctx->bsout, key);
        bool found = false;
        int c = 0;
        if (ctx->expandall) { //Set of array elements to add
            while ((bt = bson_iterator_next(&ait)) != BSON_EOO) { //Flush current array
                bson_append_field_from_iterator(&ait, ctx->bsout);
                ++c;
            }
            //Iterate over set to add
            bson_iterator mitsub;
            bson_iterator_subiterator(&mit, &mitsub); //mit has BSON_ARRAY type
            while ((bt = bson_iterator_next(&mitsub)) != BSON_EOO) {
                found = false;
                bson_iterator_subiterator(it, &ait); //Rewind main array iterator
                while ((bt = bson_iterator_next(&ait)) != BSON_EOO) {
                    if (!bson_compare_it_current(&ait, &mitsub)) {
                        found = true;
                        break;
                    }
                }
                if (!found) { //Append missing element
                    char kbuf[TCNUMBUFSIZ];
                    bson_numstrn(kbuf, TCNUMBUFSIZ, c++);
                    bson_append_field_from_iterator2(kbuf, &mitsub, ctx->bsout);
                    ctx->duty = true;
                }
            }
        } else { //Single element to add
            while ((bt = bson_iterator_next(&ait)) != BSON_EOO) {
                if (!found && !bson_compare_it_current(&ait, &mit)) {
                    found = true;
                }
                bson_append_field_from_iterator(&ait, ctx->bsout);
                ++c;
            }
            if (!found) { //uppend missing element into array
                char kbuf[TCNUMBUFSIZ];
                bson_numstrn(kbuf, TCNUMBUFSIZ, c);
                bson_append_field_from_iterator2(kbuf, &mit, ctx->bsout);
                ctx->duty = true;
            }
        }
        bson_append_finish_array(ctx->bsout);
        return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
    }
    //bt is BSON_OBJECT
    if (!after) {
        bson_append_start_object(ctx->bsout, key);
    } else { //after
        bson_append_finish_object(ctx->bsout);
    }
    return (BSON_VCMD_OK);

}

EJDB_EXPORT int bson_merge_array_sets(const void *mbuf, const void *inbuf, bool pull, bool expandall, bson *bsout) {
    assert(mbuf && inbuf && bsout);
    if (bsout->finished) {
        return BSON_ERROR;
    }
    BSON_MASETS_CTX ctx = {
        .bsout = bsout,
        .mbuf = mbuf,
        .mfields = 0,
        .duty = false,
        .expandall = expandall,
        .ecode = BSON_OK
    };
    bson_type bt, bt2;
    bson_iterator it, it2;
    bson_iterator_from_buffer(&it, mbuf);
    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        if (expandall && bt != BSON_ARRAY) {
            continue;
        }
        ctx.mfields++;
    }
    bson_iterator_from_buffer(&it, inbuf);
    if (pull) {
        bson_visit_fields(&it, BSON_TRAVERSE_ARRAYS_EXCLUDED, bson_merge_array_sets_pull_tf, &ctx);
    } else {
        bson_visit_fields(&it, BSON_TRAVERSE_ARRAYS_EXCLUDED, bson_merge_array_sets_tf, &ctx);
    }
    if (ctx.mfields == 0 || pull) {
        return ctx.ecode;
    }
    //Append missing arrays fields
    bson_iterator_from_buffer(&it, mbuf);
    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        const char *fpath = bson_iterator_key(&it);
        bson_iterator_from_buffer(&it2, inbuf);
        bt2 = bson_find_fieldpath_value(fpath, &it2);
        if (bt2 != BSON_EOO) continue;
        int i = 0;
        int lvl = 0;
        const char *pdp = fpath;
        while (*(fpath + i) != '\0') {
            for (; *(fpath + i) != '\0' && *(fpath + i) != '.'; ++i);
            bson_iterator_from_buffer(&it2, inbuf);
            bt2 = bson_find_fieldpath_value2(fpath, i, &it2);
            if (bt2 == BSON_EOO) {
                if (*(fpath + i) == '\0') { //EOF
                    assert((fpath + i) - pdp > 0);
                    bson_append_start_array2(bsout, pdp, (fpath + i) - pdp);
                    bson_append_field_from_iterator2("0", &it, bsout);
                    bson_append_finish_array(bsout);
                    break;
                } else {
                    ++lvl;
                    assert((fpath + i) - pdp > 0);
                    bson_append_start_object2(bsout, pdp, (fpath + i) - pdp);
                }
            } else if (bt2 != BSON_OBJECT) {
                break;
            }
            pdp = (fpath + i);
            while (*pdp == '.') {
                ++pdp;
                ++i;
            }
        }
        for (; lvl > 0; --lvl) {
            bson_append_finish_object(bsout);
        }
    }
    return ctx.ecode;
}
