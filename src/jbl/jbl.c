#include "jbl.h"
#include <ctype.h>
#include <stdarg.h>
#include <ejdb2/iowow/iwconv.h>
#include "jbl_internal.h"
#include "utf8proc.h"
#include "convert.h"

#define _STRX(x) #x
#define _STR(x)  _STRX(x)

IW_INLINE int _jbl_printf_estimate_size(const char *format, va_list ap) {
  char buf[1];
  int ret = vsnprintf(buf, sizeof(buf), format, ap);
  if (ret < 0) {
    return ret;
  } else {
    return ret + 1;
  }
}

IW_INLINE void _jbn_remove_item(JBL_NODE parent, JBL_NODE child);
static void _jbn_add_item(JBL_NODE parent, JBL_NODE node);

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

void jbl_set_user_data(JBL jbl, void *user_data, void (*user_data_free_fn)(void*)) {
  binn_set_user_data(&jbl->bn, user_data, user_data_free_fn);
}

void* jbl_get_user_data(JBL jbl) {
  return jbl->bn.user_data;
}

iwrc jbl_set_int64(JBL jbl, const char *key, int64_t v) {
  jbl_type_t t = jbl_type(jbl);
  if (((t != JBV_OBJECT) && (t != JBV_ARRAY)) || !jbl->bn.writable) {
    return JBL_ERROR_CREATION;
  }
  binn *bv = &jbl->bn;
  if (key) {
    if (t == JBV_OBJECT) {
      if (!binn_object_set_int64(bv, key, v)) {
        return JBL_ERROR_CREATION;
      }
    } else {
      return JBL_ERROR_CREATION;
    }
    return 0;
  } else if (t == JBV_ARRAY) {
    if (!binn_list_add_int64(bv, v)) {
      return JBL_ERROR_CREATION;
    }
    return 0;
  }
  return JBL_ERROR_INVALID;
}

iwrc jbl_set_f64(JBL jbl, const char *key, double v) {
  jbl_type_t t = jbl_type(jbl);
  if (((t != JBV_OBJECT) && (t != JBV_ARRAY)) || !jbl->bn.writable) {
    return JBL_ERROR_CREATION;
  }
  binn *bv = &jbl->bn;
  if (key) {
    if (t == JBV_OBJECT) {
      if (!binn_object_set_double(bv, key, v)) {
        return JBL_ERROR_CREATION;
      }
    } else {
      return JBL_ERROR_CREATION;
    }
    return 0;
  } else if (t == JBV_ARRAY) {
    if (!binn_list_add_double(bv, v)) {
      return JBL_ERROR_CREATION;
    }
    return 0;
  }
  return JBL_ERROR_INVALID;
}

iwrc jbl_set_string(JBL jbl, const char *key, const char *v) {
  jbl_type_t t = jbl_type(jbl);
  if (((t != JBV_OBJECT) && (t != JBV_ARRAY)) || !jbl->bn.writable) {
    return JBL_ERROR_CREATION;
  }
  binn *bv = &jbl->bn;
  if (key) {
    if (t == JBV_OBJECT) {
      if (!binn_object_set_str(bv, key, v)) {
        return JBL_ERROR_CREATION;
      }
    } else {
      return JBL_ERROR_CREATION;
    }
    return 0;
  } else if (t == JBV_ARRAY) {
    if (!binn_list_add_const_str(bv, v)) {
      return JBL_ERROR_CREATION;
    }
    return 0;
  }
  return JBL_ERROR_INVALID;
}

iwrc jbl_set_string_printf(JBL jbl, const char *key, const char *format, ...) {
  iwrc rc = 0;
  va_list ap;

  va_start(ap, format);
  int size = _jbl_printf_estimate_size(format, ap);
  if (size < 0) {
    va_end(ap);
    return IW_ERROR_INVALID_ARGS;
  }
  va_end(ap);

  va_start(ap, format);
  char *buf = malloc(size);
  RCGA(buf, finish);
  vsnprintf(buf, size, format, ap);
  va_end(ap);

  rc = jbl_set_string(jbl, key, buf);
finish:
  free(buf);
  return rc;
}

iwrc jbl_from_json_printf_va(JBL *jblp, const char *format, va_list va) {
  iwrc rc = 0;
  va_list cva;

  va_copy(cva, va);
  int size = _jbl_printf_estimate_size(format, va);
  if (size < 0) {
    va_end(cva);
    return IW_ERROR_INVALID_ARGS;
  }
  char *buf = malloc(size);
  RCGA(buf, finish);
  vsnprintf(buf, size, format, cva);
  va_end(cva);

  rc = jbl_from_json(jblp, buf);

finish:
  free(buf);
  return rc;
}

iwrc jbl_from_json_printf(JBL *jblp, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  iwrc rc = jbl_from_json_printf_va(jblp, format, ap);
  va_end(ap);
  return rc;
}

iwrc jbn_from_json_printf_va(JBL_NODE *node, IWPOOL *pool, const char *format, va_list va) {
  iwrc rc = 0;
  va_list cva;

  va_copy(cva, va);
  int size = _jbl_printf_estimate_size(format, va);
  if (size < 0) {
    va_end(cva);
    return IW_ERROR_INVALID_ARGS;
  }
  char *buf = malloc(size);
  RCGA(buf, finish);
  vsnprintf(buf, size, format, cva);
  va_end(cva);

  rc = jbn_from_json(buf, node, pool);

finish:
  free(buf);
  return rc;
}

iwrc jbn_from_json_printf(JBL_NODE *node, IWPOOL *pool, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  iwrc rc = jbn_from_json_printf_va(node, pool, format, ap);
  va_end(ap);
  return rc;
}

iwrc jbl_set_bool(JBL jbl, const char *key, bool v) {
  jbl_type_t t = jbl_type(jbl);
  if (((t != JBV_OBJECT) && (t != JBV_ARRAY)) || !jbl->bn.writable) {
    return JBL_ERROR_CREATION;
  }
  binn *bv = &jbl->bn;
  if (key) {
    if (t == JBV_OBJECT) {
      if (!binn_object_set_bool(bv, key, v)) {
        return JBL_ERROR_CREATION;
      }
    } else {
      return JBL_ERROR_CREATION;
    }
    return 0;
  } else if (t == JBV_ARRAY) {
    if (!binn_list_add_bool(bv, v)) {
      return JBL_ERROR_CREATION;
    }
    return 0;
  }
  return JBL_ERROR_INVALID;
}

iwrc jbl_set_null(JBL jbl, const char *key) {
  jbl_type_t t = jbl_type(jbl);
  if (((t != JBV_OBJECT) && (t != JBV_ARRAY)) || !jbl->bn.writable) {
    return JBL_ERROR_CREATION;
  }
  binn *bv = &jbl->bn;
  if (key) {
    if (t == JBV_OBJECT) {
      if (!binn_object_set_null(bv, key)) {
        return JBL_ERROR_CREATION;
      }
    } else {
      return JBL_ERROR_CREATION;
    }
    return 0;
  } else if (t == JBV_ARRAY) {
    if (!binn_list_add_null(bv)) {
      return JBL_ERROR_CREATION;
    }
    return 0;
  }
  return JBL_ERROR_INVALID;
}

iwrc jbl_set_empty_array(JBL jbl, const char *key) {
  JBL v = 0;
  iwrc rc = jbl_create_empty_array(&v);
  RCGO(rc, finish);
  rc = jbl_set_nested(jbl, key, v);
finish:
  jbl_destroy(&v);
  return rc;
}

iwrc jbl_set_empty_object(JBL jbl, const char *key) {
  JBL v = 0;
  iwrc rc = jbl_create_empty_object(&v);
  RCGO(rc, finish);
  rc = jbl_set_nested(jbl, key, v);
finish:
  jbl_destroy(&v);
  return rc;
}

iwrc jbl_set_nested(JBL jbl, const char *key, JBL v) {
  jbl_type_t t = jbl_type(jbl);
  if (((t != JBV_OBJECT) && (t != JBV_ARRAY)) || !jbl->bn.writable) {
    return JBL_ERROR_CREATION;
  }
  binn *bv = &jbl->bn;
  if (key) {
    if (t == JBV_OBJECT) {
      if (!binn_object_set_value(bv, key, &v->bn)) {
        return JBL_ERROR_CREATION;
      }
    } else {
      return JBL_ERROR_CREATION;
    }
    return 0;
  } else if (t == JBV_ARRAY) {
    if (!binn_list_add_value(bv, &v->bn)) {
      return JBL_ERROR_CREATION;
    }
    return 0;
  }
  return JBL_ERROR_INVALID;
}

iwrc jbl_from_buf_keep(JBL *jblp, void *buf, size_t bufsz, bool keep_on_destroy) {
  int type, size = 0, count = 0;
  if ((bufsz < MIN_BINN_SIZE) || !binn_is_valid_header(buf, &type, &count, &size, NULL)) {
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
  jbl->bn.type = type;
  jbl->bn.ptr = buf;
  jbl->bn.size = size;
  jbl->bn.count = count;
  jbl->bn.freefn = keep_on_destroy ? 0 : free;
  return 0;
}

iwrc jbl_clone(JBL src, JBL *targetp) {
  *targetp = calloc(1, sizeof(**targetp));
  JBL t = *targetp;
  if (!t) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  binn *bn = binn_copy(&src->bn);
  if (!bn) {
    return JBL_ERROR_CREATION;
  }
  t->node = 0;
  bn->allocated = 0;
  memcpy(&t->bn, bn, sizeof(*bn));
  free(bn);
  return 0;
}

IW_EXPORT iwrc jbl_object_copy_to(JBL src, JBL target) {
  iwrc rc = 0;
  // According to binn spec keys are not null terminated
  // and key length is not more than 255 bytes
  char *key, kbuf[256];
  int klen;
  JBL holder = 0;
  JBL_iterator it;

  if ((jbl_type(src) != JBV_OBJECT) || (jbl_type(target) != JBV_OBJECT)) {
    return JBL_ERROR_NOT_AN_OBJECT;
  }
  RCC(rc, finish, jbl_create_iterator_holder(&holder));
  RCC(rc, finish, jbl_iterator_init(src, &it));
  while (jbl_iterator_next(&it, holder, &key, &klen)) {
    memcpy(kbuf, key, klen);
    kbuf[klen] = '\0';
    RCC(rc, finish, jbl_set_nested(target, kbuf, holder));
  }

finish:
  jbl_destroy(&holder);
  return rc;
}

iwrc jbl_clone_into_pool(JBL src, JBL *targetp, IWPOOL *pool) {
  *targetp = 0;
  if (src->bn.writable && src->bn.dirty) {
    if (!binn_save_header(&src->bn)) {
      return JBL_ERROR_INVALID;
    }
  }
  JBL jbl = iwpool_alloc(sizeof(*jbl) + src->bn.size, pool);
  if (!jbl) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  jbl->node = 0;
  memcpy(&jbl->bn, &src->bn, sizeof(jbl->bn));
  jbl->bn.ptr = (char*) jbl + sizeof(*jbl);
  memcpy(jbl->bn.ptr, src->bn.ptr, src->bn.size);
  jbl->bn.freefn = 0;
  *targetp = jbl;
  return 0;
}

iwrc jbl_from_buf_keep_onstack(JBL jbl, void *buf, size_t bufsz) {
  int type, size = 0, count = 0;
  if ((bufsz < MIN_BINN_SIZE) || !binn_is_valid_header(buf, &type, &count, &size, NULL)) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  if (size > bufsz) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  memset(jbl, 0, sizeof(*jbl));
  jbl->bn.header = BINN_MAGIC;
  jbl->bn.type = type;
  jbl->bn.ptr = buf;
  jbl->bn.size = size;
  jbl->bn.count = count;
  return 0;
}

iwrc jbl_from_buf_keep_onstack2(JBL jbl, void *buf) {
  int type, size = 0, count = 0;
  if (!binn_is_valid_header(buf, &type, &count, &size, NULL)) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  memset(jbl, 0, sizeof(*jbl));
  jbl->bn.header = BINN_MAGIC;
  jbl->bn.type = type;
  jbl->bn.ptr = buf;
  jbl->bn.size = size;
  jbl->bn.count = count;
  return 0;
}

void jbl_destroy(JBL *jblp) {
  if (*jblp) {
    JBL jbl = *jblp;
    binn_free(&jbl->bn);
    free(jbl);
    *jblp = 0;
  }
}

iwrc jbl_create_iterator_holder(JBL *jblp) {
  *jblp = calloc(1, sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  return 0;
}

iwrc jbl_iterator_init(JBL jbl, JBL_iterator *iter) {
  int btype = jbl->bn.type;
  if ((btype != BINN_OBJECT) && (btype != BINN_LIST) && (btype != BINN_MAP)) {
    memset(iter, 0, sizeof(*iter));
    return 0;
  }
  binn_iter *biter = (binn_iter*) iter;
  if (!binn_iter_init(biter, &jbl->bn, btype)) {
    return JBL_ERROR_CREATION;
  }
  return 0;
}

bool jbl_iterator_next(JBL_iterator *iter, JBL holder, char **pkey, int *klen) {
  binn_iter *biter = (binn_iter*) iter;
  if (pkey) {
    *pkey = 0;
  }
  if (klen) {
    *klen = 0;
  }
  if (!iter || (iter->type == 0)) {
    return false;
  }
  if (iter->type == BINN_LIST) {
    if (klen) {
      *klen = iter->current;
    }
    return binn_list_next(biter, &holder->bn);
  } else {
    return binn_read_next_pair2(iter->type, biter, klen, pkey, &holder->bn);
  }
  return false;
}

IW_INLINE jbl_type_t _jbl_binn_type(int btype) {
  switch (btype) {
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
    default:
      return JBV_NONE;
  }
}

jbl_type_t jbl_type(JBL jbl) {
  if (jbl) {
    return _jbl_binn_type(jbl->bn.type);
  }
  return JBV_NONE;
}

size_t jbl_count(JBL jbl) {
  return (size_t) jbl->bn.count;
}

size_t jbl_size(JBL jbl) {
  return (size_t) jbl->bn.size;
}

size_t jbl_structure_size(void) {
  return sizeof(struct _JBL);
}

iwrc jbl_from_json(JBL *jblp, const char *jsonstr) {
  *jblp = 0;
  iwrc rc = 0;
  IWPOOL *pool = iwpool_create(2 * strlen(jsonstr));
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl;
  JBL_NODE node;
  rc = jbn_from_json(jsonstr, &node, pool);
  RCGO(rc, finish);
  if (node->type == JBV_OBJECT) {
    rc = jbl_create_empty_object(&jbl);
    RCGO(rc, finish);
  } else if (node->type == JBV_ARRAY) {
    rc = jbl_create_empty_array(&jbl);
    RCGO(rc, finish);
  } else {
    // TODO: Review
    rc = JBL_ERROR_CREATION;
    goto finish;
  }
  rc = jbl_fill_from_node(jbl, node);
  if (!rc) {
    *jblp = jbl;
  }

finish:
  iwpool_destroy(pool);
  return rc;
}

iwrc _jbl_write_double(double num, jbl_json_printer pt, void *op) {
  size_t sz;
  char buf[JBNUMBUF_SIZE];
  jbi_ftoa(num, buf, &sz);
  return pt(buf, -1, 0, 0, op);
}

iwrc _jbl_write_int(int64_t num, jbl_json_printer pt, void *op) {
  char buf[JBNUMBUF_SIZE];
  int sz = iwitoa(num, buf, sizeof(buf));
  return pt(buf, sz, 0, 0, op);
}

iwrc _jbl_write_string(const char *str, int len, jbl_json_printer pt, void *op, jbl_print_flags_t pf) {
  iwrc rc = pt(0, 0, '"', 1, op);
  RCRET(rc);
  static const char *specials = "btnvfr";
  const uint8_t *p = (const uint8_t*) str;

#define PT(data_, size_, ch_, count_) do { \
    rc = pt((const char*) (data_), size_, ch_, count_, op); \
    RCRET(rc); \
} while (0)

  if (len < 0) {
    len = (int) strlen(str);
  }
  for (size_t i = 0; i < len; i++) {
    uint8_t ch = p[i];
    if ((ch == '"') || (ch == '\\')) {
      PT(0, 0, '\\', 1);
      PT(0, 0, ch, 1);
    } else if ((ch >= '\b') && (ch <= '\r')) {
      PT(0, 0, '\\', 1);
      PT(0, 0, specials[ch - '\b'], 1);
    } else if (isprint(ch)) {
      PT(0, 0, ch, 1);
    } else if (pf & JBL_PRINT_CODEPOINTS) {
      char sbuf[7]; // escaped unicode seq
      utf8proc_int32_t cp;
      utf8proc_ssize_t sz = utf8proc_iterate(p + i, len - i, &cp);
      if (sz < 0) {
        return JBL_ERROR_PARSE_INVALID_UTF8;
      }
      if (cp > 0x0010000UL) {
        uint32_t hs = 0xD800, ls = 0xDC00; // surrogates
        cp -= 0x0010000UL;
        hs |= ((cp >> 10) & 0x3FF);
        ls |= (cp & 0x3FF);
        snprintf(sbuf, 7, "\\u%04X", hs);
        PT(sbuf, 6, 0, 0);
        snprintf(sbuf, 7, "\\u%04X", ls);
        PT(sbuf, 6, 0, 0);
      } else {
        snprintf(sbuf, 7, "\\u%04X", cp);
        PT(sbuf, 6, 0, 0);
      }
      i += sz - 1;
    } else {
      PT(0, 0, ch, 1);
    }
  }
  rc = pt(0, 0, '"', 1, op);
  return rc;
#undef PT
}

static iwrc _jbl_as_json(binn *bn, jbl_json_printer pt, void *op, int lvl, jbl_print_flags_t pf) {
  iwrc rc = 0;
  binn bv;
  binn_iter iter;
  int lv;
  int64_t llv;
  double dv;
  char key[MAX_BIN_KEY_LEN + 1];
  bool pretty = pf & JBL_PRINT_PRETTY;

#define PT(data_, size_, ch_, count_) do { \
    rc = pt(data_, size_, ch_, count_, op); \
    RCGO(rc, finish); \
} while (0)

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
        rc = _jbl_as_json(&bv, pt, op, lvl + 1, pf);
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
          rc = _jbl_write_string(key, -1, pt, op, pf);
          RCGO(rc, finish);
          if (pretty) {
            PT(": ", -1, 0, 0);
          } else {
            PT(0, 0, ':', 1);
          }
          rc = _jbl_as_json(&bv, pt, op, lvl + 1, pf);
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
          rc = _jbl_as_json(&bv, pt, op, lvl + 1, pf);
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
      rc = _jbl_write_string(bn->ptr, -1, pt, op, pf);
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
      llv = bn->vint8; // NOLINT(bugprone-signed-char-misuse)
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
      llv = (int64_t) bn->vuint64;
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
      PT("true", 4, 0, 0);
      break;
    case BINN_FALSE:
      PT("false", 5, 0, 0);
      break;
    case BINN_BOOL:
      PT(bn->vbool ? "true" : "false", -1, 0, 1);
      break;
    case BINN_NULL:
      PT("null", 4, 0, 0);
      break;
    default:
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      rc = IW_ERROR_ASSERTION;
      break;
  }

finish:
  return rc;
#undef PT
}

iwrc jbl_as_json(JBL jbl, jbl_json_printer pt, void *op, jbl_print_flags_t pf) {
  if (!jbl || !pt) {
    return IW_ERROR_INVALID_ARGS;
  }
  return _jbl_as_json(&jbl->bn, pt, op, 0, pf);
}

iwrc jbl_fstream_json_printer(const char *data, int size, char ch, int count, void *op) {
  FILE *file = op;
  if (!file) {
    return IW_ERROR_INVALID_ARGS;
  }
  if (!data) {
    if (count) {
      char cbuf[count]; // TODO: review overflow
      memset(cbuf, ch, sizeof(cbuf));
      size_t wc = fwrite(cbuf, 1, count, file);
      if (wc != sizeof(cbuf)) {
        return iwrc_set_errno(IW_ERROR_IO_ERRNO, errno);
      }
    }
  } else {
    if (size < 0) {
      size = (int) strlen(data);
    }
    if (!count) {
      count = 1;
    }
    for (int i = 0; i < count; ++i) {
      if (fprintf(file, "%.*s", size, data) < 0) {
        return iwrc_set_errno(IW_ERROR_IO_ERRNO, errno);
      }
    }
  }
  return 0;
}

iwrc jbl_xstr_json_printer(const char *data, int size, char ch, int count, void *op) {
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
    if (size < 0) {
      size = (int) strlen(data);
    }
    if (!count) {
      count = 1;
    }
    for (int i = 0; i < count; ++i) {
      iwrc rc = iwxstr_cat(xstr, data, size);
      RCRET(rc);
    }
  }
  return 0;
}

iwrc jbl_count_json_printer(const char *data, int size, char ch, int count, void *op) {
  int *cnt = op;
  if (!data) {
    *cnt = *cnt + count;
  } else {
    if (size < 0) {
      size = (int) strlen(data);
    }
    if (!count) {
      count = 1;
    }
    *cnt = *cnt + count * size;
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
    case BINN_BOOL:
      return jbl->bn.vbool;
    case BINN_FLOAT32:
      return (int64_t) jbl->bn.vfloat;
    case BINN_FLOAT64:
      return (int64_t) jbl->bn.vdouble;
    default:
      return 0;
  }
}

int32_t jbl_get_i32(JBL jbl) {
  return (int32_t) jbl_get_i64(jbl);
}

double jbl_get_f64(JBL jbl) {
  assert(jbl);
  switch (jbl->bn.type) {
    case BINN_FLOAT64:
      return jbl->bn.vdouble;
    case BINN_FLOAT32:
      return jbl->bn.vfloat;
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
    case BINN_BOOL:
      return jbl->bn.vbool;
    default:
      return 0.0;
  }
}

const char* jbl_get_str(JBL jbl) {
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

jbl_type_t jbl_object_get_type(JBL jbl, const char *key) {
  if (jbl->bn.type != BINN_OBJECT) {
    return JBV_NONE;
  }
  binn bv;
  if (!binn_object_get_value(&jbl->bn, key, &bv)) {
    return JBV_NONE;
  }
  return _jbl_binn_type(bv.type);
}

iwrc jbl_object_get_i64(JBL jbl, const char *key, int64_t *out) {
  *out = 0;
  if (jbl->bn.type != BINN_OBJECT) {
    return JBL_ERROR_NOT_AN_OBJECT;
  }
  int64 v;
  if (!binn_object_get_int64(&jbl->bn, key, &v)) {
    return JBL_ERROR_CREATION;
  }
  *out = v;
  return 0;
}

iwrc jbl_object_get_f64(JBL jbl, const char *key, double *out) {
  *out = 0.0;
  if (jbl->bn.type != BINN_OBJECT) {
    return JBL_ERROR_NOT_AN_OBJECT;
  }
  if (!binn_object_get_double(&jbl->bn, key, out)) {
    return JBL_ERROR_CREATION;
  }
  return 0;
}

iwrc jbl_object_get_bool(JBL jbl, const char *key, bool *out) {
  *out = false;
  if (jbl->bn.type != BINN_OBJECT) {
    return JBL_ERROR_NOT_AN_OBJECT;
  }
  BOOL v;
  if (!binn_object_get_bool(&jbl->bn, key, &v)) {
    return JBL_ERROR_CREATION;
  }
  *out = v;
  return 0;
}

iwrc jbl_object_get_str(JBL jbl, const char *key, const char **out) {
  *out = 0;
  if (jbl->bn.type != BINN_OBJECT) {
    return JBL_ERROR_NOT_AN_OBJECT;
  }
  if (!binn_object_get_str(&jbl->bn, key, (char**) out)) {
    return JBL_ERROR_CREATION;
  }
  return 0;
}

iwrc jbl_object_get_fill_jbl(JBL jbl, const char *key, JBL out) {
  if (jbl->bn.type != BINN_OBJECT) {
    return JBL_ERROR_NOT_AN_OBJECT;
  }
  binn_free(&out->bn);
  if (!binn_object_get_value(&jbl->bn, key, &out->bn)) {
    return JBL_ERROR_CREATION;
  }
  return 0;
}

iwrc jbl_as_buf(JBL jbl, void **buf, size_t *size) {
  assert(jbl && buf && size);
  if (jbl->bn.writable && jbl->bn.dirty) {
    if (!binn_save_header(&jbl->bn)) {
      return JBL_ERROR_INVALID;
    }
  }
  *buf = jbl->bn.ptr;
  *size = (size_t) jbl->bn.size;
  return 0;
}

//----------------------------------------------------------------------------------------------------------

static iwrc _jbl_ptr_pool(const char *path, JBL_PTR *jpp, IWPOOL *pool) {
  iwrc rc = 0;
  int cnt = 0, len, sz, doff;
  int i, j, k;
  JBL_PTR jp;
  char *jpr; // raw pointer to jp
  *jpp = 0;
  if (!path || (path[0] != '/')) {
    return JBL_ERROR_JSON_POINTER;
  }
  for (i = 0; path[i]; ++i) {
    if (path[i] == '/') {
      ++cnt;
    }
  }
  len = i;
  if ((len > 1) && (path[len - 1] == '/')) {
    return JBL_ERROR_JSON_POINTER;
  }
  sz = (int) (sizeof(struct _JBL_PTR) + cnt * sizeof(char*) + len);
  if (pool) {
    jp = iwpool_alloc(sz, pool);
  } else {
    jp = malloc(sz);
  }
  if (!jp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  jpr = (char*) jp;
  jp->cnt = cnt;
  jp->sz = sz;

  doff = offsetof(struct _JBL_PTR, n) + cnt * sizeof(char*);
  assert(sz - doff >= len);

  for (i = 0, j = 0, cnt = 0; path[i] && cnt < jp->cnt; ++i, ++j) {
    if (path[i++] == '/') {
      jp->n[cnt] = jpr + doff + j;
      for (k = 0; ; ++i, ++k) {
        if (!path[i] || (path[i] == '/')) {
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

iwrc jbl_ptr_alloc(const char *path, JBL_PTR *jpp) {
  return _jbl_ptr_pool(path, jpp, 0);
}

iwrc jbl_ptr_alloc_pool(const char *path, JBL_PTR *jpp, IWPOOL *pool) {
  return _jbl_ptr_pool(path, jpp, pool);
}

int jbl_ptr_cmp(JBL_PTR p1, JBL_PTR p2) {
  if (p1->sz != p2->sz) {
    return p1->sz - p2->sz;
  }
  if (p1->cnt != p2->cnt) {
    return p1->cnt - p2->cnt;
  }
  for (int i = 0; i < p1->cnt; ++i) {
    int r = strcmp(p1->n[i], p2->n[i]);
    if (r) {
      return r;
    }
  }
  return 0;
}

iwrc jbl_ptr_serialize(JBL_PTR ptr, IWXSTR *xstr) {
  for (int i = 0; i < ptr->cnt; ++i) {
    iwrc rc = iwxstr_cat(xstr, "/", 1);
    RCRET(rc);
    rc = iwxstr_cat(xstr, ptr->n[i], strlen(ptr->n[i]));
    RCRET(rc);
  }
  return 0;
}

iwrc _jbl_visit(binn_iter *iter, int lvl, JBL_VCTX *vctx, JBL_VISITOR visitor) {
  iwrc rc = 0;
  binn *bn = vctx->bn;
  jbl_visitor_cmd_t cmd;
  int idx;
  binn bv;

  if (lvl > JBL_MAX_NESTING_LEVEL) {
    return JBL_ERROR_MAX_NESTING_LEVEL_EXCEEDED;
  }
  if (!iter) {
    binn_iter it;
    if (!BINN_IS_CONTAINER_TYPE(bn->type)) {
      return JBL_ERROR_INVALID;
    }
    if (!binn_iter_init(&it, bn, bn->type)) {
      return JBL_ERROR_INVALID;
    }
    rc = _jbl_visit(&it, 0, vctx, visitor);
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
          rc = _jbl_visit(&it, lvl + 1, vctx, visitor);
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
          rc = _jbl_visit(&it, lvl + 1, vctx, visitor);
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
          rc = _jbl_visit(&it, lvl + 1, vctx, visitor);
          RCRET(rc);
        }
      }
      break;
    }
  }
  return rc;
}

iwrc jbn_visit(JBL_NODE node, int lvl, JBN_VCTX *vctx, JBN_VISITOR visitor) {
  iwrc rc = 0;
  if (lvl > JBL_MAX_NESTING_LEVEL) {
    return JBL_ERROR_MAX_NESTING_LEVEL_EXCEEDED;
  }
  if (!node) {
    node = vctx->root;
    lvl = 0;
    if (!node) {
      return IW_ERROR_INVALID_ARGS;
    }
  }
  JBL_NODE n = node;
  switch (node->type) {
    case JBV_OBJECT:
    case JBV_ARRAY: {
      for (n = n->child; !vctx->terminate && n; n = n->next) {
        jbn_visitor_cmd_t cmd = visitor(lvl, n, n->key, n->klidx, vctx, &rc);
        RCRET(rc);
        if (cmd & JBL_VCMD_TERMINATE) {
          vctx->terminate = true;
        }
        if (cmd & JBN_VCMD_DELETE) {
          JBL_NODE nn = n->next; // Keep pointer to next
          _jbn_remove_item(node, n);
          n->next = nn;
        } else if (!(cmd & JBL_VCMD_SKIP_NESTED) && (n->type >= JBV_OBJECT)) {
          rc = jbn_visit(n, lvl + 1, vctx, visitor);
          RCRET(rc);
        }
      }
      break;
    }
    default:
      break;
  }
  RCRET(rc);
  if (lvl == 0) {
    visitor(-1, node, 0, 0, vctx, &rc);
  }
  return rc;
}

IW_INLINE bool _jbl_visitor_update_jptr_cursor(JBL_VCTX *vctx, int lvl, const char *key, int idx) {
  JBL_PTR jp = vctx->op;
  if (lvl < jp->cnt) {
    if (vctx->pos >= lvl) {
      vctx->pos = lvl - 1;
    }
    if (vctx->pos + 1 == lvl) {
      const char *keyptr;
      char buf[JBNUMBUF_SIZE];
      if (key) {
        keyptr = key;
      } else {
        iwitoa(idx, buf, JBNUMBUF_SIZE);
        keyptr = buf;
      }
      if (!strcmp(keyptr, jp->n[lvl]) || ((jp->n[lvl][0] == '*') && (jp->n[lvl][1] == '\0'))) {
        vctx->pos = lvl;
        return (jp->cnt == lvl + 1);
      }
    }
  }
  return false;
}

IW_INLINE bool _jbn_visitor_update_jptr_cursor(JBN_VCTX *vctx, int lvl, const char *key, int idx) {
  JBL_PTR jp = vctx->op;
  if (lvl < jp->cnt) {
    if (vctx->pos >= lvl) {
      vctx->pos = lvl - 1;
    }
    if (vctx->pos + 1 == lvl) {
      const char *keyptr;
      char buf[JBNUMBUF_SIZE];
      if (key) {
        keyptr = key;
      } else {
        iwitoa(idx, buf, JBNUMBUF_SIZE);
        keyptr = buf;
        idx = (int) strlen(keyptr);
      }
      int jplen = (int) strlen(jp->n[lvl]);
      if ((  (idx == jplen)
          && !strncmp(keyptr, jp->n[lvl], idx)) || ((jp->n[lvl][0] == '*') && (jp->n[lvl][1] == '\0'))) {
        vctx->pos = lvl;
        return (jp->cnt == lvl + 1);
      }
    }
  }
  return false;
}

static jbl_visitor_cmd_t _jbl_get_visitor2(int lvl, binn *bv, const char *key, int idx, JBL_VCTX *vctx, iwrc *rc) {
  JBL_PTR jp = vctx->op;
  assert(jp);
  if (_jbl_visitor_update_jptr_cursor(vctx, lvl, key, idx)) { // Pointer matched
    JBL jbl = vctx->result;
    memcpy(&jbl->bn, bv, sizeof(*bv));
    vctx->found = true;
    return JBL_VCMD_TERMINATE;
  } else if (jp->cnt < lvl + 1) {
    return JBL_VCMD_SKIP_NESTED;
  }
  return JBL_VCMD_OK;
}

static jbl_visitor_cmd_t _jbl_get_visitor(int lvl, binn *bv, const char *key, int idx, JBL_VCTX *vctx, iwrc *rc) {
  JBL_PTR jp = vctx->op;
  assert(jp);
  if (_jbl_visitor_update_jptr_cursor(vctx, lvl, key, idx)) { // Pointer matched
    JBL jbl = malloc(sizeof(struct _JBL));
    if (!jbl) {
      *rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      return JBL_VCMD_TERMINATE;
    }
    memcpy(&jbl->bn, bv, sizeof(*bv));
    vctx->result = jbl;
    return JBL_VCMD_TERMINATE;
  } else if (jp->cnt < lvl + 1) {
    return JBL_VCMD_SKIP_NESTED;
  }
  return JBL_VCMD_OK;
}

bool _jbl_at(JBL jbl, JBL_PTR jp, JBL res) {
  JBL_VCTX vctx = {
    .bn     = &jbl->bn,
    .op     = jp,
    .pos    = -1,
    .result = res
  };
  _jbl_visit(0, 0, &vctx, _jbl_get_visitor2);
  return vctx.found;
}

iwrc jbl_at2(JBL jbl, JBL_PTR jp, JBL *res) {
  JBL_VCTX vctx = {
    .bn  = &jbl->bn,
    .op  = jp,
    .pos = -1
  };
  iwrc rc = _jbl_visit(0, 0, &vctx, _jbl_get_visitor);
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
  return rc;
}

iwrc jbl_at(JBL jbl, const char *path, JBL *res) {
  JBL_PTR jp;
  iwrc rc = _jbl_ptr_pool(path, &jp, 0);
  if (rc) {
    *res = 0;
    return rc;
  }
  rc = jbl_at2(jbl, jp, res);
  free(jp);
  return rc;
}

static jbn_visitor_cmd_t _jbn_get_visitor(int lvl, JBL_NODE n, const char *key, int klidx, JBN_VCTX *vctx, iwrc *rc) {
  if (lvl < 0) { // EOF
    return JBL_VCMD_OK;
  }
  JBL_PTR jp = vctx->op;
  assert(jp);
  if (_jbn_visitor_update_jptr_cursor(vctx, lvl, key, klidx)) { // Pointer matched
    vctx->result = n;
    return JBL_VCMD_TERMINATE;
  } else if (jp->cnt < lvl + 1) {
    return JBL_VCMD_SKIP_NESTED;
  }
  return JBL_VCMD_OK;
}

iwrc jbn_at2(JBL_NODE node, JBL_PTR jp, JBL_NODE *res) {
  JBN_VCTX vctx = {
    .root = node,
    .op   = jp,
    .pos  = -1
  };
  iwrc rc = jbn_visit(node, 0, &vctx, _jbn_get_visitor);
  if (rc) {
    *res = 0;
  } else {
    if (!vctx.result) {
      rc = JBL_ERROR_PATH_NOTFOUND;
      *res = 0;
    } else {
      *res = (JBL_NODE) vctx.result;
    }
  }
  return rc;
}

iwrc jbn_at(JBL_NODE node, const char *path, JBL_NODE *res) {
  JBL_PTR jp;
  iwrc rc = _jbl_ptr_pool(path, &jp, 0);
  if (rc) {
    *res = 0;
    return rc;
  }
  rc = jbn_at2(node, jp, res);
  free(jp);
  return rc;
}

int jbn_paths_compare(JBL_NODE n1, const char *n1path, JBL_NODE n2, const char *n2path, jbl_type_t vtype, iwrc *rcp) {
  *rcp = 0;
  JBL_NODE v1 = 0, v2 = 0;
  iwrc rc = jbn_at(n1, n1path, &v1);
  if (rc && (rc != JBL_ERROR_PATH_NOTFOUND)) {
    *rcp = rc;
    return -2;
  }
  rc = jbn_at(n2, n2path, &v2);
  if (rc && (rc != JBL_ERROR_PATH_NOTFOUND)) {
    *rcp = rc;
    return -2;
  }
  if (vtype) {
    if (((v1 == 0) || (v1->type != vtype)) || ((v2 == 0) || (v2->type != vtype))) {
      *rcp = JBL_ERROR_TYPE_MISMATCHED;
      return -2;
    }
  }
  return _jbl_compare_nodes(v1, v2, rcp);
}

int jbn_path_compare(JBL_NODE n1, JBL_NODE n2, const char *path, jbl_type_t vtype, iwrc *rcp) {
  return jbn_paths_compare(n1, path, n2, path, vtype, rcp);
}

int jbn_path_compare_str(JBL_NODE n, const char *path, const char *sv, iwrc *rcp) {
  *rcp = 0;
  JBL_NODE v;
  iwrc rc = jbn_at(n, path, &v);
  if (rc) {
    *rcp = rc;
    return -2;
  }
  struct _JBL_NODE cn = {
    .type  = JBV_STR,
    .vptr  = sv,
    .vsize = (int) strlen(sv)
  };
  return _jbl_compare_nodes(v, &cn, rcp);
}

int jbn_path_compare_i64(JBL_NODE n, const char *path, int64_t iv, iwrc *rcp) {
  *rcp = 0;
  JBL_NODE v;
  iwrc rc = jbn_at(n, path, &v);
  if (rc) {
    *rcp = rc;
    return -2;
  }
  struct _JBL_NODE cn = {
    .type = JBV_I64,
    .vi64 = iv
  };
  return _jbl_compare_nodes(v, &cn, rcp);
}

int jbn_path_compare_f64(JBL_NODE n, const char *path, double fv, iwrc *rcp) {
  *rcp = 0;
  JBL_NODE v;
  iwrc rc = jbn_at(n, path, &v);
  if (rc) {
    *rcp = rc;
    return -2;
  }
  struct _JBL_NODE cn = {
    .type = JBV_F64,
    .vf64 = fv
  };
  return _jbl_compare_nodes(v, &cn, rcp);
}

int jbn_path_compare_bool(JBL_NODE n, const char *path, bool bv, iwrc *rcp) {
  *rcp = 0;
  JBL_NODE v;
  iwrc rc = jbn_at(n, path, &v);
  if (rc) {
    *rcp = rc;
    return -2;
  }
  struct _JBL_NODE cn = {
    .type  = JBV_BOOL,
    .vbool = bv
  };
  return _jbl_compare_nodes(v, &cn, rcp);
}

IW_INLINE void _jbl_node_reset_data(JBL_NODE target) {
  jbl_type_t t = target->type;
  memset(((uint8_t*) target) + offsetof(struct _JBL_NODE, child),
         0,
         sizeof(struct _JBL_NODE) - offsetof(struct _JBL_NODE, child));
  target->type = t;
}

IW_INLINE void _jbl_copy_node_data(JBL_NODE target, JBL_NODE value) {
  memcpy(((uint8_t*) target) + offsetof(struct _JBL_NODE, child),
         ((uint8_t*) value) + offsetof(struct _JBL_NODE, child),
         sizeof(struct _JBL_NODE) - offsetof(struct _JBL_NODE, child));
}

iwrc _jbl_increment_node_data(JBL_NODE target, JBL_NODE value) {
  if ((value->type != JBV_I64) && (value->type != JBV_F64)) {
    return JBL_ERROR_PATCH_INVALID_VALUE;
  }
  if (target->type == JBV_I64) {
    if (value->type == JBV_I64) {
      target->vi64 += value->vi64;
    } else {
      target->vi64 += (int64_t) value->vf64;
    }
    return 0;
  } else if (target->type == JBV_F64) {
    if (value->type == JBV_F64) {
      target->vf64 += value->vf64;
    } else {
      target->vf64 += (double) value->vi64;
    }
    return 0;
  } else {
    return JBL_ERROR_PATCH_TARGET_INVALID;
  }
}

void jbn_data(JBL_NODE node) {
  _jbl_node_reset_data(node);
}

int jbn_length(JBL_NODE node) {
  int ret = 0;
  for (JBL_NODE n = node->child; n; n = n->next) {
    ++ret;
  }
  return ret;
}

static void _jbn_add_item(JBL_NODE parent, JBL_NODE node) {
  assert(parent && node);
  node->next = 0;
  node->prev = 0;
  node->parent = parent;
  if (parent->child) {
    JBL_NODE prev = parent->child->prev;
    parent->child->prev = node;
    if (prev) { // -V1051
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
    node->key = 0;
    if (node->prev) {
      node->klidx = node->prev->klidx + 1;
    } else {
      node->klidx = 0;
    }
  }
}

void jbn_add_item(JBL_NODE parent, JBL_NODE node) {
  _jbn_add_item(parent, node);
}

iwrc jbn_add_item_str(JBL_NODE parent, const char *key, const char *val, int vlen, JBL_NODE *node_out, IWPOOL *pool) {
  if (!parent || !pool || (parent->type < JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n = iwpool_calloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (parent->type == JBV_OBJECT) {
    if (!key) {
      return IW_ERROR_INVALID_ARGS;
    }
    n->key = iwpool_strdup(pool, key, &rc);
    RCGO(rc, finish);
    n->klidx = (int) strlen(n->key);
  }
  n->type = JBV_STR;
  if (val) {
    if (vlen < 0) {
      vlen = (int) strlen(val);
    }
    n->vptr = iwpool_strndup(pool, val, vlen, &rc);
    RCGO(rc, finish);
    n->vsize = vlen;
  }
  jbn_add_item(parent, n);
  if (node_out) {
    *node_out = n;
  }
finish:
  return rc;
}

iwrc jbn_add_item_null(JBL_NODE parent, const char *key, IWPOOL *pool) {
  if (!parent || !pool || (parent->type < JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n = iwpool_calloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (parent->type == JBV_OBJECT) {
    if (!key) {
      return IW_ERROR_INVALID_ARGS;
    }
    n->key = iwpool_strdup(pool, key, &rc);
    RCGO(rc, finish);
    n->klidx = (int) strlen(n->key);
  }
  n->type = JBV_NULL;
  jbn_add_item(parent, n);
finish:
  return rc;
}

iwrc jbn_add_item_i64(JBL_NODE parent, const char *key, int64_t val, JBL_NODE *node_out, IWPOOL *pool) {
  if (!parent || !pool || (parent->type < JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n = iwpool_calloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (parent->type == JBV_OBJECT) {
    if (!key) {
      return IW_ERROR_INVALID_ARGS;
    }
    n->key = iwpool_strdup(pool, key, &rc);
    RCGO(rc, finish);
    n->klidx = (int) strlen(n->key);
  }
  n->type = JBV_I64;
  n->vi64 = val;
  jbn_add_item(parent, n);
  if (node_out) {
    *node_out = n;
  }
finish:
  return rc;
}

iwrc jbn_add_item_f64(JBL_NODE parent, const char *key, double val, JBL_NODE *node_out, IWPOOL *pool) {
  if (!parent || !pool || (parent->type < JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n = iwpool_calloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (parent->type == JBV_OBJECT) {
    if (!key) {
      return IW_ERROR_INVALID_ARGS;
    }
    n->key = iwpool_strdup(pool, key, &rc);
    RCGO(rc, finish);
    n->klidx = (int) strlen(n->key);
  }
  n->type = JBV_F64;
  n->vf64 = val;
  jbn_add_item(parent, n);
  if (node_out) {
    *node_out = n;
  }
finish:
  return rc;
}

iwrc jbn_add_item_bool(JBL_NODE parent, const char *key, bool val, JBL_NODE *node_out, IWPOOL *pool) {
  if (!parent || !pool || (parent->type < JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n = iwpool_calloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (parent->type == JBV_OBJECT) {
    if (!key) {
      return IW_ERROR_INVALID_ARGS;
    }
    n->key = iwpool_strdup(pool, key, &rc);
    RCGO(rc, finish);
    n->klidx = (int) strlen(n->key);
  }
  n->type = JBV_BOOL;
  n->vbool = val;
  jbn_add_item(parent, n);
  if (node_out) {
    *node_out = n;
  }
finish:
  return rc;
}

iwrc jbn_add_item_obj(JBL_NODE parent, const char *key, JBL_NODE *out, IWPOOL *pool) {
  if (!parent || !pool || (parent->type < JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n = iwpool_calloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (parent->type == JBV_OBJECT) {
    if (!key) {
      return IW_ERROR_INVALID_ARGS;
    }
    n->key = iwpool_strdup(pool, key, &rc);
    RCGO(rc, finish);
    n->klidx = (int) strlen(n->key);
  }
  n->type = JBV_OBJECT;
  jbn_add_item(parent, n);
  if (out) {
    *out = n;
  }
finish:
  return rc;
}

iwrc jbn_add_item_arr(JBL_NODE parent, const char *key, JBL_NODE *out, IWPOOL *pool) {
  if (!parent || !pool || (parent->type < JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n = iwpool_calloc(sizeof(*n), pool);
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (parent->type == JBV_OBJECT) {
    if (!key) {
      return IW_ERROR_INVALID_ARGS;
    }
    n->key = iwpool_strdup(pool, key, &rc);
    RCGO(rc, finish);
    n->klidx = (int) strlen(n->key);
  }
  n->type = JBV_ARRAY;
  jbn_add_item(parent, n);
  if (out) {
    *out = n;
  }
finish:
  return rc;
}

iwrc jbn_copy_path(
  JBL_NODE    src,
  const char *src_path,
  JBL_NODE    target,
  const char *target_path,
  bool        overwrite_on_nulls,
  bool        no_src_clone,
  IWPOOL     *pool
  ) {
  if (!src || !src_path || !target || !target_path || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  JBL_NODE n1, n2;
  jbp_patch_t op = JBP_REPLACE;

  if (strcmp("/", src_path) != 0) { // -V526
    rc = jbn_at(src, src_path, &n1);
    if (rc == JBL_ERROR_PATH_NOTFOUND) {
      return 0;
    }
    RCRET(rc);
  } else {
    n1 = src;
  }
  if (!overwrite_on_nulls && (n1->type <= JBV_NULL)) {
    return 0;
  }
  if (no_src_clone) {
    n2 = n1;
  } else {
    rc = jbn_clone(n1, &n2, pool);
    RCRET(rc);
  }

  rc = jbn_at(target, target_path, &n1);
  if (rc == JBL_ERROR_PATH_NOTFOUND) {
    rc = 0;
    op = JBP_ADD_CREATE;
  }
  JBL_PATCH p[] = {
    {
      .op = op,
      .path = target_path,
      .vnode = n2
    }
  };
  return jbn_patch(target, p, sizeof(p) / sizeof(p[0]), pool);
}

IW_EXPORT iwrc jbn_copy_paths(
  JBL_NODE     src,
  JBL_NODE     target,
  const char **paths,
  bool         overwrite_on_nulls,
  bool         no_src_clone,
  IWPOOL      *pool
  ) {
  if (!target || !src || !paths || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  for (const char **p = paths; *p; ++p) {
    const char *path = *p;
    rc = jbn_copy_path(src, path, target, path, overwrite_on_nulls, no_src_clone, pool);
    RCBREAK(rc);
  }
  return rc;
}

IW_INLINE void _jbn_remove_item(JBL_NODE parent, JBL_NODE child) {
  assert(parent->child);
  if (parent->child == child) {                 // First element
    if (child->next) {
      parent->child = child->next;
      parent->child->prev = child->prev;
      if (child->prev) {
        child->prev->next = 0;
      }
    } else {
      parent->child = 0;
    }
  } else if (parent->child->prev == child) {    // Last element
    parent->child->prev = child->prev;
    if (child->prev) {
      child->prev->next = 0;
    }
  } else { // Somewhere in middle
    if (child->next) {
      child->next->prev = child->prev;
    }
    if (child->prev) {
      child->prev->next = child->next;
    }
  }
  child->next = 0;
  child->prev = 0;
  child->child = 0;
  child->parent = 0;
}

void jbn_remove_item(JBL_NODE parent, JBL_NODE child) {
  _jbn_remove_item(parent, child);
}

static iwrc _jbl_create_node(
  JBLDRCTX   *ctx,
  const binn *bv,
  JBL_NODE    parent,
  const char *key,
  int         klidx,
  JBL_NODE   *node,
  bool        clone_strings
  ) {
  iwrc rc = 0;
  JBL_NODE n = iwpool_alloc(sizeof(*n), ctx->pool);
  if (node) {
    *node = 0;
  }
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  memset(n, 0, sizeof(*n));
  if (key && clone_strings) {
    n->key = iwpool_strndup(ctx->pool, key, klidx, &rc);
    RCGO(rc, finish);
  } else {
    n->key = key;
  }
  n->klidx = klidx;
  n->parent = parent;
  switch (bv->type) {
    case BINN_NULL:
      n->type = JBV_NULL;
      break;
    case BINN_STRING:
      n->type = JBV_STR;
      if (!clone_strings) {
        n->vptr = bv->ptr;
        n->vsize = bv->size;
      } else {
        n->vptr = iwpool_strndup(ctx->pool, bv->ptr, bv->size, &rc);
        n->vsize = bv->size;
        RCGO(rc, finish);
      }
      break;
    case BINN_OBJECT:
    case BINN_MAP:
      n->type = JBV_OBJECT;
      break;
    case BINN_LIST:
      n->type = JBV_ARRAY;
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
      n->vi64 = bv->vint8; // NOLINT(bugprone-signed-char-misuse)
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
    _jbn_add_item(parent, n);
  }

finish:
  if (rc) {
    free(n);
  } else {
    if (node) {
      *node = n;
    }
  }
  return rc;
}

static iwrc _jbl_node_from_binn_impl(
  JBLDRCTX   *ctx,
  const binn *bn,
  JBL_NODE    parent,
  char       *key,
  int         klidx,
  bool        clone_strings
  ) {
  binn bv;
  binn_iter iter;
  iwrc rc = 0;

  switch (bn->type) {
    case BINN_OBJECT:
    case BINN_MAP:
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, &parent, clone_strings);
      RCRET(rc);
      if (!ctx->root) {
        ctx->root = parent;
      }
      if (!binn_iter_init(&iter, (binn*) bn, bn->type)) {
        return JBL_ERROR_INVALID;
      }
      if (bn->type == BINN_OBJECT) {
        for (int i = 0; binn_object_next2(&iter, &key, &klidx, &bv); ++i) {
          rc = _jbl_node_from_binn_impl(ctx, &bv, parent, key, klidx, clone_strings);
          RCRET(rc);
        }
      } else if (bn->type == BINN_MAP) {
        for (int i = 0; binn_map_next(&iter, &klidx, &bv); ++i) {
          rc = _jbl_node_from_binn_impl(ctx, &bv, parent, 0, klidx, clone_strings);
          RCRET(rc);
        }
      }
      break;
    case BINN_LIST:
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, &parent, clone_strings);
      RCRET(rc);
      if (!ctx->root) {
        ctx->root = parent;
      }
      if (!binn_iter_init(&iter, (binn*) bn, bn->type)) {
        return JBL_ERROR_INVALID;
      }
      for (int i = 0; binn_list_next(&iter, &bv); ++i) {
        rc = _jbl_node_from_binn_impl(ctx, &bv, parent, 0, i, clone_strings);
        RCRET(rc);
      }
      break;
    default: {
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, 0, clone_strings);
      RCRET(rc);
      break;
    }
  }
  return rc;
}

iwrc _jbl_node_from_binn(const binn *bn, JBL_NODE *node, bool clone_strings, IWPOOL *pool) {
  JBLDRCTX ctx = {
    .pool = pool
  };
  iwrc rc = _jbl_node_from_binn_impl(&ctx, bn, 0, 0, -1, clone_strings);
  if (rc) {
    *node = 0;
  } else {
    *node = ctx.root;
  }
  return rc;
}

static JBL_NODE _jbl_node_find(JBL_NODE node, JBL_PTR ptr, int from, int to) {
  if (!ptr || !node) {
    return 0;
  }
  JBL_NODE n = node;

  for (int i = from; n && i < ptr->cnt && i < to; ++i) {
    if (n->type == JBV_OBJECT) {
      int ptrnlen = (int) strlen(ptr->n[i]);
      for (n = n->child; n; n = n->next) {
        if (!strncmp(n->key, ptr->n[i], n->klidx) && (ptrnlen == n->klidx)) {
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

IW_INLINE JBL_NODE _jbl_node_find2(JBL_NODE node, JBL_PTR ptr) {
  if (!node || !ptr || !ptr->cnt) {
    return 0;
  }
  return _jbl_node_find(node, ptr, 0, ptr->cnt - 1);
}

static JBL_NODE _jbl_node_detach(JBL_NODE target, JBL_PTR path) {
  if (!path) {
    return 0;
  }
  JBL_NODE parent = (path->cnt > 1) ? _jbl_node_find(target, path, 0, path->cnt - 1) : target;
  if (!parent) {
    return 0;
  }
  JBL_NODE child = _jbl_node_find(parent, path, path->cnt - 1, path->cnt);
  if (!child) {
    return 0;
  }
  _jbn_remove_item(parent, child);
  return child;
}

JBL_NODE jbn_detach2(JBL_NODE target, JBL_PTR path) {
  return _jbl_node_detach(target, path);
}

JBL_NODE jbn_detach(JBL_NODE target, const char *path) {
  JBL_PTR jp;
  iwrc rc = _jbl_ptr_pool(path, &jp, 0);
  if (rc) {
    return 0;
  }
  JBL_NODE res = jbn_detach2(target, jp);
  free(jp);
  return res;
}

static int _jbl_cmp_node_keys(const void *o1, const void *o2) {
  JBL_NODE n1 = *((JBL_NODE*) o1);
  JBL_NODE n2 = *((JBL_NODE*) o2);
  if (!n1 && !n2) {
    return 0;
  }
  if (!n2 || (n1->klidx > n2->klidx)) { // -V522
    return 1;
  } else if (!n1 || (n1->klidx < n2->klidx)) { // -V522
    return -1;
  }
  return strncmp(n1->key, n2->key, n1->klidx);
}

static uint32_t _jbl_node_count(JBL_NODE n) {
  uint32_t ret = 0;
  n = n->child;
  while (n) {
    ret++;
    n = n->next;
  }
  return ret;
}

static int _jbl_compare_objects(JBL_NODE n1, JBL_NODE n2, iwrc *rcp) {
  int ret = 0;
  uint32_t cnt = _jbl_node_count(n1);
  uint32_t i = _jbl_node_count(n2);
  if (cnt > i) {
    return 1;
  } else if (cnt < i) {
    return -1;
  } else if (cnt == 0) {
    return 0;
  }
  JBL_NODE *s1 = malloc(2 * sizeof(JBL_NODE) * cnt);
  if (!s1) {
    *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return 0;
  }
  JBL_NODE *s2 = s1 + cnt;

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
  qsort(s1, cnt, sizeof(JBL_NODE), _jbl_cmp_node_keys);
  qsort(s2, cnt, sizeof(JBL_NODE), _jbl_cmp_node_keys);
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

int _jbl_compare_nodes(JBL_NODE n1, JBL_NODE n2, iwrc *rcp) {
  if (!n1 && !n2) {
    return 0;
  } else if (!n1) {
    return -1;
  } else if (!n2) {
    return 1;
  } else if (n1->type != n2->type) {
    return (int) n1->type - (int) n2->type;
  }
  switch (n1->type) {
    case JBV_BOOL:
      return n1->vbool - n2->vbool;
    case JBV_I64:
      return n1->vi64 > n2->vi64 ? 1 : n1->vi64 < n2->vi64 ? -1 : 0;
    case JBV_F64: {
      size_t sz1, sz2;
      char b1[JBNUMBUF_SIZE];
      char b2[JBNUMBUF_SIZE];
      jbi_ftoa(n1->vf64, b1, &sz1);
      jbi_ftoa(n2->vf64, b2, &sz2);
      return iwafcmp(b1, sz1, b2, sz2);
    }
    case JBV_STR:
      if (n1->vsize != n2->vsize) {
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

int jbn_compare_nodes(JBL_NODE n1, JBL_NODE n2, iwrc *rcp) {
  return _jbl_compare_nodes(n1, n2, rcp);
}

static iwrc _jbl_target_apply_patch(JBL_NODE target, const JBL_PATCHEXT *ex, IWPOOL *pool) {
  struct _JBL_NODE *ntmp;
  jbp_patch_t op = ex->p->op;
  JBL_PTR path = ex->path;
  JBL_NODE value = ex->p->vnode;
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
    } else if ((op == JBP_REPLACE) || (op == JBP_ADD) || (op == JBP_ADD_CREATE)) {
      if (!value) {
        return JBL_ERROR_PATCH_NOVALUE;
      }
      memmove(target, value, sizeof(*value));
    }
  } else { // Not a root
    if ((op == JBP_REMOVE) || (op == JBP_REPLACE)) {
      _jbl_node_detach(target, ex->path);
    }
    if (op == JBP_REMOVE) {
      return 0;
    } else if ((op == JBP_MOVE) || (op == JBP_COPY) || (op == JBP_SWAP)) {
      if (op == JBP_MOVE) {
        value = _jbl_node_detach(target, ex->from);
      } else {
        value = _jbl_node_find(target, ex->from, 0, ex->from->cnt);
      }
      if (!value) {
        return JBL_ERROR_PATH_NOTFOUND;
      }
      if (op == JBP_SWAP) {
        ntmp = iwpool_calloc(sizeof(*ntmp), pool);
        if (!ntmp) {
          return iwrc_set_errno(IW_ERROR_ALLOC, errno);
        }
      }
    } else { // ADD/REPLACE/INCREMENT
      if (!value) {
        return JBL_ERROR_PATCH_NOVALUE;
      }
    }
    int lastidx = path->cnt - 1;
    JBL_NODE parent = (path->cnt > 1) ? _jbl_node_find(target, path, 0, lastidx) : target;
    if (!parent) {
      if (op == JBP_ADD_CREATE) {
        parent = target;
        for (int i = 0; i < lastidx; ++i) {
          JBL_NODE pn = _jbl_node_find(parent, path, i, i + 1);
          if (!pn) {
            pn = iwpool_calloc(sizeof(*pn), pool);
            if (!pn) {
              return iwrc_set_errno(IW_ERROR_ALLOC, errno);
            }
            pn->type = JBV_OBJECT;
            pn->key = path->n[i];
            pn->klidx = (int) strlen(pn->key);
            _jbn_add_item(parent, pn);
          } else if (pn->type != JBV_OBJECT) {
            return JBL_ERROR_PATCH_TARGET_INVALID;
          }
          parent = pn;
        }
      } else {
        return JBL_ERROR_PATCH_TARGET_INVALID;
      }
    }
    if (parent->type == JBV_ARRAY) {
      if ((path->n[lastidx][0] == '-') && (path->n[lastidx][1] == '\0')) {
        if (op == JBP_SWAP) {
          value = _jbl_node_detach(target, ex->from);
        }
        _jbn_add_item(parent, value); // Add to end of array
      } else {                        // Insert into the specified index
        int idx = iwatoi(path->n[lastidx]);
        int cnt = idx;
        JBL_NODE child = parent->child;
        while (child && cnt > 0) {
          cnt--;
          child = child->next;
        }
        if (cnt > 0) {
          return JBL_ERROR_PATCH_INVALID_ARRAY_INDEX;
        }
        value->klidx = idx;
        if (child) {
          if (op == JBP_SWAP) {
            _jbl_copy_node_data(ntmp, value);
            _jbl_copy_node_data(value, child);
            _jbl_copy_node_data(child, ntmp);
          } else {
            value->parent = parent;
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
          }
        } else {
          if (op == JBP_SWAP) {
            value = _jbl_node_detach(target, ex->from);
          }
          _jbn_add_item(parent, value);
        }
      }
    } else if (parent->type == JBV_OBJECT) {
      JBL_NODE child = _jbl_node_find(parent, path, path->cnt - 1, path->cnt);
      if (child) {
        if (op == JBP_INCREMENT) {
          return _jbl_increment_node_data(child, value);
        } else {
          if (op == JBP_SWAP) {
            _jbl_copy_node_data(ntmp, value);
            _jbl_copy_node_data(value, child);
            _jbl_copy_node_data(child, ntmp);
          } else {
            _jbl_copy_node_data(child, value);
          }
        }
      } else if (op != JBP_INCREMENT) {
        if (op == JBP_SWAP) {
          value = _jbl_node_detach(target, ex->from);
        }
        value->key = path->n[path->cnt - 1];
        value->klidx = (int) strlen(value->key);
        _jbn_add_item(parent, value);
      } else {
        return JBL_ERROR_PATCH_TARGET_INVALID;
      }
    } else {
      return JBL_ERROR_PATCH_TARGET_INVALID;
    }
  }
  return 0;
}

static iwrc _jbl_from_node_impl(binn *res, JBL_NODE node) {
  iwrc rc = 0;
  switch (node->type) {
    case JBV_OBJECT:
      if (!binn_create(res, BINN_OBJECT, 0, NULL)) {
        return JBL_ERROR_CREATION;
      }
      for (JBL_NODE n = node->child; n; n = n->next) {
        binn bv;
        rc = _jbl_from_node_impl(&bv, n);
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
      for (JBL_NODE n = node->child; n; n = n->next) {
        binn bv;
        rc = _jbl_from_node_impl(&bv, n);
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
      binn_set_string(res, (void*) node->vptr, 0);
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
      binn_init_item(res);
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

iwrc _jbl_binn_from_node(binn *res, JBL_NODE node) {
  iwrc rc = _jbl_from_node_impl(res, node);
  if (!rc) {
    if (res->writable && res->dirty) {
      binn_save_header(res);
    }
  }
  return rc;
}

iwrc _jbl_from_node(JBL jbl, JBL_NODE node) {
  jbl->node = node;
  return _jbl_binn_from_node(&jbl->bn, node);
}

static iwrc _jbl_patch_node(JBL_NODE root, const JBL_PATCH *p, size_t cnt, IWPOOL *pool) {
  if (cnt < 1) {
    return 0;
  }
  if (!root || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  size_t i = 0;
  JBL_PATCHEXT parr[cnt];
  memset(parr, 0, cnt * sizeof(JBL_PATCHEXT));
  for (i = 0; i < cnt; ++i) {
    JBL_PATCHEXT *ext = &parr[i];
    ext->p = &p[i];
    rc = _jbl_ptr_pool(p[i].path, &ext->path, pool);
    RCRET(rc);
    if (p[i].from) {
      rc = _jbl_ptr_pool(p[i].from, &ext->from, pool);
      RCRET(rc);
    }
  }
  for (i = 0; i < cnt; ++i) {
    rc = _jbl_target_apply_patch(root, &parr[i], pool);
    RCRET(rc);
  }
  return rc;
}

static iwrc _jbl_patch(JBL jbl, const JBL_PATCH *p, size_t cnt, IWPOOL *pool) {
  if (cnt < 1) {
    return 0;
  }
  if (!jbl || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  binn bv;
  binn *bn;
  JBL_NODE root;
  iwrc rc = _jbl_node_from_binn(&jbl->bn, &root, false, pool);
  RCRET(rc);
  rc = _jbl_patch_node(root, p, cnt, pool);
  RCRET(rc);
  if (root->type != JBV_NONE) {
    rc = _jbl_from_node_impl(&bv, root);
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

int _jbl_cmp_atomic_values(JBL v1, JBL v2) {
  jbl_type_t t1 = jbl_type(v1);
  jbl_type_t t2 = jbl_type(v2);
  if (t1 != t2) {
    return (int) t1 - (int) t2;
  }
  switch (t1) {
    case JBV_BOOL:
    case JBV_I64: {
      int64_t vv1 = jbl_get_i64(v1);
      int64_t vv2 = jbl_get_i64(v2);
      return vv1 > vv2 ? 1 : vv1 < vv2 ? -1 : 0;
    }
    case JBV_STR:
      return strcmp(jbl_get_str(v1), jbl_get_str(v2)); // -V575
    case JBV_F64: {
      double vv1 = jbl_get_f64(v1);
      double vv2 = jbl_get_f64(v2);
      return vv1 > vv2 ? 1 : vv1 < vv2 ? -1 : 0;
    }
    default:
      return 0;
  }
}

bool _jbl_is_eq_atomic_values(JBL v1, JBL v2) {
  jbl_type_t t1 = jbl_type(v1);
  jbl_type_t t2 = jbl_type(v2);
  if (t1 != t2) {
    return false;
  }
  switch (t1) {
    case JBV_BOOL:
    case JBV_I64:
      return jbl_get_i64(v1) == jbl_get_i64(v2);
    case JBV_STR:
      return !strcmp(jbl_get_str(v1), jbl_get_str(v2)); // -V575
    case JBV_F64:
      return jbl_get_f64(v1) == jbl_get_f64(v2); // -V550
    case JBV_OBJECT:
    case JBV_ARRAY:
      return false;
    default:
      return true;
  }
}

// --------------------------- Public API

void jbn_apply_from(JBL_NODE target, JBL_NODE from) {
  const int off = offsetof(struct _JBL_NODE, child);
  memcpy((char*) target + off,
         (char*) from + off,
         sizeof(struct _JBL_NODE) - off);
}

iwrc jbl_to_node(JBL jbl, JBL_NODE *node, bool clone_strings, IWPOOL *pool) {
  if (jbl->node) {
    *node = jbl->node;
    return 0;
  }
  return _jbl_node_from_binn(&jbl->bn, node, clone_strings, pool);
}

iwrc jbn_patch(JBL_NODE root, const JBL_PATCH *p, size_t cnt, IWPOOL *pool) {
  return _jbl_patch_node(root, p, cnt, pool);
}

iwrc jbl_patch(JBL jbl, const JBL_PATCH *p, size_t cnt) {
  if (cnt < 1) {
    return 0;
  }
  if (!jbl || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  IWPOOL *pool = iwpool_create(jbl->bn.size);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_patch(jbl, p, cnt, pool);
  iwpool_destroy(pool);
  return rc;
}

static iwrc _jbl_create_patch(JBL_NODE node, JBL_PATCH **pptr, int *cntp, IWPOOL *pool) {
  *pptr = 0;
  *cntp = 0;
  int i = 0;
  for (JBL_NODE n = node->child; n; n = n->next) {
    if (n->type != JBV_OBJECT) {
      return JBL_ERROR_PATCH_INVALID;
    }
    ++i;
  }
  JBL_PATCH *p = iwpool_alloc(i * sizeof(*p), pool);
  if (!p) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  memset(p, 0, i * sizeof(*p));
  i = 0;
  for (JBL_NODE n = node->child; n; n = n->next, ++i) {
    JBL_PATCH *pp = p + i;
    for (JBL_NODE n2 = n->child; n2; n2 = n2->next) {
      if (!strncmp("op", n2->key, n2->klidx)) {
        if (n2->type != JBV_STR) {
          return JBL_ERROR_PATCH_INVALID;
        }
        if (!strncmp("add", n2->vptr, n2->vsize)) {
          pp->op = JBP_ADD;
        } else if (!strncmp("remove", n2->vptr, n2->vsize)) {
          pp->op = JBP_REMOVE;
        } else if (!strncmp("replace", n2->vptr, n2->vsize)) {
          pp->op = JBP_REPLACE;
        } else if (!strncmp("copy", n2->vptr, n2->vsize)) {
          pp->op = JBP_COPY;
        } else if (!strncmp("move", n2->vptr, n2->vsize)) {
          pp->op = JBP_MOVE;
        } else if (!strncmp("test", n2->vptr, n2->vsize)) {
          pp->op = JBP_TEST;
        } else if (!strncmp("increment", n2->vptr, n2->vsize)) {
          pp->op = JBP_INCREMENT;
        } else if (!strncmp("add_create", n2->vptr, n2->vsize)) {
          pp->op = JBP_ADD_CREATE;
        } else if (!strncmp("swap", n2->vptr, n2->vsize)) {
          pp->op = JBP_SWAP;
        } else {
          return JBL_ERROR_PATCH_INVALID_OP;
        }
      } else if (!strncmp("value", n2->key, n2->klidx)) {
        pp->vnode = n2;
      } else if (!strncmp("path", n2->key, n2->klidx)) {
        if (n2->type != JBV_STR) {
          return JBL_ERROR_PATCH_INVALID;
        }
        pp->path = n2->vptr;
      } else if (!strncmp("from", n2->key, n2->klidx)) {
        if (n2->type != JBV_STR) {
          return JBL_ERROR_PATCH_INVALID;
        }
        pp->from = n2->vptr;
      }
    }
  }
  *cntp = i;
  *pptr = p;
  return 0;
}

iwrc jbl_patch_from_json(JBL jbl, const char *patchjson) {
  if (!jbl || !patchjson) {
    return IW_ERROR_INVALID_ARGS;
  }
  JBL_PATCH *p;
  JBL_NODE patch;
  int cnt = (int) strlen(patchjson);
  IWPOOL *pool = iwpool_create(MAX(cnt, 1024U));
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = jbn_from_json(patchjson, &patch, pool);
  RCGO(rc, finish);
  if (patch->type == JBV_ARRAY) {
    rc = _jbl_create_patch(patch, &p, &cnt, pool);
    RCGO(rc, finish);
    rc = _jbl_patch(jbl, p, cnt, pool);
  } else if (patch->type == JBV_OBJECT) {
    // FIXME: Merge patch not implemented
    //_jbl_merge_patch_node()
    rc = IW_ERROR_NOT_IMPLEMENTED;
  } else {
    rc = JBL_ERROR_PATCH_INVALID;
  }

finish:
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_fill_from_node(JBL jbl, JBL_NODE node) {
  if (!jbl || !node) {
    return IW_ERROR_INVALID_ARGS;
  }
  if (node->type == JBV_NONE) {
    memset(jbl, 0, sizeof(*jbl));
    return 0;
  }
  binn bv = { 0 };
  iwrc rc = _jbl_binn_from_node(&bv, node);
  RCRET(rc);
  binn_free(&jbl->bn);
  memcpy(&jbl->bn, &bv, sizeof(jbl->bn));
  jbl->bn.allocated = 0;
  return rc;
}

iwrc jbl_from_node(JBL *jblp, JBL_NODE node) {
  if (!jblp || !node) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  if (node->type == JBV_OBJECT) {
    rc = jbl_create_empty_object(jblp);
  } else if (node->type == JBV_ARRAY) {
    rc = jbl_create_empty_array(jblp);
  } else {
    rc = IW_ERROR_INVALID_ARGS;
  }
  RCRET(rc);
  return jbl_fill_from_node(*jblp, node);
}

static JBL_NODE _jbl_merge_patch_node(JBL_NODE target, JBL_NODE patch, IWPOOL *pool, iwrc *rcp) {
  *rcp = 0;
  if (!patch) {
    return 0;
  }
  if (patch->type == JBV_OBJECT) {
    if (!target) {
      target = iwpool_alloc(sizeof(*target), pool);
      if (!target) {
        *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        return 0;
      }
      memset(target, 0, sizeof(*target));
      target->type = JBV_OBJECT;
      target->key = patch->key;
      target->klidx = patch->klidx;
    } else if (target->type != JBV_OBJECT) {
      _jbl_node_reset_data(target);
      target->type = JBV_OBJECT;
    }
    patch = patch->child;
    while (patch) {
      JBL_NODE patch_next = patch->next;
      if (patch->type == JBV_NULL) {
        JBL_NODE node = target->child;
        while (node) {
          if ((node->klidx == patch->klidx) && !strncmp(node->key, patch->key, node->klidx)) {
            _jbn_remove_item(target, node);
            break;
          }
          node = node->next;
        }
      } else {
        JBL_NODE node = target->child;
        while (node) {
          if ((node->klidx == patch->klidx) && !strncmp(node->key, patch->key, node->klidx)) {
            _jbl_copy_node_data(node, _jbl_merge_patch_node(node, patch, pool, rcp));
            break;
          }
          node = node->next;
        }
        if (!node) {
          _jbn_add_item(target, _jbl_merge_patch_node(0, patch, pool, rcp));
        }
      }
      patch = patch_next;
    }
    return target;
  } else {
    return patch;
  }
}

iwrc jbn_merge_patch_from_json(JBL_NODE root, const char *patchjson, IWPOOL *pool) {
  if (!root || !patchjson || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  JBL_NODE patch, res;
  iwrc rc = jbn_from_json(patchjson, &patch, pool);
  RCRET(rc);
  res = _jbl_merge_patch_node(root, patch, pool, &rc);
  RCGO(rc, finish);
  if (res != root) {
    memcpy(root, res, sizeof(*root)); // -V575
  }

finish:
  return rc;
}

iwrc jbl_merge_patch(JBL jbl, const char *patchjson) {
  if (!jbl || !patchjson) {
    return IW_ERROR_INVALID_ARGS;
  }
  binn bv;
  JBL_NODE target;
  IWPOOL *pool = iwpool_create(jbl->bn.size * 2);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_node_from_binn(&jbl->bn, &target, false, pool);
  RCGO(rc, finish);
  rc = jbn_merge_patch_from_json(target, patchjson, pool);
  RCGO(rc, finish);

  rc = _jbl_binn_from_node(&bv, target);
  RCGO(rc, finish);

  binn_free(&jbl->bn);
  memcpy(&jbl->bn, &bv, sizeof(jbl->bn));
  jbl->bn.allocated = 0;

finish:
  iwpool_destroy(pool);
  return 0;
}

iwrc jbl_merge_patch_jbl(JBL jbl, JBL patch) {
  IWXSTR *xstr = iwxstr_new();
  if (!xstr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = jbl_as_json(patch, jbl_xstr_json_printer, xstr, 0);
  RCGO(rc, finish);
  rc = jbl_merge_patch(jbl, iwxstr_ptr(xstr));
finish:
  iwxstr_destroy(xstr);
  return rc;
}

iwrc jbn_patch_auto(JBL_NODE root, JBL_NODE patch, IWPOOL *pool) {
  if (!root || !patch || !pool) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  if (patch->type == JBV_OBJECT) {
    _jbl_merge_patch_node(root, patch, pool, &rc);
  } else if (patch->type == JBV_ARRAY) {
    int cnt;
    JBL_PATCH *p;
    rc = _jbl_create_patch(patch, &p, &cnt, pool);
    RCRET(rc);
    rc = _jbl_patch_node(root, p, cnt, pool);
  } else {
    return IW_ERROR_INVALID_ARGS;
  }
  return rc;
}

iwrc jbn_merge_patch(JBL_NODE root, JBL_NODE patch, IWPOOL *pool) {
  if (!root || !patch || !pool || (root->type != JBV_OBJECT)) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  _jbl_merge_patch_node(root, patch, pool, &rc);
  return rc;
}

static const char* _jbl_ecodefn(locale_t locale, uint32_t ecode) {
  if (!((ecode > _JBL_ERROR_START) && (ecode < _JBL_ERROR_END))) {
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
    case JBL_ERROR_PARSE_INVALID_UTF8:
      return "Invalid utf8 string (JBL_ERROR_PARSE_INVALID_UTF8)";
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
    case JBL_ERROR_PATCH_INVALID_VALUE:
      return "Invalid value specified by patch (JBL_ERROR_PATCH_INVALID_VALUE)";
    case JBL_ERROR_PATCH_INVALID_ARRAY_INDEX:
      return "Invalid array index in JSON patch path (JBL_ERROR_PATCH_INVALID_ARRAY_INDEX)";
    case JBL_ERROR_PATCH_TEST_FAILED:
      return "JSON patch test operation failed (JBL_ERROR_PATCH_TEST_FAILED)";
    case JBL_ERROR_NOT_AN_OBJECT:
      return "JBL is not an object (JBL_ERROR_NOT_AN_OBJECT)";
    case JBL_ERROR_TYPE_MISMATCHED:
      return "Type of JBL object mismatched user type constraints (JBL_ERROR_TYPE_MISMATCHED)";
    case JBL_ERROR_MAX_NESTING_LEVEL_EXCEEDED:
      return "Exceeded the maximal object nesting level: " _STR(JBL_MAX_NESTING_LEVEL)
             " (JBL_ERROR_MAX_NESTING_LEVEL_EXCEEDED)";
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
