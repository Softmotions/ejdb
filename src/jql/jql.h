#pragma once
#ifndef JQL_H
#define JQL_H

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

/** @file
 *
 * @brief EJDB query construction API.
 *
 * EJDB query syntax inpired by ideas behind XPath and Unix shell pipes.
 *
 *
 */

#include "jbl.h"
#include <ejdb2/iowow/iwlog.h>

IW_EXTERN_C_START

struct _JQL;
typedef struct _JQL*JQL;

typedef enum {
  _JQL_ERROR_START = (IW_ERROR_START + 15000UL + 2000),
  JQL_ERROR_QUERY_PARSE,                    /**< Query parsing error (JQL_ERROR_QUERY_PARSE) */
  JQL_ERROR_INVALID_PLACEHOLDER,            /**< Invalid placeholder position (JQL_ERROR_INVALID_PLACEHOLDER) */
  JQL_ERROR_UNSET_PLACEHOLDER,              /**< Found unset placeholder (JQL_ERROR_UNSET_PLACEHOLDER) */
  JQL_ERROR_REGEXP_INVALID,                 /**< Invalid regular expression (JQL_ERROR_REGEXP_INVALID) */
  JQL_ERROR_REGEXP_CHARSET,
  /**< Invalid regular expression: expected ']' at end of character set
     (JQL_ERROR_REGEXP_CHARSET) */
  JQL_ERROR_REGEXP_SUBEXP,
  /**< Invalid regular expression: expected ')' at end of subexpression
     (JQL_ERROR_REGEXP_SUBEXP) */
  JQL_ERROR_REGEXP_SUBMATCH,
  /**< Invalid regular expression: expected '}' at end of submatch
     (JQL_ERROR_REGEXP_SUBMATCH) */
  JQL_ERROR_REGEXP_ENGINE,
  /**< Illegal instruction in compiled regular expression (please report this
     bug) (JQL_ERROR_REGEXP_ENGINE) */
  JQL_ERROR_SKIP_ALREADY_SET,               /**< Skip clause already specified (JQL_ERROR_SKIP_ALREADY_SET) */
  JQL_ERROR_LIMIT_ALREADY_SET,              /**< Limit clause already specified (JQL_ERROR_SKIP_ALREADY_SET) */
  JQL_ERROR_ORDERBY_MAX_LIMIT,
  /**< Reached max number of asc/desc order clauses: 64
     (JQL_ERROR_ORDERBY_MAX_LIMIT) */
  JQL_ERROR_NO_COLLECTION,                  /**< No collection specified in query (JQL_ERROR_NO_COLLECTION) */
  JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE,
  /**< Invalid type of placeholder value
     (JQL_ERROR_INVALID_PLACEHOLDER_VALUE_TYPE) */
  _JQL_ERROR_END,
  _JQL_ERROR_UNMATCHED,
} jql_ecode_t;

typedef uint8_t jql_create_mode_t;

/// Do not destroy query object in `jql_create()` on query parsing error,
/// convenient for parsing error reporting using `jql_error()`
#define JQL_KEEP_QUERY_ON_PARSE_ERROR ((jql_create_mode_t) 0x01U)

/// Do not print query parsing errors to `stderr`
#define JQL_SILENT_ON_PARSE_ERROR ((jql_create_mode_t) 0x02U)

/**
 * @brief Create query object from specified text query.
 * @param qptr Pointer to resulting query object
 * @param coll Optional collection name used to execute query
 * @param query Query text
 */
IW_EXPORT WUR iwrc jql_create(JQL *qptr, const char *coll, const char *query);

IW_EXPORT WUR iwrc jql_create2(JQL *qptr, const char *coll, const char *query, jql_create_mode_t mode);

IW_EXPORT const char* jql_collection(JQL q);

/**
 * @brief Bind JSON node data to query placeholder.
 * @warning Value JSON data is not copied and used as is.
 *          Caller is responsible to maintain `val` availability during execution of query.
 * @see jql_set_json2()
 */
IW_EXPORT WUR iwrc jql_set_json(JQL q, const char *placeholder, int index, JBL_NODE val);

IW_EXPORT WUR iwrc jql_set_json2(
  JQL q, const char *placeholder, int index, JBL_NODE val,
  void (*freefn)(void*, void*), void *op);

IW_EXPORT WUR iwrc jql_set_json_jbl(JQL q, const char *placeholder, int index, JBL jbl);

IW_EXPORT WUR iwrc jql_set_i64(JQL q, const char *placeholder, int index, int64_t val);

IW_EXPORT WUR iwrc jql_set_f64(JQL q, const char *placeholder, int index, double val);

/**
 * @brief Bind string data to query placeholder.
 * @warning Value string data is not copied and used as is.
 *          Caller is responsible to maintain `val` availability during execution of query.
 * @see jql_set_str2()
 */
IW_EXPORT WUR iwrc jql_set_str(JQL q, const char *placeholder, int index, const char *val);

IW_EXPORT WUR iwrc jql_set_str2(
  JQL q, const char *placeholder, int index, const char *val,
  void (*freefn)(void*, void*), void *op);

IW_EXPORT WUR iwrc jql_set_bool(JQL q, const char *placeholder, int index, bool val);


/**
 * @brief Bind regexp data string to query placeholder.
 * @warning Value string data is not copied and used as is.
 *          Caller is responsible to maintain `val` availability during execution of query.
 * @see jql_set_regexp2()
 */
IW_EXPORT WUR iwrc jql_set_regexp(JQL q, const char *placeholder, int index, const char *expr);

IW_EXPORT WUR iwrc jql_set_regexp2(
  JQL q, const char *placeholder, int index, const char *expr,
  void (*freefn)(void*, void*), void *op);

IW_EXPORT WUR iwrc jql_set_null(JQL q, const char *placeholder, int index);

IW_EXPORT WUR iwrc jql_matched(JQL q, JBL jbl, bool *out);

IW_EXPORT const char* jql_first_anchor(JQL q);

IW_EXPORT const char* jql_error(JQL q);

IW_EXPORT bool jql_has_apply(JQL q);

IW_EXPORT bool jql_has_apply_upsert(JQL q);

IW_EXPORT bool jql_has_apply_delete(JQL q);

IW_EXPORT bool jql_has_projection(JQL q);

IW_EXPORT bool jql_has_orderby(JQL q);

IW_EXPORT bool jql_has_aggregate_count(JQL q);

IW_EXPORT iwrc jql_get_skip(JQL q, int64_t *out);

IW_EXPORT iwrc jql_get_limit(JQL q, int64_t *out);

IW_EXPORT WUR iwrc jql_apply(JQL q, JBL_NODE root, IWPOOL *pool);

IW_EXPORT WUR iwrc jql_project(JQL q, JBL_NODE root, IWPOOL *pool, void *exec_ctx);

IW_EXPORT WUR iwrc jql_apply_and_project(JQL q, JBL jbl, JBL_NODE *out, void *exec_ctx, IWPOOL *pool);

IW_EXPORT void jql_reset(JQL q, bool reset_match_cache, bool reset_placeholders);

IW_EXPORT void jql_destroy(JQL *qptr);

IW_EXPORT size_t jql_estimate_allocated_size(JQL q);

IW_EXPORT WUR iwrc jql_init(void);

IW_EXTERN_C_END
#endif
