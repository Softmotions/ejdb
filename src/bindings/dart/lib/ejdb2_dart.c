#define DART_SHARED_LIB
#include "dart_api.h"
#include "dart_native_api.h"
#include "ejdb2.h"
#include "ejdb2cfg.h"

#if defined(__GNUC__)
#define IW_NORETURN __attribute__((noreturn))
#else
#define IW_NORETURN
#endif

typedef enum {
  _EJD_ERROR_START = (IW_ERROR_START + 15000UL + 4000),
  EJD_ERROR_CREATE_PORT,   /**< Failed to create a Dart port  (EJD_ERROR_CREATE_PORT) */
  _EJD_ERROR_END,
} jbr_ecode_t;

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

#define EJE(label_, args_, msg_)                        \
  do {                                                  \
    Dart_SetReturnValue(args_, Dart_NewApiError(msg_)); \
    goto label_;                                        \
  } while(0)

IW_INLINE Dart_Handle ejd_error_check_propagate(Dart_Handle handle) {
  if (Dart_IsError(handle)) {
    Dart_PropagateError(handle);
  }
  return handle;
}

static Dart_Handle ejd_error_api_create(const char *msg) {
  return Dart_NewUnhandledExceptionError(Dart_NewApiError(msg));
}

IW_INLINE Dart_Handle ejd_error_throw_scope(iwrc rc, const char *msg) {
  Dart_Handle hmsg;
  if (msg) {
    hmsg = EJC(Dart_NewStringFromCString(msg));
  } else {
    const char *explained = iwlog_ecode_explained(rc);
    hmsg = EJC(Dart_NewStringFromCString(explained ? explained : ""));
  }
  Dart_Handle hrc = EJC(Dart_NewIntegerFromUint64(rc));
  Dart_Handle args[] = {hrc, hmsg};
  Dart_Handle hclass = EJC(Dart_GetClass(EJLIB(), Dart_NewStringFromCString("EJDB2Error")));
  return Dart_ThrowException(Dart_New(hclass, Dart_NewStringFromCString("_internal"), 2, args));
}

static const char *_ejd_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _EJD_ERROR_START && ecode < _EJD_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case EJD_ERROR_CREATE_PORT:
      return "Failed to create a Dart port  (EJD_ERROR_CREATE_PORT)";
  }
  return 0;
}

DART_EXPORT Dart_Handle ejdb2_dart_Init(Dart_Handle parent_library) {
  fprintf(stderr, "\nejdb2_dart_Init");
  static volatile int ejd_ecodefn_initialized = 0;
  if (__sync_bool_compare_and_swap(&ejd_ecodefn_initialized, 0, 1)) {
    iwlog_register_ecodefn(_ejd_ecodefn);
  }
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

static void ejdb2_ctx_finalizer(void *isolate_callback_data, Dart_WeakPersistentHandle handle, void *peer) {
  EJDB2Ctx *ctx = peer;
  if (!ctx) return;
  if (ctx->db) {
    iwrc rc = ejdb_close(&ctx->db);
    if (rc) iwlog_ecode_error3(rc);
    ctx->db = 0;
  }
  if (ctx->port != ILLEGAL_PORT) {
    Dart_CloseNativePort(ctx->port);
  }
  free(ctx);
}

void ejdb2_port_nfn(Dart_NativeArguments args) {
  EJDB2Ctx *ctx;
  intptr_t ptr = 0;

  Dart_EnterScope();
  Dart_SetReturnValue(args, Dart_Null());

  Dart_Handle self = EJC(Dart_GetNativeArgument(args, 0));
  EJC(Dart_GetNativeInstanceField(self, 0, &ptr));

  if (!ptr) {
    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
      Dart_SetReturnValue(args, ejd_error_throw_scope(IW_ERROR_ALLOC, 0));
      goto finish;
    }
    ctx->db = 0;
    ctx->port = Dart_NewNativePort("ejdb2_port_nfn", ejdb2_port_handler, true);
    if (ctx->port == ILLEGAL_PORT) {
      Dart_SetReturnValue(args, ejd_error_throw_scope(EJD_ERROR_CREATE_PORT, 0));
      goto finish;
    }
    EJC(Dart_SetNativeInstanceField(self, 0, (intptr_t) ctx));
    if (!Dart_NewWeakPersistentHandle(self, ctx, sizeof(*ctx), ejdb2_ctx_finalizer)) {
      EJE(finish, args, "ejdb2_port_nfn::Dart_NewWeakPersistentHandle");
    }
  } else {
    ctx = (void *) ptr;
  }

	Dart_SetReturnValue(args, EJC(Dart_NewSendPort(ctx->port)));

finish:
  Dart_ExitScope();
}

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
