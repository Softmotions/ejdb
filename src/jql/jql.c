#include "jqp.h"
#include "lwre.h"
#include "jbl_internal.h"

/** Query matching context */
typedef struct MCTX {
  int lvl;
  binn *bv;
  char *key;
  struct _JQL *q;
  JQP_QUERY *qp;
  JBL_VCTX *vctx;
} MCTX;

/** Expression node matching context */
typedef struct MENCTX {
  bool matched;
} MENCTX;


/** Filter matching context */
typedef struct MFCTX {
  bool matched;
  int last_lvl;           /**< Last matched level */
  JQP_NODE *nodes;
  JQP_NODE *last_node;
  JQP_FILTER *qpf;
} MFCTX;


/** Query object */
struct _JQL {
  bool dirty;
  bool matched;
  JQP_QUERY *qp;
};

/** Placeholder value type */
typedef enum {
  JQVAL_NULL,
  JQVAL_I64,
  JQVAL_F64,
  JQVAL_STR,
  JQVAL_BOOL,
  JQVAL_RE,
  JQVAL_JBLNODE, // Do not reorder JQVAL_JBLNODE,JQVAL_BINN must be last
  JQVAL_BINN
} jqval_type_t;

/** Placeholder value */
typedef struct {
  jqval_type_t type;
  union {
    JBL_NODE vnode;
    binn *vbinn;
    int64_t vi64;
    double vf64;
    const char *vstr;
    bool vbool;
    struct re *vre;
  };
} JQVAL;

static JQP_NODE *_match_node(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp);

static void _jql_jqval_destroy(JQVAL *qv) {
  if (qv->type == JQVAL_RE) {
    re_free(qv->vre);
  }
  free(qv);
}

static JQVAL *_jql_find_placeholder(JQL q, const char *name) {
  JQP_AUX *aux = q->qp->aux;
  for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->next) {
    if (!strcmp(pv->value, name)) {
      return pv->opaque;
    }
  }
  return 0;
}

static iwrc _jql_set_placeholder(JQL q, const char *placeholder, int index, JQVAL *val) {
  JQP_AUX *aux = q->qp->aux;
  if (!placeholder) { // Index
    char nbuf[JBNUMBUF_SIZE];
    iwitoa(index, nbuf, JBNUMBUF_SIZE);
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
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_JBLNODE;
  qv->vnode = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_i64(JQL q, const char *placeholder, int index, int64_t val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_I64;
  qv->vi64 = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_f64(JQL q, const char *placeholder, int index, double val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_F64;
  qv->vf64 = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_str(JQL q, const char *placeholder, int index, const char *val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_STR;
  qv->vstr = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_bool(JQL q, const char *placeholder, int index, bool val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->type = JQVAL_BOOL;
  qv->vbool = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_regexp(JQL q, const char *placeholder, int index, const char *expr) {
  struct re *rx = re_new(expr);
  if (!rx) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  JQVAL *qv = malloc(sizeof(*qv));
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
  JQVAL *qv = iwpool_alloc(sizeof(JQVAL), aux->pool);
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
      MFCTX *fctx = ((JQP_FILTER *) en)->opaque;
      if (!fctx->matched && fctx->last_lvl == lvl) {
        return true;
      }
    }
  }
  return false;
}

static void _jql_reset_expression_node(JQP_EXPR_NODE *en, JQP_AUX *aux) {
  MENCTX *ectx = en->opaque;
  ectx->matched = false;
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      _jql_reset_expression_node(en, aux);
    } else if (en->type == JQP_FILTER_TYPE) {
      MFCTX *fctx = ((JQP_FILTER *) en)->opaque;
      fctx->matched = false;
      fctx->last_lvl = -1;
      for (JQP_NODE *n = fctx->nodes; n; n = n->next) {
        n->start = -1;
        n->end = -1;
      }
    }
  }
}

static iwrc _jql_init_expression_node(JQP_EXPR_NODE *en, JQP_AUX *aux) {
  en->opaque = iwpool_calloc(sizeof(MENCTX), aux->pool);
  if (!en->opaque) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      iwrc rc = _jql_init_expression_node(en, aux);
      RCRET(rc);
    } else if (en->type == JQP_FILTER_TYPE) {
      MFCTX *fctx = iwpool_calloc(sizeof(*fctx), aux->pool);
      JQP_FILTER *f = (JQP_FILTER *) en;
      if (!fctx) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
      f->opaque = fctx;
      fctx->last_lvl = -1;
      fctx->qpf = f;
      fctx->nodes = f->node;
      for (JQP_NODE *n = f->node; n; n = n->next) {
        fctx->last_node = n;
        n->start = -1;
        n->end = -1;
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
    for (JQP_OP *op = aux->start_op; op; op = op->next) {
      if (op->opaque) {
        if (op->op == JQP_OP_RE) {
          re_free(op->opaque);
        }
      }
    }
    jqp_aux_destroy(&aux);
    *qptr = 0;
  }
}

IW_INLINE jbl_type_t _binn_to_jqval(binn *vbinn, JQVAL *qval) {
  switch (vbinn->type) {
    case BINN_OBJECT:
    case BINN_MAP:
    case BINN_LIST:
      qval->type = JQVAL_BINN;
      qval->vbinn = vbinn;
      return qval->type;
    case BINN_NULL:
      qval->type = JQVAL_NULL;
      return qval->type;
    case BINN_STRING:
      qval->type = JQVAL_STR;
      qval->vstr = vbinn->ptr;
      return qval->type;
    case BINN_BOOL:
    case BINN_TRUE:
    case BINN_FALSE:
      qval->type = JQVAL_BOOL;
      qval->vbool = vbinn->vbool;
      return qval->type;
    case BINN_UINT8:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vuint8;
      return qval->type;
    case BINN_UINT16:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vuint16;
      return qval->type;
    case BINN_UINT32:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vuint32;
      return qval->type;
    case BINN_UINT64:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vuint64;
      return qval->type;
    case BINN_INT8:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vint8;
      return qval->type;
    case BINN_INT16:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vint16;
      return qval->type;
    case BINN_INT32:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vint32;
      return qval->type;
    case BINN_INT64:
      qval->type = JQVAL_I64;
      qval->vi64 = vbinn->vint64;
      return qval->type;
    case BINN_FLOAT32:
      qval->type = JQVAL_F64;
      qval->vf64 = vbinn->vdouble;
      return qval->type;
    case BINN_FLOAT64:
      qval->type = JQVAL_F64;
      qval->vf64 = vbinn->vfloat;
      return qval->type;
  }
  return JBV_NONE;
}

IW_INLINE void _node_to_jqval(JBL_NODE jn, JQVAL *qv) {
  switch (jn->type) {
    case JBV_STR:
      qv->type = JQVAL_STR;
      qv->vstr = jn->vptr;
      break;
    case JBV_I64:
      qv->type = JQVAL_I64;
      qv->vi64 = jn->vi64;
      break;
    case JBV_BOOL:
      qv->type = JQVAL_BOOL;
      qv->vbool = jn->vbool;
      break;
    case JBV_F64:
      qv->type = JQVAL_F64;
      qv->vf64 = jn->vf64;
      break;
    case JBV_NULL:
    case JBV_NONE:
      qv->type = JQVAL_NULL;
      break;
    case JBV_OBJECT:
    case JBV_ARRAY:
      qv->type = JQVAL_JBLNODE;
      qv->vnode = jn;
      break;
  }
}

/**
 * Allowed on left:   JQVAL_STR|JQVAL_BINN
 * Allowed on right:  JQVAL_NULL|JQVAL_JBLNODE|JQVAL_I64|JQVAL_F64|JQVAL_STR|JQVAL_BOOL
 */
static int _cmp_jqval_pair(MCTX *mctx, JQVAL *left, JQVAL *right, iwrc *rcp) {
  JQVAL  sleft, sright;   // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  
  if (lv->type == JQVAL_BINN) {
    _binn_to_jqval(lv->vbinn, &sleft);
    lv = &sleft;
  }
  if (rv->type == JQVAL_JBLNODE) {
    _node_to_jqval(rv->vnode, &sright);
    rv = &sright;
  }
  
  switch (lv->type) {
    case JQVAL_STR:
      switch (rv->type) {
        case JQVAL_STR: {
          int l1 = strlen(lv->vstr);
          int l2 = strlen(rv->vstr);
          if (l1 - l2) {
            return l1 - l2;
          }
          return strncmp(lv->vstr, rv->vstr, l1);
        }
        case JQVAL_BOOL:
          return !strcmp(lv->vstr, "true") - rv->vbool;
        case JQVAL_I64: {
          char nbuf[JBNUMBUF_SIZE];
          iwitoa(rv->vi64, nbuf, JBNUMBUF_SIZE);
          return strcmp(lv->vstr, nbuf);
        }
        case JQVAL_F64: {
          char nbuf[IWFTOA_BUFSIZE];
          iwftoa(rv->vf64, nbuf);
          return strcmp(lv->vstr, nbuf);
        }
        case JQVAL_NULL:
          return (!lv->vstr || lv->vstr[0] == '\0') ? 0 : 1;
        default:
          break;
      }
      break;
    case JQVAL_I64:
      switch (rv->type) {
        case JQVAL_I64:
          return lv->vi64 - rv->vi64;
        case JQVAL_F64:
          return (double) lv->vi64 > rv->vf64 ? 1 : (double) lv->vi64 < rv->vf64 ? -1 : 0;
        case JQVAL_STR:
          return lv->vi64 - iwatoi(rv->vstr);
        case JQVAL_NULL:
          return 1;
        case JQVAL_BOOL:
          return lv->vi64 - rv->vbool;
        default:
          break;
      }
      break;
    case JQVAL_F64:
      switch (rv->type) {
        case JQVAL_F64:
          return lv->vf64 > rv->vf64 ? 1 : lv->vf64 < rv->vf64 ? -1 : 0;
        case JQVAL_I64:
          return lv->vf64 > (double) rv->vi64 ? 1 : lv->vf64 < (double) rv->vf64 ? -1 : 0;
        case JQVAL_STR: {
          double rval = iwatof(rv->vstr);
          return lv->vf64 > rval ? 1 : lv->vf64 < rval ? -1 : 0;
        }
        case JQVAL_NULL:
          return 1;
        case JQVAL_BOOL:
          return lv->vf64 > (double) rv->vbool ? 1 : lv->vf64 < (double) rv->vbool ? -1 : 0;
        default:
          break;
      }
      break;
    case JQVAL_BOOL:
      switch (rv->type) {
        case JQVAL_BOOL:
          return lv->vbool - rv->vbool;
        case JQVAL_I64:
          return lv->vbool - (rv->vi64 != 0L);
        case JQVAL_F64:
          return lv->vbool - (rv->vf64 != 0.0);
        case JQVAL_STR:
          return lv->vbool - !strcmp(rv->vstr, "true");
        case JQVAL_NULL:
          return lv->vbool;
        default:
          break;
      }
      break;
    case JQVAL_NULL:
      switch (rv->type) {
        case JQVAL_NULL:
          return 0;
        case JQVAL_STR:
          return (!rv->vstr || rv->vstr[0] == '\0') ? 0 : -1;
        default:
          return -1;
      }
      break;
    case JQVAL_BINN: {
      if (rv->type != JQVAL_JBLNODE
          || (rv->vnode->type == JBV_ARRAY && lv->vbinn->type != BINN_LIST)
          || (rv->vnode->type == JBV_OBJECT && (lv->vbinn->type != BINN_OBJECT && lv->vbinn->type != BINN_MAP))) {
        // Incompatible types
        *rcp = _JQL_ERROR_UNMATCHED;
        return 0;
      }
      JBL_NODE lnode;
      IWPOOL *pool = iwpool_create(rv->vbinn->size * 2);
      if (!pool) {
        *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        return 0;
      }
      *rcp = _jbl_node_from_binn2(lv->vbinn, &lnode, pool);
      if (*rcp) {
        iwpool_destroy(pool);
        return 0;
      }
      int cmp = jbl_compare_nodes(lnode, rv->vnode, rcp);
      iwpool_destroy(pool);
      return cmp;
    }
    default:
      break;
  }
  *rcp = _JQL_ERROR_UNMATCHED;
  return 0;
}

static bool _match_regexp(MCTX *mctx,
                          JQVAL *left, JQP_OP *jqop, JQVAL *right,
                          char *(*expr_transform)(MCTX *, const char *, iwrc *),
                          iwrc *rcp) {
  struct re *rx;
  char nbuf[JBNUMBUF_SIZE];
  static_assert(JBNUMBUF_SIZE >= IWFTOA_BUFSIZE, "JBNUMBUF_SIZE >= IWFTOA_BUFSIZE");
  JQVAL sleft, sright; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  JQP_AUX *aux = mctx->qp->aux;
  char *input = 0;
  int rci, match_end = 0;
  const char *expr = 0;
  bool matched = false,
       match_start = false;
       
  if (lv->type == JQVAL_JBLNODE) {
    _node_to_jqval(lv->vnode, &sleft);
    lv = &sleft;
  } else if (lv->type == JQVAL_BINN) {
    _binn_to_jqval(lv->vbinn, &sleft);
    lv = &sleft;
  }
  if (lv->type >= JQVAL_JBLNODE) {
    *rcp = _JQL_ERROR_UNMATCHED;
    return false;
  }
  
  if (jqop->opaque) {
    rx = jqop->opaque;
  } else if (right->type == JQVAL_RE) {
    rx = right->vre;
  } else {
    if (rv->type == JQVAL_JBLNODE) {
      _node_to_jqval(rv->vnode, &sright);
      rv = &sright;
    }
    switch (rv->type) {
      case JQVAL_STR:
        expr = rv->vstr;
        break;
      case JQVAL_I64: {
        iwitoa(rv->vi64, nbuf, JBNUMBUF_SIZE);
        expr = iwpool_strdup(aux->pool, nbuf, rcp);
        if (*rcp) return false;
        break;
      }
      case JQVAL_F64: {
        iwftoa(rv->vf64, nbuf);
        expr = iwpool_strdup(aux->pool, nbuf, rcp);
        if (*rcp) return false;
        break;
      }
      case JQVAL_BOOL:
        expr = rv->vbool ? "true" : "false";
        break;
      default:
        *rcp = _JQL_ERROR_UNMATCHED;
        return false;
    }
    
    if (expr_transform) {
      expr = expr_transform(mctx, expr, rcp);
      if (*rcp) return false;
    }
    
    assert(expr);
    if (expr[0] == '^') {
      expr += 1;
      match_start = true;
    }
    rci = strlen(expr);
    if (rci && expr[rci - 1] == '$') {
      char *aexpr = iwpool_alloc(rci, aux->pool);
      if (!aexpr) {
        *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        return false;
      }
      match_end = rci - 1;
      memcpy(aexpr, expr, match_end);
      aexpr[rci - 1] = '\0';
      expr = aexpr;
    }
    rx = re_new(expr);
    if (!rx) {
      *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      return false;
    }
    jqop->opaque = rx;
  }
  assert(rx);
  
  switch (lv->type) {
    case JQVAL_STR:
      input = (char *) lv->vstr; // FIXME: const discarded
      break;
    case JQVAL_I64:
      iwitoa(lv->vi64, nbuf, JBNUMBUF_SIZE);
      input = nbuf;
      break;
    case JQVAL_F64:
      iwftoa(lv->vf64, nbuf);
      input = nbuf;
      break;
    case JQVAL_BOOL:
      input = lv->vbool ? "true" : "false";
      break;
    default:
      *rcp = _JQL_ERROR_UNMATCHED;
      return false;
  }
  
  assert(input);
  rci = re_match(rx, input);
  switch (rci) {
    case RE_ERROR_NOMATCH:
      return false;
    case RE_ERROR_NOMEM:
      *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      return false;
    case RE_ERROR_CHARSET:
      *rcp = JQL_ERROR_REGEXP_CHARSET;
      return false;
    case RE_ERROR_SUBEXP:
      *rcp = JQL_ERROR_REGEXP_SUBEXP;
      return false;
    case RE_ERROR_SUBMATCH:
      *rcp = JQL_ERROR_REGEXP_SUBMATCH;
      return false;
    case RE_ERROR_ENGINE:
      *rcp = JQL_ERROR_REGEXP_ENGINE;
      iwlog_ecode_error3(JQL_ERROR_REGEXP_ENGINE);
      return false;
  }
  if (rci > 0) {
    if (match_start && rx->position - rci != input) {
      return false;
    }
    if (match_end && rx->position != input + match_end) {
      return false;
    }
    return true;
  }
  return false;
}

static bool _match_in(MCTX *mctx,
                      JQVAL *left, JQP_OP *jqop, JQVAL *right,
                      iwrc *rcp) {
                      
  JQVAL sleft; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  if (rv->type != JQVAL_JBLNODE && rv->vnode->type != JBV_ARRAY) {
    *rcp = _JQL_ERROR_UNMATCHED;
    return false;
  }
  if (lv->type == JQVAL_JBLNODE) {
    _node_to_jqval(lv->vnode, &sleft);
    lv = &sleft;
  } else if (lv->type == JQVAL_BINN) {
    _binn_to_jqval(lv->vbinn, &sleft);
    lv = &sleft;
  }
  for (JBL_NODE n = rv->vnode->child; n; n = n->next) {
    JQVAL qv = {
      .type = JQVAL_JBLNODE,
      .vnode = n
    };
    if (!_cmp_jqval_pair(mctx, lv, &qv, rcp)) {
      if (*rcp) return false;
      return true;
    }
    if (*rcp) return false;
  }
  return false;
}

static bool _match_ni(MCTX *mctx,
                      JQVAL *left, JQP_OP *jqop, JQVAL *right,
                      iwrc *rcp) {
                      
  JQVAL sleft; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  binn *bn, bv;
  binn_iter iter;
  if (rv->type != JQVAL_BINN || rv->vbinn->type != BINN_LIST) {
    *rcp = _JQL_ERROR_UNMATCHED;
    return false;
  }
  if (lv->type == JQVAL_JBLNODE) {
    _node_to_jqval(lv->vnode, &sleft);
    lv = &sleft;
  } else if (lv->type == JQVAL_BINN) {
    _binn_to_jqval(lv->vbinn, &sleft);
    lv = &sleft;
  }
  if (lv->type >= JQVAL_JBLNODE) {
    *rcp = _JQL_ERROR_UNMATCHED;
    return false;
  }
  if (!binn_iter_init(&iter, rv->vbinn, rv->vbinn->type)) {
    *rcp = JBL_ERROR_INVALID;
    return false;
  }
  while (binn_list_next(&iter, &bv)) {
    JQVAL qv = {
      .type = JQVAL_BINN,
      .vbinn = &bv
    };
    if (!_cmp_jqval_pair(mctx, &qv, lv, rcp)) {
      if (*rcp) return false;
      return true;
    } else if (*rcp) {
      return false;
    }
  }
  return false;
}

static bool _match_jqval_pair(MCTX *mctx,
                              JQVAL *left, JQP_OP *jqop, JQVAL *right,
                              iwrc *rcp) {
  bool match = false;
  jqp_op_t op = jqop->op;
  if (op >= JQP_OP_EQ && op <= JQP_OP_LTE) {
    int cmp = _cmp_jqval_pair(mctx, left, right, rcp);
    if (*rcp) {
      goto finish;
    }
    switch (op) {
      case JQP_OP_EQ:
        match = (cmp == 0);
        break;
      case JQP_OP_GT:
        match = (cmp > 0);
        break;
      case JQP_OP_GTE:
        match = (cmp >= 0);
        break;
      case JQP_OP_LT:
        match = (cmp < 0);
        break;
      case JQP_OP_LTE:
        match = (cmp <= 0);
        break;
      default:
        break;
    }
  } else {
    switch (op) {
      case JQP_OP_RE:
        match = _match_regexp(mctx, left, jqop, right, 0, rcp);
        break;
      case JQP_OP_IN:
        match = _match_in(mctx, left, jqop, right, rcp);
        break;
      case JQP_OP_NI:
        match = _match_ni(mctx, right, jqop, left, rcp);
        break;
      default:
        break;
    }
  }
  
finish:
  if (*rcp) {
    if (*rcp == _JQL_ERROR_UNMATCHED) {
      *rcp = 0;
    }
    match = false;
  }
  if (jqop->negate) {
    match = !match;
  }
  return match;
}

static JQVAL *_unit_to_jqval(MCTX *mctx, JQPUNIT *unit, iwrc *rcp) {
  if (unit->type == JQP_STRING_TYPE) {
    if (unit->string.opaque) {
      return (JQVAL *) unit->string.opaque;
    }
    if (unit->string.flavour & JQP_STR_PLACEHOLDER) {
      *rcp = JQL_ERROR_INVALID_PLACEHOLDER;
      return 0;
    } else {
      JQVAL *qv = iwpool_alloc(sizeof(*qv), mctx->qp->aux->pool);
      if (!qv) {
        *rcp = IW_ERROR_ALLOC;
        return 0;
      }
      unit->string.opaque = qv;
      qv->type = JQVAL_STR;
      qv->vstr = unit->string.value;
    }
    return unit->string.opaque;
  } else if (unit->type == JQP_JSON_TYPE) {
    if (unit->json.opaque) {
      return (JQVAL *) unit->json.opaque;
    }
    JQVAL *qv = iwpool_alloc(sizeof(*qv), mctx->qp->aux->pool);
    if (!qv) {
      *rcp = IW_ERROR_ALLOC;
      return 0;
    }
    unit->json.opaque = qv;
    qv->type = JQVAL_JBLNODE;
    qv->vnode = &unit->json.jn;
    return unit->json.opaque;
  } else {
    *rcp = IW_ERROR_ASSERTION;
    return 0;
  }
}

static bool _match_node_expr_impl(MCTX *mctx, JQP_NODE *n, JQP_EXPR *expr, iwrc *rcp) {
  const bool negate = (expr->join && expr->join->negate);
  JQPUNIT *left = expr->left;
  JQP_OP *op = expr->op;
  JQPUNIT *right = expr->right;
  if (left->type == JQP_STRING_TYPE) {
    if (left->string.flavour & JQP_STR_STAR) {
      JQVAL lv, *rv = _unit_to_jqval(mctx, right, rcp);
      if (*rcp) return false;
      lv.type = JQVAL_STR;
      lv.vstr = mctx->key;
      bool ret = _match_jqval_pair(mctx, &lv, op, rv, rcp);
      return negate ? !ret : ret;
    } else if (strcmp(mctx->key, left->string.value)) {
      return negate;
    }
  } else if (left->type == JQP_EXPR_TYPE) {
    if (left->expr.left->type != JQP_STRING_TYPE || !(left->expr.left->string.flavour & JQP_STR_STAR)) {
      *rcp = IW_ERROR_ASSERTION;
      return false;
    }
    JQVAL lv, *rv = _unit_to_jqval(mctx, left->expr.right, rcp);
    if (*rcp) return false;
    lv.type = JQVAL_STR;
    lv.vstr = mctx->key;
    if (!_match_jqval_pair(mctx, &lv, left->expr.op, rv, rcp)) {
      return negate;
    }
  }
  JQVAL lv, *rv = _unit_to_jqval(mctx, right, rcp);
  if (*rcp) return false;
  lv.type = JQVAL_BINN;
  lv.vbinn = mctx->bv;
  bool ret = _match_jqval_pair(mctx, &lv, expr->op, rv, rcp);
  return negate ? !ret : ret;
}

static bool _match_node_expr(MCTX *mctx, JQP_NODE *n, iwrc *rcp) {
  n->start = mctx->lvl;
  n->end = n->start;
  JQPUNIT *unit = n->value;
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

IW_INLINE bool _match_node_field(MCTX *mctx, JQP_NODE *n, iwrc *rcp) {
  n->start = mctx->lvl;
  n->end = n->start;
  if (n->value->type != JQP_STRING_TYPE) {
    *rcp = IW_ERROR_ASSERTION;
    return false;
  }
  return (strcmp(n->value->string.value, mctx->key) == 0);
}

IW_INLINE JQP_NODE *_match_node_anys(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp) {
  if (n->start < 0) {
    n->start = mctx->lvl;
  }
  if (n->next) {
    JQP_NODE *nn = _match_node(mctx, n->next, res, rcp);
    if (*res) {
      n->end = -mctx->lvl; // Exclude node from matching
      n = nn;
    } else {
      n->end = INT_MAX; // Gather next level
    }
  } else {
    n->end = INT_MAX;
  }
  *res = true;
  return n;
}

static JQP_NODE *_match_node(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp) {
  switch (n->ntype) {
    case JQP_NODE_FIELD:
      *res = _match_node_field(mctx, n, rcp);
      return n;
    case JQP_NODE_EXPR:
      *res = _match_node_expr(mctx, n, rcp);
      return n;
    case JQP_NODE_ANY:
      n->start = mctx->lvl;
      n->end = n->start;
      *res = true;
      return n;
    case JQP_NODE_ANYS:
      return _match_node_anys(mctx, n, res, rcp);
  }
  return n;
}

static bool _match_filter(JQP_FILTER *f, MCTX *mctx, iwrc *rcp) {
  MFCTX *fctx = f->opaque;
  if (fctx->matched) {
    return true;
  }
  bool matched = false;
  const int lvl = mctx->lvl;
  JQP_NODE *nodes = fctx->nodes;
  if (fctx->last_lvl + 1 < lvl) {
    return false;
  }
  if (lvl <= fctx->last_lvl) {
    fctx->last_lvl = lvl - 1;
    for (JQP_NODE *n = fctx->nodes; n; n = n->next) {
      if (n->start >= lvl || -n->end >= lvl) {
        n->start = -1;
        n->end = -1;
      }
    }
  }
  for (JQP_NODE *n = fctx->nodes; n; n = n->next) {
    if (n->start < 0 || (lvl >= n->start && lvl <= n->end)) {
      n = _match_node(mctx, n, &matched, rcp);
      if (*rcp) return false;
      if (matched) {
        if (n == fctx->last_node) {
          fctx->matched = true;
          mctx->q->dirty = true;
        }
        fctx->last_lvl = lvl;
      }
      break;
    }
  }
  return fctx->matched;
}

static bool _match_expression_node(JQP_EXPR_NODE *en, MCTX *mctx, iwrc *rcp) {
  MENCTX *enctx = en->opaque;
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
    iwitoa(idx, nbuf, sizeof(nbuf));
    nkey = nbuf;
  }
  MCTX mctx = {
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
  if (vctx.pool) {
    iwpool_destroy(vctx.pool);
  }
  if (rc) {
    return rc;
  } else {
    *out = q->matched;
    return 0;
  }
}

bool jql_has_apply(JQL q) {
  return (q->qp->apply || q->qp->projection);
}

static iwrc _jql_project(JBL_NODE root, JQL q, IWPOOL *pool) {
  // TODO:
  return 0;
}

iwrc jql_apply(JQL q, const JBL jbl, JBL_NODE *out, IWPOOL *pool) {
  *out = 0;
  if (!q->qp->apply && !q->qp->projection) {
    return 0;
  }
  JBL_NODE root;
  iwrc rc = jbl_to_node(jbl, &root, pool);
  RCRET(rc);
  if (q->qp->apply) {
    rc = jbl_patch_auto(root, q->qp->apply, pool);
    RCRET(rc);
  }
  if (q->qp->projection) {
    rc = _jql_project(root, q, pool);
    RCRET(rc);
  }
  *out = root;
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
    case JQL_ERROR_REGEXP_INVALID:
      return "Invalid regular expression (JQL_ERROR_REGEXP_INVALID)";
    case JQL_ERROR_REGEXP_CHARSET:
      return "Invalid regular expression: expected ']' at end of character set (JQL_ERROR_REGEXP_CHARSET)";
    case JQL_ERROR_REGEXP_SUBEXP:
      return "Invalid regular expression: expected ')' at end of subexpression (JQL_ERROR_REGEXP_SUBEXP)";
    case JQL_ERROR_REGEXP_SUBMATCH:
      return "Invalid regular expression: expected '}' at end of submatch (JQL_ERROR_REGEXP_SUBMATCH)";
    case JQL_ERROR_REGEXP_ENGINE:
      return "Illegal instruction in compiled regular expression (please report this bug) (JQL_ERROR_REGEXP_ENGINE)";
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
