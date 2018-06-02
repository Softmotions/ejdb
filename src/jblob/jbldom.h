#ifndef JBLDOM_H
#define JBLDOM_H

#include "jbl.h"

struct _JBLNODE;
typedef struct _JBLNODE *JBLNODE;

typedef enum {  
  JBP_ADD = 1,
  JBP_REMOVE,
  JBP_REPLACE,
  JBP_COPY,
  JBP_MOVE  
} jbp_patch_t;

typedef struct JBL_PATCH {
  jbp_patch_t op;
  jbl_type_t type;
  const char *path;
  const char *from;
  union {
    JBL vjbl;
    bool vbool;
    int32_t vi32;
    int64_t vi64;
    double vf64;
  };
} JBL_PATCH;


IW_EXPORT iwrc jbl_patch(JBL jbl, const JBL_PATCH patch[], size_t num);

#endif
