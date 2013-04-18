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

/*
 * Document-class: EJDB
 * Main EJDB class that contains all database control methods. Instance should be created by EJDB::open
 */

/*
 * Document-class: EJDBResults
 * Class for accessing EJDB query resuts. Access to results is provided by methods of {Enumerble}[http://ruby-doc.org/core-1.9.1/Enumerable.html] mixin.
 * Instance of this class can be created only by calling EJDB.find method.
 */

/*
 * Document-class: EJDBBinary
 * Class for wrapping ruby binary data array to save as BSON binary. Access to data array is provided by methods of {Enumerble}[http://ruby-doc.org/core-1.9.1/Enumerable.html] mixin.
 *
 * Example:
 *  secret = EJDBBinary.new("Some binary secrect".encode("utf-8").bytes.to_a)
 *  our_string_back = secret.to_a.pack("U*")
 */

#include <tcejdb/ejdb_private.h>
#include <ruby.h>

#include "rbbson.h"

#define DEFAULT_OPEN_MODE (JBOWRITER | JBOCREAT | JBOTSYNC)

typedef struct {
    EJDB* ejdb;
} RBEJDB;

typedef struct {
    TCLIST* results;
    TCLIST* results_raw;
    int count;
    TCXSTR* log;
} RBEJDB_RESULTS;

typedef struct {
    bson* qbson;
    bson* hintsbson;
    int orarrlng;
    bson* orarrbson;
} RBEJDB_QUERY;


VALUE create_EJDB_query_results(TCLIST* qres, int count, TCXSTR *log);


VALUE ejdbClass;
VALUE ejdbResultsClass;
VALUE ejdbBinaryClass;
VALUE ejdbQueryClass;


void private_new_method_stub(VALUE clazz) {
    VALUE className = rb_inspect(clazz);
    rb_raise(rb_eRuntimeError, "%s is internal EJDB class and cannot be instantiated!", StringValuePtr(className));
}


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

int raise_ejdb_error(EJDB *ejdb) {
    int ecode = ejdbecode(ejdb);
    const char *emsg = ejdberrmsg(ecode);
    rb_raise(rb_eRuntimeError, "%s", emsg);
}

int nil_or_raise_ejdb_error(EJDB *ejdb) {
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
    rb_raise(rb_eRuntimeError, "EJDB.open() method should be used!");
    return self;
}

void EJDB_free(RBEJDB* rejdb) {
    if (rejdb->ejdb) {
        ejdbclose(rejdb->ejdb);
        ejdbdel(rejdb->ejdb);
    }
    ruby_xfree(rejdb);
}


/*
 * call-seq:
 *   EJDB::open(path, mode) -> EJDB
 *
 * Open database. Return database instance handle object. <br/>
 * Default open mode: JBOWRITER | JBOCREAT .
 *
 * - +path+ (String) - database main file name
 * - +mode+ (Number) - bitmask of open modes:
 *
 * [JBOREADER] Open as a reader.
 * [JBOWRITER] Open as a writer.
 * [JBOCREAT] Create if db file not exists
 * [JBOTRUNC] Truncate db.
 *
 */
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

/*
 * call-seq:
 *   ejdb.open? -> true|false
 *
 * Check if database in opened state.
 */
VALUE EJDB_is_open(VALUE self) {
    EJDB* ejdb = getEJDB(self);
    return ejdb && ejdbisopen(ejdb) ? Qtrue : Qfalse;
}


/*
 * call-seq:
 *   ejdb.close -> nil
 *
 * Close database.<br/>
 * If database was not opened it does nothing.
 */
VALUE EJDB_close(VALUE self) {
    ejdbclose(getEJDB(self));
    return Qnil;
}

/*
 * call-seq:
 *   ejdb.ensure_collection(collName, [copts]) -> nil
 *
 * Automatically creates new collection if it does't exists. Collection options +copts+ applied only for newly created collection.<br/>
 * For existing collections +copts+ takes no effect. <br/>
 *
 * Collection options (copts): <br/>
 * [:cachedrecords] Max number of cached records in shared memory segment. Default: 0
 * [:records] Estimated number of records in this collection. Default: 65535.
 * [:large] Specifies that the size of the database can be larger than 2GB. Default: false
 * [:compressed] If true collection records will be compressed with DEFLATE compression. Default: false.
 */
VALUE EJDB_ensure_collection(int argc, VALUE* argv, VALUE self) {
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
    return Qnil;
}

/*
 * call-seq:
 *   ejdb.drop_collection(collName, [prune=false]) -> nil
 *
 * Drop collection.
 * - +collName+ (String) - name of collection
 * - +prune+ (true|false) - if true the collection data will be erased from disk.
 */
VALUE EJDB_drop_collection(int argc, VALUE* argv, VALUE self) {
    VALUE collName;
    VALUE prune;

    rb_scan_args(argc, argv, "11", &collName, &prune);
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    if (!ejdbrmcoll(ejdb, StringValuePtr(collName), RTEST(prune))) {
        raise_ejdb_error(ejdb);        
    }
    return Qnil;
}

/*
 * call-seq:
 *   ejdb.save(collName, [obj1, …, objN, merge = false]) -> Array or Number or nil
 *
 * Save/update specified hashes or Ruby objects in the collection. If collection with +collName+ does not exists it will be created. <br/>
 * Each persistent object has unique identifier (OID) placed in the _id property. If a saved object does not have _id it will be autogenerated.
 * To identify and update object it should contains _id property.<br/><br/>
 *
 * NOTE: Field names of passed objects may not contain $ and . characters, error condition will be fired in this case.
 * - +collName+ (String) - name of collection
 * - +obj+ (Hash or Object) - one or more objects to save
 * - +merge+ (Hash or Object) - if true a saved objects will be merged with who's
 *
 * <br/>
 * Returns:
 * - oid of saved object, as string, if single object provided in arguments
 * - array of oids, in other case
 */
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

        if (i == argc - 1 && (TYPE(rbobj) == T_TRUE || TYPE(rbobj) == T_FALSE)) break;

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

/*
 * call-seq:
 *   ejdb.load(collName, oid) -> Hash or nil
 *
 * Loads JSON object identified by OID from the collection.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - object identifier (OID)
 *
 * <br/>
 * Returns:
 * - BSON object as hash
 * - nil, if it is not found
 */
VALUE EJDB_load(VALUE self, VALUE collName, VALUE rboid) {
    SafeStringValue(collName);
    SafeStringValue(rboid);

    EJDB* ejdb = getEJDB(self);

    EJCOLL *coll = ejdbgetcoll(ejdb, StringValuePtr(collName));
    if (!coll) {
        return nil_or_raise_ejdb_error(ejdb);
    }

    bson_oid_t oid = ruby_to_bson_oid(rboid);
    bson *bs = ejdbloadbson(coll, &oid);

    return bs ? bson_to_ruby_ensure_destroy(bs) : nil_or_raise_ejdb_error(ejdb);
}

/*
 * call-seq:
 *   ejdb.remove(collName, oid) -> nil
 *
 * Removes JSON object from the collection.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - object identifier (OID)
 */
VALUE EJDB_remove(VALUE self, VALUE collName, VALUE rboid) {
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
    return Qnil;
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
    prepare_query_hint(res, hints, "fields");
    return res;
}

VALUE EJDB_remove_query_internal(RBEJDB_QUERY* rbquery) {
    if (rbquery->qbson) {
        bson_destroy(rbquery->qbson);
        rbquery->qbson = NULL;
    }
    if (rbquery->hintsbson) {
        bson_destroy(rbquery->hintsbson);
        rbquery->hintsbson = NULL;
    }
    if (rbquery->orarrbson) {
        int i;
        for (i = 0; i < rbquery->orarrlng; i++) {
            bson_destroy(rbquery->orarrbson + i);
        }
        free(rbquery->orarrbson);
        rbquery->orarrbson = NULL;
    }
}

VALUE EJDB_query_free(RBEJDB_QUERY* rbquery) {
    ruby_xfree(rbquery);
}

VALUE EJDB_find_internal(VALUE self, VALUE collName, VALUE queryWrap, VALUE q, VALUE orarr, VALUE hints) {
    RBEJDB_QUERY* rbquery;
    Data_Get_Struct(queryWrap, RBEJDB_QUERY, rbquery);

    VALUE orarrlng = rb_funcall(orarr, rb_intern("length"), 0);
    rbquery->qbson = NULL;
    rbquery->hintsbson = NULL;
    rbquery->orarrbson = NUM2INT(orarrlng) ? (bson*) malloc(NUM2INT(orarrlng) * sizeof(bson)) : NULL;
    rbquery->orarrlng = 0;

    ruby_to_bson(q, &(rbquery->qbson), RUBY_TO_BSON_AS_QUERY);

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
            return !onlycount || explain ? create_EJDB_query_results(tclistnew2(1), 0, NULL) : INT2NUM(0);
        }
    }

    EJQ *ejq = ejdbcreatequery(ejdb, rbquery->qbson, rbquery->orarrbson, NUM2INT(orarrlng), rbquery->hintsbson);

    int count;
    int qflags = onlycount ? EJQONLYCOUNT : 0;
    TCXSTR *log = explain ? tcxstrnew() : NULL;

    TCLIST* qres = ejdbqryexecute(coll, ejq, &count, qflags, log);

    return !onlycount || explain ? create_EJDB_query_results(qres, count, log) : INT2NUM(count);
}

VALUE EJDB_find_internal_wrapper(VALUE args) {
    return EJDB_find_internal(rb_ary_pop(args), rb_ary_pop(args), rb_ary_pop(args),
                              rb_ary_pop(args), rb_ary_pop(args), rb_ary_pop(args));
}

VALUE EJDB_find_ensure(VALUE queryWrap, VALUE exception) {
    RBEJDB_QUERY* rbquery;
    Data_Get_Struct(queryWrap, RBEJDB_QUERY, rbquery);
    EJDB_remove_query_internal(rbquery);
}


/*
 * call-seq:
 *   ejdb.find(collName, [q = {}, orarr = [], hints = {}]) -> EJDBResults or Number
 *
 * Execute query on collection. EJDB queries inspired by MongoDB (mongodb.org) and follows same philosophy.
 * Both in query and in +hints+ strings or symbols may be used as keys (f. e. "fpath" and :fpath are equal).
 *  Supported queries:
 *  - Simple matching of String OR Number OR Array value:
 *    -   {"fpath" => "val", ...}
 *  - $not Negate operation.
 *    -   {"fpath" => {"$not" => val}} //Field not equal to val
 *    -   {"fpath" => {"$not" => {"$begin" => prefix}}} //Field not begins with val
 *  - $begin String starts with prefix
 *    -   {"fpath" => {"$begin" => prefix}}
 *  - $gt, $gte (>, >=) and $lt, $lte for number types:
 *    -   {"fpath" => {"$gt" => number}, ...}
 *  - $bt Between for number types:
 *    -   {"fpath" => {"$bt" => [num1, num2]}}
 *  - $in String OR Number OR Array val matches to value in specified array:
 *    -   {"fpath" => {"$in" => [val1, val2, val3]}}
 *  - $nin - Not IN
 *  - $strand String tokens OR String array val matches all tokens in specified array:
 *    -   {"fpath" => {"$strand" => [val1, val2, val3]}}
 *  - $stror String tokens OR String array val matches any token in specified array:
 *    -   {"fpath" => {"$stror" => [val1, val2, val3]}}
 *  - $exists Field existence matching:
 *    -   {"fpath" => {"$exists" => true|false}}
 *  - $icase Case insensitive string matching:
 *    -  {"fpath" => {"$icase" => "val1"}} //icase matching
 *    Ignore case matching with "$in" operation:
 *    -  {"name" => {"$icase" => {"$in" => ["tHéâtre - театр", "heLLo WorlD"]}}}
 *    For case insensitive matching you can create special type of string index.
 *  - $elemMatch The $elemMatch operator matches more than one component within an array element.
 *    -  { array=> { $elemMatch=> { value1 => 1, value2 => { $gt=> 1 } } } }
 *    Restriction: only one $elemMatch allowed in context of one array field.
 *
 *  Queries can be used to update records:
 *
 *  - $set Field set operation.
 *    - {.., "$set" => {"field1" => val1, "fieldN" => valN}}
 *  - $upsert Atomic upsert. If matching records are found it will be "$set" operation, otherwise new record will be
 *    inserted with fields specified by argment object.
 *     - {.., "$upsert" => {"field1" => val1, "fieldN" => valN}}
 *  - $inc Increment operation. Only number types are supported.
 *     - {.., "$inc" => {"field1" => number, ...,  "field1" => number}
 *  - $dropall In-place record removal operation.
 *     - {.., "$dropall" => true}
 *  - $addToSet Atomically adds value to the array only if its not in the array already. If containing array is missing it will be created.
 *     - {.., "$addToSet" => {"fpath" => val1, "fpathN" => valN, ...}}
 *  - $addToSetAll Batch version if $addToSet
 *     - {.., "$addToSetAll" => {"fpath" => [array of values to add], ...}}
 *  - $pull Atomically removes all occurrences of value from field, if field is an array.
 *     - {.., "$pull" => {"fpath" => val1, "fpathN" => valN, ...}}
 *  - $pullAll Batch version of $pull
 *     - {.., "$pullAll" => {"fpath" => [array of values to remove], ...}}
 *
 * <br/>
 * NOTE: It is better to execute update queries with `$onlycount=true` hint flag
 * or use the special +update+ method to avoid unnecessarily rows fetching.<br/><br/>
 *
 * NOTE: Negate operations: $not and $nin not using indexes
 * so they can be slow in comparison to other matching operations.<br/><br/>
 *
 * NOTE: Only one index can be used in search query operation.<br/><br/>
 *
 * QUERY HINTS (specified by +hints+ argument):
 * [:max] Maximum number in the result set
 * [:skip] Number of skipped results in the result set
 * [:orderby] Sorting order of query fields.
 * [:onlycount] If `true` only count of matching records will be returned without placing records in result set.
 * [:fields] Set subset of fetched fields
 * If a field presented in +:orderby+ clause it will be forced to include in resulting records.<br/>
 * Example:<br/>
 *  hints = {
 *    :orderby => { //ORDER BY field1 ASC, field2 DESC
 *        "field1" => 1,
 *        "field2" => -1
 *    },
 *    :fields => { //SELECT ONLY {_id, field1, field2}
 *        "field1" => 1,
 *        "field2" => 1
 *    }
 *  }
 *
 * Many C API query examples can be found in `tcejdb/testejdb/t2.c` test case.<br/><br/>
 *
 * - +collName+ (String) - name of collection
 * - +q+ (Hash or Object) - query object. In most cases it will be easier to use hash to specify EJDB queries
 * - +orarr+ (Array) - array of additional OR query objects (joined with OR predicate). If 3rd argument is not array it
 * will be recognized as +hints+ argument
 * - +hints+ (Hash or Object) - object with query hints
 *
 * <br/>
 * Returns:
 * - EJDBResults object, if no +:onlycount+ hint specified or :explain hint specified
 * - results count as Number, otherwise
 *
 */
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
    rbquery->qbson = NULL;
    rbquery->hintsbson = NULL;
    rbquery->orarrbson = NULL;
    rbquery->orarrlng = 0;

    VALUE params = rb_ary_new();
    rb_ary_push(params, self);
    rb_ary_push(params, collName);
    rb_ary_push(params, queryWrap);
    rb_ary_push(params, q);
    rb_ary_push(params, orarr);
    rb_ary_push(params, hints);

    // Even if exception raised during find() we will free memory, taken for query
    return rb_ensure(EJDB_find_internal_wrapper, params, EJDB_find_ensure, queryWrap);
}

VALUE EJDB_block_true(VALUE yielded_object, VALUE context, int argc, VALUE argv[]){
    return Qtrue;
}


/*
 * call-seq:
 *   ejdb.find_one(collName, oid) -> Hash or Number or nil
 *
 * Same as +find+ but retrieves only one matching object.
 *
 * - +collName+ (String) - name of collection
 * - +q+ (Hash or Object) - query object. In most cases it will be easier to use hash to specify EJDB queries
 * - +orarr+ (Array) - array of additional OR query objects (joined with OR predicate). If 3rd argument is not array it
 * will be recognized as +hints+ argument
 * - +hints+ (Hash or Object) - object with query hints
 *
 * <br/>
 * Returns:
 * - found object as hash, if no +:onlycount+ hint specified
 * - nil, if no +:onlycount+ hint specified and nothing found
 * - results count as Number, otherwise
 */
VALUE EJDB_find_one(int argc, VALUE* argv, VALUE self) {
    VALUE results = EJDB_find(argc, argv, self);
    if (TYPE(results) == T_DATA) {
        return rb_block_call(results, rb_intern("find"), 0, NULL, RUBY_METHOD_FUNC(EJDB_block_true), Qnil); // "find" with "always true" block gets first element
    }
    return results;
}


/*
 * call-seq:
 *   ejdb.update(collName, oid) -> EJDBResults or Number
 *
 * Convenient method to execute update queries.
 *
 *  - $set Field set operation:
 *    - {some fields for selection, "$set" => {"field1" => obj, ..., "fieldN" => obj}}
 *  - $upsert Atomic upsert. If matching records are found it will be "$set" operation, otherwise new record will be inserted with fields specified by argment object.
 *    - {.., "$upsert" => {"field1" => val1, "fieldN" => valN}}
 *  - $inc Increment operation. Only number types are supported.
 *    - {some fields for selection, "$inc" => {"field1" => number, ..., "fieldN" => number}
 *  - $dropall In-place record removal operation.
 *    - {some fields for selection, "$dropall" => true}
 *  - $addToSet | $addToSetAll Atomically adds value to the array only if its not in the array already. If containing array is missing it will be created.
 *    - {.., "$addToSet" => {"fpath" => val1, "fpathN" => valN, ...}}
 *  - $pull | pullAll Atomically removes all occurrences of value from field, if field is an array.
 *    - {.., "$pull" => {"fpath" => val1, "fpathN" => valN, ...}}
 *
 * <br/>
 * - +collName+ (String) - name of collection
 * - +q+ (Hash or Object) - query object. In most cases it will be easier to use hash to specify EJDB queries
 * - +orarr+ (Array) - array of additional OR query objects (joined with OR predicate). If 3rd argument is not array it
 * will be recognized as +hints+ argument
 * - +hints+ (Hash or Object) - object with query hints
 *
 * <br/>
 * Returns:
 * Returns:
 * - EJDBResults object, with only +count+ and +log+ methods available if :explain hint specified
 * - updated objects count as Number, otherwise
 */
VALUE EJDB_update(int argc, VALUE* argv, VALUE self) {
    VALUE collName;
    VALUE q;
    VALUE orarr;
    VALUE hints;

    VALUE p3;
    VALUE p4;

    rb_scan_args(argc, argv, "13", &collName, &q, &p3, &p4);

    orarr = TYPE(p3) == T_ARRAY ? p3 : rb_ary_new();
    hints = TYPE(p3) != T_ARRAY ? p3 : p4;
    hints = !NIL_P(hints) ? hints : rb_hash_new();

    Check_Type(hints, T_HASH);
    rb_hash_aset(hints, rb_str_new2("onlycount"), Qtrue);

    VALUE findargs[4] = {collName, q, orarr, hints};
    return EJDB_find(4, findargs, self);
}



VALUE EJDB_set_index_internal(VALUE self, VALUE collName, VALUE fpath, int flags) {
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
    return Qnil;
}

/*
 * call-seq:
 *   ejdb.drop_indexes(collName, fpath) -> nil
 *
 * Drop indexes of all types for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_drop_indexes(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXDROPALL);
};

/*
 * call-seq:
 *   ejdb.optimize_indexes(collName, fpath) -> nil
 *
 * Optimize indexes of all types for BSON field path. Performs B+ tree index file optimization.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_optimize_indexes(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXOP);
};

/*
 * call-seq:
 *   ejdb.ensure_string_index(collName, fpath) -> nil
 *
 * Ensure index presence of String type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_ensure_string_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXSTR);
}

/*
 * call-seq:
 *   ejdb.rebuild_string_index(collName, fpath) -> nil
 *
 * Rebuild index of String type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_rebuild_string_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXSTR | JBIDXREBLD);
}

/*
 * call-seq:
 *   ejdb.drop_string_index(collName, fpath) -> nil
 *
 * Drop index of String type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_drop_string_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXSTR | JBIDXDROP);
}

/*
 * call-seq:
 *   ejdb.ensure_istring_index(collName, fpath) -> nil
 *
 * Ensure index presence of IString type for BSON field path.<br/>
 * IString is the special type of String index for case insensitive matching.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_ensure_istring_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXISTR);
}

/*
 * call-seq:
 *   ejdb.rebuild_istring_index(collName, fpath) -> nil
 *
 * Rebuild index of IString type for BSON field path.<br/>
 * IString is the special type of String index for case insensitive matching.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_rebuild_istring_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXISTR | JBIDXREBLD);
}

/*
 * call-seq:
 *   ejdb.drop_istring_index(collName, fpath) -> nil
 *
 * Drop index of IString type for BSON field path.<br/>
 * IString is the special type of String index for case insensitive matching.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_drop_istring_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXISTR | JBIDXDROP);
}

/*
 * call-seq:
 *   ejdb.ensure_number_index(collName, fpath) -> nil
 *
 * Ensure index of Number type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_ensure_number_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXNUM);
}

/*
 * call-seq:
 *   ejdb.rebuild_number_index(collName, fpath) -> nil
 *
 * Rebuild index of Number type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_rebuild_number_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXNUM | JBIDXREBLD);
}

/*
 * call-seq:
 *   ejdb.drop_number_index(collName, fpath) -> nil
 *
 * Drop index of Number type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_drop_number_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXNUM | JBIDXDROP);
}

/*
 * call-seq:
 *   ejdb.ensure_array_index(collName, fpath) -> nil
 *
 * Ensure index presence of Array type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_ensure_array_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXARR);
}

/*
 * call-seq:
 *   ejdb.rebuild_array_index(collName, fpath) -> nil
 *
 * Rebuild index of Array type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_rebuild_array_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXARR | JBIDXREBLD);
}

/*
 * call-seq:
 *   ejdb.drop_array_index(collName, fpath) -> nil
 *
 * Drop index of Array type for BSON field path.
 *
 * - +collName+ (String) - name of collection
 * - +oid+ (String) - BSON field path
 */
VALUE EJDB_drop_array_index(VALUE self, VALUE collName, VALUE fpath) {
    return EJDB_set_index_internal(self, collName, fpath, JBIDXARR | JBIDXDROP);
}

/*
 * call-seq:
 *   ejdb.get_db_meta -> Hash
 *
 * Get hash that describes a database structure and collections.<br/>
 * Sample meta:
 *  {
 *    "collections" => [
 *      {
 *        "file" => "testdb_ecoll",
 *        "indexes" => [],
 *        "name" => "ecoll",
 *        "options" => {
 *          "buckets" => 425977,
 *          "cachedrecords" => 0,
 *          "compressed" => false,
 *          "large" => true
 *        },
 *        "records" => 1
 *      },
 *      {
 *        "file" => "testdb_mycoll",
 *        "indexes" => [ {
 *          "field" => "foo",
 *          "file" => "testdb_mycoll.idx.sfoo.lex",
 *          "iname" => "sfoo",
 *          "records" => 3,
 *          "type" => "lexical"
 *         }],
 *        "name" => "mycoll",
 *        "options" => {
 *          "buckets" => 131071,
 *          "cachedrecords" => 0,
 *          "compressed" => false,
 *          "large" => false
 *        },
 *        "records" => 4
 *      }
 *    ],
 *    "file" => "testdb"
 *  }
 */
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

    tclistdel(cols);

    return res;
}

/*
 * call-seq:
 *   ejdb.sync -> nil
 *
 * Synchronize entire EJDB database with disk.
 */
VALUE EJDB_sync(VALUE self) {
    EJDB* ejdb = getEJDB(self);
    if (!ejdbsyncdb(ejdb)) {
        raise_ejdb_error(ejdb);
    }
    return Qnil;
}

/*
 * call-seq:
 *   ejdb.get_transaction_status(collName) -> true|false
 *
 * Get collection transaction status. Returns true if transaction is active.
 *
 * - +collName+ (String) - name of collection
 */
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

/*
 * call-seq:
 *   ejdb.begin_transaction(collName) -> nil
 *
 * Begin collection transaction.
 *
 * - +collName+ (String) - name of collection
 */
VALUE EJDB_begin_transaction(VALUE self, VALUE collName) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    if (!ejdbtranbegin(coll)) {
        raise_ejdb_error(ejdb);
    }
    return Qnil;
}

/*
 * call-seq:
 *   ejdb.commit_transaction(collName) -> nil
 *
 * Commit collection transaction.
 *
 * - +collName+ (String) - name of collection
 */
VALUE EJDB_commit_transaction(VALUE self, VALUE collName) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    if (!ejdbtrancommit(coll)) {
        raise_ejdb_error(ejdb);
    }
    return Qnil;
}

/*
 * call-seq:
 *   ejdb.rollback_transaction(collName) -> nil
 *
 * Rollback collection transaction.
 *
 * - +collName+ (String) - name of collection
 */
VALUE EJDB_rollback_transaction(VALUE self, VALUE collName) {
    SafeStringValue(collName);

    EJDB* ejdb = getEJDB(self);
    EJCOLL *coll = ejdbcreatecoll(ejdb, StringValuePtr(collName), NULL);
    if (!coll) {
        raise_ejdb_error(ejdb);
    }

    if (!ejdbtranabort(coll)) {
        raise_ejdb_error(ejdb);
    }
    return Qnil;
}

/*
 * call-seq:
 *   EJDB::valid_oid_string?(oid) -> true|false
 *
 * Returns true if argument string contains valid oid. <br/>
 *
 * - +oid+ (String) - oid as string
 */
VALUE EJDB_is_valid_oid_string(VALUE clazz, VALUE oid) {
    return TYPE(oid) == T_STRING && ejdbisvalidoidstr(StringValuePtr(oid)) ? Qtrue : Qfalse;
}


void close_ejdb_results_internal(RBEJDB_RESULTS* rbres) {
    if (rbres->results) {
        int i;
        for (i = 0; i < TCLISTNUM(rbres->results); i++) {
            bson* bsonval = *(bson**) TCLISTVALPTR(rbres->results, i);
            bson_dispose(bsonval);
        }

        tclistdel(rbres->results);
        rbres->results = NULL;
    }
    if (rbres->results_raw) {
        tclistdel(rbres->results_raw);
        rbres->results_raw = NULL;
    }
    if (rbres->log) {
        tcxstrdel(rbres->log);
        rbres->log = NULL;
    }
}

void EJDB_results_free(RBEJDB_RESULTS* rbres) {
    close_ejdb_results_internal(rbres);
    ruby_xfree(rbres);
}

VALUE create_EJDB_query_results(TCLIST* qres, int count, TCXSTR *log) {
    VALUE resultsWrap = Data_Wrap_Struct(ejdbResultsClass, NULL, EJDB_results_free, ruby_xmalloc(sizeof(RBEJDB_RESULTS)));
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(resultsWrap, RBEJDB_RESULTS, rbresults);

    TCLIST* results = tclistnew2(TCLISTNUM(qres));
    int i;
    for (i = 0; i < TCLISTNUM(qres); i++) {
        char* bsrawdata = TCLISTVALPTR(qres, i);
        bson* bsonval = bson_create();
        bson_init_finished_data(bsonval, bsrawdata);
        tclistpush(results, &bsonval, sizeof(bson*));
    }

    rbresults->results = results;
    rbresults->results_raw = qres;
    rbresults->count = count;
    rbresults->log = log;

    return resultsWrap;
}

void EJDB_results_each(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    if (!rbresults || !rbresults->results) {
        rb_raise(rb_eRuntimeError, "each() method called on invalid ejdb query results");
    }

    TCLIST* results = rbresults->results;
    int i;
    for (i = 0; i < TCLISTNUM(results); i++) {
        bson* bsonval = *(bson**) TCLISTVALPTR(results, i);
        rb_yield(bson_to_ruby(bsonval));
    }
}

/*
 * call-seq:
 *   results.count -> Number
 *
 * Returns total number of query result objects.
 */
VALUE EJDB_results_count(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    if (!rbresults) {
        rb_raise(rb_eRuntimeError, "count() method called on invalid ejdb query results");
    }

    return INT2NUM(rbresults->count);
}

/*
 * call-seq:
 *   results.log -> String or nil
 *
 * Returns query log.
 * To get this log +:explain+ option must be passed to +hints+ argument of query.
 * Otherwise method returns +nil+.
 */
VALUE EJDB_results_log(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    return rbresults->log ? rb_str_new2(TCXSTRPTR(rbresults->log)) : Qnil;
}

/*
 * call-seq:
 *   results.close -> nil
 *
 * Closes query results and immediately frees memory taken for results.
 * Calling this method invalidates results container and any further access attempts will cause +RuntimeError+.
 */
VALUE EJDB_results_close(VALUE self) {
    RBEJDB_RESULTS* rbresults;
    Data_Get_Struct(self, RBEJDB_RESULTS, rbresults);

    close_ejdb_results_internal(rbresults);
    return Qnil;
}

/*
 * call-seq:
 *   EJDBBinary.new(bdata) -> EJDBBinary
 *
 * - +bdata+ (Array) - array of binary data. All elements of array must be numbers in range [0..255]
 *
 */
VALUE EJDB_binary_init(VALUE self, VALUE bdata) {
    Check_Type(bdata, T_ARRAY);

    int length = NUM2INT(rb_funcall(bdata, rb_intern("length"), 0));
    int i;
    for (i = 0; i < length; i++) {
        VALUE byte = rb_ary_entry(bdata, i);
        if (NUM2INT(byte) > 255 || NUM2INT(byte) < 0) {
            rb_raise(rb_eRuntimeError, "Invalid value in binary array for EJDBBinary: %d", NUM2INT(byte));
        }
    }

    rb_iv_set(self, "@data", rb_ary_dup(bdata));
    return self;
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
    rb_define_singleton_method(ejdbClass, "new", RUBY_METHOD_FUNC(EJDB_new), 0);

    rb_define_const(ejdbClass, "DEFAULT_OPEN_MODE", INT2FIX(DEFAULT_OPEN_MODE));
    rb_define_const(ejdbClass, "JBOREADER", INT2FIX(JBOREADER));
    rb_define_const(ejdbClass, "JBOWRITER", INT2FIX(JBOWRITER));
    rb_define_const(ejdbClass, "JBOCREAT", INT2FIX(JBOCREAT));
    rb_define_const(ejdbClass, "JBOTSYNC", INT2FIX(JBOTSYNC));
    rb_define_const(ejdbClass, "JBOTRUNC", INT2FIX(JBOTRUNC));

    rb_define_singleton_method(ejdbClass, "open", RUBY_METHOD_FUNC(EJDB_open), 2);
    rb_define_method(ejdbClass, "open?", RUBY_METHOD_FUNC(EJDB_is_open), 0);
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

    rb_define_singleton_method(ejdbClass, "valid_oid_string?", RUBY_METHOD_FUNC(EJDB_is_valid_oid_string), 1);


    ejdbResultsClass = rb_define_class("EJDBResults", rb_cObject);
    rb_define_singleton_method(ejdbResultsClass, "new", RUBY_METHOD_FUNC(private_new_method_stub), 0);
    rb_include_module(ejdbResultsClass, rb_mEnumerable);
    rb_define_method(ejdbResultsClass, "each", RUBY_METHOD_FUNC(EJDB_results_each), 0);
    rb_define_method(ejdbResultsClass, "count", RUBY_METHOD_FUNC(EJDB_results_count), 0);
    rb_define_method(ejdbResultsClass, "log", RUBY_METHOD_FUNC(EJDB_results_log), 0);
    rb_define_method(ejdbResultsClass, "close", RUBY_METHOD_FUNC(EJDB_results_close), 0);


    ejdbBinaryClass = rb_define_class("EJDBBinary", rb_cObject);
    rb_include_module(ejdbBinaryClass, rb_mEnumerable);
    rb_define_private_method(ejdbBinaryClass, "initialize", RUBY_METHOD_FUNC(EJDB_binary_init), 1);
    rb_define_method(ejdbBinaryClass, "each", RUBY_METHOD_FUNC(EJDB_binary_each), 0);

    /*
     * Internal EJDB class. :nodoc:
     */
    ejdbQueryClass = rb_define_class("EJDBQuery", rb_cObject);
}