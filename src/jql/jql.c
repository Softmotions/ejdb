#include "jqp.h"
#include "jbl_internal.h"

typedef struct JQ_FILTER {
  bool expect_next;       /**< Expecting next node to be matched */
  int pos;                /**< Current node position */
  int nnum;               /**< Number of filter nodes */
  JQP_NODE **nodes;       /**< Array query nodes */
  JQP_FILTER *qpf;        /**< Parsed query filter */
  struct JQ_FILTER *next; /**< Next filter in chain */
} JQ_FILTER;

/** Query object */
struct _JQL {
  JQP_QUERY *qp;
  JQP_AUX *aux;
  JQ_FILTER *qf;
};

iwrc jql_create(JQL *qptr, const char *query) {
  if (!qptr || !query) {
    return IW_ERROR_INVALID_ARGS;
  }
  *qptr = 0;

  JQL q;
  JQP_AUX *aux;
  int i;
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
  q->qp = aux->query;
  q->qf = 0;

  JQ_FILTER *last = 0;
  for (JQP_FILTER *qpf = q->qp->filter; qpf; qpf = qpf->next) {
    JQ_FILTER *qf = iwpool_calloc(sizeof(*qf), aux->pool);
    if (!qf) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    qf->qpf = qpf;
    if (q->qf) {
      last->next = qf;
    } else {
      q->qf = qf;
    }
    last = qf;
    for (JQP_NODE *n = qpf->node; n; n = n->next) ++qf->nnum;
    qf->nodes = iwpool_calloc(qf->nnum * sizeof(qf->nodes[0]), aux->pool);
    if (!qf) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    i = 0;
    for (JQP_NODE *n = qpf->node; n; n = n->next) {
      qf->nodes[i++] = n;
    }
  }

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
// typedef jbl_visitor_cmd_t (*JBL_VISITOR)(int lvl, binn *bv, char *key, int idx, JBL_VCTX *vctx, iwrc *rc);
// iwrc _jbl_visit(binn_iter *iter, int lvl, JBL_VCTX *vctx, JBL_VISITOR visitor);

static jbl_visitor_cmd_t _match_visitor(int lvl, binn *bv, char *key, int idx, JBL_VCTX *vctx, iwrc *rcp) {



  return 0;
}

iwrc jql_matched(JQL q, const JBL jbl, bool *out) {
  iwrc rc = 0;


  return rc;
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
