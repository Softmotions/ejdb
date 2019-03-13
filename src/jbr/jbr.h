#pragma once
#ifndef JBREST_H
#define JBREST_H

#include "ejdb2.h"

IW_EXTERN_C_START

typedef enum {
  _JBR_ERROR_START = (IW_ERROR_START + 15000UL + 3000),
  JBR_ERROR_HTTP_LISTEN_ERROR,  /**< Failed to start HTTP network listener (JBR_ERROR_HTTP_LISTEN_ERROR) */
  JBR_ERROR_RESPONSE_SENDING,   /**< Error sending response (JBR_ERROR_RESPONSE_SENDING) */
  _JBR_ERROR_END,
} jbr_ecode;

struct _JBR;
typedef struct _JBR *JBR;

iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, JBR *pjbr);

iwrc jbr_shutdown(JBR *pjbr);

iwrc jbr_init(void);

IW_EXTERN_C_END
#endif
