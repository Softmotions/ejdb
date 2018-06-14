#include "jqp.h"
#include <stdlib.h>
#include <string.h>

#define JQRC(yy_, rc_) do {           \
    iwrc __rc = (rc_);                  \
    if (__rc) _jqp_fatal(yy_, __rc, 0); \
  } while(0)

#define JQRC2(yy_, rc_, msg_) do {        \
    iwrc __rc = (rc_);                      \
    if (__rc) _jqp_fatal(yy_, __rc, msg_);  \
  } while(0)

static void _jqp_fatal(yycontext *yy, iwrc rc, const char *msg) {
  JQPAUX *aux = yy->aux;
  aux->rc = rc;
  aux->error = msg;
  if (aux->fatal_jmp) {
    longjmp(*aux->fatal_jmp, 1);
  } else {
    if (msg) {
      iwlog_ecode_error2(rc, msg);
    } else {
      iwlog_ecode_error3(rc);
    }
    exit(1);
  }
}

static void *_jqp_malloc(struct _yycontext *yy, size_t size) {
  void *ret = malloc(size);
  if (!ret) {
    JQPAUX *aux = yy->aux;
    aux->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    if (aux->fatal_jmp) {
      longjmp(*aux->fatal_jmp, 1);
    } else {
      iwlog_ecode_error3(aux->rc);
      exit(1);
    }
  }
  return ret;
}

static void *_jqp_realloc(struct _yycontext *yy, void *ptr, size_t size) {
  void *ret = realloc(ptr, size);
  if (!ret) {
    JQPAUX *aux = yy->aux;
    aux->rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    if (aux->fatal_jmp) {
      longjmp(*aux->fatal_jmp, 1);
    } else {
      iwlog_ecode_error3(aux->rc);
      exit(1);
    }
  }
  return ret;
}

static iwrc _jqp_aux_set_input(JQPAUX *aux, const char *input) {
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

IW_INLINE char *_jqp_strdup(struct _yycontext *yy, const char *text) {
  iwrc rc = 0;
  char *ret = iwpool_strdup(yy->aux->pool, text, &rc);
  JQRC(yy, rc);
  return ret;
}

IW_INLINE JQPUNIT *_jqp_unit(yycontext *yy) {
  JQPUNIT *ret = iwpool_calloc(sizeof(JQPUNIT), yy->aux->pool);
  if (!ret) JQRC(yy, iwrc_set_errno(IW_ERROR_ALLOC, errno));
  return ret;
}

static JQPSTACK *_jqp_push(yycontext *yy) {
  JQPAUX *aux = yy->aux;
  JQPSTACK *stack = iwpool_alloc(sizeof(*aux->stack), aux->pool);
  if (!stack) JQRC(yy, iwrc_set_errno(IW_ERROR_ALLOC, errno));
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

static JQPSTACK *_jqp_pop(yycontext *yy) {
  JQPAUX *aux = yy->aux;
  JQPSTACK *stack = aux->stack;
  if (!stack) {
    iwlog_error2("Unbalanced JQPSTACK");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  aux->stack = stack->prev;
  if (aux->stack) {
    aux->stack->next = 0;
  }
  stack->prev = 0;
  stack->next = 0;
  return stack;
}

static void _jqp_unit_push(yycontext *yy, JQPUNIT *unit) {
  JQPSTACK *stack = _jqp_push(yy);
  stack->type = STACK_UNIT;
  stack->unit = unit;
}

static JQPUNIT *_jqp_unit_pop(yycontext *yy) {
  JQPSTACK *stack = _jqp_pop(yy);
  if (stack->type != STACK_UNIT) {
    iwlog_error("Incorrect value type on stack: %d", stack->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return stack->unit;
}

static void _jqp_string_push(yycontext *yy, char *str) {
  JQPSTACK *stack = _jqp_push(yy);
  stack->type = STACK_STRING;
  stack->str = str;
}

static char *_jqp_string_pop(yycontext *yy) {
  JQPSTACK *stack = _jqp_pop(yy);
  if (stack->type != STACK_STRING) {
    iwlog_error("Incorrect value type on stack: %d", stack->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return stack->str;
}

static void _jqp_int_push(yycontext *yy, int64_t i64) {
  JQPSTACK *stack = _jqp_push(yy);
  stack->type = STACK_INT;
  stack->i64 = i64;
}

static int64_t _jqp_int_pop(yycontext *yy) {
  JQPSTACK *stack = _jqp_pop(yy);
  if (stack->type != STACK_INT) {
    iwlog_error("Incorrect value type on stack: %d", stack->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return stack->i64;
}

static void _jqp_float_push(yycontext *yy, double f64) {
  JQPSTACK *stack = _jqp_push(yy);
  stack->type = STACK_FLOAT;
  stack->f64 = f64;
}

static double _jqp_float_pop(yycontext *yy) {
  JQPSTACK *stack = _jqp_pop(yy);
  if (stack->type != STACK_FLOAT) {
    iwlog_error("Incorrect value type on stack: %d", stack->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return stack->f64;
}

static JQPUNIT *_jqp_string(yycontext *yy, jqp_strflags_t flags, const char *text) {
  JQPUNIT *unit = _jqp_unit(yy);
  unit->type = JQP_STRING_TYPE;
  unit->string.flags = flags;
  unit->string.value = _jqp_strdup(yy, text);
  return unit;
}

static void _jqp_op_negate(yycontext *yy) {
  yy->aux->negate = true;
}

static JQPUNIT *_jqp_unit_op(yycontext *yy, const char *text) {
  fprintf(stderr, "op=%s\n", text);
  JQPAUX *aux = yy->aux;
  JQPUNIT *unit = _jqp_unit(yy);
  unit->type = JQP_OP_TYPE;  
  unit->op.negate = aux->negate;
  aux->negate = false;
  if (!strcmp(text, "=") || !strcmp(text, "eq")) {
    unit->op.op = JQP_OP_EQ;
  } else if (!strcmp(text, ">") || !strcmp(text, "gt")) {
    unit->op.op = JQP_OP_GT;
  } else if (!strcmp(text, ">=") || !strcmp(text, "gte")) {
    unit->op.op = JQP_OP_GTE;
  } else if (!strcmp(text, "<") || !strcmp(text, "lt")) {
    unit->op.op = JQP_OP_LT;
  } else if (!strcmp(text, "<=") || !strcmp(text, "lte")) {
    unit->op.op = JQP_OP_LTE;
  } else if (!strcmp(text, "in")) {
    unit->op.op = JQP_OP_IN;
  } else if (!strcmp(text, "re")) {
    unit->op.op = JQP_OP_RE;
  } else if (!strcmp(text, "like")) {
    unit->op.op = JQP_OP_RE;
  } else {
    iwlog_error("Invalid JQP_OP_TYPE: %s", text);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  return unit;
}

static JQPUNIT *_jqp_unit_join(yycontext *yy, const char *text) {
  fprintf(stderr, "join=%s\n", text);
  JQPAUX *aux = yy->aux;
  JQPUNIT *unit = _jqp_unit(yy);
  unit->type = JQP_JOIN_TYPE;  
  unit->join.negate = aux->negate;
  aux->negate = false;
  if (!strcmp(text, "and")) {
    unit->join.join = JQP_JOIN_AND;
  } else if (!strcmp(text, "or")) {
    unit->join.join = JQP_JOIN_OR;
  }
  return unit;
}

static JQPUNIT *_jqp_expr(yycontext *yy, JQPUNIT *left, JQPUNIT *op, JQPUNIT *right) {
  if (!left || !op || !right) {
    iwlog_error2("Invalid _jqp_expr args");
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  if (op->type != JQP_OP_TYPE && op->type != JQP_JOIN_TYPE) {
    iwlog_error("Invalid _jqp_expr op type: %s", op->type);
    JQRC(yy, JQL_ERROR_QUERY_PARSE);
  }
  JQPAUX *aux = yy->aux;
  JQPUNIT *unit = _jqp_unit(yy);
  unit->type = JQP_EXPR_TYPE;
  unit->expr.left = left;
  unit->expr.op = op;
  unit->expr.right = right;
  return unit;
}

//--------------- Public

iwrc jqp_aux_create(JQPAUX **auxp, const char *input) {
  iwrc rc = 0;
  *auxp = calloc(1, sizeof(**auxp));
  if (!*auxp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JQPAUX *aux = *auxp;
  aux->line = 1;
  aux->col = 1;
  aux->pool = iwpool_create(0);
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

void jqp_aux_destroy(JQPAUX **auxp) {
  JQPAUX *aux = *auxp;
  if (aux) {
    if (aux->pool) {
      iwpool_destroy(aux->pool);
    }
    free(aux);
    *auxp = 0;
  }
}

iwrc jqp_parse(JQPAUX *aux) {
  yycontext yy = {0};
  yy.aux = aux;
  if (!yyparse(&yy)) {
    if (!aux->rc) {
      aux->rc = JQL_ERROR_QUERY_PARSE;
    }
  }
  yyrelease(&yy);
  return aux->rc;
}
