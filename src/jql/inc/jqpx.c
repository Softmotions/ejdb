#include "jqp.h"

#include <iowow/iwconv.h>
#include <iowow/utf8proc.h>

#include <stdlib.h>
#include <string.h>

#define MAX_ORDER_BY_CLAUSES 64

#define JQRC(yy_, rc_) do {                \
          iwrc __rc = (rc_);               \
          if (__rc) _jqp_fatal(yy_, __rc); \
} while (0)

static void _jqp_debug(yycontext *yy, const char *text) {
  fprintf(stderr, "TEXT=%s\n", text);
}

static void _jqp_fatal(yycontext *yy, iwrc rc) {
  struct jqp_aux *aux = yy->aux;
  aux->rc = rc;
  longjmp(aux->fatal_jmp, 1);
}

static void* _jqp_malloc(struct _yycontext *yy, size_t size) {
  void *ret = malloc(size);
  if (!ret) {
    struct jqp_aux *aux = yy->aux;
    aux->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    longjmp(aux->fatal_jmp, 1);
  }
  return ret;
}

static void* _jqp_realloc(struct _yycontext *yy, void *ptr, size_t size) {
  void *ret = realloc(ptr, size);
  if (!ret) {
    struct jqp_aux *aux = yy->aux;
    aux->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    longjmp(aux->fatal_jmp, 1);
  }
  return ret;
}

static iwrc _jqp_aux_set_input(struct jqp_aux *aux, const char *input) {
  size_t len = strlen(input) + 1;
  char *buf = iwpool_alloc(len, aux->pool);
  if (!buf) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  memcpy(buf, input, len);
  aux->buf = buf;
  return 0;
}

//-----------------

IW_INLINE char* _jqp_strdup(struct _yycontext *yy, const char *text) {
  iwrc rc = 0;
  char *ret = iwpool_strdup(yy->aux->pool, text, &rc);
  JQRC(yy, rc);
  return ret;
}

static union jqp_unit* _jqp_unit(yycontext *yy) {
  union jqp_unit *ret = iwpool_calloc(sizeof(union jqp_unit), yy->aux->pool);
  if (!ret) {
    JQRC(yy, iwrc_set_errno(IW_ERROR_ALLOC, errno));
  }
  return ret;
}

static struct jqp_stack* _jqp_push(yycontext *yy) {
  struct jqp_aux *aux = yy->aux;
  struct jqp_stack *stack;
  if (aux->stackn < (sizeof(aux->stackpool) / sizeof(aux->stackpool[0]))) {
    stack = &aux->stackpool[aux->stackn++];
  } else {
    stack = malloc(sizeof(*aux->stack));
    if (!stack) {
      JQRC(yy, iwrc_set_errno(IW_ERROR_ALLOC, errno));
    }
    aux->stackn++;
  }
  memset(stack, 0, sizeof(*stack)); // -V575
  stack->next = 0;
  if (!aux->stack) {
    stack->prev = 0;
  } else {
    aux->stack->next = stack;
    stack->prev = aux->stack;
  }
  aux->stack = stack;
  return aux->stack;
}

static struct jqp_stack _jqp_pop(yycontext *yy) {
  struct jqp_aux *aux = yy->aux;
  struct jqp_stack *stack = aux->stack, ret;
  if (!stack || (aux->stackn < 1)) {
    iwlog_error2("Unbalanced stack");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  aux->stack = stack->prev;
  if (aux->stack) {
    aux->stack->next = 0;
  }
  stack->prev = 0;
  stack->next = 0;
  ret = *stack;
  if (aux->stackn-- > (sizeof(aux->stackpool) / sizeof(aux->stackpool[0]))) {
    free(stack);
  }
  return ret;
}

static void _jqp_unit_push(yycontext *yy, union jqp_unit *unit) {
  struct jqp_stack *stack = _jqp_push(yy);
  stack->type = STACK_UNIT;
  stack->unit = unit;
}

static union jqp_unit* _jqp_unit_pop(yycontext *yy) {
  struct jqp_stack stack = _jqp_pop(yy);
  if (stack.type != STACK_UNIT) {
    iwlog_error("Unexpected type: %d", stack.type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return stack.unit;
}

static void _jqp_string_push(yycontext *yy, char *str, bool dup) {
  struct jqp_stack *stack = _jqp_push(yy);
  stack->type = STACK_STRING;
  stack->str = str;
  if (dup) {
    iwrc rc = 0;
    struct jqp_aux *aux = yy->aux;
    stack->str = iwpool_strdup(aux->pool, stack->str, &rc);
    if (rc) {
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
  }
}

static char* _jqp_string_pop(yycontext *yy) {
  struct jqp_stack stack = _jqp_pop(yy);
  if (stack.type != STACK_STRING) {
    iwlog_error("Unexpected type: %d", stack.type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return stack.str;
}

static union jqp_unit* _jqp_string(yycontext *yy, jqp_string_flavours_t flavour, const char *text) {
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_STRING_TYPE;
  unit->string.flavour |= flavour;
  unit->string.value = _jqp_strdup(yy, text);
  return unit;
}

static union jqp_unit* _jqp_number(yycontext *yy, jqp_int_flavours_t flavour, const char *text) {
  union jqp_unit *unit = _jqp_unit(yy);
  char *eptr;
  int64_t ival = strtoll(text, &eptr, 0);
  if ((eptr == text) || (errno == ERANGE)) {
    iwlog_error("Invalid number: %s", text);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if ((*eptr == '.') || (*eptr == 'e') || (*eptr == 'E')) {
    unit->type = JQP_DOUBLE_TYPE;
    unit->dblval.value = strtod(text, &eptr);
    if ((eptr == text) || (errno == ERANGE)) {
      iwlog_error("Invalid double number: %s", text);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    unit->dblval.flavour |= flavour;
  } else {
    unit->type = JQP_INTEGER_TYPE;
    unit->intval.value = ival;
    unit->intval.flavour |= flavour;
  }
  return unit;
}

static union jqp_unit* _jqp_json_number(yycontext *yy, const char *text) {
  union jqp_unit *unit = _jqp_unit(yy);
  char *eptr;
  unit->type = JQP_JSON_TYPE;
  int64_t ival = strtoll(text, &eptr, 0);
  if ((eptr == text) || (errno == ERANGE)) {
    iwlog_error("Invalid number: %s", text);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if ((*eptr == '.') || (*eptr == 'e') || (*eptr == 'E')) {
    unit->json.jn.type = JBV_F64;
    unit->json.jn.vf64 = strtod(text, &eptr);
    if ((eptr == text) || (errno == ERANGE)) {
      iwlog_error("Invalid double number: %s", text);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
  } else {
    unit->json.jn.type = JBV_I64;
    unit->json.jn.vi64 = ival;
  }
  return unit;
}

static union jqp_unit* _jqp_placeholder(yycontext *yy, const char *text) {
  struct jqp_aux *aux = yy->aux;
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_STRING_TYPE;
  unit->string.flavour |= JQP_STR_PLACEHOLDER;
  if (text[0] == '?') {
    char nbuf[IWNUMBUF_SIZE + 1];
    nbuf[0] = '?';
    int len = iwitoa(aux->num_placeholders++, nbuf + 1, IWNUMBUF_SIZE);
    nbuf[len + 1] = '\0';
    unit->string.value = _jqp_strdup(yy, nbuf);
  } else {
    unit->string.value = _jqp_strdup(yy, text);
  }
  if (!aux->start_placeholder) {
    aux->start_placeholder = &unit->string;
    aux->end_placeholder = aux->start_placeholder;
  } else {
    aux->end_placeholder->placeholder_next = &unit->string;
    aux->end_placeholder = aux->end_placeholder->placeholder_next;
  }
  return unit;
}

IW_INLINE int _jql_hex(char c) {
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'a') && (c <= 'f')) {
    return c - 'a' + 10;
  }
  if ((c >= 'A') && (c <= 'F')) {
    return c - 'A' + 10;
  }
  return -1;
}

static int _jqp_unescape_json_string(const char *p, char *d, int dlen, iwrc *rcp) {
  *rcp = 0;
  char c;
  char *ds = d;
  char *de = d + dlen;

  while (1) {
    c = *p++;
    if (c == '\0') {
      return d - ds;
    } else if (c == '\\') {
      switch (*p) {
        case '\\':
        case '/':
        case '"':
          if (d < de) {
            *d = *p;
          }
          ++p, ++d;
          break;
        case 'b':
          if (d < de) {
            *d = '\b';
          }
          ++p, ++d;
          break;
        case 'f':
          if (d < de) {
            *d = '\f';
          }
          ++p, ++d;
          break;
        case 'n':
        case 'r':
          if (d < de) {
            *d = '\n';
          }
          ++p, ++d;
          break;
        case 't':
          if (d < de) {
            *d = '\t';
          }
          ++p, ++d;
          break;
        case 'u': {
          uint32_t cp, cp2;
          int h1, h2, h3, h4;
          if (  ((h1 = _jql_hex(p[1])) < 0) || ((h2 = _jql_hex(p[2])) < 0)
             || ((h3 = _jql_hex(p[3])) < 0) || ((h4 = _jql_hex(p[4])) < 0)) {
            *rcp = JBL_ERROR_PARSE_INVALID_CODEPOINT;
            return 0;
          }
          cp = h1 << 12 | h2 << 8 | h3 << 4 | h4;
          if ((cp & 0xfc00) == 0xd800) {
            p += 6;
            if (  (p[-1] != '\\') || (*p != 'u')
               || ((h1 = _jql_hex(p[1])) < 0) || ((h2 = _jql_hex(p[2])) < 0)
               || ((h3 = _jql_hex(p[3])) < 0) || ((h4 = _jql_hex(p[4])) < 0)) {
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
          for (int i = 0; i < ulen; ++i) {
            if (d < de) {
              *d = uchars[i];
            }
            ++d;
          }
          p += 5;
          break;
        }
        default:
          if (d < de) {
            *d = c;
          }
          ++d;
      }
    } else {
      if (d < de) {
        *d = c;
      }
      ++d;
    }
  }
  *rcp = JQL_ERROR_QUERY_PARSE;
  return 0;
}

static union jqp_unit* _jqp_unescaped_string(struct _yycontext *yy, jqp_string_flavours_t flv, const char *text) {
  struct jqp_aux *aux = yy->aux;
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_STRING_TYPE;
  unit->string.flavour |= flv;
  int len = _jqp_unescape_json_string(text, 0, 0, &aux->rc);
  if (aux->rc) {
    JQRC(yy, aux->rc);            // -V547
  }
  char *dest = iwpool_alloc(len + 1, aux->pool);
  if (!dest) {
    JQRC(yy, iwrc_set_errno(IW_ERROR_ALLOC, errno));
  }
  _jqp_unescape_json_string(text, dest, len, &aux->rc);
  if (aux->rc) {
    JQRC(yy, aux->rc);            // -V547
  }
  dest[len] = '\0'; // -V1004
  unit->string.value = dest;
  return unit;
}

static union jqp_unit* _jqp_json_string(struct _yycontext *yy, const char *text) {
  struct jqp_aux *aux = yy->aux;
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_JSON_TYPE;
  unit->json.jn.type = JBV_STR;
  int len = _jqp_unescape_json_string(text, 0, 0, &aux->rc);
  if (aux->rc) {
    JQRC(yy, aux->rc);            // -V547
  }
  char *dest = iwpool_alloc(len + 1, aux->pool);
  if (!dest) {
    JQRC(yy, iwrc_set_errno(IW_ERROR_ALLOC, errno));
  }
  _jqp_unescape_json_string(text, dest, len, &aux->rc);
  if (aux->rc) {
    JQRC(yy, aux->rc);            // -V547
  }
  dest[len] = '\0'; // -V1004
  unit->json.jn.vptr = dest;
  unit->json.jn.vsize = len;
  return unit;
}

static union jqp_unit* _jqp_json_pair(yycontext *yy, union jqp_unit *key, union jqp_unit *val) {
  if ((key->type != JQP_JSON_TYPE) || (val->type != JQP_JSON_TYPE) || (key->json.jn.type != JBV_STR)) {
    iwlog_error2("Invalid arguments");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  val->json.jn.key = key->json.jn.vptr;
  val->json.jn.klidx = key->json.jn.vsize;
  return val;
}

static union jqp_unit* _jqp_json_collect(yycontext *yy, jbl_type_t type, union jqp_unit *until) {
  struct jqp_aux *aux = yy->aux;
  union jqp_unit *ret = _jqp_unit(yy);
  ret->type = JQP_JSON_TYPE;
  struct jbl_node *jn = &ret->json.jn;
  jn->type = type;
  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit == until) {
      _jqp_pop(yy);
      break;
    }
    if (unit->type != JQP_JSON_TYPE) {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    struct jbl_node *ju = &unit->json.jn;
    if (!jn->child) {
      jn->child = ju;
    } else {
      ju->next = jn->child;
      ju->prev = jn->child->prev;
      jn->child->prev = ju;
      jn->child = ju;
    }
    _jqp_pop(yy);
  }
  return ret;
}

static union jqp_unit* _jqp_json_true_false_null(yycontext *yy, const char *text) {
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_JSON_TYPE;
  int len = strlen(text);
  if (!strncmp("null", text, len)) {
    unit->json.jn.type = JBV_NULL;
  } else if (!strncmp("true", text, len)) {
    unit->json.jn.type = JBV_BOOL;
    unit->json.jn.vbool = true;
  } else if (!strncmp("false", text, len)) {
    unit->json.jn.type = JBV_BOOL;
    unit->json.jn.vbool = false;
  } else {
    iwlog_error("Invalid json value: %s", text);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return unit;
}

static void _jqp_op_negate(yycontext *yy) {
  yy->aux->negate = true;
}

static void _jqp_op_negate_reset(yycontext *yy) {
  yy->aux->negate = false;
}

static union jqp_unit* _jqp_unit_op(yycontext *yy, const char *text) {
  struct jqp_aux *aux = yy->aux;
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_OP_TYPE;
  unit->op.negate = aux->negate;
  aux->negate = false;
  if (!strcmp(text, "=") || !strcmp(text, "eq")) {
    unit->op.value = JQP_OP_EQ;
  } else if (!strcmp(text, ">") || !strcmp(text, "gt")) {
    unit->op.value = JQP_OP_GT;
  } else if (!strcmp(text, ">=") || !strcmp(text, "gte")) {
    unit->op.value = JQP_OP_GTE;
  } else if (!strcmp(text, "<") || !strcmp(text, "lt")) {
    unit->op.value = JQP_OP_LT;
  } else if (!strcmp(text, "<=") || !strcmp(text, "lte")) {
    unit->op.value = JQP_OP_LTE;
  } else if (!strcmp(text, "in")) {
    unit->op.value = JQP_OP_IN;
  } else if (!strcmp(text, "ni")) {
    unit->op.value = JQP_OP_NI;
  } else if (!strcmp(text, "re")) {
    unit->op.value = JQP_OP_RE;
  } else if (!(strcmp(text, "~"))) {
    unit->op.value = JQP_OP_PREFIX;
  } else {
    iwlog_error("Invalid operation: %s", text);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (!aux->start_op) {
    aux->start_op = &unit->op;
    aux->end_op = aux->start_op;
  } else {
    aux->end_op->next = &unit->op;
    aux->end_op = aux->end_op->next;
  }
  return unit;
}

static union jqp_unit* _jqp_unit_join(yycontext *yy, const char *text) {
  struct jqp_aux *aux = yy->aux;
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_JOIN_TYPE;
  unit->join.negate = aux->negate;
  aux->negate = false;
  if (!strcmp(text, "and")) {
    unit->join.value = JQP_JOIN_AND;
  } else if (!strcmp(text, "or")) {
    unit->join.value = JQP_JOIN_OR;
  }
  return unit;
}

static union jqp_unit* _jqp_expr(yycontext *yy, union jqp_unit *left, union jqp_unit *op, union jqp_unit *right) {
  if (!left || !op || !right) {
    iwlog_error2("Invalid arguments");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if ((op->type != JQP_OP_TYPE) && (op->type != JQP_JOIN_TYPE)) {
    iwlog_error("Unexpected type: %d", op->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_EXPR_TYPE;
  unit->expr.left = left;
  unit->expr.op = &op->op;
  unit->expr.right = right;
  return unit;
}

static union jqp_unit* _jqp_pop_expr_chain(yycontext *yy, union jqp_unit *until) {
  union jqp_unit *expr = 0;
  struct jqp_aux *aux = yy->aux;
  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit->type == JQP_EXPR_TYPE) {
      if (expr) {
        unit->expr.next = &expr->expr;
      }
      expr = unit;
    } else if ((unit->type == JQP_JOIN_TYPE) && expr) {
      expr->expr.join = &unit->join;
    } else {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    _jqp_pop(yy);
    if (unit == until) {
      break;
    }
  }
  return expr;
}

static union jqp_unit* _jqp_projection(struct _yycontext *yy, union jqp_unit *value, uint8_t flags) {
  if (value->type != JQP_STRING_TYPE) {
    iwlog_error("Unexpected type: %d", value->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_PROJECTION_TYPE;
  unit->projection.value = &value->string;
  unit->projection.flags |= flags;
  return unit;
}

static union jqp_unit* _jqp_pop_projection_nodes(yycontext *yy, union jqp_unit *until) {
  union jqp_unit *first = 0;
  struct jqp_aux *aux = yy->aux;
  uint8_t flags = 0;

  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit->type != JQP_STRING_TYPE) {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    if (first) {
      unit->string.next = &first->string;
    } else if (unit->string.flavour & JQP_STR_PROJFIELD) {
      for (struct jqp_string *s = &unit->string; s; s = s->subnext) {
        if (s->flavour & JQP_STR_PROJOIN) {
          flags |= JQP_PROJECTION_FLAG_JOINS;
        } else {
          flags |= JQP_PROJECTION_FLAG_INCLUDE;
        }
      }
    } else if (strchr(unit->string.value, '<')) { // JOIN Projection?
      unit->string.flavour |= JQP_STR_PROJOIN;
      flags |= JQP_PROJECTION_FLAG_JOINS;
    } else {
      unit->string.flavour |= JQP_STR_PROJPATH;
    }
    first = unit;
    _jqp_pop(yy);
    if (unit == until) {
      break;
    }
  }
  if (!flags) {
    flags |= JQP_PROJECTION_FLAG_INCLUDE;
  }
  return _jqp_projection(yy, first, flags);
}

static union jqp_unit* _jqp_push_joined_projection(struct _yycontext *yy, union jqp_unit *p) {
  struct jqp_aux *aux = yy->aux;
  if (!aux->stack || (aux->stack->type != STACK_STRING)) {
    iwlog_error2("Invalid stack state");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (aux->stack->str[0] == '-') {
    p->projection.flags &= ~JQP_PROJECTION_FLAG_INCLUDE;
    p->projection.flags |= JQP_PROJECTION_FLAG_EXCLUDE;
  }
  _jqp_pop(yy);
  _jqp_unit_push(yy, p);
  return p;
}

static union jqp_unit* _jqp_pop_joined_projections(yycontext *yy, union jqp_unit *until) {
  union jqp_unit *first = 0;
  struct jqp_aux *aux = yy->aux;
  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit->type != JQP_PROJECTION_TYPE) {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    if (first) {
      unit->projection.next = &first->projection;
    }
    first = unit;
    _jqp_pop(yy);
    if (unit == until) {
      break;
    }
  }
  return first;
}

static union jqp_unit* _jqp_pop_projfields_chain(yycontext *yy, union jqp_unit *until) {
  union jqp_unit *field = 0;
  struct jqp_aux *aux = yy->aux;
  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit->type != JQP_STRING_TYPE) {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    unit->string.flavour |= JQP_STR_PROJFIELD;
    if (field) {
      unit->string.subnext = &field->string;
    }
    if (strchr(unit->string.value, '<')) { // JOIN Projection?
      unit->string.flavour |= JQP_STR_PROJOIN;
    }
    field = unit;
    _jqp_pop(yy);
    if (unit == until) {
      break;
    }
  }
  return field;
}

static union jqp_unit* _jqp_pop_ordernodes(yycontext *yy, union jqp_unit *until) {
  union jqp_unit *first = 0;
  struct jqp_aux *aux = yy->aux;
  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit->type != JQP_STRING_TYPE) {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    if (first) {
      unit->string.subnext = &first->string;
    }
    first = unit;
    _jqp_pop(yy);
    if (unit == until) {
      break;
    }
  }
  return until;
}

static union jqp_unit* _jqp_node(yycontext *yy, union jqp_unit *value) {
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_NODE_TYPE;
  unit->node.value = value;
  if (value->type == JQP_EXPR_TYPE) {
    unit->node.ntype = JQP_NODE_EXPR;
  } else if (value->type == JQP_STRING_TYPE) {
    const char *str = value->string.value;
    size_t len = strlen(str);
    if (!strncmp("*", str, len)) {
      unit->node.ntype = JQP_NODE_ANY;
    } else if (!strncmp("**", str, len)) {
      unit->node.ntype = JQP_NODE_ANYS;
    } else {
      unit->node.ntype = JQP_NODE_FIELD;
    }
  } else {
    iwlog_error("Invalid node value type: %d", value->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return unit;
}

static union jqp_unit* _jqp_pop_node_chain(yycontext *yy, union jqp_unit *until) {
  union jqp_unit *filter, *first = 0;
  struct jqp_aux *aux = yy->aux;
  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit->type != JQP_NODE_TYPE) {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    if (first) {
      unit->node.next = &first->node;
    }
    first = unit;
    _jqp_pop(yy);
    if (unit == until) {
      break;
    }
  }
  if (!first) {
    iwlog_error2("Invalid state");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  filter = _jqp_unit(yy);
  filter->type = JQP_FILTER_TYPE;
  filter->filter.node = &first->node;
  if (  aux->stack
     && (aux->stack->type == STACK_UNIT)
     && (aux->stack->unit->type == JQP_STRING_TYPE)
     && (aux->stack->unit->string.flavour & JQP_STR_ANCHOR)) {
    filter->filter.anchor = _jqp_unit_pop(yy)->string.value;
    if (!aux->first_anchor) {
      aux->first_anchor = filter->filter.anchor;
    }
  }
  return filter;
}

static union jqp_unit* _jqp_pop_filter_factor_chain(yycontext *yy, union jqp_unit *until) {
  struct jqp_expr_node *factor = 0;
  struct jqp_aux *aux = yy->aux;
  union jqp_unit *exprnode = _jqp_unit(yy);
  while (aux->stack && aux->stack->type == STACK_UNIT) {
    union jqp_unit *unit = aux->stack->unit;
    if (unit->type == JQP_JOIN_TYPE) {
      factor->join = &unit->join; // -V522
    } else if ((unit->type == JQP_EXPR_NODE_TYPE) || (unit->type == JQP_FILTER_TYPE)) {
      struct jqp_expr_node *node = (struct jqp_expr_node*) unit;
      if (factor) {
        node->next = factor;
      }
      factor = node;
    } else {
      iwlog_error("Unexpected type: %d", unit->type);
      JQRC(yy, JQL_ERROR_QUERY_PARSE);
    }
    _jqp_pop(yy);
    if (unit == until) {
      break;
    }
  }
  exprnode->type = JQP_EXPR_NODE_TYPE;
  exprnode->exprnode.chain = factor;
  return exprnode;
}

static void _jqp_set_filters_expr(yycontext *yy, union jqp_unit *expr) {
  struct jqp_aux *aux = yy->aux;
  if (expr->type != JQP_EXPR_NODE_TYPE) {
    iwlog_error("Unexpected type: %d", expr->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  union jqp_unit *query = _jqp_unit(yy);
  query->type = JQP_QUERY_TYPE;
  query->query.aux = aux;
  aux->expr = &expr->exprnode;
  aux->query = &query->query;
}

static union jqp_unit* _jqp_create_filterexpr_pk(yycontext *yy, union jqp_unit *argument) {
  struct jqp_aux *aux = yy->aux;
  const char *anchor = 0;
  // Looking for optional
  if (  aux->stack
     && (aux->stack->type == STACK_UNIT)
     && (aux->stack->unit->type == JQP_STRING_TYPE)
     && (aux->stack->unit->string.flavour & JQP_STR_ANCHOR)) {
    anchor = _jqp_unit_pop(yy)->string.value;
    if (!aux->first_anchor) {
      aux->first_anchor = anchor;
    }
  }
  union jqp_unit *unit = _jqp_unit(yy);
  unit->type = JQP_EXPR_NODE_TYPE;
  struct jqp_expr_node_pk *exprnode_pk = &unit->exprnode_pk;
  exprnode_pk->flags = JQP_EXPR_NODE_FLAG_PK;
  exprnode_pk->anchor = anchor;
  exprnode_pk->argument = argument;
  return unit;
}

static void _jqp_set_apply(yycontext *yy, union jqp_unit *unit) {
  struct jqp_aux *aux = yy->aux;
  if (!unit || !aux->query) {
    iwlog_error2("Invalid arguments");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (unit->type == JQP_JSON_TYPE) {
    aux->apply = &unit->json.jn;
    aux->apply_placeholder = 0;
  } else if ((unit->type == JQP_STRING_TYPE) && (unit->string.flavour & JQP_STR_PLACEHOLDER)) {
    aux->apply_placeholder = unit->string.value;
    aux->apply = 0;
  } else {
    iwlog_error("Unexpected type: %d", unit->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
}

static void _jqp_set_apply_delete(yycontext *yy) {
  struct jqp_aux *aux = yy->aux;
  aux->qmode |= JQP_QRY_APPLY_DEL;
}

static void _jqp_set_apply_upsert(yycontext *yy, union jqp_unit *unit) {
  struct jqp_aux *aux = yy->aux;
  aux->qmode |= JQP_QRY_APPLY_UPSERT;
  _jqp_set_apply(yy, unit);
}

static void _jqp_add_orderby(yycontext *yy, union jqp_unit *unit) {
  struct jqp_aux *aux = yy->aux;
  if (unit->type != JQP_STRING_TYPE) {
    iwlog_error("Unexpected type for order by: %d", unit->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (!aux->orderby) {
    aux->orderby = &unit->string;
  } else {
    aux->orderby->next = &unit->string;
  }
}

static void _jqp_set_skip(yycontext *yy, union jqp_unit *unit) {
  struct jqp_aux *aux = yy->aux;
  if ((unit->type != JQP_INTEGER_TYPE) && !(  (unit->type == JQP_STRING_TYPE)
                                           && (unit->string.flavour & JQP_STR_PLACEHOLDER))) {
    iwlog_error("Unexpected type for skip: %d", unit->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (aux->skip) {
    JQRC(yy, JQL_ERROR_SKIP_ALREADY_SET);
  }
  aux->skip = unit;
}

static void _jqp_set_limit(yycontext *yy, union jqp_unit *unit) {
  struct jqp_aux *aux = yy->aux;
  if ((unit->type != JQP_INTEGER_TYPE) && !(  (unit->type == JQP_STRING_TYPE)
                                           && (unit->string.flavour & JQP_STR_PLACEHOLDER))) {
    iwlog_error("Unexpected type for limit: %d", unit->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (aux->limit) {
    JQRC(yy, JQL_ERROR_LIMIT_ALREADY_SET);
  }
  aux->limit = unit;
}

static void _jqp_set_aggregate_count(yycontext *yy) {
  struct jqp_aux *aux = yy->aux;
  aux->qmode |= JQP_QRY_COUNT;
  aux->projection = 0; // No projections in aggregate mode
}

static void _jqp_set_noidx(yycontext *yy) {
  struct jqp_aux *aux = yy->aux;
  aux->qmode |= JQP_QRY_NOIDX;
}

static void _jqp_set_inverse(yycontext *yy) {
  struct jqp_aux *aux = yy->aux;
  aux->qmode |= (JQP_QRY_NOIDX | JQP_QRY_INVERSE);
}

static void _jqp_set_projection(yycontext *yy, union jqp_unit *unit) {
  struct jqp_aux *aux = yy->aux;
  if (!unit || !aux->query) {
    iwlog_error2("Invalid arguments");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (unit->type == JQP_PROJECTION_TYPE) {
    struct jqp_projection *proj = &unit->projection;
    for (struct jqp_projection *p = proj; p; p = p->next) {
      if (p->value->flavour & JQP_STR_PROJALIAS) {
        if (p->flags & JQP_PROJECTION_FLAG_EXCLUDE) {
          aux->has_exclude_all_projection = true;
          break;
        } else {
          proj = p->next;
        }
      } else if (!aux->has_keep_projections && (p->flags & JQP_PROJECTION_FLAG_INCLUDE)) {
        aux->has_keep_projections = true;
      }
    }
    aux->projection = proj;
  } else {
    iwlog_error("Unexpected type: %d", unit->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
}

static void _jqp_finish(yycontext *yy) {
  iwrc rc = 0;
  int cnt = 0;

  struct iwxstr *xstr = 0;
  struct jqp_aux *aux = yy->aux;
  struct jqp_string *orderby = aux->orderby;

  for ( ; orderby; ++cnt, orderby = orderby->next) {
    if (cnt >= MAX_ORDER_BY_CLAUSES) {
      rc = JQL_ERROR_ORDERBY_MAX_LIMIT;
      RCGO(rc, finish);
    }
  }
  aux->orderby_num = cnt;
  if (cnt) {
    aux->orderby_ptrs = iwpool_alloc(cnt * sizeof(struct jbl_ptr*), aux->pool);
    if (!aux->orderby_ptrs) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    xstr = iwxstr_new();
    if (!xstr) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      RCGO(rc, finish);
    }
    cnt = 0;
    orderby = aux->orderby;
    for ( ; orderby; orderby = orderby->next) {
      iwxstr_clear(xstr);
      for (struct jqp_string *on = orderby; on; on = on->subnext) {
        rc = iwxstr_cat(xstr, "/", 1);
        RCGO(rc, finish);
        iwxstr_cat(xstr, on->value, strlen(on->value));
      }
      rc = jbl_ptr_alloc_pool(iwxstr_ptr(xstr), &aux->orderby_ptrs[cnt], aux->pool);
      RCGO(rc, finish);
      struct jbl_ptr *ptr = aux->orderby_ptrs[cnt];
      ptr->op = (uint64_t) ((orderby->flavour & JQP_STR_NEGATE) != 0);  // asc/desc
      cnt++;
    }
  }

finish:
  if (xstr) {
    iwxstr_destroy(xstr);
  }
  if (rc) {
    aux->orderby_num = 0;
    JQRC(yy, rc);
  }
}

iwrc jqp_aux_create(struct jqp_aux **auxp, const char *input) {
  iwrc rc = 0;
  *auxp = calloc(1, sizeof(**auxp));
  if (!*auxp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  struct jqp_aux *aux = *auxp;
  aux->xerr = iwxstr_new();
  if (!aux->xerr) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  aux->pool = iwpool_create(4 * 1024);
  if (!aux->pool) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  rc = _jqp_aux_set_input(aux, input);

finish:
  if (rc) {
    jqp_aux_destroy(auxp);
  }
  return rc;
}

void jqp_aux_destroy(struct jqp_aux **auxp) {
  struct jqp_aux *aux = *auxp;
  if (aux) {
    *auxp = 0;
    if (aux->pool) {
      iwpool_destroy(aux->pool);
    }
    if (aux->xerr) {
      iwxstr_destroy(aux->xerr);
    }
    free(aux);
  }
}

IW_INLINE iwrc _iwxstr_cat2(struct iwxstr *xstr, const char *buf) {
  return iwxstr_cat(xstr, buf, strlen(buf));
}

static void yyerror(yycontext *yy) {
  struct jqp_aux *aux = yy->aux;
  struct iwxstr *xerr = aux->xerr;
  if (yy->__pos && yy->__text[0]) {
    _iwxstr_cat2(xerr, "near token: '");
    _iwxstr_cat2(xerr, yy->__text);
    _iwxstr_cat2(xerr, "'\n");
  }
  if (yy->__pos < yy->__limit) {
    char buf[2] = { 0 };
    yy->__buf[yy->__limit] = '\0';
    _iwxstr_cat2(xerr, "\n");
    while (yy->__pos < yy->__limit) {
      buf[0] = yy->__buf[yy->__pos++];
      iwxstr_cat(xerr, buf, 1);
    }
  }
  _iwxstr_cat2(xerr, " <--- \n");
}

iwrc jqp_parse(struct jqp_aux *aux) {
  yycontext yy = { 0 };
  yy.aux = aux;
  if (setjmp(aux->fatal_jmp)) {
    if (aux->rc) {
      iwlog_ecode_error3(aux->rc);
    }
    goto finish;
  }
  if (!yyparse(&yy)) {
    if (!aux->rc) {
      aux->rc = JQL_ERROR_QUERY_PARSE;
    }
    yyerror(&yy);
    if (iwxstr_size(aux->xerr) && !(aux->mode & JQL_SILENT_ON_PARSE_ERROR)) {
      const char *prefix = "Syntax error: ";
      iwxstr_unshift(aux->xerr, prefix, strlen(prefix));
      iwlog_error("%s\n", iwxstr_ptr(aux->xerr));
    }
  }

finish:
  yyrelease(&yy);
  return aux->rc;
}

#define PT(data_, size_, ch_, count_) do {        \
          rc = pt(data_, size_, ch_, count_, op); \
          RCRET(rc);                              \
} while (0)

IW_INLINE iwrc _print_placeholder(const char *value, jbl_json_printer pt, void *op);

static iwrc _jqp_print_projection_nodes(const struct jqp_string *p, jbl_json_printer pt, void *op) {
  iwrc rc = 0;
  for (const struct jqp_string *s = p; s; s = s->next) {
    if (!(s->flavour & JQP_STR_PROJALIAS)) {
      PT(0, 0, '/', 1);
    }
    if (s->flavour & JQP_STR_PROJFIELD) {
      PT(0, 0, '{', 1);
      for (const struct jqp_string *pf = s; pf; pf = pf->subnext) {
        if (pf->flavour & JQP_STR_PLACEHOLDER) {
          RCR(_print_placeholder(pf->value, pt, op));
        } else {
          PT(pf->value, -1, 0, 0);
        }
        if (pf->subnext) {
          PT(0, 0, ',', 1);
        }
      }
      PT(0, 0, '}', 1);
    } else {
      if (s->flavour & JQP_STR_PLACEHOLDER) {
        RCR(_print_placeholder(s->value, pt, op));
      } else {
        PT(s->value, -1, 0, 0);
      }
    }
  }
  return rc;
}

static iwrc _jqp_print_projection(const struct jqp_projection *p, jbl_json_printer pt, void *op) {
  iwrc rc = 0;
  PT(0, 0, '|', 1);
  for (int i = 0; p; p = p->next, ++i) {
    PT(0, 0, ' ', 1);
    if (i > 0) {
      if (p->flags & JQP_PROJECTION_FLAG_EXCLUDE) {
        PT("- ", 2, 0, 0);
      } else {
        PT("+ ", 2, 0, 0);
      }
    } else if (p->flags & JQP_PROJECTION_FLAG_EXCLUDE) {
      PT("all - ", 6, 0, 0);
    }
    rc = _jqp_print_projection_nodes(p->value, pt, op);
    RCRET(rc);
  }
  return rc;
}

static iwrc _jqp_print_apply(const struct jqp_query *q, jbl_json_printer pt, void *op) {
  iwrc rc = 0;
  if (q->aux->qmode & JQP_QRY_APPLY_DEL) {
    PT("| del ", 6, 0, 0);
  } else {
    if (q->aux->qmode & JQP_QRY_APPLY_UPSERT) {
      PT("| upsert ", 9, 0, 0);
    } else {
      PT("| apply ", 8, 0, 0);
    }
    if (q->aux->apply_placeholder) {
      PT(q->aux->apply_placeholder, -1, 0, 0);
    } else if (q->aux->apply) {
      rc = jbn_as_json(q->aux->apply, pt, op, 0);
      RCRET(rc);
    }
  }
  return rc;
}

static iwrc _jqp_print_join(jqp_op_t jqop, bool negate, jbl_json_printer pt, void *op) {
  iwrc rc = 0;
  PT(0, 0, ' ', 1);
  if (jqop == JQP_OP_EQ) {
    if (negate) {
      PT(0, 0, '!', 1);
    }
    PT("= ", 2, 0, 0);
    return rc;
  }
  if (jqop == JQP_JOIN_AND) {
    PT("and ", 4, 0, 0);
    if (negate) {
      PT("not ", 4, 0, 0);
    }
    return rc;
  } else if (jqop == JQP_JOIN_OR) {
    PT("or ", 3, 0, 0);
    if (negate) {
      PT("not ", 4, 0, 0);
    }
    return rc;
  }
  if (negate) {
    PT("not ", 4, 0, 0);
  }
  switch (jqop) {
    case JQP_OP_GT:
      PT(0, 0, '>', 1);
      break;
    case JQP_OP_LT:
      PT(0, 0, '<', 1);
      break;
    case JQP_OP_GTE:
      PT(">=", 2, 0, 0);
      break;
    case JQP_OP_LTE:
      PT("<=", 2, 0, 0);
      break;
    case JQP_OP_IN:
      PT("in", 2, 0, 0);
      break;
    case JQP_OP_RE:
      PT("re", 2, 0, 0);
      break;
    case JQP_OP_PREFIX:
      PT(0, 0, '~', 1);
      break;
    default:
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      rc = IW_ERROR_ASSERTION;
      break;
  }
  PT(0, 0, ' ', 1);
  return rc;
}

IW_INLINE iwrc _print_placeholder(const char *value, jbl_json_printer pt, void *op) {
  iwrc rc;
  PT(0, 0, ':', 1);
  if (value[0] == '?') {
    PT(0, 0, '?', 1);
  } else {
    PT(value, -1, 0, 0);
  }
  return rc;
}

iwrc jqp_print_filter_node_expr(const struct jqp_expr *e, jbl_json_printer pt, void *op) {
  iwrc rc = 0;
  if (e->left->type == JQP_EXPR_TYPE) {
    PT(0, 0, '[', 1);
    jqp_print_filter_node_expr(&e->left->expr, pt, op);
    PT(0, 0, ']', 1);
  } else if (e->left->type == JQP_STRING_TYPE) {
    if (e->left->string.flavour & JQP_STR_QUOTED) {
      PT(0, 0, '"', 1);
    }
    PT(e->left->string.value, -1, 0, 0);
    if (e->left->string.flavour & JQP_STR_QUOTED) {
      PT(0, 0, '"', 1);
    }
  } else {
    iwlog_ecode_error3(IW_ERROR_ASSERTION);
    return IW_ERROR_ASSERTION;
  }
  rc = _jqp_print_join(e->op->value, e->op->negate, pt, op);
  RCRET(rc);
  if (e->right->type == JQP_STRING_TYPE) {
    if (e->right->string.flavour & JQP_STR_PLACEHOLDER) {
      rc = _print_placeholder(e->right->string.value, pt, op);
      RCRET(rc);
    } else {
      PT(e->right->string.value, -1, 0, 0);
    }
  } else if (e->right->type == JQP_JSON_TYPE) {
    rc = jbn_as_json(&e->right->json.jn, pt, op, 0);
    RCRET(rc);
  } else {
    iwlog_ecode_error3(IW_ERROR_ASSERTION);
    return IW_ERROR_ASSERTION;
  }
  return rc;
}

static iwrc _jqp_print_filter_node(const struct jqp_node *n, jbl_json_printer pt, void *op) {
  iwrc rc = 0;
  union jqp_unit *u = n->value;
  PT(0, 0, '/', 1);
  if (u->type == JQP_STRING_TYPE) {
    PT(u->string.value, -1, 0, 0);
    return rc;
  } else if (u->type == JQP_EXPR_TYPE) {
    PT(0, 0, '[', 1);
    for (struct jqp_expr *e = &u->expr; e; e = e->next) {
      if (e->join) {
        rc = _jqp_print_join(e->join->value, e->join->negate, pt, op);
        RCRET(rc);
      }
      rc = jqp_print_filter_node_expr(e, pt, op);
      RCRET(rc);
    }
    PT(0, 0, ']', 1);
  } else {
    iwlog_ecode_error3(IW_ERROR_ASSERTION);
    return IW_ERROR_ASSERTION;
  }
  return rc;
}

static iwrc _jqp_print_filter(
  const struct jqp_query  *q,
  const struct jqp_filter *f,
  jbl_json_printer         pt,
  void                    *op) {
  iwrc rc = 0;
  if (f->anchor) {
    PT(0, 0, '@', 1);
    PT(f->anchor, -1, 0, 0);
  }
  for (struct jqp_node *n = f->node; n; n = n->next) {
    rc = _jqp_print_filter_node(n, pt, op);
    RCRET(rc);
  }
  return rc;
}

static iwrc _jqp_print_expression_node(
  const struct jqp_query     *q,
  const struct jqp_expr_node *en,
  jbl_json_printer            pt,
  void                       *op) {
  iwrc rc = 0;
  bool inbraces = (en != q->aux->expr && en->type == JQP_EXPR_NODE_TYPE);
  if (inbraces) {
    PT(0, 0, '(', 1);
  }

  // Primary key expression
  if (en->flags & JQP_EXPR_NODE_FLAG_PK) {
    struct jqp_expr_node_pk *pk = (void*) en;
    if (pk->anchor) {
      PT(0, 0, '@', 1);
      PT(pk->anchor, -1, 0, 0);
    }
    PT("/=", 2, 0, 0);
    if (!pk->argument) {
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      return IW_ERROR_ASSERTION;
    }
    if (pk->argument->type == JQP_STRING_TYPE) {
      if (pk->argument->string.flavour & JQP_STR_PLACEHOLDER) {
        rc = _print_placeholder(pk->argument->string.value, pt, op);
        RCRET(rc);
      } else {
        PT(pk->argument->string.value, -1, 0, 0);
      }
    } else if (pk->argument->type == JQP_JSON_TYPE) {
      rc = jbn_as_json(&pk->argument->json.jn, pt, op, 0);
      RCRET(rc);
    } else {
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      return IW_ERROR_ASSERTION;
    }
    return rc;
  }

  for (en = en->chain; en; en = en->next) {
    if (en->join) {
      rc = _jqp_print_join(en->join->value, en->join->negate, pt, op);
      RCRET(rc);
    }
    if (en->type == JQP_EXPR_NODE_TYPE) {
      rc = _jqp_print_expression_node(q, en, pt, op);
      RCRET(rc);
    } else if (en->type == JQP_FILTER_TYPE) {
      rc = _jqp_print_filter(q, (const struct jqp_filter*) en, pt, op);  // -V1027
    } else {
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      return IW_ERROR_ASSERTION;
    }
  }
  if (inbraces) {
    PT(0, 0, ')', 1);
  }
  return rc;
}

static iwrc _jqp_print_opts(const struct jqp_query *q, jbl_json_printer pt, void *op) {
  iwrc rc = 0;
  int c = 0;
  struct jqp_aux *aux = q->aux;
  PT(0, 0, '|', 1);
  struct jqp_string *ob = aux->orderby;
  while (ob) {
    if (c++ > 0) {
      PT("\n ", 2, 0, 0);
    }
    if (ob->flavour & JQP_STR_NEGATE) {
      PT(" desc ", 6, 0, 0);
    } else {
      PT(" asc ", 5, 0, 0);
    }
    if (ob->flavour & JQP_STR_PLACEHOLDER) {
      rc = _print_placeholder(ob->value, pt, op);
      RCRET(rc);
    } else {
      // print orderby subnext chain
      struct jqp_string *n = ob;
      do {
        PT(0, 0, '/', 1);
        PT(n->value, -1, 0, 0);
      } while ((n = n->subnext));
    }
    ob = ob->next;
  }
  if (aux->skip || aux->limit) {
    if (c > 0) {
      PT("\n ", 2, 0, 0);
    }
  }
  if (aux->skip) {
    PT(" skip ", 6, 0, 0);
    if (aux->skip->type == JQP_STRING_TYPE) {
      if (aux->skip->string.flavour & JQP_STR_PLACEHOLDER) {
        rc = _print_placeholder(aux->skip->string.value, pt, op);
        RCRET(rc);
      } else {
        PT(aux->skip->string.value, -1, 0, 0);
      }
    } else if (aux->skip->type == JQP_INTEGER_TYPE) {
      char nbuf[IWNUMBUF_SIZE];
      iwitoa(aux->skip->intval.value, nbuf, IWNUMBUF_SIZE);
      PT(nbuf, -1, 0, 0);
    }
  }
  if (aux->limit) {
    PT(" limit ", 7, 0, 0);
    if (aux->limit->type == JQP_STRING_TYPE) {
      if (aux->limit->string.flavour & JQP_STR_PLACEHOLDER) {
        rc = _print_placeholder(aux->limit->string.value, pt, op);
        RCRET(rc);
      } else {
        PT(aux->limit->string.value, -1, 0, 0);
      }
    } else if (aux->limit->type == JQP_INTEGER_TYPE) {
      char nbuf[IWNUMBUF_SIZE];
      iwitoa(aux->limit->intval.value, nbuf, IWNUMBUF_SIZE);
      PT(nbuf, -1, 0, 0);
    }
  }
  return rc;
}

iwrc jqp_print_query(const struct jqp_query *q, jbl_json_printer pt, void *op) {
  if (!q || !pt) {
    return IW_ERROR_INVALID_ARGS;
  }
  struct jqp_aux *aux = q->aux;
  iwrc rc = _jqp_print_expression_node(q, aux->expr, pt, op);
  RCRET(rc);
  if (aux->apply_placeholder || aux->apply) {
    PT(0, 0, '\n', 1);
    rc = _jqp_print_apply(q, pt, op);
    RCRET(rc);
  }
  if (aux->projection) {
    PT(0, 0, '\n', 1);
    rc = _jqp_print_projection(aux->projection, pt, op);
    RCRET(rc);
  }
  if (aux->skip || aux->limit || aux->orderby) {
    PT(0, 0, '\n', 1);
    rc = _jqp_print_opts(q, pt, op);
  }

  return rc;
}

#undef PT
