#pragma once
#ifndef JQP_H
#define JQP_H

#include "jql.h"
#include <errno.h>
#include <stdbool.h>
#include <setjmp.h>

typedef struct _JQPDATA { 
  int type;
  struct _JQPDATA *child;
  struct _JQPDATA *next;
} JQPDATA;

typedef struct _JQPAUX {
  int pos;
  int line;
  int col;
  iwrc rc;
  bool fatal_jmp_set;
  jmp_buf fatal_jmp;
  const char *buf;
  JQPDATA *data;
} JQPAUX;


iwrc jqp_parse(JQPAUX *aux);

#endif
