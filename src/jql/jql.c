#include "ejdb2_internal.h"
#include "jql_internal.h"
#include "jqp.h"

#include <iowow/iwre.h>

#include <errno.h>

/** Query matching context */
typedef struct MCTX {
  int   lvl;
  binn *bv;
  const char  *key;
  struct _JQL *q;
  JQP_AUX     *aux;
  JBL_VCTX    *vctx;
} MCTX;

/** Expression node matching context */
typedef struct MENCTX {
  bool matched;
} MENCTX;

/** Filter matching context */
typedef struct MFCTX {
  bool      matched;
  int       last_lvl;     /**< Last matched level */
  JQP_NODE *nodes;
  JQP_NODE *last_node;
  JQP_FILTER *qpf;
} MFCTX;

static JQP_NODE* _jql_match_node(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp);

IW_INLINE void _jql_jqval_destroy(JQP_STRING *pv) {
  JQVAL *qv = pv->opaque;
  if (qv) {
    void *ptr;
    switch (qv->type) {
      case JQVAL_STR:
        ptr = (void*) qv->vstr;
        break;
      case JQVAL_RE:
        ptr = (void*) iwre_pattern_get(qv->vre);
        iwre_destroy(qv->vre);
        break;
      case JQVAL_JBLNODE:
        ptr = qv->vnode;
        break;
      default:
        ptr = 0;
        break;
    }
    if (--qv->refs <= 0) {
      if (ptr && qv->freefn) {
        qv->freefn(ptr, qv->freefn_op);
      }
      free(qv);
    }
    pv->opaque = 0;
  }
}

static JQVAL* _jql_find_placeholder(JQL q, const char *name) {
  JQP_AUX *aux = q->aux;
  for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) {
    if (!strcmp(pv->value, name)) {
      return pv->opaque;
    }
  }
  return 0;
}

JQVAL* jql_find_placeholder(JQL q, const char *name) {
  return _jql_find_placeholder(q, name);
}

static iwrc _jql_set_placeholder(JQL q, const char *placeholder, int index, JQVAL *val) {
  JQP_AUX *aux = q->aux;
  iwrc rc = JQL_ERROR_INVALID_PLACEHOLDER;
  if (!placeholder) { // Index
    char nbuf[IWNUMBUF_SIZE];
    iwitoa(index, nbuf, IWNUMBUF_SIZE);
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) {
      if ((pv->value[0] == '?') && !strcmp(pv->value + 1, nbuf)) {
        if ((pv->flavour & (JQP_STR_PROJFIELD | JQP_STR_PROJPATH)) && val->type != JQVAL_STR) {
          return JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE;
        }
        _jql_jqval_destroy(pv);
        pv->opaque = val;
        val->refs++;
        return 0;
      }
    }
  } else {
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) {
      if (!strcmp(pv->value, placeholder)) {
        if ((pv->flavour & (JQP_STR_PROJFIELD | JQP_STR_PROJPATH)) && val->type != JQVAL_STR) {
          rc = JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE;
          goto finish;
        }
        _jql_jqval_destroy(pv);
        pv->opaque = val;
        val->refs++;
        rc = 0;
      }
    }
  }
finish:
  if (rc) {
    val->refs = 0;
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) {
      if (pv->opaque == val) {
        pv->opaque = 0;
      }
    }
  }
  return rc;
}

iwrc jql_set_json2(
  JQL q, const char *placeholder, int index, JBL_NODE val,
  void (*freefn)(void*, void*), void *op
  ) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  qv->refs = 0;
  qv->freefn = freefn;
  qv->freefn_op = op;
  qv->type = JQVAL_JBLNODE;
  qv->vnode = val;
  iwrc rc = _jql_set_placeholder(q, placeholder, index, qv);
  if (rc) {
    if (freefn) {
      freefn(val, op);
    }
    free(qv);
  }
  return rc;
}

iwrc jql_set_json(JQL q, const char *placeholder, int index, JBL_NODE val) {
  return jql_set_json2(q, placeholder, index, val, 0, 0);
}

static void _jql_free_iwpool(void *ptr, void *op) {
  iwpool_destroy((IWPOOL*) op);
}

iwrc jql_set_json_jbl(JQL q, const char *placeholder, int index, JBL jbl) {
  IWPOOL *pool = iwpool_create(jbl_size(jbl));
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc;
  JBL_NODE n;
  RCC(rc, finish, jbl_to_node(jbl, &n, true, pool));
  rc = jql_set_json2(q, placeholder, index, n, _jql_free_iwpool, pool);

finish:
  if (rc) {
    iwpool_destroy(pool);
  }
  return rc;
}

iwrc jql_set_i64(JQL q, const char *placeholder, int index, int64_t val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  qv->refs = 0;
  qv->freefn = 0;
  qv->freefn_op = 0;
  qv->type = JQVAL_I64;
  qv->vi64 = val;
  iwrc rc = _jql_set_placeholder(q, placeholder, index, qv);
  if (rc) {
    free(qv);
  }
  return rc;
}

iwrc jql_set_f64(JQL q, const char *placeholder, int index, double val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  qv->refs = 0;
  qv->freefn = 0;
  qv->freefn_op = 0;
  qv->type = JQVAL_F64;
  qv->vf64 = val;
  iwrc rc = _jql_set_placeholder(q, placeholder, index, qv);
  if (rc) {
    free(qv);
  }
  return rc;
}

iwrc jql_set_str2(
  JQL q, const char *placeholder, int index, const char *val,
  void (*freefn)(void*, void*), void *op
  ) {
  if (val == 0) {
    if (freefn) {
      freefn((void*) val, op);
    }
    return jql_set_null(q, placeholder, index);
  }

  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  qv->refs = 0;
  qv->freefn = freefn;
  qv->freefn_op = op;
  qv->type = JQVAL_STR;
  qv->vstr = val;
  iwrc rc = _jql_set_placeholder(q, placeholder, index, qv);
  if (rc) {
    if (freefn) {
      freefn((void*) val, op);
    }
    free(qv);
  }
  return rc;
}

iwrc jql_set_str(JQL q, const char *placeholder, int index, const char *val) {
  return jql_set_str2(q, placeholder, index, val, 0, 0);
}

static void _freefn_str(void *v, void *op) {
  free(v);
}

iwrc jql_set_str3(JQL q, const char *placeholder, int index, const char *val_, size_t val_len) {
  char *val = strndup(val_, val_len);
  if (!val) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  return jql_set_str2(q, placeholder, index, val, _freefn_str, 0);
}

iwrc jql_set_bool(JQL q, const char *placeholder, int index, bool val) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  qv->refs = 0;
  qv->freefn = 0;
  qv->freefn_op = 0;
  qv->type = JQVAL_BOOL;
  qv->vbool = val;
  iwrc rc = _jql_set_placeholder(q, placeholder, index, qv);
  if (rc) {
    free(qv);
  }
  return rc;
}

iwrc jql_set_regexp2(
  JQL q, const char *placeholder, int index, const char *expr,
  void (*freefn)(void*, void*), void *op
  ) {
  iwrc rc = 0;
  JQVAL *qv = 0;
  struct iwre *rx = iwre_create(expr);
  if (!rx) {
    rc = JQL_ERROR_REGEXP_INVALID;
    goto finish;
  }
  RCA(qv = malloc(sizeof(*qv)), finish);
  qv->refs = 0;
  qv->freefn = freefn;
  qv->freefn_op = op;
  qv->type = JQVAL_RE;
  qv->vre = rx;
  rc = _jql_set_placeholder(q, placeholder, index, qv);

finish:
  if (rc) {
    if (freefn) {
      freefn((void*) expr, op);
    }
    iwre_destroy(rx);
    free(qv);
  }
  return rc;
}

iwrc jql_set_regexp(JQL q, const char *placeholder, int index, const char *expr) {
  return jql_set_regexp2(q, placeholder, index, expr, 0, 0);
}

iwrc jql_set_null(JQL q, const char *placeholder, int index) {
  JQVAL *qv = malloc(sizeof(*qv));
  if (!qv) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  qv->refs = 0;
  qv->freefn = 0;
  qv->freefn_op = 0;
  qv->type = JQVAL_NULL;
  iwrc rc = _jql_set_placeholder(q, placeholder, index, qv);
  if (rc) {
    free(qv);
  }
  return rc;
}

static bool _jql_need_deeper_match(JQP_EXPR_NODE *en, int lvl) {
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      if (_jql_need_deeper_match(en, lvl)) {
        return true;
      }
    } else if (en->type == JQP_FILTER_TYPE) {
      MFCTX *fctx = ((JQP_FILTER*) en)->opaque;
      if (!fctx->matched && (fctx->last_lvl == lvl)) {
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
      MFCTX *fctx = ((JQP_FILTER*) en)->opaque;
      fctx->matched = false;
      fctx->last_lvl = -1;
      for (JQP_NODE *n = fctx->nodes; n; n = n->next) {
        n->start = -1;
        n->end = -1;
        JQPUNIT *unit = n->value;
        if (reset_match_cache && (unit->type == JQP_EXPR_TYPE)) {
          for (JQP_EXPR *expr = &unit->expr; expr; expr = expr->next) expr->prematched = false;
        }
      }
    }
  }
}

static iwrc _jql_init_expression_node(JQP_EXPR_NODE *en, JQP_AUX *aux) {
  en->opaque = iwpool_calloc(sizeof(MENCTX), aux->pool);
  if (!en->opaque) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  for (en = en->chain; en; en = en->next) {
    if (en->type == JQP_EXPR_NODE_TYPE) {
      iwrc rc = _jql_init_expression_node(en, aux);
      RCRET(rc);
    } else if (en->type == JQP_FILTER_TYPE) {
      MFCTX *fctx = iwpool_calloc(sizeof(*fctx), aux->pool);
      JQP_FILTER *f = (JQP_FILTER*) en;
      if (!fctx) {
        return iwrc_set_errno(IW_ERROR_ALLOC, errno);
      }
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

  RCC(rc, finish, jqp_parse(aux));

  if (coll && *coll != '\0') {
    // Get a copy of collection name
    q->coll = iwpool_strdup2(aux->pool, coll);
  }

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
    if (  (rc == JQL_ERROR_QUERY_PARSE)
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

const char* jql_collection(JQL q) {
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
  if (!qptr) {
    return;
  }
  JQL q = *qptr;
  if (q) {
    JQP_AUX *aux = q->aux;
    for (JQP_STRING *pv = aux->start_placeholder; pv; pv = pv->placeholder_next) { // Cleanup placeholders
      _jql_jqval_destroy(pv);
    }
    for (JQP_OP *op = aux->start_op; op; op = op->next) {
      if (op->opaque) {
        if (op->value == JQP_OP_RE) {
          iwre_destroy(op->opaque);
        }
      }
    }
    jqp_aux_destroy(&aux);
  }
  *qptr = 0;
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
      qval->vi64 = vbinn->vint8; // NOLINT(bugprone-signed-char-misuse)
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
          int l1 = (int) strlen(lv->vstr);
          int l2 = (int) strlen(rv->vstr);
          if (l1 != l2) {
            return l1 - l2;
          }
          return strncmp(lv->vstr, rv->vstr, l1);
        }
        case JQVAL_BOOL:
          return !strcmp(lv->vstr, "true") - rv->vbool;
        case JQVAL_I64: {
          char nbuf[IWNUMBUF_SIZE];
          iwitoa(rv->vi64, nbuf, IWNUMBUF_SIZE);
          return strcmp(lv->vstr, nbuf);
        }
        case JQVAL_F64: {
          size_t osz;
          char nbuf[IWNUMBUF_SIZE];
          iwjson_ftoa(rv->vf64, nbuf, &osz);
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
          return lv->vbool - (rv->vf64 != 0.0); // -V550
        case JQVAL_STR: {
          if (strcmp(rv->vstr, "true") == 0) {
            return lv->vbool - 1;
          } else if (strcmp(rv->vstr, "false") == 0) {
            return lv->vbool;
          } else {
            return -1;
          }
        }
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
      if (  (rv->type != JQVAL_JBLNODE)
         || ((rv->vnode->type == JBV_ARRAY) && (lv->vbinn->type != BINN_LIST))
         || ((rv->vnode->type == JBV_OBJECT) && ((lv->vbinn->type != BINN_OBJECT) && (lv->vbinn->type != BINN_MAP)))) {
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
      *rcp = _jbl_node_from_binn(lv->vbinn, &lnode, false, pool);
      if (*rcp) {
        iwpool_destroy(pool);
        return 0;
      }
      int cmp = jbn_compare_nodes(lnode, rv->vnode, rcp);
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

static bool _jql_match_regexp(
  JQP_AUX *aux,
  JQVAL *left, JQP_OP *jqop, JQVAL *right,
  iwrc *rcp
  ) {
  struct iwre *rx;
  char nbuf[IWNUMBUF_SIZE];
  JQVAL sleft, sright; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  char *input = 0;
  const char *expr = 0;

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
        iwitoa(rv->vi64, nbuf, IWNUMBUF_SIZE);
        expr = iwpool_strdup(aux->pool, nbuf, rcp);
        if (*rcp) {
          return false;
        }
        break;
      }
      case JQVAL_F64: {
        size_t osz;
        iwjson_ftoa(rv->vf64, nbuf, &osz);
        expr = iwpool_strdup(aux->pool, nbuf, rcp);
        if (*rcp) {
          return false;
        }
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
    rx = iwre_create(expr);
    if (!rx) {
      *rcp = JQL_ERROR_REGEXP_INVALID;
      return false;
    }
    jqop->opaque = rx;
  }

  switch (lv->type) {
    case JQVAL_STR:
      input = (char*) lv->vstr;
      break;
    case JQVAL_I64:
      iwitoa(lv->vi64, nbuf, IWNUMBUF_SIZE);
      input = nbuf;
      break;
    case JQVAL_F64: {
      size_t osz;
      iwjson_ftoa(lv->vf64, nbuf, &osz);
      input = nbuf;
    }
    break;
    case JQVAL_BOOL:
      input = lv->vbool ? "true" : "false";
      break;
    default:
      *rcp = _JQL_ERROR_UNMATCHED;
      return false;
  }
  const char *mpairs[IWRE_MAX_MATCHES];
  int mret = iwre_match(rx, input, mpairs, IWRE_MAX_MATCHES);
  return mret > 0;
}

static bool _jql_match_in(
  JQVAL *left, JQP_OP *jqop, JQVAL *right,
  iwrc *rcp
  ) {
  JQVAL sleft; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  if ((rv->type != JQVAL_JBLNODE) && (rv->vnode->type != JBV_ARRAY)) {
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
      .type  = JQVAL_JBLNODE,
      .vnode = n
    };
    if (!_jql_cmp_jqval_pair(lv, &qv, rcp)) {
      if (*rcp) {
        return false;
      }
      return true;
    }
    if (*rcp) {
      return false;
    }
  }
  return false;
}

static bool _jql_match_ni(
  JQVAL *left, JQP_OP *jqop, JQVAL *right,
  iwrc *rcp
  ) {
  JQVAL sleft; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  binn bv;
  binn_iter iter;
  if ((rv->type != JQVAL_BINN) || (rv->vbinn->type != BINN_LIST)) {
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
      .type  = JQVAL_BINN,
      .vbinn = &bv
    };
    if (!_jql_cmp_jqval_pair(&qv, lv, rcp)) {
      if (*rcp) {
        return false;
      }
      return true;
    } else if (*rcp) {
      return false;
    }
  }
  return false;
}

static bool _jql_match_starts(
  JQVAL *left, JQP_OP *jqop, JQVAL *right,
  iwrc *rcp
  ) {
  JQVAL sleft; // Stack allocated left/right converted values
  JQVAL *lv = left, *rv = right;
  char nbuf[IWNUMBUF_SIZE];
  char nbuf2[IWNUMBUF_SIZE];
  char *input = 0, *prefix = 0;

  if (lv->type == JQVAL_JBLNODE) {
    _jql_node_to_jqval(lv->vnode, &sleft);
    lv = &sleft;
  } else if (lv->type == JQVAL_BINN) {
    _jql_binn_to_jqval(lv->vbinn, &sleft);
    lv = &sleft;
  }
  switch (lv->type) {
    case JQVAL_STR:
      input = (char*) lv->vstr;
      break;
    case JQVAL_I64:
      iwitoa(lv->vi64, nbuf, IWNUMBUF_SIZE);
      input = nbuf;
      break;
    case JQVAL_F64: {
      size_t osz;
      iwjson_ftoa(lv->vf64, nbuf, &osz);
      input = nbuf;
      break;
    }
    case JQVAL_BOOL:
      input = lv->vbool ? "true" : "false";
      break;
    default:
      *rcp = _JQL_ERROR_UNMATCHED;
      return false;
  }
  switch (rv->type) {
    case JQVAL_STR:
      prefix = (char*) rv->vstr;
      break;
    case JQVAL_I64:
      iwitoa(rv->vi64, nbuf2, IWNUMBUF_SIZE);
      prefix = nbuf2;
      break;
    case JQVAL_F64: {
      size_t osz;
      iwjson_ftoa(rv->vf64, nbuf2, &osz);
      prefix = nbuf2;
      break;
    }
    case JQVAL_BOOL:
      prefix = rv->vbool ? "true" : "false";
      break;
    default:
      *rcp = _JQL_ERROR_UNMATCHED;
      return false;
  }
  size_t plen = strlen(prefix);
  if (plen > 0) {
    return strncmp(input, prefix, plen) == 0;
  } else {
    return true;
  }
}

static bool _jql_match_jqval_pair(
  JQP_AUX *aux,
  JQVAL *left, JQP_OP *jqop, JQVAL *right,
  iwrc *rcp
  ) {
  bool match = false;
  jqp_op_t op = jqop->value;
  if ((op >= JQP_OP_EQ) && (op <= JQP_OP_LTE)) {
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
      case JQP_OP_PREFIX:
        match = _jql_match_starts(left, jqop, right, rcp);
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

bool jql_match_jqval_pair(
  JQP_AUX *aux,
  JQVAL *left, JQP_OP *jqop, JQVAL *right,
  iwrc *rcp
  ) {
  return _jql_match_jqval_pair(aux, left, jqop, right, rcp);
}

static JQVAL* _jql_unit_to_jqval(JQP_AUX *aux, JQPUNIT *unit, iwrc *rcp) {
  *rcp = 0;
  switch (unit->type) {
    case JQP_STRING_TYPE: {
      if (unit->string.opaque) {
        return (JQVAL*) unit->string.opaque;
      }
      if (unit->string.flavour & JQP_STR_PLACEHOLDER) {
        *rcp = JQL_ERROR_INVALID_PLACEHOLDER;
        return 0;
      } else {
        JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
        if (!qv) {
          *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
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
        return (JQVAL*) unit->json.opaque;
      }
      JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
      if (!qv) {
        *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
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
        return (JQVAL*) unit->intval.opaque;
      }
      JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
      if (!qv) {
        *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
        return 0;
      }
      unit->intval.opaque = qv;
      qv->type = JQVAL_I64;
      qv->vi64 = unit->intval.value;
      return unit->intval.opaque;
    }
    case JQP_DOUBLE_TYPE: {
      if (unit->dblval.opaque) {
        return (JQVAL*) unit->dblval.opaque;
      }
      JQVAL *qv = iwpool_calloc(sizeof(*qv), aux->pool);
      if (!qv) {
        *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
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

JQVAL* jql_unit_to_jqval(JQP_AUX *aux, JQPUNIT *unit, iwrc *rcp) {
  return _jql_unit_to_jqval(aux, unit, rcp);
}

bool jql_jqval_as_int(JQVAL *jqval, int64_t *out) {
  switch (jqval->type) {
    case JQVAL_I64:
      *out = jqval->vi64;
      return true;
    case JQVAL_STR:
      *out = iwatoi(jqval->vstr);
      return true;
    case JQVAL_F64:
      *out = jqval->vf64;
      return true;
    case JQVAL_BOOL:
      *out = jqval->vbool ? 1 : 0;
      return true;
    case JQVAL_JBLNODE: {
      JBL_NODE n = jqval->vnode;
      switch (n->type) {
        case JBV_I64:
          *out = n->vi64;
          return true;
        case JBV_STR:
          *out = iwatoi(jqval->vstr);
          return true;
        case JBV_F64:
          *out = n->vf64;
          return true;
        case JBV_BOOL:
          *out = n->vbool ? 1 : 0;
          return true;
        default:
          *out = 0;
          return false;
      }
    }
    default:
      *out = 0;
      return false;
  }
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
      if (*rcp) {
        return false;
      }
      lv.type = JQVAL_STR;
      lv.vstr = mctx->key;
      bool ret = _jql_match_jqval_pair(mctx->aux, &lv, op, rv, rcp);
      return negate != (0 == !ret);
    } else if (  !(left->string.flavour & JQP_STR_DBL_STAR)
              && (strcmp(mctx->key, left->string.value) != 0)) {
      return negate;
    }
  } else if (left->type == JQP_EXPR_TYPE) {
    if ((left->expr.left->type != JQP_STRING_TYPE) || !(left->expr.left->string.flavour & JQP_STR_STAR)) {
      iwlog_ecode_error3(IW_ERROR_ASSERTION);
      *rcp = IW_ERROR_ASSERTION;
      return false;
    }
    JQVAL lv, *rv = _jql_unit_to_jqval(mctx->aux, left->expr.right, rcp);
    if (*rcp) {
      return false;
    }
    lv.type = JQVAL_STR;
    lv.vstr = mctx->key;
    if (!_jql_match_jqval_pair(mctx->aux, &lv, left->expr.op, rv, rcp)) {
      return negate;
    }
  }
  JQVAL lv, *rv = _jql_unit_to_jqval(mctx->aux, right, rcp);
  if (*rcp) {
    return false;
  }
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
    if (*rcp) {
      return false;
    }
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

IW_INLINE JQP_NODE* _jql_match_node_anys(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp) {
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

static JQP_NODE* _jql_match_node(MCTX *mctx, JQP_NODE *n, bool *res, iwrc *rcp) {
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
      if ((n->start >= lvl) || (-n->end >= lvl)) {
        n->start = -1;
        n->end = -1;
      }
    }
  }
  for (JQP_NODE *n = fctx->nodes; n; n = n->next) {
    if ((n->start < 0) || ((lvl >= n->start) && (lvl <= n->end))) {
      n = _jql_match_node(mctx, n, &matched, rcp);
      if (*rcp) {
        return false;
      }
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
      matched = _jql_match_filter((JQP_FILTER*) en, mctx, rcp);
    }
    if (*rcp) {
      return JBL_VCMD_TERMINATE;
    }
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
  char nbuf[IWNUMBUF_SIZE];
  const char *nkey = key;
  JQL q = vctx->op;
  if (!nkey) {
    iwitoa(idx, nbuf, sizeof(nbuf));
    nkey = nbuf;
  }
  MCTX mctx = {
    .lvl  = lvl,
    .bv   = bv,
    .key  = nkey,
    .vctx = vctx,
    .q    = q,
    .aux  = q->aux
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
  JQP_EXPR_NODE *en = q->aux->expr;
  if (en->flags & JQP_EXPR_NODE_FLAG_PK) {
    q->matched = true;
    *out = true;
    return 0;
  }
  *out = false;
  jql_reset(q, false, false);
  if (en->chain && !en->chain->next && !en->next) {
    en = en->chain;
    if (en->type == JQP_FILTER_TYPE) {
      JQP_NODE *n = ((JQP_FILTER*) en)->node;
      if (n && ((n->ntype == JQP_NODE_ANYS) || (n->ntype == JQP_NODE_ANY)) && !n->next) {
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

const char* jql_error(JQL q) {
  if (q && q->aux) {
    return iwxstr_ptr(q->aux->xerr);
  } else {
    return "";
  }
}

const char* jql_first_anchor(JQL q) {
  return q->aux->first_anchor;
}

bool jql_has_apply(JQL q) {
  return q->aux->apply || q->aux->apply_placeholder || (q->aux->qmode & (JQP_QRY_APPLY_DEL | JQP_QRY_APPLY_UPSERT));
}

bool jql_has_apply_upsert(JQL q) {
  return (q->aux->qmode & JQP_QRY_APPLY_UPSERT);
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
  if (!skip) {
    return 0;
  }
  JQVAL *val = _jql_unit_to_jqval(aux, skip, &rc);
  RCRET(rc);
  if ((val->type != JQVAL_I64) || (val->vi64 < 0)) { // -V522
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
  if (!limit) {
    return 0;
  }
  JQVAL *val = _jql_unit_to_jqval(aux, limit, &rc);
  RCRET(rc);
  if ((val->type != JQVAL_I64) || (val->vi64 < 0)) { // -V522
    return JQL_ERROR_INVALID_PLACEHOLDER;
  }
  *out = val->vi64;
  return 0;
}

// ----------- JQL Projection

#define PROJ_MARK_PATH      0x01
#define PROJ_MARK_KEEP      0x02
#define PROJ_MARK_FROM_JOIN 0x04

typedef struct _PROJ_CTX {
  JQL q;
  JQP_PROJECTION *proj;
  IWPOOL *pool;
  JBEXEC *exec_ctx; // Optional!
} PROJ_CTX;

static void _jql_proj_mark_up(JBL_NODE n, int amask) {
  n->flags |= amask;
  while ((n = n->parent)) {
    n->flags |= PROJ_MARK_PATH;
  }
}

static bool _jql_proj_matched(
  int16_t lvl, JBL_NODE n,
  const char *key, int keylen,
  JBN_VCTX *vctx, JQP_PROJECTION *proj,
  iwrc *rc
  ) {
  if (proj->cnt <= lvl) {
    return false;
  }
  if (proj->pos >= lvl) {
    proj->pos = lvl - 1;
  }
  if (proj->pos + 1 == lvl) {
    JQP_STRING *ps = proj->value;
    for (int i = 0; i < lvl; ps = ps->next, ++i);  // -V529
    assert(ps);
    if (ps->flavour & JQP_STR_PROJFIELD) {
      for (JQP_STRING *sn = ps; sn; sn = sn->subnext) {
        const char *pv = IW_UNLIKELY(sn->flavour & JQP_STR_PLACEHOLDER) ? ((JQVAL*) sn->opaque)->vstr : sn->value;
        int pvlen = (int) strlen(pv);
        if ((pvlen == keylen) && !strncmp(key, pv, keylen)) {
          proj->pos = lvl;
          return (proj->cnt == lvl + 1);
        }
      }
    } else {
      const char *pv = IW_UNLIKELY(ps->flavour & JQP_STR_PLACEHOLDER) ? ((JQVAL*) ps->opaque)->vstr : ps->value;
      int pvlen = (int) strlen(pv);
      if (((pvlen == keylen) && !strncmp(key, pv, keylen)) || ((pv[0] == '*') && (pv[1] == '\0'))) {
        proj->pos = lvl;
        return (proj->cnt == lvl + 1);
      }
    }
  }
  return false;
}

static bool _jql_proj_join_matched(
  int16_t lvl, JBL_NODE n,
  const char *key, int keylen,
  JBN_VCTX *vctx, JQP_PROJECTION *proj,
  JBL *out,
  iwrc *rcp
  ) {
  PROJ_CTX *pctx = vctx->op;
  if (proj->cnt != lvl + 1) {
    return _jql_proj_matched(lvl, n, key, keylen, vctx, proj, rcp);
  }

  iwrc rc = 0;
  JBL jbl = 0;
  const char *pv, *spos;
  bool ret = false;
  JQP_STRING *ps = proj->value;
  for (int i = 0; i < lvl; ps = ps->next, ++i);  // -V529
  assert(ps);

  if (ps->flavour & JQP_STR_PROJFIELD) {
    for (JQP_STRING *sn = ps; sn; sn = sn->subnext) {
      pv = IW_UNLIKELY(sn->flavour & JQP_STR_PLACEHOLDER) ? ((JQVAL*) sn->opaque)->vstr : sn->value;
      spos = strchr(pv, '<');
      if (!spos) {
        if ((strlen(pv) == keylen) && !strncmp(key, pv, keylen)) {
          proj->pos = lvl;
          return true;
        }
      }
      ret = !strncmp(key, pv, spos - pv);
      if (ret) {
        break;
      }
    }
  } else {
    pv = IW_UNLIKELY(ps->flavour & JQP_STR_PLACEHOLDER) ? ((JQVAL*) ps->opaque)->vstr : ps->value;
    spos = strchr(pv, '<');
    assert(spos);
    ret = !strncmp(key, pv, spos - pv);
  }
  if (ret) {
    JBL_NODE nn;
    JQVAL jqval;
    int64_t id;
    JBEXEC *exec_ctx = pctx->exec_ctx;
    const char *coll = spos + 1;
    if (*coll == '\0') {
      return false;
    }
    jql_node_to_jqval(n, &jqval);
    if (!jql_jqval_as_int(&jqval, &id)) {
      // Unable to convert current node value as int number
      return false;
    }
    IWHMAP *cache = exec_ctx->proj_joined_nodes_cache;
    IWPOOL *pool = exec_ctx->ux->pool;
    if (!pool) {
      pool = exec_ctx->proj_joined_nodes_pool;
      if (!pool) {
        pool = iwpool_create(512);
        RCGA(pool, finish);
        exec_ctx->proj_joined_nodes_pool = pool;
      } else if (cache && (iwpool_used_size(pool) > 10 * 1024 * 1024)) { // 10Mb
        exec_ctx->proj_joined_nodes_cache = 0;
        iwhmap_destroy(exec_ctx->proj_joined_nodes_cache);
        cache = 0;
        iwpool_destroy(pool);
        pool = iwpool_create(1024 * 1024); // 1Mb
        RCGA(pool, finish);
      }
    }
    if (!cache) {
      RCB(finish, cache = iwhmap_create(jb_proj_node_cache_cmp, jb_proj_node_hash, jb_proj_node_kvfree));
      exec_ctx->proj_joined_nodes_cache = cache;
    }
    struct _JBDOCREF ref = {
      .id   = id,
      .coll = coll
    };
    nn = iwhmap_get(cache, &ref);
    if (!nn) {
      rc = jb_collection_join_resolver(id, coll, &jbl, exec_ctx);
      if (rc) {
        if ((rc == IW_ERROR_NOT_EXISTS) || (rc == IWKV_ERROR_NOTFOUND)) {
          // If collection is not exists or record is not found just
          // keep all untouched
          rc = 0;
          ret = false;
        }
        goto finish;
      }
      RCC(rc, finish, jbl_to_node(jbl, &nn, true, pool));
      struct _JBDOCREF *refkey = malloc(sizeof(*refkey));
      RCGA(refkey, finish);
      *refkey = ref;
      RCC(rc, finish, iwhmap_put(cache, refkey, nn));
    }
    jbn_apply_from(n, nn);
    proj->pos = lvl;
  }

finish:
  jbl_destroy(&jbl);
  *rcp = rc;
  return ret;
}

static jbn_visitor_cmd_t _jql_proj_visitor(int lvl, JBL_NODE n, const char *key, int klidx, JBN_VCTX *vctx, iwrc *rc) {
  PROJ_CTX *pctx = vctx->op;
  const char *keyptr;
  char buf[IWNUMBUF_SIZE];
  if (key) {
    keyptr = key;
  } else if (lvl < 0) {
    return 0;
  } else {
    iwitoa(klidx, buf, IWNUMBUF_SIZE);
    keyptr = buf;
    klidx = (int) strlen(keyptr);
  }
  for (JQP_PROJECTION *p = pctx->proj; p; p = p->next) {
    uint8_t flags = p->flags;
    JBL jbl = 0;
    bool matched;
    if (flags & JQP_PROJECTION_FLAG_JOINS) {
      matched = _jql_proj_join_matched((int16_t) lvl, n, keyptr, klidx, vctx, p, &jbl, rc);
    } else {
      matched = _jql_proj_matched((int16_t) lvl, n, keyptr, klidx, vctx, p, rc);
    }
    RCRET(*rc);
    if (matched) {
      if (flags & JQP_PROJECTION_FLAG_EXCLUDE) {
        return JBN_VCMD_DELETE;
      } else if (flags & JQP_PROJECTION_FLAG_INCLUDE) {
        _jql_proj_mark_up(n, PROJ_MARK_KEEP);
      } else if ((flags & JQP_PROJECTION_FLAG_JOINS) && pctx->q->aux->has_keep_projections) {
        _jql_proj_mark_up(n, PROJ_MARK_KEEP | PROJ_MARK_FROM_JOIN);
      }
    }
  }
  return 0;
}

static jbn_visitor_cmd_t _jql_proj_keep_visitor(
  int lvl, JBL_NODE n, const char *key, int klidx, JBN_VCTX *vctx,
  iwrc *rc
  ) {
  if ((lvl < 0) || (n->flags & PROJ_MARK_PATH)) {
    return 0;
  }
  if (n->flags & PROJ_MARK_KEEP) {
    return (n->flags & PROJ_MARK_FROM_JOIN) ? JBL_VCMD_OK : JBL_VCMD_SKIP_NESTED;
  }
  return JBN_VCMD_DELETE;
}

static iwrc _jql_project(JBL_NODE root, JQL q, IWPOOL *pool, JBEXEC *exec_ctx) {
  iwrc rc;
  JQP_AUX *aux = q->aux;
  if (aux->has_exclude_all_projection) {
    jbn_data(root);
    return 0;
  }
  JQP_PROJECTION *proj = aux->projection;
  PROJ_CTX pctx = {
    .q        = q,
    .proj     = proj,
    .pool     = pool,
    .exec_ctx = exec_ctx,
  };
  if (!pool) {
    // No pool no exec_ctx
    pctx.exec_ctx = 0;
  }
  for (JQP_PROJECTION *p = proj; p; p = p->next) {
    p->pos = -1;
    p->cnt = 0;
    for (JQP_STRING *s = p->value; s; s = s->next) {
      if (s->flavour & JQP_STR_PLACEHOLDER) {
        if (s->opaque == 0 || ((JQVAL*) s->opaque)->type != JQVAL_STR) {
          return JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE;
        }
      }
      p->cnt++;
    }
  }
  JBN_VCTX vctx = {
    .root = root,
    .op   = &pctx
  };

  RCC(rc, finish, jbn_visit(root, 0, &vctx, _jql_proj_visitor));
  if (aux->has_keep_projections) { // We have keep projections
    RCC(rc, finish, jbn_visit(root, 0, &vctx, _jql_proj_keep_visitor));
  }

finish:
  return rc;
}

#undef PROJ_MARK_PATH
#undef PROJ_MARK_KEEP

//----------------------------------

iwrc jql_apply(JQL q, JBL_NODE root, IWPOOL *pool) {
  if (q->aux->apply_placeholder) {
    JQVAL *pv = _jql_find_placeholder(q, q->aux->apply_placeholder);
    if (!pv || (pv->type != JQVAL_JBLNODE) || !pv->vnode) {
      return JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE;
    }
    return jbn_patch_auto(root, pv->vnode, pool);
  } else if (q->aux->apply) {
    return jbn_patch_auto(root, q->aux->apply, pool);
  } else {
    return 0;
  }
}

iwrc jql_project(JQL q, JBL_NODE root, IWPOOL *pool, void *exec_ctx) {
  if (q->aux->projection) {
    return _jql_project(root, q, pool, exec_ctx);
  } else {
    return 0;
  }
}

iwrc jql_apply_and_project(JQL q, JBL jbl, JBL_NODE *out, void *exec_ctx, IWPOOL *pool) {
  *out = 0;
  JQP_AUX *aux = q->aux;
  if (!(aux->apply || aux->apply_placeholder || aux->projection)) {
    return 0;
  }
  JBL_NODE root;
  iwrc rc = jbl_to_node(jbl, &root, false, pool);
  RCRET(rc);
  if (aux->apply || aux->apply_placeholder) {
    rc = jql_apply(q, root, pool);
    RCRET(rc);
  }
  if (aux->projection) {
    rc = jql_project(q, root, pool, exec_ctx);
  }
  if (!rc) {
    *out = root;
  }
  return rc;
}

static const char* _ecodefn(locale_t locale, uint32_t ecode) {
  if (!((ecode > _JQL_ERROR_START) && (ecode < _JQL_ERROR_END))) {
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
    case JQL_ERROR_SKIP_ALREADY_SET:
      return "Skip clause already specified (JQL_ERROR_SKIP_ALREADY_SET)";
    case JQL_ERROR_LIMIT_ALREADY_SET:
      return "Limit clause already specified (JQL_ERROR_SKIP_ALREADY_SET)";
    case JQL_ERROR_ORDERBY_MAX_LIMIT:
      return "Reached max number of asc/desc order clauses: 64 (JQL_ERROR_ORDERBY_MAX_LIMIT)";
    case JQL_ERROR_NO_COLLECTION:
      return "No collection specified in query (JQL_ERROR_NO_COLLECTION)";
    case JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE:
      return "Invalid type of placeholder value (JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE)";
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
