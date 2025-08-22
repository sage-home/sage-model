#pragma once

/* Legacy compatibility layer - forwards to new memory abstraction */
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Legacy API - now implemented as wrappers */
#define mymalloc(size) sage_malloc(size)
#define mycalloc(count, size) sage_calloc(count, size)
#define myrealloc(ptr, size) sage_realloc(ptr, size)
#define myfree(ptr) sage_free(ptr)

/* Legacy debugging function */
#ifdef VERBOSE
#define print_allocated() memory_print_stats()
#endif

/* Deprecation warnings (optional, controlled by build) */
#ifdef SAGE_WARN_LEGACY_MEMORY
#pragma message("Warning: Using legacy memory API. Migrate to sage_malloc/sage_free.")
#endif

#ifdef __cplusplus
}
#endif
