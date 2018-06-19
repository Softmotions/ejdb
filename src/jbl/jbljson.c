#include "jbl.h"
#include "utf8proc.h"
#include "jbl_internal.h"

#define IS_WHITESPACE(c_) ((unsigned char)(c_) <= (unsigned char) ' ')

/** JSON parsing context */
typedef struct JCTX {
  IWPOOL *pool;
  JBLNODE root;
  const char *buf;
  const char *sp;
  iwrc rc;
} JCTX;

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

static JBLNODE _jbl_json_create_node(jbl_type_t type, const char *key, int klidx, JBLNODE parent, JCTX *ctx) {
                                
  JBLNODE node = iwpool_calloc(sizeof(*node), ctx->pool);
  if (!node) {
    ctx->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return 0;
  }
  node->type = type;
  node->key = key;
  node->klidx = klidx;
  if (parent) {
    _jbl_add_item(parent, node);
  }
  if (!ctx->root) {
    ctx->root = node;
  }
  return node;
}

IW_INLINE void _jbl_skip_bom(JCTX *ctx) {
  const char *p = ctx->buf;
  if (p[0] == '\xEF' && p[1] == '\xBB' && p[2] == '\xBF') {
    ctx->buf += 3;
  }
}

IW_INLINE int _jbl_hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int _jbl_unescape_raw_json_string(const char *p, char *d, int dlen, const char **end, iwrc *rcp) {
  *rcp = 0;
  char c;
  char *ds = d;
  char *de = d + dlen;
  
  while ((c = *p++)) {
    if (c == '"') { // string closing quotes
      if (end) *end = p;
      return d - ds;
    } else if (c == '\\') {
      switch (*p) {
        case '\\':
        case '/':
        case '"':
          if (d < de) *d = *p;
          ++p, ++d;
          break;
        case 'b':
          if (d < de) *d = '\b';
          ++p, ++d;
          break;
        case 'f':
          if (d < de) *d = '\f';
          ++p, ++d;
          break;
        case 'n':
          if (d < de) *d = '\n';
          ++p, ++d;
          break;
        case 'r':
          if (d < de) *d = '\n';
          ++p, ++d;
          break;
        case 't':
          if (d < de) *d = '\t';
          ++p, ++d;
          break;
        case 'u': {
          uint32_t cp, cp2;
          int h1, h2, h3, h4;
          if ((h1 = _jbl_hex(p[1])) < 0 || (h2 = _jbl_hex(p[2])) < 0
              || (h3 = _jbl_hex(p[3])) < 0 || (h4 = _jbl_hex(p[4])) < 0) {
            *rcp = JBL_ERROR_PARSE_INVALID_CODEPOINT;
            return 0;
          }
          cp = h1 << 12 | h2 << 8 | h3 << 4 | h4;
          if ((cp & 0xfc00) == 0xd800) {
            p += 6;
            if (p[-1] != '\\' || *p != 'u'
                || (h1 = _jbl_hex(p[1])) < 0 || (h2 = _jbl_hex(p[2])) < 0
                || (h3 = _jbl_hex(p[3])) < 0 || (h4 = _jbl_hex(p[4])) < 0) {
              *rcp = JBL_ERROR_PARSE_INVALID_CODEPOINT;
              return 0;
            }
            cp2 = h1 << 12 | h2 << 8 | h3 << 4 | h4;
            if ((cp2 & 0xfc00) != 0xdc00) {
              *rcp = JBL_ERROR_PARSE_INVALID_CODEPOINT;
              return 0;
            }
            cp = 0x10000 + ((cp - 0xd800) << 10) + (cp2 - 0xdc00);
          }
          if (!utf8proc_codepoint_valid(cp)) {
            *rcp = JBL_ERROR_PARSE_INVALID_CODEPOINT;
            return 0;
          }
          uint8_t uchars[4];
          utf8proc_ssize_t ulen = utf8proc_encode_char(cp, uchars);
          assert(ulen <= sizeof(uchars));
          for (int i = 0; i < ulen; ++i) {
            if (d < de) *d = uchars[i];
            ++d;
          }
          p += 5;
          break;
        }
        default:
          if (d < de) *d = c;
          ++d;
      }
    } else {
      if (d < de) *d = c;
      ++d;
    }
  }
  *rcp = JBL_ERROR_PARSE_UNQUOTED_STRING;
  return 0;
}

static const char *_jbl_parse_key(const char **key, const char *p, JCTX *ctx) {
  char c;
  while ((c = *p++)) {
    if (c == '"') {
      int len = _jbl_unescape_raw_json_string(p, 0, 0, 0, &ctx->rc);
      if (ctx->rc) return 0;
      if (len) {
        char *kptr = iwpool_alloc(len + 1, ctx->pool);
        if (!kptr) {
          ctx->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
          return 0;
        }
        if (len != _jbl_unescape_raw_json_string(p, kptr, len, &p, &ctx->rc) || ctx->rc)  {
          if (!ctx->rc) ctx->rc = JBL_ERROR_PARSE_JSON;
          return 0;
        }
        kptr[len] =  '\0';
        *key = kptr;
      } else {
        *key = "";
      }
      while (*p && IS_WHITESPACE(*p)) p++;
      if (*p == ':') return p + 1;
      ctx->rc = JBL_ERROR_PARSE_JSON;
      return 0;
    } else if (c == '}') {
      return p - 1;
    } else if (IS_WHITESPACE(c) || c == ',') {
      continue;
    } else {
      ctx->rc = JBL_ERROR_PARSE_JSON;
      return 0;
    }
  }
  return p;
}

static const char *_jbl_parse_value(JBLNODE parent,
                                    const char *key, int klidx,
                                    const char *p,
                                    JCTX *ctx) {
  JBLNODE node;
  while (1) {
    switch (*p) {
      case '\0':
        ctx->rc = JBL_ERROR_PARSE_JSON;
        return 0;
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      case ',':
        ++p;
        break;
      case 'n':
        if (!strncmp(p, "null", 4)) {
          node = _jbl_json_create_node(JBV_NULL, key, klidx, parent, ctx);
          if (ctx->rc) return 0;
          return p + 4;
        }
        ctx->rc = JBL_ERROR_PARSE_JSON;
        return 0;
      case 't':
        if (!strncmp(p, "true", 4)) {
          node = _jbl_json_create_node(JBV_BOOL, key, klidx, parent, ctx);
          if (ctx->rc) return 0;
          node->vbool = true;
          return p + 4;
        }
        ctx->rc = JBL_ERROR_PARSE_JSON;
        return 0;
      case 'f':
        if (!strncmp(p, "false", 5)) {
          node = _jbl_json_create_node(JBV_BOOL, key, klidx, parent, ctx);
          if (ctx->rc) return 0;
          node->vbool = false;
          return p + 5;
        }
        ctx->rc = JBL_ERROR_PARSE_JSON;
        return 0;
      case '"':
        ++p;
        int len = _jbl_unescape_raw_json_string(p, 0, 0, 0, &ctx->rc);
        if (ctx->rc) return 0;
        node = _jbl_json_create_node(JBV_STR, key, klidx, parent, ctx);
        if (ctx->rc) return 0;
        if (len) {
          char *vptr = iwpool_alloc(len + 1, ctx->pool);
          if (!vptr) {
            ctx->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
            return 0;
          }
          if (len != _jbl_unescape_raw_json_string(p, vptr, len, &p, &ctx->rc) || ctx->rc)  {
            if (!ctx->rc) ctx->rc = JBL_ERROR_PARSE_JSON;
            return 0;
          }
          vptr[len] = '\0';
          node->vptr = vptr;
          node->vsize = len;
        } else {
          node->vptr = "";
          node->vsize = 0;
        }
        return p;
      case '{':
        node = _jbl_json_create_node(JBV_OBJECT, key, klidx, parent, ctx);
        if (ctx->rc) return 0;
        ++p;
        while (1) {
          const char *nkey;
          p = _jbl_parse_key(&nkey, p, ctx);
          if (ctx->rc) return 0;
          if (*p == '}') return p + 1; // end of object
          p = _jbl_parse_value(node, nkey, strlen(nkey), p, ctx);
          if (ctx->rc) return 0;
        }
        break;
      case '[':
        node = _jbl_json_create_node(JBV_ARRAY, key, klidx, parent, ctx);
        if (ctx->rc) return 0;
        ++p;
        for (int i = 0;; ++i) {
          p = _jbl_parse_value(node, 0, i, p, ctx);
          if (ctx->rc) return 0;
          if (*p == ']') return p + 1;
        }
        break;
      case ']':
        return p;
        break;
      case '-':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        node = _jbl_json_create_node(JBV_I64, key, klidx, parent, ctx);
        if (ctx->rc) return 0;
        char *pe;
        node->vi64 = strtoll(p, &pe, 0);
        if (pe == p || errno == ERANGE) {
          ctx->rc = JBL_ERROR_PARSE_JSON;
          return 0;
        }
        if (*pe == '.' || *pe == 'e' || *pe == 'E') {
          node->type = JBV_F64;
          node->vf64 = strtod(p, &pe);
          if (pe == p || errno == ERANGE) {
            ctx->rc = JBL_ERROR_PARSE_JSON;
            return 0;
          }
        }
        return pe;
      }
    }
  }
  
  return p;
}

iwrc _jbl_node_as_json(JBLNODE node, jbl_json_printer pt, void *op, int lvl, jbl_print_flags_t pf) {
  iwrc rc = 0;
  bool pretty = pf & JBL_PRINT_PRETTY;
  
#define PT(data_, size_, ch_, count_) do {\
    rc = pt(data_, size_, ch_, count_, op); \
    RCRET(rc); \
  } while(0)
  
  switch (node->type) {  
    case JBV_ARRAY:
      PT(0, 0, '[', 1);
      if (node->child && pretty) PT(0, 0, '\n', 1);
      for (JBLNODE n = node->child; n; n = n->next) {
        if (pretty) PT(0, 0, ' ', lvl + 1);
        rc = _jbl_node_as_json(n, pt, op, lvl + 1, pf);
        RCRET(rc);
        if (n->next) PT(0, 0, ',', 1);
        if (pretty) PT(0, 0, '\n', 1);
      }
      if (node->child && pretty) PT(0, 0, ' ', lvl);
      PT(0, 0, ']', 1);
      break;
    case  JBV_OBJECT:
      PT(0, 0, '{', 1);
      if (node->child && pretty) PT(0, 0, '\n', 1);
      for (JBLNODE n = node->child; n; n = n->next) {
        if (pretty) PT(0, 0, ' ', lvl + 1);
        rc = _jbl_write_string(n->key, n->klidx, pt, op, pf);
        RCRET(rc);
        if (pretty) {
          PT(": ", -1, 0, 0);
        } else {
          PT(0, 0, ':', 1);
        }
        rc = _jbl_node_as_json(n, pt, op, lvl + 1, pf);
        RCRET(rc);
        if (n->next) PT(0, 0, ',', 1);
        if (pretty) PT(0, 0, '\n', 1);
      }
      if (node->child && pretty) PT(0, 0, ' ', lvl);
      PT(0, 0, '}', 1);
      break;      
    case JBV_STR:
      rc  = _jbl_write_string(node->vptr, node->vsize, pt, op, pf);
      break;
    case JBV_I64:
      rc = _jbl_write_int(node->vi64, pt, op);
      break;
    case JBV_F64:
      rc = _jbl_write_double(node->vf64, pt, op);
      break;
    case JBV_BOOL:
      if (node->vbool) {
        PT("true", 4, 0, 1);
      } else {
        PT("false", 5, 0, 1);
      }
      break;
    case JBV_NULL:
      PT("null", 4, 0, 1);
      break;              
    default:
      return IW_ERROR_ASSERTION;        
  }  
#undef PT
  return rc;
}

//------ Public

iwrc jbl_node_as_json(JBLNODE node, jbl_json_printer pt, void *op, jbl_print_flags_t pf) {
  return _jbl_node_as_json(node, pt, op, 0, pf);
}

iwrc jbl_node_from_json(const char *json, JBLNODE *node, IWPOOL *pool) {
  *node = 0;
  JCTX ctx = {
    .pool = pool,
    .buf = json
  };
  _jbl_skip_bom(&ctx);
  _jbl_parse_value(0, 0, 0, ctx.buf, &ctx);
  *node = ctx.root;
  return ctx.rc;
}
