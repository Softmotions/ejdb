#include "jbl.h"
#include <ctype.h>
#include <iowow/iwconv.h>
#include <iowow/iwxstr.h>
#include "jbl_internal.h"

iwrc jbl_create_empty_object(JBL *jblp) {
  *jblp = calloc(1, sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  binn_create(&(*jblp)->bn, BINN_OBJECT, 0, 0);
  return 0;
}

iwrc jbl_create_empty_array(JBL *jblp) {
  *jblp = calloc(1, sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  binn_create(&(*jblp)->bn, BINN_LIST, 0, 0);
  return 0;
}

iwrc jbl_from_buf_keep(JBL *jblp, void *buf, size_t bufsz) {
  iwrc rc = 0;
  if (bufsz < MIN_BINN_SIZE) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  int type, size = 0;
  if (!binn_is_valid_header(buf, &type, NULL, &size, NULL)) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  if (size > bufsz) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  *jblp = calloc(1, sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl = *jblp;
  jbl->bn.header = BINN_MAGIC;
  jbl->bn.ptr = buf;
  jbl->bn.freefn = free;
  return rc;
}

void jbl_destroy(JBL *jblp) {
  if (*jblp) {
    JBL jbl = *jblp;
    binn_free(&jbl->bn);
    free(jbl);
    *jblp = 0;
  }
}

jbl_type_t jbl_type(JBL jbl) {
  switch (jbl->bn.type) {
    case BINN_NULL:
      return JBV_NULL;
    case BINN_STRING:
      return JBV_STR;
    case BINN_OBJECT:
    case BINN_MAP:
      return JBV_OBJECT;
    case BINN_LIST:
      return JBV_ARRAY;
    case BINN_BOOL:
    case BINN_TRUE:
    case BINN_FALSE:
      return JBV_BOOL;
    case BINN_UINT8:
    case BINN_UINT16:
    case BINN_UINT32:
    case BINN_UINT64:
    case BINN_INT8:
    case BINN_INT16:
    case BINN_INT32:
    case BINN_INT64:
      return JBV_I64;
    case BINN_FLOAT32:
    case BINN_FLOAT64:
      return JBV_F64;
  }
  return JBV_NONE;
}

size_t jbl_count(JBL jbl) {
  return jbl->bn.count;
}

size_t jbl_size(JBL jbl) {
  return jbl->bn.size;
}

static iwrc _jbl_from_json(binn *res, nx_json *nxjson) {
  iwrc rc = 0;
  switch (nxjson->type) {
    case NX_JSON_OBJECT:
      if (!binn_create(res, BINN_OBJECT, 0, NULL)) {
        return JBL_ERROR_CREATION;
      }
      for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
        binn bv;
        rc = _jbl_from_json(&bv, nxj);
        RCRET(rc);
        if (!binn_object_set_value(res, nxj->key, &bv)) {
          rc = JBL_ERROR_CREATION;
        }
        binn_free(&bv);
        RCRET(rc);
      }
      break;
    case NX_JSON_ARRAY:
      if (!binn_create(res, BINN_LIST, 0, NULL)) {
        return JBL_ERROR_CREATION;
      }
      for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
        binn bv;
        rc = _jbl_from_json(&bv, nxj);
        RCRET(rc);
        if (!binn_list_add_value(res, &bv)) {
          rc = JBL_ERROR_CREATION;
        }
        binn_free(&bv);
        RCRET(rc);
      }
      break;
    case NX_JSON_STRING:
      binn_init_item(res);
      binn_set_string(res, (void *) nxjson->text_value, 0);
      break;
    case NX_JSON_INTEGER:
      binn_init_item(res);
      if (nxjson->int_value <= INT_MAX && nxjson->int_value >= INT_MIN) {
        binn_set_int(res, nxjson->int_value);
      } else {
        binn_set_int64(res, nxjson->int_value);
      }
      break;
    case NX_JSON_DOUBLE:
      binn_init_item(res);
      binn_set_double(res, nxjson->dbl_value);
      break;
    case NX_JSON_BOOL:
      binn_init_item(res);
      binn_set_bool(res, nxjson->int_value);
      break;
    case NX_JSON_NULL:
      binn_init_item(res);
      binn_set_null(res);
      break;
    default:
      rc = JBL_ERROR_CREATION;
      break;
  }
  return rc;
}

iwrc jbl_from_json(JBL *jblp, const char *jsonstr) {
  iwrc rc = 0;
  *jblp = 0;
  char *json = strdup(jsonstr); //nxjson uses inplace data modification
  if (!json) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  binn bv;
  JBL jbl = 0;
  nx_json *nxjson = nx_json_parse_utf8(json);
  if (!nxjson) {
    rc = JBL_ERROR_PARSE_JSON;
    goto finish;
  }
  rc = _jbl_from_json(&bv, nxjson);
  RCGO(rc, finish);
  jbl = malloc(sizeof(*jbl));
  if (!jbl) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  if (bv.writable && bv.dirty) {
    binn_save_header(&bv);
  }
  memcpy(&jbl->bn, &bv, sizeof(bv));
  jbl->bn.allocated = 0;

finish:
  if (nxjson) {
    nx_json_free(nxjson);
  }
  free(json);
  if (!rc) {
    *jblp = jbl;
  } else if (jbl) {
    jbl_destroy(&jbl);
  }
  return rc;
}

IW_INLINE int _jbl_utf8_ch_len(uint8_t ch) {
  if (!(ch & 0x80)) return 1;
  switch (ch & 0xf0) {
    case 0xf0:
      return 4;
    case 0xe0:
      return 3;
    default:
      return 2;
  }
}

static iwrc _jbl_write_double(double num, jbl_json_printer pt, void *op) {
  char buf[IWFTOA_BUFSIZE];
  iwftoa(num, buf);
  return pt(buf, -1, 0, 0, op);
}

static iwrc _jbl_write_int(int64_t num, jbl_json_printer pt, void *op) {
  char buf[JBNUMBUF_SIZE];
  int sz = iwitoa(num, buf, sizeof(buf));
  return pt(buf, sz, 0, 0, op);
}

static iwrc _jbl_write_string(const char *str, size_t len, jbl_json_printer pt, void *op) {
  iwrc rc = pt(0, 0, '"', 1, op);
  RCRET(rc);
  static const char *digits = "0123456789abcdef";
  static const char *specials = "btnvfr";
  const uint8_t *p = (const uint8_t *) str;

#define PT(data_, size_, ch_, count_) do {\
    rc = pt((const char*) (data_), size_, ch_, count_, op);\
    RCGO(rc, finish); \
  } while(0)

  if (len == -1) {
    len = strlen(str);
  }
  for (size_t i = 0; i < len; i++) {
    size_t clen;
    uint8_t ch = p[i];
    if (ch == '"' || ch == '\\') {
      PT(0, 0, '\\', 1);
      PT(0, 0, ch, 1);
    } else if (ch >= '\b' && ch <= '\r') {
      PT(0, 0, '\\', 1);
      PT(0, 0, specials[ch - '\b'], 1);
    } else if (isprint(ch)) {
      PT(0, 0, ch, 1);
    } else if ((clen = _jbl_utf8_ch_len(ch)) == 1) {
      PT("\\u00", -1, 0, 0);
      PT(0, 0, digits[(ch >> 4) % 0xf], 1);
      PT(0, 0, digits[ch % 0xf], 1);
    } else {
      PT(p + i, clen, 0, 0);
      i += clen - 1;
    }
  }
  rc = pt(0, 0, '"', 1, op);
finish:
  return rc;
#undef PT
}

static iwrc _jbl_as_json(binn *bn, jbl_json_printer pt, void *op, int lvl, bool pretty) {
  iwrc rc = 0;
  binn bv;
  binn_iter iter;
  int lv;
  int64_t llv;
  double dv;
  char key[MAX_BIN_KEY_LEN + 1];

#define PT(data_, size_, ch_, count_) do {\
    rc = pt(data_, size_, ch_, count_, op); \
    RCGO(rc, finish); \
  } while(0)

  switch (bn->type) {

    case BINN_LIST:
      if (!binn_iter_init(&iter, bn, bn->type)) {
        rc = JBL_ERROR_INVALID;
        goto finish;
      }
      PT(0, 0, '[', 1);
      if (bn->count && pretty) {
        PT(0, 0, '\n', 1);
      }
      for (int i = 0; binn_list_next(&iter, &bv); ++i) {
        if (pretty) {
          PT(0, 0, ' ', lvl + 1);
        }
        rc = _jbl_as_json(&bv, pt, op, lvl + 1, pretty);
        RCGO(rc, finish);
        if (i < bn->count - 1) {
          PT(0, 0, ',', 1);
        }
        if (pretty) {
          PT(0, 0, '\n', 1);
        }
      }
      if (bn->count && pretty) {
        PT(0, 0, ' ', lvl);
      }
      PT(0, 0, ']', 1);
      break;

    case BINN_OBJECT:
    case BINN_MAP:
      if (!binn_iter_init(&iter, bn, bn->type)) {
        rc = JBL_ERROR_INVALID;
        goto finish;
      }
      PT(0, 0, '{', 1);
      if (bn->count && pretty) {
        PT(0, 0, '\n', 1);
      }
      if (bn->type == BINN_OBJECT) {
        for (int i = 0; binn_object_next(&iter, key, &bv); ++i) {
          if (pretty) {
            PT(0, 0, ' ', lvl + 1);
          }
          rc = _jbl_write_string(key, -1, pt, op);
          RCGO(rc, finish);
          if (pretty) {
            PT(": ", -1, 0, 0);
          } else {
            PT(0, 0, ':', 1);
          }
          rc = _jbl_as_json(&bv, pt, op, lvl + 1, pretty);
          RCGO(rc, finish);
          if (i < bn->count - 1) {
            PT(0, 0, ',', 1);
          }
          if (pretty) {
            PT(0, 0, '\n', 1);
          }
        }
      } else {
        for (int i = 0; binn_map_next(&iter, &lv, &bv); ++i) {
          if (pretty) {
            PT(0, 0, ' ', lvl + 1);
          }
          PT(0, 0, '"', 1);
          rc = _jbl_write_int(lv, pt, op);
          RCGO(rc, finish);
          PT(0, 0, '"', 1);
          if (pretty) {
            PT(": ", -1, 0, 0);
          } else {
            PT(0, 0, ':', 1);
          }
          rc = _jbl_as_json(&bv, pt, op, lvl + 1, pretty);
          RCGO(rc, finish);
          if (i < bn->count - 1) {
            PT(0, 0, ',', 1);
          }
          if (pretty) {
            PT(0, 0, '\n', 1);
          }
        }
      }
      if (bn->count && pretty) {
        PT(0, 0, ' ', lvl);
      }
      PT(0, 0, '}', 1);
      break;

    case BINN_STRING:
      rc = _jbl_write_string(bn->ptr, -1, pt, op);
      break;

    case BINN_UINT8:
      llv = bn->vuint8;
      goto loc_int;
    case BINN_UINT16:
      llv = bn->vuint16;
      goto loc_int;
    case BINN_UINT32:
      llv = bn->vuint32;
      goto loc_int;
    case BINN_INT8:
      llv = bn->vint8;
      goto loc_int;
    case BINN_INT16:
      llv = bn->vint16;
      goto loc_int;
    case BINN_INT32:
      llv = bn->vint32;
      goto loc_int;
    case BINN_INT64:
      llv = bn->vint64;
      goto loc_int;
    case BINN_UINT64: // overflow?
      llv = bn->vuint64;
loc_int:
      rc = _jbl_write_int(llv, pt, op);
      break;

    case BINN_FLOAT32:
      dv = bn->vfloat;
      goto loc_float;
    case BINN_FLOAT64:
      dv = bn->vdouble;
loc_float:
      rc = _jbl_write_double(dv, pt, op);
      break;

    case BINN_TRUE:
      PT("true", -1, 0, 1);
      break;
    case BINN_FALSE:
      PT("false", -1, 0, 1);
      break;
    case BINN_BOOL:
      PT(bn->vbool ? "true" : "false", -1, 0, 1);
      break;
    case BINN_NULL:
      PT("null", -1, 0, 1);
      break;
    default:
      rc = IW_ERROR_ASSERTION;
      break;
  }
finish:
  return rc;
#undef PT
}

iwrc jbl_as_json(JBL jbl, jbl_json_printer pt, void *op, bool pretty) {
  if (!jbl || !pt) {
    return IW_ERROR_INVALID_ARGS;
  }
  return _jbl_as_json(&jbl->bn, pt, op, 0, pretty);
}

iwrc jbl_fstream_json_printer(const char *data, size_t size, char ch, int count, void *op) {
  FILE *file = op;
  if (!file) {
    return IW_ERROR_INVALID_ARGS;
  }
  if (!data) {
    if (count) {
      size_t wc = fwrite(&ch, sizeof(ch), count, file);
      if (wc != count) {
        return iwrc_set_errno(IW_ERROR_IO_ERRNO, errno);
      }
    }
  } else {
    if (size == -1) size = strlen(data);
    if (!count) count = 1;
    for (int i = 0; i < count; ++i) {
      if (fprintf(file, "%.*s", (int) size, data) < 0) {
        return iwrc_set_errno(IW_ERROR_IO_ERRNO, errno);
      }
    }
  }
  return 0;
}

iwrc jbl_xstr_json_printer(const char *data, size_t size, char ch, int count, void *op) {
  IWXSTR *xstr = op;
  if (!xstr) {
    return IW_ERROR_INVALID_ARGS;
  }
  if (!data) {
    if (count) {
      for (int i = 0; i < count; ++i) {
        iwrc rc = iwxstr_cat(xstr, &ch, 1);
        RCRET(rc);
      }
    }
  } else {
    if (size == -1) size = strlen(data);
    if (!count) count = 1;
    for (int i = 0; i < count; ++i) {
      iwrc rc = iwxstr_cat(xstr, data, size);
      RCRET(rc);
    }
  }
  return 0;
}

int64_t jbl_get_i64(JBL jbl) {
  assert(jbl);
  switch (jbl->bn.type) {
    case BINN_UINT8:
      return jbl->bn.vuint8;
    case BINN_UINT16:
      return jbl->bn.vuint16;
    case BINN_UINT32:
      return jbl->bn.vuint32;
    case BINN_UINT64:
      return jbl->bn.vuint64;
    case BINN_INT8:
      return jbl->bn.vint8;
    case BINN_INT16:
      return jbl->bn.vint16;
    case BINN_INT32:
      return jbl->bn.vint32;
    case BINN_INT64:
      return jbl->bn.vint64;
    default:
      return 0;
  }
}

int32_t jbl_get_i32(JBL jbl) {
  return jbl_get_i64(jbl);
}

double jbl_get_f64(JBL jbl) {
  assert(jbl);
  switch (jbl->bn.type) {
    case BINN_FLOAT64:
      return jbl->bn.vdouble;
    case BINN_FLOAT32:
      return jbl->bn.vfloat;
    default:
      return 0.0;
  }
}

char *jbl_get_str(JBL jbl) {
  assert(jbl && jbl->bn.type == BINN_STRING);
  if (jbl->bn.type != BINN_STRING) {
    return 0;
  } else {
    return jbl->bn.ptr;
  }
}

size_t jbl_copy_strn(JBL jbl, char *buf, size_t bufsz) {
  assert(jbl && buf && jbl->bn.type == BINN_STRING);
  if (jbl->bn.type != BINN_STRING) {
    return 0;
  }
  size_t slen = strlen(jbl->bn.ptr);
  size_t ret = MIN(slen, bufsz);
  memcpy(buf, jbl->bn.ptr, ret);
  return ret;
}

iwrc jbl_as_buf(JBL jbl, void **buf, size_t *size) {
  assert(jbl && buf && size);
  if (jbl->bn.writable && jbl->bn.dirty) {
    if (!binn_save_header(&jbl->bn)) {
      return JBL_ERROR_INVALID;
    }
  }
  *buf = jbl->bn.ptr;
  *size = jbl->bn.size;
  return 0;
}

//----------------------------------------------------------------------------------------------------------

static iwrc _jbl_ptr_pool(const char *path, JBLPTR *jpp, IWPOOL *pool) {
  iwrc rc = 0;
  int cnt = 0, len, sz, doff;
  int i, j, k;
  JBLPTR jp;
  char *jpr; // raw pointer to jp
  *jpp = 0;
  if (!path || path[0] != '/') {
    return JBL_ERROR_JSON_POINTER;
  }
  for (i = 0; path[i]; ++i) {
    if (path[i] == '/') ++cnt;
  }
  len = i;
  if (len > 1 && path[len - 1] == '/') {
    return JBL_ERROR_JSON_POINTER;
  }
  sz = sizeof(struct _JBLPTR) + cnt * sizeof(char *) + len;
  if (pool) {
    jp = iwpool_alloc(sz, pool);
  } else {
    jp = malloc(sz);
  }
  if (!jp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  jpr = (char *) jp;
  jp->pos = -1;
  jp->cnt = cnt;
  doff = offsetof(struct _JBLPTR, n) + cnt * sizeof(char *);
  assert(sz - doff >= len);

  for (i = 0, j = 0, cnt = 0; path[i] && cnt < jp->cnt; ++i, ++j) {
    if (path[i++] == '/') {
      jp->n[cnt] = jpr + doff + j;
      for (k = 0; ; ++i, ++k) {
        if (!path[i] || path[i] == '/') {
          --i;
          *(jp->n[cnt] + k) = '\0';
          break;
        }
        if (path[i] == '~') {
          if (path[i + 1] == '0') {
            *(jp->n[cnt] + k) = '~';
          } else if (path[i + 1] == '1') {
            *(jp->n[cnt] + k) = '/';
          }
          ++i;
        } else {
          *(jp->n[cnt] + k) = path[i];
        }
      }
      j += k;
      ++cnt;
    }
  }
  *jpp = jp;
  return rc;
}

iwrc _jbl_ptr_malloc(const char *path, JBLPTR *jpp) {
  return _jbl_ptr_pool(path, jpp, 0);
}

static iwrc jbl_visit(binn_iter *iter, int lvl, JBLVCTX *vctx, JBLVISITOR visitor) {
  iwrc rc = 0;
  binn *bn = vctx->bn;
  jbl_visitor_cmd_t cmd;
  int idx;
  binn bv;

  if (!iter) {
    binn_iter it;
    if (!BINN_IS_CONTAINER_TYPE(bn->type)) {
      return JBL_ERROR_INVALID;
    }
    if (!binn_iter_init(&it, bn, bn->type)) {
      return JBL_ERROR_INVALID;
    }
    rc = jbl_visit(&it, 0, vctx, visitor);
    return rc;
  }

  switch (iter->type) {
    case BINN_OBJECT: {
      char key[MAX_BIN_KEY_LEN + 1];
      while (!vctx->terminate && binn_object_next(iter, key, &bv)) {
        cmd = visitor(lvl, &bv, key, -1, vctx, &rc);
        RCRET(rc);
        if (cmd & JBL_VCMD_TERMINATE) {
          vctx->terminate = true;
          break;
        }
        if (!(cmd & JBL_VCMD_SKIP_NESTED) && BINN_IS_CONTAINER_TYPE(bv.type)) {
          binn_iter it;
          if (!binn_iter_init(&it, &bv, bv.type)) {
            return JBL_ERROR_INVALID;
          }
          rc = jbl_visit(&it, lvl + 1, vctx, visitor);
          RCRET(rc);
        }
      }
      break;
    }
    case BINN_MAP: {
      while (!vctx->terminate && binn_map_next(iter, &idx, &bv)) {
        cmd = visitor(lvl, &bv, 0, idx, vctx, &rc);
        RCRET(rc);
        if (cmd & JBL_VCMD_TERMINATE) {
          vctx->terminate = true;
          break;
        }
        if (!(cmd & JBL_VCMD_SKIP_NESTED) && BINN_IS_CONTAINER_TYPE(bv.type)) {
          binn_iter it;
          if (!binn_iter_init(&it, &bv, bv.type)) {
            return JBL_ERROR_INVALID;
          }
          rc = jbl_visit(&it, lvl + 1, vctx, visitor);
          RCRET(rc);
        }
      }
      break;
    }
    case BINN_LIST: {
      for (idx = 0; !vctx->terminate && binn_list_next(iter, &bv); ++idx) {
        cmd = visitor(lvl, &bv, 0, idx, vctx, &rc);
        RCRET(rc);
        if (cmd & JBL_VCMD_TERMINATE) {
          vctx->terminate = true;
          break;
        }
        if (!(cmd & JBL_VCMD_SKIP_NESTED) && BINN_IS_CONTAINER_TYPE(bv.type)) {
          binn_iter it;
          if (!binn_iter_init(&it, &bv, bv.type)) {
            return JBL_ERROR_INVALID;
          }
          rc = jbl_visit(&it, lvl + 1, vctx, visitor);
          RCRET(rc);
        }
      }
      break;
    }
  }
  return rc;
}

IW_INLINE bool _jbl_visitor_update_jptr_cursor(JBLPTR jp, int lvl, char *key, int idx) {
  if (lvl < jp->cnt) {
    if (jp->pos + 1 == lvl) {
      char *keyptr;
      char buf[JBNUMBUF_SIZE];
      if (key) {
        keyptr = key;
      } else {
        iwitoa(idx, buf, JBNUMBUF_SIZE);
        keyptr = buf;
      }
      if (!strcmp(keyptr, jp->n[lvl]) || (jp->n[lvl][0] == '*' && jp->n[lvl][1] == '\0')) {
        jp->pos = lvl;
        return (jp->cnt == lvl + 1);
      }
    }
  }
  return false;
}

static jbl_visitor_cmd_t _jbl_get_visitor(int lvl, binn *bv, char *key, int idx, JBLVCTX *vctx, iwrc *rc) {
  JBLPTR jp = vctx->op;
  assert(jp);
  if (_jbl_visitor_update_jptr_cursor(jp, lvl, key, idx)) { // Pointer matched
    JBL jbl = malloc(sizeof(struct _JBL));
    memcpy(&jbl->bn, bv, sizeof(*bv));
    vctx->result = jbl;
    return JBL_VCMD_TERMINATE;
  } else if (jp->cnt < lvl + 1)  {
    return JBL_VCMD_SKIP_NESTED;
  }
  return JBL_VCMD_OK;
}

iwrc jbl_at(JBL jbl, const char *path, JBL *res) {
  JBLPTR jp;
  iwrc rc = _jbl_ptr_malloc(path, &jp);
  RCRET(rc);
  JBLVCTX vctx = {
    .bn = &jbl->bn,
    .op = jp
  };
  rc = jbl_visit(0, 0, &vctx, _jbl_get_visitor);
  if (rc) {
    *res = 0;
  } else {
    if (!vctx.result) {
      rc = JBL_ERROR_PATH_NOTFOUND;
      *res = 0;
    } else {
      *res = (JBL) vctx.result;
    }
  }
  free(jp);
  return rc;
}

IW_INLINE void _jbl_reset_node_data(JBLNODE target) {
  memset(((uint8_t *) target) + offsetof(struct _JBLNODE, child),
         0,
         sizeof(struct _JBLNODE) - offsetof(struct _JBLNODE, child));
}

IW_INLINE void _jbl_copy_node_data(JBLNODE target, JBLNODE value) {
  memcpy(((uint8_t *) target) + offsetof(struct _JBLNODE, child),
         ((uint8_t *) value) + offsetof(struct _JBLNODE, child),
         sizeof(struct _JBLNODE) - offsetof(struct _JBLNODE, child));
}

static void _jbl_add_item(JBLNODE parent, JBLNODE node) {
  assert(parent && node);
  node->next = 0;
  if (parent->child) {
    JBLNODE prev = parent->child->prev;
    parent->child->prev = node;
    if (prev) {
      prev->next = node;
      node->prev = prev;
    } else {
      parent->child->next = node;
      node->prev = parent->child;
    }
  } else {
    parent->child = node;
  }
  if (parent->type == JBV_ARRAY) {
    if (node->prev) {
      node->klidx = node->prev->klidx + 1;
    } else {
      node->klidx = 0;
    }
  }
}

IW_INLINE void _jbl_remove_item(JBLNODE parent, JBLNODE child) {
  if (parent->child == child) {
    parent->child = child->next;
  }
  if (child->next) {
    child->next->prev = child->prev;
  }
  if (child->prev && child->prev->next) {
    child->prev->next = child->next;
  }
  child->next = 0;
  child->prev = 0;
  child->child = 0;
}

static iwrc _jbl_create_node(JBLDRCTX *ctx,
                             const binn *bv,
                             JBLNODE parent,
                             const char *key,
                             int klidx,
                             JBLNODE *node) {
  iwrc rc = 0;
  JBLNODE n = iwpool_alloc(sizeof(*n), ctx->pool);
  if (node) *node = 0;
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  memset(n, 0, sizeof(*n));
  n->key = key;
  n->klidx = klidx;
  switch (bv->type) {
    case BINN_NULL:
      n->type = JBV_NULL;
      break;
    case BINN_STRING:
      n->type = JBV_STR;
      n->vptr = bv->ptr;
      n->vsize = bv->size + 1;
      break;
    case BINN_OBJECT:
    case BINN_MAP:
      n->type = JBV_OBJECT;
      n->vptr = bv->ptr;
      n->vsize = bv->size;
      break;
    case BINN_LIST:
      n->type = JBV_ARRAY;
      n->vptr = bv->ptr;
      n->vsize = bv->size;
      break;
    case BINN_TRUE:
      n->type = JBV_BOOL;
      n->vbool = true;
      break;
    case BINN_FALSE:
      n->type = JBV_BOOL;
      n->vbool = false;
      break;
    case BINN_BOOL:
      n->type = JBV_BOOL;
      n->vbool = bv->vbool;
      break;
    case BINN_UINT8:
      n->vi64 = bv->vuint8;
      n->type = JBV_I64;
      break;
    case BINN_UINT16:
      n->vi64 = bv->vuint16;
      n->type = JBV_I64;
      break;
    case BINN_UINT32:
      n->vi64 = bv->vuint32;
      n->type = JBV_I64;
      break;
    case BINN_UINT64:
      n->vi64 = bv->vuint64;
      n->type = JBV_I64;
      break;
    case BINN_INT8:
      n->vi64 = bv->vint8;
      n->type = JBV_I64;
      break;
    case BINN_INT16:
      n->vi64 = bv->vint16;
      n->type = JBV_I64;
      break;
    case BINN_INT32:
      n->vi64 = bv->vint32;
      n->type = JBV_I64;
      break;
    case BINN_INT64:
      n->vi64 = bv->vint64;
      n->type = JBV_I64;
      break;
    case BINN_FLOAT32:
    case BINN_FLOAT64:
      n->vf64 = bv->vdouble;
      n->type = JBV_F64;
      break;
    default:
      rc = JBL_ERROR_CREATION;
      goto finish;
  }
  if (parent) {
    _jbl_add_item(parent, n);
  }

finish:
  if (rc) {
    free(n);
  } else {
    if (node) *node = n;
  }
  return rc;
}

static iwrc _jbl_node_from_binn(JBLDRCTX *ctx, const binn *bn, JBLNODE parent, char *key, int klidx) {
  binn bv;
  binn_iter iter;
  iwrc rc = 0;

  switch (bn->type) {
    case BINN_OBJECT:
    case BINN_MAP:
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, &parent);
      RCRET(rc);
      if (!ctx->root) {
        ctx->root = parent;
      }
      if (!binn_iter_init(&iter, (binn *) bn, bn->type)) {
        return JBL_ERROR_INVALID;
      }
      if (bn->type == BINN_OBJECT) {
        for (int i = 0; binn_object_next2(&iter, &key, &klidx, &bv); ++i) {
          rc = _jbl_node_from_binn(ctx, &bv, parent, key, klidx);
          RCRET(rc);
        }
      } else if (bn->type == BINN_MAP) {
        for (int i = 0; binn_map_next(&iter, &klidx, &bv); ++i) {
          rc = _jbl_node_from_binn(ctx, &bv, parent, 0, klidx);
          RCRET(rc);
        }
      }
      break;
    case BINN_LIST:
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, &parent);
      RCRET(rc);
      if (!ctx->root) {
        ctx->root = parent;
      }
      if (!binn_iter_init(&iter, (binn *) bn, bn->type)) {
        return JBL_ERROR_INVALID;
      }
      for (int i = 0; binn_list_next(&iter, &bv); ++i) {
        rc = _jbl_node_from_binn(ctx, &bv, parent, 0, i);
        RCRET(rc);
      }
      break;
    default: {
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, 0);
      RCRET(rc);
      break;
    }
  }
  return rc;
}

static iwrc _jbl_node_from_binn2(const binn *bn, JBLNODE *node, IWPOOL *pool) {
  JBLDRCTX ctx = {
    .pool = pool
  };
  iwrc rc = _jbl_node_from_binn(&ctx, bn, 0, 0, -1);
  if (rc) {
    *node = 0;
  } else {
    *node = ctx.root;
  }
  return rc;
}

static iwrc _jbl_node_from_patch(const JBLPATCH *p, JBLNODE *node, IWPOOL *pool) {
  iwrc rc = 0;
  *node = 0;
  JBL vjbl = 0;
  jbl_type_t type = p->type;

  if (p->vjson) { // JSON string specified as value
    rc = jbl_from_json(&vjbl, p->vjson);
    RCRET(rc);
    type = jbl_type(vjbl);
  }
  if (type == JBV_OBJECT || type == JBV_ARRAY) {
    *node = p->vnode;
  } else {
    JBLNODE n = iwpool_alloc(sizeof(*n), pool);
    if (!n) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    memset(n, 0, sizeof(*n));
    n->type = type;
    switch (n->type) {
      case JBV_STR:
        n->vptr = p->vptr;
        n->vsize = p->vsize;
        break;
      case JBV_I64:
        n->vi64 = p->vi64;
        break;
      case JBV_F64:
        n->vf64 = p->vf64;
        break;
      case JBV_BOOL:
        n->vbool = p->vbool;
        break;
      case JBV_NONE:
      case JBV_NULL:
        break;
      default:
        rc = JBL_ERROR_INVALID;
        break;
    }
    if (!rc) {
      *node = n;
    }
  }
  return rc;
}

static JBLNODE _jbl_node_find(const JBLNODE node, const JBLPTR ptr, int from, int to) {
  if (!ptr || !node) return 0;
  JBLNODE n = node;

  for (int i = from; n && i < ptr->cnt && i < to; ++i) {
    if (n->type == JBV_OBJECT) {
      for (n = n->child; n; n = n->next) {
        if (!strncmp(n->key, ptr->n[i], n->klidx) && strlen(ptr->n[i]) == n->klidx) {
          break;
        }
      }
    } else if (n->type == JBV_ARRAY) {
      int64_t idx = iwatoi(ptr->n[i]);
      for (n = n->child; n; n = n->next) {
        if (idx == n->klidx) {
          break;
        }
      }
    } else {
      return 0;
    }
  }
  return n;
}

IW_INLINE JBLNODE _jbl_node_find2(const JBLNODE node, const JBLPTR ptr) {
  if (!node || !ptr || !ptr->cnt) return 0;
  return _jbl_node_find(node, ptr, 0, ptr->cnt - 1);
}

static JBLNODE _jbl_node_detach(JBLNODE target, const JBLPTR path) {
  if (!path) {
    return 0;
  }
  JBLNODE parent = (path->cnt > 1) ? _jbl_node_find(target, path, 0, path->cnt - 1) : target;
  if (!parent) {
    return 0;
  }
  JBLNODE child = _jbl_node_find(parent, path, path->cnt - 1, path->cnt);
  if (!child) {
    return 0;
  }
  _jbl_remove_item(parent, child);
  return child;
}

static int _jbl_cmp_node_keys(const void *o1, const void *o2) {
  JBLNODE n1 = *((const JBLNODE *) o1);
  JBLNODE n2 = *((const JBLNODE *) o2);
  if (!n1 && !n2) {
    return 0;
  }
  if (!n2 || n1->klidx > n2->klidx) {
    return 1;
  } else if (!n1 || n1->klidx < n2->klidx) {
    return -1;
  }
  return strncmp(n1->key, n2->key, n1->klidx);
}

static int _jbl_node_count(JBLNODE n) {
  int ret = 0;
  n = n->child;
  while (n) {
    ret++;
    n = n->next;
  }
  return ret;
}

static int _jbl_compare_nodes(JBLNODE n1, JBLNODE n2, iwrc *rcp);

static int _jbl_compare_objects(JBLNODE n1, JBLNODE n2, iwrc *rcp) {
  int ret = 0;
  int cnt = _jbl_node_count(n1);
  int i = _jbl_node_count(n2);
  if (cnt > i) {
    return 1;
  } else if (cnt < i) {
    return -1;
  } else if (cnt == 0) {
    return 0;
  }
  JBLNODE *s1 = malloc(2 * sizeof(JBLNODE) * cnt);
  if (!s1) {
    *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return 0;
  }
  JBLNODE *s2 = s1 + cnt;

  i = 0;
  n1 = n1->child;
  n2 = n2->child;
  while (n1 && n2) {
    s1[i] = n1;
    s2[i] = n2;
    n1 = n1->next;
    n2 = n2->next;
    ++i;
  }
  qsort(s1, cnt, sizeof(JBLNODE), _jbl_cmp_node_keys);
  qsort(s2, cnt, sizeof(JBLNODE), _jbl_cmp_node_keys);
  for (i = 0; i < cnt; ++i) {
    ret = _jbl_cmp_node_keys(s1 + i, s2 + i);
    if (ret) {
      goto finish;
    }
    ret = _jbl_compare_nodes(s1[i], s2[i], rcp);
    if (*rcp || ret) {
      goto finish;
    }
  }

finish:
  free(s1);
  return ret;
}

static int _jbl_compare_nodes(JBLNODE n1, JBLNODE n2, iwrc *rcp) {
  if (!n1 && !n2) {
    return 0;
  } else if (!n1) {
    return -1;
  } else if (!n2) {
    return 1;
  } else if (n1->type - n2->type) {
    return n1->type - n2->type;
  }
  switch (n1->type) {
    case JBV_BOOL:
      return n1->vbool - n2->vbool;
    case JBV_I64:
      return n1->vi64 - n2->vi64;
    case JBV_F64:
      return n1->vf64 - n2->vf64;
    case JBV_STR:
      if (n1->vsize - n2->vsize) {
        return n1->vsize - n2->vsize;
      }
      return strncmp(n1->vptr, n2->vptr, n1->vsize);
    case JBV_ARRAY:
      for (n1 = n1->child, n2 = n2->child; n1 && n2; n1 = n1->next, n2 = n2->next) {
        int res = _jbl_compare_nodes(n1, n2, rcp);
        if (res) {
          return res;
        }
      }
      if (n1) {
        return 1;
      } else if (n2) {
        return -1;
      } else {
        return 0;
      }
    case JBV_OBJECT:
      return _jbl_compare_objects(n1, n2, rcp);
    case JBV_NULL:
    case JBV_NONE:
      break;
  }
  return 0;
}

static iwrc _jbl_target_apply_patch(JBLNODE target, const JBLPATCHEXT *ex) {

  jbp_patch_t op = ex->p->op;
  JBLPTR path = ex->path;
  JBLNODE value = ex->value;
  bool oproot = ex->path->cnt == 1 && *ex->path->n[0] == '\0';

  if (op == JBP_TEST) {
    iwrc rc = 0;
    if (!value) {
      return JBL_ERROR_PATCH_NOVALUE;
    }
    if (_jbl_compare_nodes(oproot ? target : _jbl_node_find(target, path, 0, path->cnt), value, &rc)) {
      RCRET(rc);
      return JBL_ERROR_PATCH_TEST_FAILED;
    } else {
      return rc;
    }
  }
  if (oproot) { // Root operation
    if (op == JBP_REMOVE) {
      memset(target, 0, sizeof(*target));
    } else if (op == JBP_REPLACE || op == JBP_ADD) {
      if (!value) {
        return JBL_ERROR_PATCH_NOVALUE;
      }
      memmove(target, value, sizeof(*value));
    }
  } else { // Not a root
    if (op == JBP_REMOVE || op == JBP_REPLACE) {
      _jbl_node_detach(target, ex->path);
    }
    if (op == JBP_REMOVE) {
      return 0;
    } else if (op == JBP_MOVE || op == JBP_COPY) {
      if (op == JBP_MOVE) {
        value = _jbl_node_detach(target, ex->from);
      } else {
        value = _jbl_node_find2(target, ex->from);
      }
      if (!value) {
        return JBL_ERROR_PATH_NOTFOUND;
      }
    } else { // ADD/REPLACE
      if (!value) {
        return JBL_ERROR_PATCH_NOVALUE;
      }
    }
    int lastidx = path->cnt - 1;
    JBLNODE parent = (path->cnt > 1) ? _jbl_node_find(target, path, 0, lastidx) : target;
    if (!parent) {
      return JBL_ERROR_PATCH_TARGET_INVALID;
    }
    if (parent->type == JBV_ARRAY) {
      if (path->n[lastidx][0] == '-' && path->n[lastidx][1] == '\0') {
        _jbl_add_item(parent, value); // Add to end of array
      } else { // Insert into the specified index
        int idx = iwatoi(path->n[lastidx]);
        int cnt = idx;
        JBLNODE child = parent->child;
        while (child && cnt > 0) {
          cnt--;
          child = child->next;
        }
        if (cnt > 0) {
          return JBL_ERROR_PATCH_INVALID_ARRAY_INDEX;
        }
        value->klidx = idx;
        if (child) {
          value->next = child;
          value->prev = child->prev;
          child->prev = value;
          if (child == parent->child) {
            parent->child = value;
          } else {
            value->prev->next = value;
          }
          while (child) {
            child->klidx++;
            child = child->next;
          }
        } else {
          _jbl_add_item(parent, value);
        }
      }
    } else if (parent->type == JBV_OBJECT) {
      JBLNODE child = _jbl_node_find(parent, path, path->cnt - 1, path->cnt);
      if (child) {
        _jbl_copy_node_data(child, value);
      } else {
        value->key = path->n[path->cnt - 1];
        value->klidx = strlen(value->key);
        _jbl_add_item(parent, value);
      }
    } else {
      return JBL_ERROR_PATCH_TARGET_INVALID;
    }
  }
  return 0;
}

static iwrc _jbl_from_node(binn *res, JBLNODE node, int size) {
  iwrc rc = 0;
  switch (node->type) {
    case JBV_OBJECT:
      if (!binn_create(res, BINN_OBJECT, 0, NULL)) {
        return JBL_ERROR_CREATION;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        binn bv;
        rc = _jbl_from_node(&bv, n, 0);
        RCRET(rc);
        if (!binn_object_set_value2(res, n->key, n->klidx, &bv)) {
          rc = JBL_ERROR_CREATION;
        }
        binn_free(&bv);
        RCRET(rc);
      }
      break;
    case JBV_ARRAY:
      if (!binn_create(res, BINN_LIST, 0, NULL)) {
        return JBL_ERROR_CREATION;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        binn bv;
        rc = _jbl_from_node(&bv, n, 0);
        RCRET(rc);
        if (!binn_list_add_value(res, &bv)) {
          rc = JBL_ERROR_CREATION;
        }
        binn_free(&bv);
        RCRET(rc);
      }
      break;
    case JBV_STR:
      binn_init_item(res);
      binn_set_string(res, (void *) node->vptr, 0);
      break;
    case JBV_I64:
      binn_init_item(res);
      binn_set_int64(res, node->vi64);
      break;
    case JBV_F64:
      binn_init_item(res);
      binn_set_double(res, node->vf64);
      break;
    case JBV_BOOL:
      binn_set_bool(res, node->vbool);
      break;
    case JBV_NULL:
      binn_init_item(res);
      binn_set_null(res);
      break;
    case JBV_NONE:
      rc = JBL_ERROR_CREATION;
      break;
  }
  return rc;
}

static iwrc _jbl_patch_node(JBLNODE root, const JBLPATCH *p, size_t cnt, IWPOOL *pool) {
  if (cnt < 1) return 0;
  if (!root || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  size_t i = 0;
  JBLPATCHEXT parr[cnt];
  memset(parr, 0, cnt * sizeof(JBLPATCHEXT));
  for (i = 0; i < cnt; ++i) {
    JBLPATCHEXT *ext = &parr[i];
    ext->p = &p[i];
    rc = _jbl_ptr_pool(p[i].path, &ext->path, pool);
    RCRET(rc);
    if (p[i].from) {
      rc = _jbl_ptr_pool(p[i].from, &ext->from, pool);
      RCRET(rc);
    }
    rc = _jbl_node_from_patch(ext->p, &ext->value, pool);
    RCRET(rc);
  }
  for (i = 0; i < cnt; ++i) {
    rc = _jbl_target_apply_patch(root, &parr[i]);
    RCRET(rc);
  }
  return rc;
}

static iwrc _jbl_patch(JBL jbl, const JBLPATCH *p, size_t cnt, IWPOOL *pool) {
  if (cnt < 1) return 0;
  if (!jbl || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  binn bv;
  binn *bn;
  JBLNODE root;
  iwrc rc = _jbl_node_from_binn2(&jbl->bn, &root, pool);
  RCRET(rc);
  rc = _jbl_patch_node(root, p, cnt, pool);
  RCRET(rc);
  if (root->type != JBV_NONE) {
    rc = _jbl_from_node(&bv, root, MAX(jbl->bn.size, 256));
    RCRET(rc);
    bn = &bv;
  } else {
    bn = 0;
  }
  binn_free(&jbl->bn);
  if (bn) {
    if (bn->writable && bn->dirty) {
      binn_save_header(bn);
    }
    memcpy(&jbl->bn, bn, sizeof(jbl->bn));
    jbl->bn.allocated = 0;
  } else {
    memset(&jbl->bn, 0, sizeof(jbl->bn));
    root->type = JBV_NONE;
  }
  return rc;
}

static iwrc _jbl_node_from_json(nx_json *nxjson, JBLNODE parent, const char *key, int idx, JBLNODE *node, IWPOOL *pool) {
  iwrc rc = 0;
  JBLNODE n = iwpool_alloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  memset(n, 0, sizeof(*n));
  n->key = key;
  n->klidx = n->key ? strlen(n->key) : idx;

  switch (nxjson->type) {
    case NX_JSON_OBJECT:
      n->type = JBV_OBJECT;
      for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
        rc = _jbl_node_from_json(nxj, n, nxj->key, 0, 0, pool);
        RCGO(rc, finish);
      }
      break;
    case NX_JSON_ARRAY:
      n->type = JBV_ARRAY;
      int i = 0;
      for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
        rc = _jbl_node_from_json(nxj, n, 0, i++, 0, pool);
        RCGO(rc, finish);
      }
      break;
    case NX_JSON_STRING:
      n->type = JBV_STR;
      n->vsize = strlen(nxjson->text_value) + 1;
      n->vptr = iwpool_strndup(pool, nxjson->text_value, n->vsize, &rc);
      RCGO(rc, finish);
      break;
    case NX_JSON_INTEGER:
      n->type = JBV_I64;
      n->vi64 = nxjson->int_value;
      break;
    case NX_JSON_DOUBLE:
      n->type = JBV_F64;
      n->vf64 = nxjson->dbl_value;
      break;
    case NX_JSON_BOOL:
      n->type = JBV_BOOL;
      n->vbool = nxjson->int_value;
      break;
    case NX_JSON_NULL:
      n->type = JBV_NULL;
      break;
    default:
      break;
  }

finish:
  if (!rc) {
    if (parent) {
      _jbl_add_item(parent, n);
    }
    if (node) {
      *node = n;
    }
  } else if (node) {
    *node = 0;
  }
  return rc;
}

// --------------------------- Public API

iwrc jbl_to_node(JBL jbl, JBLNODE *node, IWPOOL *pool) {
  return _jbl_node_from_binn2(&jbl->bn, node, pool);
}

iwrc jbl_patch_node(JBLNODE root, const JBLPATCH *p, size_t cnt) {
  IWPOOL *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_patch_node(root, p, cnt, pool);
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_patch(JBL jbl, const JBLPATCH *p, size_t cnt) {
  if (cnt < 1) return 0;
  if (!jbl || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  IWPOOL *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_patch(jbl, p, cnt, pool);
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_patch_from_json(JBL jbl, const char *patchjson) {
  binn bv;
  iwrc rc = 0;
  int cnt = 0;
  JBLPATCH *p = 0;
  IWPOOL *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }

  int i = strlen(patchjson);
  nx_json *nxjson = 0;
  char *json = iwpool_alloc(i + 1, pool);
  if (!json) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  memcpy(json, patchjson, i + 1);

  nxjson = nx_json_parse_utf8(json);
  if (!nxjson) {
    rc = JBL_ERROR_PARSE_JSON;
    goto finish;
  }
  if (nxjson->type != NX_JSON_ARRAY) {
    rc = JBL_ERROR_PATCH_INVALID;
    goto finish;
  }
  for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
    if (nxj->type != NX_JSON_OBJECT) {
      rc = JBL_ERROR_PATCH_INVALID;
      goto finish;
    }
    cnt++;
  }

  p = iwpool_alloc(cnt * sizeof(*p), pool);
  if (!p) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  memset(p, 0, cnt * sizeof(*p));

  i = 0;
  for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next, ++i) {
    JBLPATCH *pp = p + i;
    for (nx_json *nxj2 = nxj->child; nxj2; nxj2 = nxj2->next) {
      if (!strcmp("op", nxj2->key)) {
        if (nxj2->type != NX_JSON_STRING) {
          rc = JBL_ERROR_PATCH_INVALID;
          goto finish;
        }
        const char *v = nxj2->text_value;
        if (!strcmp("add", v)) {
          pp->op = JBP_ADD;
        } else if (!strcmp("remove", v)) {
          pp->op = JBP_REMOVE;
        } else if (!strcmp("replace", v)) {
          pp->op = JBP_REPLACE;
        } else if (!strcmp("copy", v)) {
          pp->op = JBP_COPY;
        } else if (!strcmp("move", v)) {
          pp->op = JBP_MOVE;
        } else if (!strcmp("test", v)) {
          pp->op = JBP_TEST;
        } else {
          rc = JBL_ERROR_PATCH_INVALID_OP;
          goto finish;
        }
      } else if (!strcmp("value", nxj2->key)) {
        switch (nxj2->type) {
          case NX_JSON_STRING:
            pp->type = JBV_STR;
            pp->vptr = nxj2->text_value;
            pp->vsize = strlen(nxj2->text_value) + 1;
            break;
          case NX_JSON_INTEGER:
            pp->type = JBV_I64;
            pp->vi64 = nxj2->int_value;
            break;
          case NX_JSON_OBJECT:
          case NX_JSON_ARRAY: {
            pp->type = (nxj2->type == NX_JSON_ARRAY) ? JBV_ARRAY : JBV_OBJECT;
            rc = _jbl_node_from_json(nxj2, 0, 0, 0, &pp->vnode, pool);
            break;
          }
          case NX_JSON_BOOL:
            pp->type = JBV_BOOL;
            pp->vbool = nxj2->int_value;
            break;
          case NX_JSON_NULL:
            pp->type = JBV_NULL;
            break;
          case NX_JSON_DOUBLE:
            pp->type = JBV_F64;
            pp->vf64 = nxj2->dbl_value;
            break;
          default:
            rc = JBL_ERROR_PARSE_JSON;
            break;
        }
      } else if (!strcmp("path", nxj2->key)) {
        if (nxj2->type != NX_JSON_STRING) {
          rc = JBL_ERROR_PATCH_INVALID;
          goto finish;
        }
        pp->path = nxj2->text_value;
      } else if (!strcmp("from", nxj2->key)) {
        if (nxj2->type != NX_JSON_STRING) {
          rc = JBL_ERROR_PATCH_INVALID;
          goto finish;
        }
        pp->from = nxj2->text_value;
      }
    }
  }
  rc = _jbl_patch(jbl, p, cnt, pool);

finish:
  if (nxjson) {
    nx_json_free(nxjson);
  }
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_from_node(JBL jbl, JBLNODE node) {
  if (!jbl || !node) {
    return IW_ERROR_INVALID_ARGS;
  }
  if (node->type == JBV_NONE) {
    memset(&jbl->bn, 0, sizeof(jbl->bn));
    return 0;
  }
  binn bv;
  iwrc rc = _jbl_from_node(&bv, node, 0);
  RCRET(rc);
  if (bv.writable && bv.dirty) {
    binn_save_header(&bv);
  }
  binn_free(&jbl->bn);
  memcpy(&jbl->bn, &bv, sizeof(jbl->bn));
  jbl->bn.allocated = 0;
  return rc;
}

iwrc jbl_json_to_node(const char *_json, JBLNODE *node, IWPOOL *pool) {
  if (!_json || !node || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc;
  char *json = iwpool_strdup(pool, _json, &rc);
  RCRET(rc);
  nx_json *nxj = nx_json_parse_utf8(json);
  if (!nxj) {
    rc = JBL_ERROR_PARSE_JSON;
    goto finish;
  }
  rc = _jbl_node_from_json(nxj, 0, 0, 0, node, pool);
finish:
  if (nxj) {
    nx_json_free(nxj);
  }
  return rc;
}

static JBLNODE _jbl_merge_patch_node(JBLNODE target, JBLNODE patch, IWPOOL *pool, iwrc *rcp) {
  *rcp = 0;
  if (!patch) {
    return 0;
  }
  if (patch->type == JBV_OBJECT) {
    if (!target) {
       target = iwpool_alloc(sizeof(*target), pool);
       if (!target) {
         *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
       }
       memset(target, 0, sizeof(*target));
       target->type = JBV_OBJECT;
       target->key = patch->key;
       target->klidx = patch->klidx;
    } else if (target->type != JBV_OBJECT) {
      _jbl_reset_node_data(target);
      target->type = JBV_OBJECT;
    }
    patch = patch->child;
    while (patch) {
      if (patch->type == JBV_NULL) {
        JBLNODE node = target->child;
        while (node) {
          if (node->klidx == patch->klidx && !strncmp(node->key, patch->key, node->klidx)) {
            _jbl_remove_item(target, node);
            break;
          }
          node = node->next;
        }
      } else {
        JBLNODE node = target->child;
        while (node) {
          if (node->klidx == patch->klidx && !strncmp(node->key, patch->key, node->klidx)) {
            _jbl_copy_node_data(node, _jbl_merge_patch_node(node, patch, pool, rcp));
            break;
          }
          node = node->next;
        }
        if (!node) {
           _jbl_add_item(target, _jbl_merge_patch_node(0, patch, pool, rcp));
        }
      }
      patch = patch->next;
    }
    return target;
  } else {
    return patch;
  }
}

iwrc jbl_merge_patch_node(JBLNODE root, const char *patchjson, IWPOOL *pool) {
  if (!root || !patchjson || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBLNODE patch, res;
  nx_json *nxjson;
  int i = strlen(patchjson);
  char *json = iwpool_strndup(pool, patchjson, i, &rc);
  RCRET(rc);
  nxjson = nx_json_parse_utf8(json);
  if (!nxjson) {
    rc = JBL_ERROR_PARSE_JSON;
    goto finish;
  }
  rc = _jbl_node_from_json(nxjson, 0, 0, 0, &patch, pool);
  RCGO(rc, finish);
  res = _jbl_merge_patch_node(root, patch, pool, &rc);
  RCGO(rc, finish);
  if (res != root) {
    memcpy(root, res, sizeof(*root));
  }

finish:
  if (nxjson) {
    nx_json_free(nxjson);
  }
  return rc;
}

iwrc jbl_merge_patch(JBL jbl, const char *patchjson) {
  if (!jbl || !patchjson) {
    return IW_ERROR_INVALID_ARGS;
  }
  binn bv;
  JBLNODE target;
  IWPOOL *pool = iwpool_create(1024);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_node_from_binn2(&jbl->bn, &target, pool);
  RCGO(rc, finish);
  rc = jbl_merge_patch_node(target, patchjson, pool);
  RCGO(rc, finish);

  rc = _jbl_from_node(&bv, target, 0);
  RCGO(rc, finish);
  if (bv.writable && bv.dirty) {
    binn_save_header(&bv);
  }
  binn_free(&jbl->bn);
  memcpy(&jbl->bn, &bv, sizeof(jbl->bn));
  jbl->bn.allocated = 0;

finish:
  iwpool_destroy(pool);
  return 0;
}

static const char *_jbl_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JBL_ERROR_START && ecode < _JBL_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case JBL_ERROR_INVALID_BUFFER:
      return "Invalid JBL buffer (JBL_ERROR_INVALID_BUFFER)";
    case JBL_ERROR_CREATION:
      return "Cannot create JBL object (JBL_ERROR_CREATION)";
    case JBL_ERROR_INVALID:
      return "Invalid JBL object (JBL_ERROR_INVALID)";
    case JBL_ERROR_PARSE_JSON:
      return "Failed to parse JSON string (JBL_ERROR_PARSE_JSON)";
    case JBL_ERROR_PARSE_UNQUOTED_STRING:
      return "Unquoted JSON string (JBL_ERROR_PARSE_UNQUOTED_STRING)";
    case JBL_ERROR_PARSE_INVALID_CODEPOINT:
      return "Invalid unicode codepoint/escape sequence (JBL_ERROR_PARSE_INVALID_CODEPOINT)";
    case JBL_ERROR_JSON_POINTER:
      return "Invalid JSON pointer (rfc6901) path (JBL_ERROR_JSON_POINTER)";
    case JBL_ERROR_PATH_NOTFOUND:
      return "JSON object not matched the path specified (JBL_ERROR_PATH_NOTFOUND)";
    case JBL_ERROR_PATCH_INVALID:
      return "Invalid JSON patch specified (JBL_ERROR_PATCH_INVALID)";
    case JBL_ERROR_PATCH_INVALID_OP:
      return "Invalid JSON patch operation specified (JBL_ERROR_PATCH_INVALID_OP)";
    case JBL_ERROR_PATCH_NOVALUE:
      return "No value specified in JSON patch (JBL_ERROR_PATCH_NOVALUE)";
    case JBL_ERROR_PATCH_TARGET_INVALID:
      return "Could not find target object to set value (JBL_ERROR_PATCH_TARGET_INVALID)";
    case JBL_ERROR_PATCH_INVALID_ARRAY_INDEX:
      return "Invalid array index in JSON patch path (JBL_ERROR_PATCH_INVALID_ARRAY_INDEX)";
    case JBL_ERROR_PATCH_TEST_FAILED:
      return "JSON patch test operation failed (JBL_ERROR_PATCH_TEST_FAILED)";
  }
  return 0;
}

iwrc jbl_init() {
  static int _jbl_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbl_initialized, 0, 1)) {
    return 0;
  }
  return iwlog_register_ecodefn(_jbl_ecodefn);
}
