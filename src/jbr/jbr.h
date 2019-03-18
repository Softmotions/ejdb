#pragma once
#ifndef JBREST_H
#define JBREST_H

#include "ejdb2.h"

IW_EXTERN_C_START

typedef enum {
  _JBR_ERROR_START = (IW_ERROR_START + 15000UL + 3000),
  JBR_ERROR_HTTP_LISTEN,        /**< Failed to start HTTP network listener (JBR_ERROR_HTTP_LISTEN) */
  JBR_ERROR_SEND_RESPONSE,      /**< Error sending response (JBR_ERROR_SEND_RESPONSE) */
  JBR_ERROR_WS_UPGRADE,         /**< Failed upgrading to websocket connection (JBR_ERROR_WS_UPGRADE) */
  _JBR_ERROR_END,
} jbr_ecode;

struct _JBR;
typedef struct _JBR *JBR;

iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, JBR *pjbr);

iwrc jbr_shutdown(JBR *pjbr);

iwrc jbr_init(void);

IW_EXTERN_C_END
#endif
