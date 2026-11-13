#pragma once
#ifndef JBREST_H
#define JBREST_H

/**************************************************************************************************
 * EJDB2
 *
 * MIT License
 *
 * Copyright (c) 2012-2022 Softmotions Ltd <info@softmotions.com>
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

IW_EXTERN_C_START;

struct jbr;
typedef struct jbr*JBR;

/**
 * @brief Starts the HTTP/WebSocket endpoint for an opened database.
 *
 * Normally the endpoint is started by `ejdb_open()` when `ejdb_opts.http.enabled`
 * is set. This function is intended for applications that manage the server
 * lifecycle themselves.
 *
 * @param db       Database handle. Not zero.
 * @param opts     Open options; `opts->http` describes the server. Not zero.
 * @param [out] out_jbr Placeholder for the server handle. Not zero.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
iwrc jbr_start(struct ejdb *db, const struct ejdb_opts *opts, struct jbr **out_jbr);

/**
 * @brief Requests the HTTP/WebSocket endpoint shutdown.
 *
 * Does not block. Safe to call from a signal handler or from another thread.
 * The shutdown is completed and the server handle is released by
 * `jbr_shutdown_wait()` or by `ejdb_close()`.
 *
 * @param db Database handle. Not zero.
 */
void jbr_shutdown_request(struct ejdb *db);

/**
 * @brief Waits for the HTTP/WebSocket endpoint to stop and releases its handle.
 *
 * @param jbr Server handle returned by `jbr_start()`. Can be zero.
 */
void jbr_shutdown_wait(struct jbr *jbr);

/**
 * @brief Registers HTTP/WebSocket API error codes.
 *
 * Called by `ejdb_init()`; no need to call it directly.
 *
 * @return `0` on success.
 *          Any non zero error codes.
 */
iwrc jbr_init(void);

IW_EXTERN_C_END;
#endif
