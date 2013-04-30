/**************************************************************************************************
 *  EJDB database library http://ejdb.org
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

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <ejdb_private.h>

#include "ejdb_args.h"
#include "ejdb_cmd.h"
#include "ejdb_logging.h"
#include "ejdb_thread.h"

#include <math.h>
#include <vector>
#include <sstream>
#include <locale.h>
#include <stdint.h>
#include <unordered_set>

using namespace node;
using namespace v8;

static const int CMD_RET_ERROR = 1;

#define DEFINE_INT64_CONSTANT(target, constant)                       \
  (target)->Set(String::NewSymbol(#constant),                         \
                Number::New((int64_t) constant),                      \
                static_cast<PropertyAttribute>(                       \
                    ReadOnly|DontDelete))

namespace ejdb {

    ///////////////////////////////////////////////////////////////////////////
    //                           Some symbols                                //
    ///////////////////////////////////////////////////////////////////////////


    static Persistent<String> sym_large;
    static Persistent<String> sym_compressed;
    static Persistent<String> sym_records;
    static Persistent<String> sym_cachedrecords;
    static Persistent<String> sym_explain;
    static Persistent<String> sym_merge;

    static Persistent<String> sym_name;
    static Persistent<String> sym_iname;
    static Persistent<String> sym_field;
    static Persistent<String> sym_indexes;
    static Persistent<String> sym_options;
    static Persistent<String> sym_file;
    static Persistent<String> sym_buckets;
    static Persistent<String> sym_type;


    ///////////////////////////////////////////////////////////////////////////
    //                             Fetch functions                           //
    ///////////////////////////////////////////////////////////////////////////

    enum eFetchStatus {
        FETCH_NONE,
        FETCH_DEFAULT,
        FETCH_VAL
    };

    char* fetch_string_data(Handle<Value> sobj, eFetchStatus* fs, const char* def) {
        HandleScope scope;
        if (sobj->IsNull() || sobj->IsUndefined()) {
            if (fs) {
                *fs = FETCH_DEFAULT;
            }
            return def ? strdup(def) : strdup("");
        }
        String::Utf8Value value(sobj);
        const char* data = *value;
        if (fs) {
            *fs = FETCH_VAL;
        }
        return data ? strdup(data) : strdup("");
    }

    int64_t fetch_int_data(Handle<Value> sobj, eFetchStatus* fs, int64_t def) {
        HandleScope scope;
        if (!(sobj->IsNumber() || sobj->IsInt32() || sobj->IsUint32())) {
            if (fs) {
                *fs = FETCH_DEFAULT;
            }
            return def;
        }
        if (fs) {
            *fs = FETCH_VAL;
        }
        return sobj->ToNumber()->IntegerValue();
    }

    bool fetch_bool_data(Handle<Value> sobj, eFetchStatus* fs, bool def) {
        HandleScope scope;
        if (sobj->IsNull() || sobj->IsUndefined()) {
            if (fs) {
                *fs = FETCH_DEFAULT;
            }
            return def;
        }
        if (fs) {
            *fs = FETCH_VAL;
        }
        return sobj->BooleanValue();
    }

    double fetch_real_data(Handle<Value> sobj, eFetchStatus* fs, double def) {
        HandleScope scope;
        if (!(sobj->IsNumber() || sobj->IsInt32())) {
            if (fs) {
                *fs = FETCH_DEFAULT;
            }
            return def;
        }
        if (fs) {
            *fs = FETCH_VAL;
        }
        return sobj->ToNumber()->NumberValue();
    }

	struct V8ObjHash {

        size_t operator()(const Handle<Object>& obj) const {
            return (size_t) obj->GetIdentityHash();
        }
    };
    struct V8ObjEq {

        bool operator()(const Handle<Object>& o1, const Handle<Object>& o2) const {
            return (o1 == o2);
        }
    };

	typedef std::unordered_set<Handle<Object>, V8ObjHash, V8ObjEq> V8ObjSet;


    struct TBSONCTX {
        V8ObjSet tset; //traversed objects set
        int nlevel;
        bool inquery;

        TBSONCTX() : nlevel(0), inquery(false) {
        }
    };

    static Handle<Object> toV8Object(bson_iterator *it, bson_type obt = BSON_OBJECT);
    static Handle<Value> toV8Value(bson_iterator *it);
    static void toBSON0(Handle<Object> obj, bson *bs, TBSONCTX *ctx);

    static Handle<Value> toV8Value(bson_iterator *it) {
        HandleScope scope;
        bson_type bt = bson_iterator_type(it);

        switch (bt) {
            case BSON_OID:
            {
                char xoid[25];
                bson_oid_to_string(bson_iterator_oid(it), xoid);
                return scope.Close(String::New(xoid, 24));
            }
            case BSON_STRING:
            case BSON_SYMBOL:
                return scope.Close(String::New(bson_iterator_string(it), bson_iterator_string_len(it) - 1));
            case BSON_NULL:
                return scope.Close(Null());
            case BSON_UNDEFINED:
                return scope.Close(Undefined());
            case BSON_INT:
                return scope.Close(Integer::New(bson_iterator_int_raw(it)));
            case BSON_LONG:
                return scope.Close(Number::New((double) bson_iterator_long_raw(it)));
            case BSON_DOUBLE:
                return scope.Close(Number::New(bson_iterator_double_raw(it)));
            case BSON_BOOL:
                return scope.Close(Boolean::New(bson_iterator_bool_raw(it) ? true : false));
            case BSON_OBJECT:
            case BSON_ARRAY:
            {
                bson_iterator nit;
                bson_iterator_subiterator(it, &nit);
                return scope.Close(toV8Object(&nit, bt));
            }
            case BSON_DATE:
                return scope.Close(Date::New((double) bson_iterator_date(it)));
            case BSON_BINDATA:
                //TODO test it!
                return scope.Close(Buffer::New(String::New(bson_iterator_bin_data(it), bson_iterator_bin_len(it))));
            case BSON_REGEX:
            {
                const char *re = bson_iterator_regex(it);
                const char *ro = bson_iterator_regex_opts(it);
                int rflgs = RegExp::kNone;
                for (int i = (strlen(ro) - 1); i >= 0; --i) {
                    if (ro[i] == 'i') {
                        rflgs |= RegExp::kIgnoreCase;
                    } else if (ro[i] == 'g') {
                        rflgs |= RegExp::kGlobal;
                    } else if (ro[i] == 'm') {
                        rflgs |= RegExp::kMultiline;
                    }
                }
                return scope.Close(RegExp::New(String::New(re), (RegExp::Flags) rflgs));
            }
            default:
                break;
        }
        return scope.Close(Undefined());
    }

    static Handle<Object> toV8Object(bson_iterator *it, bson_type obt) {
        HandleScope scope;
        Local<Object> ret;
        uint32_t knum = 0;
        if (obt == BSON_ARRAY) {
            ret = Array::New();
        } else if (obt == BSON_OBJECT) {
            ret = Object::New();
        } else {
            assert(0);
        }
        bson_type bt;
        while ((bt = bson_iterator_next(it)) != BSON_EOO) {
            const char *key = bson_iterator_key(it);
            if (obt == BSON_ARRAY) {
                knum = (uint32_t) tcatoi(key);
            }
            switch (bt) {
                case BSON_OID:
                {
                    char xoid[25];
                    bson_oid_to_string(bson_iterator_oid(it), xoid);
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, String::New(xoid, 24));
                    } else {
                        ret->Set(String::New(key), String::New(xoid, 24));
                    }
                    break;
                }
                case BSON_STRING:
                case BSON_SYMBOL:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum,
                                String::New(bson_iterator_string(it), bson_iterator_string_len(it) - 1));
                    } else {
                        ret->Set(String::New(key),
                                String::New(bson_iterator_string(it), bson_iterator_string_len(it) - 1));
                    }
                    break;
                case BSON_NULL:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Null());
                    } else {
                        ret->Set(String::New(key), Null());
                    }
                    break;
                case BSON_UNDEFINED:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Undefined());
                    } else {
                        ret->Set(String::New(key), Undefined());
                    }
                    break;
                case BSON_INT:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Integer::New(bson_iterator_int_raw(it)));
                    } else {
                        ret->Set(String::New(key), Integer::New(bson_iterator_int_raw(it)));
                    }
                    break;
                case BSON_LONG:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Number::New((double) bson_iterator_long_raw(it)));
                    } else {
                        ret->Set(String::New(key), Number::New((double) bson_iterator_long_raw(it)));
                    }
                    break;
                case BSON_DOUBLE:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Number::New(bson_iterator_double_raw(it)));
                    } else {
                        ret->Set(String::New(key), Number::New(bson_iterator_double_raw(it)));
                    }
                    break;
                case BSON_BOOL:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Boolean::New(bson_iterator_bool_raw(it) ? true : false));
                    } else {
                        ret->Set(String::New(key), Boolean::New(bson_iterator_bool_raw(it) ? true : false));
                    }
                    break;
                case BSON_OBJECT:
                case BSON_ARRAY:
                {
                    bson_iterator nit;
                    bson_iterator_subiterator(it, &nit);
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, toV8Object(&nit, bt));
                    } else {
                        ret->Set(String::New(key), toV8Object(&nit, bt));
                    }
                    break;
                }
                case BSON_DATE:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Date::New((double) bson_iterator_date(it)));
                    } else {
                        ret->Set(String::New(key), Date::New((double) bson_iterator_date(it)));
                    }
                    break;
                case BSON_BINDATA:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum,
                                Buffer::New(String::New(bson_iterator_bin_data(it),
                                bson_iterator_bin_len(it))));
                    } else {
                        ret->Set(String::New(key),
                                Buffer::New(String::New(bson_iterator_bin_data(it),
                                bson_iterator_bin_len(it))));
                    }
                    break;
                case BSON_REGEX:
                {
                    const char *re = bson_iterator_regex(it);
                    const char *ro = bson_iterator_regex_opts(it);
                    int rflgs = RegExp::kNone;
                    for (int i = (strlen(ro) - 1); i >= 0; --i) {
                        if (ro[i] == 'i') {
                            rflgs |= RegExp::kIgnoreCase;
                        } else if (ro[i] == 'g') {
                            rflgs |= RegExp::kGlobal;
                        } else if (ro[i] == 'm') {
                            rflgs |= RegExp::kMultiline;
                        }
                    }
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, RegExp::New(String::New(re), (RegExp::Flags) rflgs));
                    } else {
                        ret->Set(String::New(key), RegExp::New(String::New(re), (RegExp::Flags) rflgs));
                    }
                    break;
                }
                default:
                    if (obt == BSON_ARRAY) {
                        ret->Set(knum, Undefined());
                    } else {
                        ret->Set(String::New(key), Undefined());
                    }
                    break;
            }
        }
        return scope.Close(ret);
    }

    static void toBSON0(Handle<Object> obj, bson *bs, TBSONCTX *ctx) {
        HandleScope scope;
        assert(ctx && obj->IsObject());
        V8ObjSet::iterator it = ctx->tset.find(obj);
        if (it != ctx->tset.end()) {
            bs->err = BSON_ERROR_ANY;
            bs->errstr = strdup("Converting circular structure to JSON");
            return;
        }
        ctx->nlevel++;
        ctx->tset.insert(obj);
        Local<Array> pnames = obj->GetOwnPropertyNames();
        for (uint32_t i = 0; i < pnames->Length(); ++i) {
            if (bs->err) {
                break;
            }
            Local<Value> pn = pnames->Get(i);
            String::Utf8Value spn(pn);
            Local<Value> pv = obj->Get(pn);

            if (!ctx->inquery && ctx->nlevel == 1 && !strcmp(JDBIDKEYNAME, *spn)) { //top level _id key
                if (pv->IsNull() || pv->IsUndefined()) { //skip _id addition for null or undefined vals
                    continue;
                }
                String::Utf8Value idv(pv->ToString());
                if (ejdbisvalidoidstr(*idv)) {
                    bson_oid_t oid;
                    bson_oid_from_string(&oid, *idv);
                    bson_append_oid(bs, JDBIDKEYNAME, &oid);
                } else {
                    bs->err = BSON_ERROR_ANY;
                    bs->errstr = strdup("Invalid bson _id field value");
                    break;
                }
            }
            if (pv->IsString()) {
                String::Utf8Value val(pv);
                bson_append_string(bs, *spn, *val);
            } else if (pv->IsInt32()) {
                bson_append_int(bs, *spn, pv->Int32Value());
            } else if (pv->IsUint32()) {
                bson_append_long(bs, *spn, pv->Uint32Value());
            } else if (pv->IsNumber()) {
                double nv = pv->NumberValue();
                double ipart;
                if (modf(nv, &ipart) == 0.0) {
                    bson_append_long(bs, *spn, pv->IntegerValue());
                } else {
                    bson_append_double(bs, *spn, nv);
                }
            } else if (pv->IsNull()) {
                bson_append_null(bs, *spn);
            } else if (pv->IsUndefined()) {
                bson_append_undefined(bs, *spn);
            } else if (pv->IsBoolean()) {
                bson_append_bool(bs, *spn, pv->BooleanValue());
            } else if (pv->IsDate()) {
                bson_append_date(bs, *spn, Handle<Date>::Cast(pv)->IntegerValue());
            } else if (pv->IsRegExp()) {
                Handle<RegExp> regexp = Handle<RegExp>::Cast(pv);
                int flags = regexp->GetFlags();
                String::Utf8Value sr(regexp->GetSource());
                std::string sf;
                if (flags & RegExp::kIgnoreCase) {
                    sf.append("i");
                }
                if (flags & RegExp::kGlobal) {
                    sf.append("g");
                }
                if (flags & RegExp::kMultiline) {
                    sf.append("m");
                }
                bson_append_regex(bs, *spn, *sr, sf.c_str());
            } else if (Buffer::HasInstance(pv)) {
                bson_append_binary(bs, *spn, BSON_BIN_BINARY,
                        Buffer::Data(Handle<Object>::Cast(pv)),
                        Buffer::Length(Handle<Object>::Cast(pv)));
            } else if (pv->IsObject() || pv->IsArray()) {
                if (pv->IsArray()) {
                    bson_append_start_array(bs, *spn);
                } else {
                    bson_append_start_object(bs, *spn);
                }
                toBSON0(Handle<Object>::Cast(pv), bs, ctx);
                if (bs->err) {
                    break;
                }
                if (pv->IsArray()) {
                    bson_append_finish_array(bs);
                } else {
                    bson_append_finish_object(bs);
                }
            }
        }
        ctx->nlevel--;
        it = ctx->tset.find(obj);
        if (it != ctx->tset.end()) {
            ctx->tset.erase(it);
        }
    }

    /** Convert V8 object into binary json instance. After usage, it must be freed by bson_del() */
    static void toBSON(Handle<Object> obj, bson *bs, bool inquery) {
        HandleScope scope;
        TBSONCTX ctx;
        ctx.inquery = inquery;
        toBSON0(obj, bs, &ctx);
    }

    class NodeEJDBCursor;
    class NodeEJDB;

    ///////////////////////////////////////////////////////////////////////////
    //                          Main NodeEJDB                                //
    ///////////////////////////////////////////////////////////////////////////

    class NodeEJDB : public ObjectWrap {

        enum { //Commands
            cmdSave = 1, //Save JSON object
            cmdLoad = 2, //Load BSON by oid
            cmdRemove = 3, //Remove BSON by oid
            cmdQuery = 4, //Query collection
            cmdRemoveColl = 5, //Remove collection
            cmdSetIndex = 6, //Set index
            cmdSync = 7, //Sync database
            cmdTxBegin = 8, //Begin collection transaction
            cmdTxAbort = 9, //Abort collection transaction
            cmdTxCommit = 10, //Commit collection transaction
            cmdTxStatus = 11 //Get collection transaction status
        };

        struct BSONCmdData { //Any bson related cmd data
            std::string cname; //Name of collection
            std::vector<bson*> bsons; //bsons to save|query
            std::vector<bson_oid_t> ids; //saved updated oids
            bson_oid_t ref; //Bson ref
            bool merge; //Merge bson on save

            BSONCmdData(const char* _cname) : cname(_cname), merge(false) {
                memset(&ref, 0, sizeof (ref));
            }

            virtual ~BSONCmdData() {
                std::vector<bson*>::iterator it;
                for (it = bsons.begin(); it < bsons.end(); it++) {
                    bson *bs = *(it);
                    if (bs) bson_del(bs);
                }
            }
        };

        struct BSONQCmdData : public BSONCmdData { //Query cmd data
            TCLIST *res;
            int qflags;
            uint32_t count;
            TCXSTR *log;

            BSONQCmdData(const char *_cname, int _qflags) :
            BSONCmdData(_cname), res(NULL), qflags(_qflags), count(0), log(NULL) {
            }

            virtual ~BSONQCmdData() {
                if (res) {
                    tclistdel(res);
                }
                if (log) {
                    tcxstrdel(log);
                }

            }
        };

        struct RMCollCmdData { //Remove collection command data
            std::string cname; //Name of collection
            bool prune;

            RMCollCmdData(const char* _cname, bool _prune) : cname(_cname), prune(_prune) {
            }
        };

        struct SetIndexCmdData { //Set index command data
            std::string cname; //Name of collection
            std::string ipath; //JSON field path for index
            int flags; //set index op flags

            SetIndexCmdData(const char *_cname, const char *_ipath, int _flags) :
            cname(_cname), ipath(_ipath), flags(_flags) {
            }
        };

        struct TxCmdData { //Transaction control command data
            std::string cname; //Name of collection
            bool txactive; //If true we are in transaction

            TxCmdData(const char *_name) : cname(_name), txactive(false) {

            }
        };

        typedef EIOCmdTask<NodeEJDB> EJBTask; //Most generic task
        typedef EIOCmdTask<NodeEJDB, BSONCmdData> BSONCmdTask; //Any bson related task
        typedef EIOCmdTask<NodeEJDB, BSONQCmdData> BSONQCmdTask; //Query task
        typedef EIOCmdTask<NodeEJDB, RMCollCmdData> RMCollCmdTask; //Remove collection
        typedef EIOCmdTask<NodeEJDB, SetIndexCmdData> SetIndexCmdTask; //Set index command
        typedef EIOCmdTask<NodeEJDB, TxCmdData> TxCmdTask; //Transaction control command

        static Persistent<FunctionTemplate> constructor_template;

        EJDB *m_jb;

        static Handle<Value> s_new_object(const Arguments& args) {
            HandleScope scope;
            REQ_STR_ARG(0, dbPath);
            REQ_INT32_ARG(1, mode);
            NodeEJDB *njb = new NodeEJDB();
            if (!njb->open(*dbPath, mode)) {
                std::ostringstream os;
                os << "Unable to open database: " << (*dbPath) << " error: " << njb->_jb_error_msg();
                EJ_LOG_ERROR("%s", os.str().c_str());
                delete njb;
                return scope.Close(ThrowException(Exception::Error(String::New(os.str().c_str()))));
            }
            njb->Wrap(args.This());
            return scope.Close(args.This());
        }

        static void s_exec_cmd_eio(uv_work_t *req) {
            EJBTask *task = (EJBTask*) (req->data);
            NodeEJDB *njb = task->wrapped;
            assert(njb);
            njb->exec_cmd(task);
        }

        static void s_exec_cmd_eio_after(uv_work_t *req) {
            EJBTask *task = (EJBTask*) (req->data);
            NodeEJDB *njb = task->wrapped;
            assert(njb);
            njb->exec_cmd_after(task);
            delete task;
        }

        static Handle<Value> s_close(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (!njb->close()) {
                return scope.Close(ThrowException(Exception::Error(String::New(njb->_jb_error_msg()))));
            }
            return scope.Close(Undefined());
        }

        static Handle<Value> s_load(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(2);
            REQ_STR_ARG(0, cname); //Collection name
            REQ_STR_ARG(1, soid); //String OID
            if (!ejdbisvalidoidstr(*soid)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Argument 2: Invalid OID string"))));
            }
            Local<Function> cb;
            bson_oid_t oid;
            bson_oid_from_string(&oid, *soid);
            BSONCmdData *cmdata = new BSONCmdData(*cname);
            cmdata->ref = oid;

            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (args[2]->IsFunction()) {
                cb = Local<Function>::Cast(args[2]);
                BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdLoad, cmdata, BSONCmdTask::delete_val);
                uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
                return scope.Close(Undefined());
            } else {
                BSONCmdTask task(cb, njb, cmdLoad, cmdata, BSONCmdTask::delete_val);
                njb->load(&task);
                return scope.Close(njb->load_after(&task));
            }
        }

        static Handle<Value> s_remove(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(2);
            REQ_STR_ARG(0, cname); //Collection name
            REQ_STR_ARG(1, soid); //String OID
            if (!ejdbisvalidoidstr(*soid)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Argument 2: Invalid OID string"))));
            }
            Local<Function> cb;
            bson_oid_t oid;
            bson_oid_from_string(&oid, *soid);
            BSONCmdData *cmdata = new BSONCmdData(*cname);
            cmdata->ref = oid;

            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (args[2]->IsFunction()) {
                cb = Local<Function>::Cast(args[2]);
                BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdRemove, cmdata, BSONCmdTask::delete_val);
                uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
                return scope.Close(Undefined());
            } else {
                BSONCmdTask task(cb, njb, cmdRemove, cmdata, BSONCmdTask::delete_val);
                njb->remove(&task);
                return scope.Close(njb->remove_after(&task));
            }
        }

        static Handle<Value> s_save(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(3);
            REQ_STR_ARG(0, cname); //Collection name
            REQ_ARR_ARG(1, oarr); //Array of JSON objects
            REQ_OBJ_ARG(2, opts); //Options obj

            Local<Function> cb;
            BSONCmdData *cmdata = new BSONCmdData(*cname);
            for (uint32_t i = 0; i < oarr->Length(); ++i) {
                Local<Value> v = oarr->Get(i);
                if (!v->IsObject()) {
                    cmdata->bsons.push_back(NULL);
                    continue;
                }
                bson *bs = bson_create();
                assert(bs);
                bson_init(bs);
                toBSON(Handle<Object>::Cast(v), bs, false);
                if (bs->err) {
                    Local<String> msg = String::New(bson_first_errormsg(bs));
                    bson_del(bs);
                    delete cmdata;
                    return scope.Close(ThrowException(Exception::Error(msg)));
                }
                bson_finish(bs);
                cmdata->bsons.push_back(bs);
            }
            if (opts->Get(sym_merge)->BooleanValue()) {
                cmdata->merge = true;
            }
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());

            if (args[3]->IsFunction()) { //callback provided
                cb = Local<Function>::Cast(args[3]);
                BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdSave, cmdata, BSONCmdTask::delete_val);
                uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
                return scope.Close(Undefined());
            } else {
                BSONCmdTask task(cb, njb, cmdSave, cmdata, BSONCmdTask::delete_val);
                njb->save(&task);
                return scope.Close(njb->save_after(&task));
            }
        }

        static Handle<Value> s_query(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(3);
            REQ_STR_ARG(0, cname)
            REQ_ARR_ARG(1, qarr);
            REQ_INT32_ARG(2, qflags);

            if (qarr->Length() == 0) {
                return scope.Close(ThrowException(Exception::Error(String::New("Query array must have at least one element"))));
            }
            Local<Function> cb;
            BSONQCmdData *cmdata = new BSONQCmdData(*cname, qflags);
            uint32_t len = qarr->Length();
            for (uint32_t i = 0; i < len; ++i) {
                Local<Value> qv = qarr->Get(i);
                if (i > 0 && i == len - 1 && (qv->IsNull() || qv->IsUndefined())) { //Last hints element can be NULL
                    cmdata->bsons.push_back(NULL);
                    continue;
                } else if (!qv->IsObject()) {
                    delete cmdata;
                    return scope.Close(ThrowException(
                            Exception::Error(
                            String::New("Each element of query array must be an object (except last hints element)"))
                            ));
                }
                bson *bs = bson_create();
                bson_init_as_query(bs);
                toBSON(Local<Object>::Cast(qv), bs, true);
                bson_finish(bs);
                if (bs->err) {
                    Local<String> msg = String::New(bson_first_errormsg(bs));
                    bson_del(bs);
                    delete cmdata;
                    return scope.Close(ThrowException(Exception::Error(msg)));
                }
                cmdata->bsons.push_back(bs);
            }

            if (len > 1 && qarr->Get(len - 1)->IsObject()) {
                Local<Object> hints = Local<Object>::Cast(qarr->Get(len - 1));
                if (hints->Get(sym_explain)->BooleanValue()) {
                    cmdata->log = tcxstrnew();
                }
            }

            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());

            if (args[3]->IsFunction()) { //callback provided
                cb = Local<Function>::Cast(args[3]);
                BSONQCmdTask *task = new BSONQCmdTask(cb, njb, cmdQuery, cmdata, BSONQCmdTask::delete_val);
                uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
                return scope.Close(Undefined());
            } else {
                BSONQCmdTask task(cb, njb, cmdQuery, cmdata, BSONQCmdTask::delete_val);
                njb->query(&task);
                return scope.Close(njb->query_after(&task));
            }
        }

        static Handle<Value> s_set_index(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(3);
            REQ_STR_ARG(0, cname)
            REQ_STR_ARG(1, ipath)
            REQ_INT32_ARG(2, flags);

            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            SetIndexCmdData *cmdata = new SetIndexCmdData(*cname, *ipath, flags);

            Local<Function> cb;
            if (args[3]->IsFunction()) {
                cb = Local<Function>::Cast(args[3]);
                SetIndexCmdTask *task = new SetIndexCmdTask(cb, njb, cmdSetIndex, cmdata, SetIndexCmdTask::delete_val);
                uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
            } else {
                SetIndexCmdTask task(cb, njb, cmdSetIndex, cmdata, SetIndexCmdTask::delete_val);
                njb->set_index(&task);
                njb->set_index_after(&task);
                if (task.cmd_ret) {
                    return scope.Close(Exception::Error(String::New(task.cmd_ret_msg.c_str())));
                }
            }
            return scope.Close(Undefined());
        }

        static Handle<Value> s_sync(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            Local<Function> cb;
            if (args[0]->IsFunction()) {
                cb = Local<Function>::Cast(args[0]);
                EJBTask *task = new EJBTask(cb, njb, cmdSync, NULL, NULL);
                uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
            } else {
                EJBTask task(cb, njb, cmdSync, NULL, NULL);
                njb->sync(&task);
                njb->sync_after(&task);
                if (task.cmd_ret) {
                    return scope.Close(Exception::Error(String::New(task.cmd_ret_msg.c_str())));
                }
            }
            return scope.Close(Undefined());
        }

        static Handle<Value> s_db_meta(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            Local<Object> ret = Object::New();
            if (!ejdbisopen(njb->m_jb)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Operation on closed EJDB instance"))));
            }
            TCLIST *cols = ejdbgetcolls(njb->m_jb);
            if (!cols) {
                return scope.Close(ThrowException(Exception::Error(String::New(njb->_jb_error_msg()))));
            }
            Local<Array> cinfo = Array::New();
            for (int i = 0; i < TCLISTNUM(cols); ++i) {
                EJCOLL *coll = (EJCOLL*) TCLISTVALPTR(cols, i);
                assert(coll);
                if (!ejcollockmethod(coll, false)) continue;
                Local<Object> cm = Object::New();
                cm->Set(sym_name, String::New(coll->cname, coll->cnamesz));
                cm->Set(sym_file, String::New(coll->tdb->hdb->path));
                cm->Set(sym_records, Integer::NewFromUnsigned((uint32_t) coll->tdb->hdb->rnum));
                Local<Object> opts = Object::New();
                opts->Set(sym_buckets, Integer::NewFromUnsigned((uint32_t) coll->tdb->hdb->bnum));
                opts->Set(sym_cachedrecords, Integer::NewFromUnsigned(coll->tdb->hdb->rcnum));
                opts->Set(sym_large, Boolean::New(coll->tdb->opts & TDBTLARGE));
                opts->Set(sym_compressed, Boolean::New((coll->tdb->opts & TDBTDEFLATE) ? true : false));
                cm->Set(sym_options, opts);
                Local<Array> indexes = Array::New();
                int ic = 0;
                for (int j = 0; j < coll->tdb->inum; ++j) {
                    TDBIDX *idx = (coll->tdb->idxs + j);
                    assert(idx);
                    if (idx->type != TDBITLEXICAL && idx->type != TDBITDECIMAL && idx->type != TDBITTOKEN) {
                        continue;
                    }
                    Local<Object> imeta = Object::New();
                    imeta->Set(sym_field, String::New(idx->name + 1));
                    imeta->Set(sym_iname, String::New(idx->name));
                    switch (idx->type) {
                        case TDBITLEXICAL:
                            imeta->Set(sym_type, String::New("lexical"));
                            break;
                        case TDBITDECIMAL:
                            imeta->Set(sym_type, String::New("decimal"));
                            break;
                        case TDBITTOKEN:
                            imeta->Set(sym_type, String::New("token"));
                            break;
                    }
                    TCBDB *idb = (TCBDB*) idx->db;
                    if (idb) {
                        imeta->Set(sym_records, Integer::NewFromUnsigned((uint32_t) idb->rnum));
                        imeta->Set(sym_file, String::New(idb->hdb->path));
                    }
                    indexes->Set(Integer::New(ic++), imeta);
                }
                cm->Set(sym_indexes, indexes);
                cinfo->Set(Integer::New(i), cm);
                ejcollunlockmethod(coll);
            }
            tclistdel(cols);
            ret->Set(sym_file, String::New(njb->m_jb->metadb->hdb->path));
            ret->Set(String::New("collections"), cinfo);
            return scope.Close(ret);
        }

        //transaction control handlers

        static Handle<Value> s_coll_txctl(const Arguments& args) {
            HandleScope scope;
            REQ_STR_ARG(0, cname);
            //operation values:
            //cmdTxBegin = 8, //Begin collection transaction
            //cmdTxAbort = 9, //Abort collection transaction
            //cmdTxCommit = 10, //Commit collection transaction
            //cmdTxStatus = 11 //Get collection transaction status
            REQ_INT32_ARG(1, op);
            //Arg 2 is the optional function callback arg
            if (!(op == cmdTxBegin ||
                    op == cmdTxAbort ||
                    op == cmdTxCommit ||
                    op == cmdTxStatus)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Invalid value of 1 argument"))));
            }
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            assert(njb);
            EJDB *jb = njb->m_jb;
            if (!ejdbisopen(jb)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Operation on closed EJDB instance"))));
            }
            TxCmdData *cmdata = new TxCmdData(*cname);
            Local<Function> cb;
            if (args[2]->IsFunction()) {
                cb = Local<Function>::Cast(args[2]);
                TxCmdTask *task = new TxCmdTask(cb, njb, op, cmdata, TxCmdTask::delete_val);
                uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
                return scope.Close(Undefined());
            } else {
                TxCmdTask task(cb, njb, op, cmdata, NULL);
                njb->txctl(&task);
                return scope.Close(njb->txctl_after(&task));
            }
        }

        static Handle<Value> s_ecode(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (!njb->m_jb) { //not using ejdbisopen()
                return scope.Close(ThrowException(Exception::Error(String::New("Operation on closed EJDB instance"))));
            }
            return scope.Close(Integer::New(ejdbecode(njb->m_jb)));
        }

        static Handle<Value> s_ensure_collection(const Arguments& args) {
            HandleScope scope;
            REQ_STR_ARG(0, cname);
            REQ_OBJ_ARG(1, copts);
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (!ejdbisopen(njb->m_jb)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Operation on closed EJDB instance"))));
            }
            EJCOLLOPTS jcopts;
            memset(&jcopts, 0, sizeof (jcopts));
            jcopts.cachedrecords = (int) fetch_int_data(copts->Get(sym_cachedrecords), NULL, 0);
            jcopts.compressed = fetch_bool_data(copts->Get(sym_compressed), NULL, false);
            jcopts.large = fetch_bool_data(copts->Get(sym_large), NULL, false);
            jcopts.records = fetch_int_data(copts->Get(sym_records), NULL, 0);
            EJCOLL *coll = ejdbcreatecoll(njb->m_jb, *cname, &jcopts);
            if (!coll) {
                return scope.Close(ThrowException(Exception::Error(String::New(njb->_jb_error_msg()))));
            }
            return scope.Close(Undefined());
        }

        static Handle<Value> s_rm_collection(const Arguments& args) {
            HandleScope scope;
            REQ_STR_ARG(0, cname);
            REQ_VAL_ARG(1, prune);
            REQ_FUN_ARG(2, cb);
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (!ejdbisopen(njb->m_jb)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Operation on closed EJDB instance"))));
            }
            RMCollCmdData *cmdata = new RMCollCmdData(*cname, prune->BooleanValue());
            RMCollCmdTask *task = new RMCollCmdTask(cb, njb, cmdRemoveColl, cmdata, RMCollCmdTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, (uv_after_work_cb)s_exec_cmd_eio_after);
            return scope.Close(Undefined());
        }

        static Handle<Value> s_is_open(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            return scope.Close(Boolean::New(ejdbisopen(njb->m_jb)));
        }


        ///////////////////////////////////////////////////////////////////////////
        //                            Instance methods                           //
        ///////////////////////////////////////////////////////////////////////////

        void exec_cmd(EJBTask *task) {
            int cmd = task->cmd;
            switch (cmd) {
                case cmdQuery:
                    query((BSONQCmdTask*) task);
                    break;
                case cmdLoad:
                    load((BSONCmdTask*) task);
                    break;
                case cmdSave:
                    save((BSONCmdTask*) task);
                    break;
                case cmdRemove:
                    remove((BSONCmdTask*) task);
                    break;
                case cmdRemoveColl:
                    rm_collection((RMCollCmdTask*) task);
                    break;
                case cmdSetIndex:
                    set_index((SetIndexCmdTask*) task);
                    break;
                case cmdSync:
                    sync(task);
                    break;
                case cmdTxBegin:
                case cmdTxCommit:
                case cmdTxAbort:
                case cmdTxStatus:
                    txctl((TxCmdTask*) task);
                    break;
                default:
                    assert(0);
            }
        }

        void exec_cmd_after(EJBTask *task) {
            int cmd = task->cmd;
            switch (cmd) {
                case cmdQuery:
                    query_after((BSONQCmdTask*) task);
                    break;
                case cmdLoad:
                    load_after((BSONCmdTask*) task);
                    break;
                case cmdSave:
                    save_after((BSONCmdTask*) task);
                    break;
                case cmdRemove:
                    remove_after((BSONCmdTask*) task);
                    break;
                case cmdRemoveColl:
                    rm_collection_after((RMCollCmdTask*) task);
                    break;
                case cmdSetIndex:
                    set_index_after((SetIndexCmdTask*) task);
                    break;
                case cmdSync:
                    sync_after(task);
                    break;
                case cmdTxBegin:
                case cmdTxCommit:
                case cmdTxAbort:
                case cmdTxStatus:
                    txctl_after((TxCmdTask*) task);
                    break;
                default:
                    assert(0);
            }
        }

        void sync(EJBTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            if (!ejdbsyncdb(m_jb)) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
            }
        }

        void sync_after(EJBTask *task) {
            HandleScope scope;
            Local<Value> argv[1];
            if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
                return;
            }
            if (task->cmd_ret != 0) {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            } else {
                argv[0] = Local<Primitive>::New(Null());
            }
            TryCatch try_catch;
            task->cb->Call(Context::GetCurrent()->Global(), 1, argv);
            if (try_catch.HasCaught()) {
                FatalException(try_catch);
            }
        }

        void set_index(SetIndexCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            SetIndexCmdData *cmdata = task->cmd_data;
            assert(cmdata);

            EJCOLL *coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
            if (!coll) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                return;
            }
            if (!ejdbsetindex(coll, cmdata->ipath.c_str(), cmdata->flags)) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
            }
        }

        void set_index_after(SetIndexCmdTask *task) {
            HandleScope scope;
            Local<Value> argv[1];
            if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
                return;
            }
            if (task->cmd_ret != 0) {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            } else {
                argv[0] = Local<Primitive>::New(Null());
            }
            TryCatch try_catch;
            task->cb->Call(Context::GetCurrent()->Global(), 1, argv);
            if (try_catch.HasCaught()) {
                FatalException(try_catch);
            }
        }

        void rm_collection(RMCollCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            RMCollCmdData *cmdata = task->cmd_data;
            assert(cmdata);
            if (!ejdbrmcoll(m_jb, cmdata->cname.c_str(), cmdata->prune)) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
            }
        }

        void rm_collection_after(RMCollCmdTask *task) {
            HandleScope scope;
            Local<Value> argv[1];
            if (task->cmd_ret != 0) {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            } else {
                argv[0] = Local<Primitive>::New(Null());
            }
            TryCatch try_catch;
            task->cb->Call(Context::GetCurrent()->Global(), 1, argv);
            if (try_catch.HasCaught()) {
                FatalException(try_catch);
            }
        }

        void remove(BSONCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            BSONCmdData *cmdata = task->cmd_data;
            assert(cmdata);
            EJCOLL *coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
            if (!coll) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                return;
            }
            if (!ejdbrmbson(coll, &task->cmd_data->ref)) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
            }
        }

        Handle<Value> remove_after(BSONCmdTask *task) {
            HandleScope scope;
            Local<Value> argv[1];
            if (task->cmd_ret != 0) {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            } else {
                argv[0] = Local<Primitive>::New(Null());
            }
            if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
                if (task->cmd_ret != 0)
                    return scope.Close(ThrowException(argv[0]));
                else
                    return scope.Close(Undefined());
            } else {
                TryCatch try_catch;
                task->cb->Call(Context::GetCurrent()->Global(), 1, argv);
                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
                return scope.Close(Undefined());
            }
        }

        void txctl(TxCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            TxCmdData *cmdata = task->cmd_data;
            assert(cmdata);

            EJCOLL *coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
            if (!coll) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                return;
            }
            bool ret = false;
            switch (task->cmd) {
                case cmdTxBegin:
                    ret = ejdbtranbegin(coll);
                    break;
                case cmdTxCommit:
                    ret = ejdbtrancommit(coll);
                    break;
                case cmdTxAbort:
                    ret = ejdbtranabort(coll);
                    break;
                case cmdTxStatus:
                    ret = ejdbtranstatus(coll, &(cmdata->txactive));
                    break;
                default:
                    assert(0);
            }
            if (!ret) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
            }
        }

        Handle<Value> txctl_after(TxCmdTask *task) {
            HandleScope scope;
            TxCmdData *cmdata = task->cmd_data;
            int args = 1;
            Local<Value> argv[2];
            if (task->cmd_ret != 0) {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            } else {
                argv[0] = Local<Primitive>::New(Null());
                if (task->cmd == cmdTxStatus) {
                    argv[1] = Local<Boolean>::New(Boolean::New(cmdata->txactive));
                    args = 2;
                }
            }
            if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
                if (task->cmd_ret != 0) {
                    return scope.Close(ThrowException(argv[0]));
                } else {
                    if (task->cmd == cmdTxStatus) {
                        return scope.Close(argv[1]);
                    } else {
                        return scope.Close(Undefined());
                    }
                }
            } else {
                TryCatch try_catch;
                task->cb->Call(Context::GetCurrent()->Global(), args, argv);
                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
                return scope.Close(Undefined());
            }
        }

        void save(BSONCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            BSONCmdData *cmdata = task->cmd_data;
            assert(cmdata);
            EJCOLL *coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
            if (!coll) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                return;
            }

            std::vector<bson*>::iterator it;
            for (it = cmdata->bsons.begin(); it < cmdata->bsons.end(); it++) {
                bson_oid_t oid;
                bson *bs = *(it);
                if (!bs) {
                    //Zero OID
                    oid.ints[0] = 0;
                    oid.ints[1] = 0;
                    oid.ints[2] = 0;
                } else if (!ejdbsavebson2(coll, bs, &oid, cmdata->merge)) {
                    task->cmd_ret = CMD_RET_ERROR;
                    task->cmd_ret_msg = _jb_error_msg();
                    break;
                }
                cmdata->ids.push_back(oid);
            }
        }

        Handle<Value> save_after(BSONCmdTask *task) {
            HandleScope scope;
            Local<Value> argv[2];
            if (task->cmd_ret != 0) {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            } else {
                argv[0] = Local<Primitive>::New(Null());
            }
            Local<Array> oids = Array::New();
            std::vector<bson_oid_t>::iterator it;
            int32_t c = 0;
            for (it = task->cmd_data->ids.begin(); it < task->cmd_data->ids.end(); it++) {
                bson_oid_t& oid = *it;
                if (oid.ints[0] || oid.ints[1] || oid.ints[2]) {
                    char oidhex[25];
                    bson_oid_to_string(&oid, oidhex);
                    oids->Set(Integer::New(c++), String::New(oidhex));
                } else {
                    oids->Set(Integer::New(c++), Null());
                }
            }
            argv[1] = oids;
            if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
                return (task->cmd_ret != 0) ? scope.Close(ThrowException(argv[0])) : scope.Close(argv[1]);
            } else {
                TryCatch try_catch;
                task->cb->Call(Context::GetCurrent()->Global(), 2, argv);
                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
                return scope.Close(Undefined());
            }
        }

        void load(BSONCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            BSONCmdData *cmdata = task->cmd_data;
            assert(cmdata);
            EJCOLL *coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
            if (!coll) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                return;
            }
            cmdata->bsons.push_back(ejdbloadbson(coll, &task->cmd_data->ref));
        }

        Handle<Value> load_after(BSONCmdTask *task) {
            HandleScope scope;
            Local<Value> argv[2];
            if (task->cmd_ret != 0) {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            } else {
                argv[0] = Local<Primitive>::New(Null());
            }
            bson *bs = (!task->cmd_ret && task->cmd_data->bsons.size() > 0) ?
                    task->cmd_data->bsons.front() :
                    NULL;
            if (bs) {
                bson_iterator it;
                bson_iterator_init(&it, bs);
                argv[1] = Local<Object>::New(toV8Object(&it, BSON_OBJECT));
            } else {
                argv[1] = Local<Primitive>::New(Null());
            }
            if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
                return (task->cmd_ret != 0) ? scope.Close(ThrowException(argv[0])) : scope.Close(argv[1]);
            } else {
                TryCatch try_catch;
                task->cb->Call(Context::GetCurrent()->Global(), 2, argv);
                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
                return scope.Close(Undefined());
            }
        }

        void query(BSONQCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            TCLIST *res = NULL;
            bson oqarrstack[8]; //max 8 $or bsons on stack
            BSONQCmdData *cmdata = task->cmd_data;
            std::vector<bson*> &bsons = cmdata->bsons;
            EJCOLL *coll = ejdbgetcoll(m_jb, cmdata->cname.c_str());
            if (!coll) {
                bson *qbs = bsons.front();
                bson_iterator it;
                //If we are in $upsert mode so new collection will be created
                if (qbs && bson_find(&it, qbs, "$upsert") == BSON_OBJECT) {
                    coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
                    if (!coll) {
                        task->cmd_ret = CMD_RET_ERROR;
                        task->cmd_ret_msg = _jb_error_msg();
                        return;
                    }
                } else { //No collection -> no results
                    cmdata->res = tclistnew2(1);
                    cmdata->count = 0;
                    return;
                }
            }
            int orsz = (int) bsons.size() - 2; //Minus main qry at begining and hints object at the end
            if (orsz < 0) orsz = 0;
            bson *oqarr = ((orsz <= 8) ? oqarrstack : (bson*) malloc(orsz * sizeof (bson)));
            for (int i = 1; i < (int) bsons.size() - 1; ++i) {
                oqarr[i - 1] = *(bsons.at(i));
            }
            EJQ *q = ejdbcreatequery(
                    m_jb,
                    bsons.front(),
                    (orsz > 0 ? oqarr : NULL), orsz,
                    ((bsons.size() > 1) ? bsons.back() : NULL));
            if (!q) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                goto finish;
            }
            res = ejdbqryexecute(coll, q, &cmdata->count, cmdata->qflags, cmdata->log);
            if (ejdbecode(m_jb) != TCESUCCESS) {
                if (res) {
                    tclistdel(res);
                    res = NULL;
                }
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                goto finish;
            }
            cmdata->res = res;
finish:
            if (q) {
                ejdbquerydel(q);
            }
            if (oqarr && oqarr != oqarrstack) {
                free(oqarr);
            }
        }

        Handle<Value> query_after(BSONQCmdTask *task);

        bool open(const char* dbpath, int mode) {
            m_jb = ejdbnew();
            if (!m_jb) {
                return false;
            }
            return ejdbopen(m_jb, dbpath, mode);
        }

        bool close() {
            return m_jb ? ejdbclose(m_jb) : false;
        }

        const char* _jb_error_msg() {
            return m_jb ? ejdberrmsg(ejdbecode(m_jb)) : "Unknown error";
        }

        bool _check_state(EJBTask *task) {
            if (!ejdbisopen(m_jb)) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = "Database is not opened";
                return false;
            }
            return true;
        }

        NodeEJDB() : m_jb(NULL) {
        }

        virtual ~NodeEJDB() {
            if (m_jb) {
                ejdbdel(m_jb);
            }
        }

    public:

        static void Init(Handle<Object> target) {
            HandleScope scope;

            //Symbols
            sym_large = NODE_PSYMBOL("large");
            sym_compressed = NODE_PSYMBOL("compressed");
            sym_records = NODE_PSYMBOL("records");
            sym_cachedrecords = NODE_PSYMBOL("cachedrecords");
            sym_explain = NODE_PSYMBOL("$explain");
            sym_merge = NODE_PSYMBOL("$merge");

            sym_name = NODE_PSYMBOL("name");
            sym_iname = NODE_PSYMBOL("iname");
            sym_field = NODE_PSYMBOL("field");
            sym_indexes = NODE_PSYMBOL("indexes");
            sym_options = NODE_PSYMBOL("options");
            sym_file = NODE_PSYMBOL("file");
            sym_buckets = NODE_PSYMBOL("buckets");
            sym_type = NODE_PSYMBOL("type");


            Local<FunctionTemplate> t = FunctionTemplate::New(s_new_object);
            constructor_template = Persistent<FunctionTemplate>::New(t);
            constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
            constructor_template->SetClassName(String::NewSymbol("NodeEJDB"));

            //Open mode
            NODE_DEFINE_CONSTANT(target, JBOREADER);
            NODE_DEFINE_CONSTANT(target, JBOWRITER);
            NODE_DEFINE_CONSTANT(target, JBOCREAT);
            NODE_DEFINE_CONSTANT(target, JBOTRUNC);
            NODE_DEFINE_CONSTANT(target, JBONOLCK);
            NODE_DEFINE_CONSTANT(target, JBOLCKNB);
            NODE_DEFINE_CONSTANT(target, JBOTSYNC);

            //Indexes
            NODE_DEFINE_CONSTANT(target, JBIDXDROP);
            NODE_DEFINE_CONSTANT(target, JBIDXDROPALL);
            NODE_DEFINE_CONSTANT(target, JBIDXOP);
            NODE_DEFINE_CONSTANT(target, JBIDXREBLD);
            NODE_DEFINE_CONSTANT(target, JBIDXNUM);
            NODE_DEFINE_CONSTANT(target, JBIDXSTR);
            NODE_DEFINE_CONSTANT(target, JBIDXISTR);
            NODE_DEFINE_CONSTANT(target, JBIDXARR);

            //Misc
            NODE_DEFINE_CONSTANT(target, JBQRYCOUNT);

            NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", s_close);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "save", s_save);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "load", s_load);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "remove", s_remove);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "query", s_query);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "lastError", s_ecode);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "ensureCollection", s_ensure_collection);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "removeCollection", s_rm_collection);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "isOpen", s_is_open);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "setIndex", s_set_index);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "sync", s_sync);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "dbMeta", s_db_meta);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "_txctl", s_coll_txctl);

            //Symbols
            target->Set(String::NewSymbol("NodeEJDB"), constructor_template->GetFunction());
        }

        void Ref() {
            ObjectWrap::Ref();
        }

        void Unref() {
            ObjectWrap::Unref();
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    //                        ResultSet cursor                               //
    ///////////////////////////////////////////////////////////////////////////

    class NodeEJDBCursor : public ObjectWrap {
        friend class NodeEJDB;

        static Persistent<FunctionTemplate> constructor_template;

        NodeEJDB *m_nejdb;
        intptr_t m_mem; //amount of memory contained in cursor

        TCLIST *m_rs; //result set bsons
        int m_pos; //current cursor position
        bool m_no_next; //no next() was called

        static Handle<Value> s_new_object(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(2);
            REQ_EXT_ARG(0, nejedb);
            REQ_EXT_ARG(1, rs);
            NodeEJDBCursor *cursor = new NodeEJDBCursor((NodeEJDB*) nejedb->Value(), (TCLIST*) rs->Value());
            cursor->Wrap(args.This());
            return scope.Close(args.This());
        }

        static Handle<Value> s_close(const Arguments& args) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap< NodeEJDBCursor > (args.This());
            c->close();
            return scope.Close(Undefined());
        }

        static Handle<Value> s_reset(const Arguments& args) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap< NodeEJDBCursor > (args.This());
            c->m_pos = 0;
            c->m_no_next = true;
            return scope.Close(Undefined());
        }

        static Handle<Value> s_has_next(const Arguments& args) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap< NodeEJDBCursor > (args.This());
            if (!c->m_rs) {
                return ThrowException(Exception::Error(String::New("Cursor closed")));
            }
            int rsz = TCLISTNUM(c->m_rs);
            return scope.Close(Boolean::New(c->m_rs && ((c->m_no_next && rsz > 0) || (c->m_pos + 1 < rsz))));
        }

        static Handle<Value> s_next(const Arguments& args) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap< NodeEJDBCursor > (args.This());
            if (!c->m_rs) {
                return ThrowException(Exception::Error(String::New("Cursor closed")));
            }
            int rsz = TCLISTNUM(c->m_rs);
            if (c->m_no_next) {
                c->m_no_next = false;
                return scope.Close(Boolean::New(rsz > 0));
            } else if (c->m_pos + 1 < rsz) {
                c->m_pos++;
                return scope.Close(Boolean::New(true));
            } else {
                return scope.Close(Boolean::New(false));
            }
        }

        static Handle<Value> s_get_length(Local<String> property, const AccessorInfo &info) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap<NodeEJDBCursor > (info.This());
            if (!c->m_rs) {
                return ThrowException(Exception::Error(String::New("Cursor closed")));
            }
            return scope.Close(Integer::New(TCLISTNUM(c->m_rs)));
        }

        static Handle<Value> s_get_pos(Local<String> property, const AccessorInfo &info) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap<NodeEJDBCursor > (info.This());
            if (!c->m_rs) {
                return ThrowException(Exception::Error(String::New("Cursor closed")));
            }
            return scope.Close(Integer::New(c->m_pos));
        }

        static void s_set_pos(Local<String> property, Local<Value> val, const AccessorInfo &info) {
            HandleScope scope;
            if (!val->IsNumber()) {
                return;
            }
            NodeEJDBCursor *c = ObjectWrap::Unwrap<NodeEJDBCursor > (info.This());
            if (!c->m_rs) {
                return;
            }
            int nval = val->Int32Value();
            int rsz = TCLISTNUM(c->m_rs);
            if (nval < 0) {
                nval = rsz + nval;
            }
            if (nval >= 0 && rsz > 0) {
                nval = (nval >= rsz) ? rsz - 1 : nval;
            } else {
                nval = 0;
            }
            c->m_pos = nval;
            c->m_no_next = false;
        }

        static Handle<Value> s_field(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(1);
            REQ_STR_ARG(0, fpath);
            NodeEJDBCursor *c = ObjectWrap::Unwrap<NodeEJDBCursor > (args.This());
            if (!c->m_rs) {
                return scope.Close(ThrowException(Exception::Error(String::New("Cursor closed"))));
            }
            int pos = c->m_pos;
            int rsz = TCLISTNUM(c->m_rs);
            if (rsz == 0) {
                return scope.Close(ThrowException(Exception::Error(String::New("Empty cursor"))));
            }
            assert(!(pos < 0 || pos >= rsz)); //m_pos correctly set by s_set_pos
            const void *bsdata = TCLISTVALPTR(c->m_rs, pos);
            assert(bsdata);
            bson_iterator it;
            bson_iterator_from_buffer(&it, (const char*) bsdata);
            bson_type bt = bson_find_fieldpath_value2(*fpath, fpath.length(), &it);
            if (bt == BSON_EOO) {
                return scope.Close(Undefined());
            }
            return scope.Close(toV8Value(&it));
        }

        static Handle<Value> s_object(const Arguments& args) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap<NodeEJDBCursor > (args.This());
            if (!c->m_rs) {
                return scope.Close(ThrowException(Exception::Error(String::New("Cursor closed"))));
            }
            int pos = c->m_pos;
            int rsz = TCLISTNUM(c->m_rs);
            if (rsz == 0) {
                return scope.Close(ThrowException(Exception::Error(String::New("Empty cursor"))));
            }
            assert(!(pos < 0 || pos >= rsz)); //m_pos correctly set by s_set_pos
            const void *bsdata = TCLISTVALPTR(c->m_rs, pos);
            assert(bsdata);
            bson_iterator it;
            bson_iterator_from_buffer(&it, (const char*) bsdata);
            return scope.Close(toV8Object(&it, BSON_OBJECT));
        }

        void close() {
            if (m_nejdb) {
                m_nejdb->Unref();
                m_nejdb = NULL;
            }
            if (m_rs) {
                tclistdel(m_rs);
                m_rs = NULL;
            }
            V8::AdjustAmountOfExternalAllocatedMemory(-m_mem + sizeof (NodeEJDBCursor));
        }

        NodeEJDBCursor(NodeEJDB *_nejedb, TCLIST *_rs) : m_nejdb(_nejedb), m_rs(_rs), m_pos(0), m_no_next(true) {
            assert(m_nejdb);
            this->m_nejdb->Ref();
            m_mem = sizeof (NodeEJDBCursor);
            if (m_rs) {
                intptr_t cmem = 0;
                int i = 0;
                for (; i < TCLISTNUM(m_rs) && i < 1000; ++i) { //Max 1K iterations
                    cmem += bson_size2(TCLISTVALPTR(m_rs, i));
                }
                if (i > 0) {
                    m_mem += (intptr_t) (((double) cmem / i) * TCLISTNUM(m_rs));
                }
                V8::AdjustAmountOfExternalAllocatedMemory(m_mem);
            }
        }

        virtual ~NodeEJDBCursor() {
            close();
            V8::AdjustAmountOfExternalAllocatedMemory((int)sizeof (NodeEJDBCursor) * -1);
        }

    public:

        static void Init(Handle<Object> target) {
            HandleScope scope;
            Local<FunctionTemplate> t = FunctionTemplate::New(s_new_object);
            constructor_template = Persistent<FunctionTemplate>::New(t);
            constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
            constructor_template->SetClassName(String::NewSymbol("NodeEJDBCursor"));

            constructor_template->PrototypeTemplate()
                    ->SetAccessor(String::NewSymbol("length"), s_get_length, 0, Handle<Value > (), ALL_CAN_READ);

            constructor_template->PrototypeTemplate()
                    ->SetAccessor(String::NewSymbol("pos"), s_get_pos, s_set_pos, Handle<Value > (), ALL_CAN_READ);



            NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", s_close);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "reset", s_reset);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "hasNext", s_has_next);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "next", s_next);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "field", s_field);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "object", s_object);
        }

        void Ref() {
            ObjectWrap::Ref();
        }

        void Unref() {
            ObjectWrap::Unref();
        }
    };

    Persistent<FunctionTemplate> NodeEJDB::constructor_template;
    Persistent<FunctionTemplate> NodeEJDBCursor::constructor_template;

    ///////////////////////////////////////////////////////////////////////////
    //                           rest                                        //
    ///////////////////////////////////////////////////////////////////////////

    Handle<Value> NodeEJDB::query_after(BSONQCmdTask *task) {
        HandleScope scope;
        BSONQCmdData *cmdata = task->cmd_data;
        assert(cmdata);

        Local<Value> argv[4];
        if (task->cmd_ret != 0) { //error case
            if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
                return scope.Close(ThrowException(Exception::Error(String::New(task->cmd_ret_msg.c_str()))));
            } else {
                argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
                TryCatch try_catch;
                task->cb->Call(Context::GetCurrent()->Global(), 1, argv);
                if (try_catch.HasCaught()) {
                    FatalException(try_catch);
                }
                return scope.Close(Undefined());
            }
        }
        TCLIST *res = cmdata->res;
        argv[0] = Local<Primitive>::New(Null());
        if (res) {
            cmdata->res = NULL; //res will be freed by NodeEJDBCursor instead of ~BSONQCmdData()
            Local<Value> cursorArgv[2];
            cursorArgv[0] = External::New(task->wrapped);
            cursorArgv[1] = External::New(res);
            Local<Value> cursor(NodeEJDBCursor::constructor_template->GetFunction()->NewInstance(2, cursorArgv));
            argv[1] = cursor;
        } else { //this is update query so no result set
            argv[1] = Local<Primitive>::New(Null());
        }
        argv[2] = Integer::New(cmdata->count);
        if (cmdata->log) {
            argv[3] = String::New((const char*) tcxstrptr(cmdata->log));
        }
        if (task->cb.IsEmpty() || task->cb->IsNull() || task->cb->IsUndefined()) {
            if (res) {
                return scope.Close(argv[1]); //cursor
            } else {
                return scope.Close(argv[2]); //count
            }
        } else {
            TryCatch try_catch;
            task->cb->Call(Context::GetCurrent()->Global(), (cmdata->log) ? 4 : 3, argv);
            if (try_catch.HasCaught()) {
                FatalException(try_catch);
            }
            return scope.Close(Undefined());
        }
    }

    void Init(Handle<Object> target) {
#ifdef __unix
        setlocale(LC_ALL, "en_US.UTF-8"); //todo review it
#endif
        ejdb::NodeEJDB::Init(target);
        ejdb::NodeEJDBCursor::Init(target);
    }

}

// Register the module with node.
NODE_MODULE(ejdb_native, ejdb::Init)