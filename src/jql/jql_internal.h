#pragma once
#ifndef JQL_INTERNAL_H
#define JQL_INTERNAL_H

#include "jqp.h"

/** Query object */
struct _JQL {
  bool dirty;
  bool matched;
  JQP_QUERY *qp;
  const char *coll;
  void *opaque;
};


#endif

