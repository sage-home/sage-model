#include "memory.h"
#include "core_allvars.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Compile-time configuration */
#ifndef SAGE_MEMORY_TRACKING
#define SAGE_MEMORY_TRACKING 0
#endif

/* Memory tracking structures (only compiled if tracking enabled) */
#if SAGE_MEMORY_TRACKING

typedef struct allocation_info {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    struct allocation_info *next;
} allocation_info_t;

static allocation_info_t *allocation_list = NULL;
static memory_stats_t global_stats = {0};
static bool tracking_initialized = false;

/* Internal tracking functions */
static void track_allocation(void *ptr, size_t size, const char *file, int line) {
    if (!tracking_initialized) return;
    
    allocation_info_t *info = malloc(sizeof(allocation_info_t));
    if (!info) {
        fprintf(stderr, "Warning: Memory tracking disabled due to allocation failure at %s:%d\n", file, line);
        tracking_initialized = false;  /* Disable tracking to prevent further failures */
        return;
    }
    
    info->ptr = ptr;
    info->size = size;
    info->file = file;
    info->line = line;
    info->next = allocation_list;
    allocation_list = info;
    
    global_stats.total_allocated += size;
    global_stats.current_allocated += size;
    global_stats.allocation_count++;
    
    if (global_stats.current_allocated > global_stats.peak_allocated) {
        global_stats.peak_allocated = global_stats.current_allocated;
    }
}

static void track_deallocation(void *ptr) {
    if (!tracking_initialized || !ptr) return;
    
    allocation_info_t **current = &allocation_list;
    while (*current) {
        if ((*current)->ptr == ptr) {
            allocation_info_t *to_remove = *current;
            *current = (*current)->next;
            
            global_stats.current_allocated -= to_remove->size;
            global_stats.deallocation_count++;
            
            free(to_remove);
            return;
        }
        current = &(*current)->next;
    }
    
    fprintf(stderr, "Warning: Attempted to free untracked pointer %p\n", ptr);
}

#endif /* SAGE_MEMORY_TRACKING */

/* Core allocation functions */
void *sage_malloc_impl(size_t size, const char *file, int line) {
    if (size == 0) {
        fprintf(stderr, "Warning: Zero-size allocation at %s:%d\n", file, line);
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Error: Failed to allocate %zu bytes at %s:%d\n", size, file, line);
        ABORT(MALLOC_FAILURE);
    }
    
#if SAGE_MEMORY_TRACKING
    track_allocation(ptr, size, file, line);
#endif
    
    return ptr;
}

void *sage_calloc_impl(size_t count, size_t size, const char *file, int line) {
    if (count == 0 || size == 0) {
        fprintf(stderr, "Warning: Zero-size calloc at %s:%d\n", file, line);
        return NULL;
    }
    
    void *ptr = calloc(count, size);
    if (!ptr) {
        fprintf(stderr, "Error: Failed to allocate %zu*%zu bytes at %s:%d\n", 
                count, size, file, line);
        ABORT(MALLOC_FAILURE);
    }
    
#if SAGE_MEMORY_TRACKING
    track_allocation(ptr, count * size, file, line);
#endif
    
    return ptr;
}

void *sage_realloc_impl(void *ptr, size_t size, const char *file, int line) {
    if (size == 0) {
        sage_free_impl(ptr, file, line);
        return NULL;
    }
    
    if (!ptr) {
        return sage_malloc_impl(size, file, line);
    }
    
#if SAGE_MEMORY_TRACKING
    track_deallocation(ptr);
#endif
    
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "Error: Failed to reallocate %zu bytes at %s:%d\n", 
                size, file, line);
        ABORT(MALLOC_FAILURE);
    }
    
#if SAGE_MEMORY_TRACKING
    track_allocation(new_ptr, size, file, line);
#endif
    
    return new_ptr;
}

void sage_free_impl(void *ptr, const char *file, int line) {
    if (!ptr) return;
    
#if SAGE_MEMORY_TRACKING
    track_deallocation(ptr);
#else
    /* Suppress unused parameter warnings when tracking is disabled */
    (void)file;
    (void)line;
#endif
    
    free(ptr);
}

/* Memory tracking API implementation */
void memory_tracking_init(void) {
#if SAGE_MEMORY_TRACKING
    tracking_initialized = true;
    memset(&global_stats, 0, sizeof(global_stats));
    allocation_list = NULL;
#endif
}

void memory_tracking_cleanup(void) {
#if SAGE_MEMORY_TRACKING
    if (memory_check_leaks()) {
        fprintf(stderr, "Warning: Memory leaks detected during cleanup\n");
    }
    
    /* Free tracking structures */
    while (allocation_list) {
        allocation_info_t *next = allocation_list->next;
        free(allocation_list);
        allocation_list = next;
    }
    
    tracking_initialized = false;
#endif
}

memory_stats_t memory_get_stats(void) {
#if SAGE_MEMORY_TRACKING
    return global_stats;
#else
    memory_stats_t empty_stats = {0};
    return empty_stats;
#endif
}

void memory_print_stats(void) {
#if SAGE_MEMORY_TRACKING
    printf("\n=== Memory Statistics ===\n");
    printf("Total allocated: %zu bytes (%.2f MB)\n", 
           global_stats.total_allocated, 
           global_stats.total_allocated / (1024.0 * 1024.0));
    printf("Peak allocated: %zu bytes (%.2f MB)\n", 
           global_stats.peak_allocated,
           global_stats.peak_allocated / (1024.0 * 1024.0));
    printf("Currently allocated: %zu bytes (%.2f MB)\n", 
           global_stats.current_allocated,
           global_stats.current_allocated / (1024.0 * 1024.0));
    printf("Allocations: %zu, Deallocations: %zu\n", 
           global_stats.allocation_count, global_stats.deallocation_count);
    printf("========================\n");
#else
    printf("Memory tracking not enabled in this build\n");
#endif
}

bool memory_check_leaks(void) {
#if SAGE_MEMORY_TRACKING
    if (!allocation_list) return false;
    
    printf("\n=== Memory Leaks Detected ===\n");
    allocation_info_t *current = allocation_list;
    size_t leak_count = 0;
    size_t leak_bytes = 0;
    
    while (current) {
        printf("Leak: %zu bytes allocated at %s:%d (ptr=%p)\n", 
               current->size, current->file, current->line, current->ptr);
        leak_count++;
        leak_bytes += current->size;
        current = current->next;
    }
    
    printf("Total: %zu leaks, %zu bytes\n", leak_count, leak_bytes);
    printf("=============================\n");
    return true;
#else
    return false;
#endif
}