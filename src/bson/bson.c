/* bson.c */

/*    Copyright 2009, 2010 10gen Inc.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include "bson.h"
#include "encoding.h"
#include "myconf.h"
#include "tcutil.h"

#ifdef _MYBIGEND
#define bson_little_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_little_endian32(out, in) ( bson_swap_endian32(out, in) )
#define bson_big_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_big_endian32(out, in) ( memcpy(out, in, 4) )
#else
#define bson_little_endian64(out, in) ( memcpy(out, in, 8) )
#define bson_little_endian32(out, in) ( memcpy(out, in, 4) )
#define bson_big_endian64(out, in) ( bson_swap_endian64(out, in) )
#define bson_big_endian32(out, in) ( bson_swap_endian32(out, in) )
#endif

const int initialBufferSize = 128;

#ifndef MIN
#define        MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define        MAX(a,b) (((a)>(b))?(a):(b))
#endif

/* only need one of these */
static const int zero = 0;

extern void *(*bson_malloc_func)(size_t);
extern void *(*bson_realloc_func)(void *, size_t);
extern void ( *bson_free_func)(void *);
extern bson_printf_func bson_errprintf;

/* Custom standard function pointers. */
void *(*bson_malloc_func)(size_t) = MYMALLOC;
void *(*bson_realloc_func)(void *, size_t) = MYREALLOC;
void ( *bson_free_func)(void *) = MYFREE;

static int _bson_errprintf(const char *, ...);
bson_printf_func bson_errprintf = _bson_errprintf;

/* ObjectId fuzz functions. */
static int ( *oid_fuzz_func)(void) = NULL;
static int ( *oid_inc_func)(void) = NULL;

const char* bson_first_errormsg(bson *b) {
    if (b->errstr) {
        return b->errstr;
    }
    if (b->err & BSON_FIELD_HAS_DOT) {
        return "BSON key contains '.' character";
    } else if (b->err & BSON_FIELD_INIT_DOLLAR) {
        return "BSON key starts with '$' character";
    } else if (b->err & BSON_ALREADY_FINISHED) {
        return "Trying to modify a finished BSON object";
    } else if (b->err & BSON_NOT_UTF8) {
        return "A key or a string is not valid UTF-8";
    } else if (b->err & BSON_NOT_FINISHED) {
        return "BSON not finished";
    }
    return "Unspecified BSON error";
}

void bson_reset(bson *b) {
    b->finished = 0;
    b->stackPos = 0;
    b->err = 0;
    b->errstr = NULL;
    b->flags = 0;
}

static bson_bool_t bson_isnumstr(const char *str, int len);
static void bson_append_fpath_from_iterator(const char *fpath, const bson_iterator *from, bson *into);
static const char *bson_iterator_value2(const bson_iterator *i, int *klen);

/* ----------------------------
   READING
   ------------------------------ */

bson* bson_create(void) {
    return (bson*) bson_malloc(sizeof (bson));
}

void bson_dispose(bson* b) {
    bson_free(b);
}

bson *bson_empty(bson *obj) {
    static char *data = "\005\0\0\0\0";
    bson_init_data(obj, data);
    obj->finished = 1;
    obj->err = 0;
    obj->errstr = NULL;
    obj->stackPos = 0;
    obj->flags = 0;
    return obj;
}

int bson_copy(bson *out, const bson *in) {
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

int bson_init_finished_data(bson *b, const char *data) {
    bson_init_data(b, (char*) data);
    bson_reset(b);
    b->finished = 1;
    return BSON_OK;
}

int bson_size(const bson *b) {
    int i;
    if (!b || !b->data)
        return 0;
    bson_little_endian32(&i, b->data);
    return i;
}

int bson_size2(const void *bsdata) {
    int i;
    if (!bsdata)
        return 0;
    bson_little_endian32(&i, bsdata);
    return i;
}

int bson_buffer_size(const bson *b) {
    return (b->cur - b->data + 1);
}

const char *bson_data(const bson *b) {
    return (const char *) b->data;
}

const char* bson_data2(const bson *b, int *bsize) {
    *bsize = bson_size(b);
    return b->data;
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

void bson_oid_from_string(bson_oid_t *oid, const char *str) {
    int i;
    for (i = 0; i < 12; i++) {
        oid->bytes[i] = (hexbyte(str[2 * i]) << 4) | hexbyte(str[2 * i + 1]);
    }
}

void bson_oid_to_string(const bson_oid_t *oid, char *str) {
    static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    int i;
    for (i = 0; i < 12; i++) {
        str[2 * i] = hex[(oid->bytes[i] & 0xf0) >> 4];
        str[2 * i + 1] = hex[ oid->bytes[i] & 0x0f ];
    }
    str[24] = '\0';
}

void bson_set_oid_fuzz(int ( *func)(void)) {
    oid_fuzz_func = func;
}

void bson_set_oid_inc(int ( *func)(void)) {
    oid_inc_func = func;
}

void bson_oid_gen(bson_oid_t *oid) {
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

time_t bson_oid_generated_time(bson_oid_t *oid) {
    time_t out = 0;
    bson_big_endian32(&out, &oid->ints[0]);
    return out;
}

void bson_print_raw(const char *data, int depth) {
    FILE *f = stdout;
    bson_iterator i;
    const char *key;
    int temp;
    bson_timestamp_t ts;
    char oidhex[25];
    bson scope;
    BSON_ITERATOR_FROM_BUFFER(&i, data);

    while (bson_iterator_next(&i)) {
        bson_type t = BSON_ITERATOR_TYPE(&i);
        if (t == 0)
            break;
        key = BSON_ITERATOR_KEY(&i);

        for (temp = 0; temp <= depth; temp++)
            fprintf(f, "\t");
        fprintf(f, "%s : %d \t ", key, t);
        switch (t) {
            case BSON_DOUBLE:
                fprintf(f, "%f", bson_iterator_double(&i));
                break;
            case BSON_STRING:
                fprintf(f, "%s", bson_iterator_string(&i));
                break;
            case BSON_SYMBOL:
                fprintf(f, "SYMBOL: %s", bson_iterator_string(&i));
                break;
            case BSON_OID:
                bson_oid_to_string(bson_iterator_oid(&i), oidhex);
                fprintf(f, "%s", oidhex);
                break;
            case BSON_BOOL:
                fprintf(f, "%s", bson_iterator_bool(&i) ? "true" : "false");
                break;
            case BSON_DATE:
                fprintf(f, "%" PRId64, bson_iterator_date(&i));
                break;
            case BSON_BINDATA:
                fprintf(f, "BSON_BINDATA");
                break;
            case BSON_UNDEFINED:
                fprintf(f, "BSON_UNDEFINED");
                break;
            case BSON_NULL:
                fprintf(f, "BSON_NULL");
                break;
            case BSON_REGEX:
                fprintf(f, "BSON_REGEX: %s", bson_iterator_regex(&i));
                break;
            case BSON_CODE:
                fprintf(f, "BSON_CODE: %s", bson_iterator_code(&i));
                break;
            case BSON_CODEWSCOPE:
                fprintf(f, "BSON_CODE_W_SCOPE: %s", bson_iterator_code(&i));
                /* bson_init( &scope ); */ /* review - stepped on by bson_iterator_code_scope? */
                bson_iterator_code_scope(&i, &scope);
                fprintf(f, "\n\t SCOPE: ");
                bson_print_raw((const char*) &scope, 0);
                /* bson_destroy( &scope ); */ /* review - causes free error */
                break;
            case BSON_INT:
                fprintf(f, "%d", bson_iterator_int(&i));
                break;
            case BSON_LONG:
                fprintf(f, "%" PRId64 "", (uint64_t) bson_iterator_long(&i));
                break;
            case BSON_TIMESTAMP:
                ts = bson_iterator_timestamp(&i);
                fprintf(f, "i: %d, t: %d", ts.i, ts.t);
                break;
            case BSON_OBJECT:
            case BSON_ARRAY:
                fprintf(f, "\n");
                bson_print_raw(bson_iterator_value(&i), depth + 1);
                break;
            default:
                bson_errprintf("can't print type : %d\n", t);
        }
        fprintf(f, "\n");
    }
}

/* ----------------------------
   ITERATOR
   ------------------------------ */

bson_iterator* bson_iterator_create(void) {
    return (bson_iterator*) malloc(sizeof ( bson_iterator));
}

void bson_iterator_dispose(bson_iterator* i) {
    free(i);
}

void bson_iterator_init(bson_iterator *i, const bson *b) {
    i->cur = b->data + 4;
    i->first = 1;
}

void bson_iterator_from_buffer(bson_iterator *i, const char *buffer) {
    i->cur = buffer + 4;
    i->first = 1;
}

bson_type bson_find(bson_iterator *it, const bson *obj, const char *name) {
    BSON_ITERATOR_INIT(it, (bson *) obj);
    while (bson_iterator_next(it)) {
        if (strcmp(name, BSON_ITERATOR_KEY(it)) == 0)
            break;
    }
    return BSON_ITERATOR_TYPE(it);
}

bson_type bson_find_from_buffer(bson_iterator *it, const char *buffer, const char *name) {
    BSON_ITERATOR_FROM_BUFFER(it, buffer);
    while (bson_iterator_next(it)) {
        if (strcmp(name, BSON_ITERATOR_KEY(it)) == 0)
            break;
    }
    return BSON_ITERATOR_TYPE(it);
}

static void bson_visit_fields_impl(bson_traverse_flags_t flags, char* pstack, int curr, bson_iterator *it, BSONVISITOR visitor, void *op) {
    int klen = 0;
    bson_type t;
    bson_visitor_cmd_t vcmd = 0;
    while (!(vcmd & BSON_VCMD_TERMINATE) && (t = bson_iterator_next(it)) != BSON_EOO) {
        const char* key = BSON_ITERATOR_KEY(it);
        klen = strlen(key);
        if (curr + klen > BSON_MAX_FPATH_LEN) {
            continue;
        }
        //PUSH
        if (curr > 0) { //add leading dot
            *(pstack + curr) = '.';
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
            BSON_ITERATOR_SUBITERATOR(it, &sit);
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

void bson_visit_fields(bson_iterator *it, bson_traverse_flags_t flags, BSONVISITOR visitor, void *op) {
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
        const char* key = BSON_ITERATOR_KEY(it);
        klen = strlen(key);
        if (curr + klen > fplen) {
            continue;
        }
        //PUSH
        if (curr > 0) { //add leading dot            
            *(pstack + curr) = '.';
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
            BSON_ITERATOR_SUBITERATOR(it, &sit);
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

bson_type bson_find_fieldpath_value(const char *fpath, bson_iterator *it) {
    return bson_find_fieldpath_value2(fpath, strlen(fpath), it);
}

bson_type bson_find_fieldpath_value2(const char *fpath, int fplen, bson_iterator *it) {
    FFPCTX ffctx = {
        .fpath = fpath,
        .fplen = fplen,
        .input = it,
        .stopnestedarr = false,
        .stopos = 0,
        .mpos = -1,
        .dpos = -1
    };
    return bson_find_fieldpath_value3(&ffctx);
}

bson_type bson_find_fieldpath_value3(FFPCTX* ffctx) {
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

bson_bool_t bson_iterator_more(const bson_iterator *i) {
    return *(i->cur);
}

bson_type bson_iterator_next(bson_iterator *i) {
    int ds, out, klen = 0;
    if (i->first) {
        i->first = 0;
        return (bson_type) (*i->cur);
    }
    switch (BSON_ITERATOR_TYPE(i)) {
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
            bson_little_endian32(&out, bson_iterator_value2(i, &klen));
            ds = 4 + out;
            break;
        case BSON_BINDATA:
            bson_little_endian32(&out, bson_iterator_value2(i, &klen));
            ds = 5 + out;
            break;
        case BSON_OBJECT:
        case BSON_ARRAY:
        case BSON_CODEWSCOPE:
            bson_little_endian32(&out, bson_iterator_value2(i, &klen));
            ds = out;
            break;
        case BSON_DBREF:
            bson_little_endian32(&out, bson_iterator_value2(i, &klen));
            ds = 4 + 12 + out;
            break;
        case BSON_REGEX:
        {
            const char *s = bson_iterator_value2(i, &klen);
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
    if (klen == 0) {
        for (; *(i->cur + 1 + klen) != '\0'; ++klen);
    }
    i->cur += (1 + klen + 1 + ds);
    return (bson_type) (*i->cur);
}

bson_type bson_iterator_type(const bson_iterator *i) {
    return (bson_type) i->cur[0];
}

const char *bson_iterator_key(const bson_iterator *i) {
    return i->cur + 1;
}

const char *bson_iterator_value(const bson_iterator *i) {
    int len = 0;
    const char *t = i->cur + 1;
    for (; *(t + len) != '\0'; ++len);
    t += len + 1;
    return t;
}

/* types */

int bson_iterator_int_raw(const bson_iterator *i) {
    int out;
    bson_little_endian32(&out, bson_iterator_value(i));
    return out;
}

double bson_iterator_double_raw(const bson_iterator *i) {
    double out;
    bson_little_endian64(&out, bson_iterator_value(i));
    return out;
}

int64_t bson_iterator_long_raw(const bson_iterator *i) {
    int64_t out;
    bson_little_endian64(&out, bson_iterator_value(i));
    return out;
}

bson_bool_t bson_iterator_bool_raw(const bson_iterator *i) {
    return bson_iterator_value(i)[0];
}

bson_oid_t *bson_iterator_oid(const bson_iterator *i) {
    return (bson_oid_t *) bson_iterator_value(i);
}

int bson_iterator_int(const bson_iterator *i) {
    switch (BSON_ITERATOR_TYPE(i)) {
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

double bson_iterator_double(const bson_iterator *i) {
    switch (BSON_ITERATOR_TYPE(i)) {
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

int64_t bson_iterator_long(const bson_iterator *i) {
    switch (BSON_ITERATOR_TYPE(i)) {
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
    switch (BSON_ITERATOR_TYPE(i)) {
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

bson_timestamp_t bson_iterator_timestamp(const bson_iterator *i) {
    bson_timestamp_t ts;
    bson_little_endian32(&(ts.i), bson_iterator_value(i));
    bson_little_endian32(&(ts.t), bson_iterator_value(i) + 4);
    return ts;
}

int bson_iterator_timestamp_time(const bson_iterator *i) {
    int time;
    bson_little_endian32(&time, bson_iterator_value(i) + 4);
    return time;
}

int bson_iterator_timestamp_increment(const bson_iterator *i) {
    int increment;
    bson_little_endian32(&increment, bson_iterator_value(i));
    return increment;
}

bson_bool_t bson_iterator_bool(const bson_iterator *i) {
    switch (BSON_ITERATOR_TYPE(i)) {
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

const char *bson_iterator_string(const bson_iterator *i) {
    switch (BSON_ITERATOR_TYPE(i)) {
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

const char *bson_iterator_code(const bson_iterator *i) {
    switch (BSON_ITERATOR_TYPE(i)) {
        case BSON_STRING:
        case BSON_CODE:
            return bson_iterator_value(i) + 4;
        case BSON_CODEWSCOPE:
            return bson_iterator_value(i) + 8;
        default:
            return NULL;
    }
}

void bson_iterator_code_scope(const bson_iterator *i, bson *scope) {
    if (BSON_ITERATOR_TYPE(i) == BSON_CODEWSCOPE) {
        int code_len;
        bson_little_endian32(&code_len, bson_iterator_value(i) + 4);
        bson_init_data(scope, (void *) (bson_iterator_value(i) + 8 + code_len));
        bson_reset(scope);
        scope->finished = 1;
    } else {
        bson_empty(scope);
    }
}

bson_date_t bson_iterator_date(const bson_iterator *i) {
    return bson_iterator_long_raw(i);
}

time_t bson_iterator_time_t(const bson_iterator *i) {
    return bson_iterator_date(i) / 1000;
}

int bson_iterator_bin_len(const bson_iterator *i) {
    return ( bson_iterator_bin_type(i) == BSON_BIN_BINARY_OLD)
            ? bson_iterator_int_raw(i) - 4
            : bson_iterator_int_raw(i);
}

char bson_iterator_bin_type(const bson_iterator *i) {
    return bson_iterator_value(i)[4];
}

const char *bson_iterator_bin_data(const bson_iterator *i) {
    return ( bson_iterator_bin_type(i) == BSON_BIN_BINARY_OLD)
            ? bson_iterator_value(i) + 9
            : bson_iterator_value(i) + 5;
}

const char *bson_iterator_regex(const bson_iterator *i) {
    return bson_iterator_value(i);
}

const char *bson_iterator_regex_opts(const bson_iterator *i) {
    const char *p = bson_iterator_value(i);
    return p + strlen(p) + 1;

}

void bson_iterator_subobject(const bson_iterator *i, bson *sub) {
    bson_init_data(sub, (char *) bson_iterator_value(i));
    bson_reset(sub);
    sub->finished = 1;
}

void bson_iterator_subiterator(const bson_iterator *i, bson_iterator *sub) {
    BSON_ITERATOR_FROM_BUFFER(sub, bson_iterator_value(i));
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

void bson_init(bson *b) {
    _bson_init_size(b, initialBufferSize);
}

void bson_init_as_query(bson *b) {
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

void bson_append(bson *b, const void *data, int len) {
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

int bson_finish(bson *b) {
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

void bson_destroy(bson *b) {
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

void bson_del(bson *b) {
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

int bson_append_int(bson *b, const char *name, const int i) {
    if (bson_append_estart(b, BSON_INT, name, 4) == BSON_ERROR)
        return BSON_ERROR;
    bson_append32(b, &i);
    return BSON_OK;
}

int bson_append_long(bson *b, const char *name, const int64_t i) {
    if (bson_append_estart(b, BSON_LONG, name, 8) == BSON_ERROR)
        return BSON_ERROR;
    bson_append64(b, &i);
    return BSON_OK;
}

int bson_append_double(bson *b, const char *name, const double d) {
    if (bson_append_estart(b, BSON_DOUBLE, name, 8) == BSON_ERROR)
        return BSON_ERROR;
    bson_append64(b, &d);
    return BSON_OK;
}

int bson_append_bool(bson *b, const char *name, const bson_bool_t i) {
    if (bson_append_estart(b, BSON_BOOL, name, 1) == BSON_ERROR)
        return BSON_ERROR;
    bson_append_byte(b, i != 0);
    return BSON_OK;
}

int bson_append_null(bson *b, const char *name) {
    if (bson_append_estart(b, BSON_NULL, name, 0) == BSON_ERROR)
        return BSON_ERROR;
    return BSON_OK;
}

int bson_append_undefined(bson *b, const char *name) {
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

int bson_append_string(bson *b, const char *name, const char *value) {
    return bson_append_string_base(b, name, value, strlen(value), BSON_STRING);
}

int bson_append_symbol(bson *b, const char *name, const char *value) {
    return bson_append_string_base(b, name, value, strlen(value), BSON_SYMBOL);
}

int bson_append_code(bson *b, const char *name, const char *value) {
    return bson_append_string_base(b, name, value, strlen(value), BSON_CODE);
}

int bson_append_string_n(bson *b, const char *name, const char *value, int len) {
    return bson_append_string_base(b, name, value, len, BSON_STRING);
}

int bson_append_symbol_n(bson *b, const char *name, const char *value, int len) {
    return bson_append_string_base(b, name, value, len, BSON_SYMBOL);
}

int bson_append_code_n(bson *b, const char *name, const char *value, int len) {
    return bson_append_string_base(b, name, value, len, BSON_CODE);
}

int bson_append_code_w_scope_n(bson *b, const char *name,
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

int bson_append_code_w_scope(bson *b, const char *name, const char *code, const bson *scope) {
    return bson_append_code_w_scope_n(b, name, code, strlen(code), scope);
}

int bson_append_binary(bson *b, const char *name, char type, const char *str, int len) {
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

int bson_append_oid(bson *b, const char *name, const bson_oid_t *oid) {
    if (bson_append_estart(b, BSON_OID, name, 12) == BSON_ERROR)
        return BSON_ERROR;
    bson_append(b, oid, 12);
    return BSON_OK;
}

int bson_append_new_oid(bson *b, const char *name) {
    bson_oid_t oid;
    bson_oid_gen(&oid);
    return bson_append_oid(b, name, &oid);
}

int bson_append_regex(bson *b, const char *name, const char *pattern, const char *opts) {
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

int bson_append_bson(bson *b, const char *name, const bson *bson) {
    if (!bson) return BSON_ERROR;
    if (bson_append_estart(b, BSON_OBJECT, name, bson_size(bson)) == BSON_ERROR)
        return BSON_ERROR;
    bson_append(b, bson->data, bson_size(bson));
    return BSON_OK;
}

int bson_append_element(bson *b, const char *name_or_null, const bson_iterator *elem) {
    bson_iterator next = *elem;
    int size;

    bson_iterator_next(&next);
    size = next.cur - elem->cur;

    if (name_or_null == NULL) {
        if (bson_ensure_space(b, size) == BSON_ERROR)
            return BSON_ERROR;
        bson_append(b, elem->cur, size);
    } else {
        int data_size = size - 2 - strlen(BSON_ITERATOR_KEY(elem));
        bson_append_estart(b, elem->cur[0], name_or_null, data_size);
        bson_append(b, bson_iterator_value(elem), data_size);
    }

    return BSON_OK;
}

int bson_append_timestamp(bson *b, const char *name, bson_timestamp_t *ts) {
    if (bson_append_estart(b, BSON_TIMESTAMP, name, 8) == BSON_ERROR) return BSON_ERROR;

    bson_append32(b, &(ts->i));
    bson_append32(b, &(ts->t));

    return BSON_OK;
}

int bson_append_timestamp2(bson *b, const char *name, int time, int increment) {
    if (bson_append_estart(b, BSON_TIMESTAMP, name, 8) == BSON_ERROR) return BSON_ERROR;

    bson_append32(b, &increment);
    bson_append32(b, &time);
    return BSON_OK;
}

int bson_append_date(bson *b, const char *name, bson_date_t millis) {
    if (bson_append_estart(b, BSON_DATE, name, 8) == BSON_ERROR) return BSON_ERROR;
    bson_append64(b, &millis);
    return BSON_OK;
}

int bson_append_time_t(bson *b, const char *name, time_t secs) {
    return bson_append_date(b, name, (bson_date_t) secs * 1000);
}

int bson_append_start_object(bson *b, const char *name) {
    if (bson_append_estart(b, BSON_OBJECT, name, 5) == BSON_ERROR) return BSON_ERROR;
    b->stack[ b->stackPos++ ] = b->cur - b->data;
    bson_append32(b, &zero);
    return BSON_OK;
}

int bson_append_start_object2(bson *b, const char *name, int namelen) {
    if (bson_append_estart2(b, BSON_OBJECT, name, namelen, 5) == BSON_ERROR) return BSON_ERROR;
    b->stack[ b->stackPos++ ] = b->cur - b->data;
    bson_append32(b, &zero);
    return BSON_OK;
}

int bson_append_start_array(bson *b, const char *name) {
    return bson_append_start_array2(b, name, strlen(name));
}

int bson_append_start_array2(bson *b, const char *name, int namelen) {
    if (bson_append_estart2(b, BSON_ARRAY, name, namelen, 5) == BSON_ERROR) return BSON_ERROR;
    b->stack[ b->stackPos++ ] = b->cur - b->data;
    bson_append32(b, &zero);
    return BSON_OK;
}

int bson_append_finish_object(bson *b) {
    char *start;
    int i;
    if (bson_ensure_space(b, 1) == BSON_ERROR) return BSON_ERROR;
    bson_append_byte(b, 0);

    start = b->data + b->stack[ --b->stackPos ];
    i = b->cur - start;
    bson_little_endian32(start, &i);

    return BSON_OK;
}

double bson_int64_to_double(int64_t i64) {
    return (double) i64;
}

int bson_append_finish_array(bson *b) {
    return bson_append_finish_object(b);
}

/* Error handling and allocators. */

static bson_err_handler err_handler = NULL;

bson_err_handler set_bson_err_handler(bson_err_handler func) {
    bson_err_handler old = err_handler;
    err_handler = func;
    return old;
}

void bson_free(void *ptr) {
    bson_free_func(ptr);
}

void *bson_malloc(int size) {
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
    ret = vfprintf(stderr, format, ap);
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
    bson_errprintf("error: %s\n", msg);
}


/* Efficiently copy an integer to a string. */
extern const char bson_numstrs[1000][4];

void bson_numstr(char *str, int64_t i) {
    if (i >= 0 && i < 1000)
        memcpy(str, bson_numstrs[i], 4);
    else
        sprintf(str, "%" PRId64 "", (int64_t) i);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"

int bson_numstrn(char *str, int maxbuf, int64_t i) {
    if (i >= 0 && i < 1000 && maxbuf > 4) {
        memcpy(str, bson_numstrs[i], 4);
        return strlen(bson_numstrs[i]);
    } else {
        return snprintf(str, maxbuf, "%" PRId64 "", (int64_t) i);
    }
}
#pragma GCC diagnostic pop

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

void bson_swap_endian64(void *outp, const void *inp) {
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

void bson_swap_endian32(void *outp, const void *inp) {
    const char *in = (const char *) inp;
    char *out = (char *) outp;
    out[0] = in[3];
    out[1] = in[2];
    out[2] = in[1];
    out[3] = in[0];
}

static const char *bson_iterator_value2(const bson_iterator *i, int *klen) {
    const char *t = i->cur + 1;
    *klen = strlen(t);
    t += (*klen + 1);
    return t;
}

int bson_append_array_from_iterator(const char *key, bson_iterator *from, bson *into) {
    assert(key && from && into);
    bson_type bt;
    bson_append_start_array(into, key);
    while ((bt = bson_iterator_next(from)) != BSON_EOO) {
        bson_append_field_from_iterator(from, into);
    }
    bson_append_finish_array(into);
    return BSON_OK;
}

int bson_append_object_from_iterator(const char *key, bson_iterator *from, bson *into) {
    assert(key && from && into);
    bson_type bt;
    bson_append_start_object(into, key);
    while ((bt = bson_iterator_next(from)) != BSON_EOO) {
        bson_append_field_from_iterator(from, into);
    }
    bson_append_finish_object(into);
    return BSON_OK;
}

static void bson_append_fpath_from_iterator(const char *fpath, const bson_iterator *from, bson *into) {
    char key[BSON_MAX_FPATH_LEN + 1];
    int fplen = strlen(fpath);
    if (fplen >= BSON_MAX_FPATH_LEN) { //protect me silently
        return; //todo error?
    }
    const char *fp = fpath;
    int keylen = 0;
    int nl = 0; //nesting level
    while (fplen > 0) { //split fpath with '.' delim
        const char *rp = fp;
        const char *ep = fp + fplen;
        while (rp < ep) {
            if (*rp == '.') break;
            rp++;
        }
        keylen = (rp - fp);
        memcpy(key, fp, keylen);
        key[keylen] = '\0';
        rp++;
        fplen -= keylen + 1;
        if (fplen <= 0) { //last part of fp
            bson_append_field_from_iterator2(key, from, into);
            while (nl-- > 0) {
                bson_append_finish_object(into); //arrays are covered also
            }
        } else { //intermediate part
            nl++;
            bson_append_start_object2(into, key, keylen);
        }
        fp = rp;
    }
}

int bson_append_field_from_iterator2(const char *key, const bson_iterator *from, bson *into) {
    assert(key && from && into);
    bson_type t = BSON_ITERATOR_TYPE(from);
    if (t == BSON_EOO || into->finished) {
        return BSON_ERROR;
    }
    switch (t) {
        case BSON_STRING:
            bson_append_string(into, key, bson_iterator_string(from));
            break;
        case BSON_SYMBOL:
			bson_append_symbol(into, key, bson_iterator_string(from));
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
            BSON_ITERATOR_SUBITERATOR(from, &sit);
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
            BSON_ITERATOR_SUBITERATOR(from, &sit);
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

int bson_append_field_from_iterator(const bson_iterator *from, bson *into) {
    assert(from && into);
    return bson_append_field_from_iterator2(BSON_ITERATOR_KEY(from), from, into);
}

typedef struct {
    bson *bsout;
    TCMAP *mfields;
    const void *bsdata2; //bsdata to merge with
    int nstack; //nested object stack pos
    int matched; //number of matched merge fields
} _BSONMERGE3CTX;

static bson_visitor_cmd_t _bson_merge_fieldpaths_visitor(
        const char *ipath, int ipathlen, 
        const char *key, int keylen,
        const bson_iterator *it, 
        bool after, void *op) {
            
    _BSONMERGE3CTX *ctx = op;
    assert(ctx && ctx->bsout && ctx->mfields && ipath && key && it && op);
    const void *buf;
    const char *mpath;
    int bufsz;
    bson_type bt = BSON_ITERATOR_TYPE(it);
    buf = (TCMAPRNUM(ctx->mfields) == 0 || after) ? NULL : tcmapget(ctx->mfields, ipath, ipathlen, &bufsz);
    if (buf) {
        bson_iterator it2;
        BSON_ITERATOR_FROM_BUFFER(&it2, ctx->bsdata2);
        off_t it2off;
        assert(bufsz == sizeof (it2off));
        memcpy(&it2off, buf, sizeof (it2off));
        assert(it2off >= 0);
        it2.cur = it2.cur + it2off;
        it2.first = (it2off == 0);
        tcmapout(ctx->mfields, ipath, ipathlen);
        bson_append_field_from_iterator2(key, &it2, ctx->bsout);
        return (BSON_VCMD_SKIP_AFTER | BSON_VCMD_SKIP_NESTED);
    } else {
        if (bt == BSON_OBJECT || bt == BSON_ARRAY) {
            if (!after) {
                ctx->nstack++;
                if (bt == BSON_OBJECT) {
                    bson_append_start_object2(ctx->bsout, key, keylen);
                } else if (bt == BSON_ARRAY) {
                    bson_append_start_array2(ctx->bsout, key, keylen);
                }
                return BSON_VCMD_OK;
            } else {
                if (ctx->nstack > 0) {
                    //do we have something to add into end of nested object?
                    tcmapiterinit(ctx->mfields);
                    int mpathlen = 0;
                    while ((mpath = tcmapiternext(ctx->mfields, &mpathlen)) != NULL) {
                        int i = 0;
                        for (; i < ipathlen && *(mpath + i) == *(ipath + i); ++i);
                        if (i == ipathlen && *(mpath + i) == '.' && *(mpath + i + 1) != '\0') { //ipath prefixed
                            bson_iterator it2;
                            BSON_ITERATOR_FROM_BUFFER(&it2, ctx->bsdata2);
                            buf = tcmapget(ctx->mfields, mpath, mpathlen, &bufsz);
                            off_t it2off;
                            assert(bufsz == sizeof (it2off));
                            memcpy(&it2off, buf, sizeof (it2off));
                            assert(it2off >= 0);
                            it2.cur = it2.cur + it2off;
                            it2.first = (it2off == 0);
                            bson_append_fpath_from_iterator(mpath + i + 1, &it2, ctx->bsout);
                            tcmapout(ctx->mfields, mpath, mpathlen);
                        }
                    }
                    ctx->nstack--;
                    if (bt == BSON_OBJECT) {
                        bson_append_finish_object(ctx->bsout);
                    } else if (bt == BSON_ARRAY) {
                        bson_append_finish_array(ctx->bsout);
                    }
                }
                return BSON_VCMD_OK;
            }
        } else {
            bson_append_field_from_iterator(it, ctx->bsout);
            return BSON_VCMD_SKIP_AFTER;
        }
    }
}

//merge with fpath support

int bson_merge_fieldpaths(const void *bsdata1, const void *bsdata2, bson *out) {
    assert(bsdata1 && bsdata2 && out);
    bson_iterator it1, it2;
    bson_type bt;
    BSON_ITERATOR_FROM_BUFFER(&it1, bsdata1);
    BSON_ITERATOR_FROM_BUFFER(&it2, bsdata2);
    const char *it2start = it2.cur;
    TCMAP *mfields = tcmapnew2(TCMAPTINYBNUM);
    _BSONMERGE3CTX ctx = {
        .bsout = out,
        .mfields = mfields,
        .bsdata2 = bsdata2,
        .matched = 0,
        .nstack = 0
    };
    //collect active fpaths
    while ((bt = bson_iterator_next(&it2)) != BSON_EOO) {
        const char* key = BSON_ITERATOR_KEY(&it2);
        off_t it2off = (it2.cur - it2start);
        tcmapput(mfields, key, strlen(key), &it2off, sizeof (it2off));
    }
    bson_visit_fields(&it1, 0, _bson_merge_fieldpaths_visitor, &ctx);
    assert(ctx.nstack == 0);
    if (TCMAPRNUM(mfields) == 0) { //all merge fields applied
        tcmapdel(mfields);
        return BSON_OK;
    }

    //apply the remaining merge fields
    tcmapiterinit(mfields);
    const char *fpath;
    int fplen;
    while ((fpath = tcmapiternext(mfields, &fplen)) != NULL) {
        BSON_ITERATOR_FROM_BUFFER(&it2, bsdata2);
        if (bson_find_fieldpath_value2(fpath, fplen, &it2) != BSON_EOO) {
            bson_append_fpath_from_iterator(fpath, &it2, out);
        }
    }
    tcmapdel(mfields);

    if (!out->err) {
        // check duplicate paths
        bson_finish(out);
        if (bson_check_duplicate_keys(out)) {
            bson bstmp;
            bson_copy(&bstmp, out);
            bson_destroy(out);
            bson_init(out);
            bson_fix_duplicate_keys(&bstmp, out);
            bson_destroy(&bstmp);
        }
    }
    return out->err;
}

int bson_merge2(const void *b1data, const void *b2data, bson_bool_t overwrite, bson *out) {
    bson_iterator it1, it2;
    bson_type bt1, bt2;

    BSON_ITERATOR_FROM_BUFFER(&it1, b1data);
    BSON_ITERATOR_FROM_BUFFER(&it2, b2data);
    //Append all fields in B1 overwriten by B2
    while ((bt1 = bson_iterator_next(&it1)) != BSON_EOO) {
        const char* k1 = BSON_ITERATOR_KEY(&it1);
        if (overwrite && strcmp(JDBIDKEYNAME, k1) && (bt2 = bson_find_from_buffer(&it2, b2data, k1)) != BSON_EOO) {
            bson_append_field_from_iterator(&it2, out);
        } else {
            bson_append_field_from_iterator(&it1, out);
        }
    }
    BSON_ITERATOR_FROM_BUFFER(&it1, b1data);
    BSON_ITERATOR_FROM_BUFFER(&it2, b2data);
    //Append all fields from B2 missing in B1
    while ((bt2 = bson_iterator_next(&it2)) != BSON_EOO) {
        const char* k2 = BSON_ITERATOR_KEY(&it2);
        if ((bt1 = bson_find_from_buffer(&it1, b1data, k2)) == BSON_EOO) {
            bson_append_field_from_iterator(&it2, out);
        }
    }
    return BSON_OK;
}

int bson_merge(const bson *b1, const bson *b2, bson_bool_t overwrite, bson *out) {
    assert(b1 && b2 && out);
    if (!b1->finished || !b2->finished || out->finished) {
        return BSON_ERROR;
    }
    return bson_merge2(bson_data(b1), bson_data(b2), overwrite, out);
}

int bson_merge_recursive2(const void *b1data, const void *b2data, bson_bool_t overwrite, bson *out) {
    bson_iterator it1, it2;
    bson_type bt1, bt2;
    if (out->finished) {
        return BSON_ERROR;
    }
    BSON_ITERATOR_FROM_BUFFER(&it1, b1data);
    BSON_ITERATOR_FROM_BUFFER(&it2, b2data);
    //Append all fields in B1 merging with fields in B2 (for nested objects & arrays)
    while ((bt1 = bson_iterator_next(&it1)) != BSON_EOO) {
        const char* k1 = BSON_ITERATOR_KEY(&it1);
        bt2 = bson_find_from_buffer(&it2, b2data, k1);
        if (bt1 == BSON_OBJECT && bt2 == BSON_OBJECT) {
            int res;
            bson_append_start_object(out, k1);
            if ((res = bson_merge_recursive2(bson_iterator_value(&it1), bson_iterator_value(&it2), overwrite, out)) != BSON_OK) {
                return res;
            }
            bson_append_finish_object(out);
        } else if (bt1 == BSON_ARRAY && bt2 == BSON_ARRAY) {
            int c = 0;
            bson_iterator sit;
            bson_type sbt;

            bson_append_start_array(out, k1);
            BSON_ITERATOR_SUBITERATOR(&it1, &sit);
            while ((sbt = bson_iterator_next(&sit)) != BSON_EOO) {
                bson_append_field_from_iterator(&sit, out);
                ++c;
            }
            char kbuf[TCNUMBUFSIZ];
            BSON_ITERATOR_SUBITERATOR(&it2, &sit);
            while ((sbt = bson_iterator_next(&sit)) != BSON_EOO) {
                bson_numstrn(kbuf, TCNUMBUFSIZ, c++);
                bson_append_field_from_iterator2(kbuf, &sit, out);
            }

            bson_append_finish_array(out);
        } else if (overwrite && strcmp(JDBIDKEYNAME, k1) && bt2 != BSON_EOO) {
            bson_append_field_from_iterator(&it2, out);
        } else {
            bson_append_field_from_iterator(&it1, out);
        }
    }

    BSON_ITERATOR_FROM_BUFFER(&it1, b1data);
    BSON_ITERATOR_FROM_BUFFER(&it2, b2data);
    //Append all fields from B2 missing in B1
    while ((bt2 = bson_iterator_next(&it2)) != BSON_EOO) {
        const char* k2 = BSON_ITERATOR_KEY(&it2);
        if ((bt1 = bson_find_from_buffer(&it1, b1data, k2)) == BSON_EOO) {
            bson_append_field_from_iterator(&it2, out);
        }
    }
    return BSON_OK;
}

int bson_merge_recursive(const bson *b1, const bson *b2, bson_bool_t overwrite, bson *out) {
    assert(b1 && b2 && out);
    if (!b1->finished || !b2->finished || out->finished) {
        return BSON_ERROR;
    }
    return bson_merge_recursive2(bson_data(b1), bson_data(b2), overwrite, out);
}

static bson_bool_t _bson_check_duplicate_keys(bson_iterator *it) {
    bson_iterator it2;
    bson_type bt, bt2;
    while ((bt = bson_iterator_next(it)) != BSON_EOO) {
        BSON_ITERATOR_CLONE(it, &it2);
        while((bt2 = bson_iterator_next(&it2)) != BSON_EOO) {
            if (!strcmp(BSON_ITERATOR_KEY(it), BSON_ITERATOR_KEY(&it2))) {
                return true;
            }
        }
        if (bt == BSON_OBJECT || bt == BSON_ARRAY) {
            BSON_ITERATOR_SUBITERATOR(it, &it2);
            if (_bson_check_duplicate_keys(&it2)) {
                return true;
            }
        }
    }

    return false;
}

bson_bool_t bson_check_duplicate_keys(const bson *bs) {
    bson_iterator it;
    BSON_ITERATOR_INIT(&it, bs);
    return _bson_check_duplicate_keys(&it);
}

static void _bson_fix_duplicate_keys(bson_iterator *it, bson *bso) {
    bson_iterator it2;
    bson_type bt, bt2;

    TCMAP *keys = tcmapnew();
    while((bt = bson_iterator_next(it)) != BSON_EOO) {
        if (NULL != tcmapget2(keys, BSON_ITERATOR_KEY(it))) {
            continue;
        }
        tcmapput2(keys, BSON_ITERATOR_KEY(it), BSON_ITERATOR_KEY(it));

        TCLIST *dups = tclistnew();
        off_t itoff  = 0;
        tclistpush(dups, &itoff, sizeof(itoff));

        BSON_ITERATOR_CLONE(it, &it2);
        while((bt2 = bson_iterator_next(&it2)) != BSON_EOO) {
            if (!strcmp(BSON_ITERATOR_KEY(it), BSON_ITERATOR_KEY(&it2))) {
                bt2 = BSON_ITERATOR_TYPE(&it2);
                if (bt != bt2 || (bt != BSON_OBJECT && bt != BSON_ARRAY)) {
                    tclistclear(dups);
                    bt = bt2;
                }
                itoff = it2.cur - it->cur;
                tclistpush(dups, &itoff, sizeof(itoff));
            }
        }

        const char *buf;
        int bufsz;

        buf = tclistval(dups, TCLISTNUM(dups) - 1, &bufsz);
        memcpy(&itoff, buf, sizeof(itoff));
        it2.cur = it->cur + itoff;
        it2.first = itoff == 0 ? it->first : 0;

        bt2 = BSON_ITERATOR_TYPE(&it2);
        if (bt2 == BSON_OBJECT) {
            bson bst;
            bson_init(&bst);
            int j = -1;
            while(++j < TCLISTNUM(dups)) {
                buf = tclistval(dups, j, &bufsz);
                memcpy(&itoff, buf, sizeof(itoff));
                it2.cur = it->cur + itoff;
                it2.first = itoff == 0 ? it->first : 0;

                bson_iterator sit;
                BSON_ITERATOR_SUBITERATOR(&it2, &sit);
                while(bson_iterator_next(&sit) != BSON_EOO){
                    bson_append_field_from_iterator(&sit, &bst);
                }
            }
            bson_finish(&bst);

            bson_append_start_object(bso, BSON_ITERATOR_KEY(it));
            BSON_ITERATOR_INIT(&it2, &bst);
            _bson_fix_duplicate_keys(&it2, bso);
            bson_append_finish_object(bso);
            bson_destroy(&bst);
        } else if (bt2 == BSON_ARRAY) {
            char ibuf[TCNUMBUFSIZ];
            memset(ibuf, '\0', TCNUMBUFSIZ);

            bson_append_start_array(bso, BSON_ITERATOR_KEY(it));
            int ind = 0;
            int j = -1;
            while(++j < TCLISTNUM(dups)) {
                buf = tclistval(dups, j, &bufsz);
                memcpy(&itoff, buf, sizeof(itoff));
                it2.cur = it->cur + itoff;
                it2.first = itoff == 0 ? it->first : 0;

                bson_iterator sit, sit2;
                bson_type sbt;
                BSON_ITERATOR_SUBITERATOR(&it2, &sit);
                while((sbt = bson_iterator_next(&sit)) != BSON_EOO) {
                    bson_numstrn(ibuf, TCNUMBUFSIZ, ind++);
                    if (sbt == BSON_OBJECT) {
                        bson_append_start_object(bso, ibuf);
                        BSON_ITERATOR_SUBITERATOR(&sit, &sit2);
                        _bson_fix_duplicate_keys(&sit2, bso);
                        bson_append_finish_object(bso);
                    } else if(sbt == BSON_ARRAY) {
                        bson_append_start_array(bso, ibuf);
                        BSON_ITERATOR_SUBITERATOR(&sit, &sit2);
                        _bson_fix_duplicate_keys(&sit2, bso);
                        bson_append_finish_array(bso);
                    } else {
                        bson_append_field_from_iterator2(ibuf, &sit, bso);
                    }
                }
            }
            bson_append_finish_array(bso);
        } else {
            bson_append_field_from_iterator(&it2, bso);
        }
        tclistdel(dups);
    }
    tcmapdel(keys);
}

void bson_fix_duplicate_keys(const bson *bsi, bson *bso) {
    bson_iterator it;

    BSON_ITERATOR_INIT(&it, bsi);
    _bson_fix_duplicate_keys(&it, bso);
}


typedef struct {
    int nstack; //nested object stack pos
    int matched; //number of matched include fields
    int astack; //nested array stack pos
    BSONSTRIPCTX *sctx;
} _BSONSTRIPVISITORCTX;

/* Discard excluded fields from BSON */
static bson_visitor_cmd_t _bsonstripvisitor_exclude(
        const char *ipath, int ipathlen, 
        const char *key, int keylen,
        const bson_iterator *it, 
        bool after, void *op) {
    
    _BSONSTRIPVISITORCTX *ictx = op;
    assert(ictx);
    BSONSTRIPCTX *sctx = ictx->sctx;
    assert(sctx && sctx->bsout && sctx->ifields && ipath && key && it && op);
    TCMAP *ifields = sctx->ifields;
    const void *buf;
    int bufsz;
    const char* ifpath;
    bson_type bt = BSON_ITERATOR_TYPE(it);

    buf = after ? NULL : tcmapget(ifields, ipath, ipathlen, &bufsz);
    if (!buf) {
        if (bt == BSON_OBJECT || bt == BSON_ARRAY) {
            if (!after) {
                tcmapiterinit(ifields); //check prefix
                while ((ifpath = tcmapiternext2(ifields)) != NULL) {
                    int i = 0;
                    for (; i < ipathlen && *(ifpath + i) == *(ipath + i); ++i);
                    if (i == ipathlen) { //ipath prefixes some exclude object field
                        ictx->nstack++;
                        if (bt == BSON_OBJECT) {
                            bson_append_start_object2(sctx->bsout, key, keylen);
                        } else if (bt == BSON_ARRAY) {
                            ictx->astack++;
                            bson_append_start_array2(sctx->bsout, key, keylen);
                        }
                        return (BSON_VCMD_OK);
                    }
                }
                bson_append_field_from_iterator(it, sctx->bsout);
                return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
            } else {
                if (ictx->nstack > 0) {
                    --ictx->nstack;
                    if (bt == BSON_OBJECT) {
                        bson_append_finish_object(sctx->bsout);
                    } else if (bt == BSON_ARRAY) {
                        --ictx->astack;
                        bson_append_finish_array(sctx->bsout);
                    }
                }
                return (BSON_VCMD_OK);
            }
        } else {
            bson_append_field_from_iterator(it, sctx->bsout);
            return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
        }
    } else {
        if (sctx->collector) {
            const char *k = NULL;
            char cpath[BSON_MAX_FPATH_LEN + 1];
            assert(ipathlen <= BSON_MAX_FPATH_LEN && !sctx->collector->finished);
            if (sctx->fkfields) {
                k = tcmapget(sctx->fkfields, ipath, ipathlen, &bufsz);
            }
            if (!k) {
                memcpy(cpath, ipath, ipathlen);
                cpath[ipathlen] = '\0'; 
                k = cpath;
            }
            bson_iterator cit = *it;
            bson_append_field_from_iterator2(k, &cit, sctx->collector);
        }
        if (!after && ictx->astack > 0 && bson_isnumstr(key, keylen)) {
            bson_append_undefined(sctx->bsout, key);
        }
        ictx->matched++;
    }
    return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
}

/* Accept only included fields into BSON */
static bson_visitor_cmd_t _bsonstripvisitor_include(
        const char *ipath, int ipathlen, 
        const char *key, int keylen,
        const bson_iterator *it, bool after, void *op) {
    
    _BSONSTRIPVISITORCTX *ictx = op;
    assert(ictx);
    BSONSTRIPCTX *sctx = ictx->sctx;
    assert(sctx && sctx->bsout && sctx->ifields && ipath && key && it && op);
    bson_visitor_cmd_t rv = BSON_VCMD_OK;
    TCMAP *ifields = sctx->ifields;
    const void *buf;
    const char* ifpath;
    int bufsz;

    const char *k = key;
    if (sctx->fkfields) { //find keys to override
        k = tcmapget(sctx->fkfields, ipath, ipathlen, &bufsz);
    }
    if (!k) {
        k = key;
    }
    bson_type bt = BSON_ITERATOR_TYPE(it);
    if (bt != BSON_OBJECT && bt != BSON_ARRAY) {
        if (after) { //simple primitive case
            return BSON_VCMD_OK;
        }
        buf = tcmapget(ifields, ipath, ipathlen, &bufsz);
        if (buf) {
            ictx->matched++;
            bson_append_field_from_iterator2(k, it, sctx->bsout);
        }
        return (BSON_VCMD_SKIP_AFTER);
    } else { //more complicated case
        if (!after) {
            buf = tcmapget(ifields, ipath, ipathlen, &bufsz);
            if (buf) { //field hitted
                bson_iterator cit = *it; //copy iterator
                bson_append_field_from_iterator(&cit, sctx->bsout);
                ictx->matched++;
                return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
            } else { //check prefix
                int onstack = ictx->nstack;
                tcmapiterinit(ifields);
                while ((ifpath = tcmapiternext2(ifields)) != NULL) {
                    int i = 0;
                    for (; i < ipathlen && *(ifpath + i) == *(ipath + i); ++i);
                    if (i == ipathlen) { //ipath prefixes some included field
                        ictx->nstack++;
                        if (bt == BSON_OBJECT) {
                            bson_append_start_object2(sctx->bsout, k, keylen);
                        } else if (bt == BSON_ARRAY) {
                            bson_append_start_array2(sctx->bsout, k, keylen);
                        } else {
                            assert(0);
                        }
                        break;
                    }
                }
                if (onstack == ictx->nstack) {
                    return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
                }
            }
        } else { //after
            if (ictx->nstack > 0) {
                --ictx->nstack;
                if (bt == BSON_OBJECT) {
                    bson_append_finish_object(sctx->bsout);
                } else if (bt == BSON_ARRAY) {
                    bson_append_finish_array(sctx->bsout);
                } else {
                    assert(0);
                }
            }
        }
    }

    if (ictx->nstack == 0 && ictx->matched == TCMAPRNUM(ifields)) {
        return BSON_VCMD_TERMINATE;
    }
    return rv;
}

int bson_strip(TCMAP *ifields, bool imode, const void *bsbuf, bson *bsout, int *matched) {
    BSONSTRIPCTX sctx = {
        .ifields = ifields,
        .imode = imode,
        .bsbuf = bsbuf,
        .bsout = bsout,
        .fkfields = NULL,
        .matched = 0
    };
    int rc = bson_strip2(&sctx);
    *matched = sctx.matched;
    return rc;
}

/* Include or exclude fpaths in the specified BSON and put resulting data into `bsout`. */
int bson_strip2(BSONSTRIPCTX *sctx) {
    assert(sctx && sctx->bsbuf && sctx->bsout);
    if (!sctx->ifields || sctx->bsout->finished) {
        return BSON_ERROR;
    }
    _BSONSTRIPVISITORCTX ictx = {
        .nstack = 0,
        .astack = 0,
        .matched = 0,
        .sctx = sctx
    };
    bson_iterator it;
    BSON_ITERATOR_FROM_BUFFER(&it, sctx->bsbuf);
    bson_visit_fields(&it, 0, (sctx->imode) ? _bsonstripvisitor_include : _bsonstripvisitor_exclude, &ictx);
    assert(ictx.nstack == 0);
    sctx->matched = ictx.matched;
    return bson_finish(sctx->bsout);
}

int bson_rename(TCMAP *fields, const void *bsbuf, bson *bsout, int *rencnt) {
    *rencnt = 0;
    if (TCMAPRNUM(fields) == 0) {
        return BSON_OK; //nothing to rename
    }
    
    int rv;
    bson res, collector;
    bson_init(&res);
    bson_init(&collector);
    
    BSONSTRIPCTX sctx = {
        .ifields = fields,
        .imode = false,
        .bsbuf = bsbuf,
        .bsout = &res,
        .collector = &collector,
        .fkfields = fields
    };
    if ((rv = bson_strip2(&sctx)) != BSON_OK) {
        goto finish;
    }
    if ((rv = bson_finish(&res)) != BSON_OK) {
        goto finish;
    }
    if ((rv = bson_finish(&collector)) != BSON_OK) {
        goto finish;
    }
    if ((rv = bson_merge_fieldpaths(bson_data(&res), 
                                    bson_data(&collector), 
                                    bsout)) != BSON_OK) {
        goto finish;
    }
    *rencnt = sctx.matched;
finish:
    bson_destroy(&res);
    bson_destroy(&collector);
    return rv;
}

int bson_inplace_set_bool(bson_iterator *pos, bson_bool_t val) {
    assert(pos);
    bson_type bt = BSON_ITERATOR_TYPE(pos);
    if (bt != BSON_BOOL) {
        return BSON_ERROR;
    }
    char *t = (char*) pos->cur + 1;
    t += strlen(t) + 1;
    *t = (val != 0);
    return BSON_OK;
}

int bson_inplace_set_long(bson_iterator *pos, int64_t val) {
    assert(pos);
    bson_type bt = BSON_ITERATOR_TYPE(pos);
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

int bson_inplace_set_double(bson_iterator *pos, double val) {
    assert(pos);
    bson_type bt = BSON_ITERATOR_TYPE(pos);
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

int bson_compare_fpaths(const void *bsdata1, const void *bsdata2, const char *fpath1, int fplen1, const char *fpath2, int fplen2) {
    assert(bsdata1 && bsdata2 && fpath1 && fpath2);
    bson_iterator it1, it2;
    BSON_ITERATOR_FROM_BUFFER(&it1, bsdata1);
    BSON_ITERATOR_FROM_BUFFER(&it2, bsdata2);
    bson_find_fieldpath_value2(fpath1, fplen1, &it1);
    bson_find_fieldpath_value2(fpath2, fplen2, &it2);
    return bson_compare_it_current(&it1, &it2);
}

static inline int bson_compare_type(bson_type t1, bson_type t2) {
    if (t1 == BSON_EOO || BSON_IS_NULL_TYPE(t1)) {
        return t2 == BSON_EOO || BSON_IS_NULL_TYPE(t2) ? 0 : -1;
    }
    return t1 - t2;
}

/**
 *
 * Return <0 if value pointing by it1 lesser than from it2.
 * Return  0 if values equal
 * Return >0 if value pointing by it1 greater than from it2.
 * @param it1
 * @param i
 * @return
 */
int bson_compare_it_current(const bson_iterator *it1, const bson_iterator *it2) {
    bson_type t1 = BSON_ITERATOR_TYPE(it1);
    bson_type t2 = BSON_ITERATOR_TYPE(it2);

    if (bson_compare_type(t1, t2) > 0) {
        return - bson_compare_it_current(it2, it1);
    }
	
	if ((BSON_IS_STRING_TYPE(t1) && !BSON_IS_STRING_TYPE(t2)) ||
		(BSON_IS_STRING_TYPE(t2) && !BSON_IS_STRING_TYPE(t1))) {
		return (t1 - t2);		
	}

    if (BSON_IS_NULL_TYPE(t1)) {
        return BSON_IS_NULL_TYPE(t2) ? 0 : -1;
    } else if (t1 == BSON_BOOL || t1 == BSON_EOO) {
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
    } else if (BSON_IS_STRING_TYPE(t1)) {
        const char* v1 = bson_iterator_string(it1);
        int l1 = bson_iterator_string_len(it1);
        const char* v2 = bson_iterator_string(it2);
        int l2 = bson_iterator_string_len(it2);
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
        BSON_ITERATOR_SUBITERATOR(it1, &sit1);
        BSON_ITERATOR_SUBITERATOR(it2, &sit2);
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

int bson_compare(const void *bsdata1, const void *bsdata2, const char* fpath, int fplen) {
    return bson_compare_fpaths(bsdata1, bsdata2, fpath, fplen, fpath, fplen);
}

int bson_compare_string(const char *cv, const void *bsdata, const char *fpath) {
    assert(cv && bsdata && fpath);
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_string(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

int bson_compare_long(const int64_t cv, const void *bsdata, const char *fpath) {
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_long(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

int bson_compare_double(double cv, const void *bsdata, const char *fpath) {
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_double(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

int bson_compare_bool(bson_bool_t cv, const void *bsdata, const char *fpath) {
    bson *bs1 = bson_create();
    bson_init(bs1);
    bson_append_bool(bs1, "$", cv);
    bson_finish(bs1);
    int res = bson_compare_fpaths(bson_data(bs1), bsdata, "$", 1, fpath, strlen(fpath));
    bson_del(bs1);
    return res;
}

bson* bson_dup(const bson *src) {
    assert(src);
    bson *rv = bson_create();
    int s = bson_size(src);
    _bson_init_size(rv, s);
    memmove(rv->data, src->data, s);
    rv->finished = 1;
    return rv;
}

bson* bson_create_from_iterator(bson_iterator *from) {
    assert(from);
    bson_type bt;
    bson *bs = bson_create();
    bson_init_as_query(bs);
    while ((bt = bson_iterator_next(from)) != BSON_EOO) {
        bson_append_field_from_iterator(from, bs);
    }
    bson_finish(bs);
    return bs;
}

bson* bson_create_from_buffer(const void* buf, int bufsz) {
    return bson_create_from_buffer2(bson_create(), buf, bufsz);
}

bson* bson_create_from_buffer2(bson *rv, const void* buf, int bufsz) {
    assert(buf);
    assert(bufsz - 4 > 0);
    bson_init_size(rv, bufsz);
    bson_ensure_space(rv, bufsz - 4);
    bson_append(rv, (char*) buf + 4, bufsz - (4 + 1/*BSON_EOO*/));
    bson_finish(rv);
    return rv;
}

void bson_init_with_data(bson *bs, const void *bsdata) {
    memset(bs, 0, sizeof (*bs));
    bs->data = (char*) bsdata;
    bson_little_endian32(&bs->dataSize, bsdata);
    bs->finished = true;
}

bool bson_find_merged_arrays(const void *mbuf, const void *inbuf, bool expandall) {
    assert(mbuf && inbuf);
    bool found = false;
    bson_iterator it, it2;
    bson_type bt, bt2;
    BSON_ITERATOR_FROM_BUFFER(&it, mbuf);

    while (!found && (bt = bson_iterator_next(&it)) != BSON_EOO) {
        if (expandall && bt != BSON_ARRAY) {
            continue;
        }
        BSON_ITERATOR_FROM_BUFFER(&it2, inbuf);
        bt2 = bson_find_fieldpath_value(BSON_ITERATOR_KEY(&it), &it2);
        if (bt2 != BSON_ARRAY) {
            continue;
        }
        bson_iterator sit;
        BSON_ITERATOR_SUBITERATOR(&it2, &sit);
        while (!found && (bt2 = bson_iterator_next(&sit)) != BSON_EOO) {
            if (expandall) {
                bson_iterator sit2;
                BSON_ITERATOR_SUBITERATOR(&it, &sit2);
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

bool bson_find_unmerged_arrays(const void *mbuf, const void *inbuf) {
    assert(mbuf && inbuf);
    bool allfound = false;
    bson_iterator it, it2;
    bson_type bt, bt2;
    BSON_ITERATOR_FROM_BUFFER(&it, mbuf);
    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        BSON_ITERATOR_FROM_BUFFER(&it2, inbuf);
        bt2 = bson_find_fieldpath_value(BSON_ITERATOR_KEY(&it), &it2);
        if (bt2 == BSON_EOO) { //array missing it will be created
            allfound = false;
            break;
        }
        if (bt2 != BSON_ARRAY) { //not an array field
            continue;
        }
        allfound = false;
        bson_iterator sit;
        BSON_ITERATOR_SUBITERATOR(&it2, &sit);
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
    bson_merge_array_mode mode;
} _BSONMARRCTX;


static bson_visitor_cmd_t _bson_merge_arrays_pull_visitor(
                        const char *fpath, int fpathlen, 
                        const char *key, int keylen, 
                        const bson_iterator *it, bool after, void *op) {
                            
    _BSONMARRCTX *ctx = op;
    assert(ctx && ctx->mfields >= 0);
    bson_iterator mit;
    bson_type bt = BSON_ITERATOR_TYPE(it);
    if (bt != BSON_OBJECT && bt != BSON_ARRAY) { //trivial case
        if (after) {
            return (BSON_VCMD_OK);
        }
        bson_append_field_from_iterator(it, ctx->bsout);
        return (BSON_VCMD_SKIP_AFTER);
    }
    if (bt == BSON_ARRAY) {
        BSON_ITERATOR_FROM_BUFFER(&mit, ctx->mbuf);
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
        BSON_ITERATOR_SUBITERATOR(it, &ait);
        bson_append_start_array(ctx->bsout, key);
        int c = 0;
        bool found = false;
        while ((bt = bson_iterator_next(&ait)) != BSON_EOO) {
            found = false;
            if (ctx->expandall) {
                bson_iterator mitsub;
                BSON_ITERATOR_SUBITERATOR(&mit, &mitsub);
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

static bson_visitor_cmd_t _bson_merge_arrays_visitor(
                        const char *fpath, int fpathlen, 
                        const char *key, int keylen, 
                        const bson_iterator *it, 
                        bool after, void *op) {
                            
    _BSONMARRCTX *ctx = op;
    assert(ctx && ctx->mfields >= 0);
    bson_iterator mit;
    bson_type bt = BSON_ITERATOR_TYPE(it);

    if (bt != BSON_OBJECT && bt != BSON_ARRAY) { //trivial case
        if (after) {
            return (BSON_VCMD_OK);
        }
        bson_append_field_from_iterator(it, ctx->bsout);
        return (BSON_VCMD_SKIP_AFTER);
    }
    if (bt == BSON_ARRAY) {
        if (after) {
            bson_append_finish_array(ctx->bsout);
            return (BSON_VCMD_OK);
        }
        BSON_ITERATOR_FROM_BUFFER(&mit, ctx->mbuf);
        bt = bson_find_fieldpath_value2(fpath, fpathlen, &mit);
        if (bt == BSON_EOO) {
            bson_append_start_array(ctx->bsout, key);
            return (BSON_VCMD_OK);
        }
        if (ctx->expandall && bt != BSON_ARRAY) {
            bson_append_field_from_iterator(it, ctx->bsout);
            return (BSON_VCMD_SKIP_NESTED | BSON_VCMD_SKIP_AFTER);
        }
        if (ctx->mfields > 0) {
            --ctx->mfields;
        }
        //Find and merge
        bson_iterator ait;
        BSON_ITERATOR_SUBITERATOR(it, &ait);
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
            BSON_ITERATOR_SUBITERATOR(&mit, &mitsub); //mit has BSON_ARRAY type
            while ((bt = bson_iterator_next(&mitsub)) != BSON_EOO) {
                found = false;
                if (ctx->mode == BSON_MERGE_ARRAY_ADDSET) {
                    BSON_ITERATOR_SUBITERATOR(it, &ait); //Rewind main array iterator
                    while ((bt = bson_iterator_next(&ait)) != BSON_EOO) {
                        if (!bson_compare_it_current(&ait, &mitsub)) {
                            found = true;
                            break;
                        }
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
                if ((ctx->mode == BSON_MERGE_ARRAY_ADDSET) && 
                    !found && 
                    !bson_compare_it_current(&ait, &mit)) {
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

int bson_merge_arrays(const void *mbuf, 
                      const void *inbuf, 
                      bson_merge_array_mode mode, 
                      bool expandall, 
                      bson *bsout) {
                          
    assert(mbuf && inbuf && bsout);
    assert(mode == BSON_MERGE_ARRAY_ADDSET || 
           mode == BSON_MERGE_ARRAY_PULL || 
           mode == BSON_MERGE_ARRAY_PUSH);
    
    if (bsout->finished) {
        return BSON_ERROR;
    }
    _BSONMARRCTX ctx = {
        .bsout = bsout,
        .mbuf = mbuf,
        .mfields = 0,
        .duty = false,
        .expandall = expandall,
        .ecode = BSON_OK,
        .mode = mode
    };
    bson_type bt, bt2;
    bson_iterator it, it2;
    BSON_ITERATOR_FROM_BUFFER(&it, mbuf);
    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        if (expandall && bt != BSON_ARRAY) {
            continue;
        }
        ctx.mfields++;
    }
    BSON_ITERATOR_FROM_BUFFER(&it, inbuf);
    if (mode == BSON_MERGE_ARRAY_PULL) {
        bson_visit_fields(&it, 0, _bson_merge_arrays_pull_visitor, &ctx);
    } else {
        bson_visit_fields(&it, 0, _bson_merge_arrays_visitor, &ctx);
    }
    if (ctx.mfields == 0 || //all fields are merged
        mode == BSON_MERGE_ARRAY_PULL) {
        return ctx.ecode;
    }

    //Append missing arrays fields
    BSON_ITERATOR_FROM_BUFFER(&it, mbuf);
    while ((bt = bson_iterator_next(&it)) != BSON_EOO) {
        const char *fpath = BSON_ITERATOR_KEY(&it);
        // all data from inbuf already in bsout
        bson_finish(bsout);
        BSON_ITERATOR_INIT(&it2, bsout);
        bt2 = bson_find_fieldpath_value(fpath, &it2);
        if (bt2 != BSON_EOO) continue;
        int i = 0;
        int lvl = 0;
        const char *pdp = fpath;
        bson bst;
        bson_init(&bst);
        while (*(fpath + i) != '\0') {
            for (; *(fpath + i) != '\0' && *(fpath + i) != '.'; ++i);
            if (*(fpath + i) == '\0') { //EOF
                assert((fpath + i) - pdp > 0);
                bson_append_start_array2(&bst, pdp, (fpath + i) - pdp);
                bson_append_field_from_iterator2("0", &it, &bst);
                bson_append_finish_array(&bst);
                break;
            } else {
                ++lvl;
                assert((fpath + i) - pdp > 0);
                bson_append_start_object2(&bst, pdp, (fpath + i) - pdp);
            }
            pdp = (fpath + i);
            while (*pdp == '.') {
                ++pdp;
                ++i;
            }
        }
        for (; lvl > 0; --lvl) {
            bson_append_finish_object(&bst);
        }
        bson_finish(&bst);

        bson bsc;
        bson_init_finished_data(&bsc, bson_data(bsout));
        bson_init_size(bsout, bson_size(bsout));
        int res = bson_merge_recursive(&bsc, &bst, false, bsout);
        bson_destroy(&bsc);
        bson_destroy(&bst);
        if (res != BSON_OK) {
            return BSON_ERROR;
        }
    }
    return ctx.ecode;
}

typedef struct {
    int nlvl; //nesting level
    TCXSTR *out; //output buffer
} _BSON2JSONCTX;

static void _jsonxstrescaped(TCXSTR *xstr, const char *str) {
    size_t sz = strlen(str);
    int s = 0;
    int e = 0;
    char hb[7];
    hb[0] = '\\';
    hb[1] = 'u';
    hb[2] = '0';
    hb[3] = '0';
    hb[6] = '\0';
    while (e < sz) {
        const char * ebuf = NULL;
        switch (str[e]) {
            case '\r': ebuf = "\\r";
                break;
            case '\n': ebuf = "\\n";
                break;
            case '\\': ebuf = "\\\\";
                break;
            case '/':
                break;
            case '"': ebuf = "\\\"";
                break;
            case '\f': ebuf = "\\f";
                break;
            case '\b': ebuf = "\\b";
                break;
            case '\t': ebuf = "\\t";
                break;
            default:
                if ((unsigned char) str[e] < 0x20) {
                    static const char *hexchar = "0123456789ABCDEF";
                    hb[4] = hexchar[str[e] >> 4];
                    hb[5] = hexchar[str[e] & 0x0F];
                    ebuf = hb;
                }
                break;
        }
        if (ebuf != NULL) {
            if (e > s) {
                tcxstrcat(xstr, str + s, e - s);
            }
            tcxstrcat2(xstr, ebuf);
            s = ++e;
        } else {
            ++e;
        }
    }
    tcxstrcat(xstr, (str + s), e - s);
}

static int _bson2json(_BSON2JSONCTX *ctx, bson_iterator *it, bool array) {

#define BSPAD(_n) \
    for (int i = 0; i < ctx->nlvl + (_n); ++i) tcxstrcat2(ctx->out, " ")

    bson_type bt;
    TCXSTR *out = ctx->out;
    tcxstrcat2(ctx->out, array ? "[\n" : "{\n");
    ctx->nlvl += 4;
    int c = 0;
    while ((bt = bson_iterator_next(it)) != BSON_EOO) {
        if (c++ > 0) {
            if (array) {
                tcxstrcat2(out, "\n");
                BSPAD(0);
            }
            tcxstrcat2(out, ",\n");
        }
        const char *key = BSON_ITERATOR_KEY(it);
        BSPAD(0);
        if (!array) {
            tcxstrcat2(out, "\"");
            _jsonxstrescaped(out, key);
            tcxstrcat2(out, "\" : ");
        }

        switch (bt) {
            case BSON_LONG:
            case BSON_INT:
                tcxstrprintf(out, "%" PRId64, (int64_t) bson_iterator_long_ext(it));
                break;
            case BSON_DOUBLE:
            {
                tcxstrprintf(out, "%lf", bson_iterator_double(it));
                break;
            }
            case BSON_STRING:
            case BSON_SYMBOL:
            {
                tcxstrcat2(out, "\"");
                _jsonxstrescaped(out, bson_iterator_string(it));
                tcxstrcat2(out, "\"");
                break;
            }
            case BSON_OBJECT:
            case BSON_ARRAY:
            {
                bson_iterator sit;
                BSON_ITERATOR_SUBITERATOR(it, &sit);
                _bson2json(ctx, &sit, bt == BSON_ARRAY);
                break;
            }
            case BSON_NULL:
                tcxstrcat2(out, "null");
            case BSON_UNDEFINED:
                break;
            case BSON_DATE:
            {
                bson_date_t t = bson_iterator_date(it);
                char dbuf[49];
                tcdatestrwww(t, INT_MAX, dbuf);
                tcxstrprintf(out, "\"%s\"", dbuf);
                break;
            }
            case BSON_BOOL:
                tcxstrcat2(out, bson_iterator_bool(it) ? "true" : "false");
                break;
            case BSON_OID:
            {
                char xoid[25];
                bson_oid_t *oid = bson_iterator_oid(it);
                bson_oid_to_string(oid, xoid);
                tcxstrprintf(out, "\"%s\"", xoid);
                break;
            }
            case BSON_REGEX:
            {
                tcxstrcat2(out, "\"");
                _jsonxstrescaped(out, bson_iterator_regex(it));
                tcxstrcat2(out, "\"");
                break;
            }
            case BSON_BINDATA:
            {
                const char *buf = bson_iterator_bin_data(it);
                int bsz = bson_iterator_bin_len(it);
                char *b64data = tcbaseencode(buf, bsz);
                tcxstrcat2(out, "\"");
                tcxstrcat2(out, b64data);
                tcxstrcat2(out, "\"");
                TCFREE(b64data);
                break;
            }
            default:
                break;
        }
    }
    tcxstrcat2(out, "\n");
    BSPAD(-4);
    tcxstrcat2(out, array ? "]" : "}");
    ctx->nlvl -= 4;
    return 0;
#undef BSPAD
}

int bson2json(const char *bsdata, char **buf, int *sp) {
    assert(bsdata && buf && sp);
    bson_iterator it;
    BSON_ITERATOR_FROM_BUFFER(&it, bsdata);
    TCXSTR *out = tcxstrnew();
    _BSON2JSONCTX ctx = {
        .nlvl = 0,
        .out = out
    };
    int ret = _bson2json(&ctx, &it, false);
    if (ret == BSON_OK) {
        *sp = TCXSTRSIZE(out);
        *buf = tcxstrtomalloc(out);
    } else {
        *sp = 0;
        *buf = NULL;
        tcxstrclear(out);
    }
    return ret;
}


#include "nxjson.h"

static void _json2bson(bson *out, const nx_json *json, const char *forcekey) {
    const char *key =  forcekey ? forcekey : json->key;
    switch (json->type) {
        case NX_JSON_NULL:
            assert(key);
            bson_append_null(out, key);
            break;
        case NX_JSON_OBJECT:
        {
            if (key) {
                bson_append_start_object(out, key);
            }
            for (nx_json* js = json->child; js; js = js->next) {
                _json2bson(out, js, NULL);
            }
            if (key) {
                bson_append_finish_object(out);
            }
            break;
        }
        case NX_JSON_ARRAY:
        {
            if (key) {
                bson_append_start_array(out, key);
            }
            int c = 0;
            char kbuf[TCNUMBUFSIZ];
            for (nx_json* js = json->child; js; js = js->next) {
                 bson_numstrn(kbuf, TCNUMBUFSIZ, c++);
                _json2bson(out, js, kbuf);
            }
            if (key) {
                bson_append_finish_array(out);
            }
            break;
        }
        case NX_JSON_STRING:
            assert(key);
            bson_append_string(out, key, json->text_value);
            break;
        case NX_JSON_INTEGER:
            assert(key);
            if (json->int_value <= INT_MAX && json->int_value >= INT_MIN) {
                bson_append_int(out, key, (int) json->int_value);
            } else {
                bson_append_long(out, key, json->int_value);
            }
            break;
        case NX_JSON_DOUBLE:
            assert(key);
            bson_append_double(out, key, json->dbl_value);
            break;
        case NX_JSON_BOOL:
            assert(key);
            bson_append_bool(out, key, json->int_value ? true : false);
            break;
        default:
            break;
    }
}

bson* json2bson(const char *jsonstr) {
    bool err = false;
    bson *out = NULL;
    char *json = strdup(jsonstr); //nxjson uses inplace data modification
    if (!json) {
        return NULL;
    }
    out = bson_create();
    bson_init_as_query(out);
    const nx_json *nxjson = nx_json_parse_utf8(json);
    if (!nxjson) {
        err = true;
        goto finish;
    }
    _json2bson(out, nxjson, NULL);
    bson_finish(out);
    err = out->err;
finish:
    free(json);
    if (nxjson) {
        nx_json_free(nxjson);
    }
    if (err && out) {
        bson_del(out);
        out = NULL;
    }
    return out;
}


typedef struct {
    bson *bs;
    bool checkdots; 
    bool checkdollar;
} _BSONVALIDATECTX;

static bson_visitor_cmd_t _bson_validate_visitor(
        const char *ipath, int ipathlen, 
        const char *key, int keylen,
        const bson_iterator *it, 
        bool after, void *op) {
    _BSONVALIDATECTX *ctx = op;
    assert(ctx);
    if (bson_check_field_name(ctx->bs, key, keylen, 
                              ctx->checkdots, ctx->checkdollar) == BSON_ERROR) {
        return BSON_VCMD_TERMINATE;
    }
    return (BSON_VCMD_OK | BSON_VCMD_SKIP_AFTER);
}


int bson_validate(bson *bs, bool checkdots, bool checkdollar) {
    if (!bs) {
        return BSON_ERROR;
    }
    if (!bs->finished) {
        bs->err |= BSON_NOT_FINISHED;
        return BSON_ERROR;
    }
    bson_iterator it;
    bson_iterator_init(&it, bs);
    _BSONVALIDATECTX ctx = {
        .bs = bs,
        .checkdots = checkdots,
        .checkdollar = checkdollar
    };
    bson_visit_fields(&it, BSON_TRAVERSE_ARRAYS_EXCLUDED, _bson_validate_visitor, &ctx);
    return bs->err ? BSON_ERROR : BSON_OK;
}

