#include "jbr.h"
#include <iowow/iwconv.h>
#include "ejdb2_internal.h"

#include <fio.h>
#include <fiobj.h>
#include <http/http.h>

struct _JBR {
  volatile bool terminated;
  volatile iwrc rc;
  pthread_t worker_thread;
  pthread_barrier_t start_barrier;
  const EJDB_HTTP *http;
  EJDB db;
};

static iwrc jbr_query_visitor(EJDB_EXEC *ctx, EJDB_DOC doc, int64_t *step) {
  return 0;
}

static void on_http_request(http_s *h) {
  //fiobj_obj2cstr(h->method)
  //h->method

  //fio_url_parse()
  // TODO:
  http_send_body(h, "Hello World!", 12);
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
  return iwlog_register_ecodefn(_jbr_ecodefn);
}
