#pragma once
#ifndef JQP_H
#define JQP_H

#include "jql.h"
#include <errno.h>
#include <stdbool.h>
#include <setjmp.h>
#include <iowow/iwpool.h>

typedef enum {
  JQP_QUERY_TYPE = 1,
  JQP_FILTER_TYPE,
  JQP_NODE_TYPE,
  JQP_NEXPR_TYPE,
  JQP_NEXPR_LEFT_TYPE,
  JQP_NEXPR_RIGHT_TYPE
} jqp_unit_t;

typedef enum {
  JQP_NODE_FIELD = 1,
  JQP_NODE_ANY,
  JQP_NODE_ANYS,
  JQP_NODE_EXPR
} jqp_node_type_t;

typedef enum {
  JQP_NEXPR_FIELD = 1,
  JQP_NEXPR_ANY,
  JQP_NEXPR_LEFT
} jqp_nexpr_type_t;

typedef enum {
  JQP_AND = 1,
  JQP_OR = 2
} jqp_join_t;

typedef enum {
  JQP_OP_NEXPR_EQ = 1,
  JQP_OP_NEXPR_GT,
  JQP_OP_NEXPR_GTE,
  JQP_OP_NEXPR_LT,
  JQP_OP_NEXPR_LTE,
  JQP_OP_NEXPR_IN,
  JQP_OP_NEXPR_RE,
  JQP_OP_NEXPR_LIKE,
} jqp_nexpr_op_t;

typedef union _JQPUNIT JQPUNIT;

struct JQP_QUERY {
  jqp_unit_t type;
  JQPUNIT *filters;
};

struct JQP_FILTER {
  jqp_unit_t type;
  bool negate;
  jqp_join_t join;
  const char *anchor;
  JQPUNIT *nodes;
};

struct JQP_NODE {
  jqp_unit_t type;
  jqp_node_type_t node_type;
  const char *name;
  JQPUNIT *expression;
};

struct JQP_NEXPR {
  jqp_unit_t type;
  bool negate;
  jqp_join_t join;
  JQPUNIT *next;
  JQPUNIT *left;
  JQPUNIT *right;
};

//--

union _JQPUNIT {
  struct JQP_QUERY query;
  struct JQP_FILTER filter;
  struct JQP_NODE node;
};

typedef struct _JQPSTACK {
  jqp_unit_t type;
  JQPUNIT *value;
  struct _JQPSTACK *first;
  struct _JQPSTACK *last;
} JQPSTACK;

typedef struct _JQPAUX {
  int pos;
  int line;
  int col;
  iwrc rc;
  jmp_buf *fatal_jmp;
  const char *buf;
  JQPUNIT *unit;
  JQPSTACK *stack;
  IWPOOL *unitpool;
} JQPAUX;

iwrc jqp_aux_create(JQPAUX **auxp, const char *input);

void jqp_aux_destroy(JQPAUX **auxp);

iwrc jqp_parse(JQPAUX *aux);

#endif
