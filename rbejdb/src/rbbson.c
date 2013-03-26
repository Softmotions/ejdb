#include "rbbson.h"

#include <ruby.h>
#include <math.h>
#include <string.h>
#include <tcejdb/ejdb.h>

#define BSON_RUBY_CLASS "EJDB_BSON"

typedef struct {
    VALUE obj;
    bson* bsonval;
} RBBSON;


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
            ruby_to_bson(val, &subbson);
            bson_append_bson(b, attrName, subbson);
            break;
        case T_STRING:
            bson_append_string(b, attrName, StringValuePtr(val));
            break;
        case T_FIXNUM:
            bson_append_int(b, attrName, FIX2INT(val));
            break;
        case T_TRUE:
            bson_append_bool(b, attrName, 1);
            break;
        case T_FALSE:
            bson_append_bool(b, attrName, 0);
            break;
    }
    return 0;
}

VALUE iterate_keys_callback(VALUE key, VALUE bsonWrap) {
    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);
    VALUE val = rb_funcall(rbbson->obj, rb_intern("instance_variable_get"), 1, key);

    iterate_key_values_callback(key, val, bsonWrap);
    return val;
}


void ruby_object_to_bson_internal(VALUE rbobj, VALUE bsonWrap) {
    Check_Type(rbobj, T_OBJECT);

    VALUE attrs = rb_funcall(rbobj, rb_intern("instance_variables"), 0);
    Check_Type(attrs, T_ARRAY);

    rb_iterate(rb_each, attrs, iterate_keys_callback, bsonWrap);
}

void ruby_hash_to_bson_internal(VALUE rbhash, VALUE bsonWrap) {
    Check_Type(rbhash, T_HASH);
    rb_hash_foreach(rbhash, iterate_key_values_callback, bsonWrap);
}


void ruby_to_bson(VALUE rbobj, bson** bsonresp) {
    VALUE bsonWrapClass = rb_define_class(BSON_RUBY_CLASS, rb_cObject);
    VALUE bsonWrap = Data_Wrap_Struct(bsonWrapClass, NULL, NULL, ruby_xmalloc(sizeof(RBBSON)));

    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);
    rbbson->obj = rbobj;
    rbbson->bsonval = bson_create();
    bson_init(rbbson->bsonval);

    switch (TYPE(rbobj)) {
        case T_OBJECT:
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
