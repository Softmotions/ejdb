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
    TCXSTR* log;
} RBEJDB_RESULTS;

typedef struct {
    bson* qbson;
    bson* hintsbson;
    int orarrlng;
    bson* orarrbson;
} RBEJDB_QUERY;


VALUE create_EJDB_query_results(TCLIST* qres, TCXSTR *log);


VALUE ejdbClass;
VALUE ejdbResultsClass;
VALUE ejdbBinaryClass;
VALUE ejdbQueryClass;


VALUE get_hash_option(VALUE hash, const char* opt) {
    Check_Type(hash, T_HASH);

    VALUE res = Qnil;

    ID symId = rb_intern(opt);

    if (symId) {
        VALUE symbol = ID2SYM(symId);
        if (TYPE(symbol) == T_SYMBOL) {
            res = rb_hash_aref(hash, symbol);
        }
    }

    return !NIL_P(res) ? res : rb_hash_aref(hash, rb_str_new2(opt));
}

static int raise_ejdb_error(EJDB *ejdb) {
    int ecode = ejdbecode(ejdb);
    const char *emsg = ejdberrmsg(ecode);
    rb_raise(rb_eRuntimeError, "%s", emsg);
}

static int nil_or_raise_ejdb_error(EJDB *ejdb) {
    int ecode = ejdbecode(ejdb);
    if (ecode != TCESUCCESS && ecode != TCENOREC) {
        raise_ejdb_error(ejdb);
    } else {
        return Qnil;
    }
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

void EJDB_drop_collection(int argc, VALUE* argv, VALUE self) {
    VALUE collName;
    VALUE prune;

    rb_scan_args(argc, argv, "11", &collName, &prune);

    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    if (!ejdbrmcoll(ejdb, StringValuePtr(collName), RTEST(prune))) {
        raise_ejdb_error(ejdb);        
    }
}

void EJDB_ensure_collection(int argc, VALUE* argv, VALUE self) {
    VALUE collName;
    VALUE copts;

    rb_scan_args(argc, argv, "11", &collName, &copts);

    SafeStringValue(collName);

    EJCOLLOPTS jcopts = {NULL};
    if (!NIL_P(copts)) {
        Check_Type(copts, T_HASH);

        VALUE cachedrecords = get_hash_option(copts, "cachedrecords");
        VALUE compressed = get_hash_option(copts, "compressed");
        VALUE large = get_hash_option(copts, "large");
        VALUE records = get_hash_option(copts, "records");

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
        rb_raise(rb_eArgError, "Error calling EJDB.save(): need to specify collection name");
    }

    VALUE collName = argv[0];
    Check_Type(collName, T_STRING);

    bool merge = TYPE(argv[argc - 1]) == T_TRUE;

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    VALUE oids = rb_ary_new();
    int i;
    for (i = 1; i < argc; i++) {
        VALUE rbobj = argv[i];

        if (merge && i == argc - 1) break;

        if (NIL_P(rbobj)) {
            rb_ary_push(oids, Qnil);
            continue;
        }

        bson* bsonval;
        ruby_to_bson(rbobj, &bsonval, 0);

        bson_oid_t oid;
        if (!ejdbsavebson2(coll, bsonval, &oid, merge)) {
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
        return nil_or_raise_ejdb_error(ejdb);
    }

    bson_oid_t oid = ruby_to_bson_oid(rboid);
    bson *bs = ejdbloadbson(coll, &oid);

    return bs ? bson_to_ruby(bs) : nil_or_raise_ejdb_error(ejdb);
}

void prepare_query_hint(VALUE res, VALUE hints, char* hint) {
    VALUE val = get_hash_option(hints, hint);
    if (!NIL_P(val)) {
        rb_hash_aset(res, rb_str_concat(rb_str_new2("$"), rb_str_new2(hint)), val);
    }
}

VALUE prepare_query_hints(VALUE hints) {
    VALUE res = rb_hash_new();
    prepare_query_hint(res, hints, "orderby");
    prepare_query_hint(res, hints, "max");
    prepare_query_hint(res, hints, "skip");
    return res;
}

VALUE EJDB_query_free(RBEJDB_QUERY* rbquery) {
    if (rbquery->qbson) {
        bson_destroy(rbquery->qbson);
    }
    if (rbquery->hintsbson) {
        bson_destroy(rbquery->hintsbson);
    }
    if (rbquery->orarrbson) {
        int i;
        for (i = 0; i < rbquery->orarrlng; i++) {
            bson_destroy(rbquery->orarrbson + i);
        }
        free(rbquery->orarrbson);
    }
    ruby_xfree(rbquery);
}

VALUE EJDB_find(int argc, VALUE* argv, VALUE self) {
    VALUE collName;
    VALUE q;
    VALUE orarr;
    VALUE hints;

    VALUE p3;
    VALUE p4;

    rb_scan_args(argc, argv, "13", &collName, &q, &p3, &p4);

    SafeStringValue(collName);
    q = !NIL_P(q) ? q :rb_hash_new();
    orarr = TYPE(p3) == T_ARRAY ? rb_ary_dup(p3) : rb_ary_new();
    hints = TYPE(p3) != T_ARRAY ? p3 : p4;
    hints = !NIL_P(hints) ? hints :rb_hash_new();

    Check_Type(q, T_HASH);
    Check_Type(hints, T_HASH);

    VALUE queryWrap = Data_Wrap_Struct(ejdbQueryClass, NULL, EJDB_query_free, ruby_xmalloc(sizeof(RBEJDB_QUERY)));
    RBEJDB_QUERY* rbquery;
    Data_Get_Struct(queryWrap, RBEJDB_QUERY, rbquery);

    VALUE orarrlng = rb_funcall(orarr, rb_intern("length"), 0);
    rbquery->qbson = NULL;
    rbquery->hintsbson = NULL;
    rbquery->orarrbson = NUM2INT(orarrlng) ? (bson*) tcmalloc(rbquery->orarrlng * sizeof(bson)) : NULL;
    rbquery->orarrlng;

    ruby_to_bson(q, &(rbquery->qbson), RUBY_TO_BSON_AS_QUERY);

    bool onlycount = RTEST(get_hash_option(hints, "onlycount"));
    bool explain = RTEST(get_hash_option(hints, "explain"));

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbgetcoll(ejdb, StringValuePtr(collName));
    if (!coll) {
        bson_iterator it;
        if (bson_find(&it, rbquery->qbson, "$upsert") == BSON_OBJECT) {
            coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
        }
        if (!coll) {
            return !onlycount ? create_EJDB_query_results(tclistnew2(1), NULL) : INT2NUM(0);
        }
    }

    int i;
    while(!NIL_P(rb_ary_entry(orarr, 0))) {
        VALUE orq = rb_ary_shift(orarr);
        bson* orqbson;
        ruby_to_bson(orq, &orqbson, RUBY_TO_BSON_AS_QUERY);
        bson_copy(rbquery->orarrbson + (i++), orqbson);
        bson_destroy(orqbson);
        rbquery->orarrlng++;
    }

    ruby_to_bson(prepare_query_hints(hints), &(rbquery->hintsbson), RUBY_TO_BSON_AS_QUERY);

    EJQ *ejq = ejdbcreatequery(ejdb, rbquery->qbson, rbquery->orarrbson, NUM2INT(orarrlng), rbquery->hintsbson);

    int count;
    int qflags = onlycount ? EJQONLYCOUNT : 0;
    TCXSTR *log = explain ? tcxstrnew() : NULL;

    TCLIST* qres = ejdbqryexecute(coll, ejq, &count, qflags, log);

    return !onlycount ? create_EJDB_query_results(qres, log) : INT2NUM(count);
}

static VALUE EJDB_block_true(VALUE yielded_object, VALUE context, int argc, VALUE argv[]){
    return Qtrue;
}

VALUE EJDB_find_one(int argc, VALUE* argv, VALUE self) {
    VALUE results = EJDB_find(argc, argv, self);
    if (TYPE(results) == T_DATA) {
        return rb_block_call(results, rb_intern("find"), 0, NULL, RUBY_METHOD_FUNC(EJDB_block_true), Qnil); // "find" with "always true" block gets first element
    }
    return results;
}

VALUE EJDB_update(int argc, VALUE* argv, VALUE self) {
    return EJDB_find(argc, argv, self);
}


void EJDB_remove(VALUE self, VALUE collName, VALUE rboid) {
    SafeStringValue(collName);
    SafeStringValue(rboid);

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbgetcoll(ejdb, StringValuePtr(collName));
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    bson_oid_t oid = ruby_to_bson_oid(rboid);
    if (!ejdbrmbson(coll, &oid)) {
        raise_ejdb_error(ejdb);
    }
}


void EJDB_set_index_internal(VALUE self, VALUE collName, VALUE fpath, int flags) {
    SafeStringValue(collName);
    SafeStringValue(fpath);

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    if (!ejdbsetindex(coll, StringValuePtr(fpath), flags)) {
        raise_ejdb_error(ejdb);
    }
}

void EJDB_drop_indexes(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXDROPALL);
};

void EJDB_optimize_indexes(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXOP);
};

void EJDB_ensure_string_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXSTR);
}

void EJDB_rebuild_string_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXSTR | JBIDXREBLD);
}

void EJDB_drop_string_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXSTR | JBIDXDROP);
}

void EJDB_ensure_istring_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXISTR);
}

void EJDB_rebuild_istring_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXISTR | JBIDXREBLD);
}

void EJDB_drop_istring_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXISTR | JBIDXDROP);
}

void EJDB_ensure_number_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXNUM);
}

void EJDB_rebuild_number_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXNUM | JBIDXREBLD);
}

void EJDB_drop_number_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXNUM | JBIDXDROP);
}

void EJDB_ensure_array_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXARR);
}

void EJDB_rebuild_array_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXARR | JBIDXREBLD);
}

void EJDB_drop_array_index(VALUE self, VALUE collName, VALUE fpath) {
    EJDB_set_index_internal(self, collName, fpath, JBIDXARR | JBIDXDROP);
}

VALUE EJDB_get_db_meta(VALUE self) {
    EJDB* ejdb = getEJDB(self);

    TCLIST *cols = ejdbgetcolls(ejdb);
    if (!cols) {
        raise_ejdb_error(ejdb);
    }

    VALUE res = rb_hash_new();
    VALUE collections = rb_ary_new();
    rb_hash_aset(res,  rb_str_new2("collections"), collections);
    int i, j;
    for (i = 0; i < TCLISTNUM(cols); ++i) {
        EJCOLL *coll = (EJCOLL*) TCLISTVALPTR(cols, i);

        VALUE collhash = rb_hash_new();
        rb_ary_push(collections, collhash);
        rb_hash_aset(collhash,  rb_str_new2("name"), rb_str_new2(coll->cname));
        rb_hash_aset(collhash,  rb_str_new2("file"), rb_str_new2(coll->tdb->hdb->path));
        rb_hash_aset(collhash,  rb_str_new2("records"), INT2NUM(coll->tdb->hdb->rnum));

        VALUE options = rb_hash_new();
        rb_hash_aset(collhash,  rb_str_new2("options"), options);
        rb_hash_aset(options,  rb_str_new2("buckets"), INT2NUM(coll->tdb->hdb->bnum));
        rb_hash_aset(options,  rb_str_new2("cachedrecords"), INT2NUM(coll->tdb->hdb->rcnum));
        rb_hash_aset(options,  rb_str_new2("large"), coll->tdb->opts & TDBTLARGE ? Qtrue : Qfalse);
        rb_hash_aset(options,  rb_str_new2("compressed"), coll->tdb->opts & TDBTDEFLATE ? Qtrue : Qfalse);

        VALUE indexes = rb_ary_new();
        rb_hash_aset(collhash,  rb_str_new2("indexes"), indexes);
        for (j = 0; j < coll->tdb->inum; ++j) {
            TDBIDX *idx = (coll->tdb->idxs + j);
            if (idx->type != TDBITLEXICAL && idx->type != TDBITDECIMAL && idx->type != TDBITTOKEN) {
                continue;
            }
            VALUE imeta = rb_hash_new();
            rb_ary_push(indexes, imeta);
            rb_hash_aset(imeta,  rb_str_new2("filed"), rb_str_new2(idx->name + 1));
            rb_hash_aset(imeta,  rb_str_new2("iname"), rb_str_new2(idx->name));
            rb_hash_aset(imeta,  rb_str_new2("type"), rb_str_new2(
                idx->type == TDBITLEXICAL ? "lexical" :
                idx->type == TDBITDECIMAL ? "decimal" :
                idx->type == TDBITTOKEN ? "token" : ""
            ));

            TCBDB *idb = (TCBDB*) idx->db;
            if (idb) {
                rb_hash_aset(imeta,  rb_str_new2("records"), INT2NUM(idb->rnum));
                rb_hash_aset(imeta,  rb_str_new2("file"), rb_str_new2(idb->hdb->path));
            }
        }
    }
    rb_hash_aset(res,  rb_str_new2("file"), rb_str_new2(ejdb->metadb->hdb->path));
    return res;
}

void EJDB_sync(VALUE self) {
    EJDB* ejdb = getEJDB(self);
    if (!ejdbsyncdb(ejdb)) {
        raise_ejdb_error(ejdb);
    }
}

VALUE EJDB_get_transaction_status(VALUE self, VALUE collName) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    bool status;
    if (!ejdbtranstatus(coll, &status)) {
        raise_ejdb_error(ejdb);
    }

    return status ? Qtrue : Qfalse;
}

void EJDB_begin_transaction(VALUE self, VALUE collName) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    if (!ejdbtranbegin(coll)) {
        raise_ejdb_error(ejdb);
    }
}

void EJDB_commit_transaction(VALUE self, VALUE collName) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    if (!ejdbtrancommit(coll)) {
        raise_ejdb_error(ejdb);
    }
}

void EJDB_rollback_transaction(VALUE self, VALUE collName) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    if (!ejdbtranabort(coll)) {
        raise_ejdb_error(ejdb);
    }
}


VALUE EJDB_check_valid_oid_string(VALUE clazz, VALUE oid) {
    return TYPE(oid) == T_STRING && ejdbisvalidoidstr(StringValuePtr(oid)) ? Qtrue : Qfalse;
}


void close_ejdb_results_internal(RBEJDB_RESULTS* rbres) {
    tclistdel(rbres->results);
    if (rbres->log) {
        tcxstrdel(rbres->log);
    }
}

void EJDB_results_free(RBEJDB_RESULTS* rbres) {
    if (rbres->results) {
        close_ejdb_results_internal(rbres);
    }
    ruby_xfree(rbres);
}

VALUE create_EJDB_query_results(TCLIST* qres, TCXSTR *log) {
    VALUE results = Data_Wrap_Struct(ejdbResultsClass, NULL, EJDB_results_free, ruby_xmalloc(sizeof(RBEJDB_RESULTS)));
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(results, RBEJDB_RESULTS, rbresults);

    rbresults->results = qres;
    rbresults->log = log;

    return results;
}

void EJDB_results_each(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    if (!rbresults || !rbresults->results) {
        rb_raise(rb_eRuntimeError, "each() method called on invalid ejdb query results");
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

VALUE EJDB_results_log(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    return rbresults->log ? rb_str_new2(TCXSTRPTR(rbresults->log)) : Qnil;
}

void EJDB_results_close(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    close_ejdb_results_internal(rbresults);

    rbresults->results = NULL;
    rbresults->log = NULL;
}


VALUE EJDB_binary_init(VALUE self, VALUE bdata) {
    Check_Type(bdata, T_ARRAY);

    int length = NUM2INT(rb_funcall(bdata, rb_intern("length"), 0));
    int i;
    for (i = 0; i < length; i++) {
        VALUE byte = rb_ary_entry(bdata, i);
        if (NUM2INT(byte) > 255 || NUM2INT(byte) < 0) {
            rb_raise(rb_eRuntimeError, "Invalid value in binary array for EJDBBinary");
        }
    }

    rb_iv_set(self, "@data", rb_ary_dup(bdata));
}


static VALUE EJDB_block_proxy_context(VALUE yielded_object, VALUE context, int argc, VALUE argv[]){
    VALUE block = context;
    return rb_funcall(block, rb_intern("call"), 1, yielded_object);
}

void EJDB_binary_each(VALUE self) {
    VALUE bdata = rb_iv_get(self, "@data");
    Check_Type(bdata, T_ARRAY);

    VALUE block = rb_block_proc();
    rb_block_call(bdata, rb_intern("each"), 0, NULL, RUBY_METHOD_FUNC(EJDB_block_proxy_context), block);
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
    rb_define_method(ejdbClass, "find_one", RUBY_METHOD_FUNC(EJDB_find_one), -1);
    rb_define_method(ejdbClass, "update", RUBY_METHOD_FUNC(EJDB_update), -1);
    rb_define_method(ejdbClass, "remove", RUBY_METHOD_FUNC(EJDB_remove), 2);

    rb_define_method(ejdbClass, "drop_collection", RUBY_METHOD_FUNC(EJDB_drop_collection), -1);
    rb_define_method(ejdbClass, "ensure_collection", RUBY_METHOD_FUNC(EJDB_ensure_collection), -1);

    rb_define_method(ejdbClass, "drop_indexes", RUBY_METHOD_FUNC(EJDB_drop_indexes), 2);
    rb_define_method(ejdbClass, "optimize_indexes", RUBY_METHOD_FUNC(EJDB_optimize_indexes), 2);
    rb_define_method(ejdbClass, "ensure_string_index", RUBY_METHOD_FUNC(EJDB_ensure_string_index), 2);
    rb_define_method(ejdbClass, "rebuild_string_index", RUBY_METHOD_FUNC(EJDB_rebuild_string_index), 2);
    rb_define_method(ejdbClass, "drop_string_index", RUBY_METHOD_FUNC(EJDB_drop_string_index), 2);
    rb_define_method(ejdbClass, "ensure_istring_index", RUBY_METHOD_FUNC(EJDB_ensure_istring_index), 2);
    rb_define_method(ejdbClass, "rebuild_istring_index", RUBY_METHOD_FUNC(EJDB_rebuild_istring_index), 2);
    rb_define_method(ejdbClass, "drop_istring_index", RUBY_METHOD_FUNC(EJDB_drop_istring_index), 2);
    rb_define_method(ejdbClass, "ensure_number_index", RUBY_METHOD_FUNC(EJDB_ensure_number_index), 2);
    rb_define_method(ejdbClass, "rebuild_number_index", RUBY_METHOD_FUNC(EJDB_rebuild_number_index), 2);
    rb_define_method(ejdbClass, "drop_number_index", RUBY_METHOD_FUNC(EJDB_drop_number_index), 2);
    rb_define_method(ejdbClass, "ensure_array_index", RUBY_METHOD_FUNC(EJDB_ensure_array_index), 2);
    rb_define_method(ejdbClass, "rebuild_array_index", RUBY_METHOD_FUNC(EJDB_rebuild_array_index), 2);
    rb_define_method(ejdbClass, "drop_array_index", RUBY_METHOD_FUNC(EJDB_drop_array_index), 2);

    rb_define_method(ejdbClass, "get_db_meta", RUBY_METHOD_FUNC(EJDB_get_db_meta), 0);
    rb_define_method(ejdbClass, "sync", RUBY_METHOD_FUNC(EJDB_sync), 0);

    rb_define_method(ejdbClass, "get_transaction_status", RUBY_METHOD_FUNC(EJDB_get_transaction_status), 1);
    rb_define_method(ejdbClass, "begin_transaction", RUBY_METHOD_FUNC(EJDB_begin_transaction), 1);
    rb_define_method(ejdbClass, "commit_transaction", RUBY_METHOD_FUNC(EJDB_commit_transaction), 1);
    rb_define_method(ejdbClass, "rollback_transaction", RUBY_METHOD_FUNC(EJDB_rollback_transaction), 1);

    rb_define_singleton_method(ejdbClass, "check_valid_oid_string", RUBY_METHOD_FUNC(EJDB_check_valid_oid_string), 1);


    ejdbResultsClass = rb_define_class("EJDBResults", rb_cObject);
    rb_include_module(ejdbResultsClass, rb_mEnumerable);
    rb_define_method(ejdbResultsClass, "each", RUBY_METHOD_FUNC(EJDB_results_each), 0);
    rb_define_method(ejdbResultsClass, "log", RUBY_METHOD_FUNC(EJDB_results_log), 0);
    rb_define_method(ejdbResultsClass, "close", RUBY_METHOD_FUNC(EJDB_results_close), 0);


    ejdbBinaryClass = rb_define_class("EJDBBinary", rb_cObject);
    rb_include_module(ejdbBinaryClass, rb_mEnumerable);
    rb_define_private_method(ejdbBinaryClass, "initialize", RUBY_METHOD_FUNC(EJDB_binary_init), 1);
    rb_define_method(ejdbBinaryClass, "each", RUBY_METHOD_FUNC(EJDB_binary_each), 0);

    ejdbQueryClass = rb_define_class("EJDBQuery", rb_cObject);
}