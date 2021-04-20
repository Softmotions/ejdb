#pragma once
#ifndef EJDB_TEST_H
#define EJDB_TEST_H

/**************************************************************************************************
 * EJDB2
 *
 * MIT License
 *
 * Copyright (c) 2012-2021 Softmotions Ltd <info@softmotions.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *************************************************************************************************/

#include "ejdb2.h"
#include "ejdb2_internal.h"
#include <string.h>

static iwrc put_json(EJDB db, const char *coll, const char *json) {
  const size_t len = strlen(json);
  char buf[len + 1];
  memcpy(buf, json, len);
  buf[len] = '\0';
  for (int i = 0; buf[i]; ++i) {
    if (buf[i] == '\'') {
      buf[i] = '"';
    }
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
    if (buf[i] == '\'') {
      buf[i] = '"';
    }
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
    if (buf[i] == '\'') {
      buf[i] = '"';
    }
  }
  return ejdb_patch(db, coll, buf, id);
}

#endif
