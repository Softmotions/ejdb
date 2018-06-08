#ifndef JBLDOM_H
#define JBLDOM_H

#include "jbl.h"
#include <iowow/iwpool.h>

typedef struct _JBLNODE {
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
} *JBLNODE;

typedef enum {
  JBP_ADD = 1,
  JBP_REMOVE,
  JBP_REPLACE,
  JBP_COPY,
  JBP_MOVE,
  JBP_TEST
} jbp_patch_t;

typedef struct _JBLPATCH {
  jbp_patch_t op;
  jbl_type_t type;
  const char *path;
  const char *from;
  const char *vjson;
  int vsize;
  union {
    JBL vjbl;
    bool vbool;
    int64_t vi64;
    double vf64;
    const char *vptr;
  };
} JBLPATCH;

IW_EXPORT iwrc jbl_to_node(JBL jbl, JBLNODE *node, IWPOOL *pool);

IW_EXPORT iwrc jbl_to_node2(const char *json, JBLNODE *node, IWPOOL *pool);

IW_EXPORT iwrc jbl_from_node(JBL jbl, JBLNODE node);

IW_EXPORT iwrc jbl_patch(JBLNODE root, const JBLPATCH *patch, size_t cnt);

IW_EXPORT iwrc jbl_patch2(JBL jbl, const JBLPATCH *patch, size_t cnt);

IW_EXPORT iwrc jbl_patch3(JBL jbl, const char *patchjson);

IW_EXPORT void jbl_patch_destroy(JBLPATCH *patch, size_t cnt);

IW_EXPORT iwrc jbl_merge_patch(JBLNODE root, const char *patchjson);

IW_EXPORT iwrc jbl_merge_patch2(JBL jbl, const char *patchjson);


#endif
