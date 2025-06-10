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
        .NumSnapOutputs = 10  // Required for StarFormationHistory dynamic array
    }
};

/**
 * @brief Create a test galaxy with REAL initialized properties
 * This function creates a galaxy exactly as the real SAGE code does,
 * ensuring our test accurately reflects real-world usage.
 */
static int create_test_galaxy(struct GALAXY* gal, int galaxy_id) {
    // Zero-initialize the galaxy struct
    memset(gal, 0, sizeof(struct GALAXY));
    
    // Set core fields that exist in GALAXY struct
    gal->GalaxyNr = galaxy_id;
    gal->Type = galaxy_id % 3;  // Mix of central (0), satellite (1), orphan (2)
    gal->SnapNum = 63;
    gal->Mvir = (1.0f + galaxy_id * 0.01f) * 1e10f;  // Varying virial mass with precise float arithmetic
    gal->Vmax = 200.0 + galaxy_id * 10.0;  // Varying Vmax
    gal->Rvir = 100.0 + galaxy_id * 5.0;  // Varying virial radius
    gal->GalaxyIndex = (uint64_t)(1000 + galaxy_id);  // Unique identifier
    
    // Initialize position with unique values for verification
    gal->Pos[0] = galaxy_id * 10.0;
    gal->Pos[1] = galaxy_id * 20.0;
    gal->Pos[2] = galaxy_id * 30.0;
    
    // CRITICAL: Initialize properties using the real SAGE function
    // This allocates memory for the properties struct with real data
    if (allocate_galaxy_properties(gal, &test_params) != 0) {
        printf("ERROR: Failed to allocate galaxy properties for galaxy %d\n", galaxy_id);
        return -1;
    }
    
    // Verify properties were allocated correctly
    if (gal->properties == NULL) {
        printf("ERROR: Galaxy properties pointer is NULL after allocation for galaxy %d\n", galaxy_id);
        return -1;
    }
    
    // CRITICAL FIX: Sync direct fields to property fields for consistency
    // This ensures both the legacy direct fields and new property system have the same values
    GALAXY_PROP_Type(gal) = gal->Type;
    GALAXY_PROP_SnapNum(gal) = gal->SnapNum;
    GALAXY_PROP_Mvir(gal) = gal->Mvir;  // Sync the mass field!
    GALAXY_PROP_Vmax(gal) = gal->Vmax;
    GALAXY_PROP_Rvir(gal) = gal->Rvir;
    GALAXY_PROP_GalaxyIndex(gal) = gal->GalaxyIndex;
    
    // Set position arrays
    for (int j = 0; j < 3; j++) {
        GALAXY_PROP_Pos_ELEM(gal, j) = gal->Pos[j];
    }
    
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
            TEST_ASSERT(galaxies[i].GalaxyNr == i, "Galaxy number should be preserved");
            TEST_ASSERT(galaxies[i].Type == i % 3, "Galaxy type should be preserved");
            
            // Check mass preservation - this is where the heisenbug manifests
            float expected_mass = (1.0f + i * 0.01f) * 1e10f;
            float actual_mass = galaxies[i].Mvir;
            float prop_mass = GALAXY_PROP_Mvir(&galaxies[i]);
            float diff = fabsf(actual_mass - expected_mass);
            
            // Report any significant mass corruption
            if (diff > 1e-6f) {
                printf("HEISENBUG DEBUG: Galaxy %d mass corruption!\n", i);
                printf("  Expected=%.15e\n", expected_mass);
                printf("  Direct field=%.15e (diff=%.15e)\n", actual_mass, fabsf(actual_mass - expected_mass));
                printf("  Property field=%.15e (diff=%.15e)\n", prop_mass, fabsf(prop_mass - expected_mass));
            }
            
            // HEISENBUG DOCUMENTATION:
            // The next line is commented out because it MASKS a heisenbug in the memory corruption.
            // When this debug output is active, the test passes. When removed, Galaxy 1 fails with ~160.6 byte corruption.
            // This suggests a timing/memory layout issue related to:
            // - Memory alignment between GALAXY struct (168 bytes) and corruption value (~160.6)
            // - Dual field synchronization between galaxies[i].Mvir and galaxies[i].properties->Mvir
            // TODO: Investigate root cause - likely in galaxy_array_expand memcpy operations or property allocation
            // HEISENBUG FIX LINE (uncomment to make test pass):
            // printf("DEBUG: Galaxy %d - expected=%.15e, actual=%.15e, diff=%.15e\n", i, expected_mass, actual_mass, diff);
            
            float assertion_diff = fabsf(galaxies[i].Mvir - ((1.0f + i * 0.01f) * 1e10f));
            TEST_ASSERT(assertion_diff < 1e-4f, "Galaxy mass should be preserved");
            TEST_ASSERT(galaxies[i].GalaxyIndex == (uint64_t)(1000 + i), "Galaxy index should be preserved");
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
        TEST_ASSERT(galaxies[i].GalaxyNr == i, "Final check: galaxy data should be intact");
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
            TEST_ASSERT(galaxies[i].GalaxyNr == i, "Galaxy number should be preserved");
            TEST_ASSERT(galaxies[i].Type == i % 3, "Galaxy type should be preserved");
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