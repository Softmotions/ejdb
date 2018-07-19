#pragma once
#ifndef JQL_H
#define JQL_H

#include "jbl.h"
#include <iowow/iwlog.h>

IW_EXTERN_C_START

struct _JQL;
typedef struct _JQL *JQL;

typedef enum {
  _JQL_ERROR_START = (IW_ERROR_START + 10000UL + 1000),
  JQL_ERROR_QUERY_PARSE,          /**< Query parsing error (JQL_ERROR_QUERY_PARSE) */
  JQL_ERROR_INVALID_PLACEHOLDER,  /**< Invalid placeholder position (JQL_ERROR_INVALID_PLACEHOLDER) */  
  JQL_ERROR_UNSET_PLACEHOLDER,    /**< Found unset placeholder (JQL_ERROR_UNSET_PLACEHOLDER) */
  _JQL_ERROR_END
} jq_ecode;

/**
 * @brief Create query object from specified text query.
 * @param pq Pointer to resulting query object
 * @param query Query text
 */
IW_EXPORT iwrc jql_create(JQL *qptr, const char *query);

IW_EXPORT iwrc jql_set_json(JQL q, const char *placeholder, int index, JBL_NODE val);

IW_EXPORT iwrc jql_set_i64(JQL q, const char *placeholder, int index, int64_t val);

IW_EXPORT iwrc jql_set_f64(JQL q, const char *placeholder, int index, double val);

IW_EXPORT iwrc jql_set_str(JQL q, const char *placeholder, int index, const char *val);

IW_EXPORT iwrc jql_set_bool(JQL q, const char *placeholder, int index, bool val);

IW_EXPORT iwrc jql_set_regexp(JQL q, const char *placeholder, int index, const char* expr);

IW_EXPORT iwrc jql_set_null(JQL q, const char *placeholder, int index);

IW_EXPORT iwrc jql_matched(JQL q, const JBL jbl, bool *out);

IW_EXPORT iwrc jql_apply(JQL q, const JBL jbl, JBL_NODE *out);

IW_EXPORT iwrc jql_apply_projection(JQL q, const JBL jbl, JBL_NODE *out);

IW_EXPORT void jql_reset(JQL q, bool reset_placeholders);

IW_EXPORT void jql_destroy(JQL *qptr);

IW_EXPORT iwrc jql_init(void);

IW_EXTERN_C_END
#endif
