#include "rbbson.h"

#include <ruby.h>
#include <math.h>
#include <string.h>
#include <tcejdb/ejdb.h>

#define BSON_RUBY_CLASS "EJDB_BSON"

typedef struct {
    bson* bsonval;

    VALUE obj;
    int arrayIndex;
    int flags;
} RBBSON;


VALUE iterate_array_callback(VALUE val, VALUE bsonWrap);

VALUE bson_array_to_ruby(bson_iterator* it);

bson_date_t ruby_time_to_bson_internal(VALUE time);


VALUE bsonWrapClass = Qnil;


void init_ruby_to_bson() {
    bsonWrapClass = rb_define_class(BSON_RUBY_CLASS, rb_cObject);
}


VALUE createBsonWrap(bson* bsonval, VALUE rbobj, int flags) {
    if (NIL_P(bsonWrapClass)) {
        rb_raise(rb_eRuntimeError, "Ruby to BSON library must be initialized");
    }
    VALUE bsonWrap = Data_Wrap_Struct(bsonWrapClass, NULL, NULL, ruby_xmalloc(sizeof(RBBSON)));

    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);
    rbbson->bsonval = bsonval;
    rbbson->obj = rbobj;
    rbbson->arrayIndex = 0;

    return bsonWrap;
}

int iterate_key_values_callback(VALUE key, VALUE val, VALUE bsonWrap) {
    key = rb_funcall(key, rb_intern("to_s"), 0);
    char* attrName = StringValuePtr(key);

    if (attrName[0] == '@') attrName++; // hack, removing @

    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);
    bson* b = rbbson->bsonval;

    bson* subbson;

    switch (TYPE(val)) {
        case T_OBJECT:
        case T_HASH:
            ruby_to_bson(val, &subbson, rbbson->flags);
            bson_append_bson(b, attrName, subbson);
            break;
        case T_ARRAY:
            bson_append_start_array(b, attrName);
            rb_iterate(rb_each, val, iterate_array_callback, createBsonWrap(b, rbbson->obj, rbbson->flags));
            bson_append_finish_array(b);
            break;
        case T_STRING:
            bson_append_string(b, attrName, StringValuePtr(val));
            break;
        case T_FIXNUM:
            bson_append_int(b, attrName, FIX2INT(val));
            break;
        case T_DATA:
            if (0 == strcmp(rb_obj_classname(val), "Time")) {
                bson_append_date(b, attrName, ruby_time_to_bson_internal(val));
            } else {
                rb_raise(rb_eRuntimeError, "Cannot convert ruby data object to bson");
            }
        case T_TRUE:
            bson_append_bool(b, attrName, 1);
            break;
        case T_FALSE:
            bson_append_bool(b, attrName, 0);
            break;
        case T_NIL:
            bson_append_null(b, attrName);
            break;
        default:
            rb_raise(rb_eRuntimeError, "Cannot convert value type to bson: %d", TYPE(val));
    }
    return 0;
}

VALUE iterate_array_callback(VALUE val, VALUE bsonWrap) {
    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);

    iterate_key_values_callback(INT2NUM(rbbson->arrayIndex++), val, bsonWrap);
    return val;
}

VALUE iterate_object_attrs_callback(VALUE key, VALUE bsonWrap) {
    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);
    VALUE val = rb_funcall(rbbson->obj, rb_intern("instance_variable_get"), 1, key);

    iterate_key_values_callback(key, val, bsonWrap);
    return val;
}

bson_date_t ruby_time_to_bson_internal(VALUE time) {
    VALUE microsecs = rb_funcall(time, rb_intern("to_f"), 0);
    Check_Type(microsecs, T_FLOAT);
    return (bson_date_t) NUM2DBL(microsecs) * 1000;
}


void ruby_object_to_bson_internal(VALUE rbobj, VALUE bsonWrap) {
    Check_Type(rbobj, T_OBJECT);

    VALUE attrs = rb_funcall(rbobj, rb_intern("instance_variables"), 0);
    Check_Type(attrs, T_ARRAY);

    rb_iterate(rb_each, attrs, iterate_object_attrs_callback, bsonWrap);
}

void ruby_hash_to_bson_internal(VALUE rbhash, VALUE bsonWrap) {
    Check_Type(rbhash, T_HASH);
    rb_hash_foreach(rbhash, iterate_key_values_callback, bsonWrap);
}


void ruby_to_bson(VALUE rbobj, bson** bsonresp, int flags) {
    VALUE bsonWrap = createBsonWrap(bson_create(), rbobj, flags);
    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);

    if (flags & RUBY_TO_BSON_AS_QUERY) {
        bson_init_as_query(rbbson->bsonval);
    } else {
        bson_init(rbbson->bsonval);
    }

    switch (TYPE(rbobj)) {
        case T_OBJECT:
        case T_DATA:
            ruby_object_to_bson_internal(rbobj, bsonWrap);
            break;
        case T_HASH:
            ruby_hash_to_bson_internal(rbobj, bsonWrap);
            break;
        default:
            rb_raise(rb_eRuntimeError, "Cannot convert object to bson: %d", TYPE(rbobj));
    }

    bson_finish(rbbson->bsonval);

    *bsonresp = rbbson->bsonval;
}


VALUE bson_iterator_to_ruby(bson_iterator* it, bson_type t) {
    VALUE val;
    switch (t) {
        case BSON_OID:
            val = bson_oid_to_ruby(bson_iterator_oid(it));
            break;
        case BSON_STRING:
            val = rb_str_new2(bson_iterator_string(it));
            break;
        case BSON_BOOL:
            val = bson_iterator_bool(it) ? Qtrue : Qfalse;
            break;
        case BSON_INT:
            val = INT2NUM(bson_iterator_int(it));
            break;
        case BSON_OBJECT: {
                bson subbson;
                bson_iterator_subobject(it, &subbson);
                val = bson_to_ruby(&subbson);
            }
            break;
        case BSON_ARRAY:
            val = bson_array_to_ruby(it);
            break;
        default:
            rb_raise(rb_eRuntimeError, "Cannot convert object from bson: %d", t);
    }

    return val;
}

VALUE bson_array_to_ruby(bson_iterator* it) {
    VALUE res = rb_ary_new();

    bson_iterator nit;
    bson_iterator_subiterator(it, &nit);

    while (bson_iterator_next(&nit) != BSON_EOO) {
        rb_ary_push(res, bson_iterator_to_ruby(&nit, bson_iterator_type(&nit)));
    }

    return res;
}


VALUE bson_to_ruby(bson* bsonval) {
    VALUE res = rb_hash_new();

    bson_iterator it;
    bson_iterator_init(&it, bsonval);

    while (bson_iterator_next(&it) != BSON_EOO) {
        rb_hash_aset(res, rb_str_new2(bson_iterator_key(&it)),
                          bson_iterator_to_ruby(&it, bson_iterator_type(&it)));
    }
    return res;
}

VALUE bson_oid_to_ruby(bson_oid_t* oid) {
    char oidhex[25];
    bson_oid_to_string(oid, oidhex);
    return rb_str_new2(oidhex);
}
