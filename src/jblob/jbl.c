#include "jbl.h"
#include <iowow/iwconv.h>
#include "binn.h"
#include "nxjson.h"
#include "ejdb2cfg.h"
#include <ctype.h>

#define NUMBUF_SIZE 64
IW_EXPORT int iwitoa(int64_t v, char *buf, int max);

struct _JBL {
  binn bn;
};

iwrc jbl_create_object(JBL *jblp) {
  iwrc rc = 0;
  *jblp = malloc(sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl = *jblp;
  if (!binn_create(&jbl->bn, BINN_OBJECT, 0, 0)) {
    free(jbl);
    *jblp = 0;
    return JBL_ERROR_CREATION;
  }
  return rc;
}

iwrc jbl_create_array(JBL *jblp) {
  iwrc rc = 0;
  *jblp = malloc(sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl = *jblp;
  if (!binn_create(&jbl->bn, BINN_LIST, 0, 0)) {
    free(jbl);
    *jblp = 0;
    return JBL_ERROR_CREATION;
  }
  return rc;
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

size_t jbl_count(JBL jbl) {
  return jbl->bn.count;
}

size_t jbl_size(JBL jbl) {
  return jbl->bn.size;
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
    case BINN_TRUE:
    case BINN_FALSE:
      return JBV_BOOL;
    case BINN_INT32:
    case BINN_UINT16:
    case BINN_INT16:
    case BINN_UINT8:
    case BINN_INT8:
      return JBV_I32;
    case BINN_INT64:
    case BINN_UINT64: // overflow?
    case BINN_UINT32:
      return JBV_I64;
    case BINN_FLOAT32:
    case BINN_FLOAT64:
      return JBV_F64;
  }
  return JBV_NONE;
}

static binn *_jbl_from_json(nx_json *nxjson, iwrc *rcp) {
  binn  *res = 0;
  switch (nxjson->type) {
    case NX_JSON_OBJECT:
      res = binn_object();
      if (!res) {
        *rcp = JBL_ERROR_CREATION;
        return 0;
      }
      for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
        if (!binn_object_set_new(res, nxj->key, _jbl_from_json(nxj, rcp))) {
          if (!*rcp) {
            *rcp = JBL_ERROR_CREATION;
          }
          binn_free(res);
          return 0;
        }
      }
      return res;
    case NX_JSON_ARRAY:
      res = binn_list();
      if (!res) {
        *rcp = JBL_ERROR_CREATION;
        return 0;
      }
      for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
        if (!binn_list_add_new(res, _jbl_from_json(nxj, rcp))) {
          if (!*rcp) {
            *rcp = JBL_ERROR_CREATION;
          }
          binn_free(res);
          return 0;
        }
      }
      return res;
    case NX_JSON_STRING:
      return binn_string(nxjson->text_value, BINN_TRANSIENT);
    case NX_JSON_INTEGER:
      if (nxjson->int_value <= INT_MAX && nxjson->int_value >= INT_MIN) {
        return binn_int32(nxjson->int_value);
      } else {
        return binn_int64(nxjson->int_value);
      }
    case NX_JSON_DOUBLE:
      return binn_double(nxjson->dbl_value);
    case NX_JSON_BOOL:
      return binn_bool(nxjson->int_value);
    case NX_JSON_NULL:
      return binn_null();
  }
  return 0;
}

iwrc jbl_from_json(JBL *jblp, const char *jsonstr) {
  iwrc rc = 0;
  *jblp = 0;
  char *json = strdup(jsonstr); //nxjson uses inplace data modification
  if (!json) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl = 0;
  binn *bn = 0;
  nx_json *nxjson = nx_json_parse_utf8(json);
  if (!nxjson) {
    rc = JBL_ERROR_PARSE_JSON;
    goto finish;
  }
  bn = _jbl_from_json(nxjson, &rc);
  RCGO(rc, finish);
  assert(bn);
  
  jbl = malloc(sizeof(*jbl));
  if (!jbl) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  memcpy(&jbl->bn, bn, sizeof(*bn));
  jbl->bn.allocated = 0;
  free(bn);
  
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
  char buf[NUMBUF_SIZE];
  int sz = iwftoa(num, buf, sizeof(buf), 10);
  return pt(buf, sz, 0, 0, op);
}

static iwrc _jbl_write_int(int64_t num, jbl_json_printer pt, void *op) {
  char buf[NUMBUF_SIZE];
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
    rc = pt((const char*) data_, size_, ch_, count_, op);\
    RCGO(rc, finish); \
  } while(0)
  
  for (size_t i = 0; i < len; i++) {
    size_t clen;
    uint8_t ch = p[i];
    if (ch == '"' || ch == '\\') {
      PT(0, 0, '\\', 1);
      PT(p + i, 1, 0, 0);
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
  char key[MAX_BIN_KEY_LEN];
  
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
      if (pretty) {
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
      
    case BINN_INT64:
      llv = bn->vint64;
      goto loc_int;
    case BINN_UINT32:
      llv = bn->vuint32;
      goto loc_int;
    case BINN_INT32:
      llv = bn->vint32;
      goto loc_int;
    case BINN_UINT16:
      llv = bn->vuint16;
      goto loc_int;
    case BINN_INT16:
      llv = bn->vint16;
      goto loc_int;
    case BINN_UINT8:
      llv = bn->vuint8;
      goto loc_int;
    case BINN_INT8:
      llv = bn->vint8;
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
