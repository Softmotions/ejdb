#pragma once
#ifndef JQP_H
#define JQP_H

#include "jql.h"
#include "jbl.h"
#include <errno.h>
#include <stdbool.h>
#include <setjmp.h>
#include <iowow/iwpool.h>
#include <iowow/iwxstr.h>

typedef enum {
  JQP_STR_PLACEHOLDER = 1,      /**< Query string parameter placeholder */
  JQP_STR_ANCHOR      = 1 << 1, /**< Query filter anchor */
  JQP_STR_PROJFIELD   = 1 << 2, /**< Projection field **/
  JQP_STR_PROJALIAS   = 1 << 3, /**< Projection alias (all) **/
  JQP_STR_QUOTED      = 1 << 4, /**< String is quoted */
  JQP_STR_STAR        = 1 << 5  /**< Star (*) string */
} jqp_string_flavours_t;

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
  JQP_JSON_TYPE
} jqp_unit_t;

typedef enum {
  JQP_NODE_FIELD = 1,
  JQP_NODE_ANY,
  JQP_NODE_ANYS,
  JQP_NODE_EXPR
} jqp_node_type_t;

typedef enum { // Do not reorder members
  JQP_JOIN_AND = 1,
  JQP_JOIN_OR,
  JQP_OP_EQ,
  JQP_OP_GT,
  JQP_OP_GTE,
  JQP_OP_LT,
  JQP_OP_LTE,
  JQP_OP_IN,
  JQP_OP_RE,  
} jqp_op_t;

struct JQP_AUX;

typedef union _JQP_UNIT JQPUNIT;

typedef struct JQP_EXPR_NODE { // Base for JQP_FILTER
  jqp_unit_t type;
  struct JQP_EXPR_NODE *next;
  struct JQP_JOIN *join;
  void *opaque;
  // Expr only specific
  struct JQP_EXPR_NODE *chain;
} JQP_EXPR_NODE;

typedef struct JQP_FILTER {
  //- JQP_EXPR_NODE
  jqp_unit_t type;
  struct JQP_EXPR_NODE *next;
  struct JQP_JOIN *join;
  void *opaque;
  //- JQP_EXPR_NODE
  const char *anchor;
  struct JQP_NODE *node;
} JQP_FILTER;

typedef struct JQP_JSON {
  jqp_unit_t type;
  struct _JBL_NODE jn;
  void *opaque;
} JQP_JSON;

typedef struct JQP_NODE {
  jqp_unit_t type;  
  jqp_node_type_t ntype;
  struct JQP_NODE *next;
  JQPUNIT *value;
  int start; // Used in query matching 
  int end;   // Used in query matching 
} JQP_NODE;

typedef struct JQP_STRING {
  jqp_unit_t type;
  jqp_string_flavours_t flavour;
  const char *value;
  struct JQP_STRING *next;
  struct JQP_STRING *subnext;
  void *opaque;
} JQP_STRING;

typedef struct JQP_INTEGER {
  jqp_unit_t type;
  int64_t value;
} JQP_INTEGER;

typedef struct JQP_DOUBLE {
  jqp_unit_t type;
  double value;
} JQP_DOUBLE;

typedef struct JQP_OP {
  jqp_unit_t type;
  bool negate;
  jqp_op_t op;
  struct JQP_OP *next;
  void *opaque;
} JQP_OP;

typedef struct JQP_JOIN {
  jqp_unit_t type;
  bool negate;
  jqp_op_t value;
} JQP_JOIN;

typedef struct JQP_EXPR {  
  jqp_unit_t type;
  struct JQP_JOIN *join;
  struct JQP_OP *op;
  JQPUNIT *left;
  JQPUNIT *right;
  struct JQP_EXPR *next;  
} JQP_EXPR;

typedef struct JQP_PROJECTION {
  jqp_unit_t type;
  bool exclude;
  struct JQP_STRING *value;
  struct JQP_PROJECTION *next;
} JQP_PROJECTION;


typedef struct JQP_QUERY {
  jqp_unit_t type;
  struct JQP_EXPR_NODE *expr;
  JBL_NODE apply;
  const char *apply_placeholder;
  struct JQP_PROJECTION *projection;
  struct JQP_AUX *aux;
} JQP_QUERY;

//--

union _JQP_UNIT {
  jqp_unit_t type;
  struct JQP_QUERY query;
  struct JQP_EXPR_NODE exprnode;
  struct JQP_FILTER filter;
  struct JQP_NODE node;
  struct JQP_EXPR expr;
  struct JQP_JOIN join;
  struct JQP_OP op;
  struct JQP_STRING string;
  struct JQP_INTEGER intval;
  struct JQP_DOUBLE dblval;
  struct JQP_JSON json;
  struct JQP_PROJECTION projection;
};

typedef enum {
  STACK_UNIT = 1,
  STACK_STRING,
  STACK_INT,
  STACK_FLOAT,
} jqp_stack_t;

typedef struct JQP_STACK {
  jqp_stack_t type;
  struct JQP_STACK *next;
  struct JQP_STACK *prev;
  union {
    JQPUNIT *unit;
    char *str;
    int64_t i64;
    double f64;
  };
} JQP_STACK;

#define JQP_AUX_STACKPOOL_NUM 128

typedef struct JQP_AUX {
  bool negate;
  int pos;
  iwrc rc;
  jmp_buf fatal_jmp;
  const char *buf;
  IWXSTR *xerr;
  IWPOOL *pool;
  struct JQP_QUERY *query;
  JQP_STACK *stack;
  int num_placeholders;
  JQP_STRING *start_placeholder;
  JQP_STRING *end_placeholder;
  JQP_OP *start_op;
  JQP_OP *end_op;
  JQP_STACK stackpool[JQP_AUX_STACKPOOL_NUM];
  int stackn;
} JQP_AUX;

iwrc jqp_aux_create(JQP_AUX **auxp, const char *input);

void jqp_aux_destroy(JQP_AUX **auxp);

iwrc jqp_parse(JQP_AUX *aux);

iwrc jqp_print_query(const JQP_QUERY *q, jbl_json_printer pt, void *op);

#endif
