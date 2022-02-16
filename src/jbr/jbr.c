#include "jbr.h"
#include "ejdb2_internal.h"

#include <iwnet/ws_server.h>
#include <iwnet/pairs.h>
#include <iowow/iwconv.h>

#include <pthread.h>
#include <ctype.h>

#define JBR_MAX_KEY_LEN          36
#define JBR_WS_STR_PREMATURE_END "Premature end of message"

typedef enum {
  _JBR_ERROR_START = (IW_ERROR_START + 15000UL + 3000),
  JBR_ERROR_WS_INVALID_MESSAGE, /**< Invalid message recieved (JBR_ERROR_WS_INVALID_MESSAGE) */
  JBR_ERROR_WS_ACCESS_DENIED,   /**< Access denied (JBR_ERROR_WS_ACCESS_DENIED) */
  _JBR_ERROR_END,
} jbr_ecode_t;

struct jbr {
  struct iwn_poller *poller;
  pthread_t poller_thread;
  const EJDB_HTTP   *http;
  struct iwn_wf_ctx *ctx;
  EJDB db;
};

struct rctx {
  struct iwn_wf_req  *req;
  struct iwn_ws_sess *ws;
  struct jbr     *jbr;
  struct iwn_vals vals;
  pthread_mutex_t mtx;
  pthread_cond_t  cond;
  EJDB_EXEC       ux;
  int64_t   id;
  pthread_t request_thread;
  bool      read_anon;
  bool      visitor_started;
  bool      visitor_finished;
  char      cname[EJDB_COLLECTION_NAME_MAX_LEN + 1];
};

struct mctx {
  struct rctx *ctx;
  IWXSTR      *wbuf;
  int64_t      id;
  char cname[EJDB_COLLECTION_NAME_MAX_LEN + 1];
  char key[JBR_MAX_KEY_LEN + 1];
};

#define JBR_RC_REPORT(ret_, r_, rc_)                                 \
  do {                                                                \
    if ((ret_) >= 500) iwlog_ecode_error3(rc_);                      \
    const char *err = iwlog_ecode_explained(rc_);                     \
    if (!iwn_http_response_write(r_, ret_, "text/plain", err, -1)) { \
      ret_ = -1;                                                     \
    } else {                                                          \
      ret_ = 1;                                                      \
    }                                                                 \
  } while (0)

void jbr_shutdown_request(EJDB db) {
  if (db->jbr) {
    iwn_poller_shutdown_request(db->jbr->poller);
  }
}

void jbr_shutdown_wait(struct jbr *jbr) {
  if (jbr) {
    pthread_t t = jbr->poller_thread;
    iwn_poller_shutdown_request(jbr->poller);
    if (t && t != pthread_self()) {
      jbr->poller_thread = 0;
      pthread_join(t, 0);
    }
    free(jbr);
  }
}

static void* _poller_worker(void *op) {
  JBR jbr = op;
  iwn_poller_poll(jbr->poller);
  iwn_poller_destroy(&jbr->poller);
  return 0;
}

static void _rctx_dispose(struct rctx *ctx) {
  if (ctx) {
    for (struct iwn_val *v = ctx->vals.first; v; ) {
      struct iwn_val *n = v->next;
      free(v->buf);
      free(v);
      v = n;
    }
    pthread_mutex_destroy(&ctx->mtx);
    pthread_cond_destroy(&ctx->cond);
    free(ctx);
  }
}

static void _on_http_request_dispose(struct iwn_http_req *req) {
  struct rctx *ctx = req->user_data;
  _rctx_dispose(ctx);
}

static int _on_get(struct rctx *ctx) {
  JBL jbl = 0;
  IWXSTR *xstr = 0;
  int nbytes = 0, ret = 500;

  iwrc rc = ejdb_get(ctx->jbr->db, ctx->cname, ctx->id, &jbl);
  if (rc) {
    if ((rc == IWKV_ERROR_NOTFOUND) || (rc == IW_ERROR_NOT_EXISTS)) {
      return 404;
    }
    goto finish;
  }

  if (ctx->req->flags & IWN_WF_HEAD) {
    RCC(rc, finish, jbl_as_json(jbl, jbl_count_json_printer, &nbytes, JBL_PRINT_PRETTY));
    RCC(rc, finish, iwn_http_response_header_i64_set(ctx->req->http, "content-length", nbytes));
  } else {
    RCA(xstr = iwxstr_new2(jbl->bn.size * 2), finish);
    RCC(rc, finish, jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY));
    nbytes = iwxstr_size(xstr);
  }

  ret = iwn_http_response_write(ctx->req->http, 200, "application/json",
                                xstr ? iwxstr_ptr(xstr) : 0, xstr ? nbytes : 0) ? 1 : -1;

finish:
  if (rc) {
    JBR_RC_REPORT(ret, ctx->req->http, rc);
  }
  jbl_destroy(&jbl);
  iwxstr_destroy(xstr);
  return ret;
}

static int _on_post(struct rctx *ctx) {
  if (ctx->read_anon) {
    return 403;
  }
  if (ctx->req->body_len < 1) {
    return 400;
  }

  JBL jbl;
  int64_t id;
  int ret = 500;
  iwrc rc = jbl_from_json(&jbl, ctx->req->body);
  if (rc) {
    ret = 400;
    goto finish;
  }
  RCC(rc, finish, ejdb_put_new(ctx->jbr->db, ctx->cname, jbl, &id));
  ret = iwn_http_response_printf(ctx->req->http, 200, "text/plain", "%" PRId64, id) ? 1 : -1;

finish:
  if (rc) {
    JBR_RC_REPORT(ret, ctx->req->http, rc);
  }
  jbl_destroy(&jbl);
  return ret;
}

static int _on_put(struct rctx *ctx) {
  if (ctx->read_anon) {
    return 403;
  }
  if (ctx->req->body_len < 1) {
    return 400;
  }

  JBL jbl;
  int ret = 500;
  iwrc rc = jbl_from_json(&jbl, ctx->req->body);
  if (rc) {
    ret = 400;
    goto finish;
  }
  RCC(rc, finish, ejdb_put(ctx->jbr->db, ctx->cname, jbl, ctx->id));
  ret = 200;

finish:
  if (rc) {
    JBR_RC_REPORT(ret, ctx->req->http, rc);
  }
  jbl_destroy(&jbl);
  return ret;
}

static int _on_patch(struct rctx *ctx) {
  if (ctx->read_anon) {
    return 403;
  }
  if (ctx->req->body_len < 1) {
    return 400;
  }
  int ret = 200;
  iwrc rc = ejdb_patch(ctx->jbr->db, ctx->cname, ctx->req->body, ctx->id);
  if (rc) {
    iwrc_strip_code(&rc);
    switch (rc) {
      case IWKV_ERROR_NOTFOUND:
      case IW_ERROR_NOT_EXISTS:
        rc = 0;
        ret = 404;
        break;
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
        ret = 400;
        break;
      default:
        ret = 500;
        break;
    }
  }
  if (rc) {
    JBR_RC_REPORT(ret, ctx->req->http, rc);
  }
  return ret;
}

static int _on_delete(struct rctx *ctx) {
  if (ctx->read_anon) {
    return 403;
  }
  iwrc rc = ejdb_del(ctx->jbr->db, ctx->cname, ctx->id);
  if (rc) {
    if (rc == IWKV_ERROR_NOTFOUND || rc == IW_ERROR_NOT_EXISTS) {
      return 404;
    } else {
      int ret = 500;
      JBR_RC_REPORT(ret, ctx->req->http, rc);
      return ret;
    }
  }
  return 200;
}

static int _on_options(struct rctx *ctx) {
  iwrc rc;
  JBL jbl = 0;
  IWXSTR *xstr = 0;
  int ret = 500;

  RCC(rc, finish, ejdb_get_meta(ctx->jbr->db, &jbl));
  RCA(xstr = iwxstr_new2(jbl->bn.size * 2), finish);
  RCC(rc, finish, jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY));

  if (ctx->jbr->http->read_anon) {
    RCC(rc, finish, iwn_http_response_header_add(ctx->req->http, "Allow", "GET, HEAD, POST, OPTIONS", 24));
  } else {
    RCC(rc, finish, iwn_http_response_header_add(ctx->req->http, "Allow",
                                                 "GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS", 44));
  }

  if (ctx->jbr->http->cors) {
    RCC(rc, finish, iwn_http_response_header_add(ctx->req->http, "Access-Control-Allow-Origin", "*", 1));
    RCC(rc, finish, iwn_http_response_header_add(ctx->req->http, "Access-Control-Allow-Headers",
                                                 "X-Requested-With, Content-Type, Accept, Origin, Authorization", 61));

    if (ctx->jbr->http->read_anon) {
      RCC(rc, finish, iwn_http_response_header_add(ctx->req->http, "Access-Control-Allow-Methods",
                                                   "GET, HEAD, POST, OPTIONS", 24));
    } else {
      RCC(rc, finish, iwn_http_response_header_add(ctx->req->http, "Access-Control-Allow-Methods",
                                                   "GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS", 44));
    }
  }

  ret = iwn_http_response_write(ctx->req->http, 200, "application/json", iwxstr_ptr(xstr), iwxstr_size(xstr)) ? 1 : -1;

finish:
  if (rc) {
    JBR_RC_REPORT(ret, ctx->req->http, rc);
  }
  jbl_destroy(&jbl);
  iwxstr_destroy(xstr);
  return ret;
}

static bool _query_chunk_write_next(struct iwn_http_req *req, bool *again) {
  iwrc rc = 0;
  struct iwn_val *val;
  struct rctx *ctx = req->user_data;

  if (ctx->request_thread == pthread_self()) {
    return iwn_poller_arm_events(req->poller_adapter->poller, req->poller_adapter->fd, IWN_POLLOUT) == 0;
  }

  pthread_mutex_lock(&ctx->mtx);
start:
  val = ctx->vals.first;
  if (val) {
    ctx->vals.first = val->next;
    if (ctx->vals.last == val) {
      ctx->vals.last = 0;
    }
  } else if (!ctx->visitor_finished) {
    pthread_cond_wait(&ctx->cond, &ctx->mtx);
    goto start;
  }
  pthread_mutex_unlock(&ctx->mtx);

  if (val) {
    rc = iwn_http_response_chunk_write(req, val->buf, val->len, _query_chunk_write_next, again);
    free(val->buf);
    free(val);
  } else {
    rc = iwn_http_response_chunk_end(req);
  }

  return rc == 0;
}

static iwrc _query_visitor(EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  iwrc rc = 0;
  struct rctx *ctx = ux->opaque;
  IWXSTR *xstr = iwxstr_new();
  RCA(xstr, finish);

  if (ux->log) {
    RCC(rc, finish, iwxstr_cat(xstr, iwxstr_ptr(ux->log), iwxstr_size(ux->log)));
    RCC(rc, finish, iwxstr_cat(xstr, "--------------------", 20));
    iwxstr_destroy(ux->log);
    ux->log = 0;
  }
  RCC(rc, finish, iwxstr_printf(xstr, "\r\n%" PRId64 "\t", doc->id));
  if (doc->node) {
    RCC(rc, finish, jbn_as_json(doc->node, jbl_xstr_json_printer, xstr, 0));
  } else {
    RCC(rc, finish, jbl_as_json(doc->raw, jbl_xstr_json_printer, xstr, 0));
  }

  if (ctx->visitor_started) {
    pthread_mutex_lock(&ctx->mtx);
    iwn_val_add_new(&ctx->vals, iwxstr_ptr(xstr), iwxstr_size(xstr));
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->mtx);
    iwxstr_destroy_keep_ptr(xstr);
  } else {
    ctx->visitor_started = true;
    RCC(rc, finish, iwn_http_response_chunk_write(
          ctx->req->http, iwxstr_ptr(xstr), iwxstr_size(xstr), _query_chunk_write_next, 0));
    iwxstr_destroy(xstr);
  }

  xstr = 0;

finish:
  iwxstr_destroy(xstr);
  return rc;
}

static int _on_query(struct rctx *ctx) {
  if (ctx->req->body_len < 1) {
    return 400;
  }
  iwrc rc = 0;
  int ret = 500;

  ctx->ux.opaque = ctx;
  ctx->ux.db = ctx->jbr->db;
  ctx->ux.visitor = _query_visitor;

  RCC(rc, finish,
      jql_create2(&ctx->ux.q, 0, ctx->req->body, JQL_SILENT_ON_PARSE_ERROR | JQL_KEEP_QUERY_ON_PARSE_ERROR));

  if (ctx->read_anon && jql_has_apply(ctx->ux.q)) {
    jql_destroy(&ctx->ux.q);
    return 403;
  }

  struct iwn_val val = iwn_http_request_header_get(ctx->req->http, "x-hints", IW_LLEN("x-hints"));
  if (val.len) {
    char buf[val.len + 1];
    memcpy(buf, val.buf, val.len);
    buf[val.len] = '\0';
    if (strstr(buf, "explain")) {
      RCA(ctx->ux.log = iwxstr_new(), finish);
    }
  }

  rc = ejdb_exec(&ctx->ux);

  pthread_mutex_lock(&ctx->mtx);
  ctx->visitor_finished = true;
  pthread_cond_broadcast(&ctx->cond);
  pthread_mutex_unlock(&ctx->mtx);
  RCGO(rc, finish);

  if (!ctx->visitor_started) {
    if (ctx->ux.log) {
      iwxstr_cat(ctx->ux.log, "--------------------", 20);
      if (jql_has_aggregate_count(ctx->ux.q)) {
        iwxstr_printf(ctx->ux.log, "\n%" PRId64, ctx->ux.cnt);
      }
      ret = iwn_http_response_write(ctx->req->http, 200, "text/plain",
                                    iwxstr_ptr(ctx->ux.log), iwxstr_size(ctx->ux.log))
            ? 1 : -1;
    } else if (jql_has_aggregate_count(ctx->ux.q)) {
      ret = iwn_http_response_printf(ctx->req->http, 200, "text/plain", "%" PRId64, ctx->ux.cnt)
            ? 1 : -1;
    } else {
      ret = 200;
    }
  } else {
    ret = 1;
  }

finish:
  if (rc) {
    iwrc rcs = rc;
    iwrc_strip_code(&rcs);
    switch (rcs) {
      case JQL_ERROR_QUERY_PARSE: {
        const char *err = jql_error(ctx->ux.q);
        ret = iwn_http_response_write(ctx->req->http, 400, "text/plain", err, -1) ? 1 : -1;
        break;
      }
      case JQL_ERROR_NO_COLLECTION:
        ret = 400;
        JBR_RC_REPORT(ret, ctx->req->http, rc);
        break;
      default:
        JBR_RC_REPORT(ret, ctx->req->http, rc);
        break;
    }
  }
  jql_destroy(&ctx->ux.q);
  iwxstr_destroy(ctx->ux.log);
  return ret;
}

static int _on_http_request(struct iwn_wf_req *req, void *op) {
  struct jbr *jbr = op;
  struct rctx *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    return 500;
  }
  pthread_mutex_init(&ctx->mtx, 0);
  pthread_cond_init(&ctx->cond, 0);

  ctx->req = req;
  ctx->jbr = jbr;
  ctx->request_thread = pthread_self();

  uint32_t method = req->flags & IWN_WF_METHODS_ALL;
  req->http->user_data = ctx;
  req->http->on_request_dispose = _on_http_request_dispose;

  if ((req->flags & IWN_WF_OPTIONS) && req->path_unmatched[0] != '\0') {
    return 400;
  }

  {
    // Parse {collection name}/{id}
    const char *cname = req->path_unmatched;
    size_t len = strlen(cname);
    char *c = strchr(req->path_unmatched, '/');
    if (!c) {
      if (  len > EJDB_COLLECTION_NAME_MAX_LEN
         || (method & (IWN_WF_GET | IWN_WF_HEAD | IWN_WF_PUT | IWN_WF_DELETE | IWN_WF_PATCH))) {
        return 400;
      }
    } else {
      len = c - cname;
      if (  len > EJDB_COLLECTION_NAME_MAX_LEN
         || method == IWN_WF_POST) {
        return 400;
      }
      char *ep;
      ctx->id = strtoll(c + 1, &ep, 10);
      if (*ep != '\0' || ctx->id < 1) {
        return 400;
      }
    }
    memcpy(ctx->cname, cname, len);
    ctx->cname[len] = '\0';
  }

  if (jbr->http->access_token) {
    struct iwn_val val = iwn_http_request_header_get(req->http, "x-access-token", IW_LLEN("x-access-token"));
    if (!val.len) {
      if (jbr->http->read_anon) {
        if (  (method & (IWN_WF_GET | IWN_WF_HEAD))
           || (method == IWN_WF_POST && ctx->cname[0] == '\0')) {
          ctx->read_anon = true;
          goto process;
        }
      }
      return 401;
    } else {
      if (  val.len != jbr->http->access_token_len
         || strncmp(val.buf, jbr->http->access_token, val.len) != 0) {
        return 403;
      }
    }
  }

process:
  if (jbr->http->cors && iwn_http_response_header_set(req->http, "access-control-allow-origin", "*", 1)) {
    return 500;
  }
  if (ctx->cname[0] != '\0') {
    switch (method) {
      case IWN_WF_GET:
      case IWN_WF_HEAD:
        return _on_get(ctx);
      case IWN_WF_POST:
        return _on_post(ctx);
      case IWN_WF_PUT:
        return _on_put(ctx);
      case IWN_WF_PATCH:
        return _on_patch(ctx);
      case IWN_WF_DELETE:
        return _on_delete(ctx);
      default:
        return 400;
    }
  } else if (method == IWN_WF_POST) {
    return _on_query(ctx);
  } else if (method == IWN_WF_OPTIONS) {
    return _on_options(ctx);
  } else {
    return 400;
  }
}

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
} jbws_e;

static int _on_ws_session_http(struct iwn_wf_req *req, struct iwn_ws_handler_spec *spec) {
  struct jbr *jbr = spec->user_data;
  struct rctx *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    return 500;
  }
  pthread_mutex_init(&ctx->mtx, 0);
  pthread_cond_init(&ctx->cond, 0);
  ctx->req = req;
  ctx->jbr = jbr;
  req->http->user_data = ctx;

  if (jbr->http->access_token) {
    struct iwn_val val = iwn_http_request_header_get(req->http, "x-access-token", IW_LLEN("x-access-token"));
    if (val.len) {
      if (val.len != jbr->http->access_token_len || strncmp(val.buf, jbr->http->access_token, val.len) != 0) {
        return 403;
      }
    } else {
      if (jbr->http->read_anon) {
        ctx->read_anon = true;
      } else {
        return 401;
      }
    }
  }

  return 0;
}

static void _on_ws_session_dispose(struct iwn_ws_sess *ws) {
  _on_http_request_dispose(ws->req->http);
}

static bool _on_ws_session_init(struct iwn_ws_sess *ws) {
  struct rctx *ctx = ws->req->http->user_data;
  ctx->ws = ws;
  return true;
}

static bool _ws_error_send(struct iwn_ws_sess *ws, const char *key, const char *error, const char *extra) {
  if (key == 0) {
    return false;
  }
  if (extra) {
    return iwn_ws_server_printf(ws, "%s ERROR: %s %s", key, error, extra);
  } else {
    return iwn_ws_server_printf(ws, "%s ERROR: %s", key, error);
  }
}

static bool _ws_rc_send(struct iwn_ws_sess *ws, const char *key, iwrc rc, const char *extra) {
  const char *error = iwlog_ecode_explained(rc);
  return _ws_error_send(ws, key, error ? error : "unknown", extra);
}

static bool _ws_info(struct iwn_ws_sess *ws, struct mctx *mctx) {
  iwrc rc;
  JBL jbl = 0;
  IWXSTR *xstr = 0;
  bool ret = true;
  struct rctx *ctx = mctx->ctx;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, ejdb_get_meta(ctx->jbr->db, &jbl));
  RCA(xstr = iwxstr_new2(jbl->bn.size * 2), finish);
  RCC(rc, finish, iwxstr_printf(xstr, "%s\t", mctx->key));
  RCC(rc, finish, jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY));
  ret = iwn_ws_server_write(ws, iwxstr_ptr(xstr), iwxstr_size(xstr));

finish:
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  jbl_destroy(&jbl);
  iwxstr_destroy(xstr);
  return ret;
}

static bool _ws_coll_remove(struct iwn_ws_sess *ws, struct mctx *mctx) {
  iwrc rc;
  bool ret = true;
  struct rctx *ctx = mctx->ctx;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, ejdb_remove_collection(ctx->jbr->db, mctx->cname));
  ret = iwn_ws_server_write(ws, mctx->key, -1);

finish:
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static bool _ws_document_add(struct iwn_ws_sess *ws, struct mctx *mctx, const char *json) {
  iwrc rc;
  JBL jbl = 0;
  int64_t id;
  bool ret = true;
  struct rctx *ctx = ws->req->http->user_data;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, jbl_from_json(&jbl, json));
  RCC(rc, finish, ejdb_put_new(ctx->jbr->db, mctx->cname, jbl, &id));
  ret = iwn_ws_server_printf(ws, "%s\t%" PRId64, mctx->key, id);

finish:
  jbl_destroy(&jbl);
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static bool _ws_document_get(struct iwn_ws_sess *ws, struct mctx *mctx, int64_t id) {
  iwrc rc;
  JBL jbl = 0;
  IWXSTR *xstr = 0;
  bool ret = true;
  struct rctx *ctx = ws->req->http->user_data;

  RCC(rc, finish, ejdb_get(ctx->jbr->db, mctx->cname, id, &jbl));
  RCA(xstr = iwxstr_new2(jbl->bn.size * 2), finish);
  RCC(rc, finish, iwxstr_printf(xstr, "%s\t%" PRId64 "\t", mctx->key, id));
  RCC(rc, finish, jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY));
  ret = iwn_ws_server_write(ws, iwxstr_ptr(xstr), iwxstr_size(xstr));

finish:
  iwxstr_destroy(xstr);
  jbl_destroy(&jbl);
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static bool _ws_document_set(struct iwn_ws_sess *ws, struct mctx *mctx, int64_t id, const char *json) {
  iwrc rc;
  JBL jbl = 0;
  bool ret = true;
  struct rctx *ctx = ws->req->http->user_data;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, jbl_from_json(&jbl, json));
  RCC(rc, finish, ejdb_put(ctx->jbr->db, mctx->cname, jbl, id));
  ret = iwn_ws_server_printf(ctx->ws, "%s\t%" PRId64, mctx->key, id);

finish:
  jbl_destroy(&jbl);
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static bool _ws_document_del(struct iwn_ws_sess *ws, struct mctx *mctx, int64_t id) {
  iwrc rc;
  bool ret = true;
  struct rctx *ctx = ws->req->http->user_data;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, ejdb_del(ctx->jbr->db, mctx->cname, id));
  ret = iwn_ws_server_printf(ctx->ws, "%s\t%" PRId64, mctx->key, id);

finish:
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static bool _ws_document_patch(struct iwn_ws_sess *ws, struct mctx *mctx, int64_t id, const char *json) {
  iwrc rc;
  bool ret = true;
  struct rctx *ctx = ws->req->http->user_data;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, ejdb_patch(ctx->jbr->db, mctx->cname, json, id));
  ret = iwn_ws_server_printf(ctx->ws, "%s\t%" PRId64, mctx->key, id);

finish:
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static bool _ws_index_set(struct iwn_ws_sess *ws, struct mctx *mctx, int64_t mode, const char *path) {
  iwrc rc;
  bool ret = true;
  struct rctx *ctx = ws->req->http->user_data;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, ejdb_ensure_index(ctx->jbr->db, mctx->cname, path, mode));
  ret = iwn_ws_server_write(ws, mctx->key, -1);

finish:
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static bool _ws_index_del(struct iwn_ws_sess *ws, struct mctx *mctx, int64_t mode, const char *path) {
  iwrc rc;
  bool ret = true;
  struct rctx *ctx = ws->req->http->user_data;
  if (ctx->read_anon) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  RCC(rc, finish, ejdb_remove_index(ctx->jbr->db, mctx->cname, path, mode));
  ret = iwn_ws_server_write(ws, mctx->key, -1);

finish:
  if (rc && !_ws_rc_send(ws, mctx->key, rc, 0)) {
    ret = false;
  }
  return ret;
}

static iwrc _ws_query_visitor(EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  iwrc rc = 0;
  struct mctx *mctx = ux->opaque;
  if (!mctx->wbuf) {
    RCA(mctx->wbuf = iwxstr_new2(512), finish);
  } else {
    iwxstr_clear(mctx->wbuf);
  }
  if (ux->log) {
    iwn_ws_server_printf(mctx->ctx->ws, "%s\texplain\t%s", mctx->key, iwxstr_ptr(ux->log));
    iwxstr_destroy(ux->log);
    ux->log = 0;
  }
  RCC(rc, finish, iwxstr_printf(mctx->wbuf, "%s\t%" PRId64 "\t", mctx->key, doc->id));
  if (doc->node) {
    RCC(rc, finish, jbn_as_json(doc->node, jbl_xstr_json_printer, mctx->wbuf, 0));
  } else {
    RCC(rc, finish, jbl_as_json(doc->raw, jbl_xstr_json_printer, mctx->wbuf, 0));
  }
  if (!iwn_ws_server_write(mctx->ctx->ws, iwxstr_ptr(mctx->wbuf), iwxstr_size(mctx->wbuf))) {
    *step = 0;
  }

finish:
  return rc;
}

static bool _ws_query(struct iwn_ws_sess *ws, struct mctx *mctx, const char *query, bool explain) {
  iwrc rc;
  bool ret = false;
  struct rctx *ctx = mctx->ctx;

  EJDB_EXEC ux = {
    .db      = ctx->jbr->db,
    .opaque  = mctx,
    .visitor = _ws_query_visitor,
  };

  RCC(rc, finish,
      jql_create2(&ux.q, mctx->cname, query, JQL_SILENT_ON_PARSE_ERROR | JQL_KEEP_QUERY_ON_PARSE_ERROR));
  if (ctx->read_anon && jql_has_apply(ux.q)) {
    rc = JBR_ERROR_WS_ACCESS_DENIED;
    goto finish;
  }
  if (explain) {
    RCA(ux.log = iwxstr_new(), finish);
  }
  RCC(rc, finish, ejdb_exec(&ux));
  if (ux.log) {
    ret = iwn_ws_server_printf(mctx->ctx->ws, "%s\texplain\t%s", mctx->key, iwxstr_ptr(ux.log));
  } else {
    ret = true;
  }

finish:
  if (rc) {
    iwrc rcs = rc;
    iwrc_strip_code(&rcs);
    switch (rcs) {
      case JQL_ERROR_QUERY_PARSE:
        ret = _ws_error_send(ws, mctx->key, jql_error(ux.q), 0);
        break;
      default:
        ret = _ws_rc_send(ws, mctx->key, rc, 0);
        break;
    }
  } else {
    if (jql_has_aggregate_count(ux.q)) {
      ret = iwn_ws_server_printf(ws, "%s\t%" PRId64, mctx->key, ux.cnt);
    } else {
      ret = iwn_ws_server_write(ws, mctx->key, -1);
    }
  }
  jql_destroy(&ux.q);
  iwxstr_destroy(ux.log);
  return ret;
}

static bool _on_ws_msg_impl(struct iwn_ws_sess *ws, struct mctx *mctx, const char *msg_, size_t len) {
  if (len < 1) {
    return true;
  }

  int pos;
  jbws_e wsop = JBWS_NONE;
  const char *key = 0;
  char *msg = (char*) msg_; // Discard const

  // Trim left/right
  for ( ; len > 0 && isspace(msg[len - 1]); --len);
  for (pos = 0; pos < len && isspace(msg[pos]); ++pos);
  len -= pos;
  msg += pos;
  if (len < 1) {
    return true;
  }

  if (len == 1 && msg[0] == '?') {
    static const char help[]
      = "<key> info"
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
        "\n<key> <query>";
    return iwn_ws_server_write(ws, help, sizeof(help) - 1);
  }

  // Fetch key, after we can do good errors reporting
  for (pos = 0; pos < len && !isspace(msg[pos]); ++pos);
  if (pos > JBR_MAX_KEY_LEN) {
    return false;
  }
  memcpy(mctx->key, msg, pos);
  mctx->key[pos] = '\0';
  if (pos >= len) {
    return _ws_rc_send(ws, key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
  }

  for ( ; pos < len && isspace(msg[pos]); ++pos);
  len -= pos;
  msg += pos;
  if (len < 1) {
    return _ws_rc_send(ws, key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
  }

  // Fetch command
  for (pos = 0; pos < len && !isspace(msg[pos]); ++pos);

  if (pos <= len) {
    if (!strncmp("get", msg, pos)) {
      wsop = JBWS_GET;
    } else if (!strncmp("add", msg, pos)) {
      wsop = JBWS_ADD;
    } else if (!strncmp("set", msg, pos)) {
      wsop = JBWS_SET;
    } else if (!strncmp("query", msg, pos)) {
      wsop = JBWS_QUERY;
    } else if (!strncmp("del", msg, pos)) {
      wsop = JBWS_DEL;
    } else if (!strncmp("patch", msg, pos)) {
      wsop = JBWS_PATCH;
    } else if (!strncmp("explain", msg, pos)) {
      wsop = JBWS_EXPLAIN;
    } else if (!strncmp("info", msg, pos)) {
      wsop = JBWS_INFO;
    } else if (!strncmp("idx", msg, pos)) {
      wsop = JBWS_IDX;
    } else if (!strncmp("rmi", msg, pos)) {
      wsop = JBWS_NIDX;
    } else if (!strncmp("rmc", msg, pos)) {
      wsop = JBWS_REMOVE_COLL;
    }
  }

  if (wsop > JBWS_NONE) {
    if (wsop == JBWS_INFO) {
      return _ws_info(ws, mctx);
    }
    for ( ; pos < len && isspace(msg[pos]); ++pos);
    len -= pos;
    msg += pos;

    const char *rp = msg;
    for (pos = 0; pos < len && !isspace(msg[pos]); ++pos);
    len -= pos;
    msg += pos;

    if (pos < 1 || len < 1) {
      if (wsop != JBWS_REMOVE_COLL) {
        return _ws_rc_send(ws, mctx->key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
      }
    } else if (pos > EJDB_COLLECTION_NAME_MAX_LEN) {
      return _ws_rc_send(ws, key, JBR_ERROR_WS_INVALID_MESSAGE,
                         "Collection name exceeds maximum length allowed: "
                         "EJDB_COLLECTION_NAME_MAX_LEN");
    }
    memcpy(mctx->cname, rp, pos);
    mctx->cname[pos] = '\0';

    if (wsop == JBWS_REMOVE_COLL) {
      return _ws_coll_remove(ws, mctx);
    }

    for (pos = 0; pos < len && isspace(msg[pos]); ++pos);
    len -= pos;
    msg += pos;
    if (len < 1) {
      return _ws_rc_send(ws, mctx->key, JBR_ERROR_WS_INVALID_MESSAGE, JBR_WS_STR_PREMATURE_END);
    }

    switch (wsop) {
      case JBWS_ADD:
        msg[len] = '\0';
        return _ws_document_add(ws, mctx, msg);
      case JBWS_QUERY:
      case JBWS_EXPLAIN:
        msg[len] = '\0';
        return _ws_query(ws, mctx, msg, (wsop == JBWS_EXPLAIN));
      default: {
        char nbuf[JBNUMBUF_SIZE];
        for (pos = 0; pos < len && pos < JBNUMBUF_SIZE - 1 && isdigit(msg[pos]); ++pos) {
          nbuf[pos] = msg[pos];
        }
        nbuf[pos] = '\0';
        for ( ; pos < len && isspace(msg[pos]); ++pos);
        len -= pos;
        msg += pos;

        int64_t id = iwatoi(nbuf);
        if (id < 1) {
          return _ws_rc_send(ws, mctx->key, JBR_ERROR_WS_INVALID_MESSAGE, "Invalid document id specified");
        }
        msg[len] = '\0';
        switch (wsop) {
          case JBWS_GET:
            return _ws_document_get(ws, mctx, id);
          case JBWS_SET:
            return _ws_document_set(ws, mctx, id, msg);
          case JBWS_DEL:
            return _ws_document_del(ws, mctx, id);
          case JBWS_PATCH:
            return _ws_document_patch(ws, mctx, id, msg);
          case JBWS_IDX:
            return _ws_index_set(ws, mctx, id, msg);
          case JBWS_NIDX:
            return _ws_index_del(ws, mctx, id, msg);
          default:
            return _ws_rc_send(ws, mctx->key, JBR_ERROR_WS_INVALID_MESSAGE, 0);
        }
      }
    }
  } else {
    msg[len] = '\0';
    return _ws_query(ws, mctx, msg, false);
  }
}

static bool _on_ws_msg(struct iwn_ws_sess *ws, const char *msg, size_t len) {
  struct mctx mctx = {
    .ctx = ws->req->http->user_data,
  };
  bool ret = _on_ws_msg_impl(ws, &mctx, msg, len);
  iwxstr_destroy(mctx.wbuf);
  return ret;
}

static iwrc _configure(struct jbr *jbr) {
  iwrc rc;

  RCC(rc, finish, iwn_wf_create(&(struct iwn_wf_route) {
    .handler = 0
  }, &jbr->ctx));

  RCC(rc, finish, iwn_wf_route(iwn_ws_server_route_attach(&(struct iwn_wf_route) {
    .ctx = jbr->ctx,
    .pattern = "/",
    .flags = IWN_WF_GET,
  }, &(struct iwn_ws_handler_spec) {
    .handler = _on_ws_msg,
    .on_http_init = _on_ws_session_http,
    .on_session_init = _on_ws_session_init,
    .on_session_dispose = _on_ws_session_dispose,
    .user_data = jbr
  }), 0));

  RCC(rc, finish, iwn_wf_route(&(struct iwn_wf_route) {
    .ctx = jbr->ctx,
    .pattern = "/",
    .flags = IWN_WF_MATCH_PREFIX | IWN_WF_METHODS_ALL,
    .handler = _on_http_request,
    .user_data = jbr
  }, 0));

finish:
  return rc;
}

static iwrc _start(struct jbr *jbr) {
  const EJDB_HTTP *http = jbr->http;
  struct iwn_wf_server_spec spec = {
    .poller = jbr->poller,
    .listen = http->bind ? http->bind : "localhost",
    .port   = http->port > 0 ? http->port : 9292,
  };
  if (http->ssl_private_key) {
    spec.ssl.private_key = http->ssl_private_key;
    spec.ssl.private_key_len = -1;
  }
  if (http->ssl_certs) {
    spec.ssl.certs = http->ssl_certs;
    spec.ssl.certs_len = -1;
  }

  return iwn_wf_server(&spec, jbr->ctx);
}

iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, struct jbr **jbrp) {
  iwrc rc = 0;
  *jbrp = 0;
  if (!opts->http.enabled) {
    return 0;
  }
  JBR jbr = calloc(1, sizeof(*jbr));
  if (!jbr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  jbr->db = db;
  jbr->http = &opts->http;
  *jbrp = jbr;

  uint16_t cores = iwp_num_cpu_cores();
  cores = MAX(2, cores == 0 ? 1 : cores - 1);

  RCC(rc, finish, _configure(jbr));
  RCC(rc, finish, iwn_poller_create(cores, cores / 2, &jbr->poller));
  RCC(rc, finish, _start(jbr));

  if (!jbr->http->blocking) {
    pthread_create(&jbr->poller_thread, 0, _poller_worker, jbr);
  } else {
    iwn_poller_poll(jbr->poller);
    iwn_poller_destroy(&jbr->poller);
    *jbrp = 0;
    free(jbr);
  }

finish:
  if (rc) {
    *jbrp = 0;
    iwn_wf_destroy(jbr->ctx);
    iwn_poller_destroy(&jbr->poller);
    free(jbr);
  }
  return rc;
}

static const char* _jbr_ecodefn(locale_t locale, uint32_t ecode) {
  if (!((ecode > _JBR_ERROR_START) && (ecode < _JBR_ERROR_END))) {
    return 0;
  }
  switch (ecode) {
    case JBR_ERROR_WS_INVALID_MESSAGE:
      return "Invalid message recieved (JBR_ERROR_WS_INVALID_MESSAGE)";
    case JBR_ERROR_WS_ACCESS_DENIED:
      return "Access denied (JBR_ERROR_WS_ACCESS_DENIED)";
  }
  return 0;
}

iwrc jbr_init(void) {
  static int _jbr_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbr_initialized, 0, 1)) {
    return 0;
  }
  return iwlog_register_ecodefn(_jbr_ecodefn);
}
