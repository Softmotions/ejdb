#include "jbr.h"
#include <iowow/iwconv.h>
#include "ejdb2_internal.h"

#define FIO_INCLUDE_STR

#include <fio.h>
#include <fiobj.h>
#include <http/http.h>

// HTTP REST API:
//
// Access to HTTP endpoint can be optionally protected by a token specified in `EJDB_HTTP` options.
// In this case `X-Access-Token` header value must be provided in client request.
// If token is not provided `401` HTTP code will be in response.
// If access token doesn't matched a token provided by client `403` HTTP code will be in response.
//
// In any error case `500` error will be returned.
//
// `X-Hints` request header used to provide comma separated extra hints to ejdb2 database engine
//    Hints:
//      `explain` Show the query execution plan (indexes used).
//                Used in query execution POST `/` requests.
//                In this case instead of query resultset client will get query explanation text.
//
// Endpoints:
//
//    POST    `/{collection}`
//      Add a new document to the `collection`
//      Response:
//                200 with a new document identifier as int64 decimal number
//
//    PUT     `/{collection>/{id}`  Replaces/store document under specific numeric `id`
//      Response:
//                200 with empty body
//
//    DELETE  `/{collection}/{id}`
//      Removes document identified by `id` from a `collection`
//      Response:
//                200 on success
//                404 if document not found
//
//    PATCH   `/{collection}/{id}`
//      Patch a document identified by `id` by rfc7396,rfc6902 data.
//      Response:
//                200 on success
//
//    GET     `/{collection}/{id}`
//      Retrieve document identified by `id` from a `collection`
//      Response:
//                200 on success, document JSON
//                404 if document not found
//
//    POST `/`  - Query a collection by provided query spec as body
//      Response:
//               200 on success, HTTP chunked transfer, JSON documents separated by `\n`
//
//

static uint64_t x_access_token_hash;

typedef enum {
  JBR_GET = 1,
  JBR_PUT,
  JBR_POST,
  JBR_PATCH,
  JBR_DELETE
} jbr_method_t;

struct _JBR {
  volatile bool terminated;
  volatile iwrc rc;
  pthread_t worker_thread;
  pthread_barrier_t start_barrier;
  const EJDB_HTTP *http;
  EJDB db;
};

typedef struct _JBRCTX {
  jbr_method_t method;
  const char *collection;
  size_t collection_len;
  int64_t id;
} JBRCTX;

static iwrc jbr_query_visitor(EJDB_EXEC *ctx, EJDB_DOC doc, int64_t *step) {
  return 0;
}

static bool jbr_fill_ctx(http_s *req, JBRCTX *r) {
  memset(r, 0, sizeof(*r));
  fio_str_info_s method = fiobj_obj2cstr(req->method);
  switch (method.len) {
    case 3:
      if (!strncmp("GET", method.data, 3)) {
        r->method = JBR_GET;
      } else if (!strncmp("PUT", method.data, 3)) {
        r->method = JBR_PUT;
      }
      break;
    case 4:
      if (!strncmp("POST", method.data, 3)) {
        r->method = JBR_POST;
      }
      break;
    case 5:
      if (!strncmp("PATCH", method.data, 3)) {
        r->method = JBR_PATCH;
      }
      break;
    case 6:
      if (!strncmp("DELETE", method.data, 3)) {
        r->method = JBR_DELETE;
      }
  }
  if (!r->method) {
    return false;
  }
  fio_str_info_s path = fiobj_obj2cstr(req->path);
  if (!req->path || (path.len == 1 && path.data[0] == '/')) {
    return true;
  } else if (path.len < 2) {
    return false;
  }

  char *c = strchr(path.data + 1, '/');
  if (!c) {
     if (path.len > 1) {
       r->collection = path.data + 1;
       r->collection_len = path.len - 1;
     }
  } else {

  }

  return true;
}

static void on_http_request(http_s *req) {
  JBRCTX rctx;
  JBR jbr = req->udata;
  assert(jbr);
  const EJDB_HTTP *http = jbr->http;

  if (http->access_token) {
    FIOBJ h = fiobj_hash_get2(req->headers, x_access_token_hash);
    if (!h) {
      http_send_error(req, 401);
      return;
    }
    if (!fiobj_type_is(h, FIOBJ_T_STRING)) { // header specified more than once
      http_send_error(req, 400);
      return;
    }
    fio_str_info_s hv = fiobj_obj2cstr(h);
    if (hv.len != http->access_token_len || !memcmp(hv.data, http->access_token, http->access_token_len)) {
      http_send_error(req, 403);
      return;
    }
  }
  if (!jbr_fill_ctx(req, &rctx)) {
    http_send_error(req, 400); // Bad request
    return;
  }

  //fio_url_parse()
  // TODO:
  http_send_body(req, "Hello World!", 12);
}

static void on_finish(struct http_settings_s *settings) {
  iwlog_info2("HTTP endpoint closed");
}

static void on_pre_start(void *op) {
  JBR jbr = op;
  if (!jbr->http->blocking) {
    pthread_barrier_wait(&jbr->start_barrier);
  }
}

static void *_jbr_start_thread(void *op) {
  JBR jbr = op;
  char nbuf[JBNUMBUF_SIZE];
  const EJDB_HTTP *http = jbr->http;
  const char *bind = http->bind ? http->bind : "0.0.0.0";
  iwitoa(http->port, nbuf, sizeof(nbuf));

  iwlog_info("HTTP endpoint at %s:%s", bind, nbuf);
  if (http_listen(nbuf, bind,
                  .udata = jbr,
                  .on_request = on_http_request,
                  .on_finish = on_finish,
                  .max_body_size = 1024 * 1024 * 64, // 64Mb
                  .ws_max_msg_size = 1024 * 1024 * 64 // 64Mb
                 ) == -1) {
    jbr->rc = iwrc_set_errno(JBR_ERROR_HTTP_LISTEN_ERROR, errno);
    iwlog_ecode_error2(jbr->rc, "Failed to start HTTP server");
  }
  if (jbr->rc) {
    if (!jbr->http->blocking) {
      pthread_barrier_wait(&jbr->start_barrier);
    }
    return 0;
  }
  fio_state_callback_add(FIO_CALL_PRE_START, on_pre_start, jbr);
  fio_start(.threads = -1, .workers = 1); // Will block current thread here
  return 0;
}

static void _jbr_release(JBR *pjbr) {
  JBR jbr = *pjbr;
  free(jbr);
  *pjbr = 0;
}

iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, JBR *pjbr) {
  iwrc rc;
  *pjbr = 0;
  if (!opts->http.enabled) {
    return 0;
  }
  JBR jbr = calloc(1, sizeof(*jbr));
  if (!jbr) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  jbr->db = db;
  jbr->terminated = true;
  jbr->http = &opts->http;

  if (!jbr->http->blocking) {
    int rci = pthread_barrier_init(&jbr->start_barrier, 0, 2);
    if (rci) {
      free(jbr);
      return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    }
    rci = pthread_create(&jbr->worker_thread, 0, _jbr_start_thread, jbr);
    if (rci) {
      pthread_barrier_destroy(&jbr->start_barrier);
      free(jbr);
      return iwrc_set_errno(IW_ERROR_THREADING_ERRNO, rci);
    }
    pthread_barrier_wait(&jbr->start_barrier);
    pthread_barrier_destroy(&jbr->start_barrier);
    jbr->terminated = false;
    rc = jbr->rc;
    if (rc) {
      jbr_shutdown(pjbr);
      return rc;
    }
    *pjbr = jbr;
  } else {
    *pjbr = jbr;
    jbr->terminated = false;
    _jbr_start_thread(jbr); // Will block here
    rc = jbr->rc;
    jbr->terminated = true;
    IWRC(jbr_shutdown(pjbr), rc);
  }
  return rc;
}

iwrc jbr_shutdown(JBR *pjbr) {
  if (!*pjbr) {
    return 0;
  }
  JBR jbr = *pjbr;
  if (__sync_bool_compare_and_swap(&jbr->terminated, 0, 1)) {
    fio_state_callback_remove(FIO_CALL_PRE_START, on_pre_start, jbr);
    fio_stop();
    if (!jbr->http->blocking) {
      pthread_join(jbr->worker_thread, 0);
    }
  }
  _jbr_release(pjbr);
  *pjbr = 0;
  return 0;
}

static const char *_jbr_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JBR_ERROR_START && ecode < _JBR_ERROR_START)) {
    return 0;
  }
  switch (ecode) {
    case JBR_ERROR_HTTP_LISTEN_ERROR:
      return "Failed to start HTTP network listener (JBR_ERROR_HTTP_LISTEN_ERROR)";
  }
  return 0;
}

iwrc jbr_init() {
  static int _jbr_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbr_initialized, 0, 1)) {
    return 0;
  }
  x_access_token_hash = fiobj_hash_string("x-access-token", 14);
  return iwlog_register_ecodefn(_jbr_ecodefn);
}
