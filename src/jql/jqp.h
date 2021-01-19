// -V::802
#pragma once
#ifndef JQP_H
#define JQP_H

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

#include "jql.h"
#include "jbl.h"
#include <errno.h>
#include <stdbool.h>
#include <setjmp.h>
#include <ejdb2/iowow/iwpool.h>
#include <ejdb2/iowow/iwxstr.h>

typedef uint16_t jqp_string_flavours_t;
/** Query string parameter placeholder */
#define JQP_STR_PLACEHOLDER ((jqp_string_flavours_t) 0x01U)
/** Query filter anchor */
#define JQP_STR_ANCHOR ((jqp_string_flavours_t) 0x02U)
/** Projection field **/
#define JQP_STR_PROJFIELD ((jqp_string_flavours_t) 0x04U)
/** Projection alias (all) **/
#define JQP_STR_PROJALIAS ((jqp_string_flavours_t) 0x08U)
/** String is quoted */
#define JQP_STR_QUOTED ((jqp_string_flavours_t) 0x10U)
/** Star (*) string */
#define JQP_STR_STAR ((jqp_string_flavours_t) 0x20U)
/** Boolean negation/mode applied to it */
#define JQP_STR_NEGATE ((jqp_string_flavours_t) 0x40U)
/** Double star (**) string */
#define JQP_STR_DBL_STAR ((jqp_string_flavours_t) 0x80U)
/** Projection JOIN */
#define JQP_STR_PROJOIN ((jqp_string_flavours_t) 0x100U)


typedef uint8_t jqp_int_flavours_t;
#define JQP_INT_SKIP  ((jqp_int_flavours_t) 0x01U)
#define JQP_INT_LIMIT ((jqp_int_flavours_t) 0x02U)

typedef enum {
  JQP_QUERY_TYPE = 1,
  JQP_EXPR_NODE_TYPE,
  JQP_FILTER_TYPE,
  JQP_NODE_TYPE,
  JQP_EXPR_TYPE,
  JQP_STRING_TYPE,
  JQP_INTEGER_TYPE,
  JQP_DOUBLE_TYPE,
  JQP_OP_TYPE,
  JQP_JOIN_TYPE,
  JQP_PROJECTION_TYPE,
  JQP_JSON_TYPE,
} jqp_unit_t;

typedef enum {
  JQP_NODE_FIELD = 1,
  JQP_NODE_ANY,
  JQP_NODE_ANYS,
  JQP_NODE_EXPR,
} jqp_node_type_t;

typedef enum {
  // Do not reorder members
  JQP_JOIN_AND = 1,
  JQP_JOIN_OR,
  JQP_OP_EQ,
  JQP_OP_GT,
  JQP_OP_GTE,
  JQP_OP_LT,
  JQP_OP_LTE,
  JQP_OP_IN,
  JQP_OP_NI,
  JQP_OP_RE,
  JQP_OP_PREFIX,
} jqp_op_t;

struct JQP_AUX;

typedef union _JQP_UNIT JQPUNIT;

#define JQP_EXPR_NODE_FLAG_PK 0x01U

#define JQP_EXPR_NODE_HEAD    \
  jqp_unit_t type;            \
  struct JQP_EXPR_NODE *next; \
  struct JQP_JOIN *join;      \
  void *opaque;               \
  uint8_t flags;

typedef struct JQP_EXPR_NODE { // Base for JQP_FILTER
  JQP_EXPR_NODE_HEAD
  struct JQP_EXPR_NODE *chain;
} JQP_EXPR_NODE;

typedef struct JQP_EXPR_NODE_PK {
  JQP_EXPR_NODE_HEAD
  struct JQP_EXPR_NODE *chain; // Not used, plased for JQP_EXPR_NODE compatibility
  const char *anchor;
  JQPUNIT    *argument;
} JQP_EXPR_NODE_PK;

typedef struct JQP_FILTER {
  JQP_EXPR_NODE_HEAD
  const char      *anchor;
  struct JQP_NODE *node;
} JQP_FILTER;

typedef struct JQP_JSON {
  jqp_unit_t       type;
  struct _JBL_NODE jn;
  void *opaque;
} JQP_JSON;

typedef struct JQP_NODE {
  jqp_unit_t       type;
  jqp_node_type_t  ntype;
  struct JQP_NODE *next;
  JQPUNIT *value;
  int      start; // Used in query matching
  int      end;   // Used in query matching
} JQP_NODE;

typedef struct JQP_STRING {
  jqp_unit_t type;
  jqp_string_flavours_t flavour;
  const char *value;
  struct JQP_STRING *next;
  struct JQP_STRING *subnext;
  struct JQP_STRING *placeholder_next;
  void *opaque;
} JQP_STRING;

typedef struct JQP_INTEGER {
  jqp_unit_t type;
  jqp_int_flavours_t flavour;
  int64_t value;
  void   *opaque;
} JQP_INTEGER;

typedef struct JQP_DOUBLE {
  jqp_unit_t type;
  jqp_int_flavours_t flavour;
  double value;
  void  *opaque;
} JQP_DOUBLE;

typedef struct JQP_OP {
  jqp_unit_t type;
  bool       negate;
  jqp_op_t   value;
  struct JQP_OP *next;
  void *opaque;
} JQP_OP;

typedef struct JQP_JOIN {
  jqp_unit_t type;
  bool       negate;
  jqp_op_t   value;
} JQP_JOIN;

typedef struct JQP_EXPR {
  jqp_unit_t       type;
  struct JQP_JOIN *join;
  struct JQP_OP   *op;
  JQPUNIT *left;
  JQPUNIT *right;
  struct JQP_EXPR *next;
  bool prematched;
} JQP_EXPR;

typedef struct JQP_PROJECTION {
  jqp_unit_t type;
  struct JQP_STRING     *value;
  struct JQP_PROJECTION *next;
  int16_t pos;          // Current matching position, used in jql.c#_jql_project
  int16_t cnt;          // Number of projection sections, used in jql.c#_jql_project

#define JQP_PROJECTION_FLAG_EXCLUDE 0x01U
#define JQP_PROJECTION_FLAG_INCLUDE 0x02U
#define JQP_PROJECTION_FLAG_JOINS   0x04U
  uint8_t flags;
} JQP_PROJECTION;

typedef struct JQP_QUERY {
  jqp_unit_t      type;
  struct JQP_AUX *aux;
} JQP_QUERY;

//--

union _JQP_UNIT {
  jqp_unit_t       type;
  struct JQP_QUERY query;
  struct JQP_EXPR_NODE    exprnode;
  struct JQP_EXPR_NODE_PK exprnode_pk;
  struct JQP_FILTER       filter;
  struct JQP_NODE    node;
  struct JQP_EXPR    expr;
  struct JQP_JOIN    join;
  struct JQP_OP      op;
  struct JQP_STRING  string;
  struct JQP_INTEGER intval;
  struct JQP_DOUBLE  dblval;
  struct JQP_JSON    json;
  struct JQP_PROJECTION projection;
};

typedef enum {
  STACK_UNIT = 1,
  STACK_STRING,
  STACK_INT,
  STACK_FLOAT,
} jqp_stack_t;

typedef struct JQP_STACK {
  jqp_stack_t       type;
  struct JQP_STACK *next;
  struct JQP_STACK *prev;
  union {
    JQPUNIT *unit;
    char    *str;
    int64_t  i64;
    double   f64;
  };
} JQP_STACK;

#define JQP_AUX_STACKPOOL_NUM 128

typedef uint8_t jqp_query_mode_t;

#define JQP_QRY_COUNT        ((jqp_query_mode_t) 0x01U)
#define JQP_QRY_NOIDX        ((jqp_query_mode_t) 0x02U)
#define JQP_QRY_APPLY_DEL    ((jqp_query_mode_t) 0x04U)
#define JQP_QRY_INVERSE      ((jqp_query_mode_t) 0x08U)
#define JQP_QRY_APPLY_UPSERT ((jqp_query_mode_t) 0x10U)

#define JQP_QRY_AGGREGATE (JQP_QRY_COUNT)

typedef struct JQP_AUX {
  int     pos;
  int     stackn;
  int     num_placeholders;
  int     orderby_num;                  /**< Number of order-by blocks */
  iwrc    rc;
  jmp_buf fatal_jmp;
  const char *buf;
  IWXSTR     *xerr;
  IWPOOL     *pool;
  struct JQP_QUERY *query;
  JQP_STACK *stack;
  jql_create_mode_t mode;
  //
  struct JQP_EXPR_NODE  *expr;
  struct JQP_PROJECTION *projection;
  JQP_STRING *start_placeholder;
  JQP_STRING *end_placeholder;
  JQP_STRING *orderby;
  JBL_PTR    *orderby_ptrs;           /**< Order-by pointers, orderby_num - number of pointers allocated */
  JQP_OP     *start_op;
  JQP_OP     *end_op;
  JQPUNIT    *skip;
  JQPUNIT    *limit;
  JBL_NODE    apply;
  const char *apply_placeholder;
  const char *first_anchor;
  jqp_query_mode_t qmode;
  bool      negate;
  bool      has_keep_projections;
  bool      has_exclude_all_projection;
  JQP_STACK stackpool[JQP_AUX_STACKPOOL_NUM];
} JQP_AUX;

iwrc jqp_aux_create(JQP_AUX **auxp, const char *input);

void jqp_aux_destroy(JQP_AUX **auxp);

iwrc jqp_parse(JQP_AUX *aux);

iwrc jqp_print_query(const JQP_QUERY *q, jbl_json_printer pt, void *op);

iwrc jqp_alloc_orderby_pointers(const JQP_QUERY *q, JBL_PTR *optr, size_t *nptr);

iwrc jqp_print_filter_node_expr(const JQP_EXPR *e, jbl_json_printer pt, void *op);

#endif
