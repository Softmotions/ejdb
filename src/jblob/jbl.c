#include "jbl.h"





static const char *_jbl_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JBL_ERROR_START && ecode < _JBL_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    // todo
  }
  return 0;
}

iwrc jbl_init() {
  static int _jbl_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbl_initialized, 0, 1)) {
    return 0;
  }
  return iwlog_register_ecodefn(_jbl_ecodefn);
}
