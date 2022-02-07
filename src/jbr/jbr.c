#include "jbr.h"
#include "ejdb2_internal.h"

#include <iwnet/ws_server.h>

typedef enum {
  JBR_GET = 1,
  JBR_PUT,
  JBR_PATCH,
  JBR_DELETE,
  JBR_POST,
  JBR_HEAD,
  JBR_OPTIONS,
} jbr_method_e;

struct _JBR {
  struct iwn_poller *poller;
  pthread_t poller_thread;
  const EJDB_HTTP   *http;
  struct iwn_wf_ctx *ctx;
  JBR *jbrp;
  EJDB db;
};

void _destroy(JBR jbr) {
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

void jbr_shutdown(JBR jbr) {
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

static iwrc _configure(JBR jbr) {
  iwrc rc = 0;

  return rc;
}

static iwrc _start(JBR jbr) {
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

iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, JBR *jbrp) {
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
