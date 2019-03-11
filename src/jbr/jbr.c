#include "jbr.h"
#include "libwebsockets.h"


iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, JBR *jbr) {
  return 0;
}

iwrc jbr_shutdown(JBR *jbr) {
  return 0;
}

static const char *_jbr_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JBR_ERROR_START && ecode < _JBR_ERROR_START)) {
    return 0;
  }
  switch (ecode) {
    case JBR_ERROR_HTTP_PORT_IN_USE:
      return "HTTP port already in use (JBR_ERROR_HTTP_PORT_IN_USE)";
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
