
#include <v8.h>
#include <node.h>
#include <ejdb.h>
#include <locale.h>
#include <stdio.h>

#include "ejdb_args.h"
#include "ejdb_cmd.h"
#include "ejdb_logging.h"
#include "ejdb_thread.h"

using namespace node;
using namespace v8;


#define DEFINE_INT64_CONSTANT(target, constant)                       \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Number::New((int64_t) constant),                      \
                static_cast<v8::PropertyAttribute>(                       \
                    v8::ReadOnly|v8::DontDelete))

namespace ejdb {

    class NodeEJDB : public ObjectWrap {
        typedef EIOCmdTask<NodeEJDB> EJTask;

        static Persistent<FunctionTemplate> constructor_template;

        static Handle<Value> s_new_object(const Arguments& args) {
            HandleScope scope;
            REQ_STR_ARG(0, dbPath);

            return scope.Close(args.This());
        }

        NodeEJDB() {
        }

        virtual ~NodeEJDB() {
        }
    public:

        static void Init(Handle<Object> target) {
            HandleScope scope;

            Local<FunctionTemplate> t = FunctionTemplate::New(s_new_object);
            constructor_template = Persistent<FunctionTemplate>::New(t);
            constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
            constructor_template->SetClassName(String::NewSymbol("NodeEJDB"));
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
NODE_MODULE(ejdb_native, ejdb::Init);