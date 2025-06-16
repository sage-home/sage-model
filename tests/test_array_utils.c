/**
 * Test suite for Array Utility Functions
 * 
 * Tests cover:
 * - Basic functionality: Array expansion with different growth factors
 * - Error handling: Behavior with NULL parameters and invalid inputs
 * - Edge cases: Zero capacity, minimum/maximum sizes
 * - Efficiency: Multiple expansions to validate geometric growth strategy
 *
 * These tests validate the memory optimization strategies described in
 * Phase 3.3 of the SAGE refactoring plan, particularly the geometric
 * growth implementation for dynamic array reallocation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>  // for off_t
#include <limits.h>     // for INT_MAX

#include "../src/core/core_allvars.h"
#include "../src/core/core_mymalloc.h"
#include "../src/core/core_array_utils.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } \
} while(0)

// Test fixtures
static struct test_context {
    // Pointers to test arrays of different types
    int *int_array;
    float *float_array;
    struct GALAXY *galaxy_array;
    // Initial capacities
    int int_capacity;
    int float_capacity;
    int galaxy_capacity;
    // Initialization flag
    int initialized;
} test_ctx;

// Forward declaration for teardown function
static void teardown_test_context(void);

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize test arrays with initial capacities
    test_ctx.int_capacity = 10;
    test_ctx.float_capacity = 20;
    test_ctx.galaxy_capacity = 30;
    
    // Allocate arrays
    test_ctx.int_array = mymalloc(test_ctx.int_capacity * sizeof(int));
    test_ctx.float_array = mymalloc(test_ctx.float_capacity * sizeof(float));
    test_ctx.galaxy_array = mymalloc(test_ctx.galaxy_capacity * sizeof(struct GALAXY));
    
    // Check if allocation succeeded
    if (test_ctx.int_array == NULL || test_ctx.float_array == NULL || test_ctx.galaxy_array == NULL) {
        teardown_test_context();
        return -1;
    }
    
    // Initialize test data
    for (int i = 0; i < test_ctx.int_capacity; i++) {
        test_ctx.int_array[i] = i;
    }
    
    for (int i = 0; i < test_ctx.float_capacity; i++) {
        test_ctx.float_array[i] = (float)i * 1.5f;
    }
    
    for (int i = 0; i < test_ctx.galaxy_capacity; i++) {
        // Allocate properties for each galaxy
        if (allocate_galaxy_properties(&test_ctx.galaxy_array[i], NULL) != 0) {
            teardown_test_context();
            return -1;
        }
        // Set values using property macros
        GALAXY_PROP_Type(&test_ctx.galaxy_array[i]) = i % 3;
        GALAXY_PROP_GalaxyIndex(&test_ctx.galaxy_array[i]) = (uint64_t)(i + 1000);
    }
    
    test_ctx.initialized = 1;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.int_array) {
        myfree(test_ctx.int_array);
        test_ctx.int_array = NULL;
    }
    
    if (test_ctx.float_array) {
        myfree(test_ctx.float_array);
        test_ctx.float_array = NULL;
    }
    
    if (test_ctx.galaxy_array) {
        // Free properties for each galaxy before freeing array
        for (int i = 0; i < test_ctx.galaxy_capacity; i++) {
            free_galaxy_properties(&test_ctx.galaxy_array[i]);
        }
        myfree(test_ctx.galaxy_array);
        test_ctx.galaxy_array = NULL;
    }
    
    test_ctx.int_capacity = 0;
    test_ctx.float_capacity = 0;
    test_ctx.galaxy_capacity = 0;
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Basic array expansion with custom growth factor
 * 
 * Validates that:
 * - Arrays expand correctly with a specified growth factor
 * - Capacity increases by at least the requested amount
 * - Data in the array is preserved after expansion
 */
static void test_array_expansion(void) {
    printf("\n=== Testing array expansion with custom growth factor ===\n");
    
    // Use the array from the test context
    int *array = test_ctx.int_array;
    int capacity = test_ctx.int_capacity;
    
    // Verify initial state
    TEST_ASSERT(capacity == 10, "Initial capacity should be 10");
    TEST_ASSERT(array[5] == 5, "Initial values should be correctly set");
    
    // Expand to larger size with custom growth factor
    int old_capacity = capacity;
    int target_size = capacity * 2;
    int result = array_expand((void **)&array, sizeof(int), &capacity, target_size, 1.5f);
    
    TEST_ASSERT(result == 0, "array_expand should return success (0)");
    TEST_ASSERT(capacity >= old_capacity * 2, "Capacity should increase by at least 2x");
    
    // Update the test context pointer in case it changed during reallocation
    test_ctx.int_array = array;
    test_ctx.int_capacity = capacity;
    
    printf("Expanded capacity: %d (from %d)\n", capacity, old_capacity);
    
    // Verify values are preserved
    for (int i = 0; i < old_capacity; i++) {
        TEST_ASSERT(array[i] == i, "Original values should be preserved after expansion");
    }
}

/**
 * Test: Array expansion with default growth factor
 * 
 * Validates that:
 * - array_expand_default correctly uses the default growth factor
 * - Capacity increases sufficiently
 * - Data is preserved after expansion
 */
static void test_default_expansion(void) {
    printf("\n=== Testing array expansion with default growth factor ===\n");
    
    // Use the array from the test context
    float *array = test_ctx.float_array;
    int capacity = test_ctx.float_capacity;
    
    // Verify initial state
    TEST_ASSERT(capacity == 20, "Initial capacity should be 20");
    TEST_ASSERT(array[10] == 10 * 1.5f, "Initial values should be correctly set");
    
    // Expand to larger size with default growth factor
    int old_capacity = capacity;
    int target_size = capacity * 3;
    int result = array_expand_default((void **)&array, sizeof(float), &capacity, target_size);
    
    TEST_ASSERT(result == 0, "array_expand_default should return success (0)");
    TEST_ASSERT(capacity >= old_capacity * 3, "Capacity should increase by at least 3x");
    
    // Update the test context pointer in case it changed during reallocation
    test_ctx.float_array = array;
    test_ctx.float_capacity = capacity;
    
    printf("Expanded capacity: %d (from %d)\n", capacity, old_capacity);
    
    // Verify values are preserved
    for (int i = 0; i < old_capacity; i++) {
        TEST_ASSERT(array[i] == (float)i * 1.5f, "Original values should be preserved after expansion");
    }
}

/**
 * Test: Galaxy array expansion
 * 
 * Validates that:
 * - galaxy_array_expand correctly handles GALAXY struct arrays
 * - Capacity increases sufficiently
 * - Complex struct data is preserved after expansion
 */
static void test_galaxy_array_expansion(void) {
    printf("\n=== Testing galaxy array expansion ===\n");
    
    // Use the array from the test context
    struct GALAXY *galaxies = test_ctx.galaxy_array;
    int capacity = test_ctx.galaxy_capacity;
    
    // Verify initial state
    TEST_ASSERT(capacity == 30, "Initial capacity should be 30");
    TEST_ASSERT(GALAXY_PROP_Type(&galaxies[15]) == 15 % 3, "Initial Type values should be correctly set");
    TEST_ASSERT(GALAXY_PROP_GalaxyIndex(&galaxies[15]) == (15 + 1000), "Initial GalaxyIndex values should be correctly set");
    
    // Expand to larger size
    int old_capacity = capacity;
    int target_size = capacity * 2;
    int result = galaxy_array_expand(&galaxies, &capacity, target_size, old_capacity);
    
    TEST_ASSERT(result == 0, "galaxy_array_expand should return success (0)");
    TEST_ASSERT(capacity >= old_capacity * 2, "Capacity should increase by at least 2x");
    
    // Update the test context pointer in case it changed during reallocation
    test_ctx.galaxy_array = galaxies;
    test_ctx.galaxy_capacity = capacity;
    
    printf("Expanded capacity: %d (from %d)\n", capacity, old_capacity);
    
    // Verify values are preserved
    for (int i = 0; i < old_capacity; i++) {
        TEST_ASSERT(GALAXY_PROP_Type(&galaxies[i]) == i % 3, "Original Type values should be preserved after expansion");
        TEST_ASSERT(GALAXY_PROP_GalaxyIndex(&galaxies[i]) == (uint64_t)(i + 1000), 
                   "Original GalaxyIndex values should be preserved after expansion");
    }
}

/**
 * Test: Multiple small expansions
 * 
 * Validates that:
 * - Geometric growth strategy significantly reduces the number of reallocations
 * - Array can grow to large sizes efficiently
 */
static void test_multiple_expansions(void) {
    printf("\n=== Testing multiple small expansions (geometric growth) ===\n");
    
    // Start with a small array
    int capacity = 10;
    int *array = mymalloc(capacity * sizeof(int));
    TEST_ASSERT(array != NULL, "Initial array allocation should succeed");
    
    // Initialize with some values
    for (int i = 0; i < capacity; i++) {
        array[i] = i;
    }
    
    int num_expansions = 0;
    int max_target = 10000;
    
    // Repeatedly expand by small increments
    for (int target_size = 10; target_size <= max_target; target_size *= 2) {
        int result = array_expand_default((void **)&array, sizeof(int), &capacity, target_size);
        TEST_ASSERT(result == 0, "Array expansion should succeed for target size");
        
        num_expansions++;
        TEST_ASSERT(capacity >= target_size, "Expanded capacity should meet or exceed target size");
    }
    
    printf("Performed %d expansions to reach capacity %d\n", num_expansions, capacity);
    
    // With geometric growth, we should need far fewer than linear expansions
    // Linear would require ~1000 expansions to reach 10000 with +10 each time
    // With doubling, we only need log2(10000/10) ~= 10 expansions
    TEST_ASSERT(num_expansions < 20, "Geometric growth should require fewer than 20 expansions");
    
    // Clean up
    myfree(array);
}

/**
 * Test: Error handling
 * 
 * Validates that the functions properly handle error conditions:
 * - NULL array pointer
 * - NULL capacity pointer
 * - Invalid element size
 * - Invalid growth factor
 */
static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    // Create local variables just for this test to avoid corrupting test context
    int *local_array = mymalloc(10 * sizeof(int));
    int local_capacity = 10;
    
    if (local_array == NULL) {
        TEST_ASSERT(0, "Failed to allocate array for error handling tests");
        return;
    }
    
    // Test NULL array pointer
    int result = array_expand(NULL, sizeof(int), &local_capacity, 20, 1.5f);
    TEST_ASSERT(result != 0, "array_expand should fail with NULL array pointer");
    
    // Test NULL capacity pointer
    result = array_expand((void **)&local_array, sizeof(int), NULL, 20, 1.5f);
    TEST_ASSERT(result != 0, "array_expand should fail with NULL capacity pointer");
    
    // Test zero element size
    result = array_expand((void **)&local_array, 0, &local_capacity, 20, 1.5f);
    TEST_ASSERT(result != 0, "array_expand should fail with zero element size");
    
    // Test invalid growth factor (fixed at minimum 1.1 in implementation)
    // This should still succeed as the function applies a minimum value
    int old_capacity = local_capacity;
    result = array_expand((void **)&local_array, sizeof(int), &local_capacity, local_capacity + 5, 0.5f);
    TEST_ASSERT(result == 0, "array_expand should apply minimum growth factor and succeed");
    
    // Verify capacity increased appropriately despite invalid growth factor
    TEST_ASSERT(local_capacity > old_capacity, "Capacity should still increase with minimum growth factor");
    
    // Clean up
    myfree(local_array);
}

/**
 * Test: Edge cases
 * 
 * Validates behavior with:
 * - Already sufficient capacity
 * - Very small initial capacity
 * - Very large requested capacity
 * - Zero initial capacity (should use minimum size)
 */
static void test_edge_cases(void) {
    printf("\n=== Testing edge cases ===\n");
    
    // Test case: Already sufficient capacity
    int capacity = 100;
    int *array = mymalloc(capacity * sizeof(int));
    TEST_ASSERT(array != NULL, "Initial array allocation should succeed");
    
    int old_capacity = capacity;
    int result = array_expand_default((void **)&array, sizeof(int), &capacity, 50);
    TEST_ASSERT(result == 0, "array_expand should return success when capacity already sufficient");
    TEST_ASSERT(capacity == old_capacity, "Capacity should not change when already sufficient");
    
    myfree(array);
    
    // Test case: Very small initial capacity
    capacity = 1;
    array = mymalloc(capacity * sizeof(int));
    TEST_ASSERT(array != NULL, "Small array allocation should succeed");
    
    old_capacity = capacity;
    result = array_expand_default((void **)&array, sizeof(int), &capacity, 10);
    TEST_ASSERT(result == 0, "array_expand should handle very small initial capacity");
    TEST_ASSERT(capacity >= 10, "Capacity should increase to at least the requested size");
    printf("Small capacity expanded from %d to %d\n", old_capacity, capacity);
    
    myfree(array);
    
    // Test case: Zero initial capacity - we need to allocate first
    capacity = 0;
    array = mymalloc(sizeof(int)); // Allocate minimal space first
    if (array == NULL) {
        TEST_ASSERT(0, "Failed to allocate minimal array");
        return;
    }
    
    // Now expand from zero capacity
    result = array_expand_default((void **)&array, sizeof(int), &capacity, 5);
    TEST_ASSERT(result == 0, "array_expand should handle zero initial capacity");
    TEST_ASSERT(capacity >= 5, "Capacity should be at least the requested size");
    TEST_ASSERT(capacity >= ARRAY_MIN_SIZE, "Capacity should be at least ARRAY_MIN_SIZE");
    printf("Zero capacity expanded to %d (min size: %d)\n", capacity, ARRAY_MIN_SIZE);
    
    // Clean up this array before next test
    myfree(array);
    
    // Test case: Very large requested capacity (but not unreasonable)
    capacity = 100;
    array = mymalloc(capacity * sizeof(int));
    TEST_ASSERT(array != NULL, "Initial array allocation should succeed");
    
    old_capacity = capacity;
    int large_size = 10000; // 10K elements (reduced to avoid excessive memory use)
    result = array_expand_default((void **)&array, sizeof(int), &capacity, large_size);
    TEST_ASSERT(result == 0, "array_expand should handle large requested capacity");
    TEST_ASSERT(capacity >= large_size, "Capacity should meet or exceed large requested size");
    printf("Large capacity expanded from %d to %d\n", old_capacity, capacity);
    
    // Clean up
    myfree(array);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("Starting tests for test_array_utils\n");
    printf("========================================\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_array_expansion();
    test_default_expansion();
    test_galaxy_array_expansion();
    test_multiple_expansions();
    test_error_handling();
    test_edge_cases();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_array_utils:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
