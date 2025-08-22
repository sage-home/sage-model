#include "memory_scope.h"
#include "memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INITIAL_SCOPE_CAPACITY 32

memory_scope_t *memory_scope_create(void) {
    memory_scope_t *scope = sage_malloc(sizeof(memory_scope_t));
    
    scope->allocations = sage_malloc(INITIAL_SCOPE_CAPACITY * sizeof(void*));
    scope->count = 0;
    scope->capacity = INITIAL_SCOPE_CAPACITY;
    scope->parent = NULL;
    
    return scope;
}

void memory_scope_destroy(memory_scope_t *scope) {
    if (!scope) return;
    
    memory_scope_cleanup_all(scope);
    sage_free(scope->allocations);
    sage_free(scope);
}

void memory_scope_register_allocation(memory_scope_t *scope, void *ptr) {
    if (!scope || !ptr) return;
    
    if (scope->count >= scope->capacity) {
        size_t new_capacity = scope->capacity * 2;
        void **new_allocations = sage_realloc(scope->allocations, 
                                              new_capacity * sizeof(void*));
        if (!new_allocations) {
            fprintf(stderr, "Error: Failed to expand memory scope capacity\n");
            return;  /* Fail the registration gracefully */
        }
        scope->allocations = new_allocations;
        scope->capacity = new_capacity;
    }
    
    scope->allocations[scope->count++] = ptr;
}

void memory_scope_cleanup_all(memory_scope_t *scope) {
    if (!scope) return;
    
    for (size_t i = 0; i < scope->count; i++) {
        sage_free(scope->allocations[i]);
    }
    
    scope->count = 0;
}

void *memory_scope_malloc(memory_scope_t *scope, size_t size) {
    void *ptr = sage_malloc(size);
    memory_scope_register_allocation(scope, ptr);
    return ptr;
}

void *memory_scope_calloc(memory_scope_t *scope, size_t count, size_t size) {
    void *ptr = sage_calloc(count, size);
    memory_scope_register_allocation(scope, ptr);
    return ptr;
}