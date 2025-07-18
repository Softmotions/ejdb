#include <stdlib.h>

#if defined(__linux__)
  #include <sys/types.h>
  #if defined(__GLIBC__) && (__GLIBC__ == 2 && __GLIBC_MINOR__ < 26)
    #include <xlocale.h>
  #else
    #include <bits/types/locale_t.h>
  #endif
#endif

#ifdef INPROJECT_BUILD
#include "ejdb2.h"
#else
#include <ejdb2/ejdb2.h>
#endif

static void swjb_free_json_node(void *ptr, void *op) {
  IWPOOL *pool = (IWPOOL*) op;
  if (pool) iwpool_destroy(pool);
}

static void swjb_free_str(void *ptr, void *op) {
  if (ptr) free(ptr);
}
