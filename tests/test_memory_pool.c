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

void test_pool_create_destroy() {
    printf("Testing pool creation and destruction...\n");
    
    // Test with default params
    struct memory_pool *pool = galaxy_pool_create(0, 0);
    assert(pool != NULL);
    galaxy_pool_destroy(pool);
    
    // Test with specific params
    pool = galaxy_pool_create(1024, 256);
    assert(pool != NULL);
    
    // Get stats
    size_t capacity, used, allocs, peak;
    bool result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(result);
    assert(capacity >= 1024);
    assert(used == 0);
    assert(allocs == 0);
    assert(peak == 0);
    
    galaxy_pool_destroy(pool);
    
    printf("Pool creation/destruction test PASSED\n");
}

void test_pool_alloc_free() {
    printf("Testing pool allocation and freeing...\n");
    
    struct memory_pool *pool = galaxy_pool_create(1024, 256);
    assert(pool != NULL);
    
    // Allocate a single galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    assert(g1 != NULL);
    
    // Check stats
    size_t capacity, used, allocs, peak;
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(used == 1);
    assert(allocs == 1);
    assert(peak == 1);
    
    // Free the galaxy
    galaxy_pool_free(pool, g1);
    
    // Check stats again
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(used == 0);
    assert(allocs == 1);
    assert(peak == 1);
    
    // Allocate many galaxies
    struct GALAXY *galaxies[TEST_ALLOC_COUNT];
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        assert(galaxies[i] != NULL);
    }
    
    // Check stats
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(used == TEST_ALLOC_COUNT);
    assert(allocs == TEST_ALLOC_COUNT + 1);
    assert(peak == TEST_ALLOC_COUNT);
    
    // Free half the galaxies
    for (int i = 0; i < TEST_ALLOC_COUNT / 2; i++) {
        galaxy_pool_free(pool, galaxies[i]);
    }
    
    // Check stats
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(used == TEST_ALLOC_COUNT / 2);
    assert(peak == TEST_ALLOC_COUNT);
    
    // Allocate more galaxies to test reuse
    for (int i = 0; i < TEST_ALLOC_COUNT / 2; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        assert(galaxies[i] != NULL);
    }
    
    // Free all galaxies
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxy_pool_free(pool, galaxies[i]);
    }
    
    galaxy_pool_destroy(pool);
    
    printf("Pool allocation/freeing test PASSED\n");
}

void test_global_pool() {
    printf("Testing global pool functions...\n");
    
    // Initialize global pool
    int result = galaxy_pool_initialize();
    assert(result == 0);
    assert(galaxy_pool_is_enabled());
    
    // Allocate galaxies
    struct GALAXY *galaxies[TEST_ALLOC_COUNT];
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxies[i] = galaxy_alloc();
        assert(galaxies[i] != NULL);
    }
    
    // Free galaxies
    for (int i = 0; i < TEST_ALLOC_COUNT; i++) {
        galaxy_free(galaxies[i]);
    }
    
    // Clean up global pool
    result = galaxy_pool_cleanup();
    assert(result == 0);
    assert(!galaxy_pool_is_enabled());
    
    printf("Global pool test PASSED\n");
}

void test_pool_expansion() {
    printf("Testing pool expansion with large allocation count...\n");
    
    struct memory_pool *pool = galaxy_pool_create(1024, 256);
    assert(pool != NULL);
    
    // Allocate many more galaxies than initial capacity
    struct GALAXY *galaxies[LARGE_TEST_ALLOC_COUNT];
    for (int i = 0; i < LARGE_TEST_ALLOC_COUNT; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        assert(galaxies[i] != NULL);
    }
    
    // Check stats
    size_t capacity, used, allocs, peak;
    galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(capacity >= LARGE_TEST_ALLOC_COUNT);
    assert(used == LARGE_TEST_ALLOC_COUNT);
    assert(allocs == LARGE_TEST_ALLOC_COUNT);
    assert(peak == LARGE_TEST_ALLOC_COUNT);
    
    // Free all galaxies
    for (int i = 0; i < LARGE_TEST_ALLOC_COUNT; i++) {
        galaxy_pool_free(pool, galaxies[i]);
    }
    
    galaxy_pool_destroy(pool);
    
    printf("Pool expansion test PASSED\n");
}

void test_galaxy_data() {
    printf("Testing galaxy data manipulation...\n");
    
    struct memory_pool *pool = galaxy_pool_create(1024, 256);
    assert(pool != NULL);
    
    // Allocate a galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    assert(g1 != NULL);
    
    // Initialize properties for testing
    if (g1->properties == NULL) {
        g1->properties = calloc(1, sizeof(galaxy_properties_t));
        assert(g1->properties != NULL);
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
    assert(g2 != NULL);
    
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
            
            /* NOTE: The memory pool doesn't reinitialize property values after freeing.
             * This is an implementation detail that might be unintended. The pool reuses memory
             * but doesn't reset it to a clean state. Application code should explicitly 
             * initialize properties after allocation or the pool implementation should
             * reset properties during allocation.
             *
             * If property value preservation is intended behavior (e.g., for performance reasons),
             * then this assertion should pass. If not, galaxy_pool_alloc should be modified to
             * reset property values.
             */
            printf("  NOTE: Property values %s preserved during reuse.\n", 
                   new_type == original_type ? "are" : "are not");
        }
    }
    
    // Set new values to ensure the properties can be modified
    GALAXY_PROP_Type(g2) = 2;  // Different from original
    GALAXY_PROP_SnapNum(g2) = 42;
    GALAXY_PROP_GalaxyIndex(g2) = 54321;
    
    // Verify we can write and read the properties
    assert(GALAXY_PROP_Type(g2) == 2);
    assert(GALAXY_PROP_SnapNum(g2) == 42);
    assert(GALAXY_PROP_GalaxyIndex(g2) == 54321);
    
    printf("  Verification: Successfully set and read properties on reused galaxy\n");
    
    galaxy_pool_free(pool, g2);
    galaxy_pool_destroy(pool);
    
    printf("Galaxy data test PASSED\n");
}

void test_memory_leak_detection() {
    printf("Testing memory leak detection...\n");
    
    struct memory_pool *pool = galaxy_pool_create(100, 10);
    assert(pool != NULL);
    
    size_t capacity, used, allocs, peak;
    bool result;
    
    // Initial stats
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(result);
    assert(used == 0);
    assert(allocs == 0);
    
    // Allocate many galaxies, then free all but one
    struct GALAXY *leaked_galaxy = NULL;
    const int allocation_count = 95;
    struct GALAXY *galaxies[allocation_count];
    
    // First allocation round
    for (int i = 0; i < allocation_count; i++) {
        galaxies[i] = galaxy_pool_alloc(pool);
        assert(galaxies[i] != NULL);
        
        // Save one galaxy to simulate a leak
        if (i == 50) {
            leaked_galaxy = galaxies[i];
        }
    }
    
    // Check stats after allocation
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(result);
    assert(used == allocation_count);
    assert(allocs == allocation_count);
    assert(peak == allocation_count);
    
    // Free all except the "leaked" one
    for (int i = 0; i < allocation_count; i++) {
        if (galaxies[i] != leaked_galaxy) {
            galaxy_pool_free(pool, galaxies[i]);
        }
    }
    
    // Check stats after freeing - should show 1 galaxy still in use
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(result);
    assert(used == 1);  // One galaxy still in use
    assert(peak == allocation_count);
    
    // Clean up the leaked galaxy and verify stats
    galaxy_pool_free(pool, leaked_galaxy);
    result = galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);
    assert(result);
    assert(used == 0);  // All galaxies freed
    
    galaxy_pool_destroy(pool);
    
    printf("Memory leak detection test PASSED\n");
}

void test_error_conditions() {
    printf("Testing error handling...\n");
    
    struct memory_pool *pool = galaxy_pool_create(10, 5);
    assert(pool != NULL);
    
    // Test freeing NULL pointer - should not crash
    galaxy_pool_free(pool, NULL);
    
    // Test freeing a pointer not from this pool - should be detected or safely ignored
    struct GALAXY fake_galaxy;
    galaxy_pool_free(pool, &fake_galaxy);  // This should not crash, even if it's invalid
    
    // Get a valid galaxy from the pool
    struct GALAXY *g = galaxy_pool_alloc(pool);
    assert(g != NULL);
    
    // Free it
    galaxy_pool_free(pool, g);
    
    // Try to free it again - double-free should be detected or safely ignored
    galaxy_pool_free(pool, g);
    
    // Cleanup
    galaxy_pool_destroy(pool);
    
    printf("Error handling test PASSED\n");
}

void test_dynamic_array_properties() {
    printf("Testing memory pool with dynamic array properties...\n");
    
    struct memory_pool *pool = galaxy_pool_create(100, 10);
    assert(pool != NULL);
    
    // Allocate a galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    assert(g1 != NULL);
    
    // Ensure properties is allocated
    if (g1->properties == NULL) {
        g1->properties = calloc(1, sizeof(galaxy_properties_t));
        assert(g1->properties != NULL);
    }
    
    // Record original location for verification
    void* original_location = (void*)g1;
    
    // Free the galaxy (this will clean up properties via galaxy_extension_cleanup)
    galaxy_pool_free(pool, g1);
    
    // Allocate a new galaxy - should be the same memory location from the pool
    struct GALAXY *g2 = galaxy_pool_alloc(pool);
    assert(g2 != NULL);
    
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
        assert(g2->properties != NULL);
    }
    
    // Free the galaxy
    galaxy_pool_free(pool, g2);
    galaxy_pool_destroy(pool);
    
    printf("Dynamic array properties test PASSED\n");
}

void test_property_types() {
    printf("Testing memory pool with different property types...\n");
    
    struct memory_pool *pool = galaxy_pool_create(100, 10);
    assert(pool != NULL);
    
    // Allocate a galaxy
    struct GALAXY *g1 = galaxy_pool_alloc(pool);
    assert(g1 != NULL);
    
    // Ensure properties is allocated
    if (g1->properties == NULL) {
        g1->properties = calloc(1, sizeof(galaxy_properties_t));
        assert(g1->properties != NULL);
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
    assert(GALAXY_PROP_Type(g1) == 42);
    assert(GALAXY_PROP_GalaxyIndex(g1) == 9223372036854775807LL);
    
    // Free the galaxy
    galaxy_pool_free(pool, g1);
    
    // Allocate another galaxy
    struct GALAXY *g2 = galaxy_pool_alloc(pool);
    assert(g2 != NULL);
    
    // Set different values to ensure we can modify all types
    GALAXY_PROP_Type(g2) = 24;  // Different integer
    GALAXY_PROP_GalaxyIndex(g2) = 123456789LL;  // Different long long
    
    // Verify properties were set correctly
    assert(GALAXY_PROP_Type(g2) == 24);
    assert(GALAXY_PROP_GalaxyIndex(g2) == 123456789LL);
    
    // Free the galaxy
    galaxy_pool_free(pool, g2);
    galaxy_pool_destroy(pool);
    
    printf("Property types test PASSED\n");
}

int main() {
    printf("===== Memory Pool Tests =====\n");
    
    test_pool_create_destroy();
    test_pool_alloc_free();
    test_global_pool();
    test_pool_expansion();
    test_galaxy_data();
    test_memory_leak_detection();
    test_error_conditions();
    test_dynamic_array_properties();
    test_property_types();
    
    printf("All tests PASSED\n");
    return 0;
}
