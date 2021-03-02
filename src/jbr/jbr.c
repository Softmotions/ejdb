#include "jbr.h"
#include <ejdb2/iowow/iwconv.h>
#include <ejdb2/iowow/iwth.h>
#include "ejdb2_internal.h"

#define FIO_INCLUDE_STR

#include <fio.h>
#include <fiobj.h>
#include <fiobj_data.h>
#include <http/http.h>
#include <ctype.h>

#define JBR_MAX_KEY_LEN          36
#define JBR_HTTP_CHUNK_SIZE      4096
#define JBR_WS_STR_PREMATURE_END "Premature end of message"

static uint64_t k_header_x_access_token_hash;
static uint64_t k_header_x_hints_hash;
static uint64_t k_header_content_length_hash;
static uint64_t k_header_content_type_hash;

typedef enum {
  JBR_GET = 1,
  JBR_PUT,
  JBR_PATCH,
  JBR_DELETE,
  JBR_POST,
  JBR_HEAD,
  JBR_OPTIONS,
} jbr_method_t;

struct _JBR {
  volatile bool terminated;
  volatile iwrc rc;
  pthread_t     worker_thread;
  pthread_barrier_t start_barrier;
  const EJDB_HTTP  *http;
  EJDB db;
};

typedef struct _JBRCTX {
  JBR     jbr;
  http_s *req;
  const char  *collection;
  IWXSTR      *wbuf;
  int64_t      id;
  size_t       collection_len;
  jbr_method_t method;
  bool read_anon;
  bool data_sent;
} JBRCTX;

#define JBR_RC_REPORT(code_, r_, rc_)                                              \
  do {                                                                             \
    if ((code_) >= 500) iwlog_ecode_error3(rc_);                                   \
    const char *strerr = iwlog_ecode_explained(rc_);                               \
    _jbr_http_send(r_, code_, "text/plain", strerr, strerr ? strlen(strerr) : 0);   \
  } while (0)

IW_INLINE void _jbr_http_set_content_length(http_s *r, uintptr_t length) {
  if (!fiobj_hash_get2(r->private_data.out_headers, k_header_content_length_hash)) {
    fiobj_hash_set(r->private_data.out_headers, HTTP_HEADER_CONTENT_LENGTH,
                   fiobj_num_new(length));
  }
}

IW_INLINE void _jbr_http_set_content_type(http_s *r, const char *ctype) {
  if (!fiobj_hash_get2(r->private_data.out_headers, k_header_content_type_hash)) {
    fiobj_hash_set(r->private_data.out_headers, HTTP_HEADER_CONTENT_TYPE,
                   fiobj_str_new(ctype, strlen(ctype)));
  }
}

IW_INLINE void _jbr_http_set_header(
  http_s *r,
  char *name, size_t nlen,
  char *val, size_t vlen) {
  http_set_header2(r, (fio_str_info_s) {
    .data = name, .len = nlen
  }, (fio_str_info_s) {
    .data = val, .len = vlen
  });
}

static iwrc _jbr_http_send(http_s *r, int status, const char *ctype, const char *body, int bodylen) {
  if (!r || !r->private_data.out_headers) {
    iwlog_ecode_error3(IW_ERROR_INVALID_ARGS);
    return IW_ERROR_INVALID_ARGS;
  }
  r->status = status;
  if (ctype) {
    _jbr_http_set_content_type(r, ctype);
  }
  if (http_send_body(r, (char*) body, bodylen)) {
    iwlog_ecode_error3(JBR_ERROR_SEND_RESPONSE);
    return JBR_ERROR_SEND_RESPONSE;
  }
  return 0;
}

IW_INLINE iwrc _jbr_http_error_send(http_s *r, int status) {
  return _jbr_http_send(r, status, 0, 0, 0);
}

IW_INLINE iwrc _jbr_http_error_send2(http_s *r, int status, const char *ctype, const char *body, int bodylen) {
  return _jbr_http_send(r, status, ctype, body, bodylen);
}

static iwrc _jbr_flush_chunk(JBRCTX *rctx, bool finish) {
  http_s *req = rctx->req;
  IWXSTR *wbuf = rctx->wbuf;
  char nbuf[JBNUMBUF_SIZE + 2]; // + \r\n
  assert(wbuf);
  if (!rctx->data_sent) {
    req->status = 200;
    _jbr_http_set_content_type(req, "application/json");
    _jbr_http_set_header(req, "transfer-encoding", 17, "chunked", 7);
    if (http_write_headers(req) < 0) {
      iwlog_ecode_error3(JBR_ERROR_SEND_RESPONSE);
      return JBR_ERROR_SEND_RESPONSE;
    }
    rctx->data_sent = true;
  }
  if (!finish && (iwxstr_size(wbuf) < JBR_HTTP_CHUNK_SIZE)) {
    return 0;
  }
  intptr_t uuid = http_uuid(req);
  if (iwxstr_size(wbuf) > 0) {
    int sz = snprintf(nbuf, JBNUMBUF_SIZE, "%zX\r\n", iwxstr_size(wbuf));
    if (fio_write(uuid, nbuf, sz) < 0) {
      iwlog_ecode_error3(JBR_ERROR_SEND_RESPONSE);
      return JBR_ERROR_SEND_RESPONSE;
    }
    if (fio_write(uuid, iwxstr_ptr(wbuf), iwxstr_size(wbuf)) < 0) {
      iwlog_ecode_error3(JBR_ERROR_SEND_RESPONSE);
      return JBR_ERROR_SEND_RESPONSE;
    }
    if (fio_write(uuid, "\r\n", 2) < 0) {
      iwlog_ecode_error3(JBR_ERROR_SEND_RESPONSE);
      return JBR_ERROR_SEND_RESPONSE;
    }
    iwxstr_clear(wbuf);
  }
  if (finish) {
    if (fio_write(uuid, "0\r\n\r\n", 5) < 0) {
      iwlog_ecode_error3(JBR_ERROR_SEND_RESPONSE);
      return JBR_ERROR_SEND_RESPONSE;
    }
  }
  return 0;
}

static iwrc _jbr_query_visitor(EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  JBRCTX *rctx = ux->opaque;
  assert(rctx);
  iwrc rc = 0;
  IWXSTR *wbuf = rctx->wbuf;
  if (!wbuf) {
    wbuf = iwxstr_new2(512);
    if (!wbuf) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    rctx->wbuf = wbuf;
  }
  if (ux->log) {
    rc = iwxstr_cat(wbuf, iwxstr_ptr(ux->log), iwxstr_size(ux->log));
    RCRET(rc);
    rc = iwxstr_cat(wbuf, "--------------------", 20);
    RCRET(rc);
    iwxstr_destroy(ux->log);
    ux->log = 0;
  }
  rc = iwxstr_printf(wbuf, "\r\n%lld\t", doc->id);
  RCRET(rc);
  if (doc->node) {
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, wbuf, 0);
  } else {
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, wbuf, 0);
  }
  RCRET(rc);
  return _jbr_flush_chunk(rctx, false);
}

static void _jbr_on_query(JBRCTX *rctx) {
  http_s *req = rctx->req;
  fio_str_info_s data = fiobj_data_read(req->body, 0);
  if (data.len < 1) {
    _jbr_http_error_send(rctx->req, 400);
    return;
  }
  EJDB_EXEC ux = {
    .opaque  = rctx,
    .db      = rctx->jbr->db,
    .visitor = _jbr_query_visitor
  };

  // Collection name must be encoded in query
  iwrc rc = jql_create2(&ux.q, 0, data.data, JQL_SILENT_ON_PARSE_ERROR | JQL_KEEP_QUERY_ON_PARSE_ERROR);
  RCGO(rc, finish);
  if (rctx->read_anon && jql_has_apply(ux.q)) {
    // We have not permitted data modification request
    jql_destroy(&ux.q);
    _jbr_http_error_send(rctx->req, 403);
    return;
  }

  FIOBJ h = fiobj_hash_get2(req->headers, k_header_x_hints_hash);
  if (h) {
    if (!fiobj_type_is(h, FIOBJ_T_STRING)) {
      jql_destroy(&ux.q);
      _jbr_http_error_send(req, 400);
      return;
    }
    fio_str_info_s hv = fiobj_obj2cstr(h);
    if (strstr(hv.data, "explain")) {
      ux.log = iwxstr_new();
      if (!ux.log) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto finish;
      }
    }
  }

  rc = ejdb_exec(&ux);

  if (!rc && rctx->wbuf) {
    rc = iwxstr_cat(rctx->wbuf, "\r\n", 2);
    RCGO(rc, finish);
    rc = _jbr_flush_chunk(rctx, true);
  }

finish:
  if (rc) {
    iwrc rcs = rc;
    iwrc_strip_code(&rcs);
    switch (rcs) {
      case JQL_ERROR_QUERY_PARSE: {
        const char *err = jql_error(ux.q);
        _jbr_http_error_send2(rctx->req, 400, "text/plain", err, err ? (int) strlen(err) : 0);
        break;
      }
      case JQL_ERROR_NO_COLLECTION:
        JBR_RC_REPORT(400, req, rc);
        break;
      default:
        if (rctx->data_sent) {
          // We cannot report error over HTTP
          // because already sent some data to client
          iwlog_ecode_error3(rc);
          http_complete(req);
        } else {
          JBR_RC_REPORT(500, req, rc);
        }
        break;
    }
  } else if (rctx->data_sent) {
    http_complete(req);
  } else if (ux.log) {
    iwxstr_cat(ux.log, "--------------------", 20);
    if (jql_has_aggregate_count(ux.q)) {
      iwxstr_printf(ux.log, "\n%lld", ux.cnt);
    }
    _jbr_http_send(req, 200, "text/plain", iwxstr_ptr(ux.log), iwxstr_size(ux.log));
  } else {
    if (jql_has_aggregate_count(ux.q)) {
      char nbuf[JBNUMBUF_SIZE];
      snprintf(nbuf, sizeof(nbuf), "%" PRId64, ux.cnt);
      _jbr_http_send(req, 200, "text/plain", nbuf, (int) strlen(nbuf));
    } else {
      _jbr_http_send(req, 200, 0, 0, 0);
    }
  }
  if (ux.q) {
    jql_destroy(&ux.q);
  }
  if (ux.log) {
    iwxstr_destroy(ux.log);
  }
  if (rctx->wbuf) {
    iwxstr_destroy(rctx->wbuf);
    rctx->wbuf = 0;
  }
}

static void _jbr_on_patch(JBRCTX *rctx) {
  if (rctx->read_anon) {
    _jbr_http_error_send(rctx->req, 403);
    return;
  }
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  fio_str_info_s data = fiobj_data_read(req->body, 0);
  if (data.len < 1) {
    _jbr_http_error_send(rctx->req, 400);
    return;
  }
  iwrc rc = ejdb_patch(db, rctx->collection, data.data, rctx->id);
  iwrc_strip_code(&rc);
  switch (rc) {
    case IWKV_ERROR_NOTFOUND:
    case IW_ERROR_NOT_EXISTS:
      _jbr_http_error_send(req, 404);
      return;
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
    _jbr_http_send(req, 200, 0, 0, 0);
  }
}

static void _jbr_on_delete(JBRCTX *rctx) {
  if (rctx->read_anon) {
    _jbr_http_error_send(rctx->req, 403);
    return;
  }
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  iwrc rc = ejdb_del(db, rctx->collection, rctx->id);
  if ((rc == IWKV_ERROR_NOTFOUND) || (rc == IW_ERROR_NOT_EXISTS)) {
    _jbr_http_error_send(req, 404);
    return;
  } else if (rc) {
    JBR_RC_REPORT(500, req, rc);
    return;
  }
  _jbr_http_send(req, 200, 0, 0, 0);
}

static void _jbr_on_put(JBRCTX *rctx) {
  if (rctx->read_anon) {
    _jbr_http_error_send(rctx->req, 403);
    return;
  }
  JBL jbl;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  fio_str_info_s data = fiobj_data_read(req->body, 0);
  if (data.len < 1) {
    _jbr_http_error_send(rctx->req, 400);
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
  _jbr_http_send(req, 200, 0, 0, 0);

finish:
  jbl_destroy(&jbl);
}

static void _jbr_on_post(JBRCTX *rctx) {
  if (rctx->read_anon) {
    _jbr_http_error_send(rctx->req, 403);
    return;
  }
  JBL jbl;
  int64_t id;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  fio_str_info_s data = fiobj_data_read(req->body, 0);
  if (data.len < 1) {
    _jbr_http_error_send(rctx->req, 400);
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
  _jbr_http_send(req, 200, "text/plain", nbuf, len);

finish:
  jbl_destroy(&jbl);
}

static void _jbr_on_get(JBRCTX *rctx) {
  JBL jbl;
  IWXSTR *xstr = 0;
  int nbytes = 0;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;

  iwrc rc = ejdb_get(db, rctx->collection, rctx->id, &jbl);
  if ((rc == IWKV_ERROR_NOTFOUND) || (rc == IW_ERROR_NOT_EXISTS)) {
    _jbr_http_error_send(req, 404);
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
      goto finish;
    }
    rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY);
  }
  RCGO(rc, finish);

  _jbr_http_send(req, 200, "application/json",
                 xstr ? iwxstr_ptr(xstr) : 0,
                 xstr ? (int) iwxstr_size(xstr) : nbytes);
finish:
  if (rc) {
    JBR_RC_REPORT(500, req, rc);
  }
  jbl_destroy(&jbl);
  if (xstr) {
    iwxstr_destroy(xstr);
  }
}

static void _jbr_on_options(JBRCTX *rctx) {
  JBL jbl;
  EJDB db = rctx->jbr->db;
  http_s *req = rctx->req;
  const EJDB_HTTP *http = rctx->jbr->http;

  iwrc rc = ejdb_get_meta(db, &jbl);
  if (rc) {
    JBR_RC_REPORT(500, req, rc);
    return;
  }
  IWXSTR *xstr = iwxstr_new2(jbl->bn.size * 2);
  if (!xstr) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY);
  RCGO(rc, finish);

  if (http->read_anon) {
    _jbr_http_set_header(req, "Allow", 5, "GET, HEAD, POST, OPTIONS", 24);
  } else {
    _jbr_http_set_header(req, "Allow", 5, "GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS", 44);
  }

  if (http->cors) {
    _jbr_http_set_header(req, "Access-Control-Allow-Origin", 27, "*", 1);
    _jbr_http_set_header(req, "Access-Control-Allow-Headers", 28, "X-Requested-With, Content-Type, Accept, Origin, Authorization", 61);

    if (http->read_anon) {
      _jbr_http_set_header(req, "Access-Control-Allow-Methods", 28, "GET, HEAD, POST, OPTIONS", 24);
    } else {
      _jbr_http_set_header(req, "Access-Control-Allow-Methods", 28, "GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS", 44);
    }
  }

  _jbr_http_send(req, 200, "application/json",
                 iwxstr_ptr(xstr),
                 iwxstr_size(xstr));
finish:
  if (rc) {
    JBR_RC_REPORT(500, req, rc);
  }
  jbl_destroy(&jbl);
  if (xstr) {
    iwxstr_destroy(xstr);
  }
}

static bool _jbr_fill_ctx(http_s *req, JBRCTX *r) {
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
      break;
    case 7:
      if (!strncmp("OPTIONS", method.data, method.len)) {
        r->method = JBR_OPTIONS;
      }
      break;
  }
  if (!r->method) {
    // Unknown method
    return false;
  }
  fio_str_info_s path = fiobj_obj2cstr(req->path);
  if (!req->path || (path.len < 2)) {
    return true;
  } else if (r->method == JBR_OPTIONS) {
    return false;
  }

  char *c = strchr(path.data + 1, '/');
  if (!c) {
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
    nbuf[nlen] = '\0';
    r->id = strtoll(nbuf, &eptr, 10);
    if ((*eptr != '\0') || (r->id < 1) || (r->method == JBR_POST)) {
      return false;
    }
  }

finish:
  return (r->collection_len <= EJDB_COLLECTION_NAME_MAX_LEN);
}

static void _jbr_on_http_request(http_s *req) {
  JBRCTX rctx;
  JBR jbr = req->udata;
  assert(jbr);
  const EJDB_HTTP *http = jbr->http;
  char cname[EJDB_COLLECTION_NAME_MAX_LEN + 1];

  if (http->cors) {
    _jbr_http_set_header(req, "Access-Control-Allow-Origin", 27, "*", 1);
  }

  if (!_jbr_fill_ctx(req, &rctx)) {
    http_send_error(req, 400); // Bad request
    return;
  }
  if (http->access_token) {
    FIOBJ h = fiobj_hash_get2(req->headers, k_header_x_access_token_hash);
    if (!h) {
      if (http->read_anon) {
        if ((rctx.method == JBR_GET) || (rctx.method == JBR_HEAD) || ((rctx.method == JBR_POST) && !rctx.collection)) {
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
    if ((hv.len != http->access_token_len) || (memcmp(hv.data, http->access_token, http->access_token_len) != 0)) { // -V526
      http_send_error(req, 403);
      return;
    }
  }

process:
  if (rctx.collection) {
    // convert to `\0` terminated c-string
    memcpy(cname, rctx.collection, rctx.collection_len);
    cname[rctx.collection_len] = '\0';
    rctx.collection = cname;
    switch (rctx.method) {
      case JBR_GET:
      case JBR_HEAD:
        _jbr_on_get(&rctx);
        break;
      case JBR_POST:
        _jbr_on_post(&rctx);
        break;
      case JBR_PUT:
        _jbr_on_put(&rctx);
        break;
      case JBR_PATCH:
        _jbr_on_patch(&rctx);
        break;
      case JBR_DELETE:
        _jbr_on_delete(&rctx);
        break;
      default:
        http_send_error(req, 400);
        break;
    }
  } else if (rctx.method == JBR_POST) {
    _jbr_on_query(&rctx);
  } else if (rctx.method == JBR_OPTIONS) {
    _jbr_on_options(&rctx);
  } else {
    http_send_error(req, 400);
  }
}

static void _jbr_on_http_finish(struct http_settings_s *settings) {
}

static void _jbr_on_pre_start(void *op) {
  JBR jbr = op;
  if (!jbr->http->blocking) {
    pthread_barrier_wait(&jbr->start_barrier);
  }
}

//------------------ WS ---------------------


#define _WS_KEYPREFIX_BUFSZ (JBNUMBUF_SIZE + JBR_MAX_KEY_LEN + 2)

typedef enum {
  JBWS_NONE,
  JBWS_SET,
  JBWS_GET,
  JBWS_ADD,
  JBWS_DEL,
  JBWS_PATCH,
  JBWS_QUERY,
  JBWS_EXPLAIN,
  JBWS_INFO,
  JBWS_IDX,
  JBWS_NIDX,
  JBWS_REMOVE_COLL,
} jbwsop_t;

typedef struct _JBWCTX {
  bool  read_anon;
  EJDB  db;
  ws_s *ws;
} JBWCTX;

IW_INLINE bool _jbr_ws_write_text(ws_s *ws, const char *data, int len) {
  if (fio_is_closed(websocket_uuid(ws)) || (websocket_write(ws, (fio_str_info_s) {
    .data = (char*) data, .len = len
  }, 1) < 0)) {
    iwlog_warn2("Websocket channel closed");
    return false;
  }
  return true;
}

IW_INLINE int _jbr_fill_prefix_buf(const char *key, int64_t id, char buf[static _WS_KEYPREFIX_BUFSZ]) {
  int len = (int) strlen(key);
  char *wp = buf;
  memcpy(wp, key, len); // NOLINT(bugprone-not-null-terminated-result)
  wp += len;
  *wp++ = '\t';
  wp += iwitoa(id, wp, _WS_KEYPREFIX_BUFSZ - (wp - buf));
  return (int) (wp - buf);
}

static void _jbr_ws_on_open(ws_s *ws) {
  JBWCTX *wctx = websocket_udata_get(ws);
  if (wctx) {
    wctx->ws = ws;
  }
}

static void _jbr_ws_on_close(intptr_t uuid, void *udata) {
  JBWCTX *wctx = udata;
  free(wctx);
}

static void _jbr_ws_send_error(JBWCTX *wctx, const char *key, const char *error, const char *extra) {
  assert(wctx && key && error);
  IWXSTR *xstr = iwxstr_new();
  if (!xstr) {
    iwlog_ecode_error3(iwrc_set_errno(IW_ERROR_ALLOC, errno));
    return;
  }
  iwrc rc;
  if (extra) {
    rc = iwxstr_printf(xstr, "%s ERROR: %s %s", key, error, extra);
  } else {
    rc = iwxstr_printf(xstr, "%s ERROR: %s", key, error);
  }
  if (rc) {
    iwlog_ecode_error3(rc);
  } else {
    _jbr_ws_write_text(wctx->ws, iwxstr_ptr(xstr), iwxstr_size(xstr));
  }
  iwxstr_destroy(xstr);
}

static void _jbr_ws_send_rc(JBWCTX *wctx, const char *key, iwrc rc, const char *extra) {
  const char *error = iwlog_ecode_explained(rc);
  if (error) {
    _jbr_ws_send_error(wctx, key, error, extra);
  }
}

static void _jbr_ws_add_document(JBWCTX *wctx, const char *key, const char *coll, const char *json) {
  if (wctx->read_anon) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    return;
  }
  JBL jbl;
  int64_t id;
  iwrc rc = jbl_from_json(&jbl, json);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    return;
  }
  rc = ejdb_put_new(wctx->db, coll, jbl, &id);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    goto finish;
  }
  char pbuf[_WS_KEYPREFIX_BUFSZ];
  _jbr_fill_prefix_buf(key, id, pbuf);
  _jbr_ws_write_text(wctx->ws, pbuf, (int) strlen(pbuf));

finish:
  jbl_destroy(&jbl);
}

static void _jbr_ws_set_document(JBWCTX *wctx, const char *key, const char *coll, int64_t id, const char *json) {
  if (wctx->read_anon) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    return;
  }
  JBL jbl;
  iwrc rc = jbl_from_json(&jbl, json);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    return;
  }
  rc = ejdb_put(wctx->db, coll, jbl, id);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    goto finish;
  }
  char pbuf[_WS_KEYPREFIX_BUFSZ];
  int len = _jbr_fill_prefix_buf(key, id, pbuf);
  _jbr_ws_write_text(wctx->ws, pbuf, len);

finish:
  jbl_destroy(&jbl);
}

static void _jbr_ws_get_document(JBWCTX *wctx, const char *key, const char *coll, int64_t id) {
  JBL jbl;
  iwrc rc = ejdb_get(wctx->db, coll, id, &jbl);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    return;
  }
  IWXSTR *xstr = iwxstr_new2(jbl->bn.size * 2);
  if (!xstr) {
    rc = iwrc_set_errno(rc, IW_ERROR_ALLOC);
    iwlog_ecode_error3(rc);
    _jbr_ws_send_rc(wctx, key, rc, 0);
    return;
  }
  char pbuf[_WS_KEYPREFIX_BUFSZ];
  _jbr_fill_prefix_buf(key, id, pbuf);
  rc = iwxstr_printf(xstr, "%s\t", pbuf);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    goto finish;
  }
  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    goto finish;
  }
  _jbr_ws_write_text(wctx->ws, iwxstr_ptr(xstr), iwxstr_size(xstr));

finish:
  iwxstr_destroy(xstr);
  jbl_destroy(&jbl);
}

static void _jbr_ws_del_document(JBWCTX *wctx, const char *key, const char *coll, int64_t id) {
  iwrc rc = ejdb_del(wctx->db, coll, id);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    return;
  }
  char pbuf[_WS_KEYPREFIX_BUFSZ];
  int len = _jbr_fill_prefix_buf(key, id, pbuf);
  _jbr_ws_write_text(wctx->ws, pbuf, len);
}

static void _jbr_ws_patch_document(JBWCTX *wctx, const char *key, const char *coll, int64_t id, const char *json) {
  if (wctx->read_anon) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    return;
  }
  iwrc rc = ejdb_patch(wctx->db, coll, json, id);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    return;
  }
  char pbuf[_WS_KEYPREFIX_BUFSZ];
  int len = _jbr_fill_prefix_buf(key, id, pbuf);
  _jbr_ws_write_text(wctx->ws, pbuf, len);
}

static void _jbr_ws_set_index(JBWCTX *wctx, const char *key, const char *coll, int64_t mode, const char *path) {
  if (wctx->read_anon) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    return;
  }
  iwrc rc = ejdb_ensure_index(wctx->db, coll, path, mode);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
  } else {
    _jbr_ws_write_text(wctx->ws, key, (int) strlen(key));
  }
}

static void _jbr_ws_del_index(JBWCTX *wctx, const char *key, const char *coll, int64_t mode, const char *path) {
  if (wctx->read_anon) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    return;
  }
  iwrc rc = ejdb_remove_index(wctx->db, coll, path, mode);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
  } else {
    _jbr_ws_write_text(wctx->ws, key, (int) strlen(key));
  }
}

typedef struct JBWQCTX {
  JBWCTX     *wctx;
  IWXSTR     *wbuf;
  const char *key;
} JBWQCTX;

static iwrc _jbr_ws_query_visitor(EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  iwrc rc = 0;
  JBWQCTX *qctx = ux->opaque;
  assert(qctx);
  IWXSTR *wbuf = qctx->wbuf;
  if (!wbuf) {
    wbuf = iwxstr_new2(512);
    if (!wbuf) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    qctx->wbuf = wbuf;
  } else {
    iwxstr_clear(wbuf);
  }
  if (ux->log) {
    rc = iwxstr_printf(wbuf, "%s\texplain\t%s", qctx->key, iwxstr_ptr(ux->log));
    iwxstr_destroy(ux->log);
    ux->log = 0;
    RCRET(rc);
    _jbr_ws_write_text(qctx->wctx->ws, iwxstr_ptr(wbuf), iwxstr_size(wbuf));
    iwxstr_clear(wbuf);
  }

  rc = iwxstr_printf(wbuf, "%s\t%lld\t", qctx->key, doc->id);
  RCRET(rc);

  if (doc->node) {
    rc = jbn_as_json(doc->node, jbl_xstr_json_printer, wbuf, 0);
  } else {
    rc = jbl_as_json(doc->raw, jbl_xstr_json_printer, wbuf, 0);
  }
  RCRET(rc);
  if (!_jbr_ws_write_text(qctx->wctx->ws, iwxstr_ptr(wbuf), iwxstr_size(wbuf))) {
    *step = 0;
  }
  return 0;
}

static void _jbr_ws_query(JBWCTX *wctx, const char *key, const char *coll, const char *query, bool explain) {
  JBWQCTX qctx = {
    .wctx = wctx,
    .key  = key
  };
  EJDB_EXEC ux = {
    .db      = wctx->db,
    .opaque  = &qctx,
    .visitor = _jbr_ws_query_visitor,
  };

  iwrc rc = jql_create2(&ux.q, coll, query, JQL_SILENT_ON_PARSE_ERROR | JQL_KEEP_QUERY_ON_PARSE_ERROR);
  RCGO(rc, finish);

  if (wctx->read_anon && jql_has_apply(ux.q)) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    goto finish;
  }

  if (explain) {
    ux.log = iwxstr_new();
    if (!ux.log) {
      iwlog_ecode_error3(iwrc_set_errno(IW_ERROR_ALLOC, errno));
      goto finish;
    }
  }

  rc = ejdb_exec(&ux);

  if (!rc) {
    if (ux.log) {
      IWXSTR *wbuf = iwxstr_new();
      if (!wbuf) {
        rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        goto finish;
      }
      if (!iwxstr_printf(wbuf, "%s\texplain\t%s", qctx.key, iwxstr_ptr(ux.log))) {
        _jbr_ws_write_text(wctx->ws, iwxstr_ptr(wbuf), iwxstr_size(wbuf));
      }
      iwxstr_destroy(wbuf);
    }
  }

finish:
  if (rc) {
    iwrc rcs = rc;
    iwrc_strip_code(&rcs);
    switch (rcs) {
      case JQL_ERROR_QUERY_PARSE:
        _jbr_ws_send_error(wctx, key, jql_error(ux.q), 0);
        break;
      default:
        _jbr_ws_send_rc(wctx, key, rc, 0);
        break;
    }
  } else {
    if (jql_has_aggregate_count(ux.q)) {
      char pbuf[_WS_KEYPREFIX_BUFSZ];
      _jbr_fill_prefix_buf(key, ux.cnt, pbuf);
      _jbr_ws_write_text(wctx->ws, pbuf, (int) strlen(pbuf));
    }
    _jbr_ws_write_text(wctx->ws, key, (int) strlen(key));
  }
  if (ux.q) {
    jql_destroy(&ux.q);
  }
  if (ux.log) {
    iwxstr_destroy(ux.log);
  }
  if (qctx.wbuf) {
    iwxstr_destroy(qctx.wbuf);
  }
}

static void _jbr_ws_info(JBWCTX *wctx, const char *key) {
  if (wctx->read_anon) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    return;
  }
  JBL jbl;
  iwrc rc = ejdb_get_meta(wctx->db, &jbl);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
    return;
  }
  IWXSTR *xstr = iwxstr_new2(jbl->bn.size * 2);
  if (!xstr) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    RCGO(rc, finish);
  }
  rc = iwxstr_printf(xstr, "%s\t", key);
  RCGO(rc, finish);

  rc = jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY);
  RCGO(rc, finish);
  _jbr_ws_write_text(wctx->ws, iwxstr_ptr(xstr), iwxstr_size(xstr));

finish:
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
  }
  jbl_destroy(&jbl);
  if (xstr) {
    iwxstr_destroy(xstr);
  }
}

static void _jbr_ws_remove_coll(JBWCTX *wctx, const char *key, const char *coll) {
  if (wctx->read_anon) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_ACCESS_DENIED, 0);
    return;
  }
  iwrc rc = ejdb_remove_collection(wctx->db, coll);
  if (rc) {
    _jbr_ws_send_rc(wctx, key, rc, 0);
  } else {
    _jbr_ws_write_text(wctx->ws, key, (int) strlen(key));
  }
}

static void _jbr_ws_on_message(ws_s *ws, fio_str_info_s msg, uint8_t is_text) {
  if (!is_text) { // Do not serve binary requests
    websocket_close(ws);
    return;
  }
  if (!msg.data || (msg.len < 1)) { // Ignore empty messages, but keep connection
    return;
  }
  JBWCTX *wctx = websocket_udata_get(ws);
  assert(wctx);
  wctx->ws = ws;
  jbwsop_t wsop = JBWS_NONE;

  char keybuf[JBR_MAX_KEY_LEN + 1];
  char cnamebuf[EJDB_COLLECTION_NAME_MAX_LEN + 1];

  char *data = msg.data, *key = 0;
  int len = msg.len, pos;

  // Trim right
  for (pos = len; pos > 0 && isspace(data[pos - 1]); --pos) ;
  len = pos;
  // Trim left
  for (pos = 0; pos < len && isspace(data[pos]); ++pos) ;
  len -= pos;
  data += pos;
  if (len < 1) {
    return;
  }
  if ((len == 1) && (data[0] == '?')) {
    const char *help
      = "\n<key> info"
        "\n<key> get     <collection> <id>"
        "\n<key> set     <collection> <id> <document json>"
        "\n<key> add     <collection> <document json>"
        "\n<key> del     <collection> <id>"
        "\n<key> patch   <collection> <id> <patch json>"
        "\n<key> idx     <collection> <mode> <path>"
        "\n<key> rmi     <collection> <mode> <path>"
        "\n<key> rmc     <collection>"
        "\n<key> query   <collection> <query>"
        "\n<key> explain <collection> <query>"
        "\n<key> <query>"
        "\n";
    _jbr_ws_write_text(ws, help, (int) strlen(help));
    return;
  }

  // Fetch key, after we can do good errors reporting
  for (pos = 0; pos < len && !isspace(data[pos]); ++pos) ;
  if (pos > JBR_MAX_KEY_LEN) {
    iwlog_warn("The key length: %d exceeded limit: %d", pos, JBR_MAX_KEY_LEN);
    return;
  }
  memcpy(keybuf, data, pos);
  keybuf[pos] = '\0';
  key = keybuf;
  if (pos >= len) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
    return;
  }

  // Space
  for ( ; pos < len && isspace(data[pos]); ++pos) ;
  len -= pos;
  data += pos;
  if (len < 1) {
    _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
    return;
  }

  // Fetch command
  for (pos = 0; pos < len && !isspace(data[pos]); ++pos) ;

  if (pos <= len) {
    if (!strncmp("get", data, pos)) {
      wsop = JBWS_GET;
    } else if (!strncmp("add", data, pos)) {
      wsop = JBWS_ADD;
    } else if (!strncmp("set", data, pos)) {
      wsop = JBWS_SET;
    } else if (!strncmp("query", data, pos)) {
      wsop = JBWS_QUERY;
    } else if (!strncmp("del", data, pos)) {
      wsop = JBWS_DEL;
    } else if (!strncmp("patch", data, pos)) {
      wsop = JBWS_PATCH;
    } else if (!strncmp("explain", data, pos)) {
      wsop = JBWS_EXPLAIN;
    } else if (!strncmp("info", data, pos)) {
      wsop = JBWS_INFO;
    } else if (!strncmp("idx", data, pos)) {
      wsop = JBWS_IDX;
    } else if (!strncmp("rmi", data, pos)) {
      wsop = JBWS_NIDX;
    } else if (!strncmp("rmc", data, pos)) {
      wsop = JBWS_REMOVE_COLL;
    }
  }

  if (wsop > JBWS_NONE) {
    if (wsop == JBWS_INFO) {
      _jbr_ws_info(wctx, key);
      return;
    }
    for ( ; pos < len && isspace(data[pos]); ++pos) ;
    len -= pos;
    data += pos;

    char *coll = data;
    for (pos = 0; pos < len && !isspace(data[pos]); ++pos) ;
    len -= pos;
    data += pos;

    if ((pos < 1) || (len < 1)) {
      if (wsop != JBWS_REMOVE_COLL) {
        _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
        return;
      }
    } else if (pos > EJDB_COLLECTION_NAME_MAX_LEN) {
      _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_INVALID_MESSAGE,
                      "Collection name exceeds maximum length allowed: "
                      "EJDB_COLLECTION_NAME_MAX_LEN");
      return;
    }
    memcpy(cnamebuf, coll, pos);
    cnamebuf[pos] = '\0';
    coll = cnamebuf;

    if (wsop == JBWS_REMOVE_COLL) {
      _jbr_ws_remove_coll(wctx, key, coll);
      return;
    }

    for (pos = 0; pos < len && isspace(data[pos]); ++pos) ;
    len -= pos;
    data += pos;
    if (len < 1) {
      _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
      return;
    }

    switch (wsop) {
      case JBWS_ADD:
        data[len] = '\0';
        _jbr_ws_add_document(wctx, key, coll, data);
        break;
      case JBWS_QUERY:
      case JBWS_EXPLAIN:
        data[len] = '\0';
        _jbr_ws_query(wctx, key, coll, data, (wsop == JBWS_EXPLAIN));
        break;
      default: {
        char nbuf[JBNUMBUF_SIZE];
        for (pos = 0; pos < len && pos < JBNUMBUF_SIZE - 1 && isdigit(data[pos]); ++pos) {
          nbuf[pos] = data[pos];
        }
        nbuf[pos] = '\0';
        for ( ; pos < len && isspace(data[pos]); ++pos) ;
        len -= pos;
        data += pos;

        int64_t id = iwatoi(nbuf);
        if (id < 1) {
          _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_INVALID_MESSAGE, "Invalid document id specified");
          return;
        }
        switch (wsop) {
          case JBWS_GET:
            _jbr_ws_get_document(wctx, key, coll, id);
            break;
          case JBWS_SET:
            data[len] = '\0';
            _jbr_ws_set_document(wctx, key, coll, id, data);
            break;
          case JBWS_DEL:
            _jbr_ws_del_document(wctx, key, coll, id);
            break;
          case JBWS_PATCH:
            data[len] = '\0';
            _jbr_ws_patch_document(wctx, key, coll, id, data);
            break;
          case JBWS_IDX:
            data[len] = '\0';
            _jbr_ws_set_index(wctx, key, coll, id, data);
            break;
          case JBWS_NIDX:
            data[len] = '\0';
            _jbr_ws_del_index(wctx, key, coll, id, data);
            break;
          default:
            _jbr_ws_send_rc(wctx, key, JBR_ERROR_WS_INVALID_MESSAGE, 0);
            return;
        }
      }
    }
  } else {
    data[len] = '\0';
    _jbr_ws_query(wctx, key, 0, data, false);
  }
}

static void _jbr_on_http_upgrade(http_s *req, char *requested_protocol, size_t len) {
  JBR jbr = req->udata;
  assert(jbr);
  const EJDB_HTTP *http = jbr->http;
  fio_str_info_s path = fiobj_obj2cstr(req->path);

  if (  ((path.len != 1) || (path.data[0] != '/'))
     || ((len != 9) || (requested_protocol[1] != 'e'))) {
    http_send_error(req, 400);
    return;
  }
  JBWCTX *wctx = calloc(1, sizeof(*wctx));
  if (!wctx) {
    http_send_error(req, 500);
    return;
  }
  wctx->db = jbr->db;

  if (http->access_token) {
    FIOBJ h = fiobj_hash_get2(req->headers, k_header_x_access_token_hash);
    if (!h) {
      if (http->read_anon) {
        wctx->read_anon = true;
      } else {
        free(wctx);
        http_send_error(req, 401);
        return;
      }
    }
    if (!fiobj_type_is(h, FIOBJ_T_STRING)) { // header specified more than once
      free(wctx);
      http_send_error(req, 400);
      return;
    }
    fio_str_info_s hv = fiobj_obj2cstr(h);
    if ((hv.len != http->access_token_len) || (memcmp(hv.data, http->access_token, http->access_token_len) != 0)) { // -V526
      free(wctx);
      http_send_error(req, 403);
      return;
    }
  }
  if (http_upgrade2ws(req,
                      .on_message = _jbr_ws_on_message,
                      .on_open = _jbr_ws_on_open,
                      .on_close = _jbr_ws_on_close,
                      .udata = wctx) < 0) {
    free(wctx);
    JBR_RC_REPORT(500, req, JBR_ERROR_WS_UPGRADE);
  }
}

//---------------- Main ---------------------

static void *_jbr_start_thread(void *op) {
  JBR jbr = op;
  char nbuf[JBNUMBUF_SIZE];
  const EJDB_HTTP *http = jbr->http;
  const char *bind = http->bind ? http->bind : "localhost";
  if (http->port < 1) {
    jbr->rc = JBR_ERROR_PORT_INVALID;
    if (!jbr->http->blocking) {
      pthread_barrier_wait(&jbr->start_barrier);
    }
    return 0;
  }
  iwitoa(http->port, nbuf, sizeof(nbuf));
  iwlog_info("HTTP/WS endpoint at %s:%s", bind, nbuf);
  websocket_optimize4broadcasts(WEBSOCKET_OPTIMIZE_PUBSUB_TEXT, 1);
  if (http_listen(nbuf, bind,
                  .udata = jbr,
                  .on_request = _jbr_on_http_request,
                  .on_upgrade = _jbr_on_http_upgrade,
                  .on_finish = _jbr_on_http_finish,
                  .max_body_size = http->max_body_size,
                  .ws_max_msg_size = http->max_body_size) == -1) {
    jbr->rc = iwrc_set_errno(JBR_ERROR_HTTP_LISTEN, errno);
  }
  if (jbr->rc) {
    if (!jbr->http->blocking) {
      pthread_barrier_wait(&jbr->start_barrier);
    }
    return 0;
  }
  fio_state_callback_add(FIO_CALL_PRE_START, _jbr_on_pre_start, jbr);
  fio_start(.threads = -2, .workers = 1, .is_no_signal_handlers = !jbr->http->blocking); // Will block current thread
                                                                                         // here
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
  if (!jbr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
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
    fio_state_callback_remove(FIO_CALL_PRE_START, _jbr_on_pre_start, jbr);
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
  if (!((ecode > _JBR_ERROR_START) && (ecode < _JBR_ERROR_END))) {
    return 0;
  }
  switch (ecode) {
    case JBR_ERROR_HTTP_LISTEN:
      return "Failed to start HTTP network listener (JBR_ERROR_HTTP_LISTEN)";
    case JBR_ERROR_PORT_INVALID:
      return "Invalid port specified (JBR_ERROR_PORT_INVALID)";
    case JBR_ERROR_SEND_RESPONSE:
      return "Error sending response (JBR_ERROR_SEND_RESPONSE)";
    case JBR_ERROR_WS_UPGRADE:
      return "Failed upgrading to websocket connection (JBR_ERROR_WS_UPGRADE)";
    case JBR_ERROR_WS_INVALID_MESSAGE:
      return "Invalid message recieved (JBR_ERROR_WS_INVALID_MESSAGE)";
    case JBR_ERROR_WS_ACCESS_DENIED:
      return "Access denied (JBR_ERROR_WS_ACCESS_DENIED)";
  }
  return 0;
}

iwrc jbr_init() {
  static int _jbr_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbr_initialized, 0, 1)) {
    return 0;
  }
  k_header_x_access_token_hash = fiobj_hash_string("x-access-token", 14);
  k_header_x_hints_hash = fiobj_hash_string("x-hints", 7);
  k_header_content_length_hash = fiobj_hash_string("content-length", 14);
  k_header_content_type_hash = fiobj_hash_string("content-type", 12);
  return iwlog_register_ecodefn(_jbr_ecodefn);
}
