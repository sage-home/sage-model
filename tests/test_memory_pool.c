#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include actual headers from the real codebase */
#include "../src/core/core_allvars.h"
#include "../src/core/core_memory_pool.h"
#include "../src/core/core_galaxy_extensions.h"

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
void LOG_DEBUG(const char *fmt, ...) {
    /* Do nothing in tests */
}

void LOG_INFO(const char *fmt, ...) {
    /* Do nothing in tests */
}

void LOG_WARNING(const char *fmt, ...) {
    /* Do nothing in tests */
}

void LOG_ERROR(const char *fmt, ...) {
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
    struct GALAXY *g = galaxy_pool_alloc(pool);
    assert(g != NULL);
    
    // Write to galaxy data
    g->StellarMass = 1.23f;
    g->BulgeMass = 0.45f;
    g->ColdGas = 2.34f;
    g->HotGas = 5.67f;
    g->Type = 0;
    
    // Free the galaxy
    galaxy_pool_free(pool, g);
    
    // Allocate a new galaxy and check if it's properly initialized
    g = galaxy_pool_alloc(pool);
    assert(g != NULL);
    
    galaxy_pool_free(pool, g);
    galaxy_pool_destroy(pool);
    
    printf("Galaxy data test PASSED\n");
}

int main() {
    printf("===== Memory Pool Tests =====\n");
    
    test_pool_create_destroy();
    test_pool_alloc_free();
    test_global_pool();
    test_pool_expansion();
    test_galaxy_data();
    
    printf("All tests PASSED\n");
    return 0;
}
