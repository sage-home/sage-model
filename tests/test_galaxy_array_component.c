/**
 * @file test_galaxy_array_component.c
 * @brief Test the new GalaxyArray component implementation
 * 
 * This test specifically verifies the new GalaxyArray component that provides
 * a safe abstraction for managing dynamic arrays of GALAXY structs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "../src/core/core_allvars.h"
#include "../src/core/galaxy_array.h"
#include "../src/core/core_properties.h"
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
        return; \
    } else { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } \
} while(0)

// Test parameters structure
static struct params test_params = {0};

/**
 * @brief Create a minimal test galaxy with basic properties
 */
static int create_simple_test_galaxy(struct GALAXY* gal, int galaxy_id) {
    // Zero-initialize the galaxy struct
    memset(gal, 0, sizeof(struct GALAXY));
    
    // Set basic fields that don't require property allocation
    gal->GalaxyNr = galaxy_id;
    gal->Type = galaxy_id % 3;
    gal->SnapNum = 63;
    gal->Mvir = 1e10 + galaxy_id * 1e8;
    gal->Vmax = 200.0 + galaxy_id * 10.0;
    gal->Rvir = 100.0 + galaxy_id * 5.0;
    gal->GalaxyIndex = (uint64_t)(1000 + galaxy_id);
    
    // Initialize position with unique values
    gal->Pos[0] = galaxy_id * 10.0;
    gal->Pos[1] = galaxy_id * 20.0;
    gal->Pos[2] = galaxy_id * 30.0;
    
    // CRITICAL FIX: The test MUST allocate properties to be valid.
    if (allocate_galaxy_properties(gal, &test_params) != 0) {
        printf("ERROR: Test setup failed - could not allocate properties for galaxy %d\n", galaxy_id);
        return -1;
    }
    
    // Add some data to the properties to verify it's preserved.
    if (gal->properties) {
        GALAXY_PROP_Mvir(gal) = (float)galaxy_id * 1.5f;
    }
    
    return 0;
}

/**
 * @brief Test basic GalaxyArray creation and destruction
 */
static void test_galaxy_array_basic_operations() {
    printf("\n=== Testing GalaxyArray basic operations ===\n");
    
    // Test creation
    GalaxyArray* arr = galaxy_array_new();
    TEST_ASSERT(arr != NULL, "GalaxyArray creation should succeed");
    
    // Test initial state
    TEST_ASSERT(galaxy_array_get_count(arr) == 0, "Initial count should be 0");
    TEST_ASSERT(galaxy_array_get_raw_data(arr) == NULL || galaxy_array_get_count(arr) == 0, "Initial raw data should be NULL or count should be 0");
    
    // Test get with invalid index
    TEST_ASSERT(galaxy_array_get(arr, 0) == NULL, "Get with invalid index should return NULL");
    TEST_ASSERT(galaxy_array_get(arr, -1) == NULL, "Get with negative index should return NULL");
    
    // Test destruction
    galaxy_array_free(&arr);
    printf("GalaxyArray properly freed\n");
}

/**
 * @brief Test adding galaxies to the array
 */
static void test_galaxy_array_append() {
    printf("\n=== Testing GalaxyArray append operations ===\n");
    
    GalaxyArray* arr = galaxy_array_new();
    TEST_ASSERT(arr != NULL, "GalaxyArray creation should succeed");
    
    // Create test galaxies
    struct GALAXY test_gal1, test_gal2, test_gal3;
    create_simple_test_galaxy(&test_gal1, 1);
    create_simple_test_galaxy(&test_gal2, 2);
    create_simple_test_galaxy(&test_gal3, 3);
    
    // Test appending galaxies
    int index1 = galaxy_array_append(arr, &test_gal1, &test_params);
    TEST_ASSERT(index1 == 0, "First galaxy should be at index 0");
    TEST_ASSERT(galaxy_array_get_count(arr) == 1, "Count should be 1 after first append");
    
    int index2 = galaxy_array_append(arr, &test_gal2, &test_params);
    TEST_ASSERT(index2 == 1, "Second galaxy should be at index 1");
    TEST_ASSERT(galaxy_array_get_count(arr) == 2, "Count should be 2 after second append");
    
    int index3 = galaxy_array_append(arr, &test_gal3, &test_params);
    TEST_ASSERT(index3 == 2, "Third galaxy should be at index 2");
    TEST_ASSERT(galaxy_array_get_count(arr) == 3, "Count should be 3 after third append");
    
    // Test retrieving galaxies
    struct GALAXY* retrieved1 = galaxy_array_get(arr, 0);
    TEST_ASSERT(retrieved1 != NULL, "Retrieved galaxy should not be NULL");
    TEST_ASSERT(retrieved1->GalaxyNr == 1, "First galaxy should have GalaxyNr = 1");
    TEST_ASSERT(retrieved1->GalaxyIndex == 1001, "First galaxy should have GalaxyIndex = 1001");
    
    struct GALAXY* retrieved2 = galaxy_array_get(arr, 1);
    TEST_ASSERT(retrieved2 != NULL, "Second retrieved galaxy should not be NULL");
    TEST_ASSERT(retrieved2->GalaxyNr == 2, "Second galaxy should have GalaxyNr = 2");
    TEST_ASSERT(retrieved2->GalaxyIndex == 1002, "Second galaxy should have GalaxyIndex = 1002");
    
    struct GALAXY* retrieved3 = galaxy_array_get(arr, 2);
    TEST_ASSERT(retrieved3 != NULL, "Third retrieved galaxy should not be NULL");
    TEST_ASSERT(retrieved3->GalaxyNr == 3, "Third galaxy should have GalaxyNr = 3");
    TEST_ASSERT(retrieved3->GalaxyIndex == 1003, "Third galaxy should have GalaxyIndex = 1003");
    
    // Test raw data access
    struct GALAXY* raw_data = galaxy_array_get_raw_data(arr);
    TEST_ASSERT(raw_data != NULL, "Raw data should not be NULL");
    TEST_ASSERT(raw_data[0].GalaxyNr == 1, "Raw data first element should match");
    TEST_ASSERT(raw_data[1].GalaxyNr == 2, "Raw data second element should match");
    TEST_ASSERT(raw_data[2].GalaxyNr == 3, "Raw data third element should match");
    
    galaxy_array_free(&arr);
}

/**
 * @brief Test array expansion under stress
 */
static void test_galaxy_array_expansion() {
    printf("\n=== Testing GalaxyArray expansion under stress ===\n");
    
    GalaxyArray* arr = galaxy_array_new();
    TEST_ASSERT(arr != NULL, "GalaxyArray creation should succeed");
    
    const int STRESS_COUNT = 1000;
    
    printf("Adding %d galaxies to test expansion...\n", STRESS_COUNT);
    
    // Add many galaxies to force expansion
    for (int i = 0; i < STRESS_COUNT; i++) {
        struct GALAXY test_gal;
        create_simple_test_galaxy(&test_gal, i);
        
        int index = galaxy_array_append(arr, &test_gal, &test_params);
        TEST_ASSERT(index == i, "Galaxy index should match iteration");
        
        if (i % 100 == 99) {
            printf("  Added %d galaxies, count = %d\n", i + 1, galaxy_array_get_count(arr));
        }
    }
    
    TEST_ASSERT(galaxy_array_get_count(arr) == STRESS_COUNT, "Final count should match expected");
    
    // Verify all galaxies are still accessible and correct
    printf("Verifying all %d galaxies...\n", STRESS_COUNT);
    for (int i = 0; i < STRESS_COUNT; i++) {
        struct GALAXY* gal = galaxy_array_get(arr, i);
        TEST_ASSERT(gal != NULL, "Galaxy should be accessible");

        // CRITICAL VERIFICATION: Check that the properties pointer is valid and data is intact.
        TEST_ASSERT(gal->properties != NULL, "Properties pointer must not be NULL after expansion");
        TEST_ASSERT(fabs(GALAXY_PROP_Mvir(gal) - (i * 1.5f)) < 1e-5, "Properties data must be preserved");

        TEST_ASSERT(gal->GalaxyNr == i, "Galaxy number should be preserved");
        TEST_ASSERT(gal->GalaxyIndex == (uint64_t)(1000 + i), "Galaxy index should be preserved");
        
        if (i % 200 == 199) {
            printf("  Verified %d galaxies\n", i + 1);
        }
    }
    
    printf("Stress test completed successfully!\n");
    galaxy_array_free(&arr);
}

/**
 * @brief Test error handling
 */
static void test_galaxy_array_error_handling() {
    printf("\n=== Testing GalaxyArray error handling ===\n");
    
    // Test NULL array handling
    TEST_ASSERT(galaxy_array_get_count(NULL) == 0, "Count of NULL array should be 0");
    TEST_ASSERT(galaxy_array_get(NULL, 0) == NULL, "Get from NULL array should return NULL");
    TEST_ASSERT(galaxy_array_get_raw_data(NULL) == NULL, "Raw data from NULL array should return NULL");
    
    // Test free of NULL array (should not crash)
    galaxy_array_free(NULL);
    printf("galaxy_array_free(NULL) handled gracefully\n");
    
    // Test append with NULL parameters
    GalaxyArray* arr = galaxy_array_new();
    TEST_ASSERT(arr != NULL, "GalaxyArray creation should succeed");
    
    struct GALAXY test_gal;
    create_simple_test_galaxy(&test_gal, 1);
    
    // Test append with NULL galaxy
    int result1 = galaxy_array_append(arr, NULL, &test_params);
    TEST_ASSERT(result1 == -1, "Append with NULL galaxy should fail");
    
    // Test append with NULL params
    int result2 = galaxy_array_append(arr, &test_gal, NULL);
    TEST_ASSERT(result2 == -1, "Append with NULL params should fail");
    
    // Test append with NULL array
    int result3 = galaxy_array_append(NULL, &test_gal, &test_params);
    TEST_ASSERT(result3 == -1, "Append to NULL array should fail");
    
    galaxy_array_free(&arr);
}

int setup_tests() {
    // Initialize basic parameters needed for property system
    test_params.cosmology.Omega = 0.3;
    test_params.cosmology.OmegaLambda = 0.7;
    test_params.cosmology.Hubble_h = 0.7;
    test_params.simulation.NumSnapOutputs = 64;  // Required for dynamic arrays
    
    return initialize_property_system(&test_params);
}

int main(void) {
    printf("\n========================================\n");
    printf("SAGE GalaxyArray Component Unit Tests\n");
    printf("========================================\n");
    printf("Testing the new GalaxyArray implementation\n");
    printf("that provides safe dynamic galaxy arrays.\n");
    printf("========================================\n");

    // Initialize systems required for the test
    if (setup_tests() != 0) {
        printf("CRITICAL FAIL: Could not initialize property system for tests.\n");
        return 1;
    }

    // Run tests
    test_galaxy_array_basic_operations();
    test_galaxy_array_append();
    test_galaxy_array_expansion();
    test_galaxy_array_error_handling();
    
    // Report results
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");
    
    if (tests_passed == tests_run) {
        printf("🎉 ALL TESTS PASSED! 🎉\n");
        printf("GalaxyArray component is working correctly.\n");
        return 0;
    } else {
        printf("❌ SOME TESTS FAILED! ❌\n");
        printf("GalaxyArray component has issues that need fixing.\n");
        return 1;
    }
}
