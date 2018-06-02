#include "jbldom.h"
#include "jblob/jbl_internal.h"

struct _JBLNODE {
  struct _JBLNODE *next;
  struct _JBLNODE *prev;
  struct _JBLNODE *child;
  jbl_type_t type;
  char *key;
  int size;
  union {
    char *vstring;
    bool vbool;
    int32_t vi32;
    int64_t vi64;
    double vf64;
  };
};

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
