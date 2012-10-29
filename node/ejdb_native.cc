
#include <v8.h>
#include <node.h>
#include <ejdb.h>
#include <locale.h>

using namespace node;
using namespace v8;

namespace ejdb {

    void Init(v8::Handle<v8::Object> target) {
        EJDB *jb = ejdbnew();
        ejdbdel(jb);

#ifdef __unix
        setlocale(LC_ALL, "en_US.UTF-8"); //todo review it
#endif
    }

}

// Register the module with node.
NODE_MODULE(ejdb_native, ejdb::Init);