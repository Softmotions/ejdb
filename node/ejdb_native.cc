
#include <v8.h>
#include <node.h>
#include <ejdb.h>

#include "ejdb_args.h"
#include "ejdb_cmd.h"
#include "ejdb_logging.h"
#include "ejdb_thread.h"

#include <vector>
#include <sstream>
#include <locale.h>
#include <stdio.h>

using namespace node;
using namespace v8;

static const int CMD_RET_ERROR = 1;

#define DEFINE_INT64_CONSTANT(target, constant)                       \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Number::New((int64_t) constant),                      \
                static_cast<v8::PropertyAttribute>(                       \
                    v8::ReadOnly|v8::DontDelete))

namespace ejdb {

    typedef struct {
        Handle<Object> traversed;
    } TBSONCTX;

    static void toBSON0(Handle<Object> obj, bson *bs, TBSONCTX *ctx) {
        HandleScope scope;
        assert(ctx && obj->IsObject());
        if (ctx->traversed->Get(obj)->IsObject()) {
            bs->err = BSON_ERROR_ANY;
            bs->errstr = strdup("Circular object reference");
            return;
        }
        ctx->traversed->Set(obj, obj);
        Local<Array> pnames = obj->GetOwnPropertyNames();
        for (uint32_t i = 0; i < pnames->Length(); ++i) {
            Local<Value> pn = pnames->Get(i);
            String::Utf8Value spn(pn);
            Local<Value> pv = obj->Get(pn);
            if (pv->IsString()) {
                String::Utf8Value val(pv);
                bson_append_string(bs, *spn, *val);
            } else if (pv->IsUint32()) {
                bson_append_long(bs, *spn, pv->Uint32Value());
            } else if (pv->IsInt32()) {
                bson_append_int(bs, *spn, pv->Int32Value());
            } else if (pv->IsNumber()) {
                bson_append_double(bs, *spn, pv->NumberValue());
            } else if (pv->IsNull()) {
                bson_append_null(bs, *spn);
            } else if (pv->IsUndefined()) {
                bson_append_undefined(bs, *spn);
            } else if (pv->IsBoolean()) {
                bson_append_bool(bs, *spn, pv->BooleanValue());
            } else if (pv->IsDate()) {
                bson_append_long(bs, *spn, Handle<Date>::Cast(pv)->IntegerValue());
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
            }
        }
    }

    /** Convert V8 object into binary json instance. After usage, it must be freed by bson_del() */
    static void toBSON(Handle<Object> obj, bson *bs) {
        HandleScope scope;
        TBSONCTX ctx;
        ctx.traversed = Object::New();
        toBSON0(obj, bs, &ctx);
    }

    ///////////////////////////////////////////////////////////////////////////
    //                          Main NodeEJDB                                //
    ///////////////////////////////////////////////////////////////////////////

    class NodeEJDB : public ObjectWrap {

        enum { //Commands
            cmdSave = 1, //Save JSON object
            cmdLoad = 2, //Load BSON by oid
            cmdQuery = 3 //Query collection
        };

        //Any bson cmd data

        struct BSONCmdData {
            std::string cname; //Name of collection
            std::vector<bson*> bsons; //bsons to save
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
                    bson_del(bs);
                }
            }
        };

        //Query cmd data

        struct BSONQCmdData : public BSONCmdData {
            TCLIST *res;

            BSONQCmdData(const char* cname) : BSONCmdData::BSONCmdData(cname), res(NULL) {
            }

            virtual ~BSONQCmdData() {
                if (res) {
                    tclistdel(res);
                }
            }
        };

        typedef EIOCmdTask<NodeEJDB> EJBTask;
        typedef EIOCmdTask<NodeEJDB, BSONCmdData> BSONCmdTask;
        typedef EIOCmdTask<NodeEJDB, BSONQCmdData> QueryCmdTask;

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
            EJBTask *task = static_cast<EJBTask*> (req->data);
            NodeEJDB *njb = task->wrapped;
            assert(njb);
            njb->exec_cmd(task);
        }

        static void s_exec_cmd_eio_after(uv_work_t *req) {
            EJBTask *task = static_cast<EJBTask*> (req->data);
            NodeEJDB *njb = task->wrapped;
            assert(njb);
            njb->exec_cmd_after(task);
            delete task;
        }

        static Handle<Value> s_close(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            assert(njb);
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
            assert(njb);
            BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdLoad, cmdata, BSONCmdTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, s_exec_cmd_eio_after);
            return scope.Close(args.This());
        }

        static Handle<Value> s_save(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(3);
            REQ_STR_ARG(0, cname); //Collection name
            REQ_ARR_ARG(1, oarr); //Array of JAVA objects
            REQ_FUN_ARG(2, cb); //Callback

            BSONCmdData *cmdata = new BSONCmdData(*cname);
            for (uint32_t i = 0; i < oarr->Length(); ++i) {
                Local<Value> v = oarr->Get(i);
                if (!v->IsObject()) continue;
                bson *bs = bson_create();
                assert(bs);
                toBSON(Handle<Object>::Cast(v), bs);
                if (bs->err) {
                    Local<String> msg = String::New(bs->errstr ? bs->errstr : "bson creation failed");
                    bson_del(bs);
                    delete cmdata;
                    return scope.Close(ThrowException(Exception::Error(msg)));
                }
                bson_finish(bs);
                cmdata->bsons.push_back(bs);
            }
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            assert(njb);
            BSONCmdTask *task = new BSONCmdTask(cb, njb, cmdSave, cmdata, BSONCmdTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, s_exec_cmd_eio_after);
            return scope.Close(args.This());
        }

        ///////////////////////////////////////////////////////////////////////////
        //                            Instance methods                           //
        ///////////////////////////////////////////////////////////////////////////

        void exec_cmd(EJBTask *task) {
            int cmd = task->cmd;
            switch (cmd) {
                case cmdLoad:
                    load((BSONCmdTask*) task);
                    break;
                case cmdSave:
                    save((BSONCmdTask*) task);
                    break;
            }
        }

        void exec_cmd_after(EJBTask *task) {
            int cmd = task->cmd;
            switch (cmd) {
                case cmdLoad:
                    load_after((BSONCmdTask*) task);
                    break;
                case cmdSave:
                    save_after((BSONCmdTask*) task);
                    break;
            }
            delete task;
        }

        void save(BSONCmdTask *task) {
            if (!_check_state((EJBTask*) task)) {
                return;
            }
            BSONCmdData *cmdata = task->cmd_data;
            assert(cmdata);
            EJCOLL *coll = ejdbcreatecoll(m_jb, cmdata->cname.c_str(), NULL);
            if (!coll) {
                task->cmd_ret_msg = _jb_error_msg();
            }

            std::vector<bson*>::iterator it;
            for (it = cmdata->bsons.begin(); it < cmdata->bsons.end(); it++) {
                bson *bs = *(it);
                assert(bs);
                bson_oid_t oid;
                if (!ejdbsavebson(coll, bs, &oid)) {
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
                char oidhex[25];
                bson_oid_t& oid = *it;
                bson_oid_to_string(&oid, oidhex);
                oids->Set(Integer::New(c++), String::New(oidhex));
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

        }

        void load_after(BSONCmdTask *task) {
            HandleScope scope;

        }

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

            //Symbols
            target->Set(v8::String::NewSymbol("NodeEJDB"), constructor_template->GetFunction());

            NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", s_close);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "save", s_save);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "load", s_load);
        }

        void Ref() {
            ObjectWrap::Ref();
        }

        void Unref() {
            ObjectWrap::Unref();
        }
    };

    Persistent<FunctionTemplate> NodeEJDB::constructor_template;

    void Init(v8::Handle<v8::Object> target) {
#ifdef __unix
        setlocale(LC_ALL, "en_US.UTF-8"); //todo review it
#endif
        ejdb::NodeEJDB::Init(target);
    }

}

// Register the module with node.
NODE_MODULE(ejdb_native, ejdb::Init)