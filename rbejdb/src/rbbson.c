#include "rbbson.h"

#include <ruby.h>
#include <math.h>
#include <string.h>
#include <tcejdb/ejdb.h>

#define BSON_RUBY_CLASS "BSON"

typedef struct {
    VALUE obj;
    bson* bsonval;
} RBBSON;

void iterate_attrs_callback(VALUE key, VALUE bsonWrap) {
    Check_Type(key, T_STRING);
    char* attrName = StringValuePtr(key);

    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);
    bson* b = rbbson->bsonval;
    VALUE val = rb_funcall(rbbson->obj, rb_intern("instance_variable_get"), 1, key);

    bson* subbson;

    switch (TYPE(val)) {
        case T_OBJECT:
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
}

void ruby_to_bson(VALUE rbobj, bson** bsonresp) {
    Check_Type(rbobj, T_OBJECT);

    VALUE bsonWrapClass = rb_define_class(BSON_RUBY_CLASS, rb_cObject);
    VALUE bsonWrap = Data_Wrap_Struct(bsonWrapClass, NULL, NULL, ruby_xmalloc(sizeof(RBBSON)));

    RBBSON* rbbson;
    Data_Get_Struct(bsonWrap, RBBSON, rbbson);
    rbbson->obj = rbobj;
    rbbson->bsonval = bson_create();
    bson_init(rbbson->bsonval);

    VALUE attrs = rb_funcall(rbobj, rb_intern("instance_variables"), 0);
    Check_Type(attrs, T_ARRAY);

    rb_iterate(rb_each, attrs, iterate_attrs_callback, bsonWrap);

    bson_finish(rbbson->bsonval);

    *bsonresp = rbbson->bsonval;
}