#include "ejdb2.h"
#include "jbl.h"

iwrc ejdb2_init() {
  iwrc rc = jbl_init();
  RCRET(rc);
  return 0;
}


