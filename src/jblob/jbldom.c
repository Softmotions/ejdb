#include "jbldom.h"
#include "jblob/jbl_internal.h"

typedef enum {
  JBLNODE_NONE = 0,
  JBLNODE_ALLCATED_KEY = 1,
} jblnode_flags_t;

struct _JBLNODE {
  struct _JBLNODE *next;
  struct _JBLNODE *prev;
  struct _JBLNODE *child;
  const char *key;
  int klidx;
  int vsize;
  jbl_type_t type;
  jblnode_flags_t flags;
  union {
    char *vptr;
    bool vbool;
    int64_t vi64;
    double vf64;
  };
};

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

static iwrc _jbl_create_node(const binn *bv, JBLNODE parent, const char *key, int klidx, JBLNODE *node) {
  iwrc rc = 0;
  JBLNODE n = calloc(1, sizeof(*node));
  if (!n) {
    *node = 0;
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
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
    *node = 0;
  } else {
    *node = n;
  }
  return rc;
}

static iwrc _jbl_load_node(binn *bn, JBLNODE parent, char *key, int klidx, JBLNODE *node) {
  binn bv;
  binn_iter iter;
  int64_t llv;
  iwrc rc = 0;
  
  switch (bn->type) {
    case BINN_OBJECT:
    case BINN_MAP:
      rc = _jbl_create_node(bn, parent, key, klidx, &parent);
      RCRET(rc);
      if (!binn_iter_init(&iter, bn, bn->type)) {
        return JBL_ERROR_INVALID;
      }
      if (bn->type == BINN_OBJECT) {
        for (int i = 0; binn_object_next2(&iter, &key, &bv); ++i) {
          rc = _jbl_load_node(&bv, parent, key, -1, 0);
          RCRET(rc);
        }
      } else if (bn->type == BINN_MAP) {
        for (int i = 0; binn_map_next(&iter, &klidx, &bv); ++i) {
          rc = _jbl_load_node(&bv, parent, 0, klidx, 0);
          RCRET(rc);
        }
      }
      break;
    case BINN_LIST:
      rc = _jbl_create_node(bn, parent, key, klidx, &parent);
      RCRET(rc);
      for (int i = 0; binn_list_next(&iter, &bv); ++i) {
        rc = _jbl_load_node(&bv, parent, 0, i, 0);
        RCRET(rc);
      }
      break;
    default:
      rc = _jbl_create_node(bn, parent, key, klidx, 0);
      RCRET(rc);
      break;
  }
  return rc;
}

iwrc jbl_load_node(binn *bn, JBLNODE *node) {
  return _jbl_load_node(bn, 0, 0, -1, node);
}

//typedef enum {
//  JBP_ADD = 1,
//  JBP_REMOVE,
//  JBP_REPLACE,
//  JBP_COPY,
//  JBP_MOVE
//} jbp_patch_t;

iwrc jbl_patch(JBL jbl, const JBL_PATCH patch[], size_t num) {
  iwrc rc = 0;
  
  
  
  return rc;
}
