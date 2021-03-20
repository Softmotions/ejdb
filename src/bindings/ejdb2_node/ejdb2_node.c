#include <ejdb2.h>
#define NAPI_EXPERIMENTAL
#include "node_api.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>

#define STR_HELPER(x) #x
#define STR(x)        STR_HELPER(x)

static void jn_resultset_tsf(
  napi_env   env,
  napi_value js_add_stream,
  void      *context,
  void      *data);
static bool jn_throw_error(napi_env env, iwrc rc, const char *location, const char *msg);
static napi_value jn_create_error(napi_env env, iwrc rc, const char *location, const char *msg);

IW_INLINE bool jn_is_exception_pending(napi_env env) {
  bool rv = false;
  napi_is_exception_pending(env, &rv);
  return rv;
}

#define JNTHROW(env, rc, message) \
  jn_throw_error(env, rc, "ejdb2_node.c" ":" STR(__LINE__), message)

#define JNTHROW_LAST(env) do {                         \
    const napi_extended_error_info *info = 0;          \
    napi_get_last_error_info((env), &info);            \
    if (info) JNTHROW((env), 0, info->error_message);  \
} while (0)

#define JNCHECK(ns, env) do {                   \
    if (ns && ns != napi_pending_exception) {   \
      JNTHROW_LAST(env);                        \
    }                                           \
} while (0)

#define JNRC(env, rc) do {                      \
    if (rc && !jn_is_exception_pending(env)) {  \
      JNTHROW(env, rc, 0);                      \
    }                                           \
} while (0)

#define JNRET(ns, env, call, res) do {          \
    ns = (call);                                \
    if (ns) {                                   \
      if (ns != napi_pending_exception) {       \
        JNTHROW_LAST(env);                      \
      }                                         \
      return (res);                             \
    }                                           \
} while (0)

#define JNGO(ns, env, call, label) do {         \
    ns = (call);                                \
    if (ns) {                                   \
      if (ns != napi_pending_exception) {       \
        JNTHROW_LAST(env);                      \
      }                                         \
      goto label;                               \
    }                                           \
} while (0)

static napi_value jn_create_error(napi_env env, iwrc rc, const char *location, const char *msg) {
  // Eg:
  //  Error [@ejdb IWRC:70002 open]: IO error with expected errno status set. (IW_ERROR_IO_ERRNO)
  char codebuf[255];
  const char *code = codebuf;
  if (!msg) {
    if (rc) {
      msg = iwlog_ecode_explained(rc);
    } else {
      msg = "";
    }
  }
  if (rc) {
    iwrc_strip_code(&rc);
    if (location) {
      snprintf(codebuf, sizeof(codebuf), "@ejdb IWRC:%" PRId64 " %s", rc, location);
    } else {
      snprintf(codebuf, sizeof(codebuf), "@ejdb IWRC:%" PRId64, rc);
    }
  } else {
    code = "@ejdb";
  }
  napi_status ns;
  napi_value vcode, vmsg, verr = 0;
  JNGO(ns, env, napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &vcode), finish);
  JNGO(ns, env, napi_create_string_utf8(env, msg, NAPI_AUTO_LENGTH, &vmsg), finish);
  JNGO(ns, env, napi_create_error(env, vcode, vmsg, &verr), finish);

finish:
  return verr;
}

IW_INLINE bool jn_throw_error(napi_env env, iwrc rc, const char *location, const char *msg) {
  napi_value verr = jn_create_error(env, rc, location, msg);
  return verr && napi_throw(env, verr) == napi_ok;
}

typedef enum {
  _JN_ERROR_START = (IW_ERROR_START + 15000UL + 6000),
  JN_ERROR_INVALID_NATIVE_CALL_ARGS, /**< Invalid native function call args (JN_ERROR_INVALID_NATIVE_CALL_ARGS) */
  JN_ERROR_INVALID_STATE,            /**< Invalid native extension state (JN_ERROR_INVALID_STATE) */
  JN_ERROR_QUERY_IN_USE,
  /**< Query object is in use by active async iteration, and cannot be changed
     (JN_ERROR_QUERY_IN_USE) */
  JN_ERROR_NAPI,                     /*< N-API Error (JN_ERROR_NAPI) */
  _JN_ERROR_END,
} jn_ecode_t;

typedef struct JBN {
  EJDB      db;
  IWPOOL   *pool;
  EJDB_OPTS opts;
  napi_threadsafe_function resultset_tsf;
} *JBN;

typedef struct JNWORK {
  iwrc rc;        // RC error
  napi_status     ns;
  napi_deferred   deferred;
  napi_async_work async_work;
  const char     *async_resource;
  void *unwrapped;
  void *data;
  void (*release_data)(napi_env env, struct JNWORK *w);
  IWPOOL *pool;
} *JNWORK;

// Globals

static napi_ref k_vadd_streamfn_ref;

IW_INLINE napi_value jn_get_ref(napi_env env, napi_ref ref) {
  napi_value ret;
  napi_status ns;
  JNRET(ns, env, napi_get_reference_value(env, ref, &ret), 0);
  return ret;
}

IW_INLINE napi_value jn_null(napi_env env) {
  napi_value ret;
  napi_status ns;
  JNRET(ns, env, napi_get_null(env, &ret), 0);
  return ret;
}

IW_INLINE napi_value jn_global(napi_env env) {
  napi_value ret;
  napi_status ns;
  JNRET(ns, env, napi_get_global(env, &ret), 0);
  return ret;
}

IW_INLINE napi_value jn_undefined(napi_env env) {
  napi_value ret;
  napi_status ns;
  JNRET(ns, env, napi_get_undefined(env, &ret), 0);
  return ret;
}

IW_INLINE bool jn_is_null(napi_env env, napi_value val) {
  bool bv = false;
  napi_value rv = jn_null(env);
  if (!rv) {
    return false;
  }
  napi_strict_equals(env, val, rv, &bv);
  return bv;
}

IW_INLINE bool jn_is_undefined(napi_env env, napi_value val) {
  bool bv = false;
  napi_value rv = jn_undefined(env);
  if (!rv) {
    return false;
  }
  napi_strict_equals(env, val, rv, &bv);
  return bv;
}

IW_INLINE bool jn_is_null_or_undefined(napi_env env, napi_value val) {
  return jn_is_null(env, val) || jn_is_undefined(env, val);
}

IW_INLINE napi_value jn_create_string(napi_env env, const char *str) {
  napi_value ret;
  napi_status ns;
  JNRET(ns, env, napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &ret), 0);
  return ret;
}

IW_INLINE napi_value jn_create_int64(napi_env env, int64_t val) {
  napi_value ret;
  napi_status ns;
  JNRET(ns, env, napi_create_int64(env, val, &ret), 0);
  return ret;
}

static char *jn_string(napi_env env, napi_value val_, IWPOOL *pool, bool nulls, bool coerce, iwrc *rcp) {
  *rcp = 0;
  size_t len = 0;
  napi_status ns = 0;
  napi_value val = val_;
  if (nulls && jn_is_null_or_undefined(env, val)) {
    return 0;
  }
  if (coerce) {
    ns = napi_coerce_to_string(env, val, &val);
    if (ns) {
      *rcp = JN_ERROR_NAPI;
      JNCHECK(ns, env);
      return 0;
    }
  }
  ns = napi_get_value_string_utf8(env, val, 0, 0, &len);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return 0;
  }
  if (!len) {
    return 0;
  }
  char *buf = iwpool_alloc(len + 1, pool);
  if (!buf) {
    *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return 0;
  }
  ns = napi_get_value_string_utf8(env, val, buf, len + 1, &len);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return 0;
  }
  return buf;
}

static char *jn_string_at(
  napi_env env, IWPOOL *pool, napi_value arr,
  bool nulls, bool coerce, uint32_t idx, iwrc *rcp) {
  *rcp = 0;
  napi_value el;
  napi_status ns = napi_get_element(env, arr, idx, &el);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return 0;
  }
  return jn_string(env, el, pool, nulls, coerce, rcp);
}

static int64_t jn_int(napi_env env, napi_value val_, bool nulls, bool coerce, iwrc *rcp) {
  *rcp = 0;
  napi_status ns = 0;
  napi_value val = val_;
  if (nulls && jn_is_null_or_undefined(env, val)) {
    return 0;
  }
  if (coerce) {
    ns = napi_coerce_to_number(env, val, &val);
    if (ns) {
      *rcp = JN_ERROR_NAPI;
      JNCHECK(ns, env);
      return 0;
    }
  }
  int64_t rv = 0;
  ns = napi_get_value_int64(env, val, &rv);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return 0;
  }
  return rv;
}

static int64_t jn_int_at(napi_env env, napi_value arr, bool nulls, bool coerce, uint32_t idx, iwrc *rcp) {
  *rcp = 0;
  napi_value el;
  napi_status ns = napi_get_element(env, arr, idx, &el);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return 0;
  }
  return jn_int(env, el, nulls, coerce, rcp);
}

static double jn_double(napi_env env, napi_value val_, bool nulls, bool coerce, iwrc *rcp) {
  *rcp = 0;
  napi_status ns = 0;
  napi_value val = val_;
  if (nulls && jn_is_null_or_undefined(env, val)) {
    return 0;
  }
  if (coerce) {
    ns = napi_coerce_to_number(env, val, &val);
    if (ns) {
      *rcp = JN_ERROR_NAPI;
      JNCHECK(ns, env);
      return 0;
    }
  }
  double rv = 0;
  ns = napi_get_value_double(env, val, &rv);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return 0;
  }
  return rv;
}

static bool jn_bool(napi_env env, napi_value val_, bool nulls, bool coerce, iwrc *rcp) {
  *rcp = 0;
  napi_status ns = 0;
  napi_value val = val_;
  if (nulls && jn_is_null_or_undefined(env, val)) {
    return false;
  }
  if (coerce) {
    ns = napi_coerce_to_bool(env, val, &val);
    if (ns) {
      *rcp = JN_ERROR_NAPI;
      JNCHECK(ns, env);
      return false;
    }
  }
  bool rv = false;
  ns = napi_get_value_bool(env, val, &rv);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return false;
  }
  return rv;
}

static bool jn_bool_at(napi_env env, napi_value arr, bool nulls, bool coerce, uint32_t idx, iwrc *rcp) {
  *rcp = 0;
  napi_value el;
  napi_status ns = napi_get_element(env, arr, idx, &el);
  if (ns) {
    *rcp = JN_ERROR_NAPI;
    JNCHECK(ns, env);
    return false;
  }
  return jn_bool(env, el, nulls, coerce, rcp);
}

static iwrc jb_jbn_alloc(JBN *vp) {
  *vp = 0;
  IWPOOL *pool = iwpool_create(255);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBN v = iwpool_calloc(sizeof(*v), pool);
  if (!v) {
    iwrc rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    iwpool_destroy(pool);
    return rc;
  }
  v->pool = pool;
  *vp = v;
  return 0;
}

static void jn_jbn_destroy(napi_env env, JBN *vp) {
  if (!vp || !*vp) {
    return;
  }
  JBN v = *vp;
  if (v->db) {
    iwrc rc = ejdb_close(&v->db);
    if (rc) {
      iwlog_ecode_error3(rc);
    }
    JNRC(env, rc);
  }
  if (v->resultset_tsf) {
    napi_release_threadsafe_function(v->resultset_tsf, napi_tsfn_abort);
  }
  if (v->pool) {
    iwpool_destroy(v->pool);
  }
  *vp = 0;
}

static JNWORK jn_work_create(iwrc *rcp) {
  *rcp = 0;
  IWPOOL *pool = iwpool_create(255);
  if (!pool) {
    *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return 0;
  }
  JNWORK w = iwpool_calloc(sizeof(*w), pool);
  if (!w) {
    *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    iwpool_destroy(pool);
    return 0;
  }
  w->pool = pool;
  return w;
}

static void *jn_work_alloc_data(size_t siz, JNWORK work, iwrc *rcp) {
  *rcp = 0;
  work->data = iwpool_calloc(siz, work->pool);
  if (!work->data) {
    *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  return work->data;
}

void jn_work_destroy(napi_env env, JNWORK *wp) {
  if (!wp || !*wp) {
    return;
  }
  JNWORK w = *wp;
  if (w->deferred) {
    // Reject promise with undefined value
    napi_reject_deferred(env, w->deferred, jn_undefined(env));
  }
  if (w->async_work) {
    napi_delete_async_work(env, w->async_work);
  }
  if (w->release_data) {
    w->release_data(env, w);
  }
  if (w->pool) {
    iwpool_destroy(w->pool);
  }
  *wp = 0;
}

static void jn_ejdb2impl_finalize(napi_env env, void *data, void *hint) {
  JBN jbn = data;
  jn_jbn_destroy(env, &jbn);
}

// string array opts
static napi_value jn_ejdb2impl_ctor(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns = 0;

  size_t argc = 1;
  bool bv = false;
  uint32_t ulv = 0;

  JBN jbn = 0;
  napi_value varr, this;
  void *data;

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, &varr, &this, &data), finish);
  if (argc != 1) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    JNRC(env, rc);
    goto finish;
  }

  napi_is_array(env, varr, &bv);
  if (!bv) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    JNRC(env, rc);
    goto finish;
  }
  JNGO(ns, env, napi_get_array_length(env, varr, &ulv), finish);
  if (ulv != 16) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    JNRC(env, rc);
    goto finish;
  }

  rc = jb_jbn_alloc(&jbn);
  RCGO(rc, finish);

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

  argc = 0;
  jbn->opts.kv.path = jn_string_at(env, jbn->pool, varr, false, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.kv.oflags = jn_int_at(env, varr, false, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.kv.wal.enabled = jn_bool_at(env, varr, false, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.kv.wal.check_crc_on_checkpoint = jn_bool_at(env, varr, true, true, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.kv.wal.checkpoint_buffer_sz = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.kv.wal.checkpoint_timeout_sec = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.kv.wal.savepoint_timeout_sec = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.kv.wal.wal_buffer_sz = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.document_buffer_sz = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.sort_buffer_sz = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.http.enabled = jn_bool_at(env, varr, true, true, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.http.access_token = jn_string_at(env, jbn->pool, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.http.bind = jn_string_at(env, jbn->pool, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.http.max_body_size = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.http.port = jn_int_at(env, varr, true, false, argc++, &rc);
  RCGO(rc, finish);
  jbn->opts.http.read_anon = jn_bool_at(env, varr, true, true, argc++, &rc);
  RCGO(rc, finish);

  jbn->opts.kv.file_lock_fail_fast = true;
  jbn->opts.no_wal = !jbn->opts.kv.wal.enabled;
  jbn->opts.http.blocking = false;
  jbn->opts.http.access_token_len = jbn->opts.http.access_token ? strlen(jbn->opts.http.access_token) : 0;

  // Result-set thread-save function initialization
  napi_value vadd_streamfn;
  JNGO(ns, env, napi_get_reference_value(env, k_vadd_streamfn_ref, &vadd_streamfn), finish);
  JNGO(ns, env, napi_create_threadsafe_function(
         env,                                           // napi_env env,
         vadd_streamfn,                                 // napi_value func,
         0,                                             // napi_value async_resource,
         jn_create_string(env, "jn_add_stream_result"), // napi_value async_resource_name,
         1,                                             // size_t max_queue_size,
         1,                                             // size_t initial_thread_count,
         0,                                             // void* thread_finalize_data,
         0,                                             // napi_finalize thread_finalize_cb,
         0,                                             // void* context,
         jn_resultset_tsf,                              // napi_threadsafe_function_call_js call_js_cb,
         &jbn->resultset_tsf                            // napi_threadsafe_function* result
         ), finish);

  // Wrap class instance
  JNGO(ns, env, napi_wrap(env, this, jbn, jn_ejdb2impl_finalize, 0, 0), finish);

finish:
  if (jn_is_exception_pending(env) || rc) {
    JNRC(env, rc);
    if (jbn) {
      jn_jbn_destroy(env, &jbn);
    }
    return 0;
  }
  return this;
}

#define JNFUNC(func)       {#func, 0, jn_ ## func, 0, 0, 0, napi_default, 0 }
#define JNVAL(name, value) {#name, 0, 0, 0, 0, value, napi_default, 0 }

bool jn_resolve_pending_errors(napi_env env, napi_status ns, JNWORK work) {
  assert(work);
  if (!work->deferred) {
    return true;
  }
  bool pending = jn_is_exception_pending(env);
  if (!(work->rc || ns || pending)) {
    return false;
  }
  napi_value ex;
  if (pending && (!work->rc || (work->rc == JN_ERROR_NAPI))) {
    ns = napi_get_and_clear_last_exception(env, &ex);
    if (ns == napi_ok) {
      napi_reject_deferred(env, work->deferred, ex);
    }
  } else {
    napi_value verr;
    const napi_extended_error_info *info = 0;
    if (ns) {
      napi_get_last_error_info(env, &info);
    }
    if (pending) {
      napi_get_and_clear_last_exception(env, &ex);
    }
    verr = jn_create_error(env, work->rc, work->async_resource, info ? info->error_message : 0);
    if (verr) {
      napi_reject_deferred(env, work->deferred, verr);
    }
  }
  work->deferred = 0;
  return true;
}

static napi_value jn_launch_promise(
  napi_env                     env,
  napi_callback_info           info,
  const char                  *async_resource_name,
  napi_async_execute_callback  execute,
  napi_async_complete_callback complete,
  JNWORK                       work) {
  size_t argc = 0;
  napi_status ns = 0;
  napi_value promise = 0, this, awork;
  void *data;

  work->async_resource = async_resource_name;
  JNGO(ns, env, napi_get_cb_info(env, info, &argc, 0, &this, &data), finish);
  JNGO(ns, env, napi_unwrap(env, this, &work->unwrapped), finish);
  JNGO(ns, env, napi_create_promise(env, &work->deferred, &promise), finish);
  JNGO(ns, env, napi_create_string_utf8(env, async_resource_name, NAPI_AUTO_LENGTH, &awork), finish);
  JNGO(ns, env, napi_create_async_work(env, 0, awork, execute, complete, work, &work->async_work), finish);
  JNGO(ns, env, napi_queue_async_work(env, work->async_work), finish);

finish:
  if (ns || jn_is_exception_pending(env)) {
    if (work) {
      jn_resolve_pending_errors(env, ns, work);
      jn_work_destroy(env, &work);
    }
  }
  return promise;
}

//  ---------------- EJDB2.open()

static void jn_open_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (jbn->db) {
    return;            // Database is already opened
  }
  work->rc = ejdb_open(&jbn->opts, &jbn->db);
}

static void jn_open_complete(napi_env env, napi_status ns, void *data) {
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, jn_undefined(env)), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_open(napi_env env, napi_callback_info info) {
  iwrc rc;
  JNWORK work = jn_work_create(&rc);
  if (rc) {
    JNRC(env, rc);
    return jn_undefined(env);
  }
  napi_value ret = jn_launch_promise(env, info, "open", jn_open_execute, jn_open_complete, work);
  return ret ? ret : jn_undefined(env);
}

//  ---------------- EJDB2.close()

static void jn_close_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    return;
  }
  work->rc = ejdb_close(&jbn->db);
}

static void jn_close_complete(napi_env env, napi_status ns, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, jn_undefined(env)), finish);
  work->deferred = 0;

finish:
  if (jbn->resultset_tsf) {
    napi_release_threadsafe_function(jbn->resultset_tsf, napi_tsfn_abort);
    jbn->resultset_tsf = 0;
  }
  jn_work_destroy(env, &work);
}

static napi_value jn_close(napi_env env, napi_callback_info info) {
  iwrc rc;
  JNWORK work = jn_work_create(&rc);
  if (rc) {
    JNRC(env, rc);
    return jn_undefined(env);
  }
  napi_value ret = jn_launch_promise(env, info, "close", jn_close_execute, jn_close_complete, work);
  return ret ? ret : jn_undefined(env);
}

//  ---------------- EJDB2.put/patch()

struct JNPUT_DATA {
  int64_t     id;
  const char *coll;
  const char *json;
  bool patch;
};

static void jn_put_execute(napi_env env, void *data) {
  JBL jbl = 0;
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    goto finish;
  }
  struct JNPUT_DATA *wdata = work->data;
  if (!wdata->patch) {
    work->rc = jbl_from_json(&jbl, wdata->json);
    RCGO(work->rc, finish);
  }
  if (wdata->id > 0) {
    if (wdata->patch) {
      work->rc = ejdb_patch(jbn->db, wdata->coll, wdata->json, wdata->id);
    } else {
      work->rc = ejdb_put(jbn->db, wdata->coll, jbl, wdata->id);
    }
  } else {
    work->rc = ejdb_put_new(jbn->db, wdata->coll, jbl, &wdata->id);
  }
finish:
  if (jbl) {
    jbl_destroy(&jbl);
  }
}

static void jn_put_complete(napi_env env, napi_status ns, void *data) {
  napi_value rv;
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  struct JNPUT_DATA *wdata = work->data;
  ns = napi_create_int64(env, wdata->id, &rv);
  if (ns) {
    jn_resolve_pending_errors(env, ns, work);
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, rv), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
}

// collection, json, id
static napi_value jn_put_patch(napi_env env, napi_callback_info info, bool patch, bool upsert) {
  iwrc rc = 0;
  napi_status ns = 0;
  napi_value this, argv[3] = { 0 };
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;
  napi_value ret = 0;
  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  struct JNPUT_DATA *wdata = jn_work_alloc_data(sizeof(*wdata), work, &rc);
  RCGO(rc, finish);
  wdata->patch = patch;
  wdata->coll = jn_string(env, argv[0], work->pool, false, true, &rc);
  RCGO(rc, finish);
  wdata->json = jn_string(env, argv[1], work->pool, false, false, &rc);
  RCGO(rc, finish);
  wdata->id = jn_int(env, argv[2], true, true, &rc);
  RCGO(rc, finish);

  if ((wdata->id < 1) && wdata->patch) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  ret = jn_launch_promise(env, info, wdata->patch ? "patch" : "put", jn_put_execute, jn_put_complete, work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

static napi_value jn_put(napi_env env, napi_callback_info info) {
  return jn_put_patch(env, info, false, false);
}

static napi_value jn_patch(napi_env env, napi_callback_info info) {
  return jn_put_patch(env, info, true, false);
}

static napi_value jn_patch_or_put(napi_env env, napi_callback_info info) {
  return jn_put_patch(env, info, true, true);
}

//  ---------------- EJDB2.get()

struct JNGET_DATA {
  int64_t     id;
  const char *coll;
  JBL jbl;
};

static void jn_get_data_destroy(napi_env env, JNWORK w) {
  struct JNGET_DATA *wdata = w->data;
  if (wdata && wdata->jbl) {
    jbl_destroy(&wdata->jbl);
  }
}

static void jn_get_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    return;
  }
  struct JNGET_DATA *wdata = work->data;
  work->rc = ejdb_get(jbn->db, wdata->coll, wdata->id, &wdata->jbl);
}

static void jn_get_complete(napi_env env, napi_status ns, void *data) {
  napi_value rv;
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  struct JNGET_DATA *wdata = work->data;
  IWXSTR *xstr = iwxstr_new2(jbl_size(wdata->jbl) * 2);
  if (!xstr) {
    work->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish0;
  }
  work->rc = jbl_as_json(wdata->jbl, jbl_xstr_json_printer, xstr, 0);
  RCGO(work->rc, finish0);
  JNGO(ns, env, napi_create_string_utf8(env, iwxstr_ptr(xstr), iwxstr_size(xstr), &rv), finish0);
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, rv), finish0);
  work->deferred = 0;

finish0:
  if (xstr) {
    iwxstr_destroy(xstr);
  }
  if (work->rc || ns) {
    jn_resolve_pending_errors(env, ns, work);
  }
finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_get(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value this, argv[2];
  napi_value ret = 0;
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;

  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  struct JNGET_DATA *wdata = jn_work_alloc_data(sizeof(*wdata), work, &rc);
  RCGO(rc, finish);
  work->release_data = jn_get_data_destroy;
  wdata->coll = jn_string(env, argv[0], work->pool, false, false, &rc);
  RCGO(rc, finish);
  wdata->id = jn_int(env, argv[1], false, false, &rc);
  RCGO(rc, finish);

  ret = jn_launch_promise(env, info, "get", jn_get_execute, jn_get_complete, work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

//  ---------------- EJDB2.del()

static void jn_del_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    return;
  }
  struct JNGET_DATA *wdata = work->data;
  work->rc = ejdb_del(jbn->db, wdata->coll, wdata->id);
}

static void jn_del_complete(napi_env env, napi_status ns, void *data) {
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, jn_undefined(env)), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_del(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value this, argv[2];
  napi_value ret = 0;
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;

  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  struct JNGET_DATA *wdata = jn_work_alloc_data(sizeof(*wdata), work, &rc);
  RCGO(rc, finish);
  wdata->coll = jn_string(env, argv[0], work->pool, false, false, &rc);
  RCGO(rc, finish);
  wdata->id = jn_int(env, argv[1], false, false, &rc);
  RCGO(rc, finish);

  ret = jn_launch_promise(env, info, "del", jn_del_execute, jn_del_complete, work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

//  ---------------- EJDB2.renameCollection()

struct JNRENAME_DATA {
  const char *old_name;
  const char *new_name;
};

static void jn_rename_collection_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    return;
  }
  struct JNRENAME_DATA *wdata = work->data;
  work->rc = ejdb_rename_collection(jbn->db, wdata->old_name, wdata->new_name);
}

static void jn_rename_collection_complete(napi_env env, napi_status ns, void *data) {
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, jn_undefined(env)), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_rename_collection(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value this, argv[2];
  napi_value ret = 0;
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;

  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  struct JNRENAME_DATA *wdata = jn_work_alloc_data(sizeof(*wdata), work, &rc);
  RCGO(rc, finish);

  wdata->old_name = jn_string(env, argv[0], work->pool, false, false, &rc);
  RCGO(rc, finish);

  wdata->new_name = jn_string(env, argv[1], work->pool, false, false, &rc);
  RCGO(rc, finish);

  ret = jn_launch_promise(env, info, "rename",
                          jn_rename_collection_execute, jn_rename_collection_complete,
                          work);
finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

//  ---------------- EJDB2.info()

static void jn_info_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    return;
  }
  struct JNGET_DATA *wdata = work->data;
  work->rc = ejdb_get_meta(jbn->db, &wdata->jbl);
}

static void jn_info_complete(napi_env env, napi_status ns, void *data) {
  napi_value rv;
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  struct JNGET_DATA *wdata = work->data;
  IWXSTR *xstr = iwxstr_new2(jbl_size(wdata->jbl) * 2);
  if (!xstr) {
    work->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish0;
  }
  work->rc = jbl_as_json(wdata->jbl, jbl_xstr_json_printer, xstr, 0);
  RCGO(work->rc, finish0);
  JNGO(ns, env, napi_create_string_utf8(env, iwxstr_ptr(xstr), iwxstr_size(xstr), &rv), finish0);

  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, rv), finish0);
  work->deferred = 0;

finish0:
  if (xstr) {
    iwxstr_destroy(xstr);
  }
  if (work->rc || ns) {
    jn_resolve_pending_errors(env, ns, work);
  }
finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_info(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value this;
  napi_value ret = 0;
  size_t argc = 0;
  void *data;
  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, 0, &this, &data), finish);
  jn_work_alloc_data(sizeof(struct JNGET_DATA), work, &rc);
  RCGO(rc, finish);
  work->release_data = jn_get_data_destroy;
  ret = jn_launch_promise(env, info, "info", jn_info_execute, jn_info_complete, work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

//  ---------------- EJDB2.ensureIndex/removeIndex()

struct JNIDX_DATA {
  const char     *coll;
  const char     *path;
  ejdb_idx_mode_t mode;
  bool remove;
};

static void jn_index_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    return;
  }
  struct JNIDX_DATA *wdata = work->data;
  if (wdata->remove) {
    work->rc = ejdb_remove_index(jbn->db, wdata->coll, wdata->path, wdata->mode);
  } else {
    work->rc = ejdb_ensure_index(jbn->db, wdata->coll, wdata->path, wdata->mode);
  }
}

static void jn_index_complete(napi_env env, napi_status ns, void *data) {
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, jn_undefined(env)), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_index(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_value ret = 0;
  napi_status ns;
  napi_value this, argv[4];
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;

  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  struct JNIDX_DATA *wdata = jn_work_alloc_data(sizeof(*wdata), work, &rc);
  RCGO(rc, finish);
  wdata->coll = jn_string(env, argv[0], work->pool, false, false, &rc);
  RCGO(rc, finish);
  wdata->path = jn_string(env, argv[1], work->pool, false, false, &rc);
  RCGO(rc, finish);
  wdata->mode = jn_int(env, argv[2], false, false, &rc);
  RCGO(rc, finish);
  wdata->remove = jn_bool(env, argv[3], false, false, &rc);
  RCGO(rc, finish);

  ret = jn_launch_promise(env, info, "index", jn_index_execute, jn_index_complete, work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

// ---------------- EJDB2.removeCollection()

struct JNRMC_DATA {
  const char *coll;
};

static void jn_rmcoll_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    return;
  }
  struct JNRMC_DATA *wdata = work->data;
  work->rc = ejdb_remove_collection(jbn->db, wdata->coll);
}

static void jn_rmcoll_complete(napi_env env, napi_status ns, void *data) {
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, jn_undefined(env)), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_rmcoll(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value this, argv;
  napi_value ret = 0;
  size_t argc = 1;
  void *data;

  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, &argv, &this, &data), finish);
  if (argc != 1) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  struct JNRMC_DATA *wdata = jn_work_alloc_data(sizeof(*wdata), work, &rc);
  RCGO(rc, finish);
  wdata->coll = jn_string(env, argv, work->pool, false, false, &rc);
  RCGO(rc, finish);

  ret = jn_launch_promise(env, info, "rmcoll", jn_rmcoll_execute, jn_rmcoll_complete, work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

//  ---------------- EJDB2.onlineBackup

struct JNBK_DATA {
  uint64_t    ts;
  const char *file_name;
};

static void jn_online_backup_execute(napi_env env, void *data) {
  JNWORK work = data;
  JBN jbn = work->unwrapped;
  if (!jbn->db) {
    work->rc = JN_ERROR_INVALID_STATE;
    return;
  }
  struct JNBK_DATA *wdata = work->data;
  work->rc = ejdb_online_backup(jbn->db, &wdata->ts, wdata->file_name);
}

static void jn_online_backup_complete(napi_env env, napi_status ns, void *data) {
  napi_value rv;
  JNWORK work = data;
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  struct JNBK_DATA *wdata = work->data;
  ns = napi_create_int64(env, (int64_t) wdata->ts, &rv);
  if (ns) {
    jn_resolve_pending_errors(env, ns, work);
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, rv), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
}

static napi_value jn_online_backup(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns = 0;
  napi_value this, argv[1] = { 0 };
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;
  napi_value ret = 0;
  JNWORK work = jn_work_create(&rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  struct JNBK_DATA *wdata = jn_work_alloc_data(sizeof(*wdata), work, &rc);
  RCGO(rc, finish);
  wdata->file_name = jn_string(env, argv[0], work->pool, false, true, &rc);
  ret = jn_launch_promise(env, info, "online_backup", jn_online_backup_execute, jn_online_backup_complete, work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (work) {
      jn_work_destroy(env, &work);
    }
  }
  return ret ? ret : jn_undefined(env);
}

// ---------------- jql_init

typedef struct JNQL {
  int refs;
  JQL jql;
} *JNQL;

static void jn_jnql_destroy_mt(JNQL *qlp) {
  if (!qlp || !*qlp) {
    return;
  }
  JNQL ql = *qlp;
  if (ql->jql) {
    jql_destroy(&ql->jql);
  }
  free(ql);
}

static void jn_jql_finalize(napi_env env, void *data, void *hint) {
  JNQL jnql = data;
  if (jnql) {
    jn_jnql_destroy_mt(&jnql);
  }
}

// JQL._impl.jql_init(this, query, collection);
static napi_value jn_jql_init(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value argv[3], this;
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;
  JNQL jnql = 0;

  IWPOOL *pool = iwpool_create(255);
  if (!pool) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  jnql = calloc(1, sizeof(*jnql));
  if (!jnql) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }

  const char *query = jn_string(env, argv[1], pool, false, false, &rc);
  RCGO(rc, finish);

  const char *collection = jn_string(env, argv[2], pool, true, false, &rc);
  RCGO(rc, finish);

  rc = jql_create2(&jnql->jql, collection, query, JQL_KEEP_QUERY_ON_PARSE_ERROR | JQL_SILENT_ON_PARSE_ERROR);
  RCGO(rc, finish);

  JNGO(ns, env, napi_wrap(env, argv[0], jnql, jn_jql_finalize, 0, 0), finish);

  napi_value vcoll = jn_create_string(env, jql_collection(jnql->jql));
  if (!vcoll) {
    goto finish;
  }
  JNGO(ns, env, napi_set_named_property(env, argv[0], "collection", vcoll), finish);

finish:
  if (rc) {
    if ((rc == JQL_ERROR_QUERY_PARSE) && jnql && jnql->jql) {
      JNTHROW(env, rc, jql_error(jnql->jql));
    } else {
      JNRC(env, rc);
    }
    if (jnql) {
      jn_jnql_destroy_mt(&jnql);
    }
  }
  iwpool_destroy(pool);
  return jn_undefined(env);
}

// ---------------- jql_stream_attach

typedef struct JNQS { // query stream associated data
  volatile bool aborted;
  volatile bool paused;
  int      refs;
  JBN      jbn;
  JNQL     jnql;
  napi_ref stream_ref;      // Reference to the stream object
  napi_ref explain_cb_ref;  // Reference to the optional explain callback
  int64_t  limit;
  struct JNWORK   work;
  pthread_mutex_t mtx;
  pthread_cond_t  cond;
} *JNQS;

typedef struct JNCS { // call data to `k_add_stream_tsfn`
  bool     has_count;
  IWXSTR  *log;
  IWXSTR  *document;
  int64_t  count;
  int64_t  document_id;
  napi_ref stream_ref; // copied from `JNQS`
} *JNCS;

static void jn_cs_destroy(JNCS *csp) {
  if (!csp || !*csp) {
    return;
  }
  JNCS cs = *csp;
  if (cs->document) {
    iwxstr_destroy(cs->document);
  }
  if (cs->log) {
    iwxstr_destroy(cs->log);
  }
  free(cs);
  *csp = 0;
}

// function addStreamResult(stream, id, jsondoc, log)
static void jn_resultset_tsf(
  napi_env   env,
  napi_value js_add_stream,
  void      *context,
  void      *data) {

  if (!env) { // shutdown pending
    JNCS cs = data;
    jn_cs_destroy(&cs);
    return;
  }

  JNCS cs = data;
  napi_status ns;
  napi_value vstream, vid, vdoc, vlog, vresult;
  napi_value vglobal = jn_global(env);
  napi_value vnull = jn_null(env);

  if (!vglobal || !vnull) {
    goto finish;
  }
  vstream = jn_get_ref(env, cs->stream_ref);
  if (!vstream) { // exception pending
    goto finish;
  }
  if (cs->document) {
    vdoc = jn_create_string(env, iwxstr_ptr(cs->document));
    iwxstr_destroy(cs->document);
    cs->document = 0;
  } else if (cs->has_count) {
    vdoc = jn_create_int64(env, cs->count);
  } else {
    vdoc = vnull;
  }
  if (!vdoc) {
    goto finish;
  }
  vid = jn_create_int64(env, cs->document_id);
  if (!vid) {
    goto finish;
  }
  if (cs->log) {
    vlog = jn_create_string(env, iwxstr_ptr(cs->log));
  } else {
    vlog = vnull;
  }

  napi_value argv[] = { vstream, vid, vdoc, vlog };
  const int argc = sizeof(argv) / sizeof(argv[0]);
  JNGO(ns, env, napi_call_function(
         env,
         vglobal,
         js_add_stream,
         argc,
         argv,
         &vresult
         ), finish);

finish:
  if (cs->document_id < 0) {
    uint32_t refs;
    napi_reference_unref(env, cs->stream_ref, &refs);
  }
  jn_cs_destroy(&cs);
}

static void jn_jnqs_destroy_mt(napi_env env, JNQS *qsp) {
  if (!qsp || !*qsp) {
    return;
  }
  JNQS qs = *qsp;
  uint32_t rcnt = 0;
  if (--qs->refs > 0) {
    return;
  }
  if (qs->jnql) {
    qs->jnql->refs--;
  }
  if (qs->stream_ref) {
    napi_reference_unref(env, qs->stream_ref, &rcnt);
    if (!rcnt) {
      napi_ref ref = qs->stream_ref;
      qs->stream_ref = 0;
      napi_delete_reference(env, ref);
    }
  }
  if (qs->explain_cb_ref) {
    napi_reference_unref(env, qs->explain_cb_ref, &rcnt);
    if (!rcnt) {
      napi_ref ref = qs->explain_cb_ref;
      qs->explain_cb_ref = 0;
      napi_delete_reference(env, ref);
    }
  }
  free(qs);
}

static iwrc jn_stream_pause_guard(JNQS qs) {
  iwrc rc = 0;
  int rci = pthread_mutex_lock(&qs->mtx);
  if (rci) {
    return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
  }
  while (qs->paused) {
    rci = pthread_cond_wait(&qs->cond, &qs->mtx);
    if (rci) {
      rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
      break;
    }
  }
  pthread_mutex_unlock(&qs->mtx);
  return rc;
}

static iwrc jn_jql_stream_visitor(EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  JNCS cs = 0;
  JNQS qs = ux->opaque;
  JNWORK work = &qs->work;

  if (qs->aborted) {
    *step = 0;
    return 0;
  }
  work->rc = jn_stream_pause_guard(qs);
  RCRET(work->rc);

  IWXSTR *xstr = iwxstr_new();
  if (!xstr) {
    work->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return work->rc;
  }
  if (doc->node) {
    work->rc = jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0);
  } else {
    work->rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0);
  }
  RCGO(work->rc, finish);

  cs = malloc(sizeof(*cs));
  if (!cs) {
    work->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  cs->count = 0;
  cs->has_count = false;
  cs->stream_ref = qs->stream_ref;
  cs->log = ux->log;
  cs->document_id = doc->id;
  cs->document = xstr;

  napi_status ns = napi_call_threadsafe_function(qs->jbn->resultset_tsf, cs, napi_tsfn_blocking);
  if (ns) {
    work->rc = JN_ERROR_NAPI;
    work->ns = ns;
    goto finish;
  }
  ux->log = 0;

finish:
  if (work->rc) {
    iwxstr_destroy(xstr);
    if (cs) {
      cs->document = 0; // kept in xstr
      jn_cs_destroy(&cs);
    }
  }
  return work->rc;
}

static void jn_jql_stream_execute(napi_env env, void *data) {
  napi_status ns;
  uint32_t refs = 0;
  JNWORK work = data;
  JNQS qs = work->data;
  JQL q = qs->jnql->jql;
  JNCS cs = 0;
  EJDB_EXEC ux = { 0 };
  bool has_count = jql_has_aggregate_count(q);

  // Trying to stop on paused stream
  // not in context of query execution
  // hence we can avoid unnecessary database locking
  // before start reading stream.
  work->rc = jn_stream_pause_guard(qs);
  RCGO(work->rc, finish);
  JNGO(ns, env, napi_reference_ref(env, qs->stream_ref, &refs), finish);

  if (qs->explain_cb_ref) {
    ux.log = iwxstr_new();
    if (!ux.log) {
      work->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
  }

  ux.q = q;
  ux.db = qs->jbn->db;
  ux.opaque = qs;
  ux.limit = qs->limit;
  if (!has_count) {
    ux.visitor = jn_jql_stream_visitor;
  }

  work->rc = ejdb_exec(&ux);
  RCGO(work->rc, finish);

  // Stream close event
  cs = malloc(sizeof(*cs));
  if (!cs) {
    work->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  cs->has_count = has_count;
  cs->count = ux.cnt;
  cs->stream_ref = qs->stream_ref;
  cs->log = ux.log;
  cs->document_id = -1;
  cs->document = 0;
  ux.log = 0;

  ns = napi_call_threadsafe_function(qs->jbn->resultset_tsf, cs, napi_tsfn_blocking);
  if (ns) {
    work->rc = JN_ERROR_NAPI;
    work->ns = ns;
    goto finish;
  }

  refs = 0;

finish:
  if (refs) {
    napi_reference_unref(env, qs->stream_ref, &refs);
  }
  if (ux.log) {
    iwxstr_destroy(ux.log);
  }
  if (work->rc) {
    jn_cs_destroy(&cs);
  }
}

static void jn_jql_stream_complete(napi_env env, napi_status ns, void *data) {
  JNWORK work = data;
  JNQS qs = work->data;
  if (!ns) {
    ns = work->ns;
  }
  if (jn_resolve_pending_errors(env, ns, work)) {
    goto finish;
  }
  JNGO(ns, env, napi_resolve_deferred(env, work->deferred, jn_undefined(env)), finish);
  work->deferred = 0;

finish:
  jn_work_destroy(env, &work);
  jn_jnqs_destroy_mt(env, &qs);
}

// this.jql._impl.jql_stream_destroy(this);
static napi_value jn_jql_stream_destroy(napi_env env, napi_callback_info info) {
  void *data;
  size_t argc = 1;
  napi_value ret = jn_undefined(env), argv, this;

  napi_status ns = napi_get_cb_info(env, info, &argc, &argv, &this, &data);
  if (ns || (argc < 1)) {
    goto finish;
  }

  ns = napi_remove_wrap(env, argv, &data);
  if (ns || !data) {
    goto finish;
  }

  JNQS qs = data;
  jn_jnqs_destroy_mt(env, &qs);

finish:
  return ret;
}

static napi_value jn_jql_stream_set_paused(napi_env env, napi_callback_info info, bool paused) {
  napi_status ns;
  napi_value argv, this;
  size_t argc = 1;
  void *data;

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, &argv, &this, &data), finish);
  if (argc != 1) {
    iwrc rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    JNRC(env, rc);
    goto finish;
  }
  JNGO(ns, env, napi_unwrap(env, argv, &data), finish);
  JNQS qs = data;
  assert(qs);

  int rci = pthread_mutex_lock(&qs->mtx);
  if (rci) {
    iwrc rc = iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    JNRC(env, rc);
    goto finish;
  }
  if (qs->paused != paused) {
    qs->paused = paused;
    pthread_cond_broadcast(&qs->cond);
  }
  pthread_mutex_unlock(&qs->mtx);

finish:
  return jn_undefined(env);
}

static napi_value jn_jql_stream_pause(napi_env env, napi_callback_info info) {
  return jn_jql_stream_set_paused(env, info, true);
}

static napi_value jn_jql_stream_resume(napi_env env, napi_callback_info info) {
  return jn_jql_stream_set_paused(env, info, false);
}

static napi_value jn_jql_stream_abort(napi_env env, napi_callback_info info) {
  napi_status ns;
  napi_value argv, this;
  size_t argc = 1;
  void *data;
  JNGO(ns, env, napi_get_cb_info(env, info, &argc, &argv, &this, &data), finish);
  if (argc != 1) {
    iwrc rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    JNRC(env, rc);
    goto finish;
  }
  JNGO(ns, env, napi_unwrap(env, argv, &data), finish);
  JNQS qs = data;
  qs->aborted = true;
finish:
  return jn_undefined(env);
}

// JQL._impl.jql_stream_attach(this, stream, [opts.limit, opts.explainCallback]);
static napi_value jn_jql_stream_attach(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value ret = 0, argv[3], this, vexplain;
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  void *data;

  JBN jbn;
  JNQL jnql;
  JNQS qs = 0;

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  JNGO(ns, env, napi_unwrap(env, argv[0], (void**) &jnql), finish);  // -V580

  qs = calloc(1, sizeof(*qs));
  if (!qs) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  memcpy(&qs->mtx, &mtx, sizeof(mtx));
  memcpy(&qs->cond, &cond, sizeof(cond));

  qs->paused = true;
  qs->jnql = jnql;
  qs->jnql->refs++; // Query in use
  qs->limit = jn_int_at(env, argv[2], true, false, 0, &rc);
  RCGO(rc, finish);

  JNGO(ns, env, napi_get_element(env, argv[2], 1, &vexplain), finish);
  if (!jn_is_null_or_undefined(env, vexplain)) {
    napi_valuetype vtype;
    JNGO(ns, env, napi_typeof(env, vexplain, &vtype), finish);
    if (vtype != napi_function) {
      rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
      goto finish;
    }
    JNGO(ns, env, napi_create_reference(env, vexplain, 1, &qs->explain_cb_ref), finish);
  }
  JNGO(ns, env, napi_unwrap(env, this, (void**) &jbn), finish);                   // -V580
  JNGO(ns, env, napi_create_reference(env, argv[1], 1, &qs->stream_ref), finish); // Reference to stream
  JNGO(ns, env, napi_wrap(env, argv[1], qs, 0, 0, 0), finish);

  // Launch async work
  qs->refs = 2;     // +1 for jn_jql_stream_complete
  // +1 for jn_jql_stream_destroy
  qs->jbn = jbn;
  qs->work.data = qs;
  ret = jn_launch_promise(env, info, "query", jn_jql_stream_execute, jn_jql_stream_complete, &qs->work);

finish:
  if (rc) {
    JNRC(env, rc);
    if (qs) {
      qs->refs = 0; // needed to destroy it completely
    }
    jn_jnqs_destroy_mt(env, &qs);
  }
  return ret ? ret : jn_undefined(env);
}

static void jn_jql_free_set_string_value(void *ptr, void *op) {
  IWPOOL *pool = op;
  if (pool) {
    iwpool_destroy(pool);
  }
}

// this._impl.jql_set(jql, placeholder, val, 1);
static napi_value jn_jql_set(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  int iplh = 0;
  napi_value argv[4], this;
  size_t argc = sizeof(argv) / sizeof(argv[0]);
  const char *splh = 0, *svalue;
  JNQL jnql;
  void *data;
  napi_status ns;
  napi_valuetype vtype;

  IWPOOL *vpool = 0; // jql_set_xxx value
  IWPOOL *pool = iwpool_create(32);
  if (!pool) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, argv, &this, &data), finish);
  if (argc != sizeof(argv) / sizeof(argv[0])) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  JNGO(ns, env, napi_unwrap(env, argv[0], &data), finish);
  jnql = data;
  if (jnql->refs > 0) {
    rc = JN_ERROR_QUERY_IN_USE;
    goto finish;
  }

  // Set type
  int stype = jn_int(env, argv[3], false, false, &rc);
  RCGO(rc, finish);

  // Placeholder
  JNGO(ns, env, napi_typeof(env, argv[1], &vtype), finish);

  if (vtype == napi_string) {
    iplh = 0;
    splh = jn_string(env, argv[1], pool, false, false, &rc);
  } else if (vtype == napi_number) {
    splh = 0;
    iplh = jn_int(env, argv[1], false, false, &rc);
  } else {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
  }
  RCGO(rc, finish);

  switch (stype) {
    case 6:   // String
    case 1:   // JSON
    case 2: { // Regexp
      JBL_NODE node;
      vpool = iwpool_create(64);
      if (!vpool) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto finish;
      }
      RCGO(rc, finish);
      switch (stype) {
        case 6: // String
        case 2: // Regexp
          svalue = jn_string(env, argv[2], vpool, false, false, &rc);
          RCGO(rc, finish);
          if (stype == 6) {
            rc = jql_set_str2(jnql->jql, splh, iplh, svalue, jn_jql_free_set_string_value, vpool); // -V614
          } else {
            rc = jql_set_regexp2(jnql->jql, splh, iplh, svalue, jn_jql_free_set_string_value, vpool);
          }
          RCGO(rc, finish);
          break;
        case 1:
          svalue = jn_string(env, argv[2], pool, false, false, &rc);
          RCGO(rc, finish);
          rc = jbn_from_json(svalue, &node, vpool);
          RCGO(rc, finish);
          rc = jql_set_json2(jnql->jql, splh, iplh, node, jn_jql_free_set_string_value, vpool);
          RCGO(rc, finish);
          break;
      }
    }
    break;

    case 3: { // Integer
      int64_t v = jn_int(env, argv[2], false, false, &rc);
      RCGO(rc, finish);
      rc = jql_set_i64(jnql->jql, splh, iplh, v);
      RCGO(rc, finish);
    }
    break;

    case 4: { // Double
      double v = jn_double(env, argv[2], false, false, &rc);
      RCGO(rc, finish);
      rc = jql_set_f64(jnql->jql, splh, iplh, v);
      RCGO(rc, finish);
    }
    break;

    case 5: { // Boolean
      bool v = jn_bool(env, argv[2], false, false, &rc);
      RCGO(rc, finish);
      rc = jql_set_bool(jnql->jql, splh, iplh, v);
      RCGO(rc, finish);
    }
    break;
  }

finish:
  if (pool) {
    iwpool_destroy(pool);
  }
  if (rc) {
    if (vpool) { // vpool will be destroyed when JQL freed
      iwpool_destroy(vpool);
    }
    JNRC(env, rc);
  }
  return jn_undefined(env);
}

// jql_limit(jql);
static napi_value jn_jql_limit(napi_env env, napi_callback_info info) {
  iwrc rc = 0;
  napi_status ns;
  napi_value ret = 0, argv, this;
  size_t argc = 1;
  void *data;
  JNQL jnql;
  int64_t limit;

  JNGO(ns, env, napi_get_cb_info(env, info, &argc, &argv, &this, &data), finish);
  if (argc != 1) {
    rc = JN_ERROR_INVALID_NATIVE_CALL_ARGS;
    goto finish;
  }
  JNGO(ns, env, napi_unwrap(env, argv, &data), finish);
  jnql = data;

  rc = jql_get_limit(jnql->jql, &limit);
  RCGO(rc, finish);

  ret = jn_create_int64(env, limit);

finish:
  if (rc) {
    JNRC(env, rc);
  }
  return ret ? ret : jn_undefined(env);
}

// ----------------

static const char *jn_ecodefn(locale_t locale, uint32_t ecode) {
  if (!((ecode > _JN_ERROR_START) && (ecode < _JN_ERROR_END))) {
    return 0;
  }
  switch (ecode) {
    case JN_ERROR_INVALID_NATIVE_CALL_ARGS:
      return "Invalid native function call args (JN_ERROR_INVALID_NATIVE_CALL_ARGS)";
    case JN_ERROR_INVALID_STATE:
      return "Invalid native extension state (JN_ERROR_INVALID_STATE)";
    case JN_ERROR_QUERY_IN_USE:
      return "Query object is in-use by active async iteration, and cannot be changed (JN_ERROR_QUERY_IN_USE)";
    case JN_ERROR_NAPI:
      return "N-API Error (JN_ERROR_NAPI)";
  }
  return 0;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_status ns = 0;
  static volatile int jn_ecodefn_initialized = 0;
  if (__sync_bool_compare_and_swap(&jn_ecodefn_initialized, 0, 1)) {
    iwrc rc = ejdb_init();
    if (rc) {
      JNRC(env, rc);
      return 0;
    }
    iwlog_register_ecodefn(jn_ecodefn);
  }
  napi_value vglobal, vadd_streamfn;

  JNGO(ns, env, napi_get_global(env, &vglobal), finish);

  // Define EJDB2Impl class
  napi_value ejdb2impl_clazz;
  napi_property_descriptor properties[] = {
    JNFUNC(open),
    JNFUNC(close),
    JNFUNC(put),
    JNFUNC(patch),
    JNFUNC(patch_or_put),
    JNFUNC(get),
    JNFUNC(del),
    JNFUNC(rename_collection),
    JNFUNC(info),
    JNFUNC(index),
    JNFUNC(rmcoll),
    JNFUNC(online_backup),
    JNFUNC(jql_init),
    JNFUNC(jql_set),
    JNFUNC(jql_limit),
    JNFUNC(jql_stream_destroy),
    JNFUNC(jql_stream_attach),
    JNFUNC(jql_stream_pause),
    JNFUNC(jql_stream_resume),
    JNFUNC(jql_stream_abort)
  };
  JNGO(ns, env, napi_define_class(env, "EJDB2Impl", NAPI_AUTO_LENGTH, jn_ejdb2impl_ctor, 0,
                                  sizeof(properties) / sizeof(properties[0]),
                                  properties, &ejdb2impl_clazz), finish);

  napi_property_descriptor descriptors[] = {
    { "EJDB2Impl", 0, 0, 0, 0, ejdb2impl_clazz, napi_default, 0 }
  };
  JNGO(ns, env, napi_define_properties(env, exports,
                                       sizeof(descriptors) / sizeof(descriptors[0]),
                                       descriptors),
       finish);

  JNGO(ns, env, napi_get_named_property(env, vglobal, "__ejdb_add_stream_result__", &vadd_streamfn), finish);
  if (jn_is_null_or_undefined(env, vadd_streamfn)) {
    iwrc rc = JN_ERROR_INVALID_STATE;
    JNRC(env, rc);
    goto finish;
  }
  JNGO(ns, env, napi_create_reference(env, vadd_streamfn, 1, &k_vadd_streamfn_ref), finish);

finish:
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
