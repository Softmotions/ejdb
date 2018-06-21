#include "jqp.h"

/** Query object */
struct _JQL {
  JQP_QUERY *q;
  JQPAUX *aux;
};

iwrc jql_create(JQL *qptr, const char *query) {
  if (!qptr || !query) {
    return IW_ERROR_INVALID_ARGS;
  }
  *qptr = 0;

  JQL q;
  JQPAUX *aux;
  iwrc rc = jqp_aux_create(&aux, query);
  RCRET(rc);

  q = iwpool_alloc(sizeof(*q), aux->pool);
  if (!q) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  rc = jqp_parse(aux);
  RCGO(rc, finish);

  q->aux = aux;
  q->q = aux->query;

finish:
  if (rc) {
    jqp_aux_destroy(&aux);
  } else {
    *qptr = q;
  }
  return rc;
}

void jql_destroy(JQL *qptr) {
  if (qptr) {
    JQL q = *qptr;
    jqp_aux_destroy(&q->aux);
    *qptr = 0;
  }
}

static const char *_jql_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JQL_ERROR_START && ecode < _JQL_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case JQL_ERROR_QUERY_PARSE:
      return "Query parsing error (JQL_ERROR_QUERY_PARSE)";
    default:
      break;
  }
  return 0;
}

iwrc jql_init() {
  static int _jql_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jql_initialized, 0, 1)) {
    return 0;
  }
  return iwlog_register_ecodefn(_jql_ecodefn);
}
