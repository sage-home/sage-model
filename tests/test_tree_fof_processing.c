/**
 * Test suite for Tree FOF Processing
 * 
 * Tests cover:
 * - FOF readiness checking logic
 * - Galaxy collection within FOF groups
 * - Integration with tree traversal system
 * - Multiple progenitor orphan creation
 * - FOF processing with snapshot gaps
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tree_context.h"
#include "tree_fof.h"
#include "tree_traversal.h"
#include "tree_galaxies.h"
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
    struct params test_params;
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize minimal test parameters
    test_ctx.test_params.simulation.NumSnapOutputs = 10;
    test_ctx.test_params.simulation.SimMaxSnaps = 64;
    test_ctx.test_params.simulation.LastSnapshotNr = 63;
    
    test_ctx.initialized = 1;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: FOF readiness checking logic
 */
static void test_fof_readiness_check(void) {
    printf("=== Testing FOF readiness checking logic ===\n");
    
    // Create test halos: FOF group with 2 halos, one has unprocessed progenitor
    struct halo_data halos[4];
    memset(halos, 0, sizeof(halos));
    
    // Halo 0: FOF root at snapshot 10
    halos[0].SnapNum = 10;
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = 1;
    halos[0].FirstProgenitor = 2;  // Has progenitor
    halos[0].Descendant = -1;
    
    // Halo 1: Second halo in FOF group
    halos[1].SnapNum = 10;
    halos[1].FirstHaloInFOFgroup = 0;
    halos[1].NextHaloInFOFgroup = -1;
    halos[1].FirstProgenitor = 3;  // Has progenitor
    halos[1].Descendant = -1;
    
    // Halo 2: Progenitor of halo 0
    halos[2].SnapNum = 9;
    halos[2].FirstHaloInFOFgroup = 2;
    halos[2].NextHaloInFOFgroup = -1;
    halos[2].FirstProgenitor = -1;
    halos[2].Descendant = 0;
    halos[2].NextProgenitor = -1;
    
    // Halo 3: Progenitor of halo 1
    halos[3].SnapNum = 9;
    halos[3].FirstHaloInFOFgroup = 3;
    halos[3].NextHaloInFOFgroup = -1;
    halos[3].FirstProgenitor = -1;
    halos[3].Descendant = 1;
    halos[3].NextProgenitor = -1;
    
    TreeContext* ctx = tree_context_create(halos, 4, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext creation should succeed");
    
    // Test 1: FOF not ready when progenitors unprocessed
    bool ready = is_fof_ready(0, ctx);
    TEST_ASSERT(ready == false, "FOF should not be ready when progenitors unprocessed");
    
    // Test 2: Mark progenitors as done
    ctx->halo_done[2] = true;
    ctx->halo_done[3] = true;
    
    ready = is_fof_ready(0, ctx);
    TEST_ASSERT(ready == true, "FOF should be ready when all progenitors processed");
    
    tree_context_destroy(&ctx);
}

/**
 * Test: Galaxy collection within FOF groups
 */
static void test_fof_group_collection(void) {
    printf("\n=== Testing Galaxy collection within FOF groups ===\n");
    
    // Create test scenario: FOF with 2 halos, each with progenitors containing galaxies
    struct halo_data halos[6];
    memset(halos, 0, sizeof(halos));
    
    // Setup FOF group at snapshot 10
    halos[0].SnapNum = 10;
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = 1;
    halos[0].FirstProgenitor = 2;
    halos[0].Descendant = -1;
    halos[0].Len = 1000;  // Mass for central identification
    
    halos[1].SnapNum = 10;
    halos[1].FirstHaloInFOFgroup = 0;
    halos[1].NextHaloInFOFgroup = -1;
    halos[1].FirstProgenitor = 3;
    halos[1].Descendant = -1;
    halos[1].Len = 500;
    
    // Progenitors at snapshot 9
    halos[2].SnapNum = 9;
    halos[2].FirstHaloInFOFgroup = 2;
    halos[2].NextHaloInFOFgroup = -1;
    halos[2].FirstProgenitor = 4;
    halos[2].Descendant = 0;
    halos[2].NextProgenitor = -1;
    halos[2].Len = 800;
    
    halos[3].SnapNum = 9;
    halos[3].FirstHaloInFOFgroup = 3;
    halos[3].NextHaloInFOFgroup = -1;
    halos[3].FirstProgenitor = 5;
    halos[3].Descendant = 1;
    halos[3].NextProgenitor = -1;
    halos[3].Len = 400;
    
    // Root progenitors at snapshot 8 (primordial)
    halos[4].SnapNum = 8;
    halos[4].FirstHaloInFOFgroup = 4;
    halos[4].NextHaloInFOFgroup = -1;
    halos[4].FirstProgenitor = -1;
    halos[4].Descendant = 2;
    halos[4].NextProgenitor = -1;
    halos[4].Len = 600;
    
    halos[5].SnapNum = 8;
    halos[5].FirstHaloInFOFgroup = 5;
    halos[5].NextHaloInFOFgroup = -1;
    halos[5].FirstProgenitor = -1;
    halos[5].Descendant = 3;
    halos[5].NextProgenitor = -1;
    halos[5].Len = 300;
    
    TreeContext* ctx = tree_context_create(halos, 6, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext creation should succeed");
    
    // Process primordial halos first (create initial galaxies)
    ctx->halo_done[4] = true;
    ctx->halo_done[5] = true;
    
    int status = collect_halo_galaxies(4, ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "Galaxy collection should succeed");
    status = collect_halo_galaxies(5, ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "Galaxy collection should succeed");
    
    // Should have created 2 primordial galaxies
    TEST_ASSERT(ctx->halo_galaxy_count[4] == 1, "Should create 1 primordial galaxy in halo 4");
    TEST_ASSERT(ctx->halo_galaxy_count[5] == 1, "Should create 1 primordial galaxy in halo 5");
    
    // Process intermediate generation
    status = inherit_galaxies_with_orphans(2, ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "Galaxy inheritance should succeed");
    status = inherit_galaxies_with_orphans(3, ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "Galaxy inheritance should succeed");
    
    ctx->halo_done[2] = true;
    ctx->halo_done[3] = true;
    
    // Should have inherited galaxies
    TEST_ASSERT(ctx->halo_galaxy_count[2] == 1, "Should inherit 1 galaxy in halo 2");
    TEST_ASSERT(ctx->halo_galaxy_count[3] == 1, "Should inherit 1 galaxy in halo 3");
    
    // Now process the FOF group
    status = process_tree_fof_group(0, ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "FOF group processing should succeed");
    
    // Check that FOF is marked as done
    TEST_ASSERT(ctx->fof_done[0] == true, "FOF group should be marked as processed");
    
    // Check that galaxies were collected for FOF halos
    TEST_ASSERT(ctx->halo_galaxy_count[0] == 1, "Should have 1 galaxy in FOF halo 0");
    TEST_ASSERT(ctx->halo_galaxy_count[1] == 1, "Should have 1 galaxy in FOF halo 1");
    
    tree_context_destroy(&ctx);
}

/**
 * Test: Integration with tree traversal system
 */
static void test_fof_integration_with_traversal(void) {
    printf("\n=== Testing Integration with tree traversal system ===\n");
    
    // Simplified test: single FOF group that should definitely be processed
    struct halo_data halos[3];
    memset(halos, 0, sizeof(halos));
    
    // FOF group at snapshot 10 with single halo
    halos[0].SnapNum = 10;
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = -1;  // Only halo in FOF
    halos[0].FirstProgenitor = 1;
    halos[0].Descendant = -1;
    halos[0].Len = 1000;
    
    // Progenitor at snapshot 9
    halos[1].SnapNum = 9;
    halos[1].FirstHaloInFOFgroup = 1;
    halos[1].NextHaloInFOFgroup = -1;
    halos[1].FirstProgenitor = 2;
    halos[1].Descendant = 0;
    halos[1].NextProgenitor = -1;
    halos[1].Len = 800;
    
    // Root progenitor at snapshot 8
    halos[2].SnapNum = 8;
    halos[2].FirstHaloInFOFgroup = 2;
    halos[2].NextHaloInFOFgroup = -1;
    halos[2].FirstProgenitor = -1;
    halos[2].Descendant = 1;
    halos[2].NextProgenitor = -1;
    halos[2].Len = 600;
    
    TreeContext* ctx = tree_context_create(halos, 3, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext creation should succeed");
    
    // Process the tree
    int status = process_forest_trees(ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "Tree processing should succeed");
    
    // Verify all halos were processed
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(ctx->halo_done[i] == true, "All halos should be processed");
    }
    
    // Check galaxy counts make sense
    int total_galaxies = galaxy_array_get_count(ctx->working_galaxies);
    TEST_ASSERT(total_galaxies > 0, "Should create galaxies during processing");
    
    tree_context_destroy(&ctx);
}

/**
 * Test: Multiple progenitor orphan creation
 */
static void test_multiple_progenitor_orphan_creation(void) {
    printf("\n=== Testing Multiple progenitor orphan creation ===\n");
    
    // Setup: halo with 3 progenitors, creating orphans from smaller ones
    struct halo_data halos[4];
    memset(halos, 0, sizeof(halos));
    
    // Descendant halo
    halos[0].SnapNum = 10;
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = -1;
    halos[0].FirstProgenitor = 1;
    halos[0].Descendant = -1;
    halos[0].Len = 1000;
    
    // Main progenitor (largest)
    halos[1].SnapNum = 9;
    halos[1].FirstHaloInFOFgroup = 1;
    halos[1].NextHaloInFOFgroup = -1;
    halos[1].FirstProgenitor = -1;
    halos[1].Descendant = 0;
    halos[1].NextProgenitor = 2;
    halos[1].Len = 800;  // Largest
    
    // Secondary progenitor (medium)
    halos[2].SnapNum = 9;
    halos[2].FirstHaloInFOFgroup = 2;
    halos[2].NextHaloInFOFgroup = -1;
    halos[2].FirstProgenitor = -1;
    halos[2].Descendant = 0;
    halos[2].NextProgenitor = 3;
    halos[2].Len = 500;  // Medium
    
    // Tertiary progenitor (small)
    halos[3].SnapNum = 9;
    halos[3].FirstHaloInFOFgroup = 3;
    halos[3].NextHaloInFOFgroup = -1;
    halos[3].FirstProgenitor = -1;
    halos[3].Descendant = 0;
    halos[3].NextProgenitor = -1;
    halos[3].Len = 200;  // Smallest
    
    TreeContext* ctx = tree_context_create(halos, 4, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext creation should succeed");
    
    // Create galaxies in all progenitors
    ctx->halo_done[1] = true;
    ctx->halo_done[2] = true;
    ctx->halo_done[3] = true;
    
    collect_halo_galaxies(1, ctx);
    collect_halo_galaxies(2, ctx);
    collect_halo_galaxies(3, ctx);
    
    // Should have 3 primordial galaxies
    TEST_ASSERT(ctx->halo_galaxy_count[1] == 1, "Should create galaxy in progenitor 1");
    TEST_ASSERT(ctx->halo_galaxy_count[2] == 1, "Should create galaxy in progenitor 2");
    TEST_ASSERT(ctx->halo_galaxy_count[3] == 1, "Should create galaxy in progenitor 3");
    
    int initial_orphans = ctx->total_orphans;
    
    // Inherit galaxies (should create orphans from secondary and tertiary)
    inherit_galaxies_with_orphans(0, ctx);
    
    // Should have 3 galaxies in descendant halo
    TEST_ASSERT(ctx->halo_galaxy_count[0] == 3, "Should inherit 3 galaxies");
    
    // Should have created 2 orphans (from secondary and tertiary progenitors)
    TEST_ASSERT(ctx->total_orphans == initial_orphans + 2, "Should create 2 orphans");
    
    // Check galaxy types
    int central_count = 0, orphan_count = 0;
    int start_idx = ctx->halo_first_galaxy[0];
    
    for (int i = 0; i < ctx->halo_galaxy_count[0]; i++) {
        struct GALAXY* gal = galaxy_array_get(ctx->working_galaxies, start_idx + i);
        if (GALAXY_PROP_Type(gal) == 0) central_count++;
        if (GALAXY_PROP_Type(gal) == 2) orphan_count++;
    }
    
    TEST_ASSERT(central_count == 1, "Should have 1 central galaxy");
    TEST_ASSERT(orphan_count == 2, "Should have 2 orphan galaxies");
    
    tree_context_destroy(&ctx);
}

/**
 * Test: FOF processing with snapshot gaps
 */
static void test_fof_processing_with_gaps(void) {
    printf("\n=== Testing FOF processing with snapshot gaps ===\n");
    
    // Create scenario with gaps: progenitor at snap 5, descendant at snap 10
    struct halo_data halos[3];
    memset(halos, 0, sizeof(halos));
    
    // Descendant FOF group
    halos[0].SnapNum = 10;
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = 1;
    halos[0].FirstProgenitor = 2;
    halos[0].Descendant = -1;
    halos[0].Len = 1000;
    
    halos[1].SnapNum = 10;
    halos[1].FirstHaloInFOFgroup = 0;
    halos[1].NextHaloInFOFgroup = -1;
    halos[1].FirstProgenitor = -1;
    halos[1].Descendant = -1;
    halos[1].Len = 500;
    
    // Progenitor with gap (snap 5 -> snap 10 = gap of 4)
    halos[2].SnapNum = 5;
    halos[2].FirstHaloInFOFgroup = 2;
    halos[2].NextHaloInFOFgroup = -1;
    halos[2].FirstProgenitor = -1;
    halos[2].Descendant = 0;
    halos[2].NextProgenitor = -1;
    halos[2].Len = 800;
    
    TreeContext* ctx = tree_context_create(halos, 3, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext creation should succeed");
    
    int initial_gaps = ctx->total_gaps_spanned;
    
    // Process the tree
    process_tree_recursive(0, ctx);
    
    // Should have detected and recorded the gap
    TEST_ASSERT(ctx->total_gaps_spanned > initial_gaps, "Should detect gaps");
    TEST_ASSERT(ctx->max_gap_length >= 4, "Should record correct gap length");
    
    // Verify galaxy inheritance worked despite gap
    TEST_ASSERT(ctx->halo_galaxy_count[0] == 1, "Should inherit galaxy across gap");
    
    tree_context_destroy(&ctx);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    printf("\n========================================\n");
    printf("Starting tests for Tree FOF Processing\n");
    printf("========================================\n\n");
    
    printf("This test verifies that FOF processing works correctly in tree-based mode:\n");
    printf("  1. FOF readiness checking with dependency validation\n");
    printf("  2. Galaxy collection and inheritance within FOF groups\n");
    printf("  3. Integration with tree traversal system\n");
    printf("  4. Orphan creation from multiple progenitors\n");
    printf("  5. Correct handling of snapshot gaps\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_fof_readiness_check();
    test_fof_group_collection();
    test_fof_integration_with_traversal();
    test_multiple_progenitor_orphan_creation();
    test_fof_processing_with_gaps();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for Tree FOF Processing:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}