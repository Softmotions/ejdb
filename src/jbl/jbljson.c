#include "jbl.h"
#include "jbl_internal.h"
#include "utf8proc.h"

// static iwrc _jbl_node_from_json(nx_json *nxjson, JBLNODE parent, const char *key, int idx, JBLNODE *node, IWPOOL *pool) {
//   iwrc rc = 0;
//   JBLNODE n = iwpool_alloc(sizeof(*n), pool);
//   if (!n) {
//     return iwrc_set_errno(IW_ERROR_ALLOC, errno);
//   }
//   memset(n, 0, sizeof(*n));
//   n->key = key;
//   n->klidx = n->key ? strlen(n->key) : idx;

//   switch (nxjson->type) {
//     case NX_JSON_OBJECT:
//       n->type = JBV_OBJECT;
//       for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
//         rc = _jbl_node_from_json(nxj, n, nxj->key, 0, 0, pool);
//         RCGO(rc, finish);
//       }
//       break;
//     case NX_JSON_ARRAY:
//       n->type = JBV_ARRAY;
//       int i = 0;
//       for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
//         rc = _jbl_node_from_json(nxj, n, 0, i++, 0, pool);
//         RCGO(rc, finish);
//       }
//       break;
//     case NX_JSON_STRING:
//       n->type = JBV_STR;
//       n->vsize = strlen(nxjson->text_value) + 1;
//       n->vptr = iwpool_strndup(pool, nxjson->text_value, n->vsize, &rc);
//       RCGO(rc, finish);
//       break;
//     case NX_JSON_INTEGER:
//       n->type = JBV_I64;
//       n->vi64 = nxjson->int_value;
//       break;
//     case NX_JSON_DOUBLE:
//       n->type = JBV_F64;
//       n->vf64 = nxjson->dbl_value;
//       break;
//     case NX_JSON_BOOL:
//       n->type = JBV_BOOL;
//       n->vbool = nxjson->int_value;
//       break;
//     case NX_JSON_NULL:
//       n->type = JBV_NULL;
//       break;
//     default:
//       break;
//   }

// finish:
//   if (!rc) {
//     if (parent) {
//       _jbl_add_item(parent, n);
//     }
//     if (node) {
//       *node = n;
//     }
//   } else if (node) {
//     *node = 0;
//   }
//   return rc;
// }

// iwrc jbl_node_from_json(const char *_json, JBLNODE *node, IWPOOL *pool) {
//   if (!_json || !node || !pool) {
//     return IW_ERROR_INVALID_ARGS;
//   }
//   iwrc rc;
//   char *json = iwpool_strdup(pool, _json, &rc);
//   RCRET(rc);
//   nx_json *nxj = nx_json_parse_utf8(json);
//   if (!nxj) {
//     rc = JBL_ERROR_PARSE_JSON;
//     goto finish;
//   }
//   rc = _jbl_node_from_json(nxj, 0, 0, 0, node, pool);
// finish:
//   if (nxj) {
//     nx_json_free(nxj);
//   }
//   return rc;
// }

#define IS_WHITESPACE(c) ((unsigned char)(c)<=(unsigned char)' ')

/** JSON parsing context */
typedef struct JCTX {
  IWPOOL *pool;
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

static JBLNODE _jbl_create_node(jbl_type_t type,
                                const char *key, int klidx,
                                JBLNODE parent, JCTX *ctx) {

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
  return node;
}

static void _jbl_skip_bom(JCTX *ctx) {
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

int _jbl_unescape_raw_json_string(const char *p, char *d, int dlen, const char **end, iwrc *rcp) {
  *rcp = 0;
  char c;
  char *sd = d;
  char *ed = d + dlen;

  while ((c = *p++)) {
    if (c == '"') { // string closing quotes
      if (end) *end = p;
      return d - sd;
    } else if (c == '\\') {
      switch (*p) {
        case '\\':
        case '/':
        case '"':
          if (d < ed) *d = *p;
          ++p, ++d;
          break;
        case 'b':
          if (d < ed) *d = '\b';
          ++p, ++d;
          break;
        case 'f':
          if (d < ed) *d = '\f';
          ++p, ++d;
          break;
        case 'n':
          if (d < ed) *d = '\n';
          ++p, ++d;
          break;
        case 'r':
          if (d < ed) *d = '\n';
          ++p, ++d;
          break;
        case 't':
          if (d < ed) *d = '\t';
          ++p, ++d;
          break;
        case 'u': {
          int h1, h2, h3, h4;
          if ((h1 = _jbl_hex(p[1])) < 0 || (h2 = _jbl_hex(p[2])) < 0 || (h3 = _jbl_hex(p[3])) < 0 || (h4 = _jbl_hex(p[4])) < 0) {
            *rcp = JBL_ERROR_PARSE_INVALID_CODEPOINT;
            return 0;
          }
          uint32_t cp = h1 << 12 | h2 << 8 | h3 << 4 | h4, cp2;
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
          uint8_t uchars[8];
          utf8proc_ssize_t ulen = utf8proc_encode_char(cp, uchars);
          assert(ulen <= sizeof(uchars));
          for (int i = 0; i < ulen; ++i) {
            if (d < ed) *d = uchars[i];
            ++d;
          }
          p += 5;
          break;
        }

        default:
          if (d < ed) *d = c;
          ++d;
      }

    } else {
      if (d < ed) *d = c;
      ++d;
    }
  }
  *rcp = JBL_ERROR_PARSE_UNQUOTED_STRING;
  return 0;
}

static char *_jbl_parse_key(const char **key, const char *p, JCTX *ctx) {
  // todo
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
          node = _jbl_create_node(JBV_NULL, key, klidx, parent, ctx);
          if (ctx->rc) return 0;
          return p + 4;
        }
        ctx->rc = JBL_ERROR_PARSE_JSON;
        return 0;
      case 't':
        if (!strncmp(p, "true", 4)) {
          node = _jbl_create_node(JBV_BOOL, key, klidx, parent, ctx);
          if (ctx->rc) return 0;
          node->vbool = true;
          return p + 4;
        }
        ctx->rc = JBL_ERROR_PARSE_JSON;
        return 0;
      case 'f':
        if (!strncmp(p, "false", 5)) {
          node = _jbl_create_node(JBV_BOOL, key, klidx, parent, ctx);
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
        node = _jbl_create_node(JBV_STR, key, klidx, parent, ctx);
        if (ctx->rc) return 0;
        if (!len) {
          node->vptr = "";
          node->vsize = 0;
        } else {
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
        }
        return p;
      case '{':
        node = _jbl_create_node(JBV_OBJECT, key, klidx, parent, ctx);
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
        break;
      case ']':
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
        node = _jbl_create_node(JBV_I64, key, klidx, parent, ctx);
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

iwrc jbl_node_from_json(const char *json, JBLNODE *node, IWPOOL *pool) {
  JCTX ctx = {
    .pool = pool,
    .buf = json,
    .sp = json
  };
  _jbl_skip_bom(&ctx);
  _jbl_parse_value(0, 0, 0, ctx.buf, &ctx);

  return ctx.rc;
}
