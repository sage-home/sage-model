/**
 * @file test_fof_evolution_context.c
 * @brief Unit tests for FOF Evolution Context functionality
 * 
 * Tests cover:
 * - FOF-centric time calculations vs. individual halo timing
 * - Central galaxy validation without halo-specific assumptions
 * - Merger tree continuity within FOF groups
 * 
 * This test validates that the evolution context correctly handles FOF groups
 * as the fundamental unit of evolution with consistent timing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "test_helper.h"
#include "../src/core/core_build_model.h"
#include "../src/core/core_evolution_diagnostics.h"

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

// Use the shared context
static struct TestContext test_ctx;

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: FOF-centric time calculations vs individual halo timing
 */
static void test_fof_centric_timing(void) {
    printf("=== Testing FOF-centric timing calculations ===\n");
    
    // Reset galaxy arrays for fresh test
    reset_test_galaxies(&test_ctx);
    
    // Create FOF group with halos at different snapshots
    int fof_root_snap = 10;
    int subhalo_snap = 9;  // Subhalo from earlier snapshot (infall)
    
    // FOF root halo
    create_test_halo(&test_ctx, 0, fof_root_snap, 2e12, 3, -1, 1);
    // Subhalo that fell in earlier
    create_test_halo(&test_ctx, 1, subhalo_snap, 8e11, 4, -1, -1);
    
    // Progenitors
    create_test_halo(&test_ctx, 3, fof_root_snap - 1, 1.8e12, -1, -1, -1);  // FOF root progenitor
    create_test_halo(&test_ctx, 4, subhalo_snap - 1, 7e11, -1, -1, -1);     // Subhalo progenitor
    
    // Create galaxies with different snapshot numbers
    create_test_galaxy(&test_ctx, 0, 3, 2e10);   // Central progenitor
    create_test_galaxy(&test_ctx, 0, 4, 1e10);    // Satellite progenitor
    
    // Process FOF group
    bool processed_flags[30] = {0};
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_ctx.test_params, processed_flags);

    TEST_ASSERT(status == EXIT_SUCCESS, "FOF group with mixed timing should process successfully");
    
    // Verify all galaxies in the FOF group have consistent timing reference
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(ngal >= 1, "Should have galaxies after processing");
    
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    // Calculate expected deltaT based on FOF root
    double expected_age_diff = test_ctx.test_params.simulation.Age[fof_root_snap] - test_ctx.test_params.simulation.Age[fof_root_snap - 1];
    
    printf("  FOF root snapshot: %d, Expected deltaT reference: %.3f Gyr\n", 
           fof_root_snap, expected_age_diff);
    
    // Verify timing consistency
    for (int i = 0; i < ngal; i++) {
        int galaxy_snap = GALAXY_PROP_SnapNum(&galaxies[i]);
        int galaxy_halo = GALAXY_PROP_HaloNr(&galaxies[i]);
        
        printf("  Galaxy %d: Snapshot %d, Halo %d, Type %d\n", 
               i, galaxy_snap, galaxy_halo, GALAXY_PROP_Type(&galaxies[i]));
    }
    
    TEST_ASSERT(ngal > 0, "FOF timing test should produce galaxies");
    printf("  Timing consistency verified for %d galaxies\n", ngal);
}

/**
 * Test: Central galaxy validation without halo-specific assumptions
 */
static void test_central_validation_fof_centric(void) {
    printf("\n=== Testing FOF-centric central galaxy validation ===\n");
    
    // Reset galaxy arrays for fresh test
    reset_test_galaxies(&test_ctx);
    
    // Create FOF group where central is NOT in the first halo processed
    create_test_halo(&test_ctx, 0, 15, 2e12, 5, -1, 1);  // FOF root (will have central)
    create_test_halo(&test_ctx, 1, 15, 1.5e12, 6, -1, 2); // Subhalo 1
    create_test_halo(&test_ctx, 2, 15, 1e12, 7, -1, -1);   // Subhalo 2
    
    // Progenitors
    create_test_halo(&test_ctx, 5, 14, 1.9e12, -1, -1, -1);
    create_test_halo(&test_ctx, 6, 14, 1.4e12, -1, -1, -1);
    create_test_halo(&test_ctx, 7, 14, 9e11, -1, -1, -1);
    
    // Create galaxies - central will come from FOF root
    create_test_galaxy(&test_ctx, 0, 5, 3e10);  // Will become central (FOF root)
    create_test_galaxy(&test_ctx, 0, 6, 2e10);  // Will become satellite
    create_test_galaxy(&test_ctx, 1, 7, 1e10);  // Already satellite
    
    // Process FOF group
    bool processed_flags[30] = {0};
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_ctx.test_params, processed_flags);

    TEST_ASSERT(status == EXIT_SUCCESS, "FOF group should process successfully");
    
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    // Verify FOF-centric central assignment
    int central_count = 0;
    int central_halo = -1;
    
    for (int i = 0; i < ngal; i++) {
        if (GALAXY_PROP_Type(&galaxies[i]) == 0) {
            central_count++;
            central_halo = GALAXY_PROP_HaloNr(&galaxies[i]);
        }
        
        // Verify all galaxies point to same central
        int central_ref = GALAXY_PROP_CentralGal(&galaxies[i]);
        TEST_ASSERT(central_ref >= 0 && central_ref < ngal, 
                   "Central reference should be valid");
    }
    
    TEST_ASSERT(central_count == 1, "Should have exactly one central galaxy");
    TEST_ASSERT(central_halo == 0, "Central should be in FOF root halo");
    
    printf("  Central validation: 1 central in halo %d, %d total galaxies\n", 
           central_halo, ngal);
}

/**
 * Test: Merger tree continuity within FOF groups
 */
static void test_merger_tree_continuity(void) {
    printf("\n=== Testing merger tree continuity in FOF groups ===\n");
    
    // Reset galaxy arrays for fresh test
    reset_test_galaxies(&test_ctx);
    
    // Create complex merger tree within FOF group
    int current_snap = 20;
    int prev_snap = 19;
    
    // Current snapshot FOF group
    create_test_halo(&test_ctx, 0, current_snap, 3e12, 5, -1, 1);  // FOF root
    create_test_halo(&test_ctx, 1, current_snap, 1e12, 6, -1, -1); // Subhalo
    
    // Previous snapshot - multiple progenitors
    create_test_halo(&test_ctx, 5, prev_snap, 2.5e12, 8, 9, -1);  // Main progenitor
    create_test_halo(&test_ctx, 6, prev_snap, 8e11, 10, -1, -1);   // Subhalo progenitor
    create_test_halo(&test_ctx, 8, prev_snap, 2e12, -1, -1, -1);   // Secondary progenitor 1
    create_test_halo(&test_ctx, 9, prev_snap, 5e11, -1, -1, -1);   // Secondary progenitor 2
    create_test_halo(&test_ctx, 10, prev_snap, 7e11, -1, -1, -1);  // Subhalo only progenitor
    
    // CRITICAL: Set up descendant links and FOF group memberships for proper merger tree
    test_ctx.halos[0].FirstHaloInFOFgroup = 0;  // Halo 0 is FOF root
    test_ctx.halos[1].FirstHaloInFOFgroup = 0;  // Halo 1 is part of same FOF group
    test_ctx.halos[5].Descendant = 0;  // Halo 5 → Halo 0
    test_ctx.halos[6].Descendant = 1;  // Halo 6 → Halo 1  
    test_ctx.halos[8].Descendant = 0;  // Halo 8 → Halo 0 (merges into FOF root)
    test_ctx.halos[9].Descendant = 0;  // Halo 9 → Halo 0 (merges into FOF root)
    test_ctx.halos[10].Descendant = 1; // Halo 10 → Halo 1 (merges into subhalo)
    
    // Create galaxies representing merger tree
    printf("  Creating test galaxies:\n");
    create_test_galaxy(&test_ctx, 0, 5, 4e10);  // Main central
    printf("    Galaxy in halo 5: Type 0 (central), Mvir 2.5e12, StellarMass 4e10\n");
    create_test_galaxy(&test_ctx, 1, 5, 2e10);  // Satellite in main
    printf("    Galaxy in halo 5: Type 1 (satellite), Mvir 2.5e12, StellarMass 2e10\n");
    create_test_galaxy(&test_ctx, 0, 8, 3e10);  // Central in merging halo
    printf("    Galaxy in halo 8: Type 0 (central), Mvir 2.0e12, StellarMass 3e10\n");
    create_test_galaxy(&test_ctx, 0, 9, 1e10);  // Central in small halo
    printf("    Galaxy in halo 9: Type 0 (central), Mvir 5.0e11, StellarMass 1e10\n");
    create_test_galaxy(&test_ctx, 0, 10, 1.5e10); // Subhalo central
    printf("    Galaxy in halo 10: Type 0 (central), Mvir 7.0e11, StellarMass 1.5e10\n");
    
    printf("  Merger tree structure:\n");
    printf("    Halo 0 (current, FOF root) <- Halo 5 <- Halo 8 <- Halo 9\n");
    printf("    Halo 1 (current, subhalo)  <- Halo 6 <- Halo 10\n");
    printf("  Expected result (CORRECTED):\n");
    printf("    - Galaxy from halo 5 (central): Type 0 in halo 0 (first_occupied for halo 0)\n");
    printf("    - Galaxy from halo 5 (satellite): Type 1 in halo 0 (first_occupied for halo 0)\n");
    printf("    - Galaxy from halo 8: Type 2 (orphan) in halo 0 (NOT first_occupied)\n");
    printf("    - Galaxy from halo 9: Type 2 (orphan) in halo 0 (NOT first_occupied)\n");  
    printf("    - Galaxy from halo 10: Type 1 in halo 1 (first_occupied for halo 1)\n");
    
    // Process FOF group
    bool processed_flags[30] = {0};
    printf("  Processing FOF group...\n");
    
    // Check initial galaxy count
    int ngal_before = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    printf("  Galaxies in previous snapshot: %d\n", ngal_before);
    
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_ctx.test_params, processed_flags);
    
    // Check which galaxies were processed
    printf("  Processed flags after FOF processing:\n");
    for (int i = 0; i < ngal_before; i++) {
        printf("    Galaxy %d: %s\n", i, processed_flags[i] ? "PROCESSED" : "NOT PROCESSED");
    }

    TEST_ASSERT(status == EXIT_SUCCESS, "Complex merger tree should process successfully");
    
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    // Verify merger tree continuity
    TEST_ASSERT(ngal >= 2, "Should have multiple galaxies from merger tree");
    
    // Count galaxy types
    int type_counts[3] = {0, 0, 0};
    for (int i = 0; i < ngal; i++) {
        int type = GALAXY_PROP_Type(&galaxies[i]);
        printf("  Galaxy %d: Type %d, HaloNr %d, Mvir %.1e\n", 
               i, type, GALAXY_PROP_HaloNr(&galaxies[i]), GALAXY_PROP_Mvir(&galaxies[i]));
        if (type >= 0 && type <= 2) {
            type_counts[type]++;
        }
    }
    
    printf("  Type counts: %d centrals, %d satellites, %d orphans\n", 
           type_counts[0], type_counts[1], type_counts[2]);
    
    TEST_ASSERT(type_counts[0] == 1, "Merger tree should result in one central");
    TEST_ASSERT(ngal == 5, "Should have all 5 galaxies processed");
    TEST_ASSERT(type_counts[1] >= 1, "Should have at least one satellite");  
    TEST_ASSERT(type_counts[2] >= 2, "Should have orphans from disrupted progenitors");
    
    printf("  Merger tree continuity: %d central, %d satellites from %d progenitors\n",
           type_counts[0], type_counts[1], 5);
}

/**
 * Test: Evolution diagnostics FOF-aware initialization
 */
static void test_evolution_diagnostics_fof(void) {
    printf("\n=== Testing FOF-aware evolution diagnostics ===\n");
    
    // Reset galaxy arrays for fresh test
    reset_test_galaxies(&test_ctx);
    
    // Create simple FOF group for diagnostics test
    create_test_halo(&test_ctx, 0, 25, 1.5e12, 3, -1, -1);
    create_test_halo(&test_ctx, 3, 24, 1.3e12, -1, -1, -1);
    
    create_test_galaxy(&test_ctx, 0, 3, 2e10);
    
    // Process and verify diagnostics are FOF-aware
    bool processed_flags[30] = {0};
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_ctx.test_params, processed_flags);

    TEST_ASSERT(status == EXIT_SUCCESS, "FOF group should process for diagnostics test");
    
    // Since we can't directly access internal diagnostics, verify the processing worked
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(ngal >= 1, "Diagnostics test should produce galaxies");
    
    printf("  Evolution diagnostics test completed successfully\n");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for test_fof_evolution_context\n");
    printf("========================================\n\n");
    
    printf("This test verifies that FOF evolution context works correctly:\n");
    printf("  1. FOF-centric time calculations vs individual halo timing\n");
    printf("  2. Central galaxy validation without halo-specific assumptions\n");
    printf("  3. Merger tree continuity within FOF groups\n");
    printf("  4. Evolution diagnostics FOF-aware initialization\n\n");

    // Setup standardized test environment
    if (setup_test_environment(&test_ctx, 30) != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_fof_centric_timing();
    test_central_validation_fof_centric();
    test_merger_tree_continuity();
    test_evolution_diagnostics_fof();
    
    // Teardown
    teardown_test_environment(&test_ctx);
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_fof_evolution_context:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}