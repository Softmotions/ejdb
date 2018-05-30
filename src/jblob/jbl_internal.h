#pragma once
#ifndef JBL_INTERNAL_H
#define JBL_INTERNAL_H

#include "binn.h"

typedef struct _JBLPTR {
  int cnt;          /**< Number of nodes */
  int pos;          /**< Current node position (like a cursor) */
  char *n[1];       /**< Path nodes */
} *JBLPTR;

struct _JBL {
  binn bn;
};

#ifdef IW_TESTS
iwrc jbl_ptr(const char *path, JBLPTR *jpp);
void jbl_ptr_destroy(JBLPTR *jpp);
#endif

#endif
