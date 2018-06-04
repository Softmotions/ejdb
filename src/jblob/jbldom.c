#include "jbldom.h"
#include "jblob/jbl_internal.h"

struct _JBLNODE {
  int klidx;
  int vsize;
  jbl_type_t type;
  IWPOOL *pool;
  struct _JBLNODE *next;
  struct _JBLNODE *prev;
  struct _JBLNODE *child;
  const char *key;
  union {
    char *vptr;
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
  JBLNODE child = parent->child;
  if (child) {
    while (child->next) {
      child = child->next;
    }
    child->next = node;
    node->prev = child;
  } else {
    parent->child = node;
  }
}

static iwrc _jbl_create_node(JBLDRCTX *ctx,
                             const binn *bv,
                             JBLNODE parent,
                             const char *key,
                             int klidx,
                             JBLNODE *node) {
  iwrc rc = 0;
  JBLNODE n = iwpool_alloc(sizeof(*node), ctx->pool);
  if (!n) {
    *node = 0;
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
    *node = 0;
  } else {
    *node = n;
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
      for (int i = 0; binn_list_next(&iter, &bv); ++i) {
        rc = _jbl_node_from_binn(ctx, &bv, parent, 0, i);
        RCRET(rc);
      }
      break;
    default:
      rc = _jbl_create_node(ctx, bn, parent, key, klidx, 0);
      RCRET(rc);
      break;
  }
  return rc;
}

static iwrc _jbl_node_from_binn2(const binn *bn, JBLNODE *node) {
  IWPOOL *pool = iwpool_create(0);
  if (!pool) {
    *node = 0;
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBLDRCTX ctx = {
    .pool = pool
  };
  iwrc rc = _jbl_node_from_binn(&ctx, bn, 0, 0, -1);
  if (rc) {
    *node = 0;
    iwpool_destroy(ctx.pool);
  } else {
    *node = ctx.root;
    (*node)->pool = ctx.pool;
  }
  return rc;
}

static iwrc _jbl_node_from_patch(const JBLPATCH *p, JBLNODE *node, IWPOOL *pool) {
  iwrc rc = 0;
  JBLDRCTX ctx = {
    .pool = pool
  };
  *node = 0;
  if (p->type == JBV_OBJECT || p->type == JBV_ARRAY) {
    rc = _jbl_node_from_binn(&ctx, &p->vjbl->bn, 0, 0, -1);
    if (!rc) {
      *node = ctx.root;
    }
  } else {
    JBLNODE n = iwpool_alloc(sizeof(*node), ctx.pool);
    if (!n) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
    memset(n, 0, sizeof(*n));
    n->type = p->type;
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
      default:
        rc = JBL_ERROR_INVALID;
        break;
    }
  }
  return rc;
}

JBLNODE _jbl_node_find(const JBLNODE node, const JBLPTR ptr, int from, int to) {
  JBLNODE n = node;
  for (int i = from; n && i < ptr->cnt && i < to; ++i) {
    if (n->type == JBV_OBJECT) {
      for (n = node->child; n && !strncmp(n->key, ptr->n[i], n->klidx); n = n->next);
    } else if (n->type == JBV_ARRAY) {
      int64_t idx = iwatoi(ptr->n[i]);
      for (n = node->child; n && idx != n->klidx; n = n->next);
    } else {
      return 0;
    }
  }
  return 0;
}

IW_INLINE void _jbl_target_remove(JBLNODE target) {
  target->next = 0;
  target->prev = 0;
  target->child = 0;
}

IW_INLINE void _jbl_target_overwrite(JBLNODE target, JBLNODE src) {
  if (!target || !src) {
    return;
  }
  memmove(target, src, sizeof(*target));
}

static JBLNODE _jbl_target_detach_ptr(JBLNODE target, const JBLPTR path) {
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
  if (child->prev) {
    child->prev->next = child->next;
  }
  if (child->next) {
    child->next->prev = child->prev;
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
  if (ex->path->cnt == 1 && *ex->path->n[0] == '\0') { // Root operation
    if (op == JBP_REMOVE) {
      _jbl_target_remove(target);
    } else if (op == JBP_REPLACE || op == JBP_ADD) {
      _jbl_target_overwrite(target, ex->value);
    }
  } else { // Not a root
    if (op == JBP_REMOVE || op == JBP_REPLACE) {
      _jbl_target_detach_ptr(target, ex->path);
    }
    if (op == JBP_REMOVE) {
      goto finish;
    }
    
    
    
    // TODO:
  }
  
finish:
  return rc;
}

static binn *_jbl_from_node(JBLNODE node, iwrc *rcp) {
  binn *res = 0;
  switch (node->type) {
    case JBV_OBJECT:
      res = binn_object();
      if (!res) {
        *rcp = JBL_ERROR_CREATION;
        return 0;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        if (!binn_object_set_new2(res, n->key, n->klidx, _jbl_from_node(n, rcp))) {
          if (!*rcp) *rcp = JBL_ERROR_CREATION;
          binn_free(res);
          return 0;
        }
      }
      break;
    case JBV_ARRAY:
      res = binn_list();
      if (!res) {
        *rcp = JBL_ERROR_CREATION;
        return 0;
      }
      for (JBLNODE n = node->child; n; n = n->next) {
        if (!binn_list_add_new(res, _jbl_from_node(n, rcp))) {
          if (!*rcp) *rcp = JBL_ERROR_CREATION;
          binn_free(res);
          return 0;
        }
      }
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

iwrc jbl_patch(JBL jbl, const JBLPATCH *p, size_t cnt) {
  iwrc rc = 0;
  size_t i = 0;
  binn *bn;
  JBLNODE root;
  JBLPATCHEXT parr[cnt];
  memset(parr, 0, cnt * sizeof(JBLPATCHEXT));
  rc = _jbl_node_from_binn2(&jbl->bn, &root);
  RCGO(rc, finish);
  for (i = 0; i < cnt; ++i) {
    parr[i].p = &p[i];
    rc = _jbl_ptr_pool(p[i].path, &parr[i].path, root->pool);
    RCGO(rc, finish);
    rc = _jbl_ptr_pool(p[i].from, &parr[i].from, root->pool);
    RCGO(rc, finish);
    rc = _jbl_node_from_patch(parr[i].p, &parr[i].value, root->pool);
    RCGO(rc, finish);
  }
  for (i = 0; i < cnt; ++i) {
    rc = _jbl_target_apply_patch(root, &parr[i]);
    RCGO(rc, finish);
  }
  bn = (root->type == JBV_NONE) ? 0 : _jbl_from_node(root, &rc);
  RCGO(rc, finish);
  binn_free(&jbl->bn);
  if (bn) {
    binn_save_header(bn);
    memcpy(&jbl->bn, bn, sizeof(jbl->bn));
    jbl->bn.allocated = 0;
  } else {
    memset(&jbl->bn, 0, sizeof(jbl->bn));
    root->type = JBV_NONE;
  }
finish:
  if (root->pool) {
    iwpool_destroy(root->pool);
  }
  return rc;
}
