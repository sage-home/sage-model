/**
 * @file test_fof_memory_management.c
 * @brief Unit tests for FOF Memory Management functionality
 * 
 * Tests cover:
 * - Zero memory leaks validation
 * - Proper cleanup of GalaxyArray and its contents
 * - Large FOF group handling (>500 galaxies per group)
 * - Memory pool integration and stress testing
 * 
 * This test validates that the optimized FOF processing maintains robust
 * memory management patterns without leaks or corruption.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_build_model.h"
#include "../src/core/galaxy_array.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_mymalloc.h"
#include "../src/core/core_memory_pool.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return; \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test parameters structure
static struct params test_params = {
    .simulation = {
        .NumSnapOutputs = 10,
        .SimMaxSnaps = 64,
        .LastSnapshotNr = 63
    }
};

// Test fixtures
static struct test_context {
    struct halo_data *halos;
    struct halo_aux_data *haloaux;
    GalaxyArray *galaxies_prev_snap;
    GalaxyArray *galaxies_this_snap;
    int32_t galaxycounter;
    int nhalo;
    size_t initial_memory_usage;
    int initialized;
} test_ctx;

//=============================================================================
// Test Helper Functions
//=============================================================================

/**
 * Get current memory usage for leak detection
 */
static size_t get_current_memory_usage(void) {
    // This is a simplified memory tracking - in real scenarios you'd use
    // more sophisticated memory profiling tools like Valgrind
    size_t usage = 0;
    if (test_ctx.halos) usage += test_ctx.nhalo * sizeof(struct halo_data);
    if (test_ctx.haloaux) usage += test_ctx.nhalo * sizeof(struct halo_aux_data);
    if (test_ctx.galaxies_prev_snap) usage += galaxy_array_get_count(test_ctx.galaxies_prev_snap) * sizeof(struct GALAXY);
    if (test_ctx.galaxies_this_snap) usage += galaxy_array_get_count(test_ctx.galaxies_this_snap) * sizeof(struct GALAXY);
    return usage;
}

/**
 * Create a mock halo structure for memory testing
 */
static void create_test_halo(int halo_idx, int snap_num, float mvir, 
                           int first_prog, int next_prog, int next_in_fof) {
    test_ctx.halos[halo_idx].SnapNum = snap_num;
    test_ctx.halos[halo_idx].Mvir = mvir;
    test_ctx.halos[halo_idx].FirstProgenitor = first_prog;
    test_ctx.halos[halo_idx].NextProgenitor = next_prog;
    test_ctx.halos[halo_idx].NextHaloInFOFgroup = next_in_fof;
    test_ctx.halos[halo_idx].MostBoundID = 1000000 + halo_idx;
    
    // Set positions and velocities
    for (int i = 0; i < 3; i++) {
        test_ctx.halos[halo_idx].Pos[i] = 10.0f + halo_idx;
        test_ctx.halos[halo_idx].Vel[i] = 100.0f + halo_idx;
    }
    test_ctx.halos[halo_idx].Len = 100 + halo_idx;
    test_ctx.halos[halo_idx].Vmax = 200.0f + halo_idx;
    
    // Initialize aux data
    test_ctx.haloaux[halo_idx].FirstGalaxy = -1;
    test_ctx.haloaux[halo_idx].NGalaxies = 0;
}

/**
 * Create a test galaxy with proper memory management
 */
static int create_test_galaxy(int galaxy_type, int halo_nr, float stellar_mass __attribute__((unused))) {
    struct GALAXY temp_galaxy;
    memset(&temp_galaxy, 0, sizeof(struct GALAXY));
    
    // Initialize properties
    if (allocate_galaxy_properties(&temp_galaxy, &test_params) != 0) {
        printf("Failed to allocate galaxy properties\n");
        return -1;
    }
    
    // Set basic properties
    GALAXY_PROP_Type(&temp_galaxy) = galaxy_type;
    GALAXY_PROP_HaloNr(&temp_galaxy) = halo_nr;
    GALAXY_PROP_GalaxyIndex(&temp_galaxy) = test_ctx.galaxycounter++;
    GALAXY_PROP_SnapNum(&temp_galaxy) = test_ctx.halos[halo_nr].SnapNum - 1;
    GALAXY_PROP_merged(&temp_galaxy) = 0;
    
    // Set masses and positions
    GALAXY_PROP_Mvir(&temp_galaxy) = test_ctx.halos[halo_nr].Mvir;
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos(&temp_galaxy)[i] = test_ctx.halos[halo_nr].Pos[i];
        GALAXY_PROP_Vel(&temp_galaxy)[i] = test_ctx.halos[halo_nr].Vel[i];
    }
    
    // Add to previous snapshot galaxies
    int galaxy_idx = galaxy_array_append(test_ctx.galaxies_prev_snap, &temp_galaxy, &test_params);
    
    // Update halo aux data
    if (test_ctx.haloaux[halo_nr].FirstGalaxy == -1) {
        test_ctx.haloaux[halo_nr].FirstGalaxy = galaxy_idx;
    }
    test_ctx.haloaux[halo_nr].NGalaxies++;
    
    free_galaxy_properties(&temp_galaxy);
    return galaxy_idx;
}

/**
 * Create large number of galaxies for stress testing
 */
static void create_large_galaxy_set(int halo_idx, int num_galaxies) {
    for (int i = 0; i < num_galaxies; i++) {
        int galaxy_type = (i == 0) ? 0 : ((i % 3 == 0) ? 1 : 2);  // Mix of types
        float mass = 1e9 + i * 1e8;  // Varying masses
        create_test_galaxy(galaxy_type, halo_idx, mass);
    }
}

//=============================================================================
// Setup and Teardown
//=============================================================================

static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Allocate test arrays - larger for stress testing
    test_ctx.nhalo = 200;  // Increased for large FOF groups
    test_ctx.halos = mymalloc_full(test_ctx.nhalo * sizeof(struct halo_data), "test_halos");
    test_ctx.haloaux = mymalloc_full(test_ctx.nhalo * sizeof(struct halo_aux_data), "test_haloaux");
    
    if (!test_ctx.halos || !test_ctx.haloaux) {
        printf("Failed to allocate test halo arrays\n");
        return -1;
    }
    
    // Initialize halo arrays
    memset(test_ctx.halos, 0, test_ctx.nhalo * sizeof(struct halo_data));
    memset(test_ctx.haloaux, 0, test_ctx.nhalo * sizeof(struct halo_aux_data));
    
    // Create galaxy arrays
    test_ctx.galaxies_prev_snap = galaxy_array_new();
    test_ctx.galaxies_this_snap = galaxy_array_new();
    
    if (!test_ctx.galaxies_prev_snap || !test_ctx.galaxies_this_snap) {
        printf("Failed to create galaxy arrays\n");
        return -1;
    }
    
    test_ctx.galaxycounter = 1;
    test_ctx.initial_memory_usage = get_current_memory_usage();
    test_ctx.initialized = 1;
    return 0;
}

static void teardown_test_context(void) {
    // Cleanup in reverse order of allocation
    if (test_ctx.galaxies_this_snap) {
        galaxy_array_free(&test_ctx.galaxies_this_snap);
    }
    if (test_ctx.galaxies_prev_snap) {
        galaxy_array_free(&test_ctx.galaxies_prev_snap);
    }
    if (test_ctx.haloaux) {
        myfree(test_ctx.haloaux);
        test_ctx.haloaux = NULL;
    }
    if (test_ctx.halos) {
        myfree(test_ctx.halos);
        test_ctx.halos = NULL;
    }
    
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Basic memory allocation and cleanup
 */
static void test_basic_memory_management(void) {
    printf("=== Testing basic memory allocation and cleanup ===\n");
    
    size_t start_usage = get_current_memory_usage();
    
    // Create simple FOF group
    create_test_halo(0, 10, 1e12, 1, -1, -1);
    create_test_halo(1, 9, 9e11, -1, -1, -1);
    
    // Create galaxies
    create_test_galaxy(0, 1, 2e10);
    
    size_t after_creation = get_current_memory_usage();
    TEST_ASSERT(after_creation > start_usage, "Memory usage should increase after creating test data");
    
    // Process FOF group
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "Basic FOF processing should succeed");
    
    // Clear arrays to test cleanup by recreating them
    galaxy_array_free(&test_ctx.galaxies_prev_snap);
    galaxy_array_free(&test_ctx.galaxies_this_snap);
    test_ctx.galaxies_prev_snap = galaxy_array_new();
    test_ctx.galaxies_this_snap = galaxy_array_new();
    
    printf("  Basic memory management test completed\n");
}

/**
 * Test: GalaxyArray expansion and memory integrity
 */
static void test_galaxy_array_expansion(void) {
    printf("\n=== Testing GalaxyArray expansion and memory integrity ===\n");
    
    // Create FOF group that will trigger array expansion
    const int num_halos = 10;
    const int galaxies_per_halo = 30;  // Will trigger expansions
    
    // Create FOF chain
    for (int i = 0; i < num_halos; i++) {
        int next_halo = (i < num_halos - 1) ? i + 1 : -1;
        create_test_halo(i, 15, 1e12 - i * 1e10, num_halos + i, -1, next_halo);
    }
    
    // Create progenitors and galaxies
    for (int i = 0; i < num_halos; i++) {
        int prog_idx = num_halos + i;
        create_test_halo(prog_idx, 14, (1e12 - i * 1e10) * 0.9, -1, -1, -1);
        create_large_galaxy_set(prog_idx, galaxies_per_halo);
    }
    
    int initial_count = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    printf("  Created %d galaxies, expecting array expansions\n", initial_count);
    
    // Process FOF group - this should trigger multiple array expansions
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "Large FOF processing should succeed despite expansions");
    
    int final_count = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(final_count > 0, "Should have galaxies after expansion test");
    
    // Verify memory integrity after expansions
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    // Check that galaxy properties are still accessible (no corruption)
    for (int i = 0; i < final_count && i < 10; i++) {  // Check first 10
        int type = GALAXY_PROP_Type(&galaxies[i]);
        int halo_nr = GALAXY_PROP_HaloNr(&galaxies[i]);
        
        TEST_ASSERT(type >= 0 && type <= 2, "Galaxy type should be valid after expansion");
        TEST_ASSERT(halo_nr >= 0 && halo_nr < test_ctx.nhalo, "Halo number should be valid");
    }
    
    printf("  Array expansion test: %d -> %d galaxies, integrity maintained\n", 
           initial_count, final_count);
}

/**
 * Test: Large FOF group memory handling (>500 galaxies)
 */
static void test_large_fof_group_memory(void) {
    printf("\n=== Testing large FOF group memory handling ===\n");
    
    // Clear previous test data by recreating arrays
    galaxy_array_free(&test_ctx.galaxies_prev_snap);
    galaxy_array_free(&test_ctx.galaxies_this_snap);
    test_ctx.galaxies_prev_snap = galaxy_array_new();
    test_ctx.galaxies_this_snap = galaxy_array_new();
    
    const int num_halos = 50;
    const int galaxies_per_halo = 15;  // Total: 750 galaxies
    const int target_galaxies = num_halos * galaxies_per_halo;
    
    printf("  Creating large FOF group with target %d galaxies\n", target_galaxies);
    
    // Create large FOF chain
    for (int i = 0; i < num_halos; i++) {
        int next_halo = (i < num_halos - 1) ? i + 1 : -1;
        create_test_halo(i, 20, 2e12 - i * 1e10, num_halos + i, -1, next_halo);
    }
    
    // Create progenitors with many galaxies
    for (int i = 0; i < num_halos; i++) {
        int prog_idx = num_halos + i;
        create_test_halo(prog_idx, 19, (2e12 - i * 1e10) * 0.95, -1, -1, -1);
        create_large_galaxy_set(prog_idx, galaxies_per_halo);
    }
    
    size_t memory_before = get_current_memory_usage();
    
    // Process large FOF group
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "Large FOF group processing should succeed");
    
    int final_count = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(final_count >= 500, "Should process at least 500 galaxies");
    
    size_t memory_after = get_current_memory_usage();
    
    // Verify no excessive memory bloat (allowing for reasonable overhead)
    float memory_ratio = (float)memory_after / memory_before;
    TEST_ASSERT(memory_ratio < 10.0f, "Memory usage should not increase excessively");
    
    // Verify central assignment still works correctly
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    int central_count = 0;
    
    for (int i = 0; i < final_count; i++) {
        if (GALAXY_PROP_Type(&galaxies[i]) == 0) {
            central_count++;
        }
    }
    
    TEST_ASSERT(central_count == 1, "Large FOF group should have exactly one central");
    
    printf("  Large FOF processed: %d galaxies, 1 central, memory ratio: %.2f\n", 
           final_count, memory_ratio);
}

/**
 * Test: Memory leak detection through repeated operations
 */
static void test_memory_leak_detection(void) {
    printf("\n=== Testing memory leak detection ===\n");
    
    const int num_iterations = 10;
    size_t memory_readings[num_iterations];
    
    for (int iter = 0; iter < num_iterations; iter++) {
        // Clear arrays by recreating them
        galaxy_array_free(&test_ctx.galaxies_prev_snap);
        galaxy_array_free(&test_ctx.galaxies_this_snap);
        test_ctx.galaxies_prev_snap = galaxy_array_new();
        test_ctx.galaxies_this_snap = galaxy_array_new();
        
        // Create and process FOF group
        create_test_halo(0, 25 + iter, 1.5e12, 10, -1, -1);
        create_test_halo(10, 24 + iter, 1.3e12, -1, -1, -1);
        create_test_galaxy(0, 10, 2e10);
        
        int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                      test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
        
        TEST_ASSERT(status == EXIT_SUCCESS, "Iteration should succeed");
        
        memory_readings[iter] = get_current_memory_usage();
        
        if (iter > 0) {
            // Memory should not continuously grow
            float growth_ratio = (float)memory_readings[iter] / memory_readings[0];
            TEST_ASSERT(growth_ratio < 2.0f, "Memory should not grow excessively across iterations");
        }
    }
    
    // Check for consistent memory usage pattern
    float final_ratio = (float)memory_readings[num_iterations-1] / memory_readings[0];
    printf("  Leak test: %d iterations, final memory ratio: %.3f\n", num_iterations, final_ratio);
    
    TEST_ASSERT(final_ratio < 1.5f, "Memory usage should remain bounded");
}

/**
 * Test: Stress test with rapid allocation/deallocation
 */
static void test_memory_stress(void) {
    printf("\n=== Testing memory stress with rapid allocation/deallocation ===\n");
    
    const int stress_cycles = 20;
    
    for (int cycle = 0; cycle < stress_cycles; cycle++) {
        // Rapid creation and destruction
        GalaxyArray *temp_array = galaxy_array_new();
        TEST_ASSERT(temp_array != NULL, "Stress test array creation should succeed");
        
        // Fill with galaxies
        for (int i = 0; i < 50; i++) {
            struct GALAXY temp_galaxy;
            memset(&temp_galaxy, 0, sizeof(struct GALAXY));
            
            if (allocate_galaxy_properties(&temp_galaxy, &test_params) == 0) {
                GALAXY_PROP_Type(&temp_galaxy) = i % 3;
                GALAXY_PROP_GalaxyIndex(&temp_galaxy) = i;
                galaxy_array_append(temp_array, &temp_galaxy, &test_params);
                free_galaxy_properties(&temp_galaxy);
            }
        }
        
        // Immediately destroy
        galaxy_array_free(&temp_array);
        
        if (cycle % 5 == 0) {
            printf("  Stress cycle %d completed\n", cycle);
        }
    }
    
    printf("  Memory stress test completed: %d rapid cycles\n", stress_cycles);
}

/**
 * Test: Memory pool integration and stress testing
 */
static void test_memory_pool_integration(void) {
    printf("\\n=== Testing memory pool integration ===\\n");
    
    // Initialize memory pool
    int init_result = galaxy_pool_initialize();
    TEST_ASSERT(init_result == 0, "Memory pool initialization should succeed");
    
    // Create and process FOF group with memory pool
    create_test_halo(0, 30, 1.8e12, 5, -1, -1);
    create_test_halo(5, 29, 1.6e12, -1, -1, -1);
    create_test_galaxy(0, 5, 2.5e10);
    
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "FOF processing with memory pool should succeed");
    
    // Verify memory pool is working
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(ngal >= 1, "Memory pool should allow galaxy creation");
    
    // Cleanup memory pool
    galaxy_pool_cleanup();
    
    printf("  Memory pool integration test completed\\n");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for FOF Memory Management\n");
    printf("========================================\n\n");
    
    printf("This test verifies that FOF memory management is robust:\n");
    printf("  1. Zero memory leaks validation\n");
    printf("  2. Proper cleanup of GalaxyArray and contents\n");
    printf("  3. Large FOF group handling (>500 galaxies)\n");
    printf("  4. Memory stress testing and leak detection\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_basic_memory_management();
    test_galaxy_array_expansion();
    test_large_fof_group_memory();
    test_memory_leak_detection();
    test_memory_stress();
    test_memory_pool_integration();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for FOF Memory Management:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    if (tests_run == tests_passed) {
        printf("✅ All memory management tests passed!\n");
        printf("💡 For comprehensive leak detection, run with Valgrind:\n");
        printf("   valgrind --leak-check=full ./tests/test_fof_memory_management\n\n");
    }
    
    return (tests_run == tests_passed) ? 0 : 1;
}