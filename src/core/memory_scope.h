#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory scope for RAII-style cleanup */
typedef struct memory_scope {
    void **allocations;
    size_t count;
    size_t capacity;
    struct memory_scope *parent;
} memory_scope_t;

/* Memory scope lifecycle */
memory_scope_t *memory_scope_create(void);
void memory_scope_destroy(memory_scope_t *scope);

/* Memory scope management */
void memory_scope_register_allocation(memory_scope_t *scope, void *ptr);
void memory_scope_cleanup_all(memory_scope_t *scope);

/* Scoped allocation convenience functions */
void *memory_scope_malloc(memory_scope_t *scope, size_t size);
void *memory_scope_calloc(memory_scope_t *scope, size_t count, size_t size);

#ifdef __cplusplus
}
#endif