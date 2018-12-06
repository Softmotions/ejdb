#include "ejdb2_internal.h"

iwrc jb_idx_dup_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc = 0;
  int64_t step = 1;

  // TODO:

  rc = consumer(ctx, 0, 0, 0, rc);
  return rc;
}
