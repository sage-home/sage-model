/**
 * @file test_galaxy_array.c
 * @brief Unit tests for safe galaxy array expansion functionality
 * 
 * This test verifies that galaxy arrays handle memory reallocation properly
 * while preserving the integrity of properties pointers, preventing the
 * segmentation faults caused by dangling pointers after realloc operations.
 * 
 * This is a CRITICAL test for memory safety - it verifies that the
 * save-realloc-restore pattern works correctly under realistic conditions.
 * 
 * NOTE: This test suite has been updated to reflect the current architecture
 * where only the safe galaxy_array_expand function exists. The dangerous
 * unsafe version has been removed from the codebase (which is a good thing!).
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "../src/core/core_allvars.h"
#include "../src/core/core_array_utils.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_mymalloc.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions - only shows failures
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

// Test parameters structure - minimal required for properties allocation
// Initialize basic test params for property allocation
static struct params test_params = {
    .simulation = {
        .NumSnapOutputs = 10,  // Required for StarFormationHistory dynamic array
        .SimMaxSnaps = 64,     // Required parameter
        .LastSnapshotNr = 63   // Required parameter
    }
};

/**
 * @brief Create a test galaxy with REAL initialized properties
 * This function creates a galaxy exactly as the real SAGE code does,
 * ensuring our test accurately reflects real-world usage.
 */
// Note: Dual property system has been eliminated. Property system is single source of truth.

static int create_test_galaxy(struct GALAXY* gal, int galaxy_id) {
    // 1. Zero-initialize entire struct to eliminate padding corruption
    memset(gal, 0, sizeof(struct GALAXY));
    
    // 2. Allocate properties first
    if (allocate_galaxy_properties(gal, &test_params) != 0) {
        printf("ERROR: Failed to allocate galaxy properties for galaxy %d\n", galaxy_id);
        return -1;
    }
    
    // 3. Set all values using property macros
    GALAXY_PROP_GalaxyNr(gal) = galaxy_id;
    
    // Continue with allocation check
    if (false) {
        printf("ERROR: Failed to allocate galaxy properties for galaxy %d\n", galaxy_id);
        return -1;
    }
    
    // Verify properties were allocated correctly
    if (gal->properties == NULL) {
        printf("ERROR: Galaxy properties pointer is NULL after allocation for galaxy %d\n", galaxy_id);
        return -1;
    }
    
    // 4. Set values through PROPERTIES (authoritative during initialization)
    GALAXY_PROP_Type(gal) = galaxy_id % 3;  // Mix of central (0), satellite (1), orphan (2)
    GALAXY_PROP_SnapNum(gal) = 63;
    GALAXY_PROP_Mvir(gal) = (1.0f + galaxy_id * 0.01f) * 1e10f;  // Varying virial mass with precise float arithmetic
    GALAXY_PROP_Vmax(gal) = 200.0 + galaxy_id * 10.0;  // Varying Vmax
    GALAXY_PROP_Rvir(gal) = 100.0 + galaxy_id * 5.0;  // Varying virial radius
    GALAXY_PROP_GalaxyIndex(gal) = (uint64_t)(1000 + galaxy_id);  // Unique identifier
    
    // Set position arrays through properties
    for (int j = 0; j < 3; j++) {
        GALAXY_PROP_Pos_ELEM(gal, j) = galaxy_id * (10.0 + j * 10.0);
    }
    
    // 5. SINGLE one-way sync from properties to direct fields
    // This ensures direct fields match properties for performance-critical runtime access
    // Property system is now the single source of truth - no sync needed
    
    return 0;
}

/**
 * @brief Test safe galaxy array expansion
 * This is the CRITICAL test that verifies the fix for the segmentation fault.
 * NOTE: Only the safe version exists now - the unsafe version was removed.
 */
static void test_safe_galaxy_array_expansion() {
    printf("\n=== Testing safe galaxy array expansion ===\n");
    
    const int INITIAL_CAPACITY = 5;
    const int NUM_GALAXIES = 3;  // Start with fewer than capacity
    const int EXPAND_TO = 20;    // Force reallocation
    
    // Create galaxy array
    struct GALAXY* galaxies = mymalloc(INITIAL_CAPACITY * sizeof(struct GALAXY));
    int capacity = INITIAL_CAPACITY;
    
    printf("Testing initial array allocation...\n");
    TEST_ASSERT(galaxies != NULL, "Initial galaxy array allocation should succeed");
    
    // Create galaxies with real properties
    printf("Creating %d test galaxies with properties...\n", NUM_GALAXIES);
    for (int i = 0; i < NUM_GALAXIES; i++) {
        if (create_test_galaxy(&galaxies[i], i) != 0) {
            printf("FAIL: Failed to create test galaxy %d\n", i);
            return;
        }
    }
    
    // Store original properties pointers for verification
    galaxy_properties_t* original_props[NUM_GALAXIES];
    for (int i = 0; i < NUM_GALAXIES; i++) {
        original_props[i] = galaxies[i].properties;
    }
    
    // Test SAFE expansion
    printf("Testing safe array expansion...\n");
    struct GALAXY* old_array = galaxies;
    int result = galaxy_array_expand(&galaxies, &capacity, EXPAND_TO, NUM_GALAXIES);
    
    TEST_ASSERT(result == 0, "Safe expansion should return 0 on success");
    TEST_ASSERT(capacity >= EXPAND_TO, "Capacity should be expanded to requested size");
    
    if (old_array != galaxies) {
        printf("Array reallocated - verifying properties preservation...\n");
        // CRITICAL: Verify properties pointers are preserved and valid
        for (int i = 0; i < NUM_GALAXIES; i++) {
            TEST_ASSERT(galaxies[i].properties != NULL, "Properties pointer should not be NULL after safe expansion");
            TEST_ASSERT(galaxies[i].properties == original_props[i], "Properties pointer should be preserved after safe expansion");
        }
        
        // Verify galaxy data integrity
        printf("Verifying galaxy data integrity after reallocation...\n");
        for (int i = 0; i < NUM_GALAXIES; i++) {
            TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxies[i]) == i, "Galaxy number should be preserved");
            TEST_ASSERT(GALAXY_PROP_Type(&galaxies[i]) == i % 3, "Galaxy type should be preserved");
            
            // Property system is now the single source of truth
            TEST_ASSERT(GALAXY_PROP_GalaxyIndex(&galaxies[i]) == (uint64_t)(1000 + i), "Galaxy index should be preserved");
        }
    } else {
        printf("No reallocation needed - testing larger expansion to force reallocation...\n");
        // Try a much larger expansion to force reallocation
        result = galaxy_array_expand(&galaxies, &capacity, EXPAND_TO * 4, NUM_GALAXIES);
        TEST_ASSERT(result == 0, "Large safe expansion should return 0 on success");
    }
    
    printf("Safe galaxy array expansion test completed.\n");
    
    // Clean up
    for (int i = 0; i < NUM_GALAXIES; i++) {
        free_galaxy_properties(&galaxies[i]);
    }
    myfree(galaxies);
}

/**
 * @brief Test massive reallocation stress test
 * This simulates the real-world scenario that causes segmentation faults
 */
static void test_massive_reallocation_stress() {
    printf("\n=== Testing massive reallocation stress (real-world scenario) ===\n");
    
    const int INITIAL_CAPACITY = 10;
    const int STRESS_GALAXIES = 500;  // Large enough to force many reallocations
    
    struct GALAXY* galaxies = mymalloc(INITIAL_CAPACITY * sizeof(struct GALAXY));
    int capacity = INITIAL_CAPACITY;
    int num_galaxies = 0;
    
    printf("Testing massive reallocation with %d galaxies...\n", STRESS_GALAXIES);
    TEST_ASSERT(galaxies != NULL, "Initial galaxy array allocation should succeed");
    
    // Store properties pointers for verification
    galaxy_properties_t** all_props = mymalloc(STRESS_GALAXIES * sizeof(galaxy_properties_t*));
    
    for (int i = 0; i < STRESS_GALAXIES; i++) {
        // Create new galaxy if needed
        if (num_galaxies >= capacity - 1) {
            // Use SAFE expansion
            int result = galaxy_array_expand(&galaxies, &capacity, num_galaxies + 10, num_galaxies);
            TEST_ASSERT(result == 0, "Safe array expansion should succeed");
            
            // Verify ALL existing properties pointers are still valid
            for (int j = 0; j < num_galaxies; j++) {
                TEST_ASSERT(galaxies[j].properties != NULL, "Properties should not be NULL after expansion");
                TEST_ASSERT(galaxies[j].properties == all_props[j], "Properties pointer should be preserved");
            }
        }
        
        // Create new galaxy
        if (create_test_galaxy(&galaxies[num_galaxies], i) != 0) {
            printf("FAIL: Failed to create galaxy %d\n", i);
            return;
        }
        
        // Store properties pointer for verification
        all_props[num_galaxies] = galaxies[num_galaxies].properties;
        num_galaxies++;
        
        // Progress indicator
        if (i % 100 == 99) {
            printf("  Progress: %d/%d galaxies created and verified\n", i + 1, STRESS_GALAXIES);
        }
    }
    
    // Final comprehensive verification
    printf("Final verification of all %d galaxies...\n", num_galaxies);
    for (int i = 0; i < num_galaxies; i++) {
        TEST_ASSERT(galaxies[i].properties != NULL, "Final check: properties should not be NULL");
        TEST_ASSERT(galaxies[i].properties == all_props[i], "Final check: properties pointer should be unchanged");
        TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxies[i]) == i, "Final check: galaxy data should be intact");
    }
    
    printf("Massive reallocation stress test completed.\n");
    
    // Clean up
    for (int i = 0; i < num_galaxies; i++) {
        free_galaxy_properties(&galaxies[i]);
    }
    myfree(all_props);
    myfree(galaxies);
}

/**
 * @brief Test properties preservation through multiple reallocations
 * This test verifies that properties remain valid through several reallocation cycles.
 */
static void test_properties_preservation() {
    printf("\n=== Testing properties preservation through multiple reallocations ===\n");
    
    const int INITIAL_CAPACITY = 3;
    const int NUM_GALAXIES = 2;
    
    struct GALAXY* galaxies = mymalloc(INITIAL_CAPACITY * sizeof(struct GALAXY));
    int capacity = INITIAL_CAPACITY;
    
    printf("Testing properties preservation through multiple reallocations...\n");
    // Create galaxies with properties
    for (int i = 0; i < NUM_GALAXIES; i++) {
        create_test_galaxy(&galaxies[i], i);
    }
    
    // Store original properties for verification
    galaxy_properties_t* original_props[NUM_GALAXIES];
    for (int i = 0; i < NUM_GALAXIES; i++) {
        original_props[i] = galaxies[i].properties;
    }
    
    // Perform multiple reallocations to stress test properties preservation
    int expansion_sizes[] = {12, 50, 100, 200};
    int num_tests = sizeof(expansion_sizes) / sizeof(expansion_sizes[0]);
    
    for (int test = 0; test < num_tests; test++) {
        int result = galaxy_array_expand(&galaxies, &capacity, expansion_sizes[test], NUM_GALAXIES);
        
        TEST_ASSERT(result == 0, "Safe expansion should return 0 on success");
        TEST_ASSERT(capacity >= expansion_sizes[test], "Capacity should meet expansion requirement");
        
        // Verify properties are preserved
        for (int i = 0; i < NUM_GALAXIES; i++) {
            TEST_ASSERT(galaxies[i].properties != NULL, "Properties should not be NULL after expansion");
            TEST_ASSERT(galaxies[i].properties == original_props[i], "Properties pointer should be preserved");
            
            // Verify galaxy data integrity
            TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxies[i]) == i, "Galaxy number should be preserved");
            TEST_ASSERT(GALAXY_PROP_Type(&galaxies[i]) == i % 3, "Galaxy type should be preserved");
        }
    }
    
    printf("Properties preservation test completed.\n");
    
    // Clean up
    for (int i = 0; i < NUM_GALAXIES; i++) {
        free_galaxy_properties(&galaxies[i]);
    }
    myfree(galaxies);
}

int main(void) {
    printf("\n========================================\n");
    printf("SAGE Galaxy Array Safety Unit Tests\n");
    printf("========================================\n");
    printf("These tests verify the fix for segmentation\n");
    printf("faults caused by properties pointer corruption\n");
    printf("during galaxy array reallocation.\n");
    printf("========================================\n");

    // Run the critical tests
    test_safe_galaxy_array_expansion();
    test_massive_reallocation_stress();
    test_properties_preservation();
    
    // Report results
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");
    
    if (tests_passed == tests_run) {
        printf("🎉 ALL TESTS PASSED! 🎉\n");
        printf("Galaxy array safety features are working correctly.\n");
        return 0;
    } else {
        printf("❌ SOME TESTS FAILED! ❌\n");
        printf("There are issues with galaxy array safety that must be fixed.\n");
        return 1;
    }
}