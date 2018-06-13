#include "jqp.h"
#include <stdlib.h>
#include <string.h>

static iwrc _jqp_aux_set_input(JQPAUX *aux, const char *input) {
  size_t len = strlen(input) + 1;
  char *buf = iwpool_alloc(len, aux->unitpool);
  if (!buf) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  memcpy(buf, input, len);
  aux->buf = buf;
  return 0;
}

iwrc jqp_aux_create(JQPAUX **auxp, const char *input) {
  iwrc rc = 0;
  *auxp = calloc(1, sizeof(**auxp));
  if (!*auxp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JQPAUX *aux = *auxp;
  aux->line = 1;
  aux->col = 1;
  aux->unitpool = iwpool_create(0);
  if (!aux->unitpool) {
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
    if (aux->unitpool) {
      iwpool_destroy(aux->unitpool);
    }
    free(aux);
    *auxp = 0;
  }
}
