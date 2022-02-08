#include "jbr.h"
#include "ejdb2_internal.h"

#include <iwnet/ws_server.h>
#include <iwnet/pairs.h>

#include <pthread.h>

struct jbr {
  struct iwn_poller *poller;
  pthread_t poller_thread;
  const EJDB_HTTP   *http;
  struct iwn_wf_ctx *ctx;
  JBR *jbrp;
  EJDB db;
};

struct rctx {
  struct iwn_wf_req *req;
  struct jbr     *jbr;
  struct iwn_vals vals;
  pthread_mutex_t mtx;
  EJDB_EXEC       ux;
  int64_t id;
  bool    read_anon;
  char    cname[EJDB_COLLECTION_NAME_MAX_LEN + 1];
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

void _destroy(struct jbr *jbr) {
  if (jbr) {
    if (jbr->jbrp) {
      *jbr->jbrp = 0;
    }
    iwn_poller_destroy(&jbr->poller);
    if (jbr->poller_thread) {
      pthread_join(jbr->poller_thread, 0);
    }
    free(jbr);
  }
}

void jbr_shutdown(struct jbr *jbr) {
  if (jbr && jbr->poller) {
    iwn_poller_shutdown_request(jbr->poller);
  }
}

static void* _poller_worker(void *op) {
  JBR jbr = op;
  iwn_poller_poll(jbr->poller);
  _destroy(jbr);
  return 0;
}

static void _on_http_request_dispose(struct iwn_http_req *req) {
  struct rctx *rctx = req->request_user_data;
  if (rctx) {
    for (struct iwn_val *v = rctx->vals.first; v; ) {
      struct iwn_val *n = v->next;
      free(v->buf);
      v = n;
    }
    pthread_mutex_destroy(&rctx->mtx);
    free(rctx);
  }
}

static int _on_get(struct rctx *ctx) {
  JBL jbl = 0;
  IWXSTR *xstr = 0;
  int nbytes = 0, ret = 500;

  iwrc rc = ejdb_get(ctx->jbr->db, ctx->cname, ctx->id, &jbl);
  if ((rc == IWKV_ERROR_NOTFOUND) || (rc == IW_ERROR_NOT_EXISTS)) {
    return 404;
  } else {
    goto finish;
  }
  if (ctx->req->flags & IWN_WF_HEAD) {
    rc = jbl_as_json(jbl, jbl_count_json_printer, &nbytes, JBL_PRINT_PRETTY);
  } else {
    xstr = iwxstr_new2(jbl->bn.size * 2);
    if (!xstr) {
      goto finish;
    }
    RCC(rc, finish, jbl_as_json(jbl, jbl_xstr_json_printer, xstr, JBL_PRINT_PRETTY));
    nbytes = iwxstr_size(xstr);
  }

  iwn_http_response_header_i64_set(ctx->req->http, "content-length", nbytes);
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

static iwrc _query_visitor(EJDB_EXEC *ux, EJDB_DOC doc, int64_t *step) {
  iwrc rc = 0;
  struct rctx *ctx = ux->opaque;
  IWXSTR *xstr = iwxstr_new();
  RCA(xstr, finish);

  if (ux->log) {
    iwxstr_cat(xstr, iwxstr_ptr(ux->log), iwxstr_size(ux->log));
  }


  //iwn_http_response_chunk_write


finish:
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

  //ejdb_exec

  if (rc) {
    JBR_RC_REPORT(ret, ctx->req->http, rc);
  }

  return 0;
}

static int _on_http_request(struct iwn_wf_req *req, void *op) {
  struct jbr *jbr = op;

  struct rctx *rctx = calloc(1, sizeof(*rctx));
  if (!rctx) {
    return 500;
  }
  pthread_mutex_init(&rctx->mtx, 0);
  rctx->req = req;
  rctx->jbr = jbr;

  uint32_t method = req->flags & (IWN_WF_METHODS_ALL);
  req->http->request_user_data = rctx;
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
      rctx->id = strtoll(c + 1, &ep, 10);
      if (*ep != '\0' || rctx->id < 1) {
        return 400;
      }
    }
    memcpy(rctx->cname, cname, len);
    rctx->cname[len] = '\0';
  }

  if (jbr->http->access_token) {
    struct iwn_val val = iwn_http_request_header_get(req->http, "x-access-token", IW_LLEN("x-access-token"));
    if (!val.len) {
      if (jbr->http->read_anon) {
        if (  (method & (IWN_WF_GET | IWN_WF_HEAD))
           || (method == IWN_WF_POST && rctx->cname[0] == '\0')) {
          rctx->read_anon = true;
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
  if (rctx->cname[0] != '\0') {
    switch (method) {
      case IWN_WF_GET:
      case IWN_WF_HEAD:
        return _on_get(rctx);
      case IWN_WF_POST:
        return _on_post(rctx);
      case IWN_WF_PUT:
        return _on_put(rctx);
      case IWN_WF_PATCH:
        return _on_patch(rctx);
      case IWN_WF_DELETE:
        return _on_delete(rctx);
      default:
        return 400;
    }
  } else if (method == IWN_WF_POST) {
    return _on_query(rctx);
  } else if (method == IWN_WF_OPTIONS) {
    return _on_options(rctx);
  } else {
    return 400;
  }
}

static bool _on_ws_session_init(struct iwn_ws_sess *sess) {
  return true;
}

static void _on_ws_session_dispose(struct iwn_ws_sess *sess) {
}

static bool _on_ws_msg(struct iwn_ws_sess *sess, const char *msg, size_t msg_len) {
  return true;
}

static iwrc _configure(struct jbr *jbr) {
  iwrc rc = 0;

  RCC(rc, finish, iwn_wf_create(&(struct iwn_wf_route) {
    .handler = 0
  }, &jbr->ctx));

  RCC(rc, finish, iwn_wf_route(iwn_ws_server_route_attach(&(struct iwn_wf_route) {
    .ctx = jbr->ctx,
    .pattern = "/",
    .flags = IWN_WF_GET,
  }, &(struct iwn_ws_handler_spec) {
    .handler = _on_ws_msg,
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
    spec.private_key = http->ssl_private_key;
    spec.private_key_len = -1;
  }
  if (http->ssl_certs) {
    spec.certs = http->ssl_certs;
    spec.certs_len = -1;
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
  jbr->jbrp = jbrp;
  *jbrp = jbr;

  RCC(rc, finish, _configure(jbr));
  RCC(rc, finish, iwn_poller_create(2, 2, &jbr->poller));
  RCC(rc, finish, _start(jbr));

  if (!jbr->http->blocking) {
    pthread_create(&jbr->poller_thread, 0, _poller_worker, jbr);
  } else {
    iwn_poller_poll(jbr->poller);
    _destroy(jbr);
    jbr = 0;
  }

finish:
  if (rc) {
    _destroy(jbr);
  }
  return rc;
}

iwrc jbr_init(void) {
  return 0;
}
