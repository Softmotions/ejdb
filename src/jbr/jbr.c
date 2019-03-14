#include "jbr.h"
#include <iowow/iwconv.h>
#include "ejdb2_internal.h"

#define FIO_INCLUDE_STR

#include <fio.h>
#include <fiobj.h>
#include <fiobj_data.h>
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
//    GET|HEAD `/{collection}/{id}`
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

static uint64_t k_header_x_access_token_hash;
static uint64_t k_header_content_length_hash;
static uint64_t k_header_content_type_hash;

typedef enum {
  JBR_GET = 1,
  JBR_PUT,
  JBR_PATCH,
  JBR_DELETE,
  JBR_POST,
  JBR_HEAD
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
  JBR jbr;
  http_s *req;
  jbr_method_t method;
  const char *collection;
  size_t collection_len;
  int64_t id;
  bool read_anon;
} JBRCTX;

#define JBR_RC_REPORT(code_, r_, rc_)                                              \
  do {                                                                             \
    if ((code_) >= 500) iwlog_ecode_error3(rc_);                                   \
    const char *strerr = iwlog_ecode_explained(rc_);                               \
    jbr_http_send(r_, code_, "text/plain", strerr, strerr ? strlen(strerr) : 0);   \
  } while(0)

IW_INLINE void jbr_http_set_content_length(http_s *r, uintptr_t length) {
  if (!fiobj_hash_get2(r->private_data.out_headers, k_header_content_length_hash)) {
    fiobj_hash_set(r->private_data.out_headers, HTTP_HEADER_CONTENT_LENGTH,
                   fiobj_num_new(length));
  }
}
IW_INLINE void jbr_http_set_content_type(http_s *r, const char *ctype) {
  if (!fiobj_hash_get2(r->private_data.out_headers, k_header_content_type_hash)) {
    fiobj_hash_set(r->private_data.out_headers, HTTP_HEADER_CONTENT_TYPE,
                   fiobj_str_new(ctype, strlen(ctype)));
  }
}

static iwrc jbr_http_send(http_s *r, int status, const char *ctype, const char *body, int bodylen)  {
  if (!r || !r->private_data.out_headers) {
    iwlog_ecode_error3(IW_ERROR_INVALID_ARGS);
    return IW_ERROR_INVALID_ARGS;
  }
  r->status = status;
  if (ctype) {
    jbr_http_set_content_type(r, ctype);
  }
  if (http_send_body(r, (char *)body, bodylen)) {
    iwlog_ecode_error3(JBR_ERROR_SEND_RESPONSE);
    return JBR_ERROR_SEND_RESPONSE;
  }
  return 0;
}

IW_INLINE iwrc jbr_http_error_send(http_s *r, int status)  {
  return jbr_http_send(r, status, 0, 0, 0);
}

IW_INLINE iwrc jbr_http_error_send2(http_s *r, int status, const char *ctype, const char *body, int bodylen)  {
  return jbr_http_send(r, status, ctype, body, bodylen);
}

static iwrc jbr_query_visitor(EJDB_EXEC *ctx, EJDB_DOC doc, int64_t *step) {
  return 0;
}

static void jbr_on_query(JBRCTX *rctx) {
  // todo
}

static void jbr_on_patch(JBRCTX *rctx) {
  if (rctx->read_anon) {
    jbr_http_error_send(rctx->req, 403);
    return;
  }
  JBL jbl;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  fio_str_info_s data = fiobj_data_read(req->body, 0);
  if (data.len < 1) {
    jbr_http_error_send(rctx->req, 400);
    return;
  }
  iwrc rc = ejdb_patch(db, rctx->collection, data.data, rctx->id);
  iwrc_strip_code(&rc);
  switch (rc) {
    case JBL_ERROR_PARSE_JSON:
    case JBL_ERROR_PARSE_INVALID_CODEPOINT:
    case JBL_ERROR_PARSE_INVALID_UTF8:
    case JBL_ERROR_PARSE_UNQUOTED_STRING:
    case JBL_ERROR_PATCH_TARGET_INVALID:
    case JBL_ERROR_PATCH_NOVALUE:
    case JBL_ERROR_PATCH_INVALID_OP:
    case JBL_ERROR_PATCH_TEST_FAILED:
    case JBL_ERROR_PATCH_INVALID_ARRAY_INDEX:
    case JBL_ERROR_JSON_POINTER:
      JBR_RC_REPORT(400, req, rc);
      break;
    default:
      break;
  }
  if (rc) {
    JBR_RC_REPORT(500, req, rc);
  } else {
    jbr_http_send(req, 200, 0, 0, 0);
  }
}

static void jbr_on_delete(JBRCTX *rctx) {
  if (rctx->read_anon) {
    jbr_http_error_send(rctx->req, 403);
    return;
  }
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  iwrc rc = ejdb_remove(db, rctx->collection, rctx->id);
  if (rc == IWKV_ERROR_NOTFOUND) {
    jbr_http_error_send(req, 404);
    return;
  } else if (rc) {
    JBR_RC_REPORT(500, req, rc);
    return;
  }
  jbr_http_send(req, 200, 0, 0, 0);
}

static void jbr_on_put(JBRCTX *rctx) {
  if (rctx->read_anon) {
    jbr_http_error_send(rctx->req, 403);
    return;
  }
  JBL jbl;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  fio_str_info_s data = fiobj_data_read(req->body, 0);
  if (data.len < 1) {
    jbr_http_error_send(rctx->req, 400);
    return;
  }
  iwrc rc = jbl_from_json(&jbl, data.data);
  if (rc) {
    JBR_RC_REPORT(400, req, rc);
    return;
  }
  rc = ejdb_put(db, rctx->collection, jbl, rctx->id);
  if (rc) {
    JBR_RC_REPORT(500, req, rc);
    goto finish;
  }
  jbr_http_send(req, 200, 0, 0, 0);

finish:
  jbl_destroy(&jbl);
}

static void jbr_on_post(JBRCTX *rctx) {
  if (rctx->read_anon) {
    jbr_http_error_send(rctx->req, 403);
    return;
  }
  JBL jbl;
  int64_t id;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  fio_str_info_s data = fiobj_data_read(req->body, 0);
  if (data.len < 1) {
    jbr_http_error_send(rctx->req, 400);
    return;
  }
  iwrc rc = jbl_from_json(&jbl, data.data);
  if (rc) {
    JBR_RC_REPORT(400, req, rc);
    return;
  }
  rc = ejdb_put_new(db, rctx->collection, jbl, &id);
  if (rc) {
    JBR_RC_REPORT(500, req, rc);
    goto finish;
  }

  char nbuf[JBNUMBUF_SIZE];
  int len = iwitoa(id, nbuf, sizeof(nbuf));
  jbr_http_send(req, 200, "text/plain", nbuf, len);

finish:
  jbl_destroy(&jbl);
}

static void jbr_on_get(JBRCTX *rctx) {
  JBL jbl;
  IWXSTR *xstr = 0;
  int nbytes = 0;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;

  iwrc rc = ejdb_get(db, rctx->collection, rctx->id, &jbl);
  if (rc == IWKV_ERROR_NOTFOUND) {
    jbr_http_error_send(req, 404);
    return;
  } else if (rc) {
    JBR_RC_REPORT(500, req, rc);
    return;
  }
  if (req->method == JBR_HEAD) {
    rc = jbl_as_json(jbl, jbl_count_json_printer, &nbytes, JBL_PRINT_PRETTY);
  } else {
    xstr = iwxstr_new2(jbl->bn.size * 2);
    if (!xstr) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      JBR_RC_REPORT(500, req, rc);
      return;
    }
    rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY);
  }
  if (rc) {
    JBR_RC_REPORT(500, req, rc);
    goto finish;
  }
  jbl_destroy(&jbl);
  jbr_http_send(req, 200, "application/json",
                xstr ? iwxstr_ptr(xstr) : 0,
                xstr ? iwxstr_size(xstr) : nbytes);
finish:
  if (xstr) {
    iwxstr_destroy(xstr);
  }
}

static bool jbr_fill_ctx(http_s *req, JBRCTX *r) {
  JBR jbr = req->udata;
  memset(r, 0, sizeof(*r));
  r->req = req;
  r->jbr = jbr;
  fio_str_info_s method = fiobj_obj2cstr(req->method);
  switch (method.len) {
    case 3:
      if (!strncmp("GET", method.data, method.len)) {
        r->method = JBR_GET;
      } else if (!strncmp("PUT", method.data, method.len)) {
        r->method = JBR_PUT;
      }
      break;
    case 4:
      if (!strncmp("POST", method.data, method.len)) {
        r->method = JBR_POST;
      } else if (!strncmp("HEAD", method.data, method.len)) {
        r->method = JBR_HEAD;
      }
      break;
    case 5:
      if (!strncmp("PATCH", method.data, method.len)) {
        r->method = JBR_PATCH;
      }
      break;
    case 6:
      if (!strncmp("DELETE", method.data, method.len)) {
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
      switch (r->method) {
        case JBR_GET:
        case JBR_HEAD:
        case JBR_PUT:
        case JBR_DELETE:
        case JBR_PATCH:
          return false;
        default:
          break;
      }
      r->collection = path.data + 1;
      r->collection_len = path.len - 1;
    } else if (r->method != JBR_POST) {
      return false;
    }
  } else {
    char *eptr;
    char nbuf[JBNUMBUF_SIZE];
    r->collection = path.data + 1;
    r->collection_len = c - r->collection;
    int nlen = path.len - (c - path.data) - 1;
    if (nlen < 1) {
      goto finish;
    }
    if (nlen > JBNUMBUF_SIZE - 1) {
      return false;
    }
    memcpy(nbuf, r->collection + r->collection_len + 1, nlen);
    nbuf[nlen] =  '\0';
    r->id = strtoll(nbuf, &eptr, 10);
    if (*eptr != '\0' || r->id < 1 || r->method == JBR_POST) {
      return false;
    }
  }

finish:
  return (r->collection_len <= EJDB_COLLECTION_NAME_MAX_LEN);
}

static void on_http_request(http_s *req) {
  JBRCTX rctx;
  JBR jbr = req->udata;
  assert(jbr);
  const EJDB_HTTP *http = jbr->http;

  if (!jbr_fill_ctx(req, &rctx)) {
    http_send_error(req, 400); // Bad request
    return;
  }
  if (http->access_token) {
    FIOBJ h = fiobj_hash_get2(req->headers, k_header_x_access_token_hash);
    if (!h) {
      if (http->read_anon) {
        if (rctx.method == JBR_GET || rctx.method == JBR_HEAD || (rctx.method == JBR_POST && !rctx.collection)) {
          rctx.read_anon = true;
          goto process;
        }
      }
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

process:
  if (rctx.collection) {
    char cname[EJDB_COLLECTION_NAME_MAX_LEN + 1];
    // convert to `\0` terminated c-string
    memcpy(cname, rctx.collection, rctx.collection_len);
    cname[rctx.collection_len] = '\0';
    rctx.collection = cname;
    switch (rctx.method) {
      case JBR_GET:
      case JBR_HEAD:
        jbr_on_get(&rctx);
        break;
      case JBR_POST:
        jbr_on_post(&rctx);
        break;
      case JBR_PUT:
        jbr_on_put(&rctx);
        break;
      case JBR_PATCH:
        jbr_on_patch(&rctx);
        break;
      case JBR_DELETE:
        jbr_on_delete(&rctx);
        break;
      default:
        http_send_error(req, 400);
        break;
    }
  } else if (rctx.method == JBR_POST) {
    jbr_on_query(&rctx);
  } else {
    http_send_error(req, 400);
  }
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
    jbr->rc = iwrc_set_errno(JBR_ERROR_HTTP_LISTEN, errno);
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
  if (!(ecode > _JBR_ERROR_START && ecode < _JBR_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case JBR_ERROR_HTTP_LISTEN:
      return "Failed to start HTTP network listener (JBR_ERROR_HTTP_LISTEN)";
    case JBR_ERROR_SEND_RESPONSE:
      return "Error sending response (JBR_ERROR_SEND_RESPONSE)";
  }
  return 0;
}

iwrc jbr_init() {
  static int _jbr_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbr_initialized, 0, 1)) {
    return 0;
  }
  k_header_x_access_token_hash = fiobj_hash_string("x-access-token", 14);
  k_header_content_length_hash = fiobj_hash_string("content-length", 14);
  k_header_content_type_hash = fiobj_hash_string("content-type", 12);
  return iwlog_register_ecodefn(_jbr_ecodefn);
}
