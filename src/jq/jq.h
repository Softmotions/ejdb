#pragma once
#ifndef JQ_H
#define JQ_H

#include <iowow/iwlog.h>

IW_EXTERN_C_START

struct _JQ;
typedef struct _JQ *JQ;

typedef enum {
  _JQ_ERROR_START = (IW_ERROR_START + 10000UL + 1000),

  _JQ_ERROR_END
} jq_ecode;


IW_EXPORT iwrc jq_init(void);

IW_EXTERN_C_END
#endif
