/**************************************************************************************************
 *  Ruby API for EJDB database library http://ejdb.org
 *  Copyright (C) 2012-2013 Softmotions Ltd <info@softmotions.com>
 *
 *  This file is part of EJDB.
 *  EJDB is free software; you can redistribute it and/or modify it under the terms of
 *  the GNU Lesser General Public License as published by the Free Software Foundation; either
 *  version 2.1 of the License or any later version.  EJDB is distributed in the hope
 *  that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 *  License for more details.
 *  You should have received a copy of the GNU Lesser General Public License along with EJDB;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA.
 *************************************************************************************************/

#include <tcejdb/ejdb_private.h>
#include <ruby.h>

#include "rbbson.h"

#define DEFAULT_OPEN_MODE (JBOWRITER | JBOCREAT | JBOTSYNC)

typedef struct {
    EJDB* ejdb;
} RBEJDB;

static int raise_ejdb_error(EJDB *ejdb) {
    int ecode = ejdbecode(ejdb);
    const char *emsg = ejdberrmsg(ecode);
    rb_raise(rb_eRuntimeError, emsg);
}


EJDB* getEJDB(VALUE self) {
    RBEJDB* rejdb;
    Data_Get_Struct(self, RBEJDB, rejdb);
    return rejdb->ejdb;
}

void EJDB_free(RBEJDB* rejdb) {
    if (!rejdb->ejdb) {
        ejdbclose(rejdb->ejdb);
        ejdbdel(rejdb->ejdb);
    }
    ruby_xfree(rejdb);
}

VALUE EJDB_alloc(VALUE klass) {
    return Data_Wrap_Struct(klass, NULL, EJDB_free, ruby_xmalloc(sizeof(RBEJDB)));
}

VALUE EJDB_init(VALUE self) {
    RBEJDB* rejdb;
    Data_Get_Struct(self, RBEJDB, rejdb);

    rejdb->ejdb = ejdbnew();
    if (!rejdb->ejdb) {
        rb_raise(rb_eRuntimeError, "Failed to init ejdb!");
    }
}

VALUE EJDB_open(VALUE self, VALUE path, VALUE mode) {
    Check_SafeStr(path);
    Check_Type(mode, T_FIXNUM);

    EJDB* ejdb = getEJDB(self);
    if (!ejdbopen(ejdb, StringValuePtr(path), FIX2INT(mode))) {
        raise_ejdb_error(ejdb);
    }
    return Qnil;
}

VALUE EJDB_is_open(VALUE self) {
    EJDB* ejdb = getEJDB(self);
    return ejdb && ejdbisopen(ejdb) ? Qtrue : Qfalse;
}

void EJDB_dropCollection(VALUE self, VALUE collName, VALUE prune) {
    Check_SafeStr(collName);

    EJDB* ejdb = getEJDB(self);
    if (!ejdbrmcoll(ejdb, StringValuePtr(collName), TYPE(prune) == T_TRUE)) {
        raise_ejdb_error(ejdb);        
    }
}

void EJDB_ensureCollection(int argc, VALUE* argv, VALUE self) {
    VALUE collName, copts;
    rb_scan_args(argc, argv, "11", &collName, &copts);

    Check_SafeStr(collName);

    EJCOLLOPTS jcopts = {NULL};
    if (!NIL_P(copts)) {
        Check_Type(copts, T_HASH);

        VALUE cachedrecords = rb_hash_aref(copts, rb_str_new2("cachedrecords"));
        VALUE compressed = rb_hash_aref(copts, rb_str_new2("compressed"));
        VALUE large = rb_hash_aref(copts, rb_str_new2("large"));
        VALUE records = rb_hash_aref(copts, rb_str_new2("records"));

        jcopts.cachedrecords = !NIL_P(cachedrecords) ? NUM2INT(cachedrecords) : 0;
        jcopts.compressed = TYPE(compressed) == T_TRUE;
        jcopts.large = TYPE(large) == T_TRUE;
        jcopts.records = !NIL_P(records) ? NUM2INT(records) : 0;
    }

    EJDB* ejdb = getEJDB(self);

    if (!ejdbcreatecoll(ejdb, StringValuePtr(collName), &jcopts)) {
        raise_ejdb_error(ejdb);
    }
}

VALUE EJDB_save(int argc, VALUE *argv, VALUE self) {
    if (argc < 1) {
        rb_raise(rb_eRuntimeError, "Error calling EJDB.save(): need to specify collection name");
    }

    EJDB* ejdb = getEJDB(self);

    VALUE collName = argv[0];
    Check_Type(collName, T_STRING);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    int i;
    for (i = 1; i < argc; i++) {
        bson* bsonval;
        ruby_to_bson(argv[i], &bsonval, 0);

        bson_oid_t oid;
        if (!ejdbsavebson2(coll, bsonval, &oid, true /*TODO read this param*/)) {
            bson_destroy(bsonval);
            raise_ejdb_error(ejdb);
        }

        bson_destroy(bsonval);
    }
    return Qnil;
}

VALUE EJDB_find(VALUE self, VALUE collName, VALUE q) {
    Check_SafeStr(collName);

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    bson* qbson;
    ruby_to_bson(q, &qbson, RUBY_TO_BSON_AS_QUERY);

    EJQ *ejq = ejdbcreatequery(ejdb, qbson, NULL, 0, NULL);

    int count;
    int qflags = 0;
    TCLIST* qres = ejdbqryexecute(coll, ejq, &count, qflags, NULL);

    int i;
    for (i = 0; i < TCLISTNUM(qres); i++) {
        char* bsrawdata = TCLISTVALPTR(qres, i);
        bson bsonval;
        bson_init_finished_data(&bsonval, bsrawdata);
        rb_yield(bson_to_ruby(&bsonval));
    }

    tclistdel(qres);
    ejdbquerydel(ejq);
}

Init_rbejdb() {
    VALUE ejdbClass = rb_define_class("EJDB", rb_cObject);
    rb_define_alloc_func(ejdbClass, EJDB_alloc);
    rb_define_private_method(ejdbClass, "initialize", RUBY_METHOD_FUNC(EJDB_init), 0);

    rb_define_const(ejdbClass, "DEFAULT_OPEN_MODE", INT2FIX(DEFAULT_OPEN_MODE));

    rb_define_method(ejdbClass, "open", RUBY_METHOD_FUNC(EJDB_open), 2);
    rb_define_method(ejdbClass, "is_open?", RUBY_METHOD_FUNC(EJDB_is_open), 0);
    rb_define_method(ejdbClass, "save", RUBY_METHOD_FUNC(EJDB_save), -1);
    rb_define_method(ejdbClass, "find", RUBY_METHOD_FUNC(EJDB_find), 2);

    rb_define_method(ejdbClass, "dropCollection", RUBY_METHOD_FUNC(EJDB_dropCollection), 2);
    rb_define_method(ejdbClass, "ensureCollection", RUBY_METHOD_FUNC(EJDB_ensureCollection), -1);
}