#pragma once
#ifndef JBLOB_H
#define JBLOB_H

#include <iowow/iwlog.h>
#include <iowow/iwxstr.h>
#include <stdbool.h>

IW_EXTERN_C_START

struct _JBL;
typedef struct _JBL *JBL;

typedef enum {
  _JBL_ERROR_START = (IW_ERROR_START + 10000UL),
  _JBL_ERROR_END
} jbl_ecode;

typedef enum {
  JBV_NONE = 0,
  JBV_BOOL,
  JBV_U32,
  JBV_U64,
  JBV_F64,
  JBV_STR,
  JBV_OBJECT,
  JBV_ARRAY,
} jbl_type_t;

typedef enum {
  JBP_ADD       = 1,
  JBP_REMOVE,
  JBP_REPLACE,
  JBP_COPY
} jbp_patch_t;

typedef struct JBL_PATCH {
  jbp_patch_t op;
  jbl_type_t type;
  const char *path;
  const char *from;
  union {
    JBL vjbl;
    bool vbool;
    uint32_t vu32;
    uint64_t vu64;
    double vf64;
  };
} JBL_PATCH;

IW_EXPORT iwrc jbl_create_object(JBL *jbl);

iwrc jbl_create_array(JBL *jbl);

iwrc jbl_from_buf(JBL *jbl, const void *buf, size_t bufsz);

iwrc jbl_from_json(JBL *jbl, const char *json);

void jbl_destroy(JBL *jbl);

iwrc jbl_apply(JBL jbl, const JBL_PATCH patch[], size_t num);

jbl_type_t jbl_type(JBL jbl);

size_t jbl_count(JBL jbl);

size_t jbl_length(JBL jbl);

iwrc jbl_get(JBL jbl, const char *path, JBL *res);

iwrc jbl_get_at(JBL jbl, const char *path, int pos, JBL *res);

iwrc jbl_get_u32(JBL jbl, uint32_t *res);

iwrc jbl_get_u64(JBL jbl, uint64_t *res);

iwrc jbl_get_f64(JBL jbl, double *res);

iwrc jbl_get_str(JBL jbl, char **ptr);

iwrc jbl_get_strn(JBL jbl, char *buf, size_t bufsz);

iwrc jbl_as_buf(JBL jbl, void **buf, size_t *size);

iwrc jbl_as_json(JBL jbl, IWXSTR *xstr, bool pretty);

iwrc jbl_init(void);

IW_EXTERN_C_END
#endif
