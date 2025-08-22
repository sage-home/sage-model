#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory allocation abstractions */
void *sage_malloc_impl(size_t size, const char *file, int line);
void *sage_calloc_impl(size_t count, size_t size, const char *file, int line);
void *sage_realloc_impl(void *ptr, size_t size, const char *file, int line);
void sage_free_impl(void *ptr, const char *file, int line);

/* Convenience macros with automatic file/line tracking */
#define sage_malloc(size) sage_malloc_impl(size, __FILE__, __LINE__)
#define sage_calloc(count, size) sage_calloc_impl(count, size, __FILE__, __LINE__)
#define sage_realloc(ptr, size) sage_realloc_impl(ptr, size, __FILE__, __LINE__)
#define sage_free(ptr) sage_free_impl(ptr, __FILE__, __LINE__)

/* Memory tracking and statistics (optional, build-time controlled) */
typedef struct {
    size_t total_allocated;
    size_t peak_allocated;
    size_t current_allocated;
    size_t allocation_count;
    size_t deallocation_count;
} memory_stats_t;

/* Memory tracking control functions */
void memory_tracking_init(void);
void memory_tracking_cleanup(void);
memory_stats_t memory_get_stats(void);
void memory_print_stats(void);
bool memory_check_leaks(void);

/* Memory scope foundation for Phase 3 RAII patterns */
/* Forward declaration - full definition in memory_scope.h */
struct memory_scope;

#ifdef __cplusplus
}
#endif