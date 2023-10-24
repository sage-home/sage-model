/* File: macros.h */

#pragma once
#include <stdio.h>

#define  NDIM             3
#define  STEPS            10         /* Number of integration intervals between two snapshots */
#define  MAXGALFAC        1
#define  ABSOLUTEMAXSNAPS 1000  /* The largest number of snapshots for any simulation */

#define  GRAVITY     6.672e-8
#define  SOLAR_MASS  1.989e33
#define  SOLAR_LUM   3.826e33
#define  RAD_CONST   7.565e-15
#define  AVOGADRO    6.0222e23
#define  BOLTZMANN   1.3806e-16
#define  GAS_CONST   8.31425e7
#define  C           2.9979e10
#define  PLANCK      6.6262e-27
#define  CM_PER_MPC  3.085678e24
#define  PROTONMASS  1.6726e-24
#define  HUBBLE      3.2407789e-18   /* in h/sec */

#define  SEC_PER_MEGAYEAR   3.155e13
#define  SEC_PER_YEAR       3.155e7

#define  MAX_STRING_LEN     1024 /* Max length of a string containing a name */
/* End of Macro Constants */


/* Function-like macros */
#define  SQR(X)      ((X) * (X))
#define  CUBE(X)     ((X) * (X) * (X))

#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)

#define ADD_DIFF_TIME(t0, t1) ((t1.tv_sec - t0.tv_sec) + 1e-6 * (t1.tv_usec - t0.tv_usec))
#define REALTIME_ELAPSED_NS(t0, t1)                                                                \
  ((t1.tv_sec - t0.tv_sec) * 1000000000.0 + (t1.tv_nsec - t0.tv_nsec))

/* Taken from
   http://stackoverflow.com/questions/19403233/compile-time-struct-size-check-error-out-if-odd which
   is in turn taken from the linux kernel */
/* #define BUILD_BUG_OR_ZERO(e) (sizeof(struct{ int:-!!(e);})) */
/* #define ENSURE_STRUCT_SIZE(e, size)  BUILD_BUG_OR_ZERO(sizeof(e) != size) */
/* However, the previous one gives me an unused-value warning and I do not want
   to turn that compiler warning off. Therefore, this version, which results in
   an unused local typedef warning is used. I turn off the corresponding warning
   in common.mk (-Wno-unused-local-typedefs) via CFLAGS
*/
#define BUILD_BUG_OR_ZERO(cond, msg) typedef volatile char assertion_on_##msg[(!!(cond)) * 2 - 1]
#define ENSURE_STRUCT_SIZE(e, size)                                                                \
  BUILD_BUG_OR_ZERO(sizeof(e) == size, sizeof_struct_config_options)


#define ABORT(sigterm)                             \
    do {                                                                \
        fprintf(stderr, "Error in file: %s\tfunc: %s\tline: %i\n", __FILE__, __FUNCTION__, __LINE__); \
        fprintf(stderr, "exit code = %d\n", sigterm);                   \
        fprintf(stderr, "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page.\n" \
                "https://github.com/sage-home/sage-model/issues/new\n"); \
        perror("Printing the output of perror (which may be useful if this was a system error) -- ");                                                   \
        exit(sigterm);                                                  \
    } while(0)


#ifdef NDEBUG
#define XASSERT(EXP, EXIT_STATUS, ...)                                  \
    do {                                                                \
    } while (0)
#else
#define XASSERT(EXP, EXIT_STATUS, ...)                                  \
    do {                                                                \
        if (!(EXP)) {                                                   \
            fprintf(stderr, "Error in file: %s\tfunc: %s\tline: %d with expression `" #EXP "'\n", \
                    __FILE__, __FUNCTION__, __LINE__);                  \
            fprintf(stderr, __VA_ARGS__);                               \
            fprintf(stderr, "If the fix to this isn't obvious, please feel free to open an issue on our GitHub page.\n" \
                            "https://github.com/sage-home/sage-model/issues/new\n"); \
            ABORT(EXIT_STATUS);                                         \
        }                                                               \
  } while (0)
#endif

#ifdef NDEBUG
#define XPRINT(EXP, ...)                                                \
    do {                                                                \
    } while (0)
#else
#define XPRINT(EXP, ...)                                                \
    do {                                                                \
        if (!(EXP)) {                                                   \
            fprintf(stderr, "Error in file: %s\tfunc: %s\tline: %d with expression `" #EXP "'\n", \
                    __FILE__, __FUNCTION__, __LINE__);                  \
            fprintf(stderr, __VA_ARGS__);                               \
        }                                                               \
    } while (0)
#endif

#ifdef NDEBUG
#define XRETURN(EXP, VAL, ...)                                                                     \
  do {                                                                                             \
  } while (0)
#else
#define XRETURN(EXP, VAL, ...)                                                                     \
  do {                                                                                             \
    if (!(EXP)) {                                                                                  \
      fprintf(stderr, "Error in file: %s\tfunc: %s\tline: %d with expression `" #EXP "'\n",        \
              __FILE__, __FUNCTION__, __LINE__);                                                   \
      fprintf(stderr, __VA_ARGS__);                                                                \
      return VAL;                                                                                  \
    }                                                                                              \
  } while (0)
#endif

#define CHECK_STATUS_AND_RETURN_ON_FAIL(status, return_value, ...) \
    do {                                                           \
        if(status < 0) {                                           \
            fprintf(stderr, __VA_ARGS__);                          \
            return return_value;                                   \
        }                                                          \
  } while (0)

#define CHECK_POINTER_AND_RETURN_ON_NULL(pointer, ...)         \
    do {                                                       \
        if(pointer == NULL) {                                  \
            fprintf(stderr, "Error in file: %s\tfunc: %s\tline: %d'\n",        \
                    __FILE__, __FUNCTION__, __LINE__);         \
            fprintf(stderr, __VA_ARGS__);                      \
            return MALLOC_FAILURE;                             \
        }                                                      \
  } while (0)
