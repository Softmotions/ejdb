#pragma once
#ifndef EJDB_TEST_H
#define EJDB_TEST_H

#include "ejdb2.h"
#include "ejdb2_internal.h"
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
  int64_t llv;
  iwrc rc = jbl_from_json(&jbl, buf);
  RCRET(rc);
  rc = ejdb_put_new(db, coll, jbl, &llv);
  jbl_destroy(&jbl);
  return rc;
}

static iwrc put_json2(EJDB db, const char *coll, const char *json, int64_t *id) {
  const size_t len = strlen(json);
  char buf[len + 1];
  memcpy(buf, json, len);
  buf[len] = '\0';
  for (int i = 0; buf[i]; ++i) {
    if (buf[i] == '\'') buf[i] = '"';
  }
  JBL jbl;
  iwrc rc = jbl_from_json(&jbl, buf);
  RCRET(rc);

  if (*id == 0) {
    rc = ejdb_put_new(db, coll, jbl, id);
  } else {
    rc = ejdb_put(db, coll, jbl, *id);
  }
  jbl_destroy(&jbl);
  return rc;
}

static iwrc patch_json(EJDB db, const char *coll, const char *patchjson, int64_t id) {
  const size_t len = strlen(patchjson);
  char buf[len + 1];
  memcpy(buf, patchjson, len);
  buf[len] = '\0';
  for (int i = 0; buf[i]; ++i) {
    if (buf[i] == '\'') buf[i] = '"';
  }
  return ejdb_patch(db, coll, buf, id);
}


#endif
