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

typedef struct {
    TCLIST* results;
} RBEJDB_RESULTS;


VALUE create_EJDB_query_results(TCLIST* qres);


VALUE ejdbClass;
VALUE ejdbResultsClass;


static int raise_ejdb_error(EJDB *ejdb) {
    int ecode = ejdbecode(ejdb);
    const char *emsg = ejdberrmsg(ecode);
    rb_raise(rb_eRuntimeError, "%s", emsg);
}


EJDB* getEJDB(VALUE self) {
    RBEJDB* rejdb;
    Data_Get_Struct(self, RBEJDB, rejdb);
    return rejdb->ejdb;
}


VALUE EJDB_new(VALUE self) {
    rb_raise(rb_eRuntimeError, "EJDB.open method should be used!");
    return self;
}

void EJDB_free(RBEJDB* rejdb) {
    if (rejdb->ejdb) {
        ejdbclose(rejdb->ejdb);
        ejdbdel(rejdb->ejdb);
    }
    ruby_xfree(rejdb);
}


VALUE EJDB_open(VALUE clazz, VALUE path, VALUE mode) {
    SafeStringValue(path);
    Check_Type(mode, T_FIXNUM);

    VALUE ejdbWrap = Data_Wrap_Struct(clazz, NULL, EJDB_free, ruby_xmalloc(sizeof(RBEJDB)));

    RBEJDB* rejdb;
    Data_Get_Struct(ejdbWrap, RBEJDB, rejdb);

    rejdb->ejdb = ejdbnew();

    if (!rejdb->ejdb) {
        rb_raise(rb_eRuntimeError, "Failed to init ejdb!");
    }

    if (!ejdbopen(rejdb->ejdb, StringValuePtr(path), NUM2INT(mode))) {
        raise_ejdb_error(rejdb->ejdb);
    }
    return ejdbWrap;
}

VALUE EJDB_is_open(VALUE self) {
    EJDB* ejdb = getEJDB(self);
    return ejdb && ejdbisopen(ejdb) ? Qtrue : Qfalse;
}

void EJDB_close(VALUE self) {
    EJDB* ejdb = getEJDB(self);
    ejdbclose(ejdb);
}

void EJDB_dropCollection(VALUE self, VALUE collName, VALUE prune) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    if (!ejdbrmcoll(ejdb, StringValuePtr(collName), RTEST(prune))) {
        raise_ejdb_error(ejdb);        
    }
}

void EJDB_ensureCollection(int argc, VALUE* argv, VALUE self) {
    VALUE collName;
    VALUE copts;

    rb_scan_args(argc, argv, "11", &collName, &copts);

    SafeStringValue(collName);

    EJCOLLOPTS jcopts = {NULL};
    if (!NIL_P(copts)) {
        Check_Type(copts, T_HASH);

        VALUE cachedrecords = rb_hash_aref(copts, rb_str_new2("cachedrecords"));
        VALUE compressed = rb_hash_aref(copts, rb_str_new2("compressed"));
        VALUE large = rb_hash_aref(copts, rb_str_new2("large"));
        VALUE records = rb_hash_aref(copts, rb_str_new2("records"));

        jcopts.cachedrecords = !NIL_P(cachedrecords) ? NUM2INT(cachedrecords) : 0;
        jcopts.compressed = RTEST(compressed);
        jcopts.large = RTEST(large);
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

    VALUE collName = argv[0];
    Check_Type(collName, T_STRING);

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    VALUE oids = rb_ary_new();
    int i;
    for (i = 1; i < argc; i++) {
        VALUE rbobj = argv[i];
        if (NIL_P(rbobj)) {
            rb_ary_push(oids, Qnil);
            continue;
        }

        bson* bsonval;
        ruby_to_bson(rbobj, &bsonval, 0);

        bson_oid_t oid;
        if (!ejdbsavebson2(coll, bsonval, &oid, true /*TODO read this param*/)) {
            bson_destroy(bsonval);
            raise_ejdb_error(ejdb);
        }

        bson_destroy(bsonval);

        VALUE roid = bson_oid_to_ruby(&oid);
        rb_ary_push(oids, roid);

        switch(TYPE(rbobj)) {
            case T_HASH:
                rb_hash_aset(rbobj, rb_str_new2("_id"), roid);
                break;
            default:
                rb_iv_set(rbobj, "@id", roid);
        }
    }

    switch (RARRAY_LEN(oids)) {
        case 0 : return Qnil;
        case 1: return rb_ary_pop(oids);
        default: return oids;
    }
}

VALUE EJDB_load(VALUE self, VALUE collName, VALUE rboid) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbgetcoll(ejdb, StringValuePtr(collName));
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    bson_oid_t oid = ruby_to_bson_oid(rboid);

    bson *bs = ejdbloadbson(coll, &oid);
    if (!bs) {
        raise_ejdb_error(ejdb);
    }

    return bson_to_ruby(bs);
}

VALUE EJDB_find(int argc, VALUE* argv, VALUE self) {
    VALUE collName;
    VALUE q;

    rb_scan_args(argc, argv, "11", &collName, &q);

    SafeStringValue(collName);
    q = !NIL_P(q) ? q :rb_hash_new();

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

    return create_EJDB_query_results(qres);
}


void EJDB_results_free(RBEJDB_RESULTS* rbres) {
    if (rbres->results) {
        tclistdel(rbres->results);
    }
    ruby_xfree(rbres);
}

VALUE create_EJDB_query_results(TCLIST* qres) {
    VALUE results = Data_Wrap_Struct(ejdbResultsClass, NULL, EJDB_results_free, ruby_xmalloc(sizeof(RBEJDB_RESULTS)));
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(results, RBEJDB_RESULTS, rbresults);

    rbresults->results = qres;
    return results;
}

void EJDB_results_each(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    if (!rbresults || !rbresults->results) {
        rb_raise(rb_eRuntimeError, "Each() method called on invalid ejdb query results");
    }

    TCLIST* qres = rbresults->results;
    int i;
    for (i = 0; i < TCLISTNUM(qres); i++) {
        char* bsrawdata = TCLISTVALPTR(qres, i);
        bson bsonval;
        bson_init_finished_data(&bsonval, bsrawdata);
        rb_yield(bson_to_ruby(&bsonval));
    }
}


Init_rbejdb() {
    init_ruby_to_bson();

    ejdbClass = rb_define_class("EJDB", rb_cObject);
    rb_define_private_method(ejdbClass, "new", RUBY_METHOD_FUNC(EJDB_new), 0);

    rb_define_const(ejdbClass, "DEFAULT_OPEN_MODE", INT2FIX(DEFAULT_OPEN_MODE));
    rb_define_const(ejdbClass, "JBOWRITER", INT2FIX(JBOWRITER));
    rb_define_const(ejdbClass, "JBOCREAT", INT2FIX(JBOCREAT));
    rb_define_const(ejdbClass, "JBOTSYNC", INT2FIX(JBOTSYNC));
    rb_define_const(ejdbClass, "JBOTRUNC", INT2FIX(JBOTRUNC));

    rb_define_singleton_method(ejdbClass, "open", RUBY_METHOD_FUNC(EJDB_open), 2);
    rb_define_method(ejdbClass, "is_open?", RUBY_METHOD_FUNC(EJDB_is_open), 0);
    rb_define_method(ejdbClass, "close", RUBY_METHOD_FUNC(EJDB_close), 0);
    rb_define_method(ejdbClass, "save", RUBY_METHOD_FUNC(EJDB_save), -1);
    rb_define_method(ejdbClass, "load", RUBY_METHOD_FUNC(EJDB_load), 2);
    rb_define_method(ejdbClass, "find", RUBY_METHOD_FUNC(EJDB_find), -1);

    rb_define_method(ejdbClass, "dropCollection", RUBY_METHOD_FUNC(EJDB_dropCollection), 2);
    rb_define_method(ejdbClass, "ensureCollection", RUBY_METHOD_FUNC(EJDB_ensureCollection), -1);


    ejdbResultsClass = rb_define_class("EJDBResults", rb_cObject);
    rb_include_module(ejdbResultsClass, rb_mEnumerable);
    rb_define_method(ejdbResultsClass, "each", RUBY_METHOD_FUNC(EJDB_results_each), 0);
}