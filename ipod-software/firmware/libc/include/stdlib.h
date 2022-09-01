/*
 * stdlib.h
 *
 * Definitions for common types, variables, and functions.
 */

#ifndef _STDLIB_H_
#ifdef __cplusplus
extern "C" {
#endif
#define _STDLIB_H_

#include "_ansi.h"

#define __need_size_t
#define __need_wchar_t
#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

_VOID   _EXFUN(qsort,(_PTR __base, size_t __nmemb, size_t __size, int(*_compar)(const _PTR, const _PTR)));

void *malloc(size_t);
void *calloc (size_t nmemb, size_t size);
void free(void *);
void *realloc(void *, size_t);
int  atexit(void (*)(void));

#ifdef  __GNUC__
# undef  alloca
# define alloca(size)   __builtin_alloca (size)
#endif /* GCC.  */

#define RAND_MAX INT_MAX

void srand(unsigned int seed);
int rand(void);

#ifndef ABS
#if defined(__GNUC__)
#define ABS(a) ({typeof (a) ___a = (a); ___a < 0 ? -___a: ___a; })
#else
#define ABS(a) (((a) < 0) ? -(a) : (a))
#endif /* __GNUC__ */
#endif

#define abs(x) ((int)ABS(x))
#define labs(x) ((long)abs(x))

#ifdef SIMULATOR
void exit(int status);
#endif

int atoi (const char *str);

#ifdef __cplusplus
}
#endif
#endif /* _STDLIB_H_ */
