/**
 * Test suite for Halo Progenitor Integrity
 * 
 * Tests cover:
 * - Halo pointer index validation
 * - Data corruption detection
 * - Tree structure integrity
 * - Error handling for invalid indices
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "core_allvars.h"
#include "core_io_tree.h"
#include "core_read_parameter_file.h"
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
    } \
} while(0)

// Test fixtures
static struct test_context {
    struct params run_params;
    struct forest_info forest_info;
    struct halo_data *halos;
    int64_t nhalos;
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Load test parameter file
    char test_param_file[] = "./tests/test_data/test-mini-millennium.par";
    int status = read_parameter_file(test_param_file, &test_ctx.run_params);
    if (status != 0) {
        printf("ERROR: Failed to read parameter file: %s\n", test_param_file);
        return -1;
    }
    
    // Setup forest I/O
    status = setup_forests_io(&test_ctx.run_params, &test_ctx.forest_info, 0, 1);
    if (status != 0) {
        printf("ERROR: Failed to setup forests I/O\n");
        return -1;
    }
    
    // Load forest 0 for testing
    test_ctx.nhalos = load_forest(&test_ctx.run_params, 0, &test_ctx.halos, &test_ctx.forest_info);
    if (test_ctx.nhalos <= 0) {
        printf("ERROR: Failed to load forest 0, nhalos = %lld\n", test_ctx.nhalos);
        cleanup_forests_io(test_ctx.run_params.io.TreeType, &test_ctx.forest_info);
        return -1;
    }
    
    test_ctx.initialized = 1;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.halos != NULL) {
        myfree(test_ctx.halos);
        test_ctx.halos = NULL;
    }
    
    if (test_ctx.initialized) {
        cleanup_forests_io(test_ctx.run_params.io.TreeType, &test_ctx.forest_info);
    }
    
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Basic halo data loading
 */
static void test_halo_data_loading(void) {
    printf("=== Testing halo data loading ===\n");
    
    TEST_ASSERT(test_ctx.nhalos > 0, "Should load at least one halo");
    TEST_ASSERT(test_ctx.halos != NULL, "Halo array should be allocated");
    
    printf("Loaded %lld halos from test forest\n", test_ctx.nhalos);
}

/**
 * Test: Descendant pointer integrity
 */
static void test_descendant_pointers(void) {
    printf("\n=== Testing Descendant pointer integrity ===\n");
    
    int validation_errors = 0;
    
    for (int64_t i = 0; i < test_ctx.nhalos; ++i) {
        int desc = test_ctx.halos[i].Descendant;
        if (desc != -1 && (desc < 0 || desc >= test_ctx.nhalos)) {
            validation_errors++;
            if (validation_errors <= 5) {  // Only report first 5 errors
                printf("ERROR: Halo %lld has invalid Descendant index: %d (valid range: -1 or 0-%lld)\n", 
                       i, desc, test_ctx.nhalos-1);
            }
        }
    }
    
    TEST_ASSERT(validation_errors == 0, "All Descendant pointers should be valid");
    if (validation_errors > 0) {
        printf("Found %d invalid Descendant pointers\n", validation_errors);
    }
}

/**
 * Test: FirstProgenitor pointer integrity
 */
static void test_firstprogenitor_pointers(void) {
    printf("\n=== Testing FirstProgenitor pointer integrity ===\n");
    
    int validation_errors = 0;
    
    for (int64_t i = 0; i < test_ctx.nhalos; ++i) {
        int prog = test_ctx.halos[i].FirstProgenitor;
        if (prog != -1 && (prog < 0 || prog >= test_ctx.nhalos)) {
            validation_errors++;
            if (validation_errors <= 5) {
                printf("ERROR: Halo %lld has invalid FirstProgenitor index: %d (valid range: -1 or 0-%lld)\n", 
                       i, prog, test_ctx.nhalos-1);
            }
        }
    }
    
    TEST_ASSERT(validation_errors == 0, "All FirstProgenitor pointers should be valid");
    if (validation_errors > 0) {
        printf("Found %d invalid FirstProgenitor pointers\n", validation_errors);
    }
}

/**
 * Test: NextProgenitor pointer integrity
 */
static void test_nextprogenitor_pointers(void) {
    printf("\n=== Testing NextProgenitor pointer integrity ===\n");
    
    int validation_errors = 0;
    
    for (int64_t i = 0; i < test_ctx.nhalos; ++i) {
        int next = test_ctx.halos[i].NextProgenitor;
        if (next != -1 && (next < 0 || next >= test_ctx.nhalos)) {
            validation_errors++;
            if (validation_errors <= 5) {
                printf("ERROR: Halo %lld has invalid NextProgenitor index: %d (valid range: -1 or 0-%lld)\n", 
                       i, next, test_ctx.nhalos-1);
            }
        }
    }
    
    TEST_ASSERT(validation_errors == 0, "All NextProgenitor pointers should be valid");
    if (validation_errors > 0) {
        printf("Found %d invalid NextProgenitor pointers\n", validation_errors);
    }
}

/**
 * Test: FOF group pointer integrity
 */
static void test_fof_pointers(void) {
    printf("\n=== Testing FOF group pointer integrity ===\n");
    
    int validation_errors = 0;
    
    for (int64_t i = 0; i < test_ctx.nhalos; ++i) {
        // FirstHaloInFOFgroup should always be valid (not -1)
        int first_fof = test_ctx.halos[i].FirstHaloInFOFgroup;
        if (first_fof < 0 || first_fof >= test_ctx.nhalos) {
            validation_errors++;
            if (validation_errors <= 5) {
                printf("ERROR: Halo %lld has invalid FirstHaloInFOFgroup index: %d (valid range: 0-%lld)\n", 
                       i, first_fof, test_ctx.nhalos-1);
            }
        }
        
        // NextHaloInFOFgroup can be -1
        int next_fof = test_ctx.halos[i].NextHaloInFOFgroup;
        if (next_fof != -1 && (next_fof < 0 || next_fof >= test_ctx.nhalos)) {
            validation_errors++;
            if (validation_errors <= 5) {
                printf("ERROR: Halo %lld has invalid NextHaloInFOFgroup index: %d (valid range: -1 or 0-%lld)\n", 
                       i, next_fof, test_ctx.nhalos-1);
            }
        }
    }
    
    TEST_ASSERT(validation_errors == 0, "All FOF group pointers should be valid");
    if (validation_errors > 0) {
        printf("Found %d invalid FOF group pointers\n", validation_errors);
    }
}

/**
 * Test: Sample halo data inspection
 */
static void test_sample_data_inspection(void) {
    printf("\n=== Testing sample halo data inspection ===\n");
    
    int sample_size = (test_ctx.nhalos < 5) ? test_ctx.nhalos : 5;
    
    printf("Sample halo data (first %d halos):\n", sample_size);
    for (int i = 0; i < sample_size; i++) {
        printf("Halo %d: Desc=%d, FirstProg=%d, NextProg=%d, FirstFOF=%d, NextFOF=%d\n",
               i, test_ctx.halos[i].Descendant, test_ctx.halos[i].FirstProgenitor,
               test_ctx.halos[i].NextProgenitor, test_ctx.halos[i].FirstHaloInFOFgroup,
               test_ctx.halos[i].NextHaloInFOFgroup);
    }
    
    TEST_ASSERT(sample_size > 0, "Should have at least one halo to inspect");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc; /* Suppress unused parameter warning */
    (void)argv; /* Suppress unused parameter warning */
    printf("\n========================================\n");
    printf("Starting tests for test_halo_progenitor_integrity\n");
    printf("========================================\n\n");
    
    printf("This test verifies halo merger tree pointer integrity:\n");
    printf("  1. All halo index pointers are within valid ranges\n");
    printf("  2. No corrupted or garbage pointer values exist\n");
    printf("  3. Tree structure consistency is maintained\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_halo_data_loading();
    test_descendant_pointers();
    test_firstprogenitor_pointers();
    test_nextprogenitor_pointers();
    test_fof_pointers();
    test_sample_data_inspection();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_halo_progenitor_integrity:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}