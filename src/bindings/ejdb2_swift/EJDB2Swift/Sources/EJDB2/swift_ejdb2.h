#if defined(__linux__)
#include <sys/types.h>
#include <bits/types/locale_t.h>
#include <stdlib.h>
#else if defined(__apple__)
#include <stdlib.h>
#endif
#include <ejdb2/ejdb2.h>

void swjb_free_json_node(void *ptr, void *op) {
  IWPOOL *pool = (IWPOOL*) op;
  if (pool) iwpool_destroy(pool);
}

void swjb_free_str(void *ptr, void *op) {
  if (ptr) free(ptr);
}
