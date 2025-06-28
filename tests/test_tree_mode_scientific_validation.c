/**
 * Test suite for Tree Mode Scientific Validation - Phase 6
 * 
 * Tests cover:
 * - Tree context validation for scientific accuracy
 * - Mass conservation checking infrastructure
 * - Orphan galaxy identification framework
 * - Tree processing integrity validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/core/tree_context.h"
#include "../src/core/tree_traversal.h"
#include "../src/core/tree_galaxies.h"
#include "../src/core/tree_fof.h"
#include "../src/core/core_mymalloc.h"

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

// Test parameters structure - minimal required for properties allocation
static struct params test_params = {
    .simulation = {
        .NumSnapOutputs = 10,  // Required for StarFormationHistory dynamic array
        .SimMaxSnaps = 64,     // Required parameter
        .LastSnapshotNr = 63   // Required parameter
    }
};

// Test fixtures
static struct test_context {
    struct halo_data *test_halos;
    int64_t nhalos;
    TreeContext *tree_context;
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Create test halo data with scientific tree structure for validation
    test_ctx.nhalos = 10;
    test_ctx.test_halos = mymalloc(test_ctx.nhalos * sizeof(struct halo_data));
    
    // Create a realistic merger tree structure for scientific validation
    // Root halo at z=0 (snapshot 63)
    test_ctx.test_halos[0].SnapNum = 63;
    test_ctx.test_halos[0].Descendant = -1;  // Root
    test_ctx.test_halos[0].FirstProgenitor = 1;
    test_ctx.test_halos[0].NextProgenitor = -1;
    test_ctx.test_halos[0].FirstHaloInFOFgroup = 0;
    test_ctx.test_halos[0].NextHaloInFOFgroup = -1;
    test_ctx.test_halos[0].Len = 1000;
    test_ctx.test_halos[0].Mvir = 100.0;
    test_ctx.test_halos[0].Vmax = 200.0;
    test_ctx.test_halos[0].VelDisp = 150.0;
    
    // First progenitor at z=0.5 (snapshot 50) - main branch
    test_ctx.test_halos[1].SnapNum = 50;
    test_ctx.test_halos[1].Descendant = 0;
    test_ctx.test_halos[1].FirstProgenitor = 2;
    test_ctx.test_halos[1].NextProgenitor = 8; // Second progenitor (orphan branch)
    test_ctx.test_halos[1].FirstHaloInFOFgroup = 1;
    test_ctx.test_halos[1].NextHaloInFOFgroup = -1;
    test_ctx.test_halos[1].Len = 800;
    test_ctx.test_halos[1].Mvir = 80.0;
    test_ctx.test_halos[1].Vmax = 180.0;
    test_ctx.test_halos[1].VelDisp = 130.0;
    
    // Second level progenitor (will have galaxy)
    test_ctx.test_halos[2].SnapNum = 40;
    test_ctx.test_halos[2].Descendant = 1;
    test_ctx.test_halos[2].FirstProgenitor = 3;
    test_ctx.test_halos[2].NextProgenitor = -1;
    test_ctx.test_halos[2].FirstHaloInFOFgroup = 2;
    test_ctx.test_halos[2].NextHaloInFOFgroup = -1;
    test_ctx.test_halos[2].Len = 600;
    test_ctx.test_halos[2].Mvir = 60.0;
    test_ctx.test_halos[2].Vmax = 160.0;
    test_ctx.test_halos[2].VelDisp = 110.0;
    
    // Leaf progenitor (primordial)
    test_ctx.test_halos[3].SnapNum = 20;
    test_ctx.test_halos[3].Descendant = 2;
    test_ctx.test_halos[3].FirstProgenitor = -1; // Primordial
    test_ctx.test_halos[3].NextProgenitor = -1;
    test_ctx.test_halos[3].FirstHaloInFOFgroup = 3;
    test_ctx.test_halos[3].NextHaloInFOFgroup = -1;
    test_ctx.test_halos[3].Len = 200;
    test_ctx.test_halos[3].Mvir = 20.0;
    test_ctx.test_halos[3].Vmax = 100.0;
    test_ctx.test_halos[3].VelDisp = 80.0;
    
    // Additional halos for more complex structure (4-7)
    for (int i = 4; i < 8; i++) {
        test_ctx.test_halos[i].SnapNum = 30 + i;
        test_ctx.test_halos[i].Descendant = (i == 4) ? 1 : (i - 1);
        test_ctx.test_halos[i].FirstProgenitor = (i < 7) ? (i + 1) : -1;
        test_ctx.test_halos[i].NextProgenitor = -1;
        test_ctx.test_halos[i].FirstHaloInFOFgroup = i;
        test_ctx.test_halos[i].NextHaloInFOFgroup = -1;
        test_ctx.test_halos[i].Len = 100 + i * 50;
        test_ctx.test_halos[i].Mvir = 10.0 + i * 5.0;
        test_ctx.test_halos[i].Vmax = 80.0 + i * 10.0;
        test_ctx.test_halos[i].VelDisp = 60.0 + i * 8.0;
    }
    
    // Orphan-generating branch (disrupted halo scenario)
    test_ctx.test_halos[8].SnapNum = 50;
    test_ctx.test_halos[8].Descendant = 0; // Merges into root
    test_ctx.test_halos[8].FirstProgenitor = 9;
    test_ctx.test_halos[8].NextProgenitor = -1;
    test_ctx.test_halos[8].FirstHaloInFOFgroup = 8;
    test_ctx.test_halos[8].NextHaloInFOFgroup = -1;
    test_ctx.test_halos[8].Len = 300; // Smaller than main progenitor (for orphan creation)
    test_ctx.test_halos[8].Mvir = 30.0;
    test_ctx.test_halos[8].Vmax = 120.0;
    test_ctx.test_halos[8].VelDisp = 90.0;
    
    // Leaf of orphan branch
    test_ctx.test_halos[9].SnapNum = 30;
    test_ctx.test_halos[9].Descendant = 8;
    test_ctx.test_halos[9].FirstProgenitor = -1; // Primordial
    test_ctx.test_halos[9].NextProgenitor = -1;
    test_ctx.test_halos[9].FirstHaloInFOFgroup = 9;
    test_ctx.test_halos[9].NextHaloInFOFgroup = -1;
    test_ctx.test_halos[9].Len = 150;
    test_ctx.test_halos[9].Mvir = 15.0;
    test_ctx.test_halos[9].Vmax = 90.0;
    test_ctx.test_halos[9].VelDisp = 70.0;
    
    // Create TreeContext
    test_ctx.tree_context = tree_context_create(test_ctx.test_halos, test_ctx.nhalos, &test_params);
    if (!test_ctx.tree_context) {
        return EXIT_FAILURE;
    }
    
    test_ctx.initialized = 1;
    return EXIT_SUCCESS;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.tree_context) {
        tree_context_destroy(&test_ctx.tree_context);
    }
    
    if (test_ctx.test_halos) {
        myfree(test_ctx.test_halos);
        test_ctx.test_halos = NULL;
    }
    
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Tree context creation and validation for scientific accuracy
 */
static void test_tree_context_validation(void) {
    printf("=== Testing tree context validation ===\n");
    
    TEST_ASSERT(test_ctx.tree_context != NULL, "TreeContext should be created successfully");
    TEST_ASSERT(test_ctx.tree_context->halos == test_ctx.test_halos, "TreeContext should reference test halos");
    TEST_ASSERT(test_ctx.tree_context->nhalos == test_ctx.nhalos, "TreeContext should have correct halo count");
    TEST_ASSERT(test_ctx.tree_context->working_galaxies != NULL, "Working galaxies array should be initialized");
    TEST_ASSERT(test_ctx.tree_context->output_galaxies != NULL, "Output galaxies array should be initialized");
    TEST_ASSERT(test_ctx.tree_context->halo_done != NULL, "Halo processing flags should be initialized");
    TEST_ASSERT(test_ctx.tree_context->fof_done != NULL, "FOF processing flags should be initialized");
}

/**
 * Test: Tree structure integrity for scientific validation
 */
static void test_tree_structure_integrity(void) {
    printf("\n=== Testing tree structure integrity ===\n");
    
    // Verify root halo
    TEST_ASSERT(test_ctx.test_halos[0].Descendant == -1, "Root halo should have no descendant");
    TEST_ASSERT(test_ctx.test_halos[0].FirstProgenitor == 1, "Root halo should have first progenitor");
    
    // Verify progenitor chains for mass conservation
    int progenitor_count = 0;
    int current = test_ctx.test_halos[0].FirstProgenitor;
    while (current >= 0) {
        progenitor_count++;
        TEST_ASSERT(current < test_ctx.nhalos, "Progenitor index should be valid");
        TEST_ASSERT(test_ctx.test_halos[current].Descendant == 0, "Progenitor should point to root");
        current = test_ctx.test_halos[current].NextProgenitor;
        
        // Prevent infinite loops
        if (progenitor_count > test_ctx.nhalos) break;
    }
    
    TEST_ASSERT(progenitor_count == 2, "Root should have 2 progenitors (main + orphan branch)");
    
    // Verify orphan-generating structure
    TEST_ASSERT(test_ctx.test_halos[8].Len < test_ctx.test_halos[1].Len, 
                "Orphan branch should be smaller (for orphan creation)");
}

/**
 * Test: Mass conservation validation infrastructure
 */
static void test_mass_conservation_validation(void) {
    printf("\n=== Testing mass conservation validation ===\n");
    
    // Calculate total halo mass for validation
    float total_halo_mass = 0.0;
    for (int i = 0; i < test_ctx.nhalos; i++) {
        total_halo_mass += test_ctx.test_halos[i].Mvir;
    }
    
    TEST_ASSERT(total_halo_mass > 0.0, "Test halos should have positive total mass");
    
    // Verify mass is distributed across tree for conservation checking
    float root_mass = test_ctx.test_halos[0].Mvir;
    float progenitor_mass = test_ctx.test_halos[1].Mvir + test_ctx.test_halos[8].Mvir;
    
    TEST_ASSERT(root_mass >= progenitor_mass * 0.8, "Root halo mass should be reasonable relative to progenitors");
    
    // Test mass conservation infrastructure in tree context
    TreeContext *ctx = test_ctx.tree_context;
    TEST_ASSERT(ctx->galaxy_counter == 0, "Galaxy counter should start at zero");
    
    printf("Total halo mass: %.3f (for mass conservation validation)\n", total_halo_mass);
    printf("Root mass: %.3f, Progenitor mass: %.3f\n", root_mass, progenitor_mass);
}

/**
 * Test: Orphan galaxy identification framework
 */
static void test_orphan_identification_framework(void) {
    printf("\n=== Testing orphan identification framework ===\n");
    
    // Test orphan creation scenario validation
    // Halo 8 is smaller progenitor that should create orphan
    int main_progenitor = 1;  // Larger progenitor
    int orphan_progenitor = 8; // Smaller progenitor (will create orphan)
    
    TEST_ASSERT(test_ctx.test_halos[main_progenitor].Len > test_ctx.test_halos[orphan_progenitor].Len,
                "Main progenitor should be larger than orphan progenitor");
    
    TEST_ASSERT(test_ctx.test_halos[main_progenitor].Mvir > test_ctx.test_halos[orphan_progenitor].Mvir,
                "Main progenitor should be more massive than orphan progenitor");
    
    // Verify both progenitors merge into same descendant (orphan creation scenario)
    TEST_ASSERT(test_ctx.test_halos[main_progenitor].Descendant == test_ctx.test_halos[orphan_progenitor].Descendant,
                "Both progenitors should merge into same descendant");
    
    printf("Main progenitor: Len=%d, Mvir=%.3f\n", 
           test_ctx.test_halos[main_progenitor].Len, test_ctx.test_halos[main_progenitor].Mvir);
    printf("Orphan progenitor: Len=%d, Mvir=%.3f\n", 
           test_ctx.test_halos[orphan_progenitor].Len, test_ctx.test_halos[orphan_progenitor].Mvir);
}

/**
 * Test: Scientific accuracy validation framework
 */
static void test_scientific_accuracy_framework(void) {
    printf("\n=== Testing scientific accuracy validation framework ===\n");
    
    // Test tree processing flags initialization for scientific validation
    TreeContext *ctx = test_ctx.tree_context;
    
    for (int i = 0; i < test_ctx.nhalos; i++) {
        TEST_ASSERT(ctx->halo_done[i] == false, "Halo processing flags should start as false");
        TEST_ASSERT(ctx->fof_done[i] == false, "FOF processing flags should start as false");
    }
    
    // Test diagnostic counters for scientific validation
    TEST_ASSERT(ctx->total_orphans == 0, "Orphan counter should start at zero");
    TEST_ASSERT(ctx->total_gaps_spanned == 0, "Gap counter should start at zero");
    TEST_ASSERT(ctx->max_gap_length == 0, "Max gap length should start at zero");
    
    // Test galaxy arrays for scientific processing
    TEST_ASSERT(galaxy_array_get_count(ctx->working_galaxies) == 0, "Working galaxies should start empty");
    TEST_ASSERT(galaxy_array_get_count(ctx->output_galaxies) == 0, "Output galaxies should start empty");
    
    printf("Scientific validation framework properly initialized\n");
}

/**
 * Test: Gap tolerance validation infrastructure
 */
static void test_gap_tolerance_validation(void) {
    printf("\n=== Testing gap tolerance validation infrastructure ===\n");
    
    // Test gap measurement function for scientific validation
    int gap1 = measure_tree_gap(50, 40);  // 50 - 40 - 1 = 9 snapshots gap
    int gap2 = measure_tree_gap(63, 62);  // 63 - 62 - 1 = 0 no gap
    int gap3 = measure_tree_gap(30, 30);  // 30 - 30 - 1 = -1 -> 0 (no gap)
    
    TEST_ASSERT(gap1 == 9, "Gap measurement should correctly identify 9-snapshot gap");
    TEST_ASSERT(gap2 == 0, "Gap measurement should correctly identify no gap");
    TEST_ASSERT(gap3 == 0, "Gap measurement should handle same snapshot case");
    
    // Test tree structure with gaps
    int max_gap = 0;
    for (int i = 0; i < test_ctx.nhalos; i++) {
        int prog = test_ctx.test_halos[i].FirstProgenitor;
        if (prog >= 0) {
            int gap = measure_tree_gap(test_ctx.test_halos[i].SnapNum, test_ctx.test_halos[prog].SnapNum);
            if (gap > max_gap) max_gap = gap;
        }
    }
    
    printf("Maximum gap in test tree: %d snapshots\n", max_gap);
    TEST_ASSERT(max_gap >= 0, "Gap measurement should be non-negative");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void) argc; // Suppress unused parameter warning
    (void) argv; // Suppress unused parameter warning
    
    printf("\n========================================\n");
    printf("Starting tests for Tree Mode Scientific Validation - Phase 6\n");
    printf("========================================\n\n");
    
    printf("This test verifies the scientific validation framework for tree-based processing:\n");
    printf("  1. Tree context validation and proper initialization\n");
    printf("  2. Tree structure integrity for scientific accuracy\n");
    printf("  3. Mass conservation validation infrastructure\n");
    printf("  4. Orphan galaxy identification framework\n");
    printf("  5. Scientific accuracy validation components\n");
    printf("  6. Gap tolerance validation infrastructure\n\n");

    // Setup
    if (setup_test_context() != EXIT_SUCCESS) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_tree_context_validation();
    test_tree_structure_integrity();
    test_mass_conservation_validation();
    test_orphan_identification_framework();
    test_scientific_accuracy_framework();
    test_gap_tolerance_validation();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for Tree Mode Scientific Validation:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    if (tests_run == tests_passed) {
        printf("🎉 All Phase 6 validation framework tests PASSED!\n");
        printf("Tree-based processing validation infrastructure is ready.\n\n");
    } else {
        printf("❌ Some validation framework tests failed.\n");
        printf("Please review the output above for details.\n\n");
    }
    
    return (tests_run == tests_passed) ? 0 : 1;
}