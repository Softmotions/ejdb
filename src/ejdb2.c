#include "ejdb2.h"
#include "jbl.h"
#include "jql.h"
#include <iowow/iowow.h>

iwrc ejdb_init() {
  iwrc rc = iw_init();
  RCRET(rc);
  rc = jbl_init();
  RCRET(rc);
  rc = jql_init();
  RCRET(rc);
  return 0;
}


