#include "jbr.h"


iwrc jbrest_init() {
  static int _jbrest_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbrest_initialized, 0, 1)) {
    return 0;
  }
  return 0;//iwlog_register_ecodefn(_jbl_ecodefn);
}
