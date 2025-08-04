/**
 * Test suite for Orphan Galaxy Tracking
 * 
 * Tests cover:
 * - Satellite galaxies becoming orphans when host halo disappears
 * - Central galaxies becoming orphans when host halo disappears
 * - Successful galaxy inheritance when halos have descendants
 * - Multi-progenitor merger handling
 * - Forward-looking orphan detection algorithm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "test_helper.h"
#include "core/core_allvars.h"
#include "core/core_build_model.h"
#include "core/galaxy_array.h"
#include "core/core_mymalloc.h"
#include "core/core_properties.h"
#include "core/core_galaxy_extensions.h"

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
static GalaxyArray *current_galaxies = NULL;
static bool *processed_flags = NULL;

// Setup function - called before tests
static int setup_test_context(void) {
    // Use standardized test helper for proper initialization
    if (setup_test_environment(&test_ctx, 6) != 0) {
        printf("ERROR: Failed to set up test environment\n");
        return -1;
    }
    
    // Initialize halos with test-specific values
    for (int i = 0; i < test_ctx.nhalo; i++) {
        test_ctx.halos[i].Descendant = -1;  // No descendant by default
        test_ctx.halos[i].FirstProgenitor = -1;
        test_ctx.halos[i].NextProgenitor = -1;
        test_ctx.halos[i].FirstHaloInFOFgroup = i;  // Self-referencing by default
        test_ctx.halos[i].NextHaloInFOFgroup = -1;
        test_ctx.halos[i].Len = 100;
        test_ctx.halos[i].Vmax = 150.0f;
        test_ctx.halos[i].MostBoundID = i + 1000;
        test_ctx.halos[i].Pos[0] = i * 10.0f;
        test_ctx.halos[i].Pos[1] = i * 10.0f + 5.0f;
        test_ctx.halos[i].Pos[2] = i * 10.0f + 10.0f;
        test_ctx.halos[i].Vel[0] = i * 5.0f;
        test_ctx.halos[i].Vel[1] = i * 5.0f + 2.0f;
        test_ctx.halos[i].Vel[2] = i * 5.0f + 4.0f;
    }
    
    // Create additional current galaxy array for orphan processing
    current_galaxies = galaxy_array_new();
    if (!current_galaxies) {
        printf("ERROR: Failed to create current galaxy array\n");
        return -1;
    }
    
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (current_galaxies) {
        galaxy_array_free(&current_galaxies);
        current_galaxies = NULL;
    }
    if (processed_flags) {
        myfree(processed_flags);
        processed_flags = NULL;
    }
    
    // Use standardized test helper cleanup
    teardown_test_environment(&test_ctx);
}

// Helper function to create a mock galaxy
static void create_mock_galaxy(struct GALAXY *galaxy, int64_t galaxy_nr, int halo_nr, 
                              int type, int central_gal, float mvir) {
    memset(galaxy, 0, sizeof(struct GALAXY));
    galaxy_extension_initialize(galaxy);
    
    // Allocate properties using properly initialized test parameters
    if (allocate_galaxy_properties(galaxy, &test_ctx.test_params) != 0) {
        printf("ERROR: Failed to allocate galaxy properties in mock galaxy creation\n");
        return;
    }
    
    // Set basic properties
    GALAXY_PROP_GalaxyNr(galaxy) = galaxy_nr;
    GALAXY_PROP_HaloNr(galaxy) = halo_nr;
    GALAXY_PROP_Type(galaxy) = type;
    GALAXY_PROP_CentralGal(galaxy) = central_gal;
    GALAXY_PROP_merged(galaxy) = 0;  // Active galaxy
    GALAXY_PROP_Mvir(galaxy) = mvir;
    GALAXY_PROP_Rvir(galaxy) = 200.0f;
    GALAXY_PROP_Vvir(galaxy) = 100.0f;
    GALAXY_PROP_Len(galaxy) = test_ctx.halos[halo_nr].Len;
    GALAXY_PROP_Vmax(galaxy) = test_ctx.halos[halo_nr].Vmax;
    GALAXY_PROP_MostBoundID(galaxy) = test_ctx.halos[halo_nr].MostBoundID;
    
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos(galaxy)[i] = test_ctx.halos[halo_nr].Pos[i];
        GALAXY_PROP_Vel(galaxy)[i] = test_ctx.halos[halo_nr].Vel[i];
    }
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Satellite galaxy becomes orphan when host halo disappears
 */
static void test_satellite_becomes_orphan(void) {
    printf("=== Testing satellite galaxy becomes orphan ===\n");
    
    // Setup: Previous snapshot has a satellite galaxy in halo 1
    // Current snapshot: halo 1 disappears, but its central (halo 0) has descendant in halo 2
    
    // Configure halos: halo 0 (central) has descendant halo 2, halo 1 (satellite) has no descendant
    test_ctx.halos[0].Descendant = 2;  // Central halo survives as halo 2
    test_ctx.halos[1].Descendant = -1; // Satellite halo disappears
    test_ctx.halos[1].FirstHaloInFOFgroup = 0;  // Satellite belongs to FOF group 0
    test_ctx.halos[2].FirstHaloInFOFgroup = 2;  // New FOF group
    
    // Create previous snapshot galaxies
    struct GALAXY prev_central, prev_satellite;
    create_mock_galaxy(&prev_central, 1001, 0, 0, 0, 1e12f);  // Central in halo 0
    create_mock_galaxy(&prev_satellite, 1002, 1, 1, 0, 1e11f); // Satellite in halo 1, central is halo 0
    
    galaxy_array_append(test_ctx.galaxies_prev_snap, &prev_central, &test_ctx.test_params);
    galaxy_array_append(test_ctx.galaxies_prev_snap, &prev_satellite, &test_ctx.test_params);
    
    // Setup processed flags
    int ngal_prev = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    processed_flags = mycalloc(ngal_prev, sizeof(bool));
    
    // Simulate normal processing: central galaxy gets processed (flag set to true)
    processed_flags[0] = true;   // Central processed
    processed_flags[1] = false;  // Satellite NOT processed (halo disappeared)
    
    // Test: Call identify_and_process_orphans for FOF group 2
    int result = identify_and_process_orphans(2, current_galaxies, test_ctx.galaxies_prev_snap,
                                            processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "identify_and_process_orphans should succeed");
    
    // Verify: The satellite should now be in current_galaxies as Type 2 orphan
    int ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 1, "Current galaxies should contain 1 orphan");
    
    if (ngal_current > 0) {
        struct GALAXY *orphan = galaxy_array_get_raw_data(current_galaxies);
        TEST_ASSERT(GALAXY_PROP_Type(orphan) == 2, "Galaxy should be Type 2 (orphan)");
        TEST_ASSERT(GALAXY_PROP_GalaxyNr(orphan) == 1002, "Should be the original satellite galaxy");
        TEST_ASSERT(GALAXY_PROP_merged(orphan) == 1, "Orphan should be marked for output filtering");
        TEST_ASSERT(GALAXY_PROP_Mvir(orphan) == 0.0f, "Orphan should have zero halo mass");
    }
    
    // Verify: Processed flag should be set
    TEST_ASSERT(processed_flags[1] == true, "Satellite should be marked as processed");
    
    // Cleanup
    free_galaxy_properties(&prev_central);
    free_galaxy_properties(&prev_satellite);
}

/**
 * Test: Central galaxy becomes orphan when host halo disappears
 */
static void test_central_becomes_orphan(void) {
    printf("\n=== Testing central galaxy becomes orphan ===\n");
    
    // Reset for this test
    reset_test_galaxies(&test_ctx);
    galaxy_array_free(&current_galaxies);
    current_galaxies = galaxy_array_new();
    if (processed_flags) {
        myfree(processed_flags);
        processed_flags = NULL;
    }
    
    // Setup: Small FOF (halo 3) merges entirely into larger FOF (halo 4)
    // Halo 3 central has no descendant, but its central (itself) points to halo 4's descendant
    test_ctx.halos[3].Descendant = -1;  // Small FOF central disappears
    test_ctx.halos[3].FirstHaloInFOFgroup = 3;  // Self-referencing (central)
    test_ctx.halos[4].Descendant = 5;   // Large FOF central survives as halo 5
    test_ctx.halos[5].FirstHaloInFOFgroup = 5;  // New FOF group
    
    // Create previous snapshot galaxy
    struct GALAXY prev_central;
    create_mock_galaxy(&prev_central, 2001, 3, 0, 3, 5e11f);  // Central in halo 3, central is itself
    
    galaxy_array_append(test_ctx.galaxies_prev_snap, &prev_central, &test_ctx.test_params);
    
    // Setup processed flags
    int ngal_prev = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    processed_flags = mycalloc(ngal_prev, sizeof(bool));
    processed_flags[0] = false;  // Central NOT processed (halo disappeared)
    
    // Test: Call identify_and_process_orphans for FOF group 5
    // This should NOT create an orphan because the central's central (itself) has no descendant
    int result = identify_and_process_orphans(5, current_galaxies, test_ctx.galaxies_prev_snap,
                                            processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "identify_and_process_orphans should succeed");
    
    // For this test case, we need to set up the merger tree correctly
    // The central galaxy's central is itself (halo 3), but halo 3 has no descendant
    // So this galaxy should not become an orphan in FOF group 5
    int ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 0, "No orphan should be created (central's central has no descendant)");
    
    // Now test the case where the central SHOULD become an orphan
    // We need to create a completely different scenario with proper setup
    
    // Clean up previous test setup
    free_galaxy_properties(&prev_central);
    reset_test_galaxies(&test_ctx);
    galaxy_array_free(&current_galaxies);
    current_galaxies = galaxy_array_new();
    myfree(processed_flags);
    
    // Setup proper scenario: halo 3 is a satellite of halo 0, both halos disappear
    // but halo 0 (the central) has a descendant in halo 5
    test_ctx.halos[0].Descendant = 5;   // Central halo 0 has descendant halo 5
    test_ctx.halos[3].Descendant = -1;  // Satellite halo 3 disappears 
    test_ctx.halos[3].FirstHaloInFOFgroup = 0;  // Halo 3 belongs to FOF group 0
    test_ctx.halos[5].FirstHaloInFOFgroup = 5;  // New FOF group
    
    // Create a galaxy that was central in its disappeared halo, but belonged to FOF group 0
    create_mock_galaxy(&prev_central, 2001, 3, 0, 0, 5e11f);  // Central in halo 3, but central FOF is halo 0
    galaxy_array_append(test_ctx.galaxies_prev_snap, &prev_central, &test_ctx.test_params);
    
    // Setup processed flags
    ngal_prev = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    processed_flags = mycalloc(ngal_prev, sizeof(bool));
    processed_flags[0] = false;  // Galaxy not processed (halo disappeared)
    
    result = identify_and_process_orphans(5, current_galaxies, test_ctx.galaxies_prev_snap,
                                        processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 1, "Orphan should be created for disrupted central");
    
    if (ngal_current > 0) {
        struct GALAXY *orphan = galaxy_array_get_raw_data(current_galaxies);
        TEST_ASSERT(GALAXY_PROP_Type(orphan) == 2, "Galaxy should be Type 2 (orphan)");
        TEST_ASSERT(GALAXY_PROP_merged(orphan) == 1, "Orphan should be marked for output filtering");
    }
    
    // Cleanup
    free_galaxy_properties(&prev_central);
}

/**
 * Test: Successful galaxy inheritance when halo has descendant
 */
static void test_successful_inheritance(void) {
    printf("\n=== Testing successful galaxy inheritance ===\n");
    
    // Reset for this test
    reset_test_galaxies(&test_ctx);
    galaxy_array_free(&current_galaxies);
    current_galaxies = galaxy_array_new();
    if (processed_flags) {
        myfree(processed_flags);
        processed_flags = NULL;
    }
    
    // Setup: Halo 0 has descendant halo 2, normal inheritance should occur
    test_ctx.halos[0].Descendant = 2;
    test_ctx.halos[2].FirstHaloInFOFgroup = 2;
    
    // Create previous snapshot galaxy
    struct GALAXY prev_galaxy;
    create_mock_galaxy(&prev_galaxy, 3001, 0, 0, 0, 2e12f);
    
    galaxy_array_append(test_ctx.galaxies_prev_snap, &prev_galaxy, &test_ctx.test_params);
    
    // Setup processed flags
    int ngal_prev = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    processed_flags = mycalloc(ngal_prev, sizeof(bool));
    processed_flags[0] = true;  // Galaxy successfully processed by normal inheritance
    
    // Test: Call identify_and_process_orphans
    int result = identify_and_process_orphans(2, current_galaxies, test_ctx.galaxies_prev_snap,
                                            processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "identify_and_process_orphans should succeed");
    
    // Verify: No orphans should be created (galaxy was already processed)
    int ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 0, "No orphans should be created for successfully inherited galaxies");
    
    // Cleanup
    free_galaxy_properties(&prev_galaxy);
}

/**
 * Test: Multi-progenitor merger handling
 */
static void test_multi_progenitor_merger(void) {
    printf("\n=== Testing multi-progenitor merger handling ===\n");
    
    // Reset for this test
    reset_test_galaxies(&test_ctx);
    galaxy_array_free(&current_galaxies);
    current_galaxies = galaxy_array_new();
    if (processed_flags) {
        myfree(processed_flags);
        processed_flags = NULL;
    }
    
    // Setup: Two halos (0 and 1) both have descendant halo 2 (merger)
    test_ctx.halos[0].Descendant = 2;  // Primary progenitor
    test_ctx.halos[1].Descendant = 2;  // Secondary progenitor
    test_ctx.halos[2].FirstHaloInFOFgroup = 2;
    
    // Create galaxies in both progenitor halos
    struct GALAXY primary_galaxy, secondary_galaxy;
    create_mock_galaxy(&primary_galaxy, 4001, 0, 0, 0, 3e12f);    // Primary
    create_mock_galaxy(&secondary_galaxy, 4002, 1, 1, 0, 1e12f);  // Secondary (satellite)
    
    galaxy_array_append(test_ctx.galaxies_prev_snap, &primary_galaxy, &test_ctx.test_params);
    galaxy_array_append(test_ctx.galaxies_prev_snap, &secondary_galaxy, &test_ctx.test_params);
    
    // Setup processed flags: both galaxies processed by normal merger logic
    int ngal_prev = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    processed_flags = mycalloc(ngal_prev, sizeof(bool));
    processed_flags[0] = true;  // Primary processed
    processed_flags[1] = true;  // Secondary processed
    
    // Test: Call identify_and_process_orphans
    int result = identify_and_process_orphans(2, current_galaxies, test_ctx.galaxies_prev_snap,
                                            processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "identify_and_process_orphans should succeed");
    
    // Verify: No orphans created (all galaxies handled by existing merger logic)
    int ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 0, "No orphans should be created in multi-progenitor mergers");
    
    // Test edge case: one galaxy not processed (simulating an error in normal processing)
    galaxy_array_free(&current_galaxies);
    current_galaxies = galaxy_array_new();
    processed_flags[1] = false;  // Secondary not processed
    
    result = identify_and_process_orphans(2, current_galaxies, test_ctx.galaxies_prev_snap,
                                        processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 1, "Unprocessed galaxy should become orphan");
    
    if (ngal_current > 0) {
        struct GALAXY *orphan = galaxy_array_get_raw_data(current_galaxies);
        TEST_ASSERT(GALAXY_PROP_GalaxyNr(orphan) == 4002, "Should be the secondary galaxy");
        TEST_ASSERT(GALAXY_PROP_Type(orphan) == 2, "Should be Type 2 orphan");
    }
    
    // Cleanup
    free_galaxy_properties(&primary_galaxy);
    free_galaxy_properties(&secondary_galaxy);
}

/**
 * Test: Error handling and edge cases
 */
static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    // Test NULL parameters
    int result = identify_and_process_orphans(0, NULL, test_ctx.galaxies_prev_snap, 
                                            processed_flags, test_ctx.halos, &test_ctx.test_params);
    TEST_ASSERT(result == EXIT_FAILURE, "Should fail with NULL temp_fof_galaxies");
    
    // Test NULL previous galaxies (should succeed and do nothing)
    result = identify_and_process_orphans(0, current_galaxies, NULL,
                                        processed_flags, test_ctx.halos, &test_ctx.test_params);
    TEST_ASSERT(result == EXIT_SUCCESS, "Should succeed with NULL prev galaxies");
    
    // Test NULL processed flags (should succeed and do nothing)
    result = identify_and_process_orphans(0, current_galaxies, test_ctx.galaxies_prev_snap,
                                        NULL, test_ctx.halos, &test_ctx.test_params);
    TEST_ASSERT(result == EXIT_SUCCESS, "Should succeed with NULL processed flags");
    
    // Test empty previous galaxy array
    reset_test_galaxies(&test_ctx);
    if (processed_flags) {
        myfree(processed_flags);
        processed_flags = mycalloc(0, sizeof(bool));  // Zero size
    }
    
    result = identify_and_process_orphans(0, current_galaxies, test_ctx.galaxies_prev_snap,
                                        processed_flags, test_ctx.halos, &test_ctx.test_params);
    TEST_ASSERT(result == EXIT_SUCCESS, "Should succeed with empty previous galaxy array");
}

/**
 * Test: Forward-looking detection algorithm validation
 */
static void test_forward_looking_algorithm(void) {
    printf("\n=== Testing forward-looking detection algorithm ===\n");
    
    // Reset for this test
    reset_test_galaxies(&test_ctx);
    galaxy_array_free(&current_galaxies);
    current_galaxies = galaxy_array_new();
    if (processed_flags) {
        myfree(processed_flags);
        processed_flags = NULL;
    }
    
    // Setup complex scenario: multiple galaxies, some become orphans, some don't
    // Halo 0: survives as halo 3 (central)
    // Halo 1: disappears (satellite of halo 0)
    // Halo 2: disappears (satellite of halo 0) 
    test_ctx.halos[0].Descendant = 3;
    test_ctx.halos[1].Descendant = -1;  // Disappears
    test_ctx.halos[2].Descendant = -1;  // Disappears
    test_ctx.halos[1].FirstHaloInFOFgroup = 0;  // Part of FOF group 0
    test_ctx.halos[2].FirstHaloInFOFgroup = 0;  // Part of FOF group 0
    test_ctx.halos[3].FirstHaloInFOFgroup = 3;  // New FOF group
    
    // Create galaxies
    struct GALAXY gal_central, gal_sat1, gal_sat2;
    create_mock_galaxy(&gal_central, 5001, 0, 0, 0, 5e12f);    // Central survives
    create_mock_galaxy(&gal_sat1, 5002, 1, 1, 0, 1e11f);      // Satellite 1 becomes orphan
    create_mock_galaxy(&gal_sat2, 5003, 2, 1, 0, 8e10f);      // Satellite 2 becomes orphan
    
    galaxy_array_append(test_ctx.galaxies_prev_snap, &gal_central, &test_ctx.test_params);
    galaxy_array_append(test_ctx.galaxies_prev_snap, &gal_sat1, &test_ctx.test_params);
    galaxy_array_append(test_ctx.galaxies_prev_snap, &gal_sat2, &test_ctx.test_params);
    
    // Setup processed flags
    int ngal_prev = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    processed_flags = mycalloc(ngal_prev, sizeof(bool));
    processed_flags[0] = true;   // Central processed normally
    processed_flags[1] = false;  // Satellite 1 not processed
    processed_flags[2] = false;  // Satellite 2 not processed
    
    // Test: Call identify_and_process_orphans
    int result = identify_and_process_orphans(3, current_galaxies, test_ctx.galaxies_prev_snap,
                                            processed_flags, test_ctx.halos, &test_ctx.test_params);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "identify_and_process_orphans should succeed");
    
    // Verify: Both satellites should become orphans
    int ngal_current = galaxy_array_get_count(current_galaxies);
    TEST_ASSERT(ngal_current == 2, "Should create 2 orphan galaxies");
    
    // Verify processed flags are updated
    TEST_ASSERT(processed_flags[1] == true, "Satellite 1 should be marked processed");
    TEST_ASSERT(processed_flags[2] == true, "Satellite 2 should be marked processed");
    
    // Verify orphan properties
    if (ngal_current >= 2) {
        struct GALAXY *orphans = galaxy_array_get_raw_data(current_galaxies);
        for (int i = 0; i < ngal_current; i++) {
            TEST_ASSERT(GALAXY_PROP_Type(&orphans[i]) == 2, "All should be Type 2 orphans");
            TEST_ASSERT(GALAXY_PROP_merged(&orphans[i]) == 1, "All orphans should be marked for output filtering");
            TEST_ASSERT(GALAXY_PROP_Mvir(&orphans[i]) == 0.0f, "Orphans should have zero halo mass");
        }
    }
    
    // Cleanup
    free_galaxy_properties(&gal_central);
    free_galaxy_properties(&gal_sat1);
    free_galaxy_properties(&gal_sat2);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for Orphan Galaxy Tracking\n");
    printf("========================================\n\n");
    
    printf("This test verifies that the orphan galaxy tracking system:\n");
    printf("  1. Converts satellites to orphans when host halos disappear\n");
    printf("  2. Converts centrals to orphans when host halos disappear\n");
    printf("  3. Does not interfere with successful galaxy inheritance\n");
    printf("  4. Handles multi-progenitor mergers correctly\n");
    printf("  5. Implements forward-looking detection algorithm properly\n");
    printf("  6. Handles error conditions gracefully\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_satellite_becomes_orphan();
    test_central_becomes_orphan();
    test_successful_inheritance();
    test_multi_progenitor_merger();
    test_error_handling();
    test_forward_looking_algorithm();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for Orphan Galaxy Tracking:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
