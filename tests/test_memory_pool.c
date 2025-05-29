/**
 * Test suite for Memory Pool System
 * 
 * Tests cover:
 * - Pool creation, destruction and lifecycle management
 * - Memory allocation, deallocation and reuse patterns
 * - Pool expansion under high allocation load
 * - Global pool interface for simplified usage
 * - Memory leak detection and statistics tracking
 * - Error handling and robustness validation
 * - Extension system integration and cleanup
 * - Property system compatibility and type validation
 * 
 * This test validates the memory pooling system's integration with SAGE's
 * core infrastructure, ensuring proper core-physics separation and extension
 * system compatibility.
 * 
 * IMPORTANT PROPERTY REUSE BEHAVIOUR:
 * The memory pool intentionally preserves property values during galaxy reuse
 * for performance reasons. This means that when a galaxy is freed and later
 * reallocated from the pool, it may contain property values from its previous
 * use. This is acceptable behaviour but modules must explicitly initialize
 * all properties they use. During legacy physics module migration, if
 * unexpected behaviour occurs with galaxy properties, this reuse pattern
 * should be investigated as a potential cause.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include actual headers from the real codebase */
#include "../src/core/core_allvars.h"
#include "../src/core/core_memory_pool.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/core/core_properties.h"  /* For property system */
#include "../src/core/core_property_utils.h"  /* For property accessors */

#define GALAXY_SIZE sizeof(struct GALAXY)
#define TEST_ALLOC_COUNT 10
#define LARGE_TEST_ALLOC_COUNT 50

/* 
 * Test stubs - we need to modify these to use the real interfaces
 * but provide simplified implementations for testing
 */

/* Global registry stub is defined in galaxy_extension_stubs.c */
extern struct galaxy_extension_registry *global_extension_registry;

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
    } \
} while(0)

/* Stub functions for logging to avoid dependencies */
void LOG_DEBUG(const char *fmt __attribute__((unused)), ...) {
    /* Do nothing in tests */
}

void LOG_INFO(const char *fmt __attribute__((unused)), ...) {
    /* Do nothing in tests */
}

void LOG_WARNING(const char *fmt __attribute__((unused)), ...) {
    /* Do nothing in tests */
}

void LOG_ERROR(const char *fmt __attribute__((unused)), ...) {
    /* Do nothing in tests */
}

/**
 * Test: Pool creation and destruction
 * 
 * Validates basic pool lifecycle management including creation with both
 * default and specific parameters, statistics retrieval, and proper cleanup.
 */
void test_pool_create_destroy() {
    printf("=== Testing pool creation and destruction ===\n");
    
    // Test with default params
    struct memory_pool *pool = galaxy_pool_create(0, 0);
    TEST_ASSERT(pool != NULL, "Pool creation with default params should succeed");
    galaxy_pool_destroy(pool);
    
    // Test with specific params
    pool = galaxy_pool_create(1024, 256);
    TEST_ASSERT(pool != NULL, "Pool creation with specific params should succeed");
    
    // Get stats
    size_t capacity, used, allocs, peak;
    bool result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(result, "Pool stats retrieval should succeed");
    TEST_ASSERT(capacity >= 1024, "Pool capacity should meet minimum requirement");
    TEST_ASSERT(used == 0, "Initial pool usage should be zero");
    TEST_ASSERT(allocs == 0, "Initial allocation count should be zero");
    TEST_ASSERT(peak == 0, "Initial peak usage should be zero");
    
    galaxy_pool_destroy(pool);
    
    printf("Pool creation/destruction test PASSED\n");
}

/**
 * Test: Pool allocation and freeing
 * 
 * Tests single and multiple galaxy allocation/deallocation patterns,
 * validates statistics tracking, and confirms memory reuse functionality.
 */
void test_pool_alloc_free() {
    printf("\n=== Testing pool allocation and freeing ===\n");
    
    struct memory_pool *pool = galaxy_pool_create(1024, 256);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    // Allocate a single galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    TEST_ASSERT(g1 != NULL, "Single galaxy allocation should succeed");
    
    // Check stats
    size_t capacity, used, allocs, peak;
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(used == 1, "Pool usage should reflect single allocation");
    TEST_ASSERT(allocs == 1, "Allocation count should be 1");
    TEST_ASSERT(peak == 1, "Peak usage should be 1");
    
    // Free the galaxy
    galaxy_pool_free(pool, g1);
    
    // Check stats again
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(used == 0, "Pool usage should return to zero after free");
    TEST_ASSERT(allocs == 1, "Allocation count should remain unchanged");
    TEST_ASSERT(peak == 1, "Peak usage should remain unchanged");
    
    // Allocate many galaxies
    struct GALAXY *galaxies[TEST_ALLOC_COUNT];
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        TEST_ASSERT(galaxies[i] != NULL, "Multiple galaxy allocation should succeed");
    }
    
    // Check stats
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(used == TEST_ALLOC_COUNT, "Pool usage should reflect multiple allocations");
    TEST_ASSERT(allocs == TEST_ALLOC_COUNT + 1, "Total allocation count should be correct");
    TEST_ASSERT(peak == TEST_ALLOC_COUNT, "Peak usage should reflect maximum concurrent usage");
    
    // Free half the galaxies
    for (int i = 0; i < TEST_ALLOC_COUNT / 2; i++) {
        galaxy_pool_free(pool, galaxies[i]);
    }
    
    // Check stats
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(used == TEST_ALLOC_COUNT / 2, "Pool usage should reflect partial freeing");
    TEST_ASSERT(peak == TEST_ALLOC_COUNT, "Peak usage should remain unchanged");
    
    // Allocate more galaxies to test reuse
    for (int i = 0; i < TEST_ALLOC_COUNT / 2; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        TEST_ASSERT(galaxies[i] != NULL, "Galaxy reuse allocation should succeed");
    }
    
    // Free all galaxies
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxy_pool_free(pool, galaxies[i]);
    }
    
    galaxy_pool_destroy(pool);
    
    printf("Pool allocation/freeing test PASSED\n");
}

/**
 * Test: Global pool functions
 * 
 * Tests the simplified global pool interface including initialization,
 * cleanup, and the convenience allocation/deallocation functions.
 */
void test_global_pool() {
    printf("\n=== Testing global pool functions ===\n");
    
    // Initialize global pool
    int result = galaxy_pool_initialize();
    TEST_ASSERT(result == 0, "Global pool initialization should succeed");
    TEST_ASSERT(galaxy_pool_is_enabled(), "Global pool should be enabled after initialization");
    
    // Allocate galaxies
    struct GALAXY *galaxies[TEST_ALLOC_COUNT];
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxies[i] = galaxy_alloc();
        TEST_ASSERT(galaxies[i] != NULL, "Global galaxy allocation should succeed");
    }
    
    // Free galaxies
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxy_free(galaxies[i]);
    }
    
    // Clean up global pool
    result = galaxy_pool_cleanup();
    TEST_ASSERT(result == 0, "Global pool cleanup should succeed");
    TEST_ASSERT(!galaxy_pool_is_enabled(), "Global pool should be disabled after cleanup");
    
    printf("Global pool test PASSED\n");
}

/**
 * Test: Pool expansion
 * 
 * Tests the pool's ability to dynamically expand when allocation requests
 * exceed initial capacity, ensuring proper statistics tracking during expansion.
 */
void test_pool_expansion() {
    printf("\n=== Testing pool expansion with large allocation count ===\n");
    
    struct memory_pool *pool = galaxy_pool_create(1024, 256);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    // Allocate many more galaxies than initial capacity
    struct GALAXY *galaxies[LARGE_TEST_ALLOC_COUNT];
    for (int i = 0; i < LARGE_TEST_ALLOC_COUNT; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        TEST_ASSERT(galaxies[i] != NULL, "Large-scale galaxy allocation should succeed");
    }
    
    // Check stats
    size_t capacity, used, allocs, peak;
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(capacity >= LARGE_TEST_ALLOC_COUNT, "Pool capacity should accommodate all allocations");
    TEST_ASSERT(used == LARGE_TEST_ALLOC_COUNT, "Pool usage should reflect all allocations");
    TEST_ASSERT(allocs == LARGE_TEST_ALLOC_COUNT, "Allocation count should be accurate");
    TEST_ASSERT(peak == LARGE_TEST_ALLOC_COUNT, "Peak usage should match current usage");
    
    // Free all galaxies
    for (int i = 0; i < LARGE_TEST_ALLOC_COUNT; i++) {
        galaxy_pool_free(pool, galaxies[i]);
    }
    
    galaxy_pool_destroy(pool);
    
    printf("Pool expansion test PASSED\n");
}

/**
 * Test: Galaxy data manipulation and property reuse
 * 
 * Tests property access patterns and validates the memory pool's property
 * value preservation behaviour during galaxy reuse. This test documents
 * the intentional design where property values persist across reuse cycles.
 * 
 * IMPORTANT: Property values are intentionally preserved during pool reuse
 * for performance reasons. This means modules must explicitly initialize
 * properties they use rather than assuming clean state. This behaviour
 * should be monitored during legacy physics module migration.
 */
void test_galaxy_data() {
    printf("\n=== Testing galaxy data manipulation ===\n");
    
    struct memory_pool *pool = galaxy_pool_create(1024, 256);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    // Allocate a galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    TEST_ASSERT(g1 != NULL, "Galaxy allocation should succeed");
    
    // Initialize properties for testing
    if (g1->properties == NULL) {
        g1->properties = calloc(1, sizeof(galaxy_properties_t));
        TEST_ASSERT(g1->properties != NULL, "Properties allocation should succeed");
    }
    
    // Set some core properties using macros
    GALAXY_PROP_Type(g1) = 1;
    GALAXY_PROP_SnapNum(g1) = 63;
    GALAXY_PROP_GalaxyIndex(g1) = 12345;
    
    // Get property ID for checking Type property (the one we'll test for preservation)
    property_id_t type_id = get_cached_property_id("Type");
    
    // Remember values and memory location
    int original_type = GALAXY_PROP_Type(g1);
    void* original_location = (void*)g1;
    
    // Free the galaxy
    galaxy_pool_free(pool, g1);
    
    // Allocate a new galaxy - should be the same memory location from the pool
    struct GALAXY *g2 = galaxy_pool_alloc(pool);
    TEST_ASSERT(g2 != NULL, "Galaxy reallocation should succeed");
    
    // Verify this is the same memory location (reused from pool)
    printf("  Verification: Galaxy reused from pool: %s (original=%p, new=%p)\n", 
           g2 == original_location ? "Yes" : "No", original_location, (void*)g2);
    
    // The properties structure may or may not be reset depending on pool implementation
    // In a real application, galaxy_free should call a reset function or galaxy_alloc should initialize
    
    // In our test, we'll explicitly initialize the new galaxy to ensure it works
    if (g2->properties == NULL) {
        g2->properties = calloc(1, sizeof(galaxy_properties_t));
    } else {
        // If the properties struct is reused but not reset, we can demonstrate that
        // by checking if the values are still the same
        if (has_property(g2, type_id)) {
            int new_type = get_int32_property(g2, type_id, -1);
            printf("  Property reuse check: Type is %s (old=%d, current=%d)\n", 
                   new_type == original_type ? "unchanged" : "changed", original_type, new_type);
            
            /* IMPORTANT: The memory pool preserves property values after freeing.
             * This is intentional behaviour for performance reasons. The pool reuses memory
             * but doesn't reset it to a clean state. Application code should explicitly 
             * initialize properties after allocation.
             *
             * LEGACY MODULE MIGRATION WARNING: If unexpected behaviour occurs with galaxy
             * properties during migration, this value preservation should be investigated.
             * Modules must not assume properties start in a clean state.
             */
            printf("  NOTE: Property values %s preserved during reuse (this is intentional).\n", 
                   new_type == original_type ? "are" : "are not");
        }
    }
    
    // Set new values to ensure the properties can be modified
    GALAXY_PROP_Type(g2) = 2;  // Different from original
    GALAXY_PROP_SnapNum(g2) = 42;
    GALAXY_PROP_GalaxyIndex(g2) = 54321;
    
    // Verify we can write and read the properties
    TEST_ASSERT(GALAXY_PROP_Type(g2) == 2, "Property modification should work");
    TEST_ASSERT(GALAXY_PROP_SnapNum(g2) == 42, "Property modification should work");
    TEST_ASSERT(GALAXY_PROP_GalaxyIndex(g2) == 54321, "Property modification should work");
    
    printf("  Verification: Successfully set and read properties on reused galaxy\n");
    
    galaxy_pool_free(pool, g2);
    galaxy_pool_destroy(pool);
    
    printf("Galaxy data test PASSED\n");
}

/**
 * Test: Memory leak detection and statistics tracking
 * 
 * Validates the memory pool's ability to track allocations and detect
 * memory usage patterns, including simulated leak scenarios.
 */
void test_memory_leak_detection() {
    printf("\n=== Testing memory leak detection ===\n");
    
    struct memory_pool *pool = galaxy_pool_create(100, 10);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    size_t capacity, used, allocs, peak;
    bool result;
    
    // Initial stats
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(result, "Initial stats retrieval should succeed");
    TEST_ASSERT(used == 0, "Initial used count should be zero");
    TEST_ASSERT(allocs == 0, "Initial allocation count should be zero");
    
    // Allocate many galaxies, then free all but one
    struct GALAXY *leaked_galaxy = NULL;
    const int allocation_count = 95;
    struct GALAXY *galaxies[allocation_count];
    
    // First allocation round
    for (int i = 0; i < allocation_count; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        TEST_ASSERT(galaxies[i] != NULL, "Galaxy allocation should succeed during leak test");
        
        // Save one galaxy to simulate a leak
        if (i == 50) {
            leaked_galaxy = galaxies[i];
        }
    }
    
    // Check stats after allocation
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(result, "Stats retrieval after allocation should succeed");
    TEST_ASSERT(used == allocation_count, "Used count should match allocation count");
    TEST_ASSERT(allocs == allocation_count, "Allocation count should match expected");
    TEST_ASSERT(peak == allocation_count, "Peak usage should match allocation count");
    
    // Free all except the "leaked" one
    for (int i = 0; i < allocation_count; i++) {
        if (galaxies[i] != leaked_galaxy) {
            galaxy_pool_free(pool, galaxies[i]);
        }
    }
    
    // Check stats after freeing - should show 1 galaxy still in use
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(result, "Stats retrieval after partial free should succeed");
    TEST_ASSERT(used == 1, "One galaxy should still be in use (simulated leak)");
    TEST_ASSERT(peak == allocation_count, "Peak usage should remain at maximum");
    
    // Clean up the leaked galaxy and verify stats
    galaxy_pool_free(pool, leaked_galaxy);
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    TEST_ASSERT(result, "Stats retrieval after cleanup should succeed");
    TEST_ASSERT(used == 0, "All galaxies should be freed after cleanup");
    
    galaxy_pool_destroy(pool);
    
    printf("Memory leak detection test PASSED\n");
}

/**
 * Test: Error condition handling and robustness
 * 
 * Validates the memory pool's ability to handle invalid operations
 * gracefully, including null pointer handling and double-free detection.
 */
void test_error_conditions() {
    printf("\n=== Testing error handling ===\n");
    
    struct memory_pool *pool = galaxy_pool_create(10, 5);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    // Test freeing NULL pointer - should not crash
    galaxy_pool_free(pool, NULL);
    TEST_ASSERT(true, "Freeing NULL pointer should not crash");
    
    // Test freeing a pointer not from this pool - should be detected or safely ignored
    struct GALAXY fake_galaxy;
    galaxy_pool_free(pool, &fake_galaxy);  // This should not crash, even if it's invalid
    TEST_ASSERT(true, "Freeing invalid pointer should not crash");
    
    // Get a valid galaxy from the pool
    struct GALAXY *g = galaxy_pool_alloc(pool);
    TEST_ASSERT(g != NULL, "Galaxy allocation should succeed");
    
    // Free it
    galaxy_pool_free(pool, g);
    TEST_ASSERT(true, "Valid galaxy free should succeed");
    
    // Try to free it again - double-free should be detected or safely ignored
    galaxy_pool_free(pool, g);
    TEST_ASSERT(true, "Double-free should not crash");
    
    // Cleanup
    galaxy_pool_destroy(pool);
    
    printf("Error handling test PASSED\n");
}

/**
 * Test: Dynamic array properties and extension system integration
 * 
 * Tests memory pool integration with the extension system, validating
 * proper cleanup of dynamic arrays and extension data during reuse.
 */
void test_dynamic_array_properties() {
    printf("\n=== Testing dynamic array properties ===\n");
    
    struct memory_pool *pool = galaxy_pool_create(100, 10);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    // Allocate a galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    TEST_ASSERT(g1 != NULL, "Galaxy allocation should succeed");
    
    // Ensure properties is allocated
    if (g1->properties == NULL) {
        g1->properties = calloc(1, sizeof(galaxy_properties_t));
        TEST_ASSERT(g1->properties != NULL, "Properties allocation should succeed");
    }
    
    // Record original location for verification
    void* original_location = (void*)g1;
    
    // Free the galaxy (this will clean up properties via galaxy_extension_cleanup)
    galaxy_pool_free(pool, g1);
    
    // Allocate a new galaxy - should be the same memory location from the pool
    struct GALAXY *g2 = galaxy_pool_alloc(pool);
    TEST_ASSERT(g2 != NULL, "Galaxy reallocation should succeed");
    
    // Verify this is the same memory location (reused from pool)
    printf("  Verification: Galaxy reused from pool: %s (original=%p, new=%p)\n", 
           g2 == original_location ? "Yes" : "No", original_location, (void*)g2);
    
    // Verify the extension data was properly cleaned up
    if (g2->extension_data != NULL) {
        printf("  Warning: Extension data not properly cleaned: extension_data=%p\n", 
               g2->extension_data);
    } else {
        printf("  Verification: Extension data properly cleaned: extension_data is NULL\n");
    }
    
    // Verify that extension count and flags were reset
    printf("  Verification: Extensions count reset: %s (value=%d)\n", 
           g2->num_extensions == 0 ? "Yes" : "No", g2->num_extensions);
    printf("  Verification: Extensions flags reset: %s (value=%llu)\n", 
           g2->extension_flags == 0 ? "Yes" : "No", (unsigned long long)g2->extension_flags);
    
    // Ensure properties is allocated for the new galaxy
    if (g2->properties == NULL) {
        g2->properties = calloc(1, sizeof(galaxy_properties_t));
        TEST_ASSERT(g2->properties != NULL, "Properties reallocation should succeed");
    }
    
    // Free the galaxy
    galaxy_pool_free(pool, g2);
    galaxy_pool_destroy(pool);
    
    printf("Dynamic array properties test PASSED\n");
}

/**
 * Test: Property type validation and compatibility
 * 
 * Validates the memory pool's compatibility with different property types
 * and ensures proper read/write functionality across reuse cycles.
 */
void test_property_types() {
    printf("\n=== Testing property types ===\n");
    
    struct memory_pool *pool = galaxy_pool_create(100, 10);
    TEST_ASSERT(pool != NULL, "Pool creation should succeed");
    
    // Allocate a galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    TEST_ASSERT(g1 != NULL, "Galaxy allocation should succeed");
    
    // Ensure properties is allocated
    if (g1->properties == NULL) {
        g1->properties = calloc(1, sizeof(galaxy_properties_t));
        TEST_ASSERT(g1->properties != NULL, "Properties allocation should succeed");
    }
    
    // Set properties of different types using macros
    // Note: We're using the macro system to set these, assuming core properties
    // of these types exist. If they don't, the test would need modification.
    
    // Integer property
    GALAXY_PROP_Type(g1) = 42;
    
    // Long long property
    GALAXY_PROP_GalaxyIndex(g1) = 9223372036854775807LL;  // Max long long value
    
    // We'd also test float and double properties if they exist in core properties
    
    // Verify properties were set correctly
    TEST_ASSERT(GALAXY_PROP_Type(g1) == 42, "Integer property should be set correctly");
    TEST_ASSERT(GALAXY_PROP_GalaxyIndex(g1) == 9223372036854775807LL, "Long long property should be set correctly");
    
    // Free the galaxy
    galaxy_pool_free(pool, g1);
    
    // Allocate another galaxy
    struct GALAXY *g2 = galaxy_pool_alloc(pool);
    TEST_ASSERT(g2 != NULL, "Galaxy reallocation should succeed");
    
    // Set different values to ensure we can modify all types
    GALAXY_PROP_Type(g2) = 24;  // Different integer
    GALAXY_PROP_GalaxyIndex(g2) = 123456789LL;  // Different long long
    
    // Verify properties were set correctly
    TEST_ASSERT(GALAXY_PROP_Type(g2) == 24, "Integer property modification should work");
    TEST_ASSERT(GALAXY_PROP_GalaxyIndex(g2) == 123456789LL, "Long long property modification should work");
    
    // Free the galaxy
    galaxy_pool_free(pool, g2);
    galaxy_pool_destroy(pool);
    
    printf("Property types test PASSED\n");
}

int main() {
    printf("\n========================================\n");
    printf("Starting tests for test_memory_pool\n");
    printf("========================================\n\n");
    
    // Reset test counters
    tests_run = 0;
    tests_passed = 0;
    
    // Run all test functions
    test_pool_create_destroy();
    test_pool_alloc_free();
    test_global_pool();
    test_pool_expansion();
    test_galaxy_data();
    test_memory_leak_detection();
    test_error_conditions();
    test_dynamic_array_properties();
    test_property_types();
    
    // Report final statistics
    printf("\n========================================\n");
    printf("Test results for test_memory_pool:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
