#pragma once
#ifndef CONVERT_H
#define CONVERT_H

#include <stdio.h>

/**
 * sizeof `buf` must be at least 64 bytes
 */
static void jbi_ftoa(long double val, char buf[static 64], size_t *osz) {
  // TODO: review
  int sz = snprintf(buf, 64, "%.8Lf", val);
  if (sz <= 0) {
    buf[0] = '\0';
    *osz = 0;
    return;
  }
  while (sz > 0 && buf[sz - 1] == '0') { // trim right zeroes
    buf[sz - 1] = '\0';
    sz--;
  }
  if ((sz > 0) && (buf[sz - 1] == '.')) {
    buf[sz - 1] = '\0';
    sz--;
  }
  *osz = (size_t) sz;
}

#endif
