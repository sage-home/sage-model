#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "memory_scope.h"  /* This includes the complete definition */
#include "memory.h"

/* Test basic allocation functions */
void test_basic_allocations(void) {
    printf("Testing basic allocations...\n");
    
    void *ptr1 = sage_malloc(1024);
    assert(ptr1 != NULL);
    
    void *ptr2 = sage_calloc(10, 100);
    assert(ptr2 != NULL);
    
    void *ptr3 = sage_realloc(ptr1, 2048);
    assert(ptr3 != NULL);
    
    sage_free(ptr2);
    sage_free(ptr3);
    
    printf("Basic allocations: PASSED\n");
}

/* Test memory scope functionality */
void test_memory_scopes(void) {
    printf("Testing memory scopes...\n");
    
    memory_scope_t *scope = memory_scope_create();
    assert(scope != NULL);
    
    // Allocate within scope
    void *ptr1 = memory_scope_malloc(scope, 1024);
    void *ptr2 = memory_scope_calloc(scope, 10, 100);
    assert(ptr1 != NULL && ptr2 != NULL);
    
    // Scope cleanup should free everything
    memory_scope_destroy(scope);
    
    printf("Memory scopes: PASSED\n");
}

/* Test memory tracking (if enabled) */
void test_memory_tracking(void) {
    printf("Testing memory tracking...\n");
    
    memory_tracking_init();
    
    memory_stats_t stats_before = memory_get_stats();
    
    void *ptr = sage_malloc(1000);
    memory_stats_t stats_after = memory_get_stats();
    
    sage_free(ptr);
    memory_stats_t stats_final = memory_get_stats();
    
    #if SAGE_MEMORY_TRACKING
    assert(stats_after.current_allocated > stats_before.current_allocated);
    assert(stats_final.current_allocated == stats_before.current_allocated);
    assert(!memory_check_leaks());
    #else
    /* Suppress unused variable warnings when tracking is disabled */
    (void)stats_before;
    (void)stats_after;
    (void)stats_final;
    printf("Memory tracking disabled in this build - test still passed\n");
    #endif
    
    memory_tracking_cleanup();
    
    printf("Memory tracking: PASSED\n");
}

/* Test legacy compatibility functions */
void test_legacy_compatibility(void) {
    printf("Testing legacy compatibility...\n");
    
    // Include the legacy header to test the macros
    #include "core_mymalloc.h"
    
    void *ptr1 = mymalloc(1024);
    assert(ptr1 != NULL);
    
    void *ptr2 = mycalloc(10, 100);
    assert(ptr2 != NULL);
    
    void *ptr3 = myrealloc(ptr1, 2048);
    assert(ptr3 != NULL);
    
    myfree(ptr2);
    myfree(ptr3);
    
    printf("Legacy compatibility: PASSED\n");
}

/* Test error handling */
void test_error_handling(void) {
    printf("Testing error handling...\n");
    
    // Test freeing NULL pointer (should not crash)
    sage_free(NULL);
    
    // Test zero-size allocation
    void *ptr = sage_malloc(0);
    assert(ptr == NULL); // Should return NULL for zero-size
    
    printf("Error handling: PASSED\n");
}

int main(void) {
    printf("=== Memory Abstraction Layer Tests ===\n");
    
    test_basic_allocations();
    test_memory_scopes();
    test_memory_tracking();
    test_legacy_compatibility();
    test_error_handling();
    
    printf("All tests PASSED!\n");
    return 0;
}