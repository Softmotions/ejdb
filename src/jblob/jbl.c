#include "jbl.h"
#include "binn.h"
#include "nxjson.h"
#include "ejdb2cfg.h"

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
        if (!binn_object_set_new(res, nxjson->key, _jbl_from_json(nxj, rcp))) {
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

static void _jbl_escape_json_string(char *str, size_t len, jbl_json_printer pt, void *op) {
  // TODO:
}

// case BINN_NULL:
//     return JBV_NULL;
//   case BINN_STRING:
//     return JBV_STR;
//   case BINN_OBJECT:
//     return JBV_OBJECT;
//   case BINN_LIST:
//     return JBV_ARRAY;
//   case BINN_TRUE:
//   case BINN_FALSE:
//     return JBV_BOOL;
//   case BINN_INT32:
//   case BINN_UINT16:
//   case BINN_INT16:
//   case BINN_UINT8:
//   case BINN_INT8:
//     return JBV_I32;
//   case BINN_INT64:
//   case BINN_UINT64: // overflow?
//   case BINN_UINT32:
//     return JBV_I64;
//   case BINN_FLOAT32:
//   case BINN_FLOAT64:
//     return JBV_F64;

static iwrc _jbl_as_json(binn *bn, jbl_json_printer pt, void *op, int lvl, bool pretty) {
  iwrc rc = 0;
  binn bv;
#define PT(data_, size_, ch_, count_, op_) do {\
    rc = pt(data_, size_, ch_, count_, op_); \
    RCGO(rc, finish); \
  } while(0)

  switch (bn->type) {

    case BINN_LIST:
      PT(0, 0, '[', 1, op);
      if (bn->count && pretty) {
        PT(0, 0, '\n', 1, op);
      }
      for (int i = 0; i < bn->count; ++i) {
        if (pretty) {
          PT(0, 0, ' ', lvl + 1, op);
        }
        if (!binn_list_get_value(bn, i, &bv)) {
          rc = JBL_ERROR_INVALID;
          goto finish;
        }
        rc = _jbl_as_json(&bv, pt, op, lvl + 1, pretty);
        RCGO(rc, finish);
        if (i < bn->count - 1) {
          PT(0, 0, ',', 1, op);
        }
        if (pretty) {
          PT(0, 0, '\n', 1, op);
        }
      }
      if (bn->count && pretty) {
        PT(0, 0, ' ', lvl, op);
      }
      PT(0, 0, ']', 1, op);
      break;

    case JBV_OBJECT:
      PT(0, 0, '{', 1, op);
      if (pretty) {
        PT(0, 0, '\n', 1, op);
      }
      for (int i = 0; i < bn->count; ++i) {
        //TODO:
      }
      PT(0, 0, '}', 1, op);
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
