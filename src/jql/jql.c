#include "jqp.h"
#include "lwre.h"
#include "jbl_internal.h"

/** Query matching context */
typedef struct _MCTX {
  int lvl;
  binn *bv;
  char *key;
  struct _JQL *q;
  JQP_QUERY *qp;
  JBL_VCTX *vctx;
} _MCTX;

/** Expression node matching context */
typedef struct _MENCTX {
  bool matched;
} _MENCTX;

/** Filter node matching context */
typedef struct _MFNCTX {
  int start;              /**< Start matching index */
  int end;                /**< End matching index */
  JQP_NODE *qpn;
} _MFNCTX;

/** Filter matching context */
typedef struct _MFCTX {
  bool matched;
  int last_lvl;           /**< Last matched level */
  int nnum;               /**< Number of filter nodes */
  _MFNCTX *nodes;         /**< Filter nodes mctx */
  JQP_FILTER *qpf;        /**< Parsed query filter */
} _MFCTX;

/** Query object */
struct _JQL {
  bool dirty;
  bool matched;
  JQP_QUERY *qp;
};

/** Placeholder value type */
typedef enum {
  JQVAL_NULL,
  JQVAL_JBLNODE,
  JQVAL_I64,
  JQVAL_F64,
  JQVAL_STR,
  JQVAL_BOOL,
  JQVAL_RE
} jqval_type_t;

/** Placeholder value */
typedef struct {
  jqval_type_t type;
  union {
    JBL_NODE vjblnode;
    int64_t vint64;
    double vf64;
    const char *vstr;
    bool vbool;
    struct re *vre;
  };
} _JQVAL;

static void _jql_jqval_destroy(_JQVAL *qv) {
  if (qv->type == JQVAL_RE) {
    re_free(qv->vre);
  }
  free(qv);
}

static iwrc _jql_set_placeholder(JQL q, const char *placeholder, int index, _JQVAL *val) {
  JQP_AUX *aux = q->qp->aux;
  if (!placeholder) { // Index
    char nbuf[JBNUMBUF_SIZE];
    int len = iwitoa(index, nbuf, JBNUMBUF_SIZE);
    nbuf[len] = '\0';
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->next) {
      if (pv->value[0] == '?' && !strcmp(pv->value + 1, nbuf)) {
        if (pv->opaque) _jql_jqval_destroy(pv->opaque);
        pv->opaque = val;
        return 0;
      }
    }
  } else {
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->next) {
      if (pv->value[0] == ':' && !strcmp(pv->value + 1, placeholder)) {
        if (pv->opaque) _jql_jqval_destroy(pv->opaque);
        pv->opaque = val;
        return 0;
      }
    }
  }
  return JQL_ERROR_INVALID_PLACEHOLDER;
}

iwrc jql_set_json(JQL q, const char *placeholder, int index, JBL_NODE val) {
  _JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_JBLNODE;
  qv->vjblnode = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_i64(JQL q, const char *placeholder, int index, int64_t val) {
  _JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_I64;
  qv->vint64 = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_f64(JQL q, const char *placeholder, int index, double val) {
  _JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_F64;
  qv->vf64 = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_str(JQL q, const char *placeholder, int index, const char *val) {
  _JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_STR;
  qv->vstr = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_bool(JQL q, const char *placeholder, int index, bool val) {
  _JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_BOOL;
  qv->vbool = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_regexp(JQL q, const char *placeholder, int index, const char *expr) {
  struct re *rx = re_new(expr);
  if (!rx) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  _JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    iwrc rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    re_free(rx);
    return rc;
  }
  qv->type = JQVAL_RE;
  qv->vre = rx;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_null(JQL q, const char *placeholder, int index) {
  JQP_AUX *aux = q->qp->aux;
  _JQVAL *qv = iwpool_alloc(sizeof(_JQVAL), aux->pool);
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_NULL;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

static bool _jql_need_deeper_match(JQP_EXPR_NODE *en, int lvl) {
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      if (_jql_need_deeper_match(en, lvl)) {
        return true;
      }
    } else if (en->type == JQP_FILTER_TYPE) {
      _MFCTX *fctx = ((JQP_FILTER *) en)->opaque;
      if (!fctx->matched && fctx->last_lvl == lvl) {
        return true;
      }
    }
  }
  return false;
}

static void _jql_reset_expression_node(JQP_EXPR_NODE *en, JQP_AUX *aux) {
  _MENCTX *ectx = en->opaque;
  ectx->matched = false;
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      _jql_reset_expression_node(en, aux);
    } else if (en->type == JQP_FILTER_TYPE) {
      _MFCTX *fctx = ((JQP_FILTER *) en)->opaque;
      fctx->matched = false;
      fctx->last_lvl = -1;
      for (int i = 0; i < fctx->nnum; ++i) {
        fctx->nodes[i].end = -1;
        fctx->nodes[i].start = -1;
      }
    }
  }
}

static iwrc _jql_init_expression_node(JQP_EXPR_NODE *en, JQP_AUX *aux) {
  en->opaque = iwpool_calloc(sizeof(_MENCTX), aux->pool);
  if (!en->opaque) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      iwrc rc = _jql_init_expression_node(en, aux);
      RCRET(rc);
    } else if (en->type == JQP_FILTER_TYPE) {
      _MFCTX *fctx = iwpool_calloc(sizeof(*fctx), aux->pool);
      JQP_FILTER *f = (JQP_FILTER *) en;
      if (!fctx) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
      f->opaque = fctx;
      fctx->last_lvl = -1;
      fctx->qpf = f;
      for (JQP_NODE *n = f->node; n; n = n->next) ++fctx->nnum;
      fctx->nodes = iwpool_calloc(fctx->nnum * sizeof(fctx->nodes[0]), aux->pool);
      if (!fctx->nodes) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
      int i = 0;
      for (JQP_NODE *n = f->node; n; n = n->next) {
        _MFNCTX nctx = { .start = -1, .end = -1, .qpn = n };
        memcpy(&fctx->nodes[i++], &nctx, sizeof(nctx));
      }
    }
  }
  return 0;
}

iwrc jql_create(JQL *qptr, const char *query) {
  if (!qptr || !query) {
    return IW_ERROR_INVALID_ARGS;
  }
  *qptr = 0;
  
  JQL q;
  JQP_AUX *aux;
  iwrc rc = jqp_aux_create(&aux, query);
  RCRET(rc);
  
  q = iwpool_alloc(sizeof(*q), aux->pool);
  if (!q) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  rc = jqp_parse(aux);
  RCGO(rc, finish);
  
  q->qp = aux->query;
  q->dirty = false;
  q->matched = false;
  
  rc = _jql_init_expression_node(q->qp->expr, aux);
  
finish:
  if (rc) {
    jqp_aux_destroy(&aux);
  } else {
    *qptr = q;
  }
  return rc;
}

void jql_reset(JQL q, bool reset_placeholders) {
  q->matched = false;
  q->dirty = false;
  _jql_reset_expression_node(q->qp->expr, q->qp->aux);
  if (reset_placeholders) {
    JQP_AUX *aux = q->qp->aux;
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->next) { // Cleanup placeholders
      _jql_jqval_destroy(pv->opaque);
    }
  }
}

void jql_destroy(JQL *qptr) {
  if (qptr) {
    JQL q = *qptr;
    JQP_AUX *aux = q->qp->aux;
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->next) { // Cleanup placeholders
      _jql_jqval_destroy(pv->opaque);
    }
    jqp_aux_destroy(&aux);
    *qptr = 0;
  }
}

static bool _match_expr_pair(_MCTX *mctx,
                             JQPUNIT *left, JQP_OP *op, JQPUNIT *right,
                             iwrc *rcp) {
  // TODO:
  return false;
}

static bool _match_node_expr_impl(_MCTX *mctx, _MFNCTX *n, JQP_EXPR *expr, iwrc *rcp) {
  JQPUNIT *left = expr->left;
  JQP_OP *op = expr->op;
  JQPUNIT *right = expr->right;
  if (left->type == JQP_STRING_TYPE) {
    if (!(left->string.flavour & JQP_STR_STAR) && strcmp(mctx->key, left->string.value)) {
      return false;
    }
  } else if (left->type == JQP_EXPR_TYPE) {
    if (left->expr.left->type != JQP_STRING_TYPE || !(left->expr.left->string.flavour & JQP_STR_STAR)) {
      *rcp = IW_ERROR_ASSERTION;
      return false;
    }
    JQPUNIT keyleft = {.type = JQP_STRING_TYPE, .string.value = mctx->key};
    if (!_match_expr_pair(mctx, &keyleft, left->expr.op, left->expr.right, rcp)) {
      return false;
    }    
    if (*rcp) return false; 
  }  
  return _match_expr_pair(mctx, left, op, right, rcp);
}

static bool _match_node_expr(_MCTX *mctx, _MFNCTX *n, _MFNCTX *nn, iwrc *rcp) {
  n->start = mctx->lvl;
  n->end = n->start;
  JQP_NODE *qpn = n->qpn;
  JQPUNIT *unit = qpn->value;
  if (unit->type != JQP_EXPR_TYPE) {
    *rcp = IW_ERROR_ASSERTION;
    return false;
  }
  bool prev = false;
  for (JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) {
    bool matched = _match_node_expr_impl(mctx, n, expr, rcp);
    if (*rcp) return false;
    const JQP_JOIN *join = expr->join;
    if (!join) {
      prev = matched;
    } else {
      if (join->negate) {
        matched = !matched;
      }
      if (join->value == JQP_JOIN_AND) { // AND
        prev = prev && matched;
      } else if (prev || matched) {      // OR
        prev = true;
        break;
      }
    }
  }
  return prev;
}

static bool _match_node_anys(_MCTX *mctx, _MFNCTX *n, _MFNCTX *nn, iwrc *rcp) {
  // TODO:
  return false;
}

static bool _match_node_field(_MCTX *mctx, _MFNCTX *n, _MFNCTX *nn, iwrc *rcp) {
  JQP_NODE *qpn = n->qpn;
  n->start = mctx->lvl;
  n->end = n->start;
  if (qpn->value->type != JQP_STRING_TYPE) {
    *rcp = IW_ERROR_ASSERTION;
    return false;
  }
  const char *val = qpn->value->string.value;
  bool ret = strcmp(val, mctx->key) == 0;
  return ret;
}

static bool _match_node(_MCTX *mctx, _MFNCTX *n, _MFNCTX *nn, iwrc *rcp) {
  switch (n->qpn->ntype) {
    case JQP_NODE_FIELD:
      return _match_node_field(mctx, n, nn, rcp);
    case JQP_NODE_EXPR:
      return _match_node_expr(mctx, n, nn, rcp);
    case JQP_NODE_ANY:
      n->start = mctx->lvl;
      n->end = n->start;
      return true;
    case JQP_NODE_ANYS:
      return _match_node_anys(mctx, n, nn, rcp);
  }
  return false;
}

static bool _match_filter(JQP_FILTER *f, _MCTX *mctx, iwrc *rcp) {
  _MFCTX *fctx = f->opaque;
  if (fctx->matched) {
    return true;
  }
  const int nnum = fctx->nnum;
  const int lvl = mctx->lvl;
  _MFNCTX *nodes = fctx->nodes;
  if (fctx->last_lvl + 1 < lvl) {
    return false;
  }
  if (lvl <= fctx->last_lvl) {
    fctx->last_lvl = lvl - 1;
    for (int i = 0; i < nnum; ++i) {
      _MFNCTX *n = nodes + i;
      if (lvl > n->start && lvl < n->end) {
        n->end = lvl;
      } else if (n->start >= lvl) {
        n->start = -1;
        n->end = -1;
      }
    }
  }
  for (int i = 0; i < nnum; ++i) {
    _MFNCTX *n = nodes + i;
    _MFNCTX *nn = 0;
    if (n->start < 0 || (lvl >= n->start && lvl <= n->end)) {
      if (i < nnum - 1) {
        nn = nodes + i + 1;
      }
      bool matched = _match_node(mctx, n, nn, rcp);
      if (*rcp) return false;
      if (matched) {
        if (i == nnum - 1) {
          fctx->matched = true;
          mctx->q->dirty = true;
        }
        fctx->last_lvl = lvl;
      }
      return fctx->matched;
    }
  }
  return fctx->matched;
}

static bool _match_expression_node(JQP_EXPR_NODE *en, _MCTX *mctx, iwrc *rcp) {
  _MENCTX *enctx = en->opaque;
  if (enctx->matched) {
    return true;
  }
  bool prev = false;
  for (en = en->chain; en; en = en->next) {
    bool matched = false;
    if (en->type == JQP_EXPR_NODE_TYPE) {
      matched = _match_expression_node(en, mctx, rcp);
    } else if (en->type == JQP_FILTER_TYPE) {
      matched = _match_filter((JQP_FILTER *) en, mctx, rcp);
    }
    if (*rcp) return JBL_VCMD_TERMINATE;
    const JQP_JOIN *join = en->join;
    if (!join) {
      prev = matched;
    } else {
      if (join->negate) {
        matched = !matched;
      }
      if (join->value == JQP_JOIN_AND) { // AND
        prev = prev && matched;
      } else if (prev || matched) {      // OR
        prev = true;
        break;
      }
    }
  }
  return prev;
}

static jbl_visitor_cmd_t _match_visitor(int lvl, binn *bv, char *key, int idx, JBL_VCTX *vctx, iwrc *rcp) {
  char nbuf[JBNUMBUF_SIZE];
  char *nkey = key;
  JQL q = vctx->op;
  if (!nkey) {
    int len = iwitoa(idx, nbuf, sizeof(nbuf));
    nbuf[len] = '\0';
    nkey = nbuf;
  }
  _MCTX mctx = {
    .lvl = lvl,
    .bv = bv,
    .key = nkey,
    .vctx = vctx,
    .q = q,
    .qp = q->qp
  };
  q->matched = _match_expression_node(mctx.qp->expr, &mctx, rcp);
  if (*rcp || q->matched) {
    return JBL_VCMD_TERMINATE;
  }
  if (q->dirty) {
    q->dirty = false;
    if (!_jql_need_deeper_match(mctx.qp->expr, lvl)) {
      return JBL_VCMD_SKIP_NESTED;
    }
  }
  return 0;
}

iwrc jql_matched(JQL q, const JBL jbl, bool *out) {
  JBL_VCTX vctx = {
    .bn = &jbl->bn,
    .op = q
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
    case JQL_ERROR_INVALID_PLACEHOLDER:
      return "Invalid placeholder position (JQL_ERROR_INVALID_PLACEHOLDER)";
    case JQL_ERROR_UNSET_PLACEHOLDER:
      return "Found unset placeholder (JQL_ERROR_UNSET_PLACEHOLDER)";
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
