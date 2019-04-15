#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArgumentSelectionDefectsInspection"

#include "jqp.h"
#include "lwre.h"
#include "jbl_internal.h"
#include "jql_internal.h"

/** Query matching context */
typedef struct MCTX {
  int lvl;
  binn *bv;
  const char *key;
  struct _JQL *q;
  JQP_AUX *aux;
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

static JQP_NODE *_jql_match_node(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp);

IW_INLINE void _jql_jqval_destroy(JQP_STRING *pv) {
  JQVAL *qv = pv->opaque;
  if (qv) {
    void *ptr;
    switch (qv->type) {
      case JQVAL_STR:
        ptr = (void *) qv->vstr;
        break;
      case JQVAL_RE:
        ptr = (void *) qv->vre->expression;
        lwre_free(qv->vre);
        break;
      case JQVAL_JBLNODE:
        ptr = qv->vnode;
        break;
      default:
        ptr = 0;
        break;
    }
    if (ptr && qv->freefn) {
       qv->freefn(ptr, qv->freefn_op);

    }
    pv->opaque = 0;
    free(qv);
  }
}

static JQVAL *_jql_find_placeholder(JQL q, const char *name) {
  JQP_AUX *aux = q->aux;
  for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) {
    if (!strcmp(pv->value, name)) {
      return pv->opaque;
    }
  }
  return 0;
}

static iwrc _jql_set_placeholder(JQL q, const char *placeholder, int index, JQVAL *val) {
  JQP_AUX *aux = q->aux;
  if (!placeholder) { // Index
    char nbuf[JBNUMBUF_SIZE];
    iwitoa(index, nbuf, JBNUMBUF_SIZE);
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) {
      if (pv->value[0] == '?' && !strcmp(pv->value + 1, nbuf)) {
        _jql_jqval_destroy(pv);
        pv->opaque = val;
        return 0;
      }
    }
  } else {
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) {
      if (!strcmp(pv->value, placeholder)) {
        _jql_jqval_destroy(pv);
        pv->opaque = val;
        return 0;
      }
    }
  }
  return JQL_ERROR_INVALID_PLACEHOLDER;
}

iwrc jql_set_json2(JQL q, const char *placeholder, int index, JBL_NODE val,
                   void (*freefn)(void *, void *), void *op) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->freefn = freefn;
  qv->freefn_op = op;
  qv->type = JQVAL_JBLNODE;
  qv->vnode = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_json(JQL q, const char *placeholder, int index, JBL_NODE val) {
  return jql_set_json2(q, placeholder, index, val, 0, 0);
}

iwrc jql_set_i64(JQL q, const char *placeholder, int index, int64_t val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->freefn = 0;
  qv->freefn_op = 0;
  qv->type = JQVAL_I64;
  qv->vi64 = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_f64(JQL q, const char *placeholder, int index, double val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->freefn = 0;
  qv->freefn_op = 0;
  qv->type = JQVAL_F64;
  qv->vf64 = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_str2(JQL q, const char *placeholder, int index, const char *val,
                  void (*freefn)(void *, void *), void *op) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->freefn = freefn;
  qv->freefn_op = op;
  qv->type = JQVAL_STR;
  qv->vstr = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_str(JQL q, const char *placeholder, int index, const char *val) {
  return jql_set_str2(q, placeholder, index, val, 0, 0);
}

iwrc jql_set_bool(JQL q, const char *placeholder, int index, bool val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->freefn = 0;
  qv->freefn_op = 0;
  qv->type = JQVAL_BOOL;
  qv->vbool = val;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_regexp2(JQL q, const char *placeholder, int index, const char *expr,
                     void (*freefn)(void *, void *), void *op) {
  struct re *rx = lwre_new(expr);
  if (!rx) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    iwrc rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    lwre_free(rx);
    return rc;
  }
  qv->freefn = freefn;
  qv->freefn_op = op;
  qv->type = JQVAL_RE;
  qv->vre = rx;
  return _jql_set_placeholder(q, placeholder, index, qv);
}

iwrc jql_set_regexp(JQL q, const char *placeholder, int index, const char *expr) {
  return jql_set_regexp2(q, placeholder, index, expr, 0, 0);
}

iwrc jql_set_null(JQL q, const char *placeholder, int index) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  qv->freefn = 0;
  qv->freefn_op = 0;
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

static void _jql_reset_expression_node(JQP_EXPR_NODE *en, JQP_AUX *aux, bool reset_match_cache) {
  MENCTX *ectx = en->opaque;
  ectx->matched = false;
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      _jql_reset_expression_node(en, aux, reset_match_cache);
    } else if (en->type == JQP_FILTER_TYPE) {
      MFCTX *fctx = ((JQP_FILTER *) en)->opaque;
      fctx->matched = false;
      fctx->last_lvl = -1;
      for (JQP_NODE *n = fctx->nodes; n; n = n->next) {
        n->start = -1;
        n->end = -1;
        JQPUNIT *unit = n->value;
        if (reset_match_cache && unit->type == JQP_EXPR_TYPE) {
          for (JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) expr->prematched = false;
        }
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

iwrc jql_create2(JQL *qptr, const char *coll, const char *query, jql_create_mode_t mode) {
  if (!qptr || !query) {
    return IW_ERROR_INVALID_ARGS;
  }
  *qptr = 0;

  JQL q;
  JQP_AUX *aux;
  iwrc rc = jqp_aux_create(&aux, query);
  RCRET(rc);

  q = iwpool_calloc(sizeof(*q), aux->pool);
  if (!q) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  aux->mode = mode;
  q->aux = aux;

  rc = jqp_parse(aux);
  RCGO(rc, finish);

  q->coll = coll;
  q->qp = aux->query;

  if (!q->coll) {
    // Try to set collection from first query anchor
    q->coll = aux->first_anchor;
    if (!q->coll) {
      rc = JQL_ERROR_NO_COLLECTION;
      goto finish;
    }
  }

  rc = _jql_init_expression_node(aux->expr, aux);

finish:
  if (rc) {
    if ((rc == JQL_ERROR_QUERY_PARSE)
        && (mode & JQL_KEEP_QUERY_ON_PARSE_ERROR)) {
      *qptr = q;
    } else {
      jqp_aux_destroy(&aux);
    }
  } else {
    *qptr = q;
  }
  return rc;
}

iwrc jql_create(JQL *qptr, const char *coll, const char *query) {
  return jql_create2(qptr, coll, query, 0);
}

size_t jql_estimate_allocated_size(JQL q) {
  size_t ret = sizeof(struct _JQL);
  if (q->aux && q->aux->pool) {
    ret += iwpool_allocated_size(q->aux->pool);
  }
  return ret;
}

const char *jql_collection(JQL q) {
  return q->coll;
}

void jql_reset(JQL q, bool reset_match_cache, bool reset_placeholders) {
  q->matched = false;
  q->dirty = false;
  JQP_AUX *aux = q->aux;
  _jql_reset_expression_node(aux->expr, aux, reset_match_cache);
  if (reset_placeholders) {
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) { // Cleanup placeholders
      _jql_jqval_destroy(pv);
    }
  }
}

void jql_destroy(JQL *qptr) {
  if (qptr) {
    JQL q = *qptr;
    JQP_AUX *aux = q->aux;
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) { // Cleanup placeholders
      _jql_jqval_destroy(pv);
    }
    for (JQP_OP *op = aux->start_op; op; op = op->next) {
      if (op->opaque) {
        if (op->value == JQP_OP_RE) {
          lwre_free(op->opaque);
        }
      }
    }
    jqp_aux_destroy(&aux);
    *qptr = 0;
  }
}

IW_INLINE jqval_type_t _jql_binn_to_jqval(binn *vbinn, JQVAL *qval) {
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
      qval->vbool = vbinn->vbool != 0;
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
      qval->vi64 = (int64_t) vbinn->vuint64;
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
      qval->vf64 = vbinn->vfloat;
      return qval->type;
    case BINN_FLOAT64:
      qval->type = JQVAL_F64;
      qval->vf64 = vbinn->vdouble;
      return qval->type;
    default:
      memset(qval, 0, sizeof(*qval));
      break;
  }
  return JQVAL_NULL;
}

jqval_type_t jql_binn_to_jqval(binn *vbinn, JQVAL *qval) {
  return _jql_binn_to_jqval(vbinn, qval);
}

IW_INLINE void _jql_node_to_jqval(JBL_NODE jn, JQVAL *qv) {
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
    default:
      qv->type = JQVAL_NULL;
      break;
  }
}

void jql_node_to_jqval(JBL_NODE jn, JQVAL *qv) {
  _jql_node_to_jqval(jn, qv);
}

/**
 * Allowed on left:   JQVAL_STR|JQVAL_I64|JQVAL_F64|JQVAL_BOOL|JQVAL_NULL|JQVAL_BINN
 * Allowed on right:  JQVAL_STR|JQVAL_I64|JQVAL_F64|JQVAL_BOOL|JQVAL_NULL|JQVAL_JBLNODE
 */
static int _jql_cmp_jqval_pair(const JQVAL *left, const JQVAL *right, iwrc *rcp) {
  JQVAL sleft, sright;   // Stack allocated left/right converted values
  const JQVAL *lv = left, *rv = right;

  if (lv->type == JQVAL_BINN) {
    _jql_binn_to_jqval(lv->vbinn, &sleft);
    lv = &sleft;
  }
  if (rv->type == JQVAL_JBLNODE) {
    _jql_node_to_jqval(rv->vnode, &sright);
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
          return lv->vi64 > rv->vi64 ? 1 : lv->vi64 < rv->vi64 ? -1 : 0;
        case JQVAL_F64:
          return (double) lv->vi64 > rv->vf64 ? 1 : (double) lv->vi64 < rv->vf64 ? -1 : 0;
        case JQVAL_STR: {
          int64_t rval = iwatoi(rv->vstr);
          return lv->vi64 > rval ? 1 : lv->vi64 < rval ? -1 : 0;
        }
        case JQVAL_NULL:
          return 1;
        case JQVAL_BOOL: {
          return (lv->vi64 != 0) - rv->vbool;
        }
        default:
          break;
      }
      break;
    case JQVAL_F64:
      switch (rv->type) {
        case JQVAL_F64:
          return lv->vf64 > rv->vf64 ? 1 : lv->vf64 < rv->vf64 ? -1 : 0;
        case JQVAL_I64:
          return lv->vf64 > (double) rv->vi64 ? 1 : lv->vf64 < rv->vf64 ? -1 : 0;
        case JQVAL_STR: {
          double rval = (double) iwatof(rv->vstr);
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
      *rcp = _jbl_node_from_binn(lv->vbinn, &lnode, pool);
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

int jql_cmp_jqval_pair(const JQVAL *left, const JQVAL *right, iwrc *rcp) {
  return _jql_cmp_jqval_pair(left, right, rcp);
}

static bool _jql_match_regexp(JQP_AUX *aux,
                              JQVAL *left, JQP_OP *jqop, JQVAL *right,
                              iwrc *rcp) {
  struct re *rx;
  char nbuf[JBNUMBUF_SIZE];
  static_assert(JBNUMBUF_SIZE >= IWFTOA_BUFSIZE, "JBNUMBUF_SIZE >= IWFTOA_BUFSIZE");
  JQVAL sleft, sright; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  char *input = 0;
  size_t rci, match_end = 0;
  const char *expr = 0;
  bool match_start = false;

  if (lv->type == JQVAL_JBLNODE) {
    _jql_node_to_jqval(lv->vnode, &sleft);
    lv = &sleft;
  } else if (lv->type == JQVAL_BINN) {
    _jql_binn_to_jqval(lv->vbinn, &sleft);
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
      _jql_node_to_jqval(rv->vnode, &sright);
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
    rx = lwre_new(expr);
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
  int mret = lwre_match(rx, input);
  switch (mret) {
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
  if (mret > 0) {
    if (match_start && rx->position - mret != input) {
      return false;
    }
    if (match_end && rx->position != input + match_end) {
      return false;
    }
    return true;
  }
  return false;
}

static bool _jql_match_in(JQVAL *left, JQP_OP *jqop, JQVAL *right,
                          iwrc *rcp) {

  JQVAL sleft; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  if (rv->type != JQVAL_JBLNODE && rv->vnode->type != JBV_ARRAY) {
    *rcp = _JQL_ERROR_UNMATCHED;
    return false;
  }
  if (lv->type == JQVAL_JBLNODE) {
    _jql_node_to_jqval(lv->vnode, &sleft);
    lv = &sleft;
  } else if (lv->type == JQVAL_BINN) {
    _jql_binn_to_jqval(lv->vbinn, &sleft);
    lv = &sleft;
  }
  for (JBL_NODE n = rv->vnode->child; n; n = n->next) {
    JQVAL qv = {
      .type = JQVAL_JBLNODE,
      .vnode = n
    };
    if (!_jql_cmp_jqval_pair(lv, &qv, rcp)) {
      if (*rcp) return false;
      return true;
    }
    if (*rcp) return false;
  }
  return false;
}

static bool _jql_match_ni(JQVAL *left, JQP_OP *jqop, JQVAL *right,
                          iwrc *rcp) {

  JQVAL sleft; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  binn bv;
  binn_iter iter;
  if (rv->type != JQVAL_BINN || rv->vbinn->type != BINN_LIST) {
    *rcp = _JQL_ERROR_UNMATCHED;
    return false;
  }
  if (lv->type == JQVAL_JBLNODE) {
    _jql_node_to_jqval(lv->vnode, &sleft);
    lv = &sleft;
  } else if (lv->type == JQVAL_BINN) {
    _jql_binn_to_jqval(lv->vbinn, &sleft);
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
    if (!_jql_cmp_jqval_pair(&qv, lv, rcp)) {
      if (*rcp) return false;
      return true;
    } else if (*rcp) {
      return false;
    }
  }
  return false;
}

static bool _jql_match_jqval_pair(JQP_AUX *aux,
                                  JQVAL *left, JQP_OP *jqop, JQVAL *right,
                                  iwrc *rcp) {
  bool match = false;
  jqp_op_t op = jqop->value;
  if (op >= JQP_OP_EQ && op <= JQP_OP_LTE) {
    int cmp = _jql_cmp_jqval_pair(left, right, rcp);
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
        match = _jql_match_regexp(aux, left, jqop, right, rcp);
        break;
      case JQP_OP_IN:
        match = _jql_match_in(left, jqop, right, rcp);
        break;
      case JQP_OP_NI:
        match = _jql_match_ni(right, jqop, left, rcp);
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

bool jql_match_jqval_pair(JQP_AUX *aux,
                          JQVAL *left, JQP_OP *jqop, JQVAL *right,
                          iwrc *rcp) {
  return _jql_match_jqval_pair(aux, left, jqop, right, rcp);
}

static JQVAL *_jql_unit_to_jqval(JQP_AUX *aux, JQPUNIT *unit, iwrc *rcp) {
  *rcp = 0;
  switch (unit->type) {
    case JQP_STRING_TYPE: {
      if (unit->string.opaque) {
        return (JQVAL *) unit->string.opaque;
      }
      if (unit->string.flavour & JQP_STR_PLACEHOLDER) {
        *rcp = JQL_ERROR_INVALID_PLACEHOLDER;
        return 0;
      } else {
        JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
        if (!qv) {
          *rcp = IW_ERROR_ALLOC;
          return 0;
        }
        unit->string.opaque = qv;
        qv->type = JQVAL_STR;
        qv->vstr = unit->string.value;
      }
      return unit->string.opaque;
    }
    case JQP_JSON_TYPE: {
      if (unit->json.opaque) {
        return (JQVAL *) unit->json.opaque;
      }
      JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
      if (!qv) {
        *rcp = IW_ERROR_ALLOC;
        return 0;
      }
      unit->json.opaque = qv;
      struct _JBL_NODE *jn = &unit->json.jn;
      switch (jn->type) {
        case JBV_BOOL:
          qv->type = JQVAL_BOOL;
          qv->vbool = jn->vbool;
          break;
        case JBV_I64:
          qv->type = JQVAL_I64;
          qv->vi64 = jn->vi64;
          break;
        case JBV_F64:
          qv->type = JQVAL_F64;
          qv->vf64 = jn->vf64;
          break;
        case JBV_STR:
          qv->type = JQVAL_STR;
          qv->vstr = jn->vptr;
          break;
        case JBV_NULL:
          qv->type = JQVAL_NULL;
          break;
        default:
          qv->type = JQVAL_JBLNODE;
          qv->vnode = &unit->json.jn;
          break;
      }
      return unit->json.opaque;
    }
    case JQP_INTEGER_TYPE: {
      if (unit->intval.opaque) {
        return (JQVAL *) unit->intval.opaque;
      }
      JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
      if (!qv) {
        *rcp = IW_ERROR_ALLOC;
        return 0;
      }
      unit->intval.opaque = qv;
      qv->type = JQVAL_I64;
      qv->vi64 = unit->intval.value;
      return unit->intval.opaque;
    }
    case JQP_DOUBLE_TYPE: {
      if (unit->dblval.opaque) {
        return (JQVAL *) unit->dblval.opaque;
      }
      JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
      if (!qv) {
        *rcp = IW_ERROR_ALLOC;
        return 0;
      }
      unit->dblval.opaque = qv;
      qv->type = JQVAL_F64;
      qv->vf64 = unit->dblval.value;
      return unit->dblval.opaque;
    }
    default:
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      *rcp = IW_ERROR_ASSERTION;
      return 0;
  }
}

JQVAL *jql_unit_to_jqval(JQP_AUX *aux, JQPUNIT *unit, iwrc *rcp) {
  return _jql_unit_to_jqval(aux, unit, rcp);
}

static bool _jql_match_node_expr_impl(MCTX *mctx, JQP_EXPR *expr, iwrc *rcp) {
  if (expr->prematched) {
    return true;
  }
  const bool negate = (expr->join && expr->join->negate);
  JQPUNIT *left = expr->left;
  JQP_OP *op = expr->op;
  JQPUNIT *right = expr->right;
  if (left->type == JQP_STRING_TYPE) {
    if (left->string.flavour & JQP_STR_STAR) {
      JQVAL lv, *rv = _jql_unit_to_jqval(mctx->aux, right, rcp);
      if (*rcp) return false;
      lv.type = JQVAL_STR;
      lv.vstr = mctx->key;
      bool ret = _jql_match_jqval_pair(mctx->aux, &lv, op, rv, rcp);
      return negate != (0 == !ret);
    } else if (!(left->string.flavour & JQP_STR_DBL_STAR)
               && strcmp(mctx->key, left->string.value) != 0) {
      return negate;
    }
  } else if (left->type == JQP_EXPR_TYPE) {
    if (left->expr.left->type != JQP_STRING_TYPE || !(left->expr.left->string.flavour & JQP_STR_STAR)) {
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      *rcp = IW_ERROR_ASSERTION;
      return false;
    }
    JQVAL lv, *rv = _jql_unit_to_jqval(mctx->aux, left->expr.right, rcp);
    if (*rcp) return false;
    lv.type = JQVAL_STR;
    lv.vstr = mctx->key;
    if (!_jql_match_jqval_pair(mctx->aux, &lv, left->expr.op, rv, rcp)) {
      return negate;
    }
  }
  JQVAL lv, *rv = _jql_unit_to_jqval(mctx->aux, right, rcp);
  if (*rcp) return false;
  lv.type = JQVAL_BINN;
  lv.vbinn = mctx->bv;
  bool ret = _jql_match_jqval_pair(mctx->aux, &lv, expr->op, rv, rcp);
  return negate != (0 == !ret);
}

static bool _jql_match_node_expr(MCTX *mctx, JQP_NODE *n, iwrc *rcp) {
  n->start = mctx->lvl;
  n->end = n->start;
  JQPUNIT *unit = n->value;
  if (unit->type != JQP_EXPR_TYPE) {
    iwlog_ecode_error3(IW_ERROR_ASSERTION);
    *rcp = IW_ERROR_ASSERTION;
    return false;
  }
  bool prev = false;
  for (JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) {
    bool matched = _jql_match_node_expr_impl(mctx, expr, rcp);
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

IW_INLINE bool _jql_match_node_field(MCTX *mctx, JQP_NODE *n, iwrc *rcp) {
  n->start = mctx->lvl;
  n->end = n->start;
  if (n->value->type != JQP_STRING_TYPE) {
    iwlog_ecode_error3(IW_ERROR_ASSERTION);
    *rcp = IW_ERROR_ASSERTION;
    return false;
  }
  return (strcmp(n->value->string.value, mctx->key) == 0);
}

IW_INLINE JQP_NODE *_jql_match_node_anys(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp) {
  if (n->start < 0) {
    n->start = mctx->lvl;
  }
  if (n->next) {
    JQP_NODE *nn = _jql_match_node(mctx, n->next, res, rcp);
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

static JQP_NODE *_jql_match_node(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp) {
  switch (n->ntype) {
    case JQP_NODE_FIELD:
      *res = _jql_match_node_field(mctx, n, rcp);
      return n;
    case JQP_NODE_EXPR:
      *res = _jql_match_node_expr(mctx, n, rcp);
      return n;
    case JQP_NODE_ANY:
      n->start = mctx->lvl;
      n->end = n->start;
      *res = true;
      return n;
    case JQP_NODE_ANYS:
      return _jql_match_node_anys(mctx, n, res, rcp);
  }
  return n;
}

static bool _jql_match_filter(JQP_FILTER *f, MCTX *mctx, iwrc *rcp) {
  MFCTX *fctx = f->opaque;
  if (fctx->matched) {
    return true;
  }
  bool matched = false;
  const int lvl = mctx->lvl;
  if (fctx->last_lvl + 1 < lvl) {
    return false;
  }
  if (fctx->last_lvl >= lvl) {
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
      n = _jql_match_node(mctx, n, &matched, rcp);
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

static bool _jql_match_expression_node(JQP_EXPR_NODE *en, MCTX *mctx, iwrc *rcp) {
  MENCTX *enctx = en->opaque;
  if (enctx->matched) {
    return true;
  }
  bool prev = false;
  for (en = en->chain; en; en = en->next) {
    bool matched = false;
    if (en->type == JQP_EXPR_NODE_TYPE) {
      matched = _jql_match_expression_node(en, mctx, rcp);
    } else if (en->type == JQP_FILTER_TYPE) {
      matched = _jql_match_filter((JQP_FILTER *) en, mctx, rcp);
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

static jbl_visitor_cmd_t _jql_match_visitor(int lvl, binn *bv, const char *key, int idx, JBL_VCTX *vctx, iwrc *rcp) {
  char nbuf[JBNUMBUF_SIZE];
  const char *nkey = key;
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
    .aux = q->aux
  };
  q->matched = _jql_match_expression_node(mctx.aux->expr, &mctx, rcp);
  if (*rcp || q->matched) {
    return JBL_VCMD_TERMINATE;
  }
  if (q->dirty) {
    q->dirty = false;
    if (!_jql_need_deeper_match(mctx.aux->expr, lvl)) {
      return JBL_VCMD_SKIP_NESTED;
    }
  }
  return 0;
}

iwrc jql_matched(JQL q, JBL jbl, bool *out) {
  JBL_VCTX vctx = {
    .bn = &jbl->bn,
    .op = q
  };
  *out = false;
  jql_reset(q, false, false);
  JQP_EXPR_NODE *en = q->aux->expr;
  if (en->chain && !en->chain->next && !en->next) {
    en = en->chain;
    if (en->type == JQP_FILTER_TYPE) {
      JQP_NODE *n = ((JQP_FILTER *) en)->node;
      if (n && (n->ntype == JQP_NODE_ANYS || n->ntype == JQP_NODE_ANY) && !n->next) {
        // Single /* | /** matches anything
        q->matched = true;
        *out = true;
        return 0;
      }
    }
  }

  iwrc rc = _jbl_visit(0, 0, &vctx, _jql_match_visitor);
  if (vctx.pool) {
    iwpool_destroy(vctx.pool);
  }
  if (!rc) {
    *out = q->matched;
  }
  return rc;
}

const char *jql_error(JQL q) {
  if (q && q->aux) {
    return iwxstr_ptr(q->aux->xerr);
  } else {
    return "";
  }
}

const char *jql_first_anchor(JQL q) {
  return q->aux->first_anchor;
}

bool jql_has_apply(JQL q) {
  return q->aux->apply || (q->aux->qmode & JQP_QRY_APPLY_DEL);
}

bool jql_has_apply_delete(JQL q) {
  return (q->aux->qmode & JQP_QRY_APPLY_DEL);
}

bool jql_has_projection(JQL q) {
  return q->aux->projection;
}

bool jql_has_orderby(JQL q) {
  return q->aux->orderby_num > 0;
}

bool jql_has_aggregate_count(JQL q) {
  return (q->aux->qmode & JQP_QRY_AGGREGATE);
}

iwrc jql_get_skip(JQL q, int64_t *out) {
  iwrc rc = 0;
  *out = 0;
  struct JQP_AUX *aux = q->aux;
  JQPUNIT *skip = aux->skip;
  if (!skip) return 0;
  JQVAL *val = _jql_unit_to_jqval(aux, skip, &rc);
  RCRET(rc);
  if (val->type != JQVAL_I64 || val->vi64 < 0) {
    return JQL_ERROR_INVALID_PLACEHOLDER;
  }
  *out = val->vi64;
  return 0;
}

iwrc jql_get_limit(JQL q, int64_t *out) {
  iwrc rc = 0;
  *out = 0;
  struct JQP_AUX *aux = q->aux;
  JQPUNIT *limit = aux->limit;
  if (!limit) return 0;
  JQVAL *val = _jql_unit_to_jqval(aux, limit, &rc);
  RCRET(rc);
  if (val->type != JQVAL_I64 || val->vi64 < 0) {
    return JQL_ERROR_INVALID_PLACEHOLDER;
  }
  *out = val->vi64;
  return 0;
}

// ----------- JQL Projection

#define PROJ_MARK_PATH    1
#define PROJ_MARK_KEEP    2

typedef struct _PROJ_CTX {
  JQL q;
  JQP_PROJECTION *proj;
} PROJ_CTX;


static void _jql_proj_mark_up(JBL_NODE n, int amask) {
  n->flags |= amask;
  while ((n = n->parent)) {
    n->flags |= PROJ_MARK_PATH;
  }
}

static bool _jql_proj_matched(int16_t lvl, JBL_NODE n,
                              const char *key, int keylen,
                              JBN_VCTX *vctx, JQP_PROJECTION *proj,
                              iwrc *rc) {
  if (proj->cnt <= lvl) {
    return false;
  }
  if (proj->pos >= lvl) {
    proj->pos = lvl - 1;
  }
  if (proj->pos + 1 == lvl) {
    JQP_STRING *ps = proj->value;
    for (int i = 0; i < lvl; ps = ps->next, ++i);
    assert(ps);
    if (ps->flavour & JQP_STR_PROJFIELD) {
      for (JQP_STRING *sn = ps; sn; sn = sn->subnext) {
        const char *pv = sn->value;
        int pvlen = strlen(pv);
        if (pvlen == keylen && !strncmp(key, pv, keylen)) {
          proj->pos = lvl;
          return (proj->cnt == lvl + 1);
        }
      }
    } else {
      const char *pv = ps->value;
      int pvlen = strlen(pv);
      if ((pvlen == keylen && !strncmp(key, pv, keylen)) || (pv[0] == '*' && pv[1] == '\0')) {
        proj->pos = lvl;
        return (proj->cnt == lvl + 1);
      }
    }
  }
  return false;
}

static jbn_visitor_cmd_t _jql_proj_visitor(int lvl, JBL_NODE n, const char *key, int klidx, JBN_VCTX *vctx, iwrc *rc) {
  PROJ_CTX *pctx = vctx->op;
  const char *keyptr;
  char buf[JBNUMBUF_SIZE];
  if (key) {
    keyptr = key;
  } else if (lvl < 0) {
    return 0;
  } else {
    iwitoa(klidx, buf, JBNUMBUF_SIZE);
    keyptr = buf;
    klidx = strlen(keyptr);
  }
  for (JQP_PROJECTION *p = pctx->proj; p; p = p->next) {
    bool matched = _jql_proj_matched((int16_t) lvl, n, keyptr, klidx, vctx, p, rc);
    RCRET(*rc);
    if (matched) {
      if (p->exclude) {
        return JBN_VCMD_DELETE;
      } else {
        _jql_proj_mark_up(n, PROJ_MARK_KEEP);
      }
    }
  }
  return 0;
}

static jbn_visitor_cmd_t _jql_proj_keep_visitor(int lvl, JBL_NODE n, const char *key, int klidx, JBN_VCTX *vctx,
                                                iwrc *rc) {
  if (lvl < 0 || (n->flags & PROJ_MARK_PATH)) {
    return 0;
  }
  if (n->flags & PROJ_MARK_KEEP) {
    return JBL_VCMD_SKIP_NESTED;
  }
  return JBN_VCMD_DELETE;
}

static iwrc _jql_project(JBL_NODE root, JQL q) {

  bool has_includes = false;
  JQP_PROJECTION *proj = q->aux->projection;

  // Check trivial cases
  for (JQP_PROJECTION *p = proj; p; p = p->next) {
    bool all = (p->value->flavour & JQP_STR_PROJALIAS);
    if (all) {
      if (p->exclude) { // Got -all in chain return empty object
        jbl_node_reset_data(root);
        return 0;
      } else {
        proj = p->next; // Dispose all before +all
      }
    } else if (!has_includes && !p->exclude) {
      has_includes = true;
    }
  }
  if (!proj) {
    // keep whole node
    return 0;
  }
  PROJ_CTX pctx = {
    .q = q,
    .proj = proj,
  };
  for (JQP_PROJECTION *p = proj; p; p = p->next) {
    p->pos = -1;
    p->cnt = 0;
    for (JQP_STRING *s = p->value; s; s = s->next) p->cnt++;
  }
  JBN_VCTX vctx = {
    .root = root,
    .op = &pctx
  };
  iwrc rc = jbn_visit(root, 0, &vctx, _jql_proj_visitor);
  RCRET(rc);

  if (has_includes || (root->flags & PROJ_MARK_PATH)) { // We have keep projections
    rc = jbn_visit(root, 0, &vctx, _jql_proj_keep_visitor);
    RCRET(rc);
  }

  return rc;
}

#undef PROJ_MARK_PATH
#undef PROJ_MARK_KEEP

//----------------------------------

iwrc jql_apply(JQL q, JBL_NODE root, IWPOOL *pool) {
  if (q->aux->apply) {
    return jbl_patch_auto(root, q->aux->apply, pool);
  } else {
    return 0;
  }
}

iwrc jql_project(JQL q, JBL_NODE root) {
  if (q->aux->projection) {
    return _jql_project(root, q);
  } else {
    return 0;
  }
}

iwrc jql_apply_and_project(JQL q, JBL jbl, JBL_NODE *out, IWPOOL *pool) {
  *out = 0;
  JQP_AUX *aux = q->aux;
  if (!aux->apply && !aux->projection) {
    return 0;
  }
  JBL_NODE root;
  iwrc rc = jbl_to_node(jbl, &root, pool);
  RCRET(rc);
  if (aux->apply) {
    rc = jql_apply(q, root, pool);
    RCRET(rc);
  }
  if (aux->projection) {
    rc = jql_project(q, root);
  }
  if (!rc) {
    *out = root;
  }
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
    case JQL_ERROR_SKIP_ALREADY_SET:
      return "Skip clause already specified (JQL_ERROR_SKIP_ALREADY_SET)";
    case JQL_ERROR_LIMIT_ALREADY_SET:
      return "Limit clause already specified (JQL_ERROR_SKIP_ALREADY_SET)";
    case JQL_ERROR_ORDERBY_MAX_LIMIT:
      return "Reached max number of asc/desc order clauses: 64 (JQL_ERROR_ORDERBY_MAX_LIMIT)";
    case JQL_ERROR_NO_COLLECTION:
      return "No collection specified in query (JQL_ERROR_NO_COLLECTION)";
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

#pragma clang diagnostic pop
