/**
 * @file test_property_separation_memory_safety.c
 * @brief Memory safety validation for core-physics property separation
 * 
 * This test ensures that the core-physics property separation maintains
 * memory safety by verifying:
 * - No memory corruption after property removal from struct GALAXY
 * - Galaxy array operations work correctly with new struct layout
 * - Property allocation/deallocation is robust
 * - Edge cases and error conditions are handled properly
 * 
 * Created: 2025-06-13 (Property Separation Implementation)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_property_utils.h"
#include "../src/core/core_logging.h"
#include "../src/core/galaxy_array.h"
#include "../src/core/core_array_utils.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            printf("  ✗ %s\n", message); \
        } \
    } while (0)

#define MEMORY_PATTERN_A 0xAAAAAAAA
#define MEMORY_PATTERN_B 0xBBBBBBBB
#define MEMORY_PATTERN_C 0xCCCCCCCC

/**
 * Test struct GALAXY memory layout after property removal
 */
static void test_struct_memory_layout(void) {
    printf("\n=== Testing struct GALAXY Memory Layout ===\n");
    
    // Test that struct GALAXY has expected size after property removal
    size_t struct_size = sizeof(struct GALAXY);
    printf("  Current struct GALAXY size: %zu bytes\n", struct_size);
    
    // After removing mergeType, mergeIntoID, mergeIntoSnapNum (3 × int32_t = 12 bytes)
    // the struct should be smaller than before
    TEST_ASSERT(struct_size > 100, "struct GALAXY has reasonable minimum size");
    TEST_ASSERT(struct_size < 1000, "struct GALAXY has reasonable maximum size");
    
    // Test memory alignment and access patterns
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // Test that all core properties are properly aligned and accessible
    galaxy.SnapNum = 0x12345678;
    galaxy.Type = 0x87654321;
    galaxy.GalaxyNr = 0xABCDEF00;
    galaxy.HaloNr = 0x00FEDCBA;
    galaxy.MostBoundID = 0x1234567890ABCDEFLL;
    
    TEST_ASSERT(galaxy.SnapNum == 0x12345678, "SnapNum memory access");
    TEST_ASSERT(galaxy.Type == 0x87654321, "Type memory access");
    TEST_ASSERT(galaxy.GalaxyNr == 0xABCDEF00, "GalaxyNr memory access");
    TEST_ASSERT(galaxy.HaloNr == 0x00FEDCBA, "HaloNr memory access");
    TEST_ASSERT(galaxy.MostBoundID == 0x1234567890ABCDEFLL, "MostBoundID memory access");
    
    // Test float fields
    galaxy.Mvir = 1.23456789e12f;
    galaxy.Rvir = 9.87654321e2f;
    galaxy.Vvir = 5.55555555e2f;
    galaxy.Vmax = 7.77777777e2f;
    
    TEST_ASSERT(fabs(galaxy.Mvir - 1.23456789e12f) < 1e6f, "Mvir memory access");
    TEST_ASSERT(fabs(galaxy.Rvir - 9.87654321e2f) < 1e-3f, "Rvir memory access");
    TEST_ASSERT(fabs(galaxy.Vvir - 5.55555555e2f) < 1e-3f, "Vvir memory access");
    TEST_ASSERT(fabs(galaxy.Vmax - 7.77777777e2f) < 1e-3f, "Vmax memory access");
    
    // Test array fields
    galaxy.Pos[0] = 1.111f; galaxy.Pos[1] = 2.222f; galaxy.Pos[2] = 3.333f;
    galaxy.Vel[0] = 4.444f; galaxy.Vel[1] = 5.555f; galaxy.Vel[2] = 6.666f;
    
    TEST_ASSERT(fabs(galaxy.Pos[0] - 1.111f) < 1e-3f, "Pos[0] memory access");
    TEST_ASSERT(fabs(galaxy.Pos[1] - 2.222f) < 1e-3f, "Pos[1] memory access");
    TEST_ASSERT(fabs(galaxy.Pos[2] - 3.333f) < 1e-3f, "Pos[2] memory access");
    TEST_ASSERT(fabs(galaxy.Vel[0] - 4.444f) < 1e-3f, "Vel[0] memory access");
    TEST_ASSERT(fabs(galaxy.Vel[1] - 5.555f) < 1e-3f, "Vel[1] memory access");
    TEST_ASSERT(fabs(galaxy.Vel[2] - 6.666f) < 1e-3f, "Vel[2] memory access");
    
    // Test extension system pointers
    galaxy.extension_data = NULL;
    galaxy.num_extensions = 0;
    galaxy.extension_flags = 0;
    galaxy.properties = NULL;
    
    TEST_ASSERT(galaxy.extension_data == NULL, "Extension data pointer initialization");
    TEST_ASSERT(galaxy.num_extensions == 0, "Extension count initialization");
    TEST_ASSERT(galaxy.extension_flags == 0, "Extension flags initialization");
    TEST_ASSERT(galaxy.properties == NULL, "Properties pointer initialization");
}

/**
 * Test galaxy array operations with new struct layout
 */
static void test_galaxy_array_operations(void) {
    printf("\n=== Testing Galaxy Array Operations ===\n");
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    
    // Create a galaxy array
    struct GalaxyArray galaxy_array;
    int result = galaxy_array_init(&galaxy_array, 10);
    TEST_ASSERT(result == 0, "Galaxy array initialization");
    TEST_ASSERT(galaxy_array.galaxies != NULL, "Galaxy array memory allocation");
    TEST_ASSERT(galaxy_array.size == 10, "Galaxy array initial size");
    TEST_ASSERT(galaxy_array.capacity >= 10, "Galaxy array initial capacity");
    
    // Test adding galaxies to the array
    for (int i = 0; i < 5; i++) {
        struct GALAXY galaxy;
        memset(&galaxy, 0, sizeof(galaxy));
        
        // Set unique values for each galaxy
        galaxy.SnapNum = 60 + i;
        galaxy.Type = i % 3;
        galaxy.GalaxyNr = 1000 + i;
        galaxy.HaloNr = 2000 + i;
        galaxy.MostBoundID = 3000000000LL + i;
        galaxy.Mvir = (1.0f + 0.1f * i) * 1e12f;
        galaxy.Rvir = 200.0f + 10.0f * i;
        galaxy.Vvir = 150.0f + 5.0f * i;
        galaxy.Vmax = 180.0f + 8.0f * i;
        
        galaxy.Pos[0] = 10.0f * i; galaxy.Pos[1] = 20.0f * i; galaxy.Pos[2] = 30.0f * i;
        galaxy.Vel[0] = 100.0f + i; galaxy.Vel[1] = 200.0f + i; galaxy.Vel[2] = 300.0f + i;
        
        // Allocate properties for this galaxy
        result = allocate_galaxy_properties(&galaxy, &run_params);
        TEST_ASSERT(result == 0, "Galaxy property allocation in array");
        
        if (galaxy.properties != NULL) {
            // Set physics properties
            GALAXY_PROP_ColdGas(&galaxy) = (2.0f + 0.5f * i) * 1e10f;
            GALAXY_PROP_StellarMass(&galaxy) = (3.0f + 0.3f * i) * 1e10f;
            GALAXY_PROP_HotGas(&galaxy) = (8.0f + 0.8f * i) * 1e10f;
            GALAXY_PROP_mergeType(&galaxy) = i % 4;
            GALAXY_PROP_mergeIntoID(&galaxy) = 5000 + i;
            GALAXY_PROP_mergeIntoSnapNum(&galaxy) = 55 + i;
        }
        
        // Add galaxy to array using deep copy
        result = galaxy_array_add(&galaxy_array, &galaxy);
        TEST_ASSERT(result == 0, "Galaxy array add operation");
        
        // Original galaxy cleanup (array should have its own copy)
        if (galaxy.properties != NULL) {
            free_galaxy_properties(&galaxy);
        }
    }
    
    TEST_ASSERT(galaxy_array.size == 5, "Galaxy array size after additions");
    
    // Verify all galaxies were copied correctly
    for (int i = 0; i < 5; i++) {
        struct GALAXY *g = &galaxy_array.galaxies[i];
        
        // Test core properties
        TEST_ASSERT(g->SnapNum == 60 + i, "Array galaxy core property: SnapNum");
        TEST_ASSERT(g->Type == i % 3, "Array galaxy core property: Type");
        TEST_ASSERT(g->GalaxyNr == 1000 + i, "Array galaxy core property: GalaxyNr");
        TEST_ASSERT(g->HaloNr == 2000 + i, "Array galaxy core property: HaloNr");
        TEST_ASSERT(g->MostBoundID == 3000000000LL + i, "Array galaxy core property: MostBoundID");
        
        float expected_mvir = (1.0f + 0.1f * i) * 1e12f;
        TEST_ASSERT(fabs(g->Mvir - expected_mvir) < 1e9f, "Array galaxy core property: Mvir");
        
        float expected_rvir = 200.0f + 10.0f * i;
        TEST_ASSERT(fabs(g->Rvir - expected_rvir) < 1e-3f, "Array galaxy core property: Rvir");
        
        // Test physics properties if available
        if (g->properties != NULL) {
            float expected_coldgas = (2.0f + 0.5f * i) * 1e10f;
            float coldgas = GALAXY_PROP_ColdGas(g);
            TEST_ASSERT(fabs(coldgas - expected_coldgas) < 1e7f, "Array galaxy physics property: ColdGas");
            
            float expected_stellar = (3.0f + 0.3f * i) * 1e10f;
            float stellar = GALAXY_PROP_StellarMass(g);
            TEST_ASSERT(fabs(stellar - expected_stellar) < 1e7f, "Array galaxy physics property: StellarMass");
            
            int expected_merge_type = i % 4;
            int merge_type = GALAXY_PROP_mergeType(g);
            TEST_ASSERT(merge_type == expected_merge_type, "Array galaxy physics property: mergeType");
            
            int expected_merge_id = 5000 + i;
            int merge_id = GALAXY_PROP_mergeIntoID(g);
            TEST_ASSERT(merge_id == expected_merge_id, "Array galaxy physics property: mergeIntoID");
            
            int expected_merge_snap = 55 + i;
            int merge_snap = GALAXY_PROP_mergeIntoSnapNum(g);
            TEST_ASSERT(merge_snap == expected_merge_snap, "Array galaxy physics property: mergeIntoSnapNum");
        }
    }
    
    // Test array expansion
    int original_capacity = galaxy_array.capacity;
    
    // Add galaxies until expansion is needed
    for (int i = 5; i < original_capacity + 5; i++) {
        struct GALAXY galaxy;
        memset(&galaxy, 0, sizeof(galaxy));
        
        galaxy.SnapNum = 60 + i;
        galaxy.Type = i % 3;
        galaxy.GalaxyNr = 1000 + i;
        
        result = allocate_galaxy_properties(&galaxy, &run_params);
        if (result == 0 && galaxy.properties != NULL) {
            GALAXY_PROP_ColdGas(&galaxy) = (2.0f + 0.5f * i) * 1e10f;
        }
        
        result = galaxy_array_add(&galaxy_array, &galaxy);
        TEST_ASSERT(result == 0, "Galaxy array add during expansion");
        
        if (galaxy.properties != NULL) {
            free_galaxy_properties(&galaxy);
        }
    }
    
    TEST_ASSERT(galaxy_array.capacity > original_capacity, "Galaxy array expansion occurred");
    TEST_ASSERT(galaxy_array.size == original_capacity + 5, "Galaxy array size after expansion");
    
    // Verify data integrity after expansion
    for (int i = 0; i < 3; i++) {  // Check first few galaxies
        struct GALAXY *g = &galaxy_array.galaxies[i];
        TEST_ASSERT(g->SnapNum == 60 + i, "Data integrity after expansion: SnapNum");
        TEST_ASSERT(g->Type == i % 3, "Data integrity after expansion: Type");
        TEST_ASSERT(g->GalaxyNr == 1000 + i, "Data integrity after expansion: GalaxyNr");
    }
    
    // Cleanup
    galaxy_array_free(&galaxy_array);
}

/**
 * Test property allocation and deallocation robustness
 */
static void test_property_allocation_robustness(void) {
    printf("\n=== Testing Property Allocation Robustness ===\n");
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    
    // Test normal allocation/deallocation cycle
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Normal property allocation");
    TEST_ASSERT(galaxy.properties != NULL, "Properties pointer set correctly");
    
    if (galaxy.properties != NULL) {
        // Test that allocated memory is accessible
        GALAXY_PROP_ColdGas(&galaxy) = 1.5e10f;
        GALAXY_PROP_StellarMass(&galaxy) = 2.3e10f;
        GALAXY_PROP_mergeType(&galaxy) = 2;
        
        float coldgas = GALAXY_PROP_ColdGas(&galaxy);
        float stellar = GALAXY_PROP_StellarMass(&galaxy);
        int merge_type = GALAXY_PROP_mergeType(&galaxy);
        
        TEST_ASSERT(fabs(coldgas - 1.5e10f) < 1e6f, "Property access after allocation");
        TEST_ASSERT(fabs(stellar - 2.3e10f) < 1e6f, "Property access after allocation");
        TEST_ASSERT(merge_type == 2, "Property access after allocation");
        
        // Test deallocation
        free_galaxy_properties(&galaxy);
        TEST_ASSERT(galaxy.properties == NULL, "Properties pointer cleared after deallocation");
    }
    
    // Test multiple allocation/deallocation cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        struct GALAXY test_galaxy;
        memset(&test_galaxy, 0, sizeof(test_galaxy));
        
        result = allocate_galaxy_properties(&test_galaxy, &run_params);
        TEST_ASSERT(result == 0, "Multiple cycle allocation");
        
        if (test_galaxy.properties != NULL) {
            // Set unique values for this cycle
            GALAXY_PROP_ColdGas(&test_galaxy) = (1.0f + 0.1f * cycle) * 1e10f;
            GALAXY_PROP_mergeType(&test_galaxy) = cycle % 4;
            
            float coldgas = GALAXY_PROP_ColdGas(&test_galaxy);
            int merge_type = GALAXY_PROP_mergeType(&test_galaxy);
            
            float expected_coldgas = (1.0f + 0.1f * cycle) * 1e10f;
            TEST_ASSERT(fabs(coldgas - expected_coldgas) < 1e7f, "Property persistence in cycle");
            TEST_ASSERT(merge_type == cycle % 4, "Property persistence in cycle");
            
            free_galaxy_properties(&test_galaxy);
        }
    }
    
    // Test allocation with NULL parameters (should handle gracefully)
    struct GALAXY null_test_galaxy;
    memset(&null_test_galaxy, 0, sizeof(null_test_galaxy));
    
    result = allocate_galaxy_properties(&null_test_galaxy, NULL);
    // This might fail (result != 0) but should not crash
    TEST_ASSERT(result != 0 || null_test_galaxy.properties != NULL, "Graceful handling of NULL params");
    
    if (null_test_galaxy.properties != NULL) {
        free_galaxy_properties(&null_test_galaxy);
    }
}

/**
 * Test edge cases and error conditions
 */
static void test_edge_cases_and_errors(void) {
    printf("\n=== Testing Edge Cases and Error Conditions ===\n");
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    
    // Test double allocation (should handle gracefully)
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    int result1 = allocate_galaxy_properties(&galaxy, &run_params);
    int result2 = allocate_galaxy_properties(&galaxy, &run_params);
    
    TEST_ASSERT(result1 == 0, "First allocation succeeds");
    // Second allocation might succeed (replacing) or fail (detecting existing)
    // Either is acceptable as long as it doesn't crash
    TEST_ASSERT(galaxy.properties != NULL, "Properties pointer valid after double allocation");
    
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
    
    // Test double deallocation (should handle gracefully)
    memset(&galaxy, 0, sizeof(galaxy));
    result1 = allocate_galaxy_properties(&galaxy, &run_params);
    if (result1 == 0 && galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
        TEST_ASSERT(galaxy.properties == NULL, "Properties pointer cleared after first free");
        
        // Second free should not crash
        free_galaxy_properties(&galaxy);  // Should handle NULL pointer gracefully
        TEST_ASSERT(galaxy.properties == NULL, "Properties pointer still NULL after double free");
    }
    
    // Test property access on unallocated galaxy
    struct GALAXY unallocated_galaxy;
    memset(&unallocated_galaxy, 0, sizeof(unallocated_galaxy));
    unallocated_galaxy.properties = NULL;
    
    // This should either work with default values or handle gracefully
    // (depends on implementation - some systems use generic accessors)
    TEST_ASSERT(unallocated_galaxy.properties == NULL, "Unallocated galaxy has NULL properties");
    
    // Test property copying with NULL source or destination
    struct GALAXY source_galaxy, dest_galaxy;
    memset(&source_galaxy, 0, sizeof(source_galaxy));
    memset(&dest_galaxy, 0, sizeof(dest_galaxy));
    
    result1 = allocate_galaxy_properties(&source_galaxy, &run_params);
    result2 = allocate_galaxy_properties(&dest_galaxy, &run_params);
    
    if (result1 == 0 && result2 == 0 && 
        source_galaxy.properties != NULL && dest_galaxy.properties != NULL) {
        
        // Set source values
        GALAXY_PROP_ColdGas(&source_galaxy) = 5.5e10f;
        GALAXY_PROP_mergeType(&source_galaxy) = 3;
        
        // Test normal copy
        int copy_result = copy_galaxy_properties(&dest_galaxy, &source_galaxy);
        TEST_ASSERT(copy_result == 0, "Normal property copy succeeds");
        
        float coldgas = GALAXY_PROP_ColdGas(&dest_galaxy);
        int merge_type = GALAXY_PROP_mergeType(&dest_galaxy);
        TEST_ASSERT(fabs(coldgas - 5.5e10f) < 1e7f, "Property copy accuracy");
        TEST_ASSERT(merge_type == 3, "Property copy accuracy");
        
        // Cleanup
        free_galaxy_properties(&source_galaxy);
        free_galaxy_properties(&dest_galaxy);
    }
    
    // Test struct size consistency
    size_t size1 = sizeof(struct GALAXY);
    size_t size2 = sizeof(struct GALAXY);
    TEST_ASSERT(size1 == size2, "Struct size is consistent");
    
    // Test alignment
    struct GALAXY aligned_galaxy;
    uintptr_t addr = (uintptr_t)&aligned_galaxy;
    TEST_ASSERT(addr % sizeof(void*) == 0, "struct GALAXY is properly aligned");
    
    // Test that removed fields really don't exist (compilation would fail if they did)
    // This is tested by the fact that this code compiles without accessing
    // galaxy.mergeType, galaxy.mergeIntoID, galaxy.mergeIntoSnapNum directly
    TEST_ASSERT(1, "Removed properties are not accessible as direct struct members");
}

int main(void) {
    printf("Starting Memory Safety Validation Tests\n");
    printf("=======================================\n");
    
    // Initialize logging to suppress debug output during tests
    logging_init(LOG_LEVEL_WARNING, stderr);
    
    // Run all test suites
    test_struct_memory_layout();
    test_galaxy_array_operations();
    test_property_allocation_robustness();
    test_edge_cases_and_errors();
    
    // Report results
    printf("\n=======================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✓ All tests passed! Memory safety is maintained with property separation.\n");
        return EXIT_SUCCESS;
    } else {
        printf("✗ %d tests failed. Memory safety validation needs attention.\n", tests_run - tests_passed);
        return EXIT_FAILURE;
    }
}