#ifndef __lwre_h_
#define __lwre_h_

#include <setjmp.h>

struct RE_Insn;

struct RE_Compiled
{
    int			 size;
    struct RE_Insn	*first;
    struct RE_Insn	*last;
};

#define RE_COMPILED_INITIALISER	{ 0, 0, 0 }

struct re
{
    char		 *expression;
    char		 *position;
    jmp_buf		 *error_env;
    int			  error_code;
    char		 *error_message;
    struct RE_Compiled	  code;
    char		**matches;
    int			  nmatches;
#ifdef RE_EXTRA_MEMBERS
    RE_MEMBERS
#endif
};

#define RE_INITIALISER(EXPR)	{ (EXPR), 0, 0, 0, 0, RE_COMPILED_INITIALISER, 0, 0 }

enum {
    RE_ERROR_NONE	=  0,
    RE_ERROR_NOMATCH	= -1,
    RE_ERROR_NOMEM	= -2,
    RE_ERROR_CHARSET	= -3,
    RE_ERROR_SUBEXP	= -4,
    RE_ERROR_SUBMATCH	= -5,
    RE_ERROR_ENGINE	= -6
};

struct re *re_new(char *expression);
int	   re_match(struct re *re, char *input);
void	   re_release(struct re *re);
void	   re_reset(struct re *re, char *expression);
void	   re_free(struct re *re);

char *re_escape(char *string, int liberal);

#endif /* __lwre_h_ */
