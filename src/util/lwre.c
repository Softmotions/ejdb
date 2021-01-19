// -V::506

/* Copyright (c) 2014 by Ian Piumarta
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the 'Software'),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, provided that the above copyright notice(s) and this
 * permission notice appear in all copies of the Software.  Acknowledgement
 * of the use of this Software in supporting documentation would be
 * appreciated but is not required.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS'.  USE ENTIRELY AT YOUR OWN RISK.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>

#include "lwre.h"

#ifndef RE_ERROR
# define RE_ERROR(RE, CODE, MESSAGE) { (RE)->error_message = (MESSAGE);  \
                                       longjmp(*(RE)->error_env, (re->error_code = RE_ERROR_ ## CODE)); }
#endif

#ifndef RE_MALLOC
# define RE_MALLOC(RE, SIZE) re__malloc((RE), (SIZE))

static void *re__malloc(struct re *re, size_t size) {
  void *p = malloc(size);
  if (!p) {
    RE_ERROR(re, NOMEM, "out of memory");
  }
  return p;
}

#endif

#ifndef RE_CALLOC
# define RE_CALLOC(RE, NMEMB, SIZE) re__calloc((RE), (NMEMB), (SIZE))

static void *re__calloc(struct re *re, size_t nmemb, size_t size) {
  void *p = calloc(nmemb, size);
  if (p) {
    return p;
  }
  if (re) {
    RE_ERROR(re, NOMEM, "out of memory");
  }
  return p;
}

#endif

#ifndef RE_REALLOC
# define RE_REALLOC(RE, PTR, SIZE) re__realloc((RE), (PTR), (SIZE))

static inline void *re__realloc(struct re *re, void *ptr, size_t size) {
  void *p = realloc(ptr, size);
  if (!p) {
    RE_ERROR(re, NOMEM, "out of memory");
  }
  return p;
}

#endif

#ifndef RE_FREE
# define RE_FREE(RE, PTR) free(PTR)
#endif

/* arrays */

#define re_array_of(TYPE) \
  struct {      \
    int   size;    \
    int   capacity;  \
    TYPE *at;    \
  }

#define RE_ARRAY_OF_INITIALISER { 0, 0, 0 }

#define re_array_append(RE, ARRAY, ELEMENT)                 \
  ((ARRAY).size++,                        \
   (((ARRAY).size > (ARRAY).capacity)                   \
    ? ((ARRAY).at = RE_REALLOC((RE), (ARRAY).at,               \
                               sizeof(*(ARRAY).at) * ((ARRAY).capacity = ((ARRAY).capacity      \
                                                                          ? ((ARRAY).capacity * 2)    \
                                                                          : 8))))       \
    : (ARRAY).at)                       \
   [(ARRAY).size - 1] = (ELEMENT))

#define re_array_copy(RE, ARRAY)          \
  { (ARRAY).size,             \
    (ARRAY).size,             \
    memcpy(RE_MALLOC((RE), sizeof((ARRAY).at[0]) * (ARRAY).size), \
           (ARRAY).at,            \
           sizeof((ARRAY).at[0]) * (ARRAY).size) }

#define re_array_release(RE, ARRAY) {   \
    if ((ARRAY).at) {     \
      RE_FREE((RE), (ARRAY).at);    \
      (ARRAY).at = 0;      \
    }         \
}

/* bit sets */

struct RE_BitSet;
typedef struct RE_BitSet RE_BitSet;

struct RE_BitSet {
  int inverted;
  unsigned char bits[256 / sizeof(unsigned char)];
};

static int re_bitset__includes(RE_BitSet *c, int i) {
  if ((i < 0) || (255 < i)) {
    return 0;
  }
  return (c->bits[i / 8] >> (i % 8)) & 1;
}

static int re_bitset_includes(RE_BitSet *c, int i) {
  int inc = re_bitset__includes(c, i);
  if (c->inverted) {
    inc = !inc;
  }
  return inc;
}

static void re_bitset_add(RE_BitSet *c, int i) {
  if ((i < 0) || (255 < i)) {
    return;
  }
  c->bits[i / 8] |= (1 << (i % 8));
}

/* character classes */

static int re_make_char(struct re *re) {
  const char *p = re->position;
  if (!*p) {
    return 0;
  }
  int c = *p++;
  if (('\\' == c) && *p) {
    c = *p++;
  }
  re->position = p;
  return c;
}

static RE_BitSet *re_make_class(struct re *re) {
  RE_BitSet *c = RE_CALLOC(re, 1, sizeof(RE_BitSet));
  int last = -1;
  c->inverted = ('^' == *re->position); // -V522
  if (c->inverted) {
    re->position++;
  }
  while (*re->position && (']' != *re->position)) {
    int this = re->position[0];
    if (('-' == this) && (last >= 0) && re->position[1] && (']' != re->position[1])) {
      re->position++;
      this = re_make_char(re);
      do {
        re_bitset_add(c, last++);
      } while (last <= this);
      last = -1;
    } else {
      this = re_make_char(re);
      re_bitset_add(c, this);
      last = this;
    }
  }
  return c;
}

/* instructions */

enum { RE_Any, RE_Char, RE_Class, RE_Accept, RE_Jump, RE_Fork, RE_Begin, RE_End, };

struct RE_Insn;
typedef struct RE_Insn RE_Insn;

struct RE_Insn {
  int  opcode;
  long x;
  union {
    long       y;
    RE_BitSet *c;
  };
  union {
    RE_Insn *next;
    char    *stamp;
  };
};

struct RE_Compiled;
typedef struct RE_Compiled RE_Compiled;

/*
   struct RE_Compiled
   {
    int    size;
    RE_Insn *first;
    RE_Insn *last;
   };

 #define RE_COMPILED_INITIALISER { 0, 0, 0 }
 */

static RE_Compiled re_insn_new(struct re *re, int opc) {
  RE_Insn *insn = RE_CALLOC(re, 1, sizeof(RE_Insn));
  insn->opcode = opc;  // -V522
  RE_Compiled insns = { 1, insn, insn };
  return insns;
}

static RE_Compiled re_new_Any(struct re *re) {
  RE_Compiled insns = re_insn_new(re, RE_Any);
  return insns;
}

static RE_Compiled re_new_Char(struct re *re, int c) {
  RE_Compiled insns = re_insn_new(re, RE_Char);
  insns.first->x = c;
  return insns;
}

static RE_Compiled re_new_Class(struct re *re, RE_BitSet *c) {
  RE_Compiled insns = re_insn_new(re, RE_Class);
  insns.first->c = c;
  return insns;
}

static RE_Compiled re_new_Accept(struct re *re) {
  RE_Compiled insns = re_insn_new(re, RE_Accept);
  return insns;
}

static RE_Compiled re_new_Jump(struct re *re, int x) {
  RE_Compiled insns = re_insn_new(re, RE_Jump);
  insns.first->x = x;
  return insns;
}

static RE_Compiled re_new_Fork(struct re *re, int x, int y) {
  RE_Compiled insns = re_insn_new(re, RE_Fork);
  insns.first->x = x;
  insns.first->y = y;
  return insns;
}

static RE_Compiled re_new_Begin(struct re *re) {
  RE_Compiled insns = re_insn_new(re, RE_Begin);
  return insns;
}

static RE_Compiled re_new_End(struct re *re) {
  RE_Compiled insns = re_insn_new(re, RE_End);
  return insns;
}

static void re_program_append(RE_Compiled *insns, RE_Compiled tail) {
  insns->last->next = tail.first;
  insns->last = tail.last;
  insns->size += tail.size;
}

static void re_program_prepend(RE_Compiled *insns, RE_Compiled head) {
  head.last->next = insns->first;
  insns->first = head.first;
  insns->size += head.size;
}

static void re_program_free(struct re *re, RE_Compiled *insns) {
  int i;
  for (i = 0; i < insns->size; ++i) {
    switch (insns->first[i].opcode) {
      case RE_Class: {
        RE_FREE(re, insns->first[i].c);
        insns->first[i].c = 0;
        break;
      }
    }
  }
  RE_FREE(re, insns->first);
  insns->first = insns->last = 0;
  insns->size = 0;
}

/* compilation */

/*
   struct re
   {
    char     *expression;
    char     *position;
    jmp_buf    *error_env;
    int       error_code;
    char     *error_message;
    struct RE_Compiled    code;
    char    **matches;
    int       nmatches;
   };
 */

static RE_Compiled re_compile_expression(struct re *re);

static RE_Compiled re_compile_primary(struct re *re) {
  int c = *re->position++;
  assert(0 != c);
  switch (c) {
    case '\\': {
      if (*re->position) {
        c = *re->position++;
      }
      break;
    }
    case '.': {
      return re_new_Any(re);
    }
    case '[': {
      RE_BitSet *cc = re_make_class(re);
      if (']' != *re->position) {
        RE_FREE(re, cc);
        RE_ERROR(re, CHARSET, "expected ']' at end of character set");
      }
      re->position++;
      return re_new_Class(re, cc);
    };
    case '(': {
      RE_Compiled insns = re_compile_expression(re);
      if (')' != *re->position) {
        RE_Insn *insn, *next;
        for (insn = insns.first; insn; insn = next) {
          next = insn->next;
          RE_FREE(re, insn);
        }
        RE_ERROR(re, SUBEXP, "expected ')' at end of subexpression");
      }
      re->position++;
      return insns;
    }
    case '{': {
      RE_Compiled insns = re_compile_expression(re);
      if ('}' != *re->position) {
        RE_Insn *insn, *next;
        for (insn = insns.first; insn; insn = next) {
          next = insn->next;
          RE_FREE(re, insn);
        }
        RE_ERROR(re, SUBMATCH, "expected '}' at end of submatch");
      }
      re_program_prepend(&insns, re_new_Begin(re));
      re_program_append(&insns, re_new_End(re));
      re->position++;
      return insns;
    }
  }
  return re_new_Char(re, c);
}

static RE_Compiled re_compile_suffix(struct re *re) {
  RE_Compiled insns = re_compile_primary(re);
  switch (*re->position) {
    case '?': {
      re->position++;
      if ('?' == *re->position) {
        re->position++;
        re_program_prepend(&insns, re_new_Fork(re, insns.size, 0));
      } else {
        re_program_prepend(&insns, re_new_Fork(re, 0, insns.size));
      }
      break;
    }
    case '*': {
      re->position++;
      if ('?' == *re->position) {
        re->position++;
        re_program_prepend(&insns, re_new_Fork(re, insns.size + 1, 0));
      } else {
        re_program_prepend(&insns, re_new_Fork(re, 0, insns.size + 1));
      }
      re_program_append(&insns, re_new_Jump(re, -(insns.size + 1)));
      break;
    }
    case '+': {
      re->position++;
      if ('?' == *re->position) {
        re->position++;
        re_program_append(&insns, re_new_Fork(re, 0, -(insns.size + 1)));
      } else {
        re_program_append(&insns, re_new_Fork(re, -(insns.size + 1), 0));
      }
      break;
    }
  }
  return insns;
}

static RE_Compiled re_compile_sequence(struct re *re) {
  if (!*re->position) {
    return re_new_Accept(re);
  }
  RE_Compiled head = re_compile_suffix(re);
  while (*re->position && !strchr("|)}>", *re->position))
    re_program_append(&head, re_compile_suffix(re));
  if (!*re->position) {
    re_program_append(&head, re_new_Accept(re));
  }
  return head;
}

static RE_Compiled re_compile_expression(struct re *re) {
  RE_Compiled head = re_compile_sequence(re);
  while ('|' == *re->position) {
    re->position++;
    RE_Compiled tail = re_compile_sequence(re);
    re_program_append(&head, re_new_Jump(re, tail.size));
    re_program_prepend(&head, re_new_Fork(re, 0, head.size));
    re_program_append(&head, tail);
  }
  return head;
}

static RE_Compiled re_compile(struct re *re) {
  jmp_buf env;
  RE_Compiled insns = RE_COMPILED_INITIALISER;
  re->error_env = &env;
  if (setjmp(env)) {  /* syntax error */
    return insns;
  }
  insns = re_compile_expression(re);
  re_array_of(RE_Insn) program = RE_ARRAY_OF_INITIALISER;
  RE_Insn *insn, *next;
  for (insn = insns.first; insn; insn = next) {
    re_array_append(re, program, *insn);
    next = insn->next;
    RE_FREE(re, insn);
  }

#if 0
  int i;
  for (i = 0; i < program.size; ++i) {
    RE_Insn *insn = &program.at[i];
    printf("%03i ", i);
    switch (insn->opcode) {
      case RE_Any:
        printf("Any\n");
        break;
      case RE_Char:
        printf("Char   %li\n", insn->x);
        break;
      case RE_Class: {
        printf("Class ");
        {
          RE_BitSet *c = insn->c;
          int i;
          putchar('[');
          if (c->inverted) {
            putchar('^');
          }
          for (i = 0; i < 256; ++i)
            if (re_bitset__includes(c, i)) {
              switch (i) {
                case '\a':
                  printf("\\a");
                  break;
                case '\b':
                  printf("\\b");
                  break;
                case '\t':
                  printf("\\t");
                  break;
                case '\n':
                  printf("\\n");
                  break;
                case '\v':
                  printf("\\v");
                  break;
                case '\f':
                  printf("\\f");
                  break;
                case '\r':
                  printf("\\r");
                  break;
                case '\\':
                  printf("\\\\");
                  break;
                case ']':
                  printf("\\]");
                  break;
                case '^':
                  printf("\\^");
                  break;
                case '-':
                  printf("\\-");
                  break;
                default:
                  if (isprint(i)) {
                    putchar(i);
                  } else {
                    printf("\\x%02x", i);
                  }
              }
            }
          putchar(']');
        }
        putchar('\n');
        break;
      }
      case RE_Accept:
        printf("Accept\n");
        break;
      case RE_Jump:
        printf("Jump   %li -> %03li\n", insn->x, i + 1 + insn->x);
        break;
      case RE_Fork:
        printf("Fork   %li %li -> %03li %03li\n", insn->x, insn->y, i + 1 + insn->x, i + 1 + insn->y);
        break;
      case RE_Begin:
        printf("Begin\n");
        break;
      case RE_End:
        printf("End\n");
        break;
      default:
        printf("?%i\n", insn->opcode);
        break;
    }
  }
#endif

  assert(program.size == insns.size);

  insns.first = program.at;
  insns.last = insns.first + insns.size;

  return insns;
}

/* submatch recording */

typedef re_array_of(char*) re_array_of_charp;

struct RE_Submatches;
typedef struct RE_Submatches RE_Submatches;

struct RE_Submatches {
  int refs;
  re_array_of_charp beginnings;
  re_array_of_charp endings;
};

static RE_Submatches *re_submatches_copy(struct re *re, RE_Submatches *orig) {
  RE_Submatches *subs = RE_CALLOC(re, 1, sizeof(RE_Submatches));
  if (orig) {
    subs->beginnings = (re_array_of_charp) re_array_copy(re, orig->beginnings); // -V522
    subs->endings = (re_array_of_charp) re_array_copy(re, orig->endings);
  }
  return subs;
}

static void re_submatches_free(struct re *re, RE_Submatches *subs) {
  assert(subs);
  assert(!subs->refs);
  re_array_release(re, subs->beginnings);
  re_array_release(re, subs->endings);
  RE_FREE(re, subs);
}

static inline RE_Submatches *re_submatches_link(RE_Submatches *subs) {
  if (subs) {
    subs->refs++;
  }
  return subs;
}

static inline void re_submatches_unlink(struct re *re, RE_Submatches *subs) {
  if (subs && (0 == --(subs->refs))) {
    re_submatches_free(re, subs);
  }
}

/* matching */

struct RE_Thread;
typedef struct RE_Thread RE_Thread;

struct RE_Thread {
  RE_Insn       *pc;
  RE_Submatches *submatches;
};

struct RE_ThreadList;
typedef struct RE_ThreadList RE_ThreadList;

struct RE_ThreadList {
  int size;
  RE_Thread *at;
};

static inline RE_Thread re_thread(RE_Insn *pc, RE_Submatches *subs) {
  return (RE_Thread) {
           pc, subs
  };
}

static void re_thread_schedule(struct re *re, RE_ThreadList *threads, RE_Insn *pc, char *sp, RE_Submatches *subs) {
  if (pc->stamp == sp) {
    return;
  }
  pc->stamp = sp;

  switch (pc->opcode) {
    case RE_Jump:
      re_thread_schedule(re, threads, pc + 1 + pc->x, sp, subs);
      return;
    case RE_Fork:
      re_thread_schedule(re, threads, pc + 1 + pc->x, sp, subs);
      re_thread_schedule(re, threads, pc + 1 + pc->y, sp, subs);
      return;
    case RE_Begin:
      subs = re_submatches_copy(re, subs);
      re_array_append(re, subs->beginnings, sp); // -V522
      re_thread_schedule(re, threads, pc + 1, sp, subs);
      if (!subs->refs) {
        re_submatches_free(re, subs);
      }
      return;
    case RE_End: {
      subs = re_submatches_copy(re, subs);
#   if 0    /* non-nesting groups: ab{cd{ef}gh}ij => {cdef} {efgh}  */
      re_array_append(re, subs->endings, sp);
#   else    /* nesting groups: ab{cd{ef}gh}ij => {cdefgh} {ef}  */
      while (subs->endings.size < subs->beginnings.size) re_array_append(re, subs->endings, 0);
      int i;
      for (i = subs->endings.size; i--; ) {
        if (!subs->endings.at[i]) {
          subs->endings.at[i] = sp;
          break;
        }
      }
#   endif
      re_thread_schedule(re, threads, pc + 1, sp, subs);
      if (!subs->refs) {
        re_submatches_free(re, subs);
      }
      return;
    }
  }
  threads->at[threads->size++] = re_thread(pc, re_submatches_link(subs));
}

static int re_program_run(struct re *re, char *input, char ***saved, int *nsaved) {
  int matched = RE_ERROR_NOMATCH;
  if (!re) {
    return matched;
  }

  RE_Submatches *submatches = 0;
  RE_ThreadList a = { 0, 0 }, b = { 0, 0 }, *here = &a, *next = &b;

  char *sp = input;
  re->position = 0;

  jmp_buf env;
  re->error_env = &env;

  if (setjmp(env)) {  /* out of memory */
    matched = re->error_code;
    goto bailout;
  }

  a.at = RE_CALLOC(re, re->code.size, sizeof(RE_Thread));
  b.at = RE_CALLOC(re, re->code.size, sizeof(RE_Thread));

  re_thread_schedule(re, here, re->code.first, input, 0);

  {
    int i;
    for (i = 0; i < re->code.size; ++i)
      re->code.first[i].stamp = 0;
  }

  for (sp = input; here->size; ++sp) {
    int i;
    for (i = 0; i < here->size; ++i) {
      RE_Thread t = here->at[i];
      switch (t.pc->opcode) {
        case RE_Any: {
          if (*sp) {
            re_thread_schedule(re, next, t.pc + 1, sp + 1, t.submatches);
          }
          break;
        }
        case RE_Char: {
          if (*sp == t.pc->x) {
            re_thread_schedule(re, next, t.pc + 1, sp + 1, t.submatches);
          }
          break;
        }
        case RE_Class: {
          if (re_bitset_includes(t.pc->c, *sp)) {
            re_thread_schedule(re, next, t.pc + 1, sp + 1, t.submatches);
          }
          break;
        }
        case RE_Accept: {
          matched = sp - input;
          re_submatches_unlink(re, submatches);
          submatches = re_submatches_link(t.submatches);
          while (i < here->size) re_submatches_unlink(re, here->at[i++].submatches);
          goto nextchar;
        }
        default:
          RE_ERROR(re, ENGINE, "illegal instruction in compiled regular expression (please report this bug)");
      }
      re_submatches_unlink(re, t.submatches);
    }
nextchar:
    ;
    RE_ThreadList *tmp = here;
    here = next;
    next = tmp;
    next->size = 0;
    if (!*sp) {
      break;
    }
  }

bailout:
  re->position = sp;

  {
    int i;
    for (i = 0; i < here->size; ++i)
      re_submatches_unlink(re, here->at[i].submatches);
  }

  RE_FREE(re, a.at);
  RE_FREE(re, b.at);

  if (submatches) {
    if (saved && nsaved && (matched >= 0)) {
      assert(submatches->beginnings.size == submatches->endings.size);
      *nsaved = submatches->beginnings.size * 2;
      *saved = RE_CALLOC(re, *nsaved, sizeof(char*));
      int i;
      for (i = 0; i < *nsaved; i += 2) {
        (*saved)[i + 0] = submatches->beginnings.at[i / 2];
        (*saved)[i + 1] = submatches->endings.at[i / 2];
      }
    }
    re_submatches_unlink(re, submatches);
  }

  return matched;
}

/* public interface */

struct re *lwre_new(const char *expr) {
  struct re *re = RE_CALLOC(0, 1, sizeof(struct re));
  if (re) {
    re->expression = expr;
  }
  return re;
}

int lwre_match(struct re *re, char *input) {
  RE_FREE(re, re->matches);
  re->matches = 0;
  re->nmatches = 0;
  if (!re->expression) {
    return 0;
  }
  if (!re->code.size) {
    re->position = re->expression;
    re->error_code = 0;
    re->error_message = 0;
    re->code = re_compile(re);
    if (re->error_code) {
      return re->error_code;
    }
    re->position = 0;
  }
  return re_program_run(re, input, &re->matches, &re->nmatches);
}

void lwre_release(struct re *re) {
  RE_FREE(re, re->matches);
  if (re->code.first) {
    re_program_free(re, &re->code);
  }
  memset(re, 0, sizeof(*re));
}

void lwre_reset(struct re *re, const char *expression) {
  lwre_release(re);
  re->expression = expression;
}

void lwre_free(struct re *re) {
  lwre_release(re);
  RE_FREE(0, re);
}

/* utility */

static int re_digit(int c, int base) {
  if ((c >= '0') && (c <= '9')) {
    c -= '0';
  } else if ((c >= 'A') && (c <= 'Z')) {
    c -= ('A' - 10);
  } else if ((c >= 'a') && (c <= 'z')) {
    c -= ('a' - 10);
  } else {
    return -1;
  }
  if (c >= base) {
    return -1;
  }
  return c;
}

static int re_byte(char **sp, int least, int most, int base, int liberal) {
  int c = 0;
  char *s = *sp;
  while (s - *sp < most) {
    int d = re_digit(*s, base);
    if (d < 0) {
      break;
    }
    ++s;
    c = c * base + d;
  }
  if (s - *sp < least) {
    if (liberal) {
      return (*sp)[-1];
    }
    --*sp;
    return '\\';
  }
  *sp = s;
  return c;
}

static int re_log2floor(unsigned int n) {
  if (!n) {
    return 0;
  }
  int b = 1;
# define _do(x) if (n >= (1U << x)) (b += x), (n >>= x)
  _do(16);
  _do(8);
  _do(4);
  _do(2);
  _do(1);
# undef _do
  return b;
}

static void re_escape_utf8(char **sp, unsigned int c) {
  char *s = *sp;
  if (c < 128) {
    *s++ = c;
  } else { /* this is good for up to 36 bits of c, which proves that Gordon Bell was right all along */
    int n = re_log2floor((unsigned) c) / 6;
    int m = 6 * n;
    *s++ = (0xff << (7 - n)) + (c >> m);
    while ((m -= 6) >= 0) *s++ = 0x80 + ((c >> m) & 0x3F);
  }
  *sp = s;
}

char *lwre_escape(char *s, int liberal) {
  char *in = s, *out = s;
  int c;
  while ((c = *in++)) {
    int u = 0;
    if ('\\' == c) {
      c = *in++;
      switch (c) {
        case '0':
        case '1':
          c = re_byte(&in, 1, 3, 8, liberal);
          break;
        case '\\':
          //c = '\\';
          break;
        case 'a':
          c = '\a';
          break;
        case 'b':
          c = '\b';
          break;
        case 'f':
          c = '\f';
          break;
        case 'n':
          c = '\n';
          break;
        case 'r':
          c = '\r';
          break;
        case 't':
          c = '\t';
          break;
        case 'U':
          c = re_byte(&in, 8, 8, 16, liberal);
          u = 1;
          break;
        case 'u':
          c = re_byte(&in, 4, 4, 16, liberal);
          u = 1;
          break;
        case 'v':
          c = '\v';
          break;
        case 'x':
          c = re_byte(&in, 1, 2, 16, liberal);
          break;
        default: {
          if (!liberal) { /* pass escape character through unharmed */
            --in;
            c = '\\';
          }
          break;
        }
      }
    }
    if (u) {
      re_escape_utf8(&out, c);
    } else {
      *out++ = c;
    }
  }
  assert(out <= in);
  *out = 0;
  return s;
}

/* testing */

#ifdef LWRE_TEST

#include <stdio.h>

/* echo stdin to stout with ANSI terminal escapes to turn every number red */

int main(int argc, char **argv) {
  static struct re re = RE_INITIALISER("(.*?{[0-9]+})*");
  char buf[1024];
  while (fgets(buf, sizeof(buf), stdin)) {
    int n;
    if (((n = lwre_match(&re, buf)) < 0) && (RE_ERROR_NOMATCH != n)) {
      fprintf(stderr, "%i %ss: %s\n", n, re.error_message, re.position);
      break;
    } else {
      char *p = buf;
      int n = 0;
      while (*p) {
        if ((n < re.nmatches) && (p == re.matches[n])) {
          if (n & 1) {
            printf("\033[0m");            /* end of match: clear all attributes */
          } else {
            printf("\033[1;31m");         /* start of match: bold and foreground red */
          }
          ++n;
        }
        putchar(*p++);
      }
    }
  }
  lwre_release(&re);
  return 0;
}

#endif /* REGEXP_TEST */
