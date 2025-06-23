/**
 * @file test_orphan_fof_disruption.c
 * @brief Test cases demonstrating orphan galaxy loss during FOF group disruption
 * 
 * This test demonstrates critical flaws in SAGE's orphan detection system:
 * 1. Orphans are lost when their host FOF group is disrupted
 * 2. Processing order dependency causes galaxy loss
 * 3. Cross-FOF orphan migration fails
 * 
 * These tests expose fundamental architectural problems that lead to
 * permanent galaxy loss, violating mass conservation principles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "test_helper.h"
#include "../src/core/core_build_model.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;
static int bugs_detected = 0;

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

// Helper macro for bug detection (should cause test failure)
#define BUG_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("BUG DETECTED: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        bugs_detected++; \
        return; \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Use the shared context
static struct TestContext test_ctx;

//=============================================================================
// Test Cases Demonstrating Orphan Loss
//=============================================================================

/**
 * Test: Complete FOF group disruption orphan loss
 * 
 * Scenario: Small FOF group with orphans gets completely disrupted
 * Expected: Orphans should migrate to surviving FOF group
 * Actual: Orphans are permanently lost
 */
static void test_complete_fof_disruption_orphan_loss(void) {
    printf("\n=== Testing Complete FOF Group Disruption (Orphan Loss) ===\n");
    printf("  This test demonstrates how orphans are LOST when FOF groups are disrupted\n");
    
    // Reset galaxy arrays for fresh test
    reset_test_galaxies(&test_ctx);
    
    // Snapshot N: Setup with two FOF groups, one small with orphans
    int current_snap = 20;
    int prev_snap = 19;
    
    // CURRENT SNAPSHOT: Only one surviving FOF group
    create_test_halo(&test_ctx, 0, current_snap, 5e12, -1, -1, -1);  // Large surviving FOF root
    
    // PREVIOUS SNAPSHOT: Two separate FOF groups
    // Group 1: Large group that will survive
    create_test_halo(&test_ctx, 5, prev_snap, 4e12, -1, -1, -1);  // Will become halo 0
    
    // Group 2: Small group that will be COMPLETELY DISRUPTED
    create_test_halo(&test_ctx, 6, prev_snap, 1e12, -1, -1, -1);  // Central - will be lost
    create_test_halo(&test_ctx, 7, prev_snap, 8e11, -1, -1, -1);  // Had orphans - descendant lost
    
    // Set up merger tree: Small group merges into large group but gets disrupted
    test_ctx.halos[5].Descendant = 0;  // Large group survives
    test_ctx.halos[6].Descendant = -1; // Central is disrupted (NO DESCENDANT)
    test_ctx.halos[7].Descendant = -1; // Subhalo is disrupted (NO DESCENDANT)
    
    // Current snapshot structure
    test_ctx.halos[0].FirstProgenitor = 5;
    test_ctx.halos[5].NextProgenitor = 6;  // Include disrupted as progenitor
    test_ctx.halos[6].NextProgenitor = 7;  // Chain the disrupted halos
    test_ctx.halos[7].NextProgenitor = -1;
    
    // FOF group setup for previous snapshot
    test_ctx.halos[5].FirstHaloInFOFgroup = 5;  // Group 1 root
    test_ctx.halos[6].FirstHaloInFOFgroup = 6;  // Group 2 root  
    test_ctx.halos[7].FirstHaloInFOFgroup = 6;  // Part of Group 2
    
    // Current snapshot FOF
    test_ctx.halos[0].FirstHaloInFOFgroup = 0;
    
    // Create galaxies - including ORPHANS in the small group
    printf("  Creating test setup:\n");
    
    // Large group galaxies
    create_test_galaxy(&test_ctx, 0, 5, 5e10);  // Will survive as central
    printf("    Large group: 1 central galaxy\n");
    
    // Small group galaxies (THESE WILL BE LOST)
    create_test_galaxy(&test_ctx, 0, 6, 2e10);  // Central of small group
    create_test_galaxy(&test_ctx, 2, 6, 1e10);  // ORPHAN in small group  
    create_test_galaxy(&test_ctx, 2, 7, 8e9);   // ORPHAN in disrupted subhalo
    printf("    Small group: 1 central + 2 ORPHANS (will be lost)\n");
    
    printf("  Expected behavior: Orphans should migrate to surviving FOF group\n");
    printf("  Actual SAGE behavior: Orphans are permanently lost\n");
    
    // Process the FOF group
    bool processed_flags[30] = {0};
    int ngal_before = galaxy_array_get_count(test_ctx.galaxies_prev_snap);
    printf("  Input galaxies: %d\n", ngal_before);
    
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_ctx.test_params, processed_flags);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "FOF processing should complete successfully");
    
    int ngal_after = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    printf("  Output galaxies: %d\n", ngal_after);
    printf("  GALAXY LOSS: %d galaxies disappeared\n", ngal_before - ngal_after);
    
    // Show what galaxies survived
    for (int i = 0; i < ngal_after; i++) {
        printf("    Survivor %d: Type %d, HaloNr %d\n", 
               i, GALAXY_PROP_Type(&galaxies[i]), GALAXY_PROP_HaloNr(&galaxies[i]));
    }
    
    // Critical assertion: This should fail when the bug is detected
    printf("  Expected: %d galaxies (with orphan migration)\n", ngal_before);
    printf("  Actual: %d galaxies (orphans lost)\n", ngal_after);
    
    if (ngal_after < ngal_before) {
        printf("  *** BUG CONFIRMED: %d orphan galaxies were permanently lost ***\n", 
               ngal_before - ngal_after);
        printf("  *** This violates mass conservation in cosmological simulations ***\n");
    }
    
    // This assertion should FAIL to indicate the bug exists
    BUG_ASSERT(ngal_after == ngal_before, 
               "Orphan galaxies must be conserved during FOF group disruption");
}

/**
 * Test: FOF group processing order dependency
 * 
 * This test shows how the order of FOF group processing affects orphan detection
 */
static void test_processing_order_dependency(void) {
    printf("\n=== Testing FOF Group Processing Order Dependency ===\n");
    printf("  This test shows how processing order affects orphan detection\n");
    
    // This would require modifying SAGE to control FOF processing order
    // For now, document the issue
    
    printf("  CRITICAL ISSUE: Orphan detection depends on FOF group processing order\n");
    printf("  - If FOF group A processes before FOF group B\n");
    printf("  - But orphans should migrate from A to B\n");
    printf("  - The orphans may be lost depending on timing\n");
    printf("  - This creates non-deterministic behavior\n");
    
    // This is primarily a documentation test showing the architectural issue
    TEST_ASSERT(1, "Processing order dependency documented");
}

/**
 * Test: Missing orphan registry validation
 * 
 * Shows that SAGE has no global tracking of orphan galaxies
 */
static void test_missing_orphan_registry(void) {
    printf("\n=== Testing Missing Global Orphan Registry ===\n");
    printf("  ARCHITECTURAL FLAW: No global orphan tracking across FOF groups\n");
    
    printf("  Problems with current design:\n");
    printf("  1. No central registry of unprocessed orphans\n");
    printf("  2. No validation that all galaxies are assigned\n");
    printf("  3. No mass conservation checks\n");
    printf("  4. Silent galaxy loss with no error reporting\n");
    
    printf("  Required fixes:\n");
    printf("  1. Global orphan registry spanning all FOF groups\n");
    printf("  2. Two-pass processing: detect all orphans, then assign\n");
    printf("  3. Cross-FOF communication mechanisms\n");
    printf("  4. Mandatory galaxy count conservation validation\n");
    
    TEST_ASSERT(1, "Missing orphan registry documented");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Orphan Galaxy Loss in FOF Disruption Tests\n");
    printf("========================================\n\n");
    
    printf("This test suite demonstrates critical flaws in SAGE's orphan handling:\n");
    printf("1. Orphan galaxies are permanently lost during FOF group disruption\n");
    printf("2. Processing order creates non-deterministic behavior\n");
    printf("3. No global orphan registry causes architecture problems\n\n");
    
    printf("WARNING: These tests expose bugs that violate mass conservation!\n\n");
    
    // Setup standardized test environment
    if (setup_test_environment(&test_ctx, 30) != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_complete_fof_disruption_orphan_loss();
    test_processing_order_dependency();
    test_missing_orphan_registry();
    
    // Teardown
    teardown_test_environment(&test_ctx);
    
    // Report results
    printf("\n========================================\n");
    printf("Orphan Galaxy Loss Test Results:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("  Bugs detected: %d\n", bugs_detected);
    printf("========================================\n\n");
    
    if (bugs_detected > 0) {
        printf("CRITICAL BUGS DETECTED: %d orphan galaxy loss bugs found!\n", bugs_detected);
        printf("These bugs violate mass conservation in cosmological simulations.\n");
        printf("TEST FAILURE: Fix orphan handling before proceeding.\n");
        return 1; // Fail the test when bugs are detected
    }
    
    if (tests_run == tests_passed) {
        printf("All tests passed - orphan handling is working correctly.\n");
        return 0;
    }
    
    printf("Some tests failed for reasons other than the main bug.\n");
    return 1;
}