#pragma once
#ifndef JBREST_H
#define JBREST_H

/**************************************************************************************************
 * EJDB2
 *
 * MIT License
 *
 * Copyright (c) 2012-2021 Softmotions Ltd <info@softmotions.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *************************************************************************************************/

#include "ejdb2.h"

IW_EXTERN_C_START

typedef enum {
  _JBR_ERROR_START = (IW_ERROR_START + 15000UL + 3000),
  JBR_ERROR_HTTP_LISTEN,        /**< Failed to start HTTP network listener (JBR_ERROR_HTTP_LISTEN) */
  JBR_ERROR_PORT_INVALID,       /**< Invalid port specified (JBR_ERROR_PORT_INVALID) */
  JBR_ERROR_SEND_RESPONSE,      /**< Error sending response (JBR_ERROR_SEND_RESPONSE) */
  JBR_ERROR_WS_UPGRADE,         /**< Failed upgrading to websocket connection (JBR_ERROR_WS_UPGRADE) */
  JBR_ERROR_WS_INVALID_MESSAGE, /**< Invalid message recieved (JBR_ERROR_WS_INVALID_MESSAGE) */
  JBR_ERROR_WS_ACCESS_DENIED,   /**< Access denied (JBR_ERROR_WS_ACCESS_DENIED) */
  _JBR_ERROR_END,
} jbr_ecode_t;

struct _JBR;
typedef struct _JBR*JBR;

iwrc jbr_start(EJDB db, const EJDB_OPTS *opts, JBR *pjbr);

iwrc jbr_shutdown(JBR *pjbr);

iwrc jbr_init(void);

IW_EXTERN_C_END
#endif
