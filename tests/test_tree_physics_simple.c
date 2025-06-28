/**
 * Test suite for Tree Physics Integration (Simplified)
 * 
 * Tests cover:
 * - Basic physics functionality
 * - Error handling
 * - FOF integration
 * - Pipeline verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/tree_physics.h"
#include "../src/core/tree_context.h"
#include "../src/core/tree_fof.h"
#include "../src/core/core_allvars.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/core/core_event_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_properties.h"

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
    double* age_array;  // Allocated Age array
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize minimal parameters
    test_ctx.test_params.simulation.SimMaxSnaps = 64;
    test_ctx.test_params.simulation.LastSnapshotNr = 63;
    test_ctx.test_params.simulation.Snaplistlen = 64;
    test_ctx.test_params.runtime.ThisTask = 0;
    test_ctx.test_params.runtime.NTasks = 1;
    
    // Initialize ZZ array (fixed array, no allocation needed)
    for (int i = 0; i < 64; i++) {
        test_ctx.test_params.simulation.ZZ[i] = 10.0 - (double)i * 0.1;  // Simple redshift sequence
    }
    
    // Allocate and initialize Age array (dynamic pointer)
    test_ctx.age_array = malloc(64 * sizeof(double));
    if (!test_ctx.age_array) {
        return -1;  // Allocation failed
    }
    test_ctx.test_params.simulation.Age = test_ctx.age_array;
    
    for (int i = 0; i < 64; i++) {
        test_ctx.test_params.simulation.Age[i] = (double)i * 0.5;  // Simple age sequence
    }
    
    // **CRITICAL: Initialize core systems required for physics pipeline**
    // Initialize logging system first (required for all logging)
    if (initialize_logging(&test_ctx.test_params) != 0) {
        printf("Failed to initialize logging system\n");
        return -1;
    }
    
    // Initialize basic units and constants
    initialize_units(&test_ctx.test_params);
    
    // Initialize module system (required for pipeline)
    if (initialize_module_system(&test_ctx.test_params) != 0) {
        printf("Failed to initialize module system\n");
        return -1;
    }
    
    // Initialize module callback system
    initialize_module_callback_system();
    
    // Initialize galaxy extension system
    initialize_galaxy_extension_system();
    
    // Initialize property system
    if (initialize_property_system(&test_ctx.test_params) != 0) {
        printf("Failed to initialize property system\n");
        cleanup_module_system();
        return -1;
    }
    
    // Initialize standard properties
    initialize_standard_properties(&test_ctx.test_params);
    
    // Initialize event system
    initialize_event_system();
    
    // Initialize pipeline system (creates physics-free pipeline for tests)
    initialize_pipeline_system();
    
    test_ctx.initialized = 1;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (!test_ctx.initialized) return;
    
    // Clean up core systems in reverse order
    cleanup_pipeline_system();
    cleanup_event_system();
    cleanup_property_system();
    cleanup_galaxy_extension_system();
    cleanup_module_system();
    cleanup_logging();
    
    if (test_ctx.age_array) {
        free(test_ctx.age_array);
        test_ctx.age_array = NULL;
    }
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Basic physics functionality
 */
static void test_physics_basic_functionality(void) {
    printf("=== Testing basic physics functionality ===\n");
    
    // Create minimal test structure
    struct halo_data halos[1];
    memset(halos, 0, sizeof(halos));
    
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = -1;
    halos[0].FirstProgenitor = -1;
    halos[0].NextProgenitor = -1;
    halos[0].Descendant = -1;
    halos[0].SnapNum = 10;
    
    // Create context
    TreeContext* ctx = tree_context_create(halos, 1, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext should be created successfully");
    
    // Test physics application to empty FOF (should succeed gracefully)
    int result = apply_physics_to_fof(0, ctx);
    TEST_ASSERT(result == EXIT_SUCCESS, "Physics application should succeed");
    
    // Should have no output galaxies
    TEST_ASSERT(galaxy_array_get_count(ctx->output_galaxies) == 0, 
                "Empty FOF should produce no galaxies");
    
    // Cleanup
    tree_context_destroy(&ctx);
}

/**
 * Test: Error handling
 */
static void test_physics_error_handling(void) {
    printf("\n=== Testing physics error handling ===\n");
    
    // Test with NULL context
    int result = apply_physics_to_fof(0, NULL);
    TEST_ASSERT(result == EXIT_FAILURE, "NULL context should return failure");
}

/**
 * Test: FOF integration
 */
static void test_fof_integration(void) {
    printf("\n=== Testing FOF integration ===\n");
    
    // Create simple FOF structure
    struct halo_data halos[1];
    memset(halos, 0, sizeof(halos));
    
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = -1;
    halos[0].FirstProgenitor = -1;
    halos[0].NextProgenitor = -1;
    halos[0].Descendant = -1;
    halos[0].SnapNum = 15;
    halos[0].Len = 1000;
    
    TreeContext* ctx = tree_context_create(halos, 1, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext should be created successfully");
    
    // Verify FOF is ready (no progenitors)
    TEST_ASSERT(is_fof_ready(0, ctx) == true, "FOF should be ready");
    
    // Process FOF group with physics integration
    int result = process_tree_fof_group(0, ctx);
    TEST_ASSERT(result == EXIT_SUCCESS, "FOF processing should succeed");
    
    // Verify FOF is marked as processed
    TEST_ASSERT(ctx->fof_done[0] == true, "FOF should be marked as done");
    
    tree_context_destroy(&ctx);
}

/**
 * Test: Pipeline verification
 */
static void test_pipeline_verification(void) {
    printf("\n=== Testing pipeline verification ===\n");
    
    // This test verifies that the physics pipeline integration compiles
    // and links correctly with the existing physics system
    
    struct halo_data halos[1];
    memset(halos, 0, sizeof(halos));
    
    halos[0].FirstHaloInFOFgroup = 0;
    halos[0].NextHaloInFOFgroup = -1;
    halos[0].FirstProgenitor = -1;
    halos[0].NextProgenitor = -1;
    halos[0].Descendant = -1;
    halos[0].SnapNum = 20;
    halos[0].Len = 500;
    
    TreeContext* ctx = tree_context_create(halos, 1, &test_ctx.test_params);
    TEST_ASSERT(ctx != NULL, "TreeContext should be created successfully");
    
    // Test direct physics application
    int result = apply_physics_to_fof(0, ctx);
    TEST_ASSERT(result == EXIT_SUCCESS, "Pipeline integration should work");
    
    tree_context_destroy(&ctx);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc;  // Suppress unused parameter warning
    (void)argv;  // Suppress unused parameter warning
    
    printf("\n========================================\n");
    printf("Starting tests for tree_physics_simple\n");
    printf("========================================\n\n");
    
    printf("This test verifies that simplified tree physics integration:\n");
    printf("  1. Basic functionality works correctly\n");
    printf("  2. Error handling is robust\n");
    printf("  3. FOF integration is functional\n");
    printf("  4. Pipeline verification succeeds\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_physics_basic_functionality();
    test_physics_error_handling();
    test_fof_integration();
    test_pipeline_verification();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for tree_physics_simple:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}