#pragma once
#ifndef JQP_H
#define JQP_H

#include "jql.h"
#include <errno.h>
#include <stdbool.h>
#include <setjmp.h>
#include <iowow/iwpool.h>

typedef enum {
  JQP_STR_PLACEHOLDER = 1,
  JQP_STR_JSON,
  JQP_STR_ANCHOR,
} jqp_strflags_t;

typedef enum {
  JQP_QUERY_TYPE = 1,
  JQP_FILTER_TYPE,
  JQP_NODE_TYPE,
  JQP_EXPR_TYPE,
  JQP_STRING_TYPE,
  JQP_OP_TYPE,
  JQP_JOIN_TYPE,
  JQP_PROJECTION_TYPE
} jqp_unit_t;

typedef enum {
  JQP_NODE_FIELD = 1,
  JQP_NODE_ANY,
  JQP_NODE_ANYS,
  JQP_NODE_EXPR
} jqp_node_type_t;

typedef enum {
  JQP_JOIN_AND = 1,
  JQP_JOIN_OR,
  JQP_OP_EQ,
  JQP_OP_GT,
  JQP_OP_GTE,
  JQP_OP_LT,
  JQP_OP_LTE,
  JQP_OP_IN,
  JQP_OP_RE,
  JQP_OP_LIKE,
} jqp_op_t;

typedef union _JQPUNIT JQPUNIT;

struct JQP_NODE {
  jqp_unit_t type;
  jqp_node_type_t ntype;  
  JQPUNIT *value;
  struct JQP_NODE *next;  
};

struct JQP_FILTER {
  jqp_unit_t type;
  const char *anchor;
  struct JQP_JOIN *join;
  struct JQP_NODE *node;  
};

struct JQP_EXPR {
  jqp_unit_t type;
  bool negate;
  struct JQP_JOIN *join;
  JQPUNIT *op;
  JQPUNIT *left;
  JQPUNIT *right;
  JQPUNIT *next;
};

struct JQP_STRING {
  jqp_unit_t type;
  jqp_strflags_t flags;
  char *value;
};

struct JQP_OP {
  jqp_unit_t type;
  bool negate;
  jqp_op_t op;
};

struct JQP_JOIN {
  jqp_unit_t type;
  bool negate;
  jqp_op_t join;
};

struct JQP_PROJECTION {
  jqp_unit_t type;
};

struct JQP_QUERY {
  jqp_unit_t type;
  struct JQP_FILTER *filter;
  struct JQP_STRING *apply;
};

//--

union _JQPUNIT {
  jqp_unit_t type;
  struct JQP_QUERY query;
  struct JQP_FILTER filter;
  struct JQP_NODE node;
  struct JQP_EXPR expr;
  struct JQP_JOIN join;
  struct JQP_OP op;
  struct JQP_STRING string; 
};

typedef enum {
  STACK_UNIT = 1,
  STACK_STRING,
  STACK_INT,
  STACK_FLOAT,
} jqp_stack_t;

typedef struct _JQPSTACK {  
  jqp_stack_t type;
  struct _JQPSTACK *next;
  struct _JQPSTACK *prev;
  union {
    JQPUNIT *unit;
    char *str;
    int64_t i64;
    double f64;    
  };
} JQPSTACK;


typedef struct _JQPAUX {
  int pos;
  int line;
  int col;
  iwrc rc;
  jmp_buf *fatal_jmp;
  const char* error;
  const char *buf;
  IWPOOL *pool;
  JQPSTACK *stack;
  struct JQP_QUERY *query;
  bool negate;
} JQPAUX;

iwrc jqp_aux_create(JQPAUX **auxp, const char *input);

void jqp_aux_destroy(JQPAUX **auxp);

iwrc jqp_parse(JQPAUX *aux);

#endif
