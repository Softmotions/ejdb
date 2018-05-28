#pragma once
#ifndef JBLOB_H
#define JBLOB_H

#include <iowow/iwlog.h>
#include <stdbool.h>

IW_EXTERN_C_START

struct _JBL;
typedef struct _JBL *JBL;

typedef enum {
  _JBL_ERROR_START = (IW_ERROR_START + 10000UL),
  JBL_ERROR_INVALID_BUFFER, /**< Invalid JBL buffer (JBL_ERROR_INVALID_BUFFER) */
  JBL_ERROR_CREATION,       /**< Cannot create JBL object (JBL_ERROR_CREATION) */
  JBL_ERROR_INVALID,        /**< Invalid JBL object (JBL_ERROR_INVALID) */
  JBL_ERROR_PARSE_JSON,     /**< Failed to parse JSON string (JBL_ERROR_PARSE_JSON) */
  _JBL_ERROR_END
} jbl_ecode;

typedef enum {
  JBV_NONE = 0,
  JBV_NULL,
  JBV_BOOL,
  JBV_I32,
  JBV_I64,
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
    int32_t vi32;
    int64_t vi64;
    double vf64;
  };
} JBL_PATCH;

IW_EXPORT iwrc jbl_create_object(JBL *jblp);

IW_EXPORT iwrc jbl_create_array(JBL *jblp);

IW_EXPORT iwrc jbl_from_buf_keep(JBL *jblp, void *buf, size_t bufsz);

IW_EXPORT iwrc jbl_from_json(JBL *jblp, const char *jsonstr);

IW_EXPORT void jbl_destroy(JBL *jblp);

IW_EXPORT iwrc jbl_apply(JBL jbl, const JBL_PATCH patch[], size_t num);

IW_EXPORT jbl_type_t jbl_type(JBL jbl);

IW_EXPORT size_t jbl_count(JBL jbl);

IW_EXPORT size_t jbl_size(JBL jbl);

IW_EXPORT iwrc jbl_get(JBL jbl, const char *path, JBL *res);

IW_EXPORT iwrc jbl_get_at(JBL jbl, const char *path, int pos, JBL *res);

IW_EXPORT int32_t jbl_get_i32(JBL jbl);

IW_EXPORT int64_t jbl_get_i64(JBL jbl);

IW_EXPORT double jbl_get_f64(JBL jbl);

IW_EXPORT char *jbl_get_str(JBL jbl);

IW_EXPORT size_t jbl_copy_strn(JBL jbl, char *buf, size_t bufsz);

IW_EXPORT iwrc jbl_as_buf(JBL jbl, void **buf, size_t *size);

typedef iwrc(*jbl_json_printer)(const char *data, size_t size, char ch, int count, void *op);

IW_EXPORT iwrc jbl_fstream_json_printer(const char *data, size_t size, char ch, int count, void *op);

IW_EXPORT iwrc jbl_xstr_json_printer(const char *data, size_t size, char ch, int count, void *op);

IW_EXPORT iwrc jbl_as_json(JBL jbl, jbl_json_printer pt, void *op, bool pretty);

IW_EXPORT iwrc jbl_init(void);

IW_EXTERN_C_END
#endif
