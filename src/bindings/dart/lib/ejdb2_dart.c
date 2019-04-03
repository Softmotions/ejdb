#define DART_SHARED_LIB
#include "dart_api.h"
#include "dart_native_api.h"
#include "ejdb2.h"
#include "ejdb2cfg.h"

struct NativeFunctionLookup {
  const char *name;
  Dart_NativeFunction fn;
};

typedef struct EJDB2Ctx {
  Dart_Port port;
  EJDB db;
} EJDB2Ctx;

Dart_NativeFunction ejd_resolve_name(Dart_Handle name, int argc, bool *auto_setup_scope);
void ejdb2_port_nfn(Dart_NativeArguments arguments);

static struct NativeFunctionLookup k_scoped_functions[] = {
  {"ejdb2_port", ejdb2_port_nfn},
  {0, 0}
};

#define EJC(h_) ejd_error_check_propagate(h_)

#define EJLIB() EJC(Dart_LookupLibrary(Dart_NewStringFromCString("package:ejdb2/ejdb2.dart")))

IW_INLINE Dart_Handle ejd_error_check_propagate(Dart_Handle handle) {
  if (Dart_IsError(handle)) {
    Dart_PropagateError(handle);
  }
  return handle;
}

static Dart_Handle ejd_error_api_create(const char *msg) {
  return Dart_NewUnhandledExceptionError(Dart_NewApiError(msg));
}

static void ejd_error_throw_scope(iwrc rc, const char *msg) {
  Dart_Handle hmsg = EJC(Dart_NewStringFromCString(msg));
  Dart_Handle hrc = EJC(Dart_NewIntegerFromUint64(rc));
  Dart_Handle args[] = {hrc, hmsg};
  Dart_Handle hclass = EJC(Dart_GetClass(EJLIB(), Dart_NewStringFromCString("EJDB2Error")));
  Dart_ThrowException(Dart_New(hclass, Dart_NewStringFromCString("_internal"), 2, args));
}

DART_EXPORT Dart_Handle ejdb2_dart_Init(Dart_Handle parent_library) {
  fprintf(stderr, "\nejdb2_dart_Init");
  if (Dart_IsError(parent_library)) {
    return parent_library;
  }
  Dart_Handle dh = Dart_SetNativeResolver(parent_library, ejd_resolve_name, 0);
  if (Dart_IsError(dh)) {
    return dh;
  }
  return Dart_Null();
}

static void ejdb2_port_handler(Dart_Port receivePort, Dart_CObject *message) {
  // TODO:
}

void ejdb2_port_nfn(Dart_NativeArguments args) {
  Dart_EnterScope();
  // Will return `NULL` on unexpected system error
  // since we have no ways for good fatal errors reporting from native extensions
  Dart_SetReturnValue(args, Dart_Null());

  EJDB2Ctx *ctx = 0;
  intptr_t ptr = 0;
  Dart_Handle self = EJC(Dart_GetNativeArgument(args, 0));
  EJC(Dart_GetNativeInstanceField(self, 0, &ptr));

  if (!ptr) {
    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) goto finish;
  }

  // static Dart_Port k_receive_port = ILLEGAL_PORT;
  // DART_EXPORT Dart_Isolate Dart_CurrentIsolate()

finish:
  Dart_ExitScope();
}

// void get_receive_port(Dart_NativeArguments arguments) {
//  Dart_EnterScope();
//  Dart_SetReturnValue(arguments, Dart_Null());

//  if (_receivePort == ILLEGAL_PORT) {
//    _receivePort = Dart_NewNativePort(RECEIVE_PORT_NAME, messageHandler, true);
//  }

//  if (_receivePort != ILLEGAL_PORT) {
//    Dart_Handle sendPort = Dart_NewSendPort(_receivePort);
//    Dart_SetReturnValue(arguments, sendPort);
//  }

//  Dart_ExitScope();

// Dart_Handle arg0 = Dart_GetNativeArgument(arguments, 0);
//     Dart_SetNativeInstanceField(arg0, 0, (intptr_t) native_db);
//     Dart_NewWeakPersistentHandle(arg0, (void*) native_db, sizeof(NativeDB) /* external_allocation_size */, NativeDBFinalizer);
// }

Dart_NativeFunction ejd_resolve_name(Dart_Handle name,
                                     int argc,
                                     bool *auto_setup_scope) {
  if (!Dart_IsString(name) || !auto_setup_scope) {
    return 0;
  }
  Dart_EnterScope();
  const char *cname;
  ejd_error_check_propagate(Dart_StringToCString(name, &cname));
  for (int i = 0; k_scoped_functions[i].name; ++i) {
    if (strcmp(cname, k_scoped_functions[i].name) == 0) {
      *auto_setup_scope = true;
      Dart_ExitScope();
      return k_scoped_functions[i].fn;
    }
  }
  Dart_ExitScope();
  return 0;
}
