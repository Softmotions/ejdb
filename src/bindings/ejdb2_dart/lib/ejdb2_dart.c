#define DART_SHARED_LIB
#include "dart_api.h"
#include "dart_native_api.h"
#include <ejdb2/iowow/iwconv.h>
#include "ejdb2.h"
#include "ejdb2cfg.h"

#if defined(__GNUC__)
#define IW_NORETURN __attribute__((noreturn))
#else
#define IW_NORETURN
#endif

typedef void(*WrapperFunction)(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);

typedef enum {
  _EJD_ERROR_START = (IW_ERROR_START + 15000UL + 4000),
  EJD_ERROR_CREATE_PORT,                 /**< Failed to create a Dart port (EJD_ERROR_CREATE_PORT) */
  EJD_ERROR_POST_PORT,                   /**< Failed to post message to Dart port (EJD_ERROR_POST_PORT) */
  EJD_ERROR_INVALID_NATIVE_CALL_ARGS,    /**< Invalid native function call args (EJD_ERROR_INVALID_NATIVE_CALL_ARGS) */
  EJD_ERROR_INVALID_STATE,               /**< Invalid native extension state (EJD_ERROR_INVALID_STATE) */
  _EJD_ERROR_END,
} jbr_ecode_t;

struct NativeFunctionLookup {
  const char *name;
  Dart_NativeFunction fn;
};

struct WrapperFunctionLookup {
  const char *name;
  WrapperFunction fn;
};

typedef struct EJDB2Context {
  Dart_Port port;
  volatile EJDB db;
} EJDB2Context;

Dart_NativeFunction ejd_resolve_name(Dart_Handle name, int argc, bool *auto_setup_scope);

IW_INLINE Dart_Handle ejd_error_check_propagate(Dart_Handle handle);
IW_INLINE Dart_Handle ejd_error_rc_create(iwrc rc);
IW_INLINE Dart_Handle ejd_error_rc_throw(iwrc rc);

static void explain_rc(Dart_NativeArguments args);
static void jql_exec(Dart_NativeArguments args);

static void ejdb2_port_handler(Dart_Port receive_port, Dart_CObject *msg);
static void ejdb2_ctx_finalizer(void *isolate_callback_data, Dart_WeakPersistentHandle handle, void *peer);

static void ejdb2_port(Dart_NativeArguments arguments);
static void ejdb2_set_handle(Dart_NativeArguments args);
static void ejdb2_get_handle(Dart_NativeArguments args);
static void ejdb2_create_query(Dart_NativeArguments args);

static void ejdb2_open_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejdb2_close_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);

static struct NativeFunctionLookup k_scoped_functions[] = {
  {"port", ejdb2_port},
  {"exec", jql_exec},
  {"create_query", ejdb2_create_query},
  {"set_handle", ejdb2_set_handle},
  {"get_handle", ejdb2_get_handle},
  {"explain_rc", explain_rc},
  {0, 0}
};

static struct WrapperFunctionLookup k_wrapped_functions[] = {
  {"open_wrapped", ejdb2_open_wrapped},
  {"close_wrapped", ejdb2_close_wrapped},
  {0, 0}
};

#define EJTH(h_) ejd_error_check_propagate(h_)

#define EJGO(h_, rh_, label_) \
  if (Dart_IsError(h_)) {   \
    rh_ = (h_);             \
    goto label_;            \
  }

#define EJLIB() EJTH(Dart_LookupLibrary(Dart_NewStringFromCString("package:ejdb2_dart/ejdb2_dart.dart")))

#define EJPORT_RC(co_, rc_)               \
  if (rc_) {                              \
    (co_)->type = Dart_CObject_kInt64;    \
    (co_)->value.as_int64 = (rc_);        \
  }

IW_INLINE char *cobject_str(Dart_CObject *co, bool nulls, iwrc *rcp) {
  *rcp = 0;
  if (co) {
    if (co->type == Dart_CObject_kString) {
      return co->value.as_string;
    } else if (nulls && co->type == Dart_CObject_kNull) {
      return 0;
    }
  }
  *rcp = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
  return 0;
}

IW_INLINE int64_t cobject_int(Dart_CObject *co, bool nulls, iwrc *rcp) {
  *rcp = 0;
  if (co) {
    if (co->type == Dart_CObject_kInt32) {
      return co->value.as_int32;
    } else if (co->type == Dart_CObject_kInt64) {
      return co->value.as_int64;
    } else if (nulls && co->type == Dart_CObject_kNull) {
      return 0;
    }
  }
  *rcp = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
  return 0;
}

IW_INLINE bool cobject_bool(Dart_CObject *co, bool nulls, iwrc *rcp) {
  *rcp = 0;
  if (co) {
    if (co->type == Dart_CObject_kBool) {
      return co->value.as_bool;
    } else if (nulls && co->type == Dart_CObject_kNull) {
      return false;
    }
  }
  *rcp = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
  return false;
}

IW_INLINE double cobject_double(Dart_CObject *co, bool nulls, iwrc *rcp) {
  *rcp = 0;
  if (co) {
    if (co->type == Dart_CObject_kDouble) {
      return co->value.as_double;
    } else if (nulls && co->type == Dart_CObject_kNull) {
      return 0;
    }
  }
  *rcp = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
  return 0;
}

IW_INLINE Dart_Handle ejd_error_check_propagate(Dart_Handle handle) {
  if (Dart_IsError(handle)) {
    Dart_PropagateError(handle);
  }
  return handle;
}

static Dart_Handle ejd_error_object_handle(iwrc rc, const char *msg) {
  Dart_Handle hmsg = Dart_Null();
  if (msg) {
    hmsg = EJTH(Dart_NewStringFromCString(msg));
  } else if (rc) {
    const char *explained = iwlog_ecode_explained(rc);
    if (explained) {
      hmsg = EJTH(Dart_NewStringFromCString(explained));
    }
  }
  Dart_Handle hrc = EJTH(Dart_NewIntegerFromUint64(rc));
  Dart_Handle hclass = EJTH(Dart_GetClass(EJLIB(), Dart_NewStringFromCString("EJDB2Error")));
  Dart_Handle args[] = {hrc, hmsg};
  return Dart_New(hclass, Dart_Null(), 2, args);
}

IW_INLINE Dart_Handle ejd_error_rc_create(iwrc rc) {
  const char *msg = iwlog_ecode_explained(rc);
  return Dart_NewUnhandledExceptionError(ejd_error_object_handle(rc, msg));
}

IW_INLINE Dart_Handle ejd_error_rc_create2(iwrc rc, const char *msg) {
  return Dart_NewUnhandledExceptionError(ejd_error_object_handle(rc, msg));
}

IW_INLINE Dart_Handle ejd_error_rc_throw(iwrc rc) {
  return Dart_PropagateError(ejd_error_rc_create(rc));
}

static void ejdb2_port(Dart_NativeArguments args) {
  EJDB2Context *ctx;
  intptr_t ptr = 0;

  Dart_EnterScope();
  Dart_Handle self = EJTH(Dart_GetNativeArgument(args, 0));
  EJTH(Dart_GetNativeInstanceField(self, 0, &ptr));

  if (!ptr) {
    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
      Dart_SetReturnValue(args, ejd_error_rc_create(IW_ERROR_ALLOC));
      goto finish;
    }
    ctx->db = 0;
    ctx->port = Dart_NewNativePort("ejdb2_port_handler", ejdb2_port_handler, true);
    if (ctx->port == ILLEGAL_PORT) {
      Dart_SetReturnValue(args, ejd_error_rc_create(EJD_ERROR_CREATE_PORT));
      goto finish;
    }
    Dart_SetNativeInstanceField(self, 0, (intptr_t) ctx);
    Dart_NewWeakPersistentHandle(self, ctx, sizeof(*ctx), ejdb2_ctx_finalizer);
  } else {
    ctx = (void *) ptr;
  }
  Dart_SetReturnValue(args, EJTH(Dart_NewSendPort(ctx->port)));

finish:
  Dart_ExitScope();
}

static void ejdb2_get_handle(Dart_NativeArguments args) {
  EJDB2Context *ctx;
  intptr_t ptr = 0;

  Dart_EnterScope();
  Dart_SetReturnValue(args, Dart_Null());

  Dart_Handle self = EJTH(Dart_GetNativeArgument(args, 0));
  EJTH(Dart_GetNativeInstanceField(self, 0, &ptr));

  ctx = (void *) ptr;
  if (ctx && ctx->db) {
    Dart_SetReturnValue(args, Dart_NewInteger((intptr_t) ctx->db));
  }
  Dart_ExitScope();
}

static void ejdb2_set_handle(Dart_NativeArguments args) {
  EJDB2Context *ctx;
  intptr_t ptr = 0;

  Dart_EnterScope();
  Dart_SetReturnValue(args, Dart_Null());

  Dart_Handle self = EJTH(Dart_GetNativeArgument(args, 0));
  EJTH(Dart_GetNativeInstanceField(self, 0, &ptr));

  ctx = (void *) ptr;
  if (!ctx) {
    Dart_SetReturnValue(args, ejd_error_rc_create(EJD_ERROR_INVALID_STATE));
    goto finish;
  }
  Dart_Handle dh = Dart_GetNativeArgument(args, 1);
  if (Dart_IsInteger(dh)) {
    EJTH(Dart_GetNativeIntegerArgument(args, 1, &ptr));
    ctx->db = (void *) ptr;
  } else if (Dart_IsNull(dh)) {
    ctx->db = 0;
  }

finish:
  Dart_ExitScope();
}

static void ejdb2_jql_finalizer(void *isolate_callback_data, Dart_WeakPersistentHandle handle, void *peer) {
  JQL q = (void *) peer;
  if (q) {
    jql_destroy(&q);
  }
}

static void ejdb2_create_query(Dart_NativeArguments args) {
  Dart_EnterScope();

  iwrc rc = 0;
  JQL q = 0;
  intptr_t ptr = 0;
  const char *query = 0;
  const char *collection = 0;

  Dart_Handle ret = Dart_Null();
  Dart_Handle hlib = EJLIB();
  Dart_Handle hself = EJTH(Dart_GetNativeArgument(args, 0));
  Dart_Handle hquery = EJTH(Dart_GetNativeArgument(args, 1));
  Dart_Handle hcoll = EJTH(Dart_GetNativeArgument(args, 2));

  EJTH(Dart_StringToCString(hquery, &query));
  if (Dart_IsString(hcoll)) {
    EJTH(Dart_StringToCString(hcoll, &collection));
  }
  EJTH(Dart_GetNativeInstanceField(hself, 0, &ptr));

  EJDB2Context *ctx = (void *) ptr;
  if (!ctx || !ctx->db) {
    rc = EJD_ERROR_INVALID_STATE;
    goto finish;
  }
  rc = jql_create2(&q, collection, query, JQL_KEEP_QUERY_ON_PARSE_ERROR | JQL_SILENT_ON_PARSE_ERROR);
  RCGO(rc, finish);

  if (!collection) {
    collection = jql_collection(q);
  }
  hcoll = Dart_NewStringFromCString(collection);
  EJGO(hcoll, ret, finish);

  Dart_Handle hclass = Dart_GetClass(hlib, Dart_NewStringFromCString("JQL"));
  EJGO(hclass, ret, finish);

  Dart_Handle jqargs[] = {hself, hquery, hcoll};
  Dart_Handle jqinst = Dart_New(hclass, Dart_NewStringFromCString("_"), 3, jqargs);
  EJGO(jqinst, ret, finish);

  ret = Dart_SetNativeInstanceField(jqinst, 0, (intptr_t) q);
  EJGO(ret, ret, finish);
  Dart_NewWeakPersistentHandle(jqinst, q, jql_estimate_allocated_size(q), ejdb2_jql_finalizer);

  ret = jqinst;

finish:
  if (rc || Dart_IsError(ret)) {
    if (rc) {
      if (q && rc == JQL_ERROR_QUERY_PARSE) {
        ret = ejd_error_rc_create2(rc, jql_error(q));
      } else {
        ret = ejd_error_rc_create(rc);
      }
    }
    if (q) {
      jql_destroy(&q);
    }
  }
  Dart_SetReturnValue(args, ret);
  Dart_ExitScope();
}

struct UXCTX {
  bool aggregate_count;
  Dart_Port reply_port;
};

static iwrc jql_exec_visitor(struct _EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  struct UXCTX *uctx = ux->opaque;
  if (uctx->aggregate_count) {
    return 0;
  }
  iwrc rc = 0;
  IWXSTR *xstr = iwxstr_new();
  if (!xstr) {
    return IW_ERROR_ALLOC;
  }
  rc = iwxstr_printf(xstr, "%lld\t", doc->id);
  RCGO(rc, finish);
  if (doc->node) {
    rc = jbl_node_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
  } else {
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
  }
  RCGO(rc, finish);

  Dart_CObject result;
  result.type = Dart_CObject_kString;
  result.value.as_string = iwxstr_ptr(xstr);

  if (!Dart_PostCObject(uctx->reply_port, &result)) {
    *step = 0; // End of cursor loop
  }

finish:
  iwxstr_destroy(xstr);
  return rc;
}

static void jql_exec_port_handler(Dart_Port receive_port, Dart_CObject *msg) {
  iwrc rc = 0;
  Dart_Port reply_port = ILLEGAL_PORT;
  Dart_CObject result = {.type = Dart_CObject_kNull};
  if (msg->type != Dart_CObject_kArray || msg->value.as_array.length != 3)  {
    iwlog_error2("Invalid message recieved");
    return;
  }
  //  [reply_port, qptr, dbptr]
  EJDB_EXEC ux = {0};
  Dart_CObject **varr = msg->value.as_array.values;
  reply_port = cobject_int(varr[0], false, &rc);
  RCGO(rc, finish);
  ux.q = (void *) cobject_int(varr[1], false, &rc);
  if (!ux.q) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB2Context *dctx = (void *) cobject_int(varr[2], false, &rc);
  if (!dctx || !dctx->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  struct UXCTX uctx = {
    .aggregate_count = jql_has_aggregate_count(ux.q),
    .reply_port = reply_port
  };
  ux.db = dctx->db;
  ux.visitor = jql_exec_visitor;
  ux.opaque = &uctx;

  rc = ejdb_exec(&ux);
  RCGO(rc, finish);

  if (uctx.aggregate_count) {
    Dart_CObject result_count;
    char nbuf[JBNUMBUF_SIZE];
    result_count.type = Dart_CObject_kString;
    result_count.value.as_string = nbuf;
    Dart_PostCObject(reply_port, &result_count);
  }

finish:
  if (rc) {
    iwlog_ecode_error3(rc);
    EJPORT_RC(&result, rc);
  }
  if (reply_port != ILLEGAL_PORT) {
    Dart_PostCObject(reply_port, &result); // Last NULL or error(int)
  }
  // Close own port, TODO: review
  Dart_CloseNativePort(receive_port);
}

static void jql_exec(Dart_NativeArguments args) {
  // void _exec(SendPort sendPort) native 'exec';
  Dart_EnterScope();

  iwrc rc = 0;
  intptr_t qptr = 0, ptr = 0;
  Dart_Port reply_port = ILLEGAL_PORT;
  Dart_Port exec_port = ILLEGAL_PORT;
  Dart_Handle ret = Dart_Null();

  Dart_Handle hself = EJTH(Dart_GetNativeArgument(args, 0));
  Dart_Handle hdb = EJTH(Dart_GetField(hself, Dart_NewStringFromCString("db")));
  Dart_Handle hport = EJTH(Dart_GetNativeArgument(args, 1));
  EJTH(Dart_SendPortGetId(hport, &reply_port));

  // EJDB2
  EJTH(Dart_GetNativeInstanceField(hdb, 0, &ptr));
  EJDB2Context *dctx = (void *) ptr;
  if (!dctx || !dctx->db) {
    rc = EJD_ERROR_INVALID_STATE;
    goto finish;
  }
  // JQL pointer
  EJTH(Dart_GetNativeInstanceField(hself, 0, &qptr));
  if (!qptr) {
    rc = EJD_ERROR_INVALID_STATE;
    goto finish;
  }
  // JQL exec port
  exec_port = Dart_NewNativePort("jql_exec_port_handler", jql_exec_port_handler, false);
  if (exec_port == ILLEGAL_PORT) {
    rc = EJD_ERROR_CREATE_PORT;
    goto finish;
  }
  // Now post a message to the query executor port: [reply_port, qptr, dctx]
  Dart_CObject msg, marg1, marg2, marg3;
  Dart_CObject *margs[] = {&marg1, &marg2, &marg3};

  msg.type = Dart_CObject_kArray;
  msg.value.as_array.length = 3;
  msg.value.as_array.values = margs;

  marg1.type = Dart_CObject_kInt64;
  marg1.value.as_int64 = reply_port;

  marg2.type = Dart_CObject_kInt64;
  marg2.value.as_int64 = qptr;

  marg3.type = Dart_CObject_kInt64;
  marg3.value.as_int64 = (int64_t) dctx;

  if (!Dart_PostCObject(exec_port, &msg)) {
    rc = EJD_ERROR_POST_PORT;
    goto finish;
  }

finish:
  if (rc || Dart_IsError(ret)) {
    if (exec_port != ILLEGAL_PORT) {
      Dart_CloseNativePort(exec_port);
    }
    if (rc) {
      ret = ejd_error_rc_create(rc);
    }
  }
  Dart_SetReturnValue(args, ret);
  Dart_ExitScope();
}

static void explain_rc(Dart_NativeArguments args) {
  Dart_EnterScope();
  int64_t llv = 0;
  EJTH(Dart_GetNativeIntegerArgument(args, 0, &llv));
  const char *msg = iwlog_ecode_explained((iwrc) llv);
  if (msg) {
    Dart_SetReturnValue(args, EJTH(Dart_NewStringFromCString(msg)));
  } else {
    Dart_SetReturnValue(args, Dart_Null());
  }
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
  EJTH(Dart_StringToCString(name, &cname));
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

static void ejdb2_port_handler(Dart_Port receive_port, Dart_CObject *msg) {
  if (msg->type != Dart_CObject_kArray
      || msg->value.as_array.length < 2
      || msg->value.as_array.values[0]->type != Dart_CObject_kSendPort
      || msg->value.as_array.values[1]->type != Dart_CObject_kString) {
    iwlog_error2("Invalid message recieved");
    return;
  }
  Dart_Port reply_port = msg->value.as_array.values[0]->value.as_send_port.id;
  const char *fname = msg->value.as_array.values[1]->value.as_string;
  for (int i = 0; k_wrapped_functions[i].name; ++i) {
    if (strcmp(k_wrapped_functions[i].name, fname) == 0) {
      k_wrapped_functions[i].fn(receive_port, msg, reply_port);
      break;
    }
  }
}

static void ejdb2_open_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result, rv1;
  Dart_CObject *rv[1] = {&rv1};

  int c = 2;
  const int pnum = 16;
  EJDB_OPTS opts = {0};
  EJDB db = 0;

  if (msg->type != Dart_CObject_kArray || msg->value.as_array.length != pnum + c)  {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  Dart_CObject **varr = msg->value.as_array.values;

  // opts.kv.path                         // non null
  // opts.kv.oflags                       // non null
  // opts.kv.wal.enabled                  // non null
  // opts.kv.wal.check_crc_on_checkpoint
  // opts.kv.wal.checkpoint_buffer_sz
  // opts.kv.wal.checkpoint_timeout_sec
  // opts.kv.wal.savepoint_timeout_sec
  // opts.kv.wal.wal_buffer_sz
  // opts.document_buffer_sz
  // opts.sort_buffer_sz
  // opts.http.enabled
  // opts.http.access_token
  // opts.http.bind
  // opts.http.max_body_size
  // opts.http.port
  // opts.http.read_anon

  opts.kv.path = cobject_str(varr[c++], false, &rc);
  RCGO(rc, finish);
  opts.kv.oflags = cobject_int(varr[c++], false, &rc);
  RCGO(rc, finish);
  opts.kv.wal.enabled = cobject_bool(varr[c++], false, &rc);
  RCGO(rc, finish);
  opts.kv.wal.check_crc_on_checkpoint = cobject_bool(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.kv.wal.checkpoint_buffer_sz = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.kv.wal.checkpoint_timeout_sec = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.kv.wal.savepoint_timeout_sec = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.kv.wal.wal_buffer_sz = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.document_buffer_sz = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.sort_buffer_sz = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.http.enabled = cobject_bool(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.http.access_token = cobject_str(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.http.bind = cobject_str(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.http.max_body_size = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.http.port = cobject_int(varr[c++], true, &rc);
  RCGO(rc, finish);
  opts.http.read_anon = cobject_bool(varr[c++], true, &rc);
  RCGO(rc, finish);

  opts.http.blocking = false;
  opts.no_wal = !opts.kv.wal.enabled;
  opts.http.access_token_len = opts.http.access_token ? strlen(opts.http.access_token) : 0;

  rc = ejdb_open(&opts, &db);
  RCGO(rc, finish);

  rv1.type = Dart_CObject_kInt64;
  rv1.value.as_int64 = (intptr_t) db;
  result.type = Dart_CObject_kArray;
  result.value.as_array.length = sizeof(rv) / sizeof(rv[0]);
  result.value.as_array.values = rv;

finish:
  if (rc) {
    iwlog_ecode_error3(rc);
    EJPORT_RC(&result, rc);
    if (db) {
      ejdb_close(&db);
    }
  }
  Dart_PostCObject(reply_port, &result);
}

static void ejdb2_close_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result;

  if (msg->value.as_array.length != 3)  {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  intptr_t ptr = cobject_int(msg->value.as_array.values[2], false, &rc);
  RCGO(rc, finish);
  EJDB db = (EJDB) ptr;
  if (!db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  rc = ejdb_close(&db);
  RCGO(rc, finish);

  if (receive_port != ILLEGAL_PORT) {
    Dart_CloseNativePort(receive_port);
  }

  result.type = Dart_CObject_kArray;
  result.value.as_array.length = 0;
  result.value.as_array.values = 0;

finish:
  if (rc) {
    EJPORT_RC(&result, rc);
  }
  Dart_PostCObject(reply_port, &result);
  return;
}

///////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////

static const char *_ejd_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _EJD_ERROR_START && ecode < _EJD_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case EJD_ERROR_CREATE_PORT:
      return "Failed to create a Dart port (EJD_ERROR_CREATE_PORT)";
    case EJD_ERROR_INVALID_NATIVE_CALL_ARGS:
      return "Invalid native function call args (EJD_ERROR_INVALID_NATIVE_CALL_ARGS)";
    case EJD_ERROR_INVALID_STATE:
      return "Invalid native extension state (EJD_ERROR_INVALID_STATE)";
    case EJD_ERROR_POST_PORT:
      return "Failed to post message to Dart port (EJD_ERROR_POST_PORT)";
  }
  return 0;
}

DART_EXPORT Dart_Handle ejdb2_dart_Init(Dart_Handle parent_library) {
  static volatile int ejd_ecodefn_initialized = 0;
  if (__sync_bool_compare_and_swap(&ejd_ecodefn_initialized, 0, 1)) {
    iwrc rc = ejdb_init();
    if (rc) {
      return ejd_error_rc_create(rc);
    }
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

static void ejdb2_ctx_finalizer(void *isolate_callback_data, Dart_WeakPersistentHandle handle, void *peer) {
  EJDB2Context *ctx = peer;
  if (!ctx) return;
  if (ctx->db) {
    iwrc rc = ejdb_close((void *)&ctx->db);
    if (rc) iwlog_ecode_error3(rc);
    ctx->db = 0;
  }
  free(ctx);
}
