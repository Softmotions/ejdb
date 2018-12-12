#include "ejdb2_internal.h"

iwrc jb_idx_array_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc = 0;
  int64_t step = 1;
  bool matched = false;

  // TODO:

  rc = consumer(ctx, 0, 0, 0, &matched, rc);
  return rc;
}
