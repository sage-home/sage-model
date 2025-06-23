/**
 * Simplified test for Orphan Galaxy Tracking
 * 
 * Tests core functionality:
 * - Satellite galaxies becoming orphans when host halo disappears
 * - Error handling for NULL parameters
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"
#include "core/core_allvars.h"
#include "core/core_build_model.h"
#include "core/core_mymalloc.h"

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

// Test context using standardized test helper
static struct TestContext test_ctx;

static void test_basic_orphan_functionality(void) {
    printf("=== Testing basic orphan functionality ===\n");
    
    // Setup halo merger tree: halo 0 has descendant halo 2, halo 1 disappears
    test_ctx.halos[0].Descendant = 2;  // Central halo survives as halo 2
    test_ctx.halos[1].Descendant = -1; // Satellite halo disappears
    test_ctx.halos[1].FirstHaloInFOFgroup = 0;  // Satellite belongs to FOF group 0
    test_ctx.halos[2].FirstHaloInFOFgroup = 2;  // New FOF group
    
    // Create a simple galaxy in the disappearing halo
    int galaxy_idx = create_test_galaxy(&test_ctx, 1, 1, 1e11f);  // Satellite in halo 1
    TEST_ASSERT(galaxy_idx >= 0, "Should create test galaxy successfully");
    
    // Setup processed flags
    int ngal_prev = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    bool *processed_flags = mycalloc(ngal_prev, sizeof(bool));
    processed_flags[0] = false;  // Galaxy not processed (halo disappeared)
    
    // Create output galaxy array for orphans
    GalaxyArray *current_galaxies = galaxy_array_new();
    
    // Test: Call identify_and_process_orphans for FOF group 2
    int result = identify_and_process_orphans(2, current_galaxies, test_ctx.galaxies_prev_snap,
                                            processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "identify_and_process_orphans should succeed");
    
    // Verify: The galaxy should become an orphan
    int ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 1, "Current galaxies should contain 1 orphan");
    
    if (ngal_current > 0) {
        struct GALAXY *orphan = galaxy_array_get_raw_data(current_galaxies);
        TEST_ASSERT(GALAXY_PROP_Type(orphan) == 2, "Galaxy should be Type 2 (orphan)");
        TEST_ASSERT(GALAXY_PROP_merged(orphan) == 0, "Orphan should remain active");
    }
    
    // Verify: Processed flag should be set
    TEST_ASSERT(processed_flags[0] == true, "Galaxy should be marked as processed");
    
    // Cleanup
    galaxy_array_free(&current_galaxies);
    myfree(processed_flags);
}

static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    // Test NULL parameters
    int result = identify_and_process_orphans(0, NULL, test_ctx.galaxies_prev_snap, 
                                            NULL, test_ctx.halos, &test_ctx.test_params);
    TEST_ASSERT(result == EXIT_FAILURE, "Should fail with NULL temp_fof_galaxies");
    
    // Test with valid parameters (should succeed)
    GalaxyArray *test_array = galaxy_array_new();
    result = identify_and_process_orphans(0, test_array, NULL, NULL, test_ctx.halos, &test_ctx.test_params);
    TEST_ASSERT(result == EXIT_SUCCESS, "Should succeed with NULL prev galaxies and flags");
    
    galaxy_array_free(&test_array);
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting SIMPLIFIED tests for Orphan Galaxy Tracking\n");
    printf("========================================\n\n");
    
    // Setup
    if (setup_test_environment(&test_ctx, 6) != 0) {
        printf("ERROR: Failed to set up test environment\n");
        return 1;
    }
    
    // Run tests
    test_basic_orphan_functionality();
    test_error_handling();
    
    // Teardown
    teardown_test_environment(&test_ctx);
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for Orphan Galaxy Tracking:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
