#pragma once
#ifndef EJDB_TEST_H
#define EJDB_TEST_H

#include "ejdb2.h"
#include <string.h>

static iwrc put_json(EJDB db, const char *coll, const char *json) {
  const size_t len = strlen(json);
  char buf[len + 1];
  memcpy(buf, json, len);
  buf[len] = '\0';
  for (int i = 0; buf[i]; ++i) {
    if (buf[i] == '\'') buf[i] = '"';
  }
  JBL jbl;
  uint64_t llv;
  iwrc rc = jbl_from_json(&jbl, buf);
  RCRET(rc);
  rc = ejdb_put_new(db, coll, jbl, &llv);
  jbl_destroy(&jbl);
  return rc;
}

#endif
