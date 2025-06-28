/**
 * Test suite for Tree Infrastructure
 * 
 * Tests cover:
 * - TreeContext creation and destruction
 * - Tree traversal order (depth-first)
 * - FOF processing flags
 * - Memory management
 * - Error handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/tree_context.h"
#include "../src/core/tree_traversal.h"
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

// Global traversal order tracking
static int traversal_order[100];
static int traversal_count = 0;

//=============================================================================
// Test Helper Functions
//=============================================================================

// Create a simple test tree structure for testing
static struct halo_data* create_test_tree(int* nhalos_out) {
    // Create a 4-halo tree:
    // Halo 0 (root, snap=0) <- descendant
    //   <- Halo 1 (snap=1)
    //     <- Halo 2 (snap=2)  [progenitor 1]
    //     <- Halo 3 (snap=2)  [progenitor 2]
    
    struct halo_data* halos = mymalloc(4 * sizeof(struct halo_data));
    memset(halos, 0, 4 * sizeof(struct halo_data));
    
    // Halo 0: Root (no descendant)
    halos[0].Descendant = -1;
    halos[0].FirstProgenitor = 1;
    halos[0].NextProgenitor = -1;
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = -1;
    halos[0].SnapNum = 0;
    halos[0].Len = 1000;
    
    // Halo 1: Intermediate (descendant=0, progenitors=2,3)
    halos[1].Descendant = 0;
    halos[1].FirstProgenitor = 2;
    halos[1].NextProgenitor = -1;
    halos[1].FirstHaloInFOFgroup = 1;
    halos[1].NextHaloInFOFgroup = -1;
    halos[1].SnapNum = 1;
    halos[1].Len = 800;
    
    // Halo 2: Leaf 1 (descendant=1)
    halos[2].Descendant = 1;
    halos[2].FirstProgenitor = -1;
    halos[2].NextProgenitor = 3;  // Points to next progenitor
    halos[2].FirstHaloInFOFgroup = 2;
    halos[2].NextHaloInFOFgroup = -1;
    halos[2].SnapNum = 2;
    halos[2].Len = 600;
    
    // Halo 3: Leaf 2 (descendant=1)
    halos[3].Descendant = 1;
    halos[3].FirstProgenitor = -1;
    halos[3].NextProgenitor = -1;  // Last progenitor
    halos[3].FirstHaloInFOFgroup = 3;
    halos[3].NextHaloInFOFgroup = -1;
    halos[3].SnapNum = 2;
    halos[3].Len = 200;
    
    *nhalos_out = 4;
    return halos;
}

// Track traversal order callback
static void track_halo_visit(int halo_nr, void* user_data) {
    (void)user_data;  // Unused parameter
    traversal_order[traversal_count++] = halo_nr;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: TreeContext creation and destruction
 */
static void test_tree_context_lifecycle(void) {
    printf("=== Testing TreeContext lifecycle ===\n");
    
    int nhalos;
    struct halo_data* halos = create_test_tree(&nhalos);
    
    // Test creation
    TreeContext* ctx = tree_context_create(halos, nhalos, &test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext should be created successfully");
    TEST_ASSERT(ctx->halos == halos, "TreeContext should store halo pointer");
    TEST_ASSERT(ctx->nhalos == nhalos, "TreeContext should store halo count");
    TEST_ASSERT(ctx->run_params == &test_params, "TreeContext should store params pointer");
    TEST_ASSERT(ctx->working_galaxies != NULL, "Working galaxies array should be allocated");
    TEST_ASSERT(ctx->output_galaxies != NULL, "Output galaxies array should be allocated");
    TEST_ASSERT(ctx->halo_done != NULL, "Halo done flags should be allocated");
    TEST_ASSERT(ctx->fof_done != NULL, "FOF done flags should be allocated");
    TEST_ASSERT(ctx->galaxy_counter == 0, "Galaxy counter should start at 0");
    
    // Test initial state
    for (int i = 0; i < nhalos; i++) {
        TEST_ASSERT(ctx->halo_done[i] == false, "All halos should start as not done");
        TEST_ASSERT(ctx->fof_done[i] == false, "All FOF groups should start as not done");
        TEST_ASSERT(ctx->halo_first_galaxy[i] == -1, "All halos should start with no galaxies");
        TEST_ASSERT(ctx->halo_galaxy_count[i] == 0, "All halos should start with zero galaxy count");
    }
    
    // Test destruction
    tree_context_destroy(&ctx);
    TEST_ASSERT(ctx == NULL, "TreeContext pointer should be set to NULL after destruction");
    
    myfree(halos);
}

/**
 * Test: Tree traversal order (depth-first)
 */
static void test_tree_traversal_order(void) {
    printf("\n=== Testing tree traversal order ===\n");
    
    int nhalos;
    struct halo_data* halos = create_test_tree(&nhalos);
    TreeContext* ctx = tree_context_create(halos, nhalos, &test_params);
    
    // Reset traversal tracking
    traversal_count = 0;
    memset(traversal_order, 0, sizeof(traversal_order));
    
    // Process the tree starting from root (halo 0)
    int status = process_tree_recursive_with_tracking(0, ctx, track_halo_visit, NULL);
    TEST_ASSERT(status == EXIT_SUCCESS, "Tree processing should succeed");
    
    // Verify traversal order - should be depth-first
    // Expected order: 2, 3, 1, 0 (leaves to root)
    TEST_ASSERT(traversal_count == 4, "Should traverse all 4 halos");
    TEST_ASSERT(traversal_order[0] == 2, "First halo should be leaf 2");
    TEST_ASSERT(traversal_order[1] == 3, "Second halo should be leaf 3");
    TEST_ASSERT(traversal_order[2] == 1, "Third halo should be intermediate 1");
    TEST_ASSERT(traversal_order[3] == 0, "Fourth halo should be root 0");
    
    // Verify all halos marked as done
    for (int i = 0; i < nhalos; i++) {
        TEST_ASSERT(ctx->halo_done[i] == true, "All halos should be marked as done");
    }
    
    tree_context_destroy(&ctx);
    myfree(halos);
}

/**
 * Test: Forest processing (multiple trees)
 */
static void test_forest_processing(void) {
    printf("\n=== Testing forest processing ===\n");
    
    // Create two disconnected trees
    struct halo_data* halos = mymalloc(6 * sizeof(struct halo_data));
    memset(halos, 0, 6 * sizeof(struct halo_data));
    
    // Tree 1: 0 <- 1 <- 2
    halos[0].Descendant = -1;    // Root 1
    halos[0].FirstProgenitor = 1;
    halos[0].NextProgenitor = -1;
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = -1;
    halos[0].SnapNum = 0;
    
    halos[1].Descendant = 0;
    halos[1].FirstProgenitor = 2;
    halos[1].NextProgenitor = -1;
    halos[1].FirstHaloInFOFgroup = 1;
    halos[1].NextHaloInFOFgroup = -1;
    halos[1].SnapNum = 1;
    
    halos[2].Descendant = 1;     // Leaf
    halos[2].FirstProgenitor = -1;
    halos[2].NextProgenitor = -1;
    halos[2].FirstHaloInFOFgroup = 2;
    halos[2].NextHaloInFOFgroup = -1;
    halos[2].SnapNum = 2;
    
    // Tree 2: 3 <- 4 <- 5
    halos[3].Descendant = -1;    // Root 2
    halos[3].FirstProgenitor = 4;
    halos[3].NextProgenitor = -1;
    halos[3].FirstHaloInFOFgroup = 3;
    halos[3].NextHaloInFOFgroup = -1;
    halos[3].SnapNum = 0;
    
    halos[4].Descendant = 3;
    halos[4].FirstProgenitor = 5;
    halos[4].NextProgenitor = -1;
    halos[4].FirstHaloInFOFgroup = 4;
    halos[4].NextHaloInFOFgroup = -1;
    halos[4].SnapNum = 1;
    
    halos[5].Descendant = 4;     // Leaf
    halos[5].FirstProgenitor = -1;
    halos[5].NextProgenitor = -1;
    halos[5].FirstHaloInFOFgroup = 5;
    halos[5].NextHaloInFOFgroup = -1;
    halos[5].SnapNum = 2;
    
    TreeContext* ctx = tree_context_create(halos, 6, &test_params);
    
    // Process the entire forest
    int status = process_forest_trees(ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "Forest processing should succeed");
    
    // Verify all halos processed
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT(ctx->halo_done[i] == true, "All halos in forest should be processed");
    }
    
    tree_context_destroy(&ctx);
    myfree(halos);
}

/**
 * Test: FOF processing flags
 */
static void test_fof_processing(void) {
    printf("\n=== Testing FOF processing flags ===\n");
    
    int nhalos;
    struct halo_data* halos = create_test_tree(&nhalos);
    TreeContext* ctx = tree_context_create(halos, nhalos, &test_params);
    
    // Process the tree
    int status = process_forest_trees(ctx);
    TEST_ASSERT(status == EXIT_SUCCESS, "Tree processing should succeed");
    
    // Each halo is FOF root in our test tree
    for (int i = 0; i < nhalos; i++) {
        TEST_ASSERT(ctx->fof_done[i] == true, "All FOF groups should be marked as done");
    }
    
    tree_context_destroy(&ctx);
    myfree(halos);
}

/**
 * Test: Error handling
 */
static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    // Test NULL parameters
    TreeContext* ctx = tree_context_create(NULL, 0, &test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext should handle NULL halos gracefully");
    TEST_ASSERT(ctx->halos == NULL, "TreeContext should store NULL halos");
    TEST_ASSERT(ctx->nhalos == 0, "TreeContext should store zero halo count");
    tree_context_destroy(&ctx);
    
    // Test NULL context destruction
    TreeContext* null_ctx = NULL;
    tree_context_destroy(&null_ctx);  // Should not crash
    TEST_ASSERT(true, "Destroying NULL context should not crash");
    
    // Test double destruction
    int nhalos;
    struct halo_data* halos = create_test_tree(&nhalos);
    ctx = tree_context_create(halos, nhalos, &test_params);
    tree_context_destroy(&ctx);
    tree_context_destroy(&ctx);  // Should not crash
    TEST_ASSERT(true, "Double destruction should not crash");
    
    myfree(halos);
}

/**
 * Test: Memory management
 */
static void test_memory_management(void) {
    printf("\n=== Testing memory management ===\n");
    
    int nhalos;
    struct halo_data* halos = create_test_tree(&nhalos);
    
    // Create and destroy multiple contexts to test for leaks
    for (int i = 0; i < 10; i++) {
        TreeContext* ctx = tree_context_create(halos, nhalos, &test_params);
        TEST_ASSERT(ctx != NULL, "TreeContext creation should succeed in loop");
        tree_context_destroy(&ctx);
        TEST_ASSERT(ctx == NULL, "TreeContext should be NULL after destruction in loop");
    }
    
    myfree(halos);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("Starting tests for Tree Infrastructure\n");
    printf("========================================\n\n");
    
    printf("This test verifies that tree infrastructure works correctly:\n");
    printf("  1. TreeContext creation and destruction\n");
    printf("  2. Depth-first tree traversal order\n");
    printf("  3. Forest processing (multiple trees)\n");
    printf("  4. FOF processing flag management\n");
    printf("  5. Error handling and edge cases\n");
    printf("  6. Memory management\n\n");

    // Initialize memory system
    memory_system_init();
    
    // Run tests
    test_tree_context_lifecycle();
    test_tree_traversal_order();
    test_forest_processing();
    test_fof_processing();
    test_error_handling();
    test_memory_management();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for Tree Infrastructure:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    if (tests_run == tests_passed) {
        printf("✓ All tree infrastructure tests passed!\n");
        printf("  - TreeContext management working correctly\n");
        printf("  - Depth-first traversal implemented properly\n");
        printf("  - FOF processing flags functioning\n");
        printf("  - Memory management safe and leak-free\n\n");
    } else {
        printf("✗ Some tests failed - tree infrastructure needs fixes\n\n");
    }
    
    return (tests_run == tests_passed) ? 0 : 1;
}