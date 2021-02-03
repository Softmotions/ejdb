#define DART_SHARED_LIB
#include "dart_api.h"
#include "dart_native_api.h"
#include <ejdb2.h>
#include <ejdb2/iowow/iwconv.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

typedef void (*WrapperFunction)(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);

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
  const char     *name;
  WrapperFunction fn;
};

typedef struct EJDB2Handle {
  EJDB    db;
  char   *path;
  int64_t refs;
  struct EJDB2Handle *next;
  struct EJDB2Handle *prev;
} EJDB2Handle;

typedef struct EJDB2Context {
  Dart_Port    port;
  EJDB2Handle *dbh;
  Dart_WeakPersistentHandle wph;
} EJDB2Context;

typedef struct EJDB2JQLContext {
  JQL q;
  Dart_WeakPersistentHandle wph;
} EJDB2JQLContext;

static pthread_mutex_t k_shared_scope_mtx = PTHREAD_MUTEX_INITIALIZER;
static EJDB2Handle *k_head_handle;

static Dart_NativeFunction ejd_resolve_name(Dart_Handle name, int argc, bool *auto_setup_scope);

IW_INLINE Dart_Handle ejd_error_check_propagate(Dart_Handle handle);
IW_INLINE Dart_Handle ejd_error_rc_create(iwrc rc);
IW_INLINE void ejd_error_rc_throw(iwrc rc);

static void ejd_explain_rc(Dart_NativeArguments args);
static void ejd_exec(Dart_NativeArguments args);
static void ejd_exec_check(Dart_NativeArguments args);
static void ejd_jql_set(Dart_NativeArguments args);
static void ejd_jql_get_limit(Dart_NativeArguments args);

static void ejd_port_handler(Dart_Port receive_port, Dart_CObject *msg);
static void ejd_ctx_finalizer(void *isolate_callback_data, void *peer);

static void ejd_port(Dart_NativeArguments arguments);
static void ejd_set_handle(Dart_NativeArguments args);
static void ejd_get_handle(Dart_NativeArguments args);
static void ejd_create_query(Dart_NativeArguments args);

static void ejd_open_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_close_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_get_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_put_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_del_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_patch_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_idx_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_rmi_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_rmc_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_info_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_rename_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);
static void ejd_bkp_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port);

static struct NativeFunctionLookup k_scoped_functions[] = {
  { "port",          ejd_port          },
  { "exec",          ejd_exec          },
  { "check_exec",    ejd_exec_check    },
  { "jql_set",       ejd_jql_set       },
  { "jql_get_limit", ejd_jql_get_limit },
  { "create_query",  ejd_create_query  },
  { "set_handle",    ejd_set_handle    },
  { "get_handle",    ejd_get_handle    },
  { "explain_rc",    ejd_explain_rc    },
  { 0,               0                 }
};

static struct WrapperFunctionLookup k_wrapped_functions[] = {
  { "get",    ejd_get_wrapped    },
  { "put",    ejd_put_wrapped    },
  { "del",    ejd_del_wrapped    },
  { "rename", ejd_rename_wrapped },
  { "patch",  ejd_patch_wrapped  },
  { "idx",    ejd_idx_wrapped    },
  { "rmi",    ejd_rmi_wrapped    },
  { "rmc",    ejd_rmc_wrapped    },
  { "info",   ejd_info_wrapped   },
  { "open",   ejd_open_wrapped   },
  { "close",  ejd_close_wrapped  },
  { "bkp",    ejd_bkp_wrapped    },
  { 0,        0                  }
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
    } else if (nulls && (co->type == Dart_CObject_kNull)) {
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
    } else if (nulls && (co->type == Dart_CObject_kNull)) {
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
    } else if (nulls && (co->type == Dart_CObject_kNull)) {
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
    } else if (nulls && (co->type == Dart_CObject_kNull)) {
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
  Dart_Handle args[] = { hrc, hmsg };
  return Dart_New(hclass, Dart_Null(), 2, args);
}

IW_INLINE Dart_Handle ejd_error_rc_create(iwrc rc) {
  const char *msg = iwlog_ecode_explained(rc);
  return Dart_NewUnhandledExceptionError(ejd_error_object_handle(rc, msg));
}

IW_INLINE Dart_Handle ejd_error_rc_create2(iwrc rc, const char *msg) {
  return Dart_NewUnhandledExceptionError(ejd_error_object_handle(rc, msg));
}

IW_INLINE void ejd_error_rc_throw(iwrc rc) {
  Dart_PropagateError(ejd_error_rc_create(rc));
}

static void ejd_port(Dart_NativeArguments args) {
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
    ctx->dbh = 0;
    ctx->port = Dart_NewNativePort("ejd_port_handler", ejd_port_handler, true);
    if (ctx->port == ILLEGAL_PORT) {
      Dart_SetReturnValue(args, ejd_error_rc_create(EJD_ERROR_CREATE_PORT));
      goto finish;
    }
    ctx->wph = Dart_NewWeakPersistentHandle(self, ctx, sizeof(*ctx), ejd_ctx_finalizer);
    if (!ctx->wph) {
      Dart_SetReturnValue(args, ejd_error_rc_create(EJD_ERROR_INVALID_STATE));
      goto finish;
    }
    Dart_SetNativeInstanceField(self, 0, (intptr_t) ctx);
  } else {
    ctx = (void*) ptr;
  }
  Dart_SetReturnValue(args, EJTH(Dart_NewSendPort(ctx->port)));

finish:
  Dart_ExitScope();
}

static void ejd_get_handle(Dart_NativeArguments args) {
  intptr_t ptr = 0;
  Dart_EnterScope();
  Dart_Handle self = EJTH(Dart_GetNativeArgument(args, 0));
  EJTH(Dart_GetNativeInstanceField(self, 0, &ptr));
  EJDB2Context *ctx = (void*) ptr;
  if (ctx && ctx->dbh) {
    Dart_SetReturnValue(args, Dart_NewInteger((intptr_t) ctx->dbh));
  } else {
    Dart_SetReturnValue(args, Dart_Null());
  }
  Dart_ExitScope();
}

static void ejd_set_handle(Dart_NativeArguments args) {
  intptr_t ptr;
  Dart_EnterScope();
  Dart_SetReturnValue(args, Dart_Null());
  Dart_Handle self = EJTH(Dart_GetNativeArgument(args, 0));
  EJTH(Dart_GetNativeInstanceField(self, 0, &ptr));
  EJDB2Context *ctx = (void*) ptr;
  if (!ctx) {
    Dart_SetReturnValue(args, ejd_error_rc_create(EJD_ERROR_INVALID_STATE));
    goto finish;
  }
  Dart_Handle dh = Dart_GetNativeArgument(args, 1);
  if (Dart_IsInteger(dh)) {
    int64_t llv;
    EJTH(Dart_GetNativeIntegerArgument(args, 1, &llv));
    ctx->dbh = (void*) llv;
  } else if (Dart_IsNull(dh)) {
    ctx->dbh = 0;
  }

finish:
  Dart_ExitScope();
}

static void ejd_jql_finalizer(void *isolate_callback_data, void *peer) {
  EJDB2JQLContext *ctx = peer;
  if (ctx) {
    if (ctx->q) {
      jql_destroy(&ctx->q);
    }
    if (ctx->wph && Dart_CurrentIsolateGroup()) {
      Dart_DeleteWeakPersistentHandle(ctx->wph);
    }
    free(ctx);
  }
}

static void ejd_create_query(Dart_NativeArguments args) {
  Dart_EnterScope();

  iwrc rc = 0;
  JQL q = 0;
  intptr_t ptr = 0;
  const char *query = 0;
  const char *collection = 0;
  EJDB2JQLContext *qctx = 0;

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

  EJDB2Context *ctx = (void*) ptr;
  if (!ctx || !ctx->dbh || !ctx->dbh->db) {
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

  Dart_Handle jqargs[] = { hself, hquery, hcoll };
  Dart_Handle jqinst = Dart_New(hclass, Dart_NewStringFromCString("_"), 3, jqargs);
  EJGO(jqinst, ret, finish);

  ret = Dart_SetNativeInstanceField(jqinst, 0, (intptr_t) q);
  EJGO(ret, ret, finish);

  qctx = malloc(sizeof(*qctx));
  if (!qctx) {
    Dart_SetReturnValue(args, ejd_error_rc_create(IW_ERROR_ALLOC));
    goto finish;
  }

  qctx->q = q;
  qctx->wph = Dart_NewWeakPersistentHandle(jqinst, qctx, jql_estimate_allocated_size(q) + sizeof(*qctx),
                                           ejd_jql_finalizer);
  if (!qctx->wph) {
    rc = EJD_ERROR_INVALID_STATE;
    goto finish;
  }

  ret = jqinst;

finish:
  if (rc || Dart_IsError(ret)) {
    if (rc) {
      if (q && (rc == JQL_ERROR_QUERY_PARSE)) {
        ret = ejd_error_rc_create2(rc, jql_error(q));
      } else {
        ret = ejd_error_rc_create(rc);
      }
    }
    if (q) {
      jql_destroy(&q);
    }
    free(qctx);
  }
  Dart_SetReturnValue(args, ret);
  Dart_ExitScope();
}

// Query execution context
// Contains a state to manage resultset backpressure
typedef struct QCTX {
  bool      aggregate_count;
  bool      explain;
  bool      paused;
  int       pending_count;
  Dart_Port reply_port;
  JQL       q;
  EJDB2Context *dctx;
  int64_t       limit;
  pthread_mutex_t mtx;
  pthread_cond_t  cond;
} *QCTX;

static iwrc ejd_exec_pause_guard(QCTX qctx) {
  iwrc rc = 0;
  int rci = pthread_mutex_lock(&qctx->mtx);
  if (rci) {
    return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
  }
  while (qctx->paused) {
    rci = pthread_cond_wait(&qctx->cond, &qctx->mtx);
    if (rci) {
      rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
      break;
    }
  }
  qctx->pending_count++;
  pthread_mutex_unlock(&qctx->mtx);
  return rc;
}

static iwrc ejd_exec_visitor(struct _EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  iwrc rc = 0;
  QCTX qctx = ux->opaque;

  rc = ejd_exec_pause_guard(qctx);
  RCRET(rc);

  IWXSTR *xstr = iwxstr_new();
  if (!xstr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (doc->node) {
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
  } else {
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
  }
  RCGO(rc, finish);

  Dart_CObject result, rv1, rv2, rv3;
  Dart_CObject *rv[] = { &rv1, &rv2, &rv3 };
  result.type = Dart_CObject_kArray;
  result.value.as_array.length = sizeof(rv) / sizeof(rv[0]);
  result.value.as_array.values = rv;
  rv1.type = Dart_CObject_kInt64;
  rv1.value.as_int64 = doc->id;
  rv2.type = Dart_CObject_kString;
  rv2.value.as_string = iwxstr_ptr(xstr);
  if (ux->log) { // Add explain log to the first record
    rv3.type = Dart_CObject_kString;
    rv3.value.as_string = iwxstr_ptr(ux->log);
    ux->log = 0;
  } else {
    rv3.type = Dart_CObject_kNull;
  }
  if (!Dart_PostCObject(qctx->reply_port, &result)) {
    *step = 0; // End of cursor loop
  }

finish:
  iwxstr_destroy(xstr);
  return rc;
}

static void ejd_exec_port_handler(Dart_Port receive_port, Dart_CObject *msg) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kNull };
  if ((msg->type != Dart_CObject_kInt64) || !msg->value.as_int64) {
    iwlog_error2("Invalid message recieved");
    return;
  }
  QCTX qctx = (void*) msg->value.as_int64;
  IWXSTR *exlog = 0;
  EJDB_EXEC ux = { 0 };
  EJDB2Context *dctx = qctx->dctx;

  if (!qctx->q || !dctx || !dctx->dbh || !dctx->dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  if (qctx->explain) {
    exlog = iwxstr_new();
    if (!exlog) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
  }

  qctx->aggregate_count = jql_has_aggregate_count(qctx->q);

  ux.q = qctx->q;
  ux.db = dctx->dbh->db;
  ux.visitor = qctx->aggregate_count ? 0 : ejd_exec_visitor;
  ux.opaque = qctx;
  ux.log = exlog;
  ux.limit = qctx->limit;

  rc = ejdb_exec(&ux);
  RCGO(rc, finish);

  if (qctx->aggregate_count) {
    Dart_CObject result, rv1, rv2, rv3;
    Dart_CObject *rv[] = { &rv1, &rv2, &rv3 };
    result.type = Dart_CObject_kArray;
    result.value.as_array.length = (sizeof(rv) / sizeof(rv[0]));
    result.value.as_array.values = rv;
    rv1.type = Dart_CObject_kInt64;
    rv1.value.as_int64 = ux.cnt;
    rv2.type = Dart_CObject_kNull;
    if (exlog) {
      rv3.type = Dart_CObject_kString;
      rv3.value.as_string = iwxstr_ptr(exlog);
    } else {
      rv3.type = Dart_CObject_kNull;
    }
    Dart_PostCObject(qctx->reply_port, &result);
  } else if (exlog && (ux.cnt == 0)) {
    result.type = Dart_CObject_kString;
    result.value.as_string = iwxstr_ptr(exlog);
  }

finish:
  if (rc) {
    iwlog_ecode_error3(rc);
    EJPORT_RC(&result, rc);
  }
  if (qctx->reply_port != ILLEGAL_PORT) {
    Dart_PostCObject(qctx->reply_port, &result); // Last NULL or error(int)
  }
  if (exlog) {
    iwxstr_destroy(exlog);
  }
  Dart_CloseNativePort(receive_port);
}

static void ejd_exec(Dart_NativeArguments args) {
  Dart_EnterScope();

  iwrc rc = 0;
  intptr_t qptr, ptr = 0;
  bool explain = false;
  int64_t limit = 0;
  QCTX qctx = 0;

  Dart_Port reply_port = ILLEGAL_PORT;
  Dart_Port exec_port = ILLEGAL_PORT;
  Dart_Handle ret = Dart_Null();

  Dart_Handle hself = EJTH(Dart_GetNativeArgument(args, 0));
  Dart_Handle hdb = EJTH(Dart_GetField(hself, Dart_NewStringFromCString("db")));
  Dart_Handle hport = EJTH(Dart_GetNativeArgument(args, 1));

  EJTH(Dart_GetNativeBooleanArgument(args, 2, &explain));
  EJTH(Dart_GetNativeIntegerArgument(args, 3, &limit));
  EJTH(Dart_SendPortGetId(hport, &reply_port));

  EJTH(Dart_GetNativeInstanceField(hdb, 0, &ptr));
  EJDB2Context *dctx = (void*) ptr;
  if (!dctx || !dctx->dbh || !dctx->dbh->db) {
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
  exec_port = Dart_NewNativePort("ejd_exec_port_handler", ejd_exec_port_handler, false);
  if (exec_port == ILLEGAL_PORT) {
    rc = EJD_ERROR_CREATE_PORT;
    goto finish;
  }

  qctx = calloc(1, sizeof(*qctx));
  if (!qctx) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  memcpy(&qctx->mtx, &mtx, sizeof(mtx));
  memcpy(&qctx->cond, &cond, sizeof(cond));

  qctx->reply_port = reply_port;
  qctx->q = (void*) qptr;
  qctx->dctx = dctx;
  qctx->explain = explain;
  qctx->limit = limit;

  // Now post a message to the query executor
  Dart_CObject msg;
  msg.type = Dart_CObject_kInt64;
  msg.value.as_int64 = (int64_t) qctx;

  if (!Dart_PostCObject(exec_port, &msg)) {
    rc = EJD_ERROR_POST_PORT;
    goto finish;
  }

finish:
  if (rc || Dart_IsError(ret)) {
    if (qctx) {
      free(qctx);
      qctx = 0;
    }
    if (exec_port != ILLEGAL_PORT) {
      Dart_CloseNativePort(exec_port);
    }
    if (rc) {
      ret = ejd_error_rc_create(rc);
    }
  }
  if (qctx) {
    ret = Dart_NewInteger((int64_t) (void*) qctx);
  }
  Dart_SetReturnValue(args, ret);
  Dart_ExitScope();
}

static void ejd_exec_check(Dart_NativeArguments args) {
  iwrc rc = 0;
  Dart_EnterScope();
  Dart_Handle ret = Dart_Null();

  if (Dart_GetNativeArgumentCount(args) < 3) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  bool terminate = false;
  int64_t hptr = 0;

  EJTH(Dart_GetNativeIntegerArgument(args, 1, &hptr));
  EJTH(Dart_GetNativeBooleanArgument(args, 2, &terminate));

  if (hptr < 1) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  QCTX qctx = (void*) hptr;

  if (terminate) {
    free(qctx);
    goto finish;
  }

  int rci = pthread_mutex_lock(&qctx->mtx);
  if (rci) {
    rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    goto finish;
  }
  qctx->pending_count--;
  if (qctx->paused) {
    if (qctx->pending_count < 32) {
      qctx->paused = false;
      pthread_cond_broadcast(&qctx->cond);
    }
  } else if (qctx->pending_count > 64) {
    qctx->paused = true;
    pthread_cond_broadcast(&qctx->cond);
  }
  pthread_mutex_unlock(&qctx->mtx);

finish:
  if (rc) {
    ret = ejd_error_rc_create(rc);
  }
  Dart_SetReturnValue(args, ret);
  Dart_ExitScope();
}

static void ejd_free_str(void *ptr, void *op) {
  free(ptr);
}

static void ejd_free_json_node(void *ptr, void *op) {
  IWPOOL *pool = op;
  if (pool) {
    iwpool_destroy(pool);
  }
}

static void ejd_jql_get_limit(Dart_NativeArguments args) {
  Dart_EnterScope();
  Dart_Handle ret = Dart_Null();
  iwrc rc;
  int64_t limit;
  intptr_t ptr = 0;
  Dart_Handle hself = EJTH(Dart_GetNativeArgument(args, 0));
  EJTH(Dart_GetNativeInstanceField(hself, 0, &ptr));
  JQL q = (void*) ptr;
  if (!q) {
    rc = EJD_ERROR_INVALID_STATE;
    goto finish;
  }
  rc = jql_get_limit(q, &limit);
  RCGO(rc, finish);
  ret = Dart_NewInteger(limit);

finish:
  if (rc || Dart_IsError(ret)) {
    if (rc) {
      ret = ejd_error_rc_create(rc);
    }
  }
  Dart_SetReturnValue(args, ret);
  Dart_ExitScope();
}

static void ejd_jql_set(Dart_NativeArguments args) {
  // void set(dynamic place, dynamic value) native 'ejd_jql_set';
  Dart_EnterScope();
  Dart_Handle ret = Dart_Null();

  iwrc rc = 0;
  intptr_t ptr = 0;
  Dart_Handle hself = EJTH(Dart_GetNativeArgument(args, 0));
  EJTH(Dart_GetNativeInstanceField(hself, 0, &ptr));
  JQL q = (void*) ptr;
  if (!q) {
    rc = EJD_ERROR_INVALID_STATE;
    goto finish;
  }
  int64_t npl = 0, type = 0;
  const char *svalue, *spl = 0;
  Dart_Handle hpl = EJTH(Dart_GetNativeArgument(args, 1));
  Dart_Handle hvalue = EJTH(Dart_GetNativeArgument(args, 2));
  Dart_Handle htype = EJTH(Dart_GetNativeArgument(args, 3));

  if (Dart_IsString(hpl)) {
    EJTH(Dart_StringToCString(hpl, &spl));
  } else {
    EJTH(Dart_IntegerToInt64(hpl, &npl));
  }
  if (Dart_IsInteger(htype)) {
    EJTH(Dart_IntegerToInt64(htype, &type));
  }
  if (type == 1) { // JSON
    EJTH(Dart_StringToCString(hvalue, &svalue));
    JBL_NODE node;
    IWPOOL *pool = iwpool_create(64);
    if (!pool) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    rc = jbn_from_json(svalue, &node, pool);
    if (rc) {
      iwpool_destroy(pool);
      goto finish;
    }
    rc = jql_set_json2(q, spl, npl, node, ejd_free_json_node, pool);
    if (rc) {
      iwpool_destroy(pool);
      goto finish;
    }
  } else if (type == 2) { // Regexp
    EJTH(Dart_StringToCString(hvalue, &svalue));
    char *str = strdup(svalue);
    if (!str) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    rc = jql_set_regexp2(q, spl, npl, str, ejd_free_str, 0);
    if (rc) {
      free(str);
      goto finish;
    }
  } else { // All other cases
    if (Dart_IsString(hvalue)) {
      EJTH(Dart_StringToCString(hvalue, &svalue));
      char *str = strdup(svalue);
      if (!str) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto finish;
      }
      rc = jql_set_str2(q, spl, npl, str, ejd_free_str, 0);
      if (rc) {
        free(str);
        goto finish;
      }
    } else if (Dart_IsInteger(hvalue)) {
      int64_t val;
      EJTH(Dart_IntegerToInt64(hvalue, &val));
      rc = jql_set_i64(q, spl, npl, val);
    } else if (Dart_IsDouble(hvalue)) {
      double val;
      EJTH(Dart_DoubleValue(hvalue, &val));
      rc = jql_set_f64(q, spl, npl, val);
    } else if (Dart_IsBoolean(hvalue)) {
      bool val;
      EJTH(Dart_BooleanValue(hvalue, &val));
      rc = jql_set_bool(q, spl, npl, val);
    } else if (Dart_IsNull(hvalue)) {
      rc = jql_set_null(q, spl, npl);
    }
  }

finish:
  if (rc || Dart_IsError(ret)) {
    if (rc) {
      ret = ejd_error_rc_create(rc);
    }
  }
  Dart_SetReturnValue(args, ret);
  Dart_ExitScope();
}

static void ejd_explain_rc(Dart_NativeArguments args) {
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

static Dart_NativeFunction ejd_resolve_name(
  Dart_Handle name,
  int         argc,
  bool       *auto_setup_scope) {
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

static void ejd_port_handler(Dart_Port receive_port, Dart_CObject *msg) {
  if (  (msg->type != Dart_CObject_kArray)
     || (msg->value.as_array.length < 2)
     || (msg->value.as_array.values[0]->type != Dart_CObject_kSendPort)
     || (msg->value.as_array.values[1]->type != Dart_CObject_kString)) {
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

static iwrc ejdb2_isolate_shared_open(const EJDB_OPTS *opts, EJDB2Handle **hptr) {
  iwrc rc = 0;
  EJDB db = 0;
  int rci = pthread_mutex_lock(&k_shared_scope_mtx);
  if (rci) {
    return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
  }
  const char *path = opts->kv.path;
  EJDB2Handle *h = k_head_handle;
  while (h) {
    if (!strcmp(h->path, path)) {
      break;
    }
    h = h->next;
  }
  if (h) {
    h->refs++;
    *hptr = h;
    goto finish;
  }
  rc = ejdb_open(opts, &db);
  RCGO(rc, finish);

  h = calloc(1, sizeof(*h));
  if (!h) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  h->path = strdup(path);
  if (!h->path) {
    free(h);
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  h->refs = 1;
  h->db = db;
  if (k_head_handle) {
    k_head_handle->prev = h;
    h->next = k_head_handle;
  }
  k_head_handle = h;
  *hptr = h;

finish:
  pthread_mutex_unlock(&k_shared_scope_mtx);
  if (rc) {
    iwlog_ecode_error3(rc);
    if (db) {
      ejdb_close(&db);
    }
  }
  return rc;
}

static iwrc ejdb2_isolate_shared_release(EJDB2Handle **hp) {
  iwrc rc = 0;
  int rci = pthread_mutex_lock(&k_shared_scope_mtx);
  if (rci) {
    return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
  }
  EJDB2Handle *h = *hp;
  *hp = 0;
  if (--h->refs <= 0) {
    if (h->db) {
      rc = ejdb_close(&h->db);
    }
    if (h->prev) {
      h->prev->next = h->next;
    } else {
      k_head_handle = h->next;
    }
    if (h->next) {
      h->next->prev = h->prev;
    }
    free(h->path);
    free(h);
  }
  pthread_mutex_unlock(&k_shared_scope_mtx);
  return rc;
}

static void ejd_open_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result, rv1;
  Dart_CObject *rv[1] = { &rv1 };

  int c = 2;
  EJDB_OPTS opts = { 0 };
  EJDB2Handle *dbh = 0;

  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 16 + c)) {
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

  opts.kv.file_lock_fail_fast = true;
  opts.no_wal = !opts.kv.wal.enabled;
  opts.http.blocking = false;
  opts.http.access_token_len = opts.http.access_token ? strlen(opts.http.access_token) : 0;

  rc = ejdb2_isolate_shared_open(&opts, &dbh);
  RCGO(rc, finish);

  rv1.type = Dart_CObject_kInt64;
  rv1.value.as_int64 = (intptr_t) dbh;
  result.type = Dart_CObject_kArray;
  result.value.as_array.length = sizeof(rv) / sizeof(rv[0]);
  result.value.as_array.values = rv;

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_close_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kArray };

  if (msg->value.as_array.length != 3) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  intptr_t ptr = cobject_int(msg->value.as_array.values[2], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  rc = ejdb2_isolate_shared_release(&dbh);
  RCGO(rc, finish);

  if (receive_port != ILLEGAL_PORT) {
    Dart_CloseNativePort(receive_port);
  }

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
  return;
}

static void ejd_patch_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kArray };

  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 5 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  const char *coll = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  const char *patch = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  int64_t id = cobject_int(msg->value.as_array.values[c++], true, &rc);
  RCGO(rc, finish);

  bool upsert = cobject_bool(msg->value.as_array.values[c++], true, &rc);
  RCGO(rc, finish);

  if (upsert) {
    rc = ejdb_merge_or_put(db, coll, patch, id);
  } else {
    rc = ejdb_patch(db, coll, patch, id);
  }

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_put_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result, rv1;
  Dart_CObject *rv[1] = { &rv1 };

  JBL jbl = 0;
  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 4 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  const char *coll = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  const char *json = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  int64_t id = cobject_int(msg->value.as_array.values[c++], true, &rc);
  RCGO(rc, finish);

  rc = jbl_from_json(&jbl, json);
  RCGO(rc, finish);

  if (id > 0) {
    rc = ejdb_put(db, coll, jbl, id);
  } else {
    rc = ejdb_put_new(db, coll, jbl, &id);
  }
  RCGO(rc, finish);

  rv1.type = Dart_CObject_kInt64;
  rv1.value.as_int64 = id;
  result.type = Dart_CObject_kArray;
  result.value.as_array.length = sizeof(rv) / sizeof(rv[0]);
  result.value.as_array.values = rv;

finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_del_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kArray };

  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 3 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  const char *coll = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  int64_t id = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  rc = ejdb_del(db, coll, id);

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_rename_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kArray };

  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 3 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;

  const char *oname = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  const char *nname = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  rc = ejdb_rename_collection(db, oname, nname);

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_idx_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kArray };

  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 4 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  const char *coll = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  const char *path = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  int64_t mode = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  rc = ejdb_ensure_index(db, coll, path, mode);

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_rmi_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kArray };

  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 4 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  const char *coll = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  const char *path = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  int64_t mode = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  rc = ejdb_remove_index(db, coll, path, mode);

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_rmc_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result = { .type = Dart_CObject_kArray };

  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 2 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  const char *coll = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  rc = ejdb_remove_collection(db, coll);

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_bkp_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result, rv1;
  Dart_CObject *rv[1] = { &rv1 };
  uint64_t ts = 0;

  int c = 2;
  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 2 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  EJDB db = dbh->db;
  const char *fileName = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  rc = ejdb_online_backup(db, &ts, fileName);

  rv1.type = Dart_CObject_kInt64;
  rv1.value.as_int64 = ts;
  result.type = Dart_CObject_kArray;
  result.value.as_array.length = sizeof(rv) / sizeof(rv[0]);
  result.value.as_array.values = rv;

finish:
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
}

static void ejd_info_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result, rv1;
  Dart_CObject *rv[1] = { &rv1 };

  JBL jbl = 0;
  IWXSTR *xstr = 0;
  int c = 2;

  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 1 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  rc = ejdb_get_meta(db, &jbl);
  RCGO(rc, finish);

  xstr = iwxstr_new2(jbl_size(jbl) * 2);
  if (!xstr) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, 0);
  RCGO(rc, finish);

  rv1.type = Dart_CObject_kString;
  rv1.value.as_string = iwxstr_ptr(xstr);
  result.type = Dart_CObject_kArray;
  result.value.as_array.length = sizeof(rv) / sizeof(rv[0]);
  result.value.as_array.values = rv;

finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
  if (xstr) {
    iwxstr_destroy(xstr);
  }
}

static void ejd_get_wrapped(Dart_Port receive_port, Dart_CObject *msg, Dart_Port reply_port) {
  iwrc rc = 0;
  Dart_CObject result, rv1;
  Dart_CObject *rv[1] = { &rv1 };

  JBL jbl = 0;
  IWXSTR *xstr = 0;
  int c = 2;

  if ((msg->type != Dart_CObject_kArray) || (msg->value.as_array.length != 3 + c)) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  intptr_t ptr = cobject_int(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);
  EJDB2Handle *dbh = (EJDB2Handle*) ptr;
  if (!dbh || !dbh->db) {
    rc = EJD_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  EJDB db = dbh->db;
  const char *coll = cobject_str(msg->value.as_array.values[c++], false, &rc);
  RCGO(rc, finish);

  int64_t id = cobject_int(msg->value.as_array.values[c++], true, &rc);
  RCGO(rc, finish);

  rc = ejdb_get(db, coll, id, &jbl);
  RCGO(rc, finish);

  xstr = iwxstr_new2(jbl_size(jbl) * 2);
  if (!xstr) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, 0);
  RCGO(rc, finish);

  rv1.type = Dart_CObject_kString;
  rv1.value.as_string = iwxstr_ptr(xstr);
  result.type = Dart_CObject_kArray;
  result.value.as_array.length = sizeof(rv) / sizeof(rv[0]);
  result.value.as_array.values = rv;

finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
  EJPORT_RC(&result, rc);
  Dart_PostCObject(reply_port, &result);
  if (xstr) {
    iwxstr_destroy(xstr);
  }
}

///////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////

static const char *ejd_ecodefn(locale_t locale, uint32_t ecode) {
  if (!((ecode > _EJD_ERROR_START) && (ecode < _EJD_ERROR_END))) {
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

DART_EXPORT Dart_Handle ejdb2dart_Init(Dart_Handle parent_library) {
  static volatile int ejd_ecodefn_initialized = 0;
  if (__sync_bool_compare_and_swap(&ejd_ecodefn_initialized, 0, 1)) {
    iwrc rc = ejdb_init();
    if (rc) {
      return ejd_error_rc_create(rc);
    }
    iwlog_register_ecodefn(ejd_ecodefn);
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

static void ejd_ctx_finalizer(void *isolate_callback_data, void *peer) {
  EJDB2Context *ctx = peer;
  if (ctx) {
    if (ctx->dbh) {
      iwrc rc = ejdb2_isolate_shared_release(&ctx->dbh);
      if (rc) {
        iwlog_ecode_error3(rc);
      }
    }
    if (ctx->wph && Dart_CurrentIsolateGroup()) {
      Dart_DeleteWeakPersistentHandle(ctx->wph);
    }
  }

  free(ctx);
}
