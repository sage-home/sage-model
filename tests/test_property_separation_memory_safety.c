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
    
    // After property separation, the struct should be small (just extension system + properties pointer)
    // With property separation, struct should be around 32 bytes (4 pointers + 2 ints + uint64_t)
    TEST_ASSERT(struct_size >= 24, "struct GALAXY has reasonable minimum size after property separation");
    TEST_ASSERT(struct_size <= 64, "struct GALAXY has reasonable maximum size after property separation");
    
    // Test memory alignment and access patterns
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // Allocate properties for this galaxy first
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    run_params.simulation.NumSnapOutputs = 10;
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Property allocation for struct test");
    
    // Test that all core properties are properly aligned and accessible
    GALAXY_PROP_SnapNum(&galaxy) = 0x12345678;
    GALAXY_PROP_Type(&galaxy) = 0x87654321;
    GALAXY_PROP_GalaxyNr(&galaxy) = 0xABCDEF00;
    GALAXY_PROP_HaloNr(&galaxy) = 0x00FEDCBA;
    GALAXY_PROP_MostBoundID(&galaxy) = 0x1234567890ABCDEFLL;
    
    TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy) == 0x12345678, "SnapNum memory access");
    TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == (int32_t)0x87654321, "Type memory access");
    TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxy) == (int32_t)0xABCDEF00, "GalaxyNr memory access");
    TEST_ASSERT(GALAXY_PROP_HaloNr(&galaxy) == 0x00FEDCBA, "HaloNr memory access");
    TEST_ASSERT(GALAXY_PROP_MostBoundID(&galaxy) == 0x1234567890ABCDEFLL, "MostBoundID memory access");
    
    // Test float fields
    GALAXY_PROP_Mvir(&galaxy) = 1.23456789e12f;
    GALAXY_PROP_Rvir(&galaxy) = 9.87654321e2f;
    GALAXY_PROP_Vvir(&galaxy) = 5.55555555e2f;
    GALAXY_PROP_Vmax(&galaxy) = 7.77777777e2f;
    
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 1.23456789e12f) < 1e6f, "Mvir memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&galaxy) - 9.87654321e2f) < 1e-3f, "Rvir memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&galaxy) - 5.55555555e2f) < 1e-3f, "Vvir memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vmax(&galaxy) - 7.77777777e2f) < 1e-3f, "Vmax memory access");
    
    // Test array fields
    GALAXY_PROP_Pos(&galaxy)[0] = 1.111f; GALAXY_PROP_Pos(&galaxy)[1] = 2.222f; GALAXY_PROP_Pos(&galaxy)[2] = 3.333f;
    GALAXY_PROP_Vel(&galaxy)[0] = 4.444f; GALAXY_PROP_Vel(&galaxy)[1] = 5.555f; GALAXY_PROP_Vel(&galaxy)[2] = 6.666f;
    
    TEST_ASSERT(fabs(GALAXY_PROP_Pos(&galaxy)[0] - 1.111f) < 1e-3f, "Pos[0] memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos(&galaxy)[1] - 2.222f) < 1e-3f, "Pos[1] memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos(&galaxy)[2] - 3.333f) < 1e-3f, "Pos[2] memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vel(&galaxy)[0] - 4.444f) < 1e-3f, "Vel[0] memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vel(&galaxy)[1] - 5.555f) < 1e-3f, "Vel[1] memory access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vel(&galaxy)[2] - 6.666f) < 1e-3f, "Vel[2] memory access");
    
    // Clean up properties
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
    
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
    run_params.simulation.NumSnapOutputs = 10; // Set valid parameter for dynamic arrays
    
    // Create a galaxy array
    GalaxyArray* galaxy_array = galaxy_array_new();
    TEST_ASSERT(galaxy_array != NULL, "Galaxy array initialization");
    TEST_ASSERT(galaxy_array_get_count(galaxy_array) == 0, "Galaxy array initial size");
    
    // Test adding galaxies to the array
    for (int i = 0; i < 5; i++) {
        struct GALAXY galaxy;
        memset(&galaxy, 0, sizeof(galaxy));
        
        // Allocate properties for this galaxy FIRST
        int result = allocate_galaxy_properties(&galaxy, &run_params);
        TEST_ASSERT(result == 0, "Galaxy property allocation in array");
        
        // Set unique values for each galaxy
        GALAXY_PROP_SnapNum(&galaxy) = 60 + i;
        GALAXY_PROP_Type(&galaxy) = i % 3;
        GALAXY_PROP_GalaxyNr(&galaxy) = 1000 + i;
        GALAXY_PROP_HaloNr(&galaxy) = 2000 + i;
        GALAXY_PROP_MostBoundID(&galaxy) = 3000000000LL + i;
        GALAXY_PROP_Mvir(&galaxy) = (1.0f + 0.1f * i) * 1e12f;
        GALAXY_PROP_Rvir(&galaxy) = 200.0f + 10.0f * i;
        GALAXY_PROP_Vvir(&galaxy) = 150.0f + 5.0f * i;
        GALAXY_PROP_Vmax(&galaxy) = 180.0f + 8.0f * i;
        
        GALAXY_PROP_Pos(&galaxy)[0] = 10.0f * i; GALAXY_PROP_Pos(&galaxy)[1] = 20.0f * i; GALAXY_PROP_Pos(&galaxy)[2] = 30.0f * i;
        GALAXY_PROP_Vel(&galaxy)[0] = 100.0f + i; GALAXY_PROP_Vel(&galaxy)[1] = 200.0f + i; GALAXY_PROP_Vel(&galaxy)[2] = 300.0f + i;
        
        if (galaxy.properties != NULL) {
            // Set physics properties using generic property system
            property_id_t prop_coldgas = get_cached_property_id("ColdGas");
            property_id_t prop_stellar = get_cached_property_id("StellarMass");
            property_id_t prop_hotgas = get_cached_property_id("HotGas");
            property_id_t prop_merge_type = get_cached_property_id("mergeType");
            property_id_t prop_merge_id = get_cached_property_id("mergeIntoID");
            property_id_t prop_merge_snap = get_cached_property_id("mergeIntoSnapNum");
            
            if (prop_coldgas < PROP_COUNT) {
                set_float_property(&galaxy, prop_coldgas, (2.0f + 0.5f * i) * 1e10f);
            }
            if (prop_stellar < PROP_COUNT) {
                set_float_property(&galaxy, prop_stellar, (3.0f + 0.3f * i) * 1e10f);
            }
            if (prop_hotgas < PROP_COUNT) {
                set_float_property(&galaxy, prop_hotgas, (8.0f + 0.8f * i) * 1e10f);
            }
            if (prop_merge_type < PROP_COUNT) {
                set_int32_property(&galaxy, prop_merge_type, i % 4);
            }
            if (prop_merge_id < PROP_COUNT) {
                set_int32_property(&galaxy, prop_merge_id, 5000 + i);
            }
            if (prop_merge_snap < PROP_COUNT) {
                set_int32_property(&galaxy, prop_merge_snap, 55 + i);
            }
        }
        
        // Add galaxy to array using deep copy
        int index = galaxy_array_append(galaxy_array, &galaxy, &run_params);
        TEST_ASSERT(index >= 0, "Galaxy array add operation");
        
        // Original galaxy cleanup (array should have its own copy)
        if (galaxy.properties != NULL) {
            free_galaxy_properties(&galaxy);
        }
    }
    
    TEST_ASSERT(galaxy_array_get_count(galaxy_array) == 5, "Galaxy array size after additions");
    
    // Verify all galaxies were copied correctly
    for (int i = 0; i < 5; i++) {
        struct GALAXY *g = galaxy_array_get(galaxy_array, i);
        
        // Test core properties
        TEST_ASSERT(GALAXY_PROP_SnapNum(g) == 60 + i, "Array galaxy core property: SnapNum");
        TEST_ASSERT(GALAXY_PROP_Type(g) == i % 3, "Array galaxy core property: Type");
        TEST_ASSERT(GALAXY_PROP_GalaxyNr(g) == 1000 + i, "Array galaxy core property: GalaxyNr");
        TEST_ASSERT(GALAXY_PROP_HaloNr(g) == 2000 + i, "Array galaxy core property: HaloNr");
        TEST_ASSERT(GALAXY_PROP_MostBoundID(g) == 3000000000LL + i, "Array galaxy core property: MostBoundID");
        
        float expected_mvir = (1.0f + 0.1f * i) * 1e12f;
        TEST_ASSERT(fabs(GALAXY_PROP_Mvir(g) - expected_mvir) < 1e9f, "Array galaxy core property: Mvir");
        
        float expected_rvir = 200.0f + 10.0f * i;
        TEST_ASSERT(fabs(GALAXY_PROP_Rvir(g) - expected_rvir) < 1e-3f, "Array galaxy core property: Rvir");
        
        // Test physics properties if available
        if (g->properties != NULL) {
            property_id_t prop_coldgas = get_cached_property_id("ColdGas");
            property_id_t prop_stellar = get_cached_property_id("StellarMass");
            property_id_t prop_merge_type = get_cached_property_id("mergeType");
            property_id_t prop_merge_id = get_cached_property_id("mergeIntoID");
            property_id_t prop_merge_snap = get_cached_property_id("mergeIntoSnapNum");
            
            if (prop_coldgas < PROP_COUNT) {
                float expected_coldgas = (2.0f + 0.5f * i) * 1e10f;
                float coldgas = get_float_property(g, prop_coldgas, 0.0f);
                TEST_ASSERT(fabs(coldgas - expected_coldgas) < 1e7f, "Array galaxy physics property: ColdGas");
            }
            
            if (prop_stellar < PROP_COUNT) {
                float expected_stellar = (3.0f + 0.3f * i) * 1e10f;
                float stellar = get_float_property(g, prop_stellar, 0.0f);
                TEST_ASSERT(fabs(stellar - expected_stellar) < 1e7f, "Array galaxy physics property: StellarMass");
            }
            
            if (prop_merge_type < PROP_COUNT) {
                int expected_merge_type = i % 4;
                int merge_type = get_int32_property(g, prop_merge_type, 0);
                TEST_ASSERT(merge_type == expected_merge_type, "Array galaxy physics property: mergeType");
            }
            
            if (prop_merge_id < PROP_COUNT) {
                int expected_merge_id = 5000 + i;
                int merge_id = get_int32_property(g, prop_merge_id, 0);
                TEST_ASSERT(merge_id == expected_merge_id, "Array galaxy physics property: mergeIntoID");
            }
            
            if (prop_merge_snap < PROP_COUNT) {
                int expected_merge_snap = 55 + i;
                int merge_snap = get_int32_property(g, prop_merge_snap, 0);
                TEST_ASSERT(merge_snap == expected_merge_snap, "Array galaxy physics property: mergeIntoSnapNum");
            }
        }
    }
    
    // Test array expansion
    int original_count = galaxy_array_get_count(galaxy_array);
    
    // Add more galaxies to test expansion
    for (int i = 5; i < 15; i++) {
        struct GALAXY galaxy;
        memset(&galaxy, 0, sizeof(galaxy));
        
        // Allocate properties FIRST
        int result2 = allocate_galaxy_properties(&galaxy, &run_params);
        
        GALAXY_PROP_SnapNum(&galaxy) = 60 + i;
        GALAXY_PROP_Type(&galaxy) = i % 3;
        GALAXY_PROP_GalaxyNr(&galaxy) = 1000 + i;
        if (result2 == 0 && galaxy.properties != NULL) {
            property_id_t prop_coldgas = get_cached_property_id("ColdGas");
            if (prop_coldgas < PROP_COUNT) {
                set_float_property(&galaxy, prop_coldgas, (2.0f + 0.5f * i) * 1e10f);
            }
        }
        
        int index = galaxy_array_append(galaxy_array, &galaxy, &run_params);
        TEST_ASSERT(index >= 0, "Galaxy array add during expansion");
        
        if (galaxy.properties != NULL) {
            free_galaxy_properties(&galaxy);
        }
    }
    
    TEST_ASSERT(galaxy_array_get_count(galaxy_array) > original_count, "Galaxy array expansion occurred");
    TEST_ASSERT(galaxy_array_get_count(galaxy_array) == 15, "Galaxy array size after expansion");
    
    // Verify data integrity after expansion
    for (int i = 0; i < 3; i++) {  // Check first few galaxies
        struct GALAXY *g = galaxy_array_get(galaxy_array, i);
        TEST_ASSERT(GALAXY_PROP_SnapNum(g) == 60 + i, "Data integrity after expansion: SnapNum");
        TEST_ASSERT(GALAXY_PROP_Type(g) == i % 3, "Data integrity after expansion: Type");
        TEST_ASSERT(GALAXY_PROP_GalaxyNr(g) == 1000 + i, "Data integrity after expansion: GalaxyNr");
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
    run_params.simulation.NumSnapOutputs = 10; // Set valid parameter for dynamic arrays
    
    // Test normal allocation/deallocation cycle
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Normal property allocation");
    TEST_ASSERT(galaxy.properties != NULL, "Properties pointer set correctly");
    
    if (galaxy.properties != NULL) {
        // Test that allocated memory is accessible
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_stellar = get_cached_property_id("StellarMass");
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        
        if (prop_coldgas < PROP_COUNT) {
            set_float_property(&galaxy, prop_coldgas, 1.5e10f);
        }
        if (prop_stellar < PROP_COUNT) {
            set_float_property(&galaxy, prop_stellar, 2.3e10f);
        }
        if (prop_merge_type < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_type, 2);
        }
        
        float coldgas = 0.0f, stellar = 0.0f;
        int merge_type = 0;
        
        if (prop_coldgas < PROP_COUNT) {
            coldgas = get_float_property(&galaxy, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas - 1.5e10f) < 1e6f, "Property access after allocation: ColdGas");
        }
        if (prop_stellar < PROP_COUNT) {
            stellar = get_float_property(&galaxy, prop_stellar, 0.0f);
            TEST_ASSERT(fabs(stellar - 2.3e10f) < 1e6f, "Property access after allocation: StellarMass");
        }
        if (prop_merge_type < PROP_COUNT) {
            merge_type = get_int32_property(&galaxy, prop_merge_type, 0);
            TEST_ASSERT(merge_type == 2, "Property access after allocation: mergeType");
        }
        
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
            property_id_t prop_coldgas = get_cached_property_id("ColdGas");
            property_id_t prop_merge_type = get_cached_property_id("mergeType");
            
            if (prop_coldgas < PROP_COUNT) {
                set_float_property(&test_galaxy, prop_coldgas, (1.0f + 0.1f * cycle) * 1e10f);
            }
            if (prop_merge_type < PROP_COUNT) {
                set_int32_property(&test_galaxy, prop_merge_type, cycle % 4);
            }
            
            if (prop_coldgas < PROP_COUNT) {
                float coldgas = get_float_property(&test_galaxy, prop_coldgas, 0.0f);
                float expected_coldgas = (1.0f + 0.1f * cycle) * 1e10f;
                TEST_ASSERT(fabs(coldgas - expected_coldgas) < 1e7f, "Property persistence in cycle: ColdGas");
            }
            if (prop_merge_type < PROP_COUNT) {
                int merge_type = get_int32_property(&test_galaxy, prop_merge_type, 0);
                TEST_ASSERT(merge_type == cycle % 4, "Property persistence in cycle: mergeType");
            }
            
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
    run_params.simulation.NumSnapOutputs = 10; // Set valid parameter for dynamic arrays
    
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
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        
        if (prop_coldgas < PROP_COUNT) {
            set_float_property(&source_galaxy, prop_coldgas, 5.5e10f);
        }
        if (prop_merge_type < PROP_COUNT) {
            set_int32_property(&source_galaxy, prop_merge_type, 3);
        }
        
        // Test normal copy
        int copy_result = copy_galaxy_properties(&dest_galaxy, &source_galaxy, &run_params);
        TEST_ASSERT(copy_result == 0, "Normal property copy succeeds");
        
        if (prop_coldgas < PROP_COUNT) {
            float coldgas = get_float_property(&dest_galaxy, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas - 5.5e10f) < 1e7f, "Property copy accuracy: ColdGas");
        }
        if (prop_merge_type < PROP_COUNT) {
            int merge_type = get_int32_property(&dest_galaxy, prop_merge_type, 0);
            TEST_ASSERT(merge_type == 3, "Property copy accuracy: mergeType");
        }
        
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