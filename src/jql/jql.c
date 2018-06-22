#include "jqp.h"
#include "jbl_internal.h"

typedef struct JQ_NODE {
  int start;  /**< start matching index */
  int end;    /**< end matching index */
  JQP_NODE *n;
} JQ_NODE;

typedef struct JQ_FILTER {
  int last;               /**< Last matched node index */
  int nnum;               /**< Number of filter nodes */
  JQ_NODE *nodes;         /**< Array query nodes */
  JQP_FILTER *f;          /**< Parsed query filter */
  struct JQ_FILTER *next; /**< Next filter in chain */
} JQ_FILTER;


typedef struct MCTX {
  bool matched_prefix;
  bool matched_all;  
} MCTX;

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
  for (JQP_FILTER *f = q->qp->filter; f; f = f->next) {
    JQ_FILTER *qf = iwpool_calloc(sizeof(*qf), aux->pool);
    if (!qf) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    qf->last = -1;
    qf->f = f;
    if (q->qf) {
      last->next = qf;
    } else {
      q->qf = qf;
    }
    last = qf;
    for (JQP_NODE *n = f->node; n; n = n->next) ++qf->nnum;
    qf->nodes = iwpool_calloc(qf->nnum * sizeof(qf->nodes[0]), aux->pool);
    if (!qf) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    i = 0;
    for (JQP_NODE *n = f->node; n; n = n->next) {
      JQ_NODE qn = { .start = -1, .end = -1, .n = n };
      memcpy(&qf->nodes[i++], &qn, sizeof(qn));            
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

void jql_reset(JQL *qptr) {
  // TODO:
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

static void _match_filter(int lvl, binn *bv, char *key, int idx, JBL_VCTX *vctx, iwrc *rcp) {
  MCTX *mctx = (MCTX*) vctx->op;
}


static jbl_visitor_cmd_t _match_visitor(int lvl, binn *bv, char *key, int idx, JBL_VCTX *vctx, iwrc *rcp) {
  char nbuf[JBNUMBUF_SIZE];
  char *nkey = key;
  if (!nkey) {
    iwitoa(idx, nbuf, sizeof(nbuf));
    nkey = nbuf;
  }
  

  return 0;
}

iwrc jql_matched(JQL q, const JBL jbl, bool *out) {
  MCTX mctx = {0};
  JBL_VCTX vctx = {
    .bn = &jbl->bn,
    .op = &mctx
  };
  iwrc rc = _jbl_visit(0, 0, &vctx, _match_visitor);
  RCRET(rc);

  
  return rc;
}

static const char *_ecodefn(locale_t locale, uint32_t ecode) {
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
  return iwlog_register_ecodefn(_ecodefn);
}
