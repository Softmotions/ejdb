
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


#define DEFINE_INT64_CONSTANT(target, constant)                       \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Number::New((int64_t) constant),                      \
                static_cast<v8::PropertyAttribute>(                       \
                    v8::ReadOnly|v8::DontDelete))

static Persistent<String> sym_close;
static Persistent<String> sym_save;
static Persistent<String> sym_load;
static Persistent<String> sym_query;

namespace ejdb {

    /** Convert V8 object into binary json instance. After usage, it must be freed by bson_del() */
    static void toBSON(Handle<Object> obj, bson *bs) {
        HandleScope scope;
        assert(obj->IsObject());
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
                toBSON(Handle<Object>::Cast(pv), bs);
                if (pv->IsArray()) {
                    bson_append_finish_array(bs);
                } else {
                    bson_append_finish_object(bs);
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    //                          Main NodeEJDB                                //
    ///////////////////////////////////////////////////////////////////////////

    class NodeEJDB : public ObjectWrap {

        enum { //Commands
            cmdSave = 1 //Save JSON object
        };

        struct BSONCmdData {
            std::vector<bson*> bsons;

            BSONCmdData() {
            }

            ~BSONCmdData() {
                std::vector<bson*>::iterator it;
                for (it = bsons.begin(); it < bsons.end(); it++) {
                    bson *bs = *(it);
                    bson_del(bs);
                }
            }
        };

        typedef EIOCmdTask<NodeEJDB> EJBTask;
        typedef EIOCmdTask<NodeEJDB, BSONCmdData> EJBSONTask;

        static Persistent<FunctionTemplate> constructor_template;

        EJDB *m_jb;

        static Handle<Value> s_new_object(const Arguments& args) {
            HandleScope scope;
            REQ_STR_ARG(0, dbPath);
            REQ_INT32_ARG(1, mode);
            NodeEJDB *njb = new NodeEJDB();
            if (!njb->open(*dbPath, mode)) {
                std::ostringstream os;
                os << "Unable to open database: " << (*dbPath) << " error: " << njb->jb_error_msg();
                EJ_LOG_ERROR("%s", os.str().c_str());
                delete njb;
                return scope.Close(ThrowException(Exception::Error(String::New(os.str().c_str()))));
            }
            njb->Wrap(args.This());
            return scope.Close(args.This());
        }

        static void s_exec_cmd_eio(uv_work_t *req) {
            EJBTask *task = static_cast<EJBTask*>(req->data);
            NodeEJDB *njb = task->wrapped;
            assert(njb);
            //TODO njb->exec_cmd(task);
        }

        static void s_exec_cmd_eio_after(uv_work_t *req) {
            EJBTask *task = static_cast<EJBTask*>(req->data);
            NodeEJDB *njb = task->wrapped;
            assert(njb);
            //TODO njb->exec_cmd_after(task);
            delete task;
        }

        static Handle<Value> s_close(const Arguments& args) {
            HandleScope scope;
            NodeEJDB *njb = ObjectWrap::Unwrap< NodeEJDB > (args.This());
            assert(njb);
            if (!njb->close()) {
                return scope.Close(ThrowException(Exception::Error(String::New(njb->jb_error_msg()))));
            }
            return scope.Close(args.This());
        }

        static Handle<Value> s_save(const Arguments& args) {
            HandleScope scope;
            REQ_ARGS(2);
            REQ_ARR_ARG(0, oarr);
            REQ_FUN_ARG(1, cb);

            BSONCmdData *cmdata = new BSONCmdData();
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
            EJBSONTask *task = new EJBSONTask(cb, njb, cmdSave, cmdata, EJBSONTask::delete_val);
            uv_queue_work(uv_default_loop(), &task->uv_work, s_exec_cmd_eio, s_exec_cmd_eio_after);
            return scope.Close(args.This());
        }

        NodeEJDB() : m_jb(NULL) {
        }

        virtual ~NodeEJDB() {
            if (m_jb) {
                ejdbdel(m_jb);
            }
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

        const char* jb_error_msg() {
            return m_jb ? ejdberrmsg(ejdbecode(m_jb)) : "unknown error";
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
            sym_close = NODE_PSYMBOL("close");
            sym_save = NODE_PSYMBOL("save");
            sym_load = NODE_PSYMBOL("load");
            sym_query = NODE_PSYMBOL("query");

            target->Set(v8::String::NewSymbol("NodeEJDB"), constructor_template->GetFunction());

            NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", s_close);
            NODE_SET_PROTOTYPE_METHOD(constructor_template, "save", s_save);
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