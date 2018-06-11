#pragma once
#ifndef JQL_H
#define JQL_H

#include <iowow/iwlog.h>

IW_EXTERN_C_START

struct _JQL;
typedef struct _JQL *JQL;

typedef enum {
  _JQL_ERROR_START = (IW_ERROR_START + 10000UL + 1000),
  JQL_ERROR_QUERY_PARSE,        /**< Query parsing error (JQL_ERROR_QUERY_PARSE) */
  _JQL_ERROR_END
} jq_ecode;


IW_EXPORT iwrc jql_init(void);

IW_EXTERN_C_END
#endif
