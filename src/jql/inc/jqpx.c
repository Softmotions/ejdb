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

static JQPUNIT *_jqp_string(yycontext *yy, jqp_strflags_t flags, const char *text) {
  JQPUNIT *ret = _jqp_unit(yy);
  ret->string.type = JQP_STRING_TYPE;
  ret->string.flags = flags;
  ret->string.value = _jqp_strdup(yy, text);
  return ret;
}

static void _jqp_unit_negate(yycontext *yy) {
  yy->aux->negate = true;
}

static JQPUNIT *_jqp_unit_op(yycontext *yy, const char *text) {
  fprintf(stderr, "op=%s\n", text);
  JQPAUX *aux = yy->aux;
  JQPUNIT *unit = _jqp_unit(yy);
  unit->op.type = JQP_OP_TYPE;
  unit->op.negate = aux->negate;
  aux->negate = false;
  unit->op.value = _jqp_strdup(yy, text);
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
