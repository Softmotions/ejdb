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
      n->vsize = bv->size - 1 /*null*/;
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

static iwrc _jbl_target_apply_patch(JBLNODE target, const JBLPATCHEXT *ex) {
  iwrc rc = 0;
  const JBLPATCH *p = ex->p;
  jbp_patch_t op = ex->p->op;
  JBLPTR path = ex->path;
  JBLNODE value = ex->value;
  
  // TODO: TEST
  
  if (ex->path->cnt == 1 && *ex->path->n[0] == '\0') { // Root operation
    if (op == JBP_REMOVE) {
      memset(target, 0, sizeof(*target));
    } else if (op == JBP_REPLACE || op == JBP_ADD) {
      if (!value) {
        rc = JBL_ERROR_PATCH_NOVALUE;
        goto finish;
      }
      memmove(target, value, sizeof(*value));
    }
  } else { // Not a root
    if (op == JBP_REMOVE || op == JBP_REPLACE) {
      _jbl_node_detach(target, ex->path);
    }
    if (op == JBP_REMOVE) {
      goto finish;
    } else if (op == JBP_MOVE || op == JBP_COPY) {
      if (op == JBP_MOVE) {
        value = _jbl_node_detach(target, ex->from);
      } else {
        value = _jbl_node_find2(target, ex->from);
      }
      if (!value) {
        rc = JBL_ERROR_PATH_NOTFOUND;
        goto finish;
      }
    } else { // ADD/REPLACE
      if (!value) {
        rc = JBL_ERROR_PATCH_NOVALUE;
        goto finish;
      }
    }
    int lastidx = path->cnt - 1;
    JBLNODE parent = (path->cnt > 1) ? _jbl_node_find(target, path, 0, lastidx) : target;
    if (!parent) {
      rc = JBL_ERROR_PATCH_TARGET_INVALID;
      goto finish;
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
          rc = JBL_ERROR_PATCH_INVALID_ARRAY_INDEX;
          goto finish;
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
        _jbl_add_item(parent, value);
      }
    } else {
      rc = JBL_ERROR_PATCH_TARGET_INVALID;
      goto finish;
    }
  }
  
finish:
  return rc;
}

static binn *_jbl_from_node(JBLNODE node, int size, iwrc *rcp) {
  binn *res = 0;
  switch (node->type) {
    case JBV_OBJECT:
      res = binn_new(BINN_OBJECT, size, 0);
      if (!res) {
        *rcp = JBL_ERROR_CREATION;
        return 0;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        if (!binn_object_set_new2(res, n->key, n->klidx, _jbl_from_node(n, 0, rcp))) {
          if (!*rcp) *rcp = JBL_ERROR_CREATION;
          binn_free(res);
          return 0;
        }
      }
      break;
    case JBV_ARRAY:
      res = binn_new(BINN_LIST, size, 0);
      if (!res) {
        *rcp = JBL_ERROR_CREATION;
        return 0;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        if (!binn_list_add_new(res, _jbl_from_node(n, 0, rcp))) {
          if (!*rcp) *rcp = JBL_ERROR_CREATION;
          binn_free(res);
          return 0;
        }
      }
      break;
    case JBV_STR:
      return binn_string(node->vptr, 0);
    case JBV_I64:
      return binn_int64(node->vi64);
    case JBV_F64:
      return binn_double(node->vf64);
    case JBV_BOOL:
      return binn_bool(node->vbool);
    case JBV_NULL:
      return binn_null();
    case JBV_NONE:
      *rcp = JBL_ERROR_CREATION;
      break;
  }
  return res;
}

static iwrc _jbl_patch(JBLNODE root, const JBLPATCH *p, size_t cnt, IWPOOL *pool) {
  if (cnt < 1) {
    return 0;
  }
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
    RCGO(rc, finish);
    if (p[i].from) {
      rc = _jbl_ptr_pool(p[i].from, &ext->from, pool);
      RCGO(rc, finish);
    }
    rc = _jbl_node_from_patch(ext->p, &ext->value, pool);
    RCGO(rc, finish);
  }
  for (i = 0; i < cnt; ++i) {
    rc = _jbl_target_apply_patch(root, &parr[i]);
    RCGO(rc, finish);
  }
  
finish:
  return rc;
}

// --------------------------- Public API

iwrc jbl_to_node(JBL jbl, JBLNODE *node, IWPOOL *pool) {
  return _jbl_node_from_binn2(&jbl->bn, node, pool);
}

iwrc jbl_patch(JBLNODE root, const JBLPATCH *p, size_t cnt) {
  iwrc rc;
  IWPOOL *pool = iwpool_create(0);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  rc = _jbl_patch(root, p, cnt, pool);
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_patch2(JBL jbl, const JBLPATCH *p, size_t cnt) {
  if (cnt < 1) {
    return 0;
  }
  if (!jbl || !p) {
    return IW_ERROR_INVALID_ARGS;
  }
  binn *bn;
  JBLNODE root;
  IWPOOL *pool = iwpool_create(0);
  if (!pool) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  iwrc rc = _jbl_node_from_binn2(&jbl->bn, &root, pool);
  RCGO(rc, finish);
  rc = _jbl_patch(root, p, cnt, pool);
  RCGO(rc, finish);
  bn = (root->type == JBV_NONE) ? 0 : _jbl_from_node(root, MAX(jbl->bn.size, 256), &rc);
  RCGO(rc, finish);
  binn_free(&jbl->bn);
  if (bn) {
    binn_save_header(bn);
    memcpy(&jbl->bn, bn, sizeof(jbl->bn));
    jbl->bn.allocated = 0;
    free(bn);
  } else {
    memset(&jbl->bn, 0, sizeof(jbl->bn));
    root->type = JBV_NONE;
  }
  
finish:
  iwpool_destroy(pool);
  return rc;
}

iwrc jbl_patch3(JBL jbl, const char *patchjson) {
  JBL jbp;
  int cnt = 0, i;
  JBLPATCH *p = 0;
  iwrc rc = 0;
  char *json = strdup(patchjson);
  if (!json) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
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
  p = calloc(cnt, sizeof(*p));
  if (!p) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    goto finish;
  }
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
      
        //        switch(nxj2->type) {
        //
        //        }
      }
    }
  }
  
finish:
  if (p) {
    free(p);
  }
  if (nxjson) {
    nx_json_free(nxjson);
  }
  if (json) {
    free(json);
  }
  jbl_destroy(&jbp);
  return rc;
}
