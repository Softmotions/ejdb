// -V::802
#pragma once
#ifndef JQP_H
#define JQP_H

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

#include "jql.h"

#include <iowow/iwpool.h>
#include <iowow/iwxstr.h>

#include <stdbool.h>
#include <setjmp.h>

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
/** Projection path */
#define JQP_STR_PROJPATH ((jqp_string_flavours_t) 0x200U)


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

struct jqp_aux;

typedef union jqp_unit JQPUNIT;

#define JQP_EXPR_NODE_FLAG_PK 0x01U

#define JQP_EXPR_NODE_HEAD          \
        jqp_unit_t type;            \
        struct jqp_expr_node *next; \
        struct jqp_join *join;      \
        void *opaque;               \
        uint8_t flags;

typedef struct jqp_expr_node { // Base for JQP_FILTER
  JQP_EXPR_NODE_HEAD
  struct jqp_expr_node *chain;
} JQP_EXPR_NODE;

typedef struct jqp_expr_node_pk {
  JQP_EXPR_NODE_HEAD
  struct jqp_expr_node *chain; // Not used, plased for JQP_EXPR_NODE compatibility
  const char *anchor;
  JQPUNIT    *argument;
} JQP_EXPR_NODE_PK;

typedef struct jqp_filter {
  JQP_EXPR_NODE_HEAD
  const char      *anchor;
  struct jqp_node *node;
} JQP_FILTER;

typedef struct jqp_json {
  jqp_unit_t      type;
  struct jbl_node jn;
  void *opaque;
} JQP_JSON;

typedef struct jqp_node {
  jqp_unit_t       type;
  jqp_node_type_t  ntype;
  struct jqp_node *next;
  JQPUNIT *value;
  int      start; // Used in query matching
  int      end;   // Used in query matching
} JQP_NODE;

typedef struct jqp_string {
  jqp_unit_t type;
  jqp_string_flavours_t flavour;
  const char *value;
  struct jqp_string *next;
  struct jqp_string *subnext;
  struct jqp_string *placeholder_next;
  void *opaque;
} JQP_STRING;

typedef struct jqp_integer {
  jqp_unit_t type;
  jqp_int_flavours_t flavour;
  int64_t value;
  void   *opaque;
} JQP_INTEGER;

typedef struct jqp_double {
  jqp_unit_t type;
  jqp_int_flavours_t flavour;
  double value;
  void  *opaque;
} JQP_DOUBLE;

typedef struct jqp_op {
  jqp_unit_t type;
  bool       negate;
  jqp_op_t   value;
  struct jqp_op *next;
  void *opaque;
} JQP_OP;

typedef struct jqp_join {
  jqp_unit_t type;
  bool       negate;
  jqp_op_t   value;
} JQP_JOIN;

typedef struct jqp_expr {
  jqp_unit_t       type;
  struct jqp_join *join;
  struct jqp_op   *op;
  JQPUNIT *left;
  JQPUNIT *right;
  struct jqp_expr *next;
  bool prematched;
} JQP_EXPR;

typedef struct jqp_projection {
  jqp_unit_t type;
  struct jqp_string     *value;
  struct jqp_projection *next;
  int16_t pos;          // Current matching position, used in jql.c#_jql_project
  int16_t cnt;          // Number of projection sections, used in jql.c#_jql_project

#define JQP_PROJECTION_FLAG_EXCLUDE 0x01U
#define JQP_PROJECTION_FLAG_INCLUDE 0x02U
#define JQP_PROJECTION_FLAG_JOINS   0x04U
  uint8_t flags;
} JQP_PROJECTION;

typedef struct jqp_query {
  jqp_unit_t      type;
  struct jqp_aux *aux;
} JQP_QUERY;

//--

union jqp_unit {
  jqp_unit_t       type;
  struct jqp_query query;
  struct jqp_expr_node    exprnode;
  struct jqp_expr_node_pk exprnode_pk;
  struct jqp_filter       filter;
  struct jqp_node    node;
  struct jqp_expr    expr;
  struct jqp_join    join;
  struct jqp_op      op;
  struct jqp_string  string;
  struct jqp_integer intval;
  struct jqp_double  dblval;
  struct jqp_json    json;
  struct jqp_projection projection;
};

typedef enum {
  STACK_UNIT = 1,
  STACK_STRING,
  STACK_INT,
  STACK_FLOAT,
} jqp_stack_t;

typedef struct jqp_stack {
  jqp_stack_t       type;
  struct jqp_stack *next;
  struct jqp_stack *prev;
  union {
    union jqp_unit *unit;
    char   *str;
    int64_t i64;
    double  f64;
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

typedef struct jqp_aux {
  int     pos;
  int     stackn;
  int     num_placeholders;
  int     orderby_num;                  /**< Number of order-by blocks */
  iwrc    rc;
  jmp_buf fatal_jmp;
  const char       *buf;
  struct iwxstr    *xerr;
  struct iwpool    *pool;
  struct jqp_query *query;
  struct jqp_stack *stack;
  jql_create_mode_t mode;
  //
  struct jqp_expr_node  *expr;
  struct jqp_projection *projection;
  struct jqp_string     *start_placeholder;
  struct jqp_string     *end_placeholder;
  struct jqp_string     *orderby;
  struct jbl_ptr       **orderby_ptrs;        /**< Order-by pointers, orderby_num - number of pointers allocated */
  struct jqp_op   *start_op;
  struct jqp_op   *end_op;
  union jqp_unit  *skip;
  union jqp_unit  *limit;
  struct jbl_node *apply;
  const char      *apply_placeholder;
  const char      *first_anchor;
  jqp_query_mode_t qmode;
  bool negate;
  bool has_keep_projections;
  bool has_exclude_all_projection;
  struct jqp_stack stackpool[JQP_AUX_STACKPOOL_NUM];
} JQP_AUX;

iwrc jqp_aux_create(struct jqp_aux **auxp, const char *input);

void jqp_aux_destroy(struct jqp_aux **auxp);

iwrc jqp_parse(struct jqp_aux *aux);

iwrc jqp_print_query(const struct jqp_query *q, jbl_json_printer pt, void *op);

iwrc jqp_alloc_orderby_pointers(const struct jqp_query *q, struct jbl_ptr **optr, size_t *nptr);

iwrc jqp_print_filter_node_expr(const struct jqp_expr *e, jbl_json_printer pt, void *op);

#endif
