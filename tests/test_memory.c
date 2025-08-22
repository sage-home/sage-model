/**
 * Unit test for Memory Abstraction Layer
 * 
 * Tests cover:
 * - Basic allocation functions (malloc, calloc, realloc, free)
 * - Memory scope functionality for automatic cleanup
 * - Memory tracking capabilities (when enabled)
 * - Legacy compatibility functions
 * - Error handling and edge cases
 * - Scope capacity expansion and resilience
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "memory_scope.h"  /* This includes the complete definition */
#include "memory.h"

/**
 * Test: Basic allocation functions
 */
static int test_basic_allocations(void) {
    printf("  Testing basic allocation functions...\n");
    
    void *ptr1 = sage_malloc(1024);
    assert(ptr1 != NULL);
    
    void *ptr2 = sage_calloc(10, 100);
    assert(ptr2 != NULL);
    
    void *ptr3 = sage_realloc(ptr1, 2048);
    assert(ptr3 != NULL);
    
    sage_free(ptr2);
    sage_free(ptr3);
    
    printf("    SUCCESS: Basic allocation functions work\n");
    return 0;
}

/**
 * Test: Memory scope functionality
 */
static int test_memory_scopes(void) {
    printf("  Testing memory scope functionality...\n");
    
    memory_scope_t *scope = memory_scope_create();
    assert(scope != NULL);
    
    // Allocate within scope
    void *ptr1 = memory_scope_malloc(scope, 1024);
    void *ptr2 = memory_scope_calloc(scope, 10, 100);
    assert(ptr1 != NULL && ptr2 != NULL);
    
    // Scope cleanup should free everything
    memory_scope_destroy(scope);
    
    printf("    SUCCESS: Memory scope functionality works\n");
    return 0;
}

/**
 * Test: Memory tracking capabilities
 */
static int test_memory_tracking(void) {
    printf("  Testing memory tracking capabilities...\n");
    
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
    printf("    SUCCESS: Memory tracking works correctly\n");
    #else
    /* Suppress unused variable warnings when tracking is disabled */
    (void)stats_before;
    (void)stats_after;
    (void)stats_final;
    printf("    SUCCESS: Memory tracking disabled in this build - test passed\n");
    #endif
    
    memory_tracking_cleanup();
    
    return 0;
}

/**
 * Test: Legacy compatibility functions
 */
static int test_legacy_compatibility(void) {
    printf("  Testing legacy compatibility functions...\n");
    
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
    
    printf("    SUCCESS: Legacy compatibility functions work\n");
    return 0;
}

/**
 * Test: Error handling
 */
static int test_error_handling(void) {
    printf("  Testing error handling...\n");
    
    // Test freeing NULL pointer (should not crash)
    sage_free(NULL);
    
    // Test zero-size allocation
    void *ptr = sage_malloc(0);
    assert(ptr == NULL); // Should return NULL for zero-size
    
    printf("    SUCCESS: Error handling works correctly\n");
    return 0;
}

/**
 * Test: Scope capacity expansion
 */
static int test_scope_capacity_expansion(void) {
    printf("  Testing scope capacity expansion...\n");
    
    memory_scope_t *scope = memory_scope_create();
    assert(scope != NULL);
    
    // Fill the scope beyond initial capacity to trigger expansion
    // Initial capacity is 32, so we'll allocate 40 pointers
    for (int i = 0; i < 40; i++) {
        void *ptr = memory_scope_malloc(scope, 100);
        assert(ptr != NULL);
        // The allocation should be registered even after capacity expansion
    }
    
    // Verify all allocations are tracked
    assert(scope->count == 40);
    assert(scope->capacity >= 40);  // Should have expanded
    
    memory_scope_destroy(scope);
    
    printf("    SUCCESS: Scope capacity expansion works\n");
    return 0;
}

/**
 * Test: Scope registration resilience
 */
static int test_scope_registration_resilience(void) {
    printf("  Testing scope registration resilience...\n");
    
    memory_scope_t *scope = memory_scope_create();
    assert(scope != NULL);
    
    // Test that NULL pointer registration is handled gracefully
    memory_scope_register_allocation(scope, NULL);
    assert(scope->count == 0);  // Should not increment count for NULL
    
    // Test that registration with NULL scope is handled gracefully
    void *test_ptr = sage_malloc(100);
    memory_scope_register_allocation(NULL, test_ptr);
    // Should not crash
    sage_free(test_ptr);
    
    memory_scope_destroy(scope);
    
    printf("    SUCCESS: Scope registration resilience works\n");
    return 0;
}

int main(void) {
    printf("\nRunning %s...\n", __FILE__);
    printf("\n=== Testing Memory Abstraction Layer ===\n");
    
    printf("This test verifies:\n");
    printf("  1. Basic allocation functions work correctly\n");
    printf("  2. Memory scopes provide automatic cleanup\n");
    printf("  3. Memory tracking operates when enabled\n");
    printf("  4. Legacy compatibility functions work\n");
    printf("  5. Error handling is robust\n");
    printf("  6. Scope capacity expansion works properly\n");
    printf("  7. Registration resilience handles edge cases\n\n");

    int result = 0;
    int test_count = 0;
    int passed_count = 0;
    
    // Run tests
    test_count++;
    if (test_basic_allocations() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_memory_scopes() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_memory_tracking() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_legacy_compatibility() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_error_handling() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_scope_capacity_expansion() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    test_count++;
    if (test_scope_registration_resilience() == 0) {
        passed_count++;
    } else {
        result = 1;
    }
    
    // Report results
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", passed_count, test_count);
    
    if (result == 0) {
        printf("%s PASSED\n", __FILE__);
    } else {
        printf("%s FAILED\n", __FILE__);
    }
    
    return result;
}