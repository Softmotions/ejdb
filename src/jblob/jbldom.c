#include "jbldom.h"
#include "jblob/jbl_internal.h"

struct _JBLNODE {
  struct _JBLNODE *next;
  struct _JBLNODE *prev;
  struct _JBLNODE *child;
  const char *key;
  // Do not sort/add members after this point (offsetof usage below)
  int klidx;
  int vsize;
  jbl_type_t type;
  union {
    const char *vptr;
    bool vbool;
    int64_t vi64;
    double vf64;
  };
};

typedef struct _JBLPATCHEXT {
  const JBLPATCH *p;
  JBLPTR path;
  JBLPTR from;
  JBLNODE value;
} JBLPATCHEXT;

typedef struct _JBLDRCTX {
  IWPOOL *pool;
  JBLNODE root;
} JBLDRCTX;

static void _jbl_add_item(JBLNODE parent, JBLNODE node) {
  assert(parent && node);
  node->next = 0;
  if (parent->child) {
    JBLNODE prev = parent->child->prev;
    parent->child->prev = node;
    if (prev) {
      prev->next = node;
      node->prev = prev;
    } else {
      parent->child->next = node;
      node->prev = parent->child;
    }
  } else {
    parent->child = node;
  }
  if (parent->type == JBV_ARRAY) {
    if (node->prev) {
      node->klidx = node->prev->klidx + 1;
    } else {
      node->klidx = 0;
    }
  }
}

static iwrc _jbl_create_node(JBLDRCTX *ctx,
                             const binn *bv,
                             JBLNODE parent,
                             const char *key,
                             int klidx,
                             JBLNODE *node) {
  iwrc rc = 0;
  JBLNODE n = iwpool_alloc(sizeof(*n), ctx->pool);
  if (node) *node = 0;
  if (!n) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  memset(n, 0, sizeof(*n));
  n->key = key;
  n->klidx = klidx;
  switch (bv->type) {
    case BINN_NULL:
      n->type = JBV_NULL;
      break;
    case BINN_STRING:
      n->type = JBV_STR;
      n->vptr = bv->ptr;
      n->vsize = bv->size;
      break;
    case BINN_OBJECT:
    case BINN_MAP:
      n->type = JBV_OBJECT;
      n->vptr = bv->ptr;
      n->vsize = bv->size;
      break;
    case BINN_LIST:
      n->type = JBV_ARRAY;
      n->vptr = bv->ptr;
      n->vsize = bv->size;
      break;
    case BINN_TRUE:
      n->type = JBV_BOOL;
      n->vbool = true;
      break;
    case BINN_FALSE:
      n->type = JBV_BOOL;
      n->vbool = false;
      break;
    case BINN_BOOL:
      n->type = JBV_BOOL;
      n->vbool = bv->vbool;
      break;
    case BINN_UINT8:
      n->vi64 = bv->vuint8;
      n->type = JBV_I64;
      break;
    case BINN_UINT16:
      n->vi64 = bv->vuint16;
      n->type = JBV_I64;
      break;
    case BINN_UINT32:
      n->vi64 = bv->vuint32;
      n->type = JBV_I64;
      break;
    case BINN_UINT64:
      n->vi64 = bv->vuint64;
      n->type = JBV_I64;
      break;
    case BINN_INT8:
      n->vi64 = bv->vint8;
      n->type = JBV_I64;
      break;
    case BINN_INT16:
      n->vi64 = bv->vint16;
      n->type = JBV_I64;
      break;
    case BINN_INT32:
      n->vi64 = bv->vint32;
      n->type = JBV_I64;
      break;
    case BINN_INT64:
      n->vi64 = bv->vint64;
      n->type = JBV_I64;
      break;
    case BINN_FLOAT32:
    case BINN_FLOAT64:
      n->vf64 = bv->vdouble;
      n->type = JBV_F64;
      break;
    default:
      rc = JBL_ERROR_CREATION;
      goto finish;
  }
  if (parent) {
    _jbl_add_item(parent, n);
  }
  
finish:
  if (rc) {
    free(n);
  } else {
    if (node) *node = n;
  }
  return rc;
}

static iwrc _jbl_node_from_binn(JBLDRCTX *ctx, const binn *bn, JBLNODE parent, char *key, int klidx) {
  binn bv;
  binn_iter iter;
  int64_t llv;
  iwrc rc = 0;
  
  switch (bn->type) {
    case BINN_OBJECT:
    case BINN_MAP:
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, &parent);
      RCRET(rc);
      if (!ctx->root) {
        ctx->root = parent;
      }
      if (!binn_iter_init(&iter, (binn *) bn, bn->type)) {
        return JBL_ERROR_INVALID;
      }
      if (bn->type == BINN_OBJECT) {
        for (int i = 0; binn_object_next2(&iter, &key, &klidx, &bv); ++i) {
          rc = _jbl_node_from_binn(ctx, &bv, parent, key, klidx);
          RCRET(rc);
        }
      } else if (bn->type == BINN_MAP) {
        for (int i = 0; binn_map_next(&iter, &klidx, &bv); ++i) {
          rc = _jbl_node_from_binn(ctx, &bv, parent, 0, klidx);
          RCRET(rc);
        }
      }
      break;
    case BINN_LIST:
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, &parent);
      RCRET(rc);
      if (!ctx->root) {
        ctx->root = parent;
      }
      if (!binn_iter_init(&iter, (binn *) bn, bn->type)) {
        return JBL_ERROR_INVALID;
      }
      for (int i = 0; binn_list_next(&iter, &bv); ++i) {
        rc = _jbl_node_from_binn(ctx, &bv, parent, 0, i);
        RCRET(rc);
      }
      break;
    default: {
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, 0);
      RCRET(rc);
      break;
    }
  }
  return rc;
}

static iwrc _jbl_node_from_binn2(const binn *bn, JBLNODE *node, IWPOOL *pool) {
  JBLDRCTX ctx = {
    .pool = pool
  };
  iwrc rc = _jbl_node_from_binn(&ctx, bn, 0, 0, -1);
  if (rc) {
    *node = 0;
  } else {
    *node = ctx.root;
  }
  return rc;
}

static iwrc _jbl_node_from_patch(const JBLPATCH *p, JBLNODE *node, IWPOOL *pool) {
  iwrc rc = 0;
  *node = 0;
  JBL vjbl = 0;
  jbl_type_t type = p->type;
  JBLDRCTX ctx = {
    .pool = pool
  };
  
  if (p->vjson) { // JSON string specified as value
    rc = jbl_from_json(&vjbl, p->vjson);
    RCRET(rc);
    type = jbl_type(vjbl);
  }
  if (type == JBV_OBJECT || type == JBV_ARRAY) {
    rc = _jbl_node_from_binn(&ctx, vjbl ? &vjbl->bn : &p->vjbl->bn, 0, 0, -1);
    if (!rc) {
      *node = ctx.root;
    }
  } else {
    JBLNODE n = iwpool_alloc(sizeof(*n), ctx.pool);
    if (!n) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    memset(n, 0, sizeof(*n));
    n->type = type;
    switch (n->type) {
      case JBV_STR:
        n->vptr = p->vptr;
        n->vsize = p->vsize;
        break;
      case JBV_I64:
        n->vi64 = p->vi64;
        break;
      case JBV_F64:
        n->vf64 = p->vf64;
        break;
      case JBV_BOOL:
        n->vbool = p->vbool;
        break;
      case JBV_NONE:
      case JBV_NULL:
        break;
      default:
        rc = JBL_ERROR_INVALID;
        break;
    }
    if (!rc) {
      *node = n;
    }
  }
  return rc;
}

static JBLNODE _jbl_node_find(const JBLNODE node, const JBLPTR ptr, int from, int to) {
  if (!ptr || !node) return 0;
  JBLNODE n = node;
  
  for (int i = from; n && i < ptr->cnt && i < to; ++i) {
    if (n->type == JBV_OBJECT) {
      for (n = n->child; n; n = n->next) {
        if (!strncmp(n->key, ptr->n[i], n->klidx) && strlen(ptr->n[i]) == n->klidx) {
          break;
        }
      }
    } else if (n->type == JBV_ARRAY) {
      int64_t idx = iwatoi(ptr->n[i]);
      for (n = n->child; n; n = n->next) {
        if (idx == n->klidx) {
          break;
        }
      }
    } else {
      return 0;
    }
  }
  return n;
}

IW_INLINE JBLNODE _jbl_node_find2(const JBLNODE node, const JBLPTR ptr) {
  if (!node || !ptr || !ptr->cnt) return 0;
  return _jbl_node_find(node, ptr, 0, ptr->cnt - 1);
}

static JBLNODE _jbl_node_detach(JBLNODE target, const JBLPTR path) {
  if (!path) {
    return 0;
  }
  JBLNODE parent = (path->cnt > 1) ? _jbl_node_find(target, path, 0, path->cnt - 1) : target;
  if (!parent) {
    return 0;
  }
  JBLNODE child = _jbl_node_find(parent, path, path->cnt - 1, path->cnt);
  if (!child) {
    return 0;
  }
  if (parent->child == child) {
    parent->child = child->next;
  }
  if (child->next) {
    child->next->prev = child->prev;
  }
  if (child->prev && child->prev->next) {
    child->prev->next = child->next;
  }
  child->next = 0;
  child->prev = 0;
  child->child = 0;
  return child;
}

static int _jbl_cmp_node_keys(const void* o1, const void *o2) {
  JBLNODE n1 = *((JBLNODE*) o1);
  JBLNODE n2 = *((JBLNODE*) o2);  
  if (!n1 && !n2) {
    return 0;
  } if (!n2 || n1->klidx > n2->klidx) {
    return 1;
  } else if (!n1 || n1->klidx < n2->klidx) {
    return -1;
  }
  return strncmp(n1->key, n2->key, n1->klidx);  
}

static int _jbl_node_count(JBLNODE n) {
  int ret = 0;
  n = n->child;
  while (n) {
    ret++;
    n = n->next;
  }
  return ret;
}

static int _jbl_compare_nodes(JBLNODE n1, JBLNODE n2, iwrc *rcp);

static int _jbl_compare_objects(JBLNODE n1, JBLNODE n2, iwrc *rcp) {
  int ret = 0;
  int cnt = _jbl_node_count(n1);
  int i = _jbl_node_count(n2);
  if (cnt > i) {
    return 1;
  } else if (cnt < i) {
    return -1;
  } else if (cnt == 0) {
    return 0;
  }
  
  JBLNODE *s1 = malloc(2 * sizeof(JBLNODE) * cnt);
  if (!s1) {
    *rcp = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    return 0;
  }
  JBLNODE *s2 = s1 + cnt;
  
  i = 0;
  n1 = n1->child;
  n2 = n2->child;
  while (n1 && n2) {
    s1[i] = n1;
    s2[i] = n2;    
    n1 = n1->next;
    n2 = n2->next;
    ++i;
  }    
  qsort(s1, cnt, sizeof(JBLNODE), _jbl_cmp_node_keys);
  qsort(s2, cnt, sizeof(JBLNODE), _jbl_cmp_node_keys);  
  for (i = 0; i < cnt; ++i) {  
    ret = _jbl_cmp_node_keys(s1 + i, s2 + i);
    if (ret) {
      goto finish;
    }  
    ret = _jbl_compare_nodes(s1[i], s2[i], rcp);
    if (*rcp || ret) {
      goto finish;
    }      
  }    
  
finish:  
  free(s1);
  return ret;
}

static int _jbl_compare_nodes(JBLNODE n1, JBLNODE n2, iwrc *rcp) {
  if (!n1 && !n2) {
    return 0;
  } else if (!n1) {
    return -1;
  } else if (!n2) {
    return 1;
  } else if (n1->type - n2->type) {
    return n1->type - n2->type;
  }  
  switch (n1->type) {
    case JBV_BOOL:
      return n1->vbool - n2->vbool;
    case JBV_I64:
      return n1->vi64 - n2->vi64;
    case JBV_F64:
      return n1->vf64 - n2->vf64;
    case JBV_STR:
      if (n1->vsize - n2->vsize) {
        return n1->vsize - n2->vsize;
      }
      return strncmp(n1->vptr, n2->vptr, n1->vsize);
    case JBV_ARRAY:
      for (n1 = n1->child, n2 = n2->child; n1 && n2; n1 = n1->next, n2 = n2->next) {
        int res = _jbl_compare_nodes(n1, n2, rcp);
        if (res) {
          return res;
        }
      }
      if (n1) {
        return 1;
      } else if (n2) {
        return -1;
      } else {
        return 0;
      }
    case JBV_OBJECT:
      return _jbl_compare_objects(n1, n2, rcp);
    case JBV_NULL:
    case JBV_NONE:
      break;
  }
  return 0;
}

static iwrc _jbl_target_apply_patch(JBLNODE target, const JBLPATCHEXT *ex) {
    
  const JBLPATCH *p = ex->p;
  jbp_patch_t op = ex->p->op;
  JBLPTR path = ex->path;
  JBLNODE value = ex->value;
  bool oproot = ex->path->cnt == 1 && *ex->path->n[0] == '\0';
  
  if (op == JBP_TEST) {
    iwrc rc = 0;
    if (!value) {
      return JBL_ERROR_PATCH_NOVALUE;
    }
    if (_jbl_compare_nodes(oproot ? target : _jbl_node_find(target, path, 0, path->cnt), value, &rc)) {
      RCRET(rc);
      return JBL_ERROR_PATCH_TEST_FAILED;
    } else {
      return rc;
    }
  }
  if (oproot) { // Root operation
    if (op == JBP_REMOVE) {
      memset(target, 0, sizeof(*target));
    } else if (op == JBP_REPLACE || op == JBP_ADD) {
      if (!value) {
        return JBL_ERROR_PATCH_NOVALUE;
      }
      memmove(target, value, sizeof(*value));
    }
  } else { // Not a root
    if (op == JBP_REMOVE || op == JBP_REPLACE) {
      _jbl_node_detach(target, ex->path);
    }
    if (op == JBP_REMOVE) {
      return 0;
    } else if (op == JBP_MOVE || op == JBP_COPY) {
      if (op == JBP_MOVE) {
        value = _jbl_node_detach(target, ex->from);
      } else {
        value = _jbl_node_find2(target, ex->from);
      }
      if (!value) {
        return JBL_ERROR_PATH_NOTFOUND;
      }
    } else { // ADD/REPLACE
      if (!value) {
        return JBL_ERROR_PATCH_NOVALUE;
      }
    }
    int lastidx = path->cnt - 1;
    JBLNODE parent = (path->cnt > 1) ? _jbl_node_find(target, path, 0, lastidx) : target;
    if (!parent) {
      return JBL_ERROR_PATCH_TARGET_INVALID;
    }
    if (parent->type == JBV_ARRAY) {
      if (path->n[lastidx][0] == '-' && path->n[lastidx][1] == '\0') {
        _jbl_add_item(parent, value); // Add to end of array
      } else { // Insert into the specified index
        int idx = iwatoi(path->n[lastidx]);
        int cnt = idx;
        JBLNODE child = parent->child;
        while (child && cnt > 0) {
          cnt--;
          child = child->next;
        }
        if (cnt > 0) {
          return JBL_ERROR_PATCH_INVALID_ARRAY_INDEX;
        }
        value->klidx = idx;
        if (child) {
          value->next = child;
          value->prev = child->prev;
          child->prev = value;
          if (child == parent->child) {
            parent->child = value;
          } else {
            value->prev->next = value;
          }
          while (child) {
            child->klidx++;
            child = child->next;
          }
        } else {
          _jbl_add_item(parent, value);
        }
      }
    } else if (parent->type == JBV_OBJECT) {
      JBLNODE child = _jbl_node_find(parent, path, path->cnt - 1, path->cnt);
      if (child) {
        memcpy(child,
               ((uint8_t *) value) + offsetof(struct _JBLNODE, klidx),
               sizeof(struct _JBLNODE) - offsetof(struct _JBLNODE, klidx));
      } else {
        value->key = path->n[path->cnt - 1];
        value->klidx = strlen(value->key);
        _jbl_add_item(parent, value);
      }
    } else {
      return JBL_ERROR_PATCH_TARGET_INVALID;
    }
  }
  return 0;
}

static iwrc _jbl_from_node(binn *res, JBLNODE node, int size) {
  iwrc rc = 0;
  switch (node->type) {
    case JBV_OBJECT:
      if (!binn_create(res, BINN_OBJECT, 0, NULL)) {
        return JBL_ERROR_CREATION;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        binn bv;
        rc = _jbl_from_node(&bv, n, 0);
        RCRET(rc);
        if (!binn_object_set_value2(res, n->key, n->klidx, &bv)) {
          rc = JBL_ERROR_CREATION;
        }
        binn_free(&bv);
        RCRET(rc);
      }
      break;
    case JBV_ARRAY:
      if (!binn_create(res, BINN_LIST, 0, NULL)) {
        return JBL_ERROR_CREATION;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        binn bv;
        rc = _jbl_from_node(&bv, n, 0);
        RCRET(rc);
        if (!binn_list_add_value(res, &bv)) {
          rc = JBL_ERROR_CREATION;
        }
        binn_free(&bv);
        RCRET(rc);
      }
      break;
    case JBV_STR:
      binn_init_item(res);
      binn_set_string(res, (void *)node->vptr, 0);
      break;
    case JBV_I64:
      binn_init_item(res);
      binn_set_int64(res, node->vi64);
      break;
    case JBV_F64:
      binn_init_item(res);
      binn_set_double(res, node->vf64);
      break;
    case JBV_BOOL:
      binn_set_bool(res, node->vbool);
      break;
    case JBV_NULL:
      binn_init_item(res);
      binn_set_null(res);
      break;
    case JBV_NONE:
      rc = JBL_ERROR_CREATION;
      break;
  }
  return rc;
}

static iwrc _jbl_patch(JBLNODE root, const JBLPATCH *p, size_t cnt, IWPOOL *pool) {
  if (cnt < 1) return 0;
  if (!root || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  iwrc rc = 0;
  size_t i = 0;
  JBLPATCHEXT parr[cnt];
  memset(parr, 0, cnt * sizeof(JBLPATCHEXT));
  for (i = 0; i < cnt; ++i) {
    JBLPATCHEXT *ext = &parr[i];
    ext->p = &p[i];
    rc = _jbl_ptr_pool(p[i].path, &ext->path, pool);
    RCRET(rc);
    if (p[i].from) {
      rc = _jbl_ptr_pool(p[i].from, &ext->from, pool);
      RCRET(rc);
    }
    rc = _jbl_node_from_patch(ext->p, &ext->value, pool);
    RCRET(rc);
  }
  for (i = 0; i < cnt; ++i) {
    rc = _jbl_target_apply_patch(root, &parr[i]);
    RCRET(rc);
  }
  return rc;
}

static iwrc _jbl_patch2(JBL jbl, const JBLPATCH *p, size_t cnt, IWPOOL *pool) {
  if (cnt < 1) return 0;
  if (!jbl || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  binn bv;
  binn *bn;
  JBLNODE root;
  iwrc rc = _jbl_node_from_binn2(&jbl->bn, &root, pool);
  RCRET(rc);
  rc = _jbl_patch(root, p, cnt, pool);
  RCRET(rc);
  if (root->type != JBV_NONE) {
    rc = _jbl_from_node(&bv, root, MAX(jbl->bn.size, 256));
    RCRET(rc);
    bn = &bv;
  } else {
    bn = 0;
  }
  binn_free(&jbl->bn);
  if (bn) {
    if (bn->writable && bn->dirty) {
      binn_save_header(bn);
    }
    memcpy(&jbl->bn, bn, sizeof(jbl->bn));
    jbl->bn.allocated = 0;
  } else {
    memset(&jbl->bn, 0, sizeof(jbl->bn));
    root->type = JBV_NONE;
  }
  return rc;
}

// --------------------------- Public API

void jbl_patch_destroy(JBLPATCH *patch, size_t cnt) {
  for (size_t i = 0; i < cnt; ++i) {
    JBLPATCH *p = patch + i;
    if (p->type == JBV_ARRAY || p->type == JBV_OBJECT) {
      binn_free(&p->vjbl->bn);
    }
  }
}

iwrc jbl_to_node(JBL jbl, JBLNODE *node, IWPOOL *pool) {
  return _jbl_node_from_binn2(&jbl->bn, node, pool);
}

iwrc jbl_patch(JBLNODE root, const JBLPATCH *p, size_t cnt) {
  IWPOOL *pool = iwpool_create(0);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_patch(root, p, cnt, pool);
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_patch2(JBL jbl, const JBLPATCH *p, size_t cnt) {
  if (cnt < 1) return 0;
  if (!jbl || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  IWPOOL *pool = iwpool_create(0);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_patch2(jbl, p, cnt, pool);
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_patch3(JBL jbl, const char *patchjson) {
  binn bv;
  JBLPATCH *p;
  int cnt = 0;
  iwrc rc = 0;
  IWPOOL *pool = iwpool_create(0);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  
  int i = strlen(patchjson);
  char *json = iwpool_alloc(i + 1, pool);
  if (!json) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  memcpy(json, patchjson, i + 1);
  
  nx_json *nxjson = nx_json_parse_utf8(json);
  if (!nxjson) {
    rc = JBL_ERROR_PARSE_JSON;
    goto finish;
  }
  if (nxjson->type != NX_JSON_ARRAY) {
    rc = JBL_ERROR_PATCH_INVALID;
    goto finish;
  }
  for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next) {
    if (nxj->type != NX_JSON_OBJECT) {
      rc = JBL_ERROR_PATCH_INVALID;
      goto finish;
    }
    cnt++;
  }
  
  p = iwpool_alloc(cnt * sizeof(*p), pool);
  if (!p) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
  memset(p, 0, cnt * sizeof(*p));
  
  i = 0;
  for (nx_json *nxj = nxjson->child; nxj; nxj = nxj->next, ++i) {
    JBLPATCH *pp = p + i;
    for (nx_json *nxj2 = nxj->child; nxj2; nxj2 = nxj2->next) {
      if (!strcmp("op", nxj2->key)) {
        if (nxj2->type != NX_JSON_STRING) {
          rc = JBL_ERROR_PATCH_INVALID;
          goto finish;
        }
        const char *v = nxj2->text_value;
        if (!strcmp("add", v)) {
          pp->op = JBP_ADD;
        } else if (!strcmp("remove", v)) {
          pp->op = JBP_REMOVE;
        } else if (!strcmp("replace", v)) {
          pp->op = JBP_REPLACE;
        } else if (!strcmp("copy", v)) {
          pp->op = JBP_COPY;
        } else if (!strcmp("move", v)) {
          pp->op = JBP_MOVE;
        } else if (!strcmp("test", v)) {
          pp->op = JBP_TEST;
        } else {
          rc = JBL_ERROR_PATCH_INVALID_OP;
          goto finish;
        }
      } else if (!strcmp("value", nxj2->key)) {
        switch (nxj2->type) {
          case NX_JSON_STRING:
            pp->type = JBV_STR;
            pp->vptr = nxj2->text_value;
            pp->vsize = strlen(nxj2->text_value);
            break;
          case NX_JSON_INTEGER:
            pp->type = JBV_I64;
            pp->vi64 = nxj2->int_value;
            break;
          case NX_JSON_OBJECT:
          case NX_JSON_ARRAY: {
            pp->type = (nxj2->type == NX_JSON_ARRAY) ? JBV_ARRAY : JBV_OBJECT;
            pp->vjbl = iwpool_alloc(sizeof(*pp->vjbl), pool);
            if (!pp->vjbl) {
              rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
              RCGO(rc, finish);
            }
            rc = _jbl_from_json(&bv, nxj2);
            RCGO(rc, finish);
            if (bv.writable && bv.dirty) {
              binn_save_header(&bv);
            }
            memcpy(&pp->vjbl->bn, &bv, sizeof(bv));
            pp->vjbl->bn.allocated = 0;
            break;
          }
          case NX_JSON_BOOL:
            pp->type = JBV_BOOL;
            pp->vbool = nxj2->int_value;
            break;
          case NX_JSON_NULL:
            pp->type = JBV_NULL;
            break;
          case NX_JSON_DOUBLE:
            pp->type = JBV_F64;
            pp->vf64 = nxj2->dbl_value;
            break;
          default:
            rc = JBL_ERROR_PARSE_JSON;
            break;
        }
      } else if (!strcmp("path", nxj2->key)) {
        if (nxj2->type != NX_JSON_STRING) {
          rc = JBL_ERROR_PATCH_INVALID;
          goto finish;
        }
        pp->path = nxj2->text_value;
      } else if (!strcmp("from", nxj2->key)) {
        if (nxj2->type != NX_JSON_STRING) {
          rc = JBL_ERROR_PATCH_INVALID;
          goto finish;
        }
        pp->from = nxj2->text_value;
      }
    }
  }
  
  rc = _jbl_patch2(jbl, p, cnt, pool);
  
finish:
  jbl_patch_destroy(p, cnt);
  if (nxjson) {
    nx_json_free(nxjson);
  }
  iwpool_destroy(pool);
  return rc;
}
