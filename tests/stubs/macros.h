#ifndef MACROS_STUBS_H
#define MACROS_STUBS_H

#include <stdio.h>
#include <stdlib.h>

#define ABORT(x) { fprintf(stderr, "ABORTING"); exit(x); }
#define XASSERT(expr, retval, ...) if(!(expr)) { fprintf(stderr, __VA_ARGS__); return retval; }
#define XRETURN(expr, retval, ...) if(!(expr)) { fprintf(stderr, __VA_ARGS__); return retval; }

#endif /* MACROS_STUBS_H */
