#include "jqp.h"
#include "jbl_internal.h"

typedef struct _NODE {
  int start;              /**< Start matching index */
  int end;                /**< End matching index */
  JQP_NODE *qpn;
} _NODE;

typedef struct _FILTER {
  bool matched;
  int last_lvl;           /**< Last matched level */
  int nnum;               /**< Number of filter nodes */
  _NODE *nodes;           /**< Array query nodes */
  JQP_FILTER *qpf;          /**< Parsed query filter */
  struct _FILTER *next;   /**< Next filter in chain */
} _FILTER;

/** Query object */
struct _JQL {
  bool matched;
  bool dirty;
  JQP_QUERY *qp;
  JQP_AUX *aux;
  _FILTER *qf;
};

/** Node matching context */
struct _NCTX {
  int lvl;
  int idx;
  binn *bv;
  char *key;
  _FILTER *qf;
  _NODE *n;
  _NODE *nn;
  JQL q;
};

bool _jql_filters_need_go_deeper(const JQL q, int lvl) {
  for (const _FILTER *qf = q->qf; qf; qf = qf->next) {
    if (!qf->matched && qf->last_lvl == lvl) {
      return true;
    }
  }
  return false;
}

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
  
  _FILTER *last = 0;
  for (JQP_FILTER *f = q->qp->filter; f; f = f->next) {
    _FILTER *qf = iwpool_calloc(sizeof(*qf), aux->pool);
    if (!qf) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    qf->last_lvl = -1;
    qf->qpf = f;
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
      _NODE qn = { .start = -1, .end = -1, .qpn = n };
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

static bool _match_node_anys(const struct _NCTX *ctx, iwrc *rcp) {
  _NODE *node = ctx->n;
  *rcp = IW_ERROR_NOT_IMPLEMENTED;
  return false;
}

static bool _match_node_expr(const struct _NCTX *ctx, iwrc *rcp) {
  _NODE *node = ctx->n;
  node->start = ctx->lvl;
  node->end = node->start;
  *rcp = IW_ERROR_NOT_IMPLEMENTED;
  return false;
}

static bool _match_node_field(const struct _NCTX *ctx, iwrc *rcp) {
  _NODE *node = ctx->n;
  JQP_NODE *qpn = node->qpn;
  node->start = ctx->lvl;
  node->end = node->start;
  if (qpn->value->type != JQP_STRING_TYPE) {
    *rcp = IW_ERROR_ASSERTION;
    return false;
  }
  const char *val = qpn->value->string.value;
  bool ret = strcmp(val, ctx->key) == 0;      
  return ret;
}

static bool _match_node(const struct _NCTX *ctx, iwrc *rcp) {
  _NODE *node = ctx->n;
  switch (node->qpn->ntype) {
    case JQP_NODE_ANY:
      node->start = ctx->lvl;
      node->end = node->start;
      return true;
    case JQP_NODE_FIELD:
      return _match_node_field(ctx, rcp);
    case JQP_NODE_EXPR:      
      return _match_node_expr(ctx, rcp);
    case JQP_NODE_ANYS:
      return _match_node_anys(ctx, rcp);
  }
  return false;
}

static iwrc _match_filter(int lvl, binn *bv, char *key, int idx, JQL q, _FILTER *qf) {
  const int nnum = qf->nnum;
  _NODE *nodes = qf->nodes;
  
  if (qf->last_lvl + 1 < lvl || qf->matched) {
    return 0;
  }
  if (lvl <= qf->last_lvl) {
    qf->last_lvl = lvl - 1;
    for (int i = 0; i < nnum; ++i) {
      _NODE *n = nodes + i;
      if (lvl > n->start && lvl < n->end) {
        n->end = lvl;
      } else if (n->start >= lvl) {
        n->start = -1;
        n->end = -1;
      }
    }
  }
  for (int i = 0; i < nnum; ++i) {
    _NODE *n = nodes + i, *nn = 0;
    if (n->start < 0 || (lvl >= n->start && lvl <= n->end)) {
      if (n->start < 0) {
        n->start = lvl;
        n->end = lvl;
      }
      if (i < nnum - 1) {
        nn = nodes + i + 1;
      }
      iwrc rc = 0;
      struct _NCTX nctx = {
        .lvl = lvl,
        .bv = bv,
        .key = key,
        .idx = idx,
        .q = q,
        .qf = qf,
        .n = n,
        .nn = nn
      };
      bool matched = _match_node(&nctx, &rc);
      RCRET(rc);
      if (matched) {
        if (i == nnum - 1) {
          qf->matched = true;
          q->dirty = true;
        }
        qf->last_lvl = lvl;
      }
      return 0;
    }
  }
  return 0;
}

static jbl_visitor_cmd_t _match_visitor(int lvl, binn *bv, char *key, int idx, JBL_VCTX *vctx, iwrc *rcp) {
  char nbuf[JBNUMBUF_SIZE];
  char *nkey = key;
  JQL q = vctx->op;
  if (!nkey) {
    iwitoa(idx, nbuf, sizeof(nbuf));
    nkey = nbuf;
  }
  bool last = false;
  for (_FILTER *qf = q->qf; qf; qf = qf->next) {
    if (!qf->matched) {
      *rcp = _match_filter(lvl, bv, key, idx, q, qf);
      if (*rcp) return JBL_VCMD_TERMINATE;
    }
    const JQP_JOIN *j = qf->qpf->join;
    if (!j) {
      last = qf->matched;
    } else if (j->value == JQP_JOIN_AND) { // AND
      last = last && qf->matched;
    } else if (last || qf->matched) {      // OR
      last = true;
      break;
    }
  }
  if ((q->matched = last)) {
    return JBL_VCMD_TERMINATE;
  }
  if (q->dirty) {
    q->dirty = false;
    if (!_jql_filters_need_go_deeper(q, lvl)) {
      return JBL_VCMD_SKIP_NESTED;
    }
  }
  return 0;
}

iwrc jql_matched(JQL q, const JBL jbl, bool *out) {
  JBL_VCTX vctx = {
    .bn = &jbl->bn,
    .op = &q
  };
  *out = false;
  iwrc rc = _jbl_visit(0, 0, &vctx, _match_visitor);
  RCRET(rc);
  *out = q->matched;  
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
