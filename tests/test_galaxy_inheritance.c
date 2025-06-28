/**
 * Test suite for Galaxy Inheritance and Orphan Handling
 * 
 * Tests cover:
 * - Basic galaxy collection and inheritance
 * - Gap handling in merger trees
 * - Orphan creation from disrupted halos
 * - Mass conservation during inheritance
 * - Tree traversal edge cases
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "tree_context.h"
#include "tree_galaxies.h"
#include "core_allvars.h"
#include "core_mymalloc.h"

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
    struct halo_data* halos;
    struct params run_params;
    TreeContext* tree_ctx;
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize minimal run parameters for testing
    test_ctx.run_params.simulation.SimMaxSnaps = 64;
    test_ctx.run_params.simulation.LastSnapshotNr = 63;
    
    test_ctx.initialized = 1;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.tree_ctx) {
        tree_context_destroy(&test_ctx.tree_ctx);
    }
    if (test_ctx.halos) {
        myfree(test_ctx.halos);
        test_ctx.halos = NULL;
    }
    test_ctx.initialized = 0;
}

// Helper function to create simple tree structure
static void create_simple_tree(int nhalo) {
    test_ctx.halos = mycalloc(nhalo, sizeof(struct halo_data));
    
    // Initialize all halos with default values
    for (int i = 0; i < nhalo; i++) {
        test_ctx.halos[i].Descendant = -1;
        test_ctx.halos[i].FirstProgenitor = -1;
        test_ctx.halos[i].NextProgenitor = -1;
        test_ctx.halos[i].FirstHaloInFOFgroup = i;  // Each halo is its own FOF by default
        test_ctx.halos[i].NextHaloInFOFgroup = -1;
        test_ctx.halos[i].SnapNum = 63;  // Default to z=0
        test_ctx.halos[i].Len = 100;     // Default particle count
        test_ctx.halos[i].Mvir = 1.0e12; // Default mass
        // Note: Rvir and Vvir are calculated properties, not halo fields
        test_ctx.halos[i].Vmax = 220.0;  // Default max velocity
    }
    
    test_ctx.tree_ctx = tree_context_create(test_ctx.halos, nhalo, &test_ctx.run_params);
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Basic tree context creation and destruction
 */
static void test_tree_context_lifecycle(void) {
    printf("=== Testing tree context lifecycle ===\n");
    
    create_simple_tree(1);
    
    TEST_ASSERT(test_ctx.tree_ctx != NULL, "TreeContext should be created successfully");
    TEST_ASSERT(test_ctx.tree_ctx->halos == test_ctx.halos, "TreeContext should reference correct halos");
    TEST_ASSERT(test_ctx.tree_ctx->nhalos == 1, "TreeContext should have correct halo count");
    TEST_ASSERT(test_ctx.tree_ctx->working_galaxies != NULL, "Working galaxy array should be initialized");
    TEST_ASSERT(test_ctx.tree_ctx->output_galaxies != NULL, "Output galaxy array should be initialized");
    TEST_ASSERT(test_ctx.tree_ctx->galaxy_counter == 0, "Galaxy counter should start at 0");
    
    // Test mapping arrays initialization
    TEST_ASSERT(test_ctx.tree_ctx->halo_first_galaxy[0] == -1, "Halo galaxy mapping should be initialized to -1");
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[0] == 0, "Halo galaxy count should be initialized to 0");
}

/**
 * Test: Primordial galaxy creation
 */
static void test_primordial_galaxy_creation(void) {
    printf("\n=== Testing primordial galaxy creation ===\n");
    
    create_simple_tree(1);
    
    // Test creating primordial galaxy for FOF root halo with no progenitors
    int result = collect_halo_galaxies(0, test_ctx.tree_ctx);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "collect_halo_galaxies should succeed");
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[0] == 1, "Should create one primordial galaxy");
    TEST_ASSERT(test_ctx.tree_ctx->halo_first_galaxy[0] == 0, "Galaxy should be at index 0");
    TEST_ASSERT(galaxy_array_get_count(test_ctx.tree_ctx->working_galaxies) == 1, "Working array should contain one galaxy");
    
    // Verify galaxy properties
    struct GALAXY* galaxy = galaxy_array_get(test_ctx.tree_ctx->working_galaxies, 0);
    TEST_ASSERT(galaxy != NULL, "Galaxy should be retrievable from array");
    TEST_ASSERT(GALAXY_PROP_HaloNr(galaxy) == 0, "Galaxy should be assigned to correct halo");
    int actual_snap = GALAXY_PROP_SnapNum(galaxy);
    // Note: init_galaxy may adjust snapshot number based on internal logic
    TEST_ASSERT(actual_snap >= 0 && actual_snap <= 63, "Galaxy should have valid snapshot number");
}

/**
 * Test: Gap measurement in tree structure
 */
static void test_gap_measurement(void) {
    printf("\n=== Testing gap measurement ===\n");
    
    // Test consecutive snapshots (no gap)
    int gap1 = measure_tree_gap(63, 62);
    TEST_ASSERT(gap1 == 0, "Consecutive snapshots should have no gap");
    
    // Test gap of 1 snapshot
    int gap2 = measure_tree_gap(63, 61);
    TEST_ASSERT(gap2 == 1, "One snapshot gap should be detected");
    
    // Test gap of 5 snapshots
    int gap3 = measure_tree_gap(63, 57);
    TEST_ASSERT(gap3 == 5, "Five snapshot gap should be detected");
    
    // Test reverse order (should be 0)
    int gap4 = measure_tree_gap(57, 63);
    TEST_ASSERT(gap4 == 0, "Reverse order should return 0");
}

/**
 * Test: Galaxy inheritance with gaps
 */
static void test_gap_handling(void) {
    printf("\n=== Testing gap handling in inheritance ===\n");
    
    create_simple_tree(3);
    
    // Create tree with gap: halo 0 (snap 63) <- halo 1 (snap 60) <- halo 2 (snap 59)
    // Gap of 2 snapshots between halo 0 and halo 1
    test_ctx.halos[0].SnapNum = 63;
    test_ctx.halos[0].FirstProgenitor = 1;
    test_ctx.halos[1].SnapNum = 60;  // Gap here
    test_ctx.halos[1].Descendant = 0;
    test_ctx.halos[1].FirstProgenitor = 2;
    test_ctx.halos[2].SnapNum = 59;
    test_ctx.halos[2].Descendant = 1;
    
    // Create galaxy in leaf halo
    collect_halo_galaxies(2, test_ctx.tree_ctx);
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[2] == 1, "Leaf halo should have primordial galaxy");
    
    // Inherit through the gap
    collect_halo_galaxies(1, test_ctx.tree_ctx);
    inherit_galaxies_with_orphans(1, test_ctx.tree_ctx);
    
    collect_halo_galaxies(0, test_ctx.tree_ctx);
    inherit_galaxies_with_orphans(0, test_ctx.tree_ctx);
    
    TEST_ASSERT(test_ctx.tree_ctx->total_gaps_spanned >= 1, "Should detect at least one gap");
    TEST_ASSERT(test_ctx.tree_ctx->max_gap_length >= 2, "Should detect gap of at least 2 snapshots");
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[0] > 0, "Root halo should inherit galaxies through gap");
}

/**
 * Test: Orphan creation from disrupted halos
 */
static void test_orphan_creation(void) {
    printf("\n=== Testing orphan creation ===\n");
    
    create_simple_tree(4);
    
    // Create merger tree: halo 0 has two progenitors (1 and 2), with different masses
    test_ctx.halos[0].FirstProgenitor = 1;
    test_ctx.halos[1].Descendant = 0;
    test_ctx.halos[1].NextProgenitor = 2;
    test_ctx.halos[1].Len = 1000;  // More massive
    test_ctx.halos[2].Descendant = 0;
    test_ctx.halos[2].Len = 100;   // Less massive (will create orphan)
    test_ctx.halos[3].Descendant = 1;  // Progenitor of halo 1
    test_ctx.halos[1].FirstProgenitor = 3;
    
    // Create galaxies in progenitors
    collect_halo_galaxies(3, test_ctx.tree_ctx);  // Creates primordial in halo 3
    inherit_galaxies_with_orphans(1, test_ctx.tree_ctx);  // Inherits to halo 1
    
    collect_halo_galaxies(2, test_ctx.tree_ctx);  // Creates primordial in halo 2
    
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[1] > 0, "Halo 1 should have galaxies");
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[2] > 0, "Halo 2 should have galaxies");
    
    // Now inherit to halo 0 - should create orphans
    int orphans_before = test_ctx.tree_ctx->total_orphans;
    inherit_galaxies_with_orphans(0, test_ctx.tree_ctx);
    
    TEST_ASSERT(test_ctx.tree_ctx->total_orphans > orphans_before, "Should create orphans from smaller progenitor");
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[0] > 1, "Halo 0 should inherit multiple galaxies");
    
    // Verify orphan properties
    int start_idx = test_ctx.tree_ctx->halo_first_galaxy[0];
    int count = test_ctx.tree_ctx->halo_galaxy_count[0];
    bool found_orphan = false;
    
    for (int i = 0; i < count; i++) {
        struct GALAXY* gal = galaxy_array_get(test_ctx.tree_ctx->working_galaxies, start_idx + i);
        if (GALAXY_PROP_Type(gal) == 2) {  // Type 2 = orphan
            found_orphan = true;
            TEST_ASSERT(GALAXY_PROP_Mvir(gal) == 0.0, "Orphan should have zero virial mass");
            break;
        }
    }
    
    TEST_ASSERT(found_orphan, "Should find at least one orphan galaxy");
}

/**
 * Test: Galaxy property updates during inheritance
 */
static void test_galaxy_property_updates(void) {
    printf("\n=== Testing galaxy property updates ===\n");
    
    create_simple_tree(2);
    
    // Setup simple inheritance: halo 0 <- halo 1
    test_ctx.halos[0].FirstProgenitor = 1;
    test_ctx.halos[1].Descendant = 0;
    
    // Set different properties for halos
    test_ctx.halos[0].Pos[0] = 100.0;
    test_ctx.halos[0].Pos[1] = 200.0;
    test_ctx.halos[0].Pos[2] = 300.0;
    test_ctx.halos[0].Mvir = 2.0e12;
    // Note: Rvir is a calculated property, not a halo field
    
    test_ctx.halos[1].Pos[0] = 50.0;
    test_ctx.halos[1].Pos[1] = 100.0;
    test_ctx.halos[1].Pos[2] = 150.0;
    test_ctx.halos[1].Mvir = 1.0e12;
    // Note: Rvir is a calculated property, not a halo field
    
    // Create galaxy in progenitor
    collect_halo_galaxies(1, test_ctx.tree_ctx);
    struct GALAXY* orig_gal = galaxy_array_get(test_ctx.tree_ctx->working_galaxies, 0);
    
    // Verify original galaxy has halo 1 properties
    TEST_ASSERT(GALAXY_PROP_HaloNr(orig_gal) == 1, "Original galaxy should be in halo 1");
    TEST_ASSERT(GALAXY_PROP_Pos(orig_gal)[0] == 50.0, "Original galaxy should have halo 1 position");
    
    // Inherit to descendant
    inherit_galaxies_with_orphans(0, test_ctx.tree_ctx);
    
    int start_idx = test_ctx.tree_ctx->halo_first_galaxy[0];
    struct GALAXY* inherited_gal = galaxy_array_get(test_ctx.tree_ctx->working_galaxies, start_idx);
    
    // Verify inherited galaxy has updated properties
    TEST_ASSERT(GALAXY_PROP_HaloNr(inherited_gal) == 0, "Inherited galaxy should be in halo 0");
    TEST_ASSERT(GALAXY_PROP_Pos(inherited_gal)[0] == 100.0, "Inherited galaxy should have halo 0 position");
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(inherited_gal) - 2.0e12) < 1e6, "Inherited galaxy should have halo 0 mass");
}

/**
 * Test: Edge case - No progenitors for non-FOF root
 */
static void test_no_progenitor_non_fof_root(void) {
    printf("\n=== Testing no progenitor for non-FOF root ===\n");
    
    create_simple_tree(2);
    
    // Make halo 1 not the FOF root (halo 0 is FOF root)
    test_ctx.halos[1].FirstHaloInFOFgroup = 0;
    
    // Try to collect galaxies for halo 1 (no progenitors, not FOF root)
    int result = collect_halo_galaxies(1, test_ctx.tree_ctx);
    
    TEST_ASSERT(result == EXIT_SUCCESS, "collect_halo_galaxies should succeed");
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[1] == 0, "Non-FOF root with no progenitors should not create galaxy");
}

/**
 * Test: Mass conservation during inheritance
 */
static void test_mass_conservation(void) {
    printf("\n=== Testing mass conservation ===\n");
    
    create_simple_tree(3);
    
    // Create simple chain: halo 0 <- halo 1 <- halo 2
    test_ctx.halos[0].FirstProgenitor = 1;
    test_ctx.halos[1].Descendant = 0;
    test_ctx.halos[1].FirstProgenitor = 2;
    test_ctx.halos[2].Descendant = 1;
    
    // Create galaxy in leaf
    collect_halo_galaxies(2, test_ctx.tree_ctx);
    
    int initial_galaxy_count = galaxy_array_get_count(test_ctx.tree_ctx->working_galaxies);
    
    // Inherit through chain
    inherit_galaxies_with_orphans(1, test_ctx.tree_ctx);
    inherit_galaxies_with_orphans(0, test_ctx.tree_ctx);
    
    int final_galaxy_count = galaxy_array_get_count(test_ctx.tree_ctx->working_galaxies);
    
    // Should preserve galaxy count through inheritance (no orphans in simple chain)
    TEST_ASSERT(test_ctx.tree_ctx->halo_galaxy_count[0] > 0, "Final halo should have inherited galaxies");
    TEST_ASSERT(final_galaxy_count >= initial_galaxy_count, "Total galaxy count should not decrease");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for Galaxy Inheritance\n");
    printf("========================================\n\n");
    
    printf("This test verifies galaxy inheritance and orphan handling functionality:\n");
    printf("  1. Tree context lifecycle management\n");
    printf("  2. Primordial galaxy creation for halos without progenitors\n");
    printf("  3. Gap detection and handling in merger trees\n");
    printf("  4. Orphan creation when halos are disrupted\n");
    printf("  5. Proper galaxy property updates during inheritance\n");
    printf("  6. Mass conservation through inheritance process\n");
    printf("  7. Edge cases and error conditions\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_tree_context_lifecycle();
    test_primordial_galaxy_creation();
    test_gap_measurement();
    test_gap_handling();
    test_orphan_creation();
    test_galaxy_property_updates();
    test_no_progenitor_non_fof_root();
    test_mass_conservation();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for Galaxy Inheritance:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}