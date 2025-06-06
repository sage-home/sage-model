/**
 * @file test_dynamic_memory_expansion.c
 * @brief Comprehensive unit tests for the dynamic memory expansion system
 *
 * This test suite validates the dynamic memory management system implemented
 * to resolve segfaults and memory allocation failures in SAGE. Tests cover:
 * 
 * - Memory system initialization and cleanup
 * - Dynamic block table expansion under memory pressure
 * - Tree-scoped memory management
 * - Memory pressure detection and automatic expansion
 * - Error handling and boundary conditions
 * - Large allocation scenarios
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

/* Include SAGE core headers */
#include "core_allvars.h"
#include "core_mymalloc.h"
#include "core_logging.h"

/* Test framework macros */
#define TEST_START(name) \
    do { \
        printf("Running test: %s\n", name); \
        test_count++; \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            failed_tests++; \
            return 0; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS\n"); \
        passed_tests++; \
        return 1; \
    } while(0)

/* Global test counters */
static int test_count = 0;
static int passed_tests = 0;
static int failed_tests = 0;

/* Forward declarations */
int test_memory_system_initialization(void);
int test_memory_system_cleanup(void);
int test_basic_allocation_and_free(void);
int test_dynamic_block_table_expansion(void);
int test_tree_memory_scoping(void);
int test_memory_pressure_detection(void);
int test_large_allocation_scenarios(void);
int test_error_handling_edge_cases(void);
int test_memory_fragmentation_patterns(void);
int test_memory_statistics_tracking(void);

/**
 * Test memory system initialization
 */
int test_memory_system_initialization(void) {
    TEST_START("memory_system_initialization");
    
    /* Test initialization */
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system initialization should succeed");
    
    /* Test double initialization (should be safe) */
    status = memory_system_init();
    TEST_ASSERT(status == 0, "Double initialization should be safe");
    
    /* Cleanup for next test */
    memory_system_cleanup();
    
    TEST_PASS();
}

/**
 * Test memory system cleanup
 */
int test_memory_system_cleanup(void) {
    TEST_START("memory_system_cleanup");
    
    /* Initialize first */
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Initialization should succeed before cleanup");
    
    /* Test cleanup */
    memory_system_cleanup();
    
    /* Test multiple cleanups (should be safe) */
    memory_system_cleanup();
    
    TEST_PASS();
}

/**
 * Test basic memory allocation and freeing
 */
int test_basic_allocation_and_free(void) {
    TEST_START("basic_allocation_and_free");
    
    /* Initialize system */
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Test basic allocation */
    void *ptr1 = mymalloc(1024);
    TEST_ASSERT(ptr1 != NULL, "Basic allocation should succeed");
    
    void *ptr2 = mymalloc(2048);
    TEST_ASSERT(ptr2 != NULL, "Second allocation should succeed");
    
    /* Test allocation with description */
    void *ptr3 = mymalloc_full(4096, "test allocation");
    TEST_ASSERT(ptr3 != NULL, "Allocation with description should succeed");
    
    /* Free allocations */
    myfree(ptr1);
    myfree(ptr2);
    myfree(ptr3);
    
    /* Test freeing NULL (should be safe) */
    myfree(NULL);
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Test dynamic block table expansion under pressure
 */
int test_dynamic_block_table_expansion(void) {
    TEST_START("dynamic_block_table_expansion");
    
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Allocate many small blocks to trigger expansion */
    void **ptrs = malloc(15000 * sizeof(void*));
    TEST_ASSERT(ptrs != NULL, "Test array allocation should succeed");
    
    for (int i = 0; i < 15000; i++) {
        ptrs[i] = mymalloc(64);  /* Small allocations to maximize block count */
        TEST_ASSERT(ptrs[i] != NULL, "Each allocation should succeed");
        
        /* Force memory pressure check every 1000 allocations */
        if (i % 1000 == 0) {
            check_memory_pressure_and_expand();
        }
    }
    
    /* Test that we can still allocate after many allocations */
    void *final_ptr = mymalloc(1024);
    TEST_ASSERT(final_ptr != NULL, "Final allocation should succeed after expansion");
    
    /* Free all allocations */
    for (int i = 0; i < 15000; i++) {
        myfree(ptrs[i]);
    }
    myfree(final_ptr);
    free(ptrs);
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Test tree-scoped memory management
 */
int test_tree_memory_scoping(void) {
    TEST_START("tree_memory_scoping");
    
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Allocate some memory before scope */
    void *pre_scope = mymalloc(1024);
    TEST_ASSERT(pre_scope != NULL, "Pre-scope allocation should succeed");
    
    /* Begin tree scope */
    begin_tree_memory_scope();
    
    /* Allocate memory within scope */
    void *scope_ptr1 = mymalloc(2048);
    void *scope_ptr2 = mymalloc(4096);
    void *scope_ptr3 = mymalloc(8192);
    
    TEST_ASSERT(scope_ptr1 != NULL, "Scope allocation 1 should succeed");
    TEST_ASSERT(scope_ptr2 != NULL, "Scope allocation 2 should succeed");
    TEST_ASSERT(scope_ptr3 != NULL, "Scope allocation 3 should succeed");
    
    /* End tree scope - should automatically free scope allocations */
    end_tree_memory_scope();
    
    /* Pre-scope allocation should still be valid, but scope allocations are freed */
    /* We can't directly test that scope allocations are freed without accessing 
       internal state, but we can test that the system is still functional */
    
    void *post_scope = mymalloc(1024);
    TEST_ASSERT(post_scope != NULL, "Post-scope allocation should succeed");
    
    /* Test nested scopes */
    begin_tree_memory_scope();
    void *outer_scope = mymalloc(1024);
    TEST_ASSERT(outer_scope != NULL, "Outer scope allocation should succeed");
    
    begin_tree_memory_scope();
    void *inner_scope = mymalloc(512);
    TEST_ASSERT(inner_scope != NULL, "Inner scope allocation should succeed");
    end_tree_memory_scope();  /* Should only free inner_scope */
    
    void *after_inner = mymalloc(256);
    TEST_ASSERT(after_inner != NULL, "Allocation after inner scope should succeed");
    
    end_tree_memory_scope();  /* Should free outer_scope and after_inner */
    
    /* Clean up remaining allocations */
    myfree(pre_scope);
    myfree(post_scope);
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Test memory pressure detection and automatic expansion
 */
int test_memory_pressure_detection(void) {
    TEST_START("memory_pressure_detection");
    
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Test explicit expansion */
    status = expand_block_table();
    TEST_ASSERT(status == 0, "Explicit block table expansion should succeed");
    
    /* Test pressure detection (function should complete without error) */
    check_memory_pressure_and_expand();
    
    /* Test multiple expansions */
    for (int i = 0; i < 5; i++) {
        status = expand_block_table();
        TEST_ASSERT(status == 0, "Multiple expansions should succeed");
    }
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Test large allocation scenarios
 */
int test_large_allocation_scenarios(void) {
    TEST_START("large_allocation_scenarios");
    
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Test progressively larger allocations */
    size_t sizes[] = {1024*1024, 5*1024*1024, 10*1024*1024, 50*1024*1024};
    void *ptrs[4] = {NULL};
    
    for (int i = 0; i < 4; i++) {
        ptrs[i] = mymalloc_full(sizes[i], "large allocation test");
        if (ptrs[i] != NULL) {
            printf("  Successfully allocated %.2f MB\n", sizes[i] / (1024.0 * 1024.0));
        } else {
            printf("  Failed to allocate %.2f MB (may be system limit)\n", sizes[i] / (1024.0 * 1024.0));
        }
    }
    
    /* Free successful allocations */
    for (int i = 0; i < 4; i++) {
        if (ptrs[i] != NULL) {
            myfree(ptrs[i]);
        }
    }
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Test error handling and edge cases
 */
int test_error_handling_edge_cases(void) {
    TEST_START("error_handling_edge_cases");
    
    /* Test operations without initialization */
    check_memory_pressure_and_expand();  /* Should be safe */
    begin_tree_memory_scope();           /* Should log warning */
    end_tree_memory_scope();             /* Should log warning */
    
    /* Now initialize properly */
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Test zero allocation */
    void *zero_ptr = mymalloc(0);
    TEST_ASSERT(zero_ptr != NULL, "Zero allocation should succeed (gets aligned to 8 bytes)");
    myfree(zero_ptr);
    
    /* Test very small allocation */
    void *tiny_ptr = mymalloc(1);
    TEST_ASSERT(tiny_ptr != NULL, "Tiny allocation should succeed");
    myfree(tiny_ptr);
    
    /* Test realloc functionality */
    void *realloc_ptr = mymalloc(1024);
    TEST_ASSERT(realloc_ptr != NULL, "Initial allocation for realloc test should succeed");
    
    void *new_ptr = myrealloc(realloc_ptr, 2048);
    TEST_ASSERT(new_ptr != NULL, "Realloc should succeed");
    myfree(new_ptr);
    
    /* Test calloc functionality */
    void *calloc_ptr = mycalloc(100, 10);
    TEST_ASSERT(calloc_ptr != NULL, "Calloc should succeed");
    
    /* Verify calloc zeroing */
    char *char_ptr = (char*)calloc_ptr;
    int all_zero = 1;
    for (int i = 0; i < 1000; i++) {
        if (char_ptr[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    TEST_ASSERT(all_zero, "Calloc should zero-initialize memory");
    myfree(calloc_ptr);
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Test memory fragmentation patterns
 */
int test_memory_fragmentation_patterns(void) {
    TEST_START("memory_fragmentation_patterns");
    
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Allocate and free in patterns that might cause fragmentation */
    void *ptrs[1000];
    
    /* Allocate many blocks */
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = mymalloc(1024 + (i % 100));  /* Variable sizes */
    }
    
    /* Free every other block */
    for (int i = 1; i < 1000; i += 2) {
        myfree(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    /* Allocate new blocks in the gaps */
    for (int i = 1; i < 1000; i += 2) {
        ptrs[i] = mymalloc(512);
        TEST_ASSERT(ptrs[i] != NULL, "Allocation in fragmented space should succeed");
    }
    
    /* Free all remaining blocks */
    for (int i = 0; i < 1000; i++) {
        if (ptrs[i] != NULL) {
            myfree(ptrs[i]);
        }
    }
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Test memory statistics and monitoring
 */
int test_memory_statistics_tracking(void) {
    TEST_START("memory_statistics_tracking");
    
    int status = memory_system_init();
    TEST_ASSERT(status == 0, "Memory system should initialize");
    
    /* Print initial statistics */
    printf("  Initial statistics:\n");
    print_memory_stats();
    
    /* Allocate some memory */
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = mymalloc((i + 1) * 1024);
    }
    
    /* Print statistics after allocation */
    printf("  After allocations:\n");
    print_memory_stats();
    
    /* Free memory */
    for (int i = 0; i < 10; i++) {
        myfree(ptrs[i]);
    }
    
    /* Print final statistics */
    printf("  After freeing:\n");
    print_memory_stats();
    
    memory_system_cleanup();
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("Dynamic Memory Expansion Test Suite\n");
    printf("===================================\n\n");
    
    /* Suppress some logging for cleaner test output */
    /* Note: In a real test environment, you might want to redirect logging */
    
    /* Run all tests */
    test_memory_system_initialization();
    test_memory_system_cleanup();
    test_basic_allocation_and_free();
    test_dynamic_block_table_expansion();
    test_tree_memory_scoping();
    test_memory_pressure_detection();
    test_large_allocation_scenarios();
    test_error_handling_edge_cases();
    test_memory_fragmentation_patterns();
    test_memory_statistics_tracking();
    
    /* Print test summary */
    printf("\n===================================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", test_count);
    printf("  Passed: %d\n", passed_tests);
    printf("  Failed: %d\n", failed_tests);
    
    if (failed_tests == 0) {
        printf("  Result: ALL TESTS PASSED ✓\n");
        return 0;
    } else {
        printf("  Result: %d TESTS FAILED ✗\n", failed_tests);
        return 1;
    }
}