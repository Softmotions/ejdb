#include "jq.h"
#include "jbldom.h"



static const char *_jq_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JQ_ERROR_START && ecode < _JQ_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    default:
      break;
  }
  return 0;
}

iwrc jq_init() {
  static int _jq_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jq_initialized, 0, 1)) {
    return 0;
  }
  return iwlog_register_ecodefn(_jq_ecodefn);
}
