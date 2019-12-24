#include <stdlib.h>
#if defined(__linux__)
#include <sys/types.h>
#include <bits/types/locale_t.h>
#include <stdlib.h>
#endif

#ifdef INPROJECT_BUILD
#include "ejdb2.h"
#else
#include <ejdb2/ejdb2.h>
#endif

void swjb_free_json_node(void *ptr, void *op) {
  IWPOOL *pool = (IWPOOL*) op;
  if (pool) iwpool_destroy(pool);
}

void swjb_free_str(void *ptr, void *op) {
  if (ptr) free(ptr);
}
