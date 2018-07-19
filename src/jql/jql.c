#include "jqp.h"
#include "lwre.h"
#include "jbl_internal.h"

typedef struct _NODE {
  int start;              /**< Start matching index */
  int end;                /**< End matching index */
  JQP_NODE *qpn;
} _NODE;

//typedef struct _FILTER {
//  bool matched;
//  int last_lvl;           /**< Last matched level */
//  int nnum;               /**< Number of filter nodes */
//  _NODE *nodes;           /**< Array query nodes */
//  JQP_FILTER *qpf;        /**< Parsed query filter */
//  struct _FILTER *next;   /**< Next filter in chain */
//} _FILTER;

/** Query object */
struct _JQL {
  bool matched;
  bool dirty;
  JQP_QUERY *qp;
  JQP_AUX *aux;
//  _FILTER *qf;
};

/** Node matching context */
struct _NCTX {
  int lvl;
  binn *bv;
  char *key;
//  _FILTER *qf;
  _NODE *n;
  _NODE *nn;
  JQL q;
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
  if (!placeholder) { // Index
    char nbuf[JBNUMBUF_SIZE];
    int len = iwitoa(index, nbuf, JBNUMBUF_SIZE);
    nbuf[len] = '\0';
    for (JQP_STRING *pv = q->aux->start_placeholder; pv; pv = pv->next) {
      if (pv->value[0] == '?' && !strcmp(pv->value + 1, nbuf)) {
        if (pv->opaque) _jql_jqval_destroy(pv->opaque);
        pv->opaque = val;
        return 0;
      }
    }
  } else {
    for (JQP_STRING *pv = q->aux->start_placeholder; pv; pv = pv->next) {
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
  _JQVAL *qv = iwpool_alloc(sizeof(_JQVAL), q->aux->pool);
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_NULL;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

bool _jql_filters_need_go_deeper(const JQL q, int lvl) {
//  for (const _FILTER *qf = q->qf; qf; qf = qf->next) {
//    if (!qf->matched && qf->last_lvl == lvl) {
//      return true;
//    }
//  }
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
//  q->qf = 0;
  q->dirty = false;
  

//typedef struct _FILTER {
//  bool matched;
//  int last_lvl;           /**< Last matched level */
//  int nnum;               /**< Number of filter nodes */
//  _NODE *nodes;           /**< Array query nodes */
//  JQP_FILTER *qpf;        /**< Parsed query filter */
//  struct _FILTER *next;   /**< Next filter in chain */
//} _FILTER;  
  

      
  
//  _FILTER *last = 0;    
//  for (JQP_FILTER *f = q->qp->filter; f; f = f->next) {
//    _FILTER *qf = iwpool_calloc(sizeof(*qf), aux->pool);
//    if (!qf) {
//      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
//      goto finish;
//    }
//    qf->last_lvl = -1;
//    qf->qpf = f;
//    if (q->qf) {
//      last->next = qf;
//    } else {
//      q->qf = qf;
//    }
//    last = qf;
//    for (JQP_NODE *n = f->node; n; n = n->next) ++qf->nnum;
//    qf->nodes = iwpool_calloc(qf->nnum * sizeof(qf->nodes[0]), aux->pool);
//    if (!qf) {
//      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
//      goto finish;
//    }
//    i = 0;
//    for (JQP_NODE *n = f->node; n; n = n->next) {
//      _NODE qn = { .start = -1, .end = -1, .qpn = n };
//      memcpy(&qf->nodes[i++], &qn, sizeof(qn));
//    }
//  }
  
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
    for (JQP_STRING *pv = q->aux->start_placeholder; pv; pv = pv->next) { // Cleanup placeholders
      _jql_jqval_destroy(pv->opaque);
    }
    jqp_aux_destroy(&q->aux);
    *qptr = 0;
  }
}

static bool _match_expr(JQPUNIT *left, JQP_OP *op, JQPUNIT *right, iwrc *rcp) {
  // TODO: 
  return false;
}

static bool _match_node_anys(const struct _NCTX *ctx, iwrc *rcp) {
  _NODE *node = ctx->n;
  *rcp = IW_ERROR_NOT_IMPLEMENTED;
  return false;
}

static bool _match_node_expr_impl(const struct _NCTX *ctx, JQP_EXPR *expr, iwrc *rcp) {
  JQPUNIT *left = expr->left;
  JQP_OP *op = expr->op;
  JQPUNIT *right = expr->right;
  if (left->type == JQP_STRING_TYPE) {
    if (!(left->string.flavour & JQP_STR_STAR) && strcmp(ctx->key, left->string.value)) {
      return false;
    }
  } else if (left->type == JQP_EXPR_TYPE) {
    if (left->expr.left->type != JQP_STRING_TYPE || !(left->expr.left->string.flavour & JQP_STR_STAR)) {
        return IW_ERROR_ASSERTION; // Not a star (*)
    }
    JQPUNIT keyleft = {0};
    keyleft.type = JQP_STRING_TYPE;
    keyleft.string.value = ctx->key;
    if (!_match_expr(&keyleft, left->expr.op, left->expr.right, rcp)) {
      return false;
    }
  }
  return _match_expr(left, op, right, rcp);
}

static bool _match_node_expr(const struct _NCTX *ctx, iwrc *rcp) {
  _NODE *node = ctx->n;
  node->start = ctx->lvl;
  node->end = node->start;
  JQP_NODE *qpn = node->qpn;
  JQPUNIT *unit = qpn->value;
  if (unit->type != JQP_EXPR_TYPE) {
    *rcp = IW_ERROR_ASSERTION;
    return false;
  }
  bool prev = false;
  bool matched = false;
  for (JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) {
    matched = _match_node_expr_impl(ctx, expr, rcp);
    if (*rcp) return false;
    const JQP_JOIN *j = expr->join;
    if (!j) {
      prev = matched;
    } else if (j->value == JQP_JOIN_AND) { // AND
      prev = prev && matched;
    } else if (prev || matched) {     // OR
      prev = true;
      break;
    }
  }
  return prev;
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

static iwrc _match_filter(int lvl, binn *bv, char *key, JQL q) {
//  const int nnum = qf->nnum;
//  _NODE *nodes = qf->nodes;
//  
//  if (qf->last_lvl + 1 < lvl || qf->matched) {
//    return 0;
//  }
//  if (lvl <= qf->last_lvl) {
//    qf->last_lvl = lvl - 1;
//    for (int i = 0; i < nnum; ++i) {
//      _NODE *n = nodes + i;
//      if (lvl > n->start && lvl < n->end) {
//        n->end = lvl;
//      } else if (n->start >= lvl) {
//        n->start = -1;
//        n->end = -1;
//      }
//    }
//  }
//  for (int i = 0; i < nnum; ++i) {
//    _NODE *n = nodes + i, *nn = 0;
//    if (n->start < 0 || (lvl >= n->start && lvl <= n->end)) {
//      if (i < nnum - 1) {
//        nn = nodes + i + 1;
//      }
//      iwrc rc = 0;
//      struct _NCTX nctx = {
//        .lvl = lvl,
//        .bv = bv,
//        .key = key,
//        .q = q,
//        .qf = qf,
//        .n = n,
//        .nn = nn
//      };
//      bool matched = _match_node(&nctx, &rc);
//      RCRET(rc);
//      if (matched) {
//        if (i == nnum - 1) {
//          qf->matched = true;
//          q->dirty = true;
//        }
//        qf->last_lvl = lvl;
//      }
//      return 0;
//    }
//  }
  return 0;
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
  bool prev = false;
//  for (_FILTER *qf = q->qf; qf; qf = qf->next) {
//    if (!qf->matched) {
//      *rcp = _match_filter(lvl, bv, nkey, q, qf);
//      if (*rcp) return JBL_VCMD_TERMINATE;
//    }
//    const JQP_JOIN *j = qf->qpf->join;
//    if (!j) {
//      prev = qf->matched;
//    } else if (j->value == JQP_JOIN_AND) { // AND
//      prev = prev && qf->matched;
//    } else if (prev || qf->matched) {      // OR
//      prev = true;
//      break;
//    }
//  }
  if ((q->matched = prev)) {
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
