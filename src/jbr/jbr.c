#include "jbr.h"
#include "ejdb2_internal.h"

#include <iwnet/ws_server.h>

#include <stdlib.h>

struct jbr {
  struct iwn_poller *poller;
  pthread_t poller_thread;
  const EJDB_HTTP   *http;
  struct iwn_wf_ctx *ctx;
  JBR *jbrp;
  EJDB db;
};

struct rctx {
  IWXSTR *wbuf;
  int64_t id;
  bool    read_anon;
  char    cname[EJDB_COLLECTION_NAME_MAX_LEN + 1];
};

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

static void _handle_request_dispose(struct iwn_http_req *req) {
  struct rctx *rctx = req->request_user_data;
  if (rctx) {
    free(rctx);
  }
}

static int _handle_request(struct iwn_wf_req *req, void *op) {
  int ret = 0;
  iwrc rc = 0;
  struct jbr *jbr = op;

  struct rctx *rctx = calloc(1, sizeof(*rctx));
  if (!rctx) {
    return 500;
  }
  uint32_t method = req->flags & (IWN_WF_METHODS_ALL);
  req->http->request_user_data = rctx;
  req->http->on_request_dispose = _handle_request_dispose;

  if ((req->flags & IWN_WF_OPTIONS) && req->path_unmatched[0] != '\0') {
    return 400;
  }

  {
    // Parse colletion {name}/{id}
    size_t len = strlen(req->path_unmatched), clen = 0;
    const char *cname = req->path_unmatched;
    char *c = strchr(req->path_unmatched, '/');
    if (!c) {
      if (  len > EJDB_COLLECTION_NAME_MAX_LEN
         || (method & (IWN_WF_GET | IWN_WF_HEAD | IWN_WF_PUT | IWN_WF_DELETE | IWN_WF_PATCH))) {
        return 400;
      }
      clen = len;
    } else {
      clen = c - cname;
      if (  clen > EJDB_COLLECTION_NAME_MAX_LEN
         || method == IWN_WF_POST) {
        return 400;
      }
      char *ep;
      rctx->id = strtoll(c + 1, &ep, 10);
      if (*ep != '\0' || rctx->id < 1) {
        return 400;
      }
    }
    memcpy(rctx->cname, cname, clen);
    rctx->cname[clen] = '\0';
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
  if (jbr->http->cors) {
    RCC(rc, finish, iwn_http_response_header_set(req->http, "access-control-allow-origin", "*", 1));
  }
  if (rctx->cname[0] != '\0') {

    
  } else if (method == IWN_WF_POST) {
  } else if (method == IWN_WF_OPTIONS) {
  } else {
    return 400;
  }

finish:
  if (rc) {
    ret = 500;
  }
  return ret;
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
    .handler = _handle_request,
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
