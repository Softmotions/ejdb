#include "ejdb2.h"
#include "jbl.h"
#include "jql.h"

iwrc ejdb2_init() {
  iwrc rc = jbl_init();
  RCRET(rc);
  rc = jql_init();
  RCRET(rc);
  return 0;
}


