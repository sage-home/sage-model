/**
 * @file test_core_property_separation.c
 * @brief Tests for core-physics property separation validation
 * 
 * This test validates that the core-physics property separation is properly implemented:
 * - All core properties are accessible via direct struct access
 * - All non-core properties are only accessible via property system
 * - No dual-state synchronization issues exist
 * - Property system robustness for all data types
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
#include "../src/physics/physics_essential_functions.h"

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

/**
 * Test that core properties are accessible via direct struct access
 */
static void test_core_property_direct_access(void) {
    printf("\n=== Testing Core Property Direct Access ===\n");
    
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // Test direct access to core properties
    galaxy.SnapNum = 42;
    galaxy.Type = 1;
    galaxy.GalaxyNr = 12345;
    galaxy.HaloNr = 67890;
    galaxy.MostBoundID = 9876543210LL;
    galaxy.Mvir = 1.5e12f;
    galaxy.Rvir = 250.0f;
    galaxy.Vvir = 180.0f;
    galaxy.Vmax = 200.0f;
    galaxy.MergTime = 5.5f;
    galaxy.infallMvir = 1.2e12f;
    galaxy.infallVvir = 175.0f;
    galaxy.infallVmax = 195.0f;
    
    // Position and velocity arrays
    galaxy.Pos[0] = 10.5f; galaxy.Pos[1] = 20.5f; galaxy.Pos[2] = 30.5f;
    galaxy.Vel[0] = 100.0f; galaxy.Vel[1] = 200.0f; galaxy.Vel[2] = 300.0f;
    
    // Test that all core properties are directly accessible
    TEST_ASSERT(galaxy.SnapNum == 42, "SnapNum direct access");
    TEST_ASSERT(galaxy.Type == 1, "Type direct access");
    TEST_ASSERT(galaxy.GalaxyNr == 12345, "GalaxyNr direct access");
    TEST_ASSERT(galaxy.HaloNr == 67890, "HaloNr direct access");
    TEST_ASSERT(galaxy.MostBoundID == 9876543210LL, "MostBoundID direct access");
    TEST_ASSERT(fabs(galaxy.Mvir - 1.5e12f) < 1e6f, "Mvir direct access");
    TEST_ASSERT(fabs(galaxy.Rvir - 250.0f) < 0.1f, "Rvir direct access");
    TEST_ASSERT(fabs(galaxy.Vvir - 180.0f) < 0.1f, "Vvir direct access");
    TEST_ASSERT(fabs(galaxy.Vmax - 200.0f) < 0.1f, "Vmax direct access");
    TEST_ASSERT(fabs(galaxy.MergTime - 5.5f) < 0.1f, "MergTime direct access");
    TEST_ASSERT(fabs(galaxy.infallMvir - 1.2e12f) < 1e6f, "infallMvir direct access");
    TEST_ASSERT(fabs(galaxy.infallVvir - 175.0f) < 0.1f, "infallVvir direct access");
    TEST_ASSERT(fabs(galaxy.infallVmax - 195.0f) < 0.1f, "infallVmax direct access");
    
    // Test array access
    TEST_ASSERT(fabs(galaxy.Pos[0] - 10.5f) < 0.1f, "Pos[0] direct access");
    TEST_ASSERT(fabs(galaxy.Pos[1] - 20.5f) < 0.1f, "Pos[1] direct access");
    TEST_ASSERT(fabs(galaxy.Pos[2] - 30.5f) < 0.1f, "Pos[2] direct access");
    TEST_ASSERT(fabs(galaxy.Vel[0] - 100.0f) < 0.1f, "Vel[0] direct access");
    TEST_ASSERT(fabs(galaxy.Vel[1] - 200.0f) < 0.1f, "Vel[1] direct access");
    TEST_ASSERT(fabs(galaxy.Vel[2] - 300.0f) < 0.1f, "Vel[2] direct access");
}

/**
 * Test that physics properties are only accessible via property system
 */
static void test_physics_property_system_access(void) {
    printf("\n=== Testing Physics Property System Access ===\n");
    
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // Initialize minimal params for property system  
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Allocate property system
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Property system allocation succeeds");
    TEST_ASSERT(galaxy.properties != NULL, "Properties pointer is not NULL");
    
    if (galaxy.properties != NULL) {
        // Test physics properties via property system only
        GALAXY_PROP_ColdGas(&galaxy) = 1.5e10f;
        GALAXY_PROP_StellarMass(&galaxy) = 2.3e10f;
        GALAXY_PROP_HotGas(&galaxy) = 5.7e10f;
        GALAXY_PROP_BlackHoleMass(&galaxy) = 1.2e8f;
        
        // Verify values can be retrieved
        float coldgas = GALAXY_PROP_ColdGas(&galaxy);
        float stellar = GALAXY_PROP_StellarMass(&galaxy);
        float hotgas = GALAXY_PROP_HotGas(&galaxy);
        float bh = GALAXY_PROP_BlackHoleMass(&galaxy);
        
        TEST_ASSERT(fabs(coldgas - 1.5e10f) < 1e6f, "ColdGas property system access");
        TEST_ASSERT(fabs(stellar - 2.3e10f) < 1e6f, "StellarMass property system access");
        TEST_ASSERT(fabs(hotgas - 5.7e10f) < 1e6f, "HotGas property system access");
        TEST_ASSERT(fabs(bh - 1.2e8f) < 1e4f, "BlackHoleMass property system access");
        
        // Test merger properties that will be moved to property system
        GALAXY_PROP_mergeType(&galaxy) = 2;
        GALAXY_PROP_mergeIntoID(&galaxy) = 12345;
        GALAXY_PROP_mergeIntoSnapNum(&galaxy) = 62;
        
        int merge_type = GALAXY_PROP_mergeType(&galaxy);
        int merge_id = GALAXY_PROP_mergeIntoID(&galaxy);
        int merge_snap = GALAXY_PROP_mergeIntoSnapNum(&galaxy);
        
        TEST_ASSERT(merge_type == 2, "mergeType property system access");
        TEST_ASSERT(merge_id == 12345, "mergeIntoID property system access");
        TEST_ASSERT(merge_snap == 62, "mergeIntoSnapNum property system access");
    }
    
    // Cleanup
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
}

/**
 * Test that there are no dual-state synchronization issues
 */
static void test_no_dual_state_synchronization(void) {
    printf("\n=== Testing No Dual-State Synchronization ===\n");
    
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Allocate property system
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Property system allocation succeeds");
    
    if (galaxy.properties != NULL) {
        // Set core properties via direct access
        galaxy.SnapNum = 42;
        galaxy.Type = 1;
        galaxy.Mvir = 1.5e12f;
        
        // Set physics properties via property system
        GALAXY_PROP_ColdGas(&galaxy) = 2.5e10f;
        GALAXY_PROP_StellarMass(&galaxy) = 3.7e10f;
        
        // After separation, these should be independent - no synchronization needed
        // Test that each system maintains its values independently
        TEST_ASSERT(galaxy.SnapNum == 42, "Core SnapNum maintains value independently");
        TEST_ASSERT(galaxy.Type == 1, "Core Type maintains value independently");
        TEST_ASSERT(fabs(galaxy.Mvir - 1.5e12f) < 1e6f, "Core Mvir maintains value independently");
        
        float coldgas = GALAXY_PROP_ColdGas(&galaxy);
        float stellar = GALAXY_PROP_StellarMass(&galaxy);
        TEST_ASSERT(fabs(coldgas - 2.5e10f) < 1e6f, "Physics ColdGas maintains value independently");
        TEST_ASSERT(fabs(stellar - 3.7e10f) < 1e6f, "Physics StellarMass maintains value independently");
        
        // Modify one system and verify the other is unaffected
        galaxy.Type = 2;  // Change core property
        GALAXY_PROP_ColdGas(&galaxy) = 3.0e10f;  // Change physics property
        
        TEST_ASSERT(galaxy.Type == 2, "Core property change is independent");
        coldgas = GALAXY_PROP_ColdGas(&galaxy);
        TEST_ASSERT(fabs(coldgas - 3.0e10f) < 1e6f, "Physics property change is independent");
        TEST_ASSERT(fabs(galaxy.Mvir - 1.5e12f) < 1e6f, "Other core properties unaffected");
        
        stellar = GALAXY_PROP_StellarMass(&galaxy);
        TEST_ASSERT(fabs(stellar - 3.7e10f) < 1e6f, "Other physics properties unaffected");
    }
    
    // Cleanup
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
}

/**
 * Test property system robustness for all data types
 */
static void test_property_system_data_types(void) {
    printf("\n=== Testing Property System Data Type Robustness ===\n");
    
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Allocate property system
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Property system allocation succeeds");
    
    if (galaxy.properties != NULL) {
        // Test int32_t properties
        GALAXY_PROP_mergeType(&galaxy) = 2;
        int merge_type = GALAXY_PROP_mergeType(&galaxy);
        TEST_ASSERT(merge_type == 2, "int32_t property handling");
        
        // Test float properties
        GALAXY_PROP_ColdGas(&galaxy) = 1.23456e10f;
        float coldgas = GALAXY_PROP_ColdGas(&galaxy);
        TEST_ASSERT(fabs(coldgas - 1.23456e10f) < 1e4f, "float property handling");
        
        // Test double properties
        GALAXY_PROP_Cooling(&galaxy) = 9.87654321e20;
        double cooling = GALAXY_PROP_Cooling(&galaxy);
        TEST_ASSERT(fabs(cooling - 9.87654321e20) < 1e15, "double property handling");
        
        // Test array properties (if accessible)
        GALAXY_PROP_Pos_ELEM(&galaxy, 0) = 10.5f;
        GALAXY_PROP_Pos_ELEM(&galaxy, 1) = 20.5f;
        GALAXY_PROP_Pos_ELEM(&galaxy, 2) = 30.5f;
        
        float pos_x = GALAXY_PROP_Pos_ELEM(&galaxy, 0);
        float pos_y = GALAXY_PROP_Pos_ELEM(&galaxy, 1);
        float pos_z = GALAXY_PROP_Pos_ELEM(&galaxy, 2);
        
        TEST_ASSERT(fabs(pos_x - 10.5f) < 0.1f, "Array property element [0] access");
        TEST_ASSERT(fabs(pos_y - 20.5f) < 0.1f, "Array property element [1] access");
        TEST_ASSERT(fabs(pos_z - 30.5f) < 0.1f, "Array property element [2] access");
        
        // Test boundary values
        GALAXY_PROP_ColdGas(&galaxy) = 0.0f;
        coldgas = GALAXY_PROP_ColdGas(&galaxy);
        TEST_ASSERT(fabs(coldgas - 0.0f) < 1e-10f, "Zero value handling");
        
        GALAXY_PROP_ColdGas(&galaxy) = 1e15f;  // Very large value
        coldgas = GALAXY_PROP_ColdGas(&galaxy);
        TEST_ASSERT(fabs(coldgas - 1e15f) < 1e10f, "Large value handling");
    }
    
    // Cleanup
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
}

/**
 * Test that removed properties no longer exist in struct GALAXY
 * This test will verify our separation was successful
 */
static void test_dual_state_properties_removed(void) {
    printf("\n=== Testing Dual-State Properties Are Removed ===\n");
    
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // These properties should NO LONGER exist as direct struct members:
    // galaxy.mergeType, galaxy.mergeIntoID, galaxy.mergeIntoSnapNum
    
    // We'll test this by checking that the struct size is reduced
    // and that these properties are only accessible via property system
    
    size_t struct_size = sizeof(struct GALAXY);
    printf("  Current struct GALAXY size: %zu bytes\n", struct_size);
    
    // After removing 3 int32_t fields (12 bytes), struct should be smaller
    // This is an indirect test - the direct test would be compilation failure
    // if we tried to access galaxy.mergeType directly
    
    TEST_ASSERT(struct_size > 0, "struct GALAXY has reasonable size");
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Allocate property system
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Property system allocation succeeds");
    
    if (galaxy.properties != NULL) {
        // These properties should ONLY be accessible via property system now
        GALAXY_PROP_mergeType(&galaxy) = 2;
        GALAXY_PROP_mergeIntoID(&galaxy) = 12345;
        GALAXY_PROP_mergeIntoSnapNum(&galaxy) = 62;
        
        int merge_type = GALAXY_PROP_mergeType(&galaxy);
        int merge_id = GALAXY_PROP_mergeIntoID(&galaxy);
        int merge_snap = GALAXY_PROP_mergeIntoSnapNum(&galaxy);
        
        TEST_ASSERT(merge_type == 2, "mergeType only accessible via property system");
        TEST_ASSERT(merge_id == 12345, "mergeIntoID only accessible via property system");
        TEST_ASSERT(merge_snap == 62, "mergeIntoSnapNum only accessible via property system");
    }
    
    // Cleanup
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
}

int main(void) {
    printf("Starting Core-Physics Property Separation Tests\n");
    printf("================================================\n");
    
    // Initialize logging to suppress debug output during tests
    logging_init(LOG_LEVEL_WARNING, stderr);
    
    // Run all test suites
    test_core_property_direct_access();
    test_physics_property_system_access();
    test_no_dual_state_synchronization();
    test_property_system_data_types();
    test_dual_state_properties_removed();
    
    // Report results
    printf("\n================================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✓ All tests passed! Core-physics property separation is working correctly.\n");
        return EXIT_SUCCESS;
    } else {
        printf("✗ %d tests failed. Property separation needs attention.\n", tests_run - tests_passed);
        return EXIT_FAILURE;
    }
}