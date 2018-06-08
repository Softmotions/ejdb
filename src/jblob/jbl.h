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
  JBL_ERROR_INVALID_BUFFER,             /**< Invalid JBL buffer (JBL_ERROR_INVALID_BUFFER) */
  JBL_ERROR_CREATION,                   /**< Cannot create JBL object (JBL_ERROR_CREATION) */
  JBL_ERROR_INVALID,                    /**< Invalid JBL object (JBL_ERROR_INVALID) */
  JBL_ERROR_PARSE_JSON,                 /**< Failed to parse JSON string (JBL_ERROR_PARSE_JSON) */
  JBL_ERROR_JSON_POINTER,               /**< Invalid JSON pointer (rfc6901) path (JBL_ERROR_JSON_POINTER) */
  JBL_ERROR_PATH_NOTFOUND,              /**< JSON object not matched the path specified (JBL_ERROR_PATH_NOTFOUND) */
  JBL_ERROR_PATCH_INVALID,              /**< Invalid JSON patch specified (JBL_ERROR_PATCH_INVALID) */
  JBL_ERROR_PATCH_INVALID_OP,           /**< Invalid JSON patch operation specified (JBL_ERROR_PATCH_INVALID_OP) */
  JBL_ERROR_PATCH_NOVALUE,              /**< No value specified in JSON patch (JBL_ERROR_PATCH_NOVALUE) */
  JBL_ERROR_PATCH_TARGET_INVALID,       /**< Could not find target object to set value (JBL_ERROR_PATCH_TARGET_INVALID) */  
  JBL_ERROR_PATCH_INVALID_ARRAY_INDEX,  /**< Invalid array index in JSON patch path (JBL_ERROR_PATCH_INVALID_ARRAY_INDEX) */
  JBL_ERROR_PATCH_TEST_FAILED,          /**< JSON patch test operation failed (JBL_ERROR_PATCH_TEST_FAILED) */
  _JBL_ERROR_END
} jbl_ecode;

typedef enum {
  JBV_NONE = 0,
  JBV_NULL,
  JBV_BOOL,
  JBV_I64,
  JBV_F64,
  JBV_STR,
  JBV_OBJECT,
  JBV_ARRAY,
} jbl_type_t;

typedef iwrc(*jbl_json_printer)(const char *data, size_t size, char ch, int count, void *op);

IW_EXPORT iwrc jbl_from_buf_keep(JBL *jblp, void *buf, size_t bufsz);

IW_EXPORT iwrc jbl_from_json(JBL *jblp, const char *jsonstr);

IW_EXPORT jbl_type_t jbl_type(JBL jbl);

IW_EXPORT size_t jbl_count(JBL jbl);

IW_EXPORT size_t jbl_size(JBL jbl);

IW_EXPORT int32_t jbl_get_i32(JBL jbl);

IW_EXPORT int64_t jbl_get_i64(JBL jbl);

IW_EXPORT double jbl_get_f64(JBL jbl);

IW_EXPORT char *jbl_get_str(JBL jbl);

IW_EXPORT size_t jbl_copy_strn(JBL jbl, char *buf, size_t bufsz);

IW_EXPORT iwrc jbl_at(JBL jbl, const char *path, JBL *res);

IW_EXPORT iwrc jbl_as_buf(JBL jbl, void **buf, size_t *size);

IW_EXPORT iwrc jbl_as_json(JBL jbl, jbl_json_printer pt, void *op, bool pretty);

IW_EXPORT iwrc jbl_fstream_json_printer(const char *data, size_t size, char ch, int count, void *op);

IW_EXPORT iwrc jbl_xstr_json_printer(const char *data, size_t size, char ch, int count, void *op);

IW_EXPORT void jbl_destroy(JBL *jblp);

IW_EXPORT iwrc jbl_init(void);

IW_EXTERN_C_END
#endif
