#pragma once
#ifndef JBREST_H
#define JBREST_H

#include "ejdb2.h"

IW_EXTERN_C_START

typedef enum {
  _JBR_ERROR_START = (IW_ERROR_START + 15000UL + 3000),
  JBR_ERROR_HTTP_PORT_IN_USE, /**< HTTP port already in use. */
  _JBR_ERROR_END,
} jbr_ecode;

struct _JBR;
typedef struct _JBR *JBR;

iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, JBR *jbr);

iwrc jbr_shutdown(JBR *jbr);

iwrc jbr_init(void);

IW_EXTERN_C_END
#endif
