
#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <ejdb.h>

#include "ejdb_args.h"
#include "ejdb_cmd.h"
#include "ejdb_logging.h"
#include "ejdb_thread.h"

#include <vector>
#include <sstream>
#include <hash_set>
#include <locale.h>


using namespace node;
using namespace v8;

#ifdef __GNUC__
using namespace __gnu_cxx;
#endif

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

    typedef hash_set<Handle<Object>, V8ObjHash, V8ObjEq> V8ObjSet;

    struct TBSONCTX {
        V8ObjSet tset; //traversed objects set
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
                return scope.Close(Boolean::New(bson_iterator_bool_raw(it)));
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
                knum = tcatoi(key);
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
                        ret->Set(knum, Boolean::New(bson_iterator_bool_raw(it)));
                    } else {
                        ret->Set(String::New(key), Boolean::New(bson_iterator_bool_raw(it)));
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
                    //TODO test it!
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
            bs->errstr = strdup("Circular object reference");
            return;
        }
        ctx->tset.insert(obj);
        Local<Array> pnames = obj->GetOwnPropertyNames();
        for (uint32_t i = 0; i < pnames->Length(); ++i) {
            Local<Value> pn = pnames->Get(i);
            String::Utf8Value spn(pn);
            Local<Value> pv = obj->Get(pn);
            if (pv->IsString()) {
                String::Utf8Value val(pv);
                bson_append_string(bs, *spn, *val);
            } else if (pv->IsInt32()) {
                bson_append_int(bs, *spn, pv->Int32Value());
            } else if (pv->IsUint32()) {
                bson_append_long(bs, *spn, pv->Uint32Value());
            } else if (pv->IsNumber()) {
                bson_append_double(bs, *spn, pv->NumberValue());
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
            } else if (pv->IsObject() || pv->IsArray()) {
                if (pv->IsArray()) {
                    bson_append_start_array(bs, *spn);
                } else {
                    bson_append_start_object(bs, *spn);
                }
                toBSON0(Handle<Object>::Cast(pv), bs, ctx);
                if (pv->IsArray()) {
                    bson_append_finish_array(bs);
                } else {
                    bson_append_finish_object(bs);
                }
            } else if (Buffer::HasInstance(pv)) {
                bson_append_binary(bs, *spn, BSON_BIN_BINARY,
                        Buffer::Data(Handle<Object>::Cast(pv)),
                        Buffer::Length(Handle<Object>::Cast(pv)));
            }
        }
    }

    /** Convert V8 object into binary json instance. After usage, it must be freed by bson_del() */
    static void toBSON(Handle<Object> obj, bson *bs) {
        HandleScope scope;
        TBSONCTX ctx;
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
            cmdQuery = 4 //Query collection
        };

        //Any bson cmd data

        struct BSONCmdData {
            std::string cname; //Name of collection
            std::vector<bson*> bsons; //bsons to save|query
            std::vector<bson_oid_t> ids; //saved updated oids
            bson_oid_t ref; //Bson ref

            BSONCmdData(const char* cname) {
                this->cname = cname;
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

        //Query cmd data

        struct BSONQCmdData : public BSONCmdData {
            TCLIST *res;
            int qflags;
            uint32_t count;

            BSONQCmdData(const char* _cname, int _qflags) :
            BSONCmdData::BSONCmdData(_cname), res(NULL), qflags(_qflags), count(0) {
            }

            virtual ~BSONQCmdData() {
                if (res) {
                    tclistdel(res);
                }
            }
        };

        typedef EIOCmdTask<NodeEJDB> EJBTask;
        typedef EIOCmdTask<NodeEJDB, BSONCmdData> BSONCmdTask;
        typedef EIOCmdTask<NodeEJDB, BSONQCmdData> BSONQCmdTask;

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
            return scope.Close(args.This());
        }

        static Handle<Value> s_load(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(3);
            REQ_STR_ARG(0, cname); //Collection name
            REQ_STR_ARG(1, soid); //String OID
            REQ_FUN_ARG(2, cb); //Callback
            if (soid.length() != 24) {
                return scope.Close(ThrowException(Exception::Error(String::New("Argument 2: Invalid OID string"))));
            }
            bson_oid_t oid;
            bson_oid_from_string(&oid, *soid);
            BSONCmdData *cmdata = new BSONCmdData(*cname);
            cmdata->ref = oid;

            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdLoad, cmdata, BSONCmdTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, s_exec_cmd_eio_after);
            return scope.Close(args.This());
        }

        static Handle<Value> s_remove(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(3);
            REQ_STR_ARG(0, cname); //Collection name
            REQ_STR_ARG(1, soid); //String OID
            REQ_FUN_ARG(2, cb); //Callback
            if (soid.length() != 24) {
                return scope.Close(ThrowException(Exception::Error(String::New("Argument 2: Invalid OID string"))));
            }
            bson_oid_t oid;
            bson_oid_from_string(&oid, *soid);
            BSONCmdData *cmdata = new BSONCmdData(*cname);
            cmdata->ref = oid;

            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdRemove, cmdata, BSONCmdTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, s_exec_cmd_eio_after);
            return scope.Close(args.This());
        }

        static Handle<Value> s_save(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(3);
            REQ_STR_ARG(0, cname); //Collection name
            REQ_ARR_ARG(1, oarr); //Array of JSON objects
            REQ_FUN_ARG(2, cb); //Callback

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
                toBSON(Handle<Object>::Cast(v), bs);
                if (bs->err) {
                    Local<String> msg = String::New(bs->errstr ? bs->errstr : "BSON creation failed");
                    bson_del(bs);
                    delete cmdata;
                    return scope.Close(ThrowException(Exception::Error(msg)));
                }
                bson_finish(bs);
                cmdata->bsons.push_back(bs);
            }
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdSave, cmdata, BSONCmdTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, s_exec_cmd_eio_after);
            return scope.Close(args.This());
        }

        static Handle<Value> s_query(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(4);
            REQ_STR_ARG(0, cname)
            REQ_ARR_ARG(1, qarr);
            REQ_INT32_ARG(2, qflags);
            REQ_FUN_ARG(3, cb);

            if (qarr->Length() == 0) {
                return scope.Close(ThrowException(Exception::Error(String::New("Query array must have at least one element"))));
            }
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
                toBSON(Local<Object>::Cast(qv), bs);
                bson_finish(bs);
                if (bs->err) {
                    Local<String> msg = String::New(bs->errstr ? bs->errstr : "BSON error");
                    bson_del(bs);
                    delete cmdata;
                    return scope.Close(ThrowException(Exception::Error(msg)));
                }
                cmdata->bsons.push_back(bs);
            }
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            BSONQCmdTask *task = new BSONQCmdTask(cb, njb, cmdQuery, cmdata, BSONQCmdTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, s_exec_cmd_eio_after);
            return scope.Close(args.This());
        }

        static Handle<Value> s_ecode(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (!njb->m_jb) {
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
            jcopts.cachedrecords = fetch_int_data(copts->Get(sym_cachedrecords), NULL, 0);
            jcopts.compressed = fetch_bool_data(copts->Get(sym_compressed), NULL, false);
            jcopts.large = fetch_bool_data(copts->Get(sym_large), NULL, false);
            jcopts.records = fetch_int_data(copts->Get(sym_records), NULL, 0);
            EJCOLL *coll = ejdbcreatecoll(njb->m_jb, *cname, &jcopts);
            if (!coll) {
                return scope.Close(ThrowException(Exception::Error(String::New(njb->_jb_error_msg()))));
            }
            return scope.Close(args.This());
        }

        static Handle<Value> s_rm_collection(const Arguments& args) {
            HandleScope scope;
            REQ_STR_ARG(0, cname);
            REQ_VAL_ARG(1, prune);
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            if (!ejdbisopen(njb->m_jb)) {
                return scope.Close(ThrowException(Exception::Error(String::New("Operation on closed EJDB instance"))));
            }
            if (!ejdbrmcoll(njb->m_jb, *cname, prune->BooleanValue())) {
                return scope.Close(ThrowException(Exception::Error(String::New(njb->_jb_error_msg()))));
            }
            return scope.Close(args.This());
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

        void remove_after(BSONCmdTask *task) {
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
                } else if (!ejdbsavebson(coll, bs, &oid)) {
                    task->cmd_ret = CMD_RET_ERROR;
                    task->cmd_ret_msg = _jb_error_msg();
                    break;
                }
                cmdata->ids.push_back(oid);
            }
        }

        void save_after(BSONCmdTask *task) {
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
            TryCatch try_catch;
            task->cb->Call(Context::GetCurrent()->Global(), 2, argv);
            if (try_catch.HasCaught()) {
                FatalException(try_catch);
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

        void load_after(BSONCmdTask *task) {
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
            TryCatch try_catch;
            task->cb->Call(Context::GetCurrent()->Global(), 2, argv);
            if (try_catch.HasCaught()) {
                FatalException(try_catch);
            }
        }

        void query(BSONQCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            bson oqarrstack[8]; //max 8 $or bsons on stack
            BSONQCmdData *cmdata = task->cmd_data;
            EJCOLL *coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
            if (!coll) {
                task->cmd_ret = CMD_RET_ERROR;
                task->cmd_ret_msg = _jb_error_msg();
                return;
            }
            std::vector<bson*> &bsons = cmdata->bsons;
            int orsz = (int) bsons.size() - 2; //Minus main qry at begining and hints object at the end
            if (orsz < 0) orsz = 0;
            bson *oqarr = ((orsz <= 8) ? oqarrstack : (bson*) malloc(orsz * sizeof (bson)));
            for (int i = 1; i < (int) bsons.size() - 1; ++i) {
                oqarr[i - 1] = *(bsons.at(i));
                break;
            }
            TCLIST *res = NULL;
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
            res = ejdbqrysearch(coll, q, &cmdata->count, cmdata->qflags, NULL);
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

        void query_after(BSONQCmdTask *task);

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
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "rmCollection", s_rm_collection);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "isOpen", s_is_open);


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
            return scope.Close(args.This());
        }

        static Handle<Value> s_reset(const Arguments& args) {
            HandleScope scope;
            NodeEJDBCursor *c = ObjectWrap::Unwrap< NodeEJDBCursor > (args.This());
            c->m_pos = 0;
            c->m_no_next = true;
            return scope.Close(args.This());
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
            void *bsdata = TCLISTVALPTR(c->m_rs, pos);
            assert(bsdata);
            bson_iterator it;
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
        }

        NodeEJDBCursor(NodeEJDB *_nejedb, TCLIST *_rs) : m_nejdb(_nejedb), m_rs(_rs), m_pos(0), m_no_next(true) {
            assert(m_rs && m_nejdb);
            this->m_nejdb->Ref();
        }

        virtual ~NodeEJDBCursor() {
            close();
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

    void NodeEJDB::query_after(BSONQCmdTask *task) {
        HandleScope scope;
        Local<Value> argv[3];
        if (task->cmd_ret != 0) {
            argv[0] = Exception::Error(String::New(task->cmd_ret_msg.c_str()));
            TryCatch try_catch;
            task->cb->Call(Context::GetCurrent()->Global(), 1, argv);
            if (try_catch.HasCaught()) {
                FatalException(try_catch);
            }
            return;
        }
        BSONQCmdData *cmdata = task->cmd_data;
        assert(cmdata);
        TCLIST *res = cmdata->res;
        cmdata->res = NULL; //res will be freed by NodeEJDBCursor instead of ~BSONQCmdData()
        argv[0] = Local<Primitive>::New(Null());
        Local<Value> cursorArgv[2];
        cursorArgv[0] = External::New(task->wrapped);
        cursorArgv[1] = External::New(res);
        Local<Object> cursor(NodeEJDBCursor::constructor_template->GetFunction()->NewInstance(2, cursorArgv));
        argv[1] = Local<Object>::New(cursor);
        argv[2] = Integer::New(cmdata->count);
        TryCatch try_catch;
        task->cb->Call(Context::GetCurrent()->Global(), 3, argv);
        if (try_catch.HasCaught()) {
            FatalException(try_catch);
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