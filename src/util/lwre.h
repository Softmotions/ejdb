#ifndef __lwre_h_
#define __lwre_h_

#include <setjmp.h>

struct RE_Insn;

struct RE_Compiled {
  int      size;
  struct RE_Insn  *first;
  struct RE_Insn  *last;
};

#define RE_COMPILED_INITIALISER { 0, 0, 0 }

struct re {
  const char     *expression;
  const char     *position;
  jmp_buf  *error_env;
  int       error_code;
  char     *error_message;
  struct RE_Compiled    code;
  char    **matches;
  int       nmatches;
#ifdef RE_EXTRA_MEMBERS
  RE_MEMBERS
#endif
};

#define RE_INITIALISER(EXPR)  { (EXPR), 0, 0, 0, 0, RE_COMPILED_INITIALISER, 0, 0 }

#define RE_ERROR_NONE       0
#define RE_ERROR_NOMATCH    -1
#define RE_ERROR_NOMEM      -2
#define RE_ERROR_CHARSET    -3
#define RE_ERROR_SUBEXP     -4
#define RE_ERROR_SUBMATCH   -5
#define RE_ERROR_ENGINE     -6

struct re *lwre_new(const char *expression);
int    lwre_match(struct re *re, char *input);
void   lwre_release(struct re *re);
void   lwre_reset(struct re *re, const char *expression);
void   lwre_free(struct re *re);

char *lwre_escape(char *string, int liberal);

#endif /* __lwre_h_ */
