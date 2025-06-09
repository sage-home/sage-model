/**
 * @file test_galaxy_array.c
 * @brief Unit tests for safe galaxy array expansion functionality
 * 
 * This test verifies that galaxy arrays handle memory reallocation properly
 * while preserving the integrity of properties pointers, preventing the
 * segmentation faults caused by dangling pointers after realloc operations.
 * 
 * This is a CRITICAL test for memory safety - it must verify that the
 * save-realloc-restore pattern works correctly under realistic conditions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../src/core/core_allvars.h"
#include "../src/core/core_array_utils.h"
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

// Test parameters structure - minimal required for properties allocation
// Note: allocate_galaxy_properties doesn't actually use params, so this can be minimal
static struct params test_params = {0};

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
    gal->Mvir = 1e10 + galaxy_id * 1e8;  // Varying virial mass
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
    
    return 0;
}

/**
 * @brief Test safe galaxy array expansion vs unsafe expansion
 * This is the CRITICAL test that verifies the fix for the segmentation fault.
 */
static void test_safe_vs_unsafe_expansion() {
    printf("\n=== Testing safe vs unsafe galaxy array expansion ===\n");
    
    const int INITIAL_CAPACITY = 5;
    const int NUM_GALAXIES = 3;  // Start with fewer than capacity
    const int EXPAND_TO = 10;    // Force reallocation
    
    // Create galaxy array
    struct GALAXY* galaxies = mymalloc(INITIAL_CAPACITY * sizeof(struct GALAXY));
    int capacity = INITIAL_CAPACITY;
    
    TEST_ASSERT(galaxies != NULL, "Initial galaxy array allocation should succeed");
    
    // Create galaxies with real properties
    printf("Creating %d test galaxies with properties...\n", NUM_GALAXIES);
    for (int i = 0; i < NUM_GALAXIES; i++) {
        if (create_test_galaxy(&galaxies[i], i) != 0) {
            printf("FAIL: Failed to create test galaxy %d\n", i);
            return;
        }
        printf("  Galaxy %d: properties at %p\n", i, (void*)galaxies[i].properties);
    }
    
    // Store original properties pointers for verification
    galaxy_properties_t* original_props[NUM_GALAXIES];
    for (int i = 0; i < NUM_GALAXIES; i++) {
        original_props[i] = galaxies[i].properties;
    }
    
    // Test UNSAFE expansion (should detect corruption)
    printf("\nTesting UNSAFE galaxy_array_expand...\n");
    struct GALAXY* old_array = galaxies;
    int result_unsafe = galaxy_array_expand(&galaxies, &capacity, EXPAND_TO);
    
    if (old_array != galaxies) {
        printf("Array was reallocated from %p to %p\n", (void*)old_array, (void*)galaxies);
        TEST_ASSERT(result_unsafe == -2, "Unsafe expansion should return -2 when properties are invalidated");
        
        // Verify that properties pointers are now NULL (unsafe function sets them to NULL)
        for (int i = 0; i < NUM_GALAXIES; i++) {
            if (galaxies[i].properties != NULL) {
                printf("WARNING: Galaxy %d properties not nullified by unsafe function: %p\n", 
                      i, (void*)galaxies[i].properties);
            }
        }
    } else {
        printf("Array was not reallocated - test needs larger expansion\n");
    }
    
    // Restore properties pointers for safe test
    printf("\nRestoring properties for safe test...\n");
    for (int i = 0; i < NUM_GALAXIES; i++) {
        galaxies[i].properties = original_props[i];
        printf("  Galaxy %d: restored properties to %p\n", i, (void*)galaxies[i].properties);
    }
    
    // Reset capacity for safe test
    capacity = INITIAL_CAPACITY;
    
    // Test SAFE expansion
    printf("\nTesting SAFE galaxy_array_expand_safe...\n");
    old_array = galaxies;
    int result_safe = galaxy_array_expand_safe(&galaxies, &capacity, EXPAND_TO * 2, NUM_GALAXIES);
    
    TEST_ASSERT(result_safe == 0, "Safe expansion should return 0 on success");
    TEST_ASSERT(capacity >= EXPAND_TO * 2, "Capacity should be expanded to requested size");
    
    if (old_array != galaxies) {
        printf("Array was safely reallocated from %p to %p\n", (void*)old_array, (void*)galaxies);
        
        // CRITICAL: Verify properties pointers are preserved and valid
        for (int i = 0; i < NUM_GALAXIES; i++) {
            TEST_ASSERT(galaxies[i].properties != NULL, "Properties pointer should not be NULL after safe expansion");
            TEST_ASSERT(galaxies[i].properties == original_props[i], "Properties pointer should be preserved after safe expansion");
            printf("  Galaxy %d: properties preserved at %p\n", i, (void*)galaxies[i].properties);
        }
        
        // Verify galaxy data integrity
        for (int i = 0; i < NUM_GALAXIES; i++) {
            TEST_ASSERT(galaxies[i].GalaxyNr == i, "Galaxy number should be preserved");
            TEST_ASSERT(galaxies[i].Type == i % 3, "Galaxy type should be preserved");
            TEST_ASSERT(galaxies[i].Mvir == 1e10 + i * 1e8, "Galaxy mass should be preserved");
            TEST_ASSERT(galaxies[i].GalaxyIndex == (uint64_t)(1000 + i), "Galaxy index should be preserved");
        }
    } else {
        printf("Array was not reallocated in safe test\n");
    }
    
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
    
    TEST_ASSERT(galaxies != NULL, "Initial galaxy array allocation should succeed");
    
    // Store properties pointers for verification
    galaxy_properties_t** all_props = mymalloc(STRESS_GALAXIES * sizeof(galaxy_properties_t*));
    
    printf("Creating and expanding galaxy array through %d galaxies...\n", STRESS_GALAXIES);
    
    for (int i = 0; i < STRESS_GALAXIES; i++) {
        // Create new galaxy if needed
        if (num_galaxies >= capacity - 1) {
            printf("  Expanding array from capacity %d to accommodate galaxy %d\n", capacity, i);
            
            // Use SAFE expansion
            int result = galaxy_array_expand_safe(&galaxies, &capacity, num_galaxies + 10, num_galaxies);
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
        
        // Periodic verification
        if (i % 100 == 99) {
            printf("  Verified %d galaxies - all properties pointers intact\n", i + 1);
        }
    }
    
    printf("Final verification of all %d galaxies...\n", num_galaxies);
    
    // Final comprehensive verification
    for (int i = 0; i < num_galaxies; i++) {
        TEST_ASSERT(galaxies[i].properties != NULL, "Final check: properties should not be NULL");
        TEST_ASSERT(galaxies[i].properties == all_props[i], "Final check: properties pointer should be unchanged");
        TEST_ASSERT(galaxies[i].GalaxyNr == i, "Final check: galaxy data should be intact");
    }
    
    // Clean up
    for (int i = 0; i < num_galaxies; i++) {
        free_galaxy_properties(&galaxies[i]);
    }
    myfree(all_props);
    myfree(galaxies);
    
    printf("Massive reallocation stress test completed successfully!\n");
}

/**
 * @brief Test corruption detection by unsafe function
 */
static void test_corruption_detection() {
    printf("\n=== Testing corruption detection by unsafe function ===\n");
    
    const int INITIAL_CAPACITY = 3;
    const int NUM_GALAXIES = 2;
    
    struct GALAXY* galaxies = mymalloc(INITIAL_CAPACITY * sizeof(struct GALAXY));
    int capacity = INITIAL_CAPACITY;
    
    // Create galaxies with properties
    for (int i = 0; i < NUM_GALAXIES; i++) {
        create_test_galaxy(&galaxies[i], i);
    }
    
    // Force reallocation with unsafe function
    struct GALAXY* old_array = galaxies;
    int result = galaxy_array_expand(&galaxies, &capacity, INITIAL_CAPACITY * 4);
    
    if (old_array != galaxies) {
        printf("Array was reallocated - unsafe function should detect corruption\n");
        TEST_ASSERT(result == -2, "Unsafe function should return -2 when corruption detected");
        
        // Properties should be set to NULL by unsafe function
        for (int i = 0; i < NUM_GALAXIES; i++) {
            TEST_ASSERT(galaxies[i].properties == NULL, "Unsafe function should nullify properties to prevent crashes");
        }
        printf("Corruption detection working correctly!\n");
    } else {
        printf("Array was not reallocated - increasing expansion size\n");
        result = galaxy_array_expand(&galaxies, &capacity, INITIAL_CAPACITY * 10);
        if (result == -2) {
            printf("Corruption detection triggered on larger expansion\n");
        }
    }
    
    // Note: Don't free properties as they were nullified by unsafe function
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
    test_safe_vs_unsafe_expansion();
    test_massive_reallocation_stress();
    test_corruption_detection();
    
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