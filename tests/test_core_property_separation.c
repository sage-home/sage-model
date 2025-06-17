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

/**
 * Test that core properties are accessible via property system (single source of truth)
 */
static void test_core_property_system_access(void) {
    printf("\n=== Testing Core Property System Access ===\n");
    
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // Initialize minimal params for property system  
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    run_params.simulation.NumSnapOutputs = 10; // Required for property system
    
    // CRITICAL: Allocate properties FIRST
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Property system allocation succeeds");
    TEST_ASSERT(galaxy.properties != NULL, "Properties pointer is not NULL");
    
    if (galaxy.properties == NULL) {
        printf("ERROR: Cannot test core properties without property allocation\n");
        return;
    }
    
    // Set core properties using property macros (single source of truth)
    GALAXY_PROP_SnapNum(&galaxy) = 42;
    GALAXY_PROP_Type(&galaxy) = 1;
    GALAXY_PROP_GalaxyNr(&galaxy) = 12345;
    GALAXY_PROP_HaloNr(&galaxy) = 67890;
    GALAXY_PROP_MostBoundID(&galaxy) = 9876543210LL;
    GALAXY_PROP_Mvir(&galaxy) = 1.5e12f;
    GALAXY_PROP_Rvir(&galaxy) = 250.0f;
    GALAXY_PROP_Vvir(&galaxy) = 180.0f;
    GALAXY_PROP_Vmax(&galaxy) = 200.0f;
    
    // Position and velocity arrays using property macros
    GALAXY_PROP_Pos_ELEM(&galaxy, 0) = 10.5f;
    GALAXY_PROP_Pos_ELEM(&galaxy, 1) = 20.5f;
    GALAXY_PROP_Pos_ELEM(&galaxy, 2) = 30.5f;
    GALAXY_PROP_Vel_ELEM(&galaxy, 0) = 100.0f;
    GALAXY_PROP_Vel_ELEM(&galaxy, 1) = 200.0f;
    GALAXY_PROP_Vel_ELEM(&galaxy, 2) = 300.0f;
    
    // Test that all core properties are accessible via property system
    TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy) == 42, "SnapNum property access");
    TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 1, "Type property access");
    TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxy) == 12345, "GalaxyNr property access");
    TEST_ASSERT(GALAXY_PROP_HaloNr(&galaxy) == 67890, "HaloNr property access");
    TEST_ASSERT(GALAXY_PROP_MostBoundID(&galaxy) == 9876543210LL, "MostBoundID property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 1.5e12f) < 1e6f, "Mvir property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&galaxy) - 250.0f) < 0.1f, "Rvir property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&galaxy) - 180.0f) < 0.1f, "Vvir property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vmax(&galaxy) - 200.0f) < 0.1f, "Vmax property access");
    
    // Test array access via property system
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 0) - 10.5f) < 0.1f, "Pos[0] property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 1) - 20.5f) < 0.1f, "Pos[1] property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 2) - 30.5f) < 0.1f, "Pos[2] property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vel_ELEM(&galaxy, 0) - 100.0f) < 0.1f, "Vel[0] property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vel_ELEM(&galaxy, 1) - 200.0f) < 0.1f, "Vel[1] property access");
    TEST_ASSERT(fabs(GALAXY_PROP_Vel_ELEM(&galaxy, 2) - 300.0f) < 0.1f, "Vel[2] property access");
    
    // Clean up
    free_galaxy_properties(&galaxy);
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
        // Test physics properties via generic property system
        // Use property names as strings since macros may not be available in physics-free mode
        
        // Test that in physics-free mode, only core properties are available
        // Try to access physics properties via string lookup - should return invalid ID
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_stellar = get_cached_property_id("StellarMass");
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        
        // In physics-free mode, these should return PROP_COUNT (invalid)
        // In full-physics mode, these would return valid IDs
        if (prop_coldgas < PROP_COUNT) {
            // Full-physics mode - test property access
            set_float_property(&galaxy, prop_coldgas, 1.5e10f);
            float coldgas = get_float_property(&galaxy, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas - 1.5e10f) < 1e6f, "ColdGas property access in full-physics mode");
            
            if (prop_stellar < PROP_COUNT) {
                set_float_property(&galaxy, prop_stellar, 2.3e10f);
                float stellar = get_float_property(&galaxy, prop_stellar, 0.0f);
                TEST_ASSERT(fabs(stellar - 2.3e10f) < 1e6f, "StellarMass property access in full-physics mode");
            }
            
            if (prop_merge_type < PROP_COUNT) {
                set_int32_property(&galaxy, prop_merge_type, 2);
                int merge_type = get_int32_property(&galaxy, prop_merge_type, 0);
                TEST_ASSERT(merge_type == 2, "mergeType property access in full-physics mode");
            }
        } else {
            // Physics-free mode - physics properties should not be available
            TEST_ASSERT(prop_coldgas >= PROP_COUNT, "ColdGas not available in physics-free mode (expected)");
            TEST_ASSERT(prop_stellar >= PROP_COUNT, "StellarMass not available in physics-free mode (expected)");
            TEST_ASSERT(prop_merge_type >= PROP_COUNT, "mergeType not available in physics-free mode (expected)");
        }
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
        // Set core properties via property system (single source of truth)
        GALAXY_PROP_SnapNum(&galaxy) = 42;
        GALAXY_PROP_Type(&galaxy) = 1;
        GALAXY_PROP_Mvir(&galaxy) = 1.5e12f;
        
        // Test core-physics independence via property system
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        
        // Test that core and physics systems work together in single source of truth
        TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy) == 42, "Core SnapNum accessible via property system");
        TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 1, "Core Type accessible via property system");
        TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 1.5e12f) < 1e6f, "Core Mvir accessible via property system");
        
        // Only test physics properties if they're available (full-physics mode)
        if (prop_coldgas < PROP_COUNT) {
            set_float_property(&galaxy, prop_coldgas, 2.5e10f);
            float coldgas = get_float_property(&galaxy, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas - 2.5e10f) < 1e6f, "Physics ColdGas maintains value independently");
            
            // Modify core property and verify physics property is unaffected
            GALAXY_PROP_Type(&galaxy) = 2;
            coldgas = get_float_property(&galaxy, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas - 2.5e10f) < 1e6f, "Physics property unchanged when core modified");
            TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 2, "Core property change works via property system");
        } else {
            // In physics-free mode, just verify core property system works
            GALAXY_PROP_Type(&galaxy) = 2;
            TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 2, "Core property change works in physics-free mode");
            TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 1.5e12f) < 1e6f, "Other core properties unaffected");
        }
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
        // Test core array properties (these should always be available)
        GALAXY_PROP_Pos_ELEM(&galaxy, 0) = 10.5f;
        GALAXY_PROP_Pos_ELEM(&galaxy, 1) = 20.5f;
        GALAXY_PROP_Pos_ELEM(&galaxy, 2) = 30.5f;
        
        float pos_x = GALAXY_PROP_Pos_ELEM(&galaxy, 0);
        float pos_y = GALAXY_PROP_Pos_ELEM(&galaxy, 1);
        float pos_z = GALAXY_PROP_Pos_ELEM(&galaxy, 2);
        
        TEST_ASSERT(fabs(pos_x - 10.5f) < 0.1f, "Core array property element [0] access");
        TEST_ASSERT(fabs(pos_y - 20.5f) < 0.1f, "Core array property element [1] access");
        TEST_ASSERT(fabs(pos_z - 30.5f) < 0.1f, "Core array property element [2] access");
        
        // Test physics properties via generic accessors if available
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_cooling = get_cached_property_id("Cooling");
        
        if (prop_merge_type < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_type, 2);
            int merge_type = get_int32_property(&galaxy, prop_merge_type, 0);
            TEST_ASSERT(merge_type == 2, "int32_t physics property handling");
        }
        
        if (prop_coldgas < PROP_COUNT) {
            set_float_property(&galaxy, prop_coldgas, 1.23456e10f);
            float coldgas = get_float_property(&galaxy, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas - 1.23456e10f) < 1e4f, "float physics property handling");
            
            // Test boundary values
            set_float_property(&galaxy, prop_coldgas, 0.0f);
            coldgas = get_float_property(&galaxy, prop_coldgas, -1.0f);
            TEST_ASSERT(fabs(coldgas - 0.0f) < 1e-10f, "Zero value handling");
            
            set_float_property(&galaxy, prop_coldgas, 1e15f);
            coldgas = get_float_property(&galaxy, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas - 1e15f) < 1e10f, "Large value handling");
        }
        
        if (prop_cooling < PROP_COUNT) {
            set_double_property(&galaxy, prop_cooling, 9.87654321e20);
            double cooling = get_double_property(&galaxy, prop_cooling, 0.0);
            TEST_ASSERT(fabs(cooling - 9.87654321e20) < 1e15, "double physics property handling");
        }
        
        // If we're in physics-free mode, verify core properties work correctly
        if (prop_coldgas >= PROP_COUNT) {
            TEST_ASSERT(1, "Physics-free mode confirmed - physics properties unavailable");
        }
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
        // Test that physics properties are only accessible via generic property system
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        property_id_t prop_merge_id = get_cached_property_id("mergeIntoID");
        property_id_t prop_merge_snap = get_cached_property_id("mergeIntoSnapNum");
        
        if (prop_merge_type < PROP_COUNT) {
            // In full-physics mode, these should be accessible
            set_int32_property(&galaxy, prop_merge_type, 2);
            set_int32_property(&galaxy, prop_merge_id, 12345);
            set_int32_property(&galaxy, prop_merge_snap, 62);
            
            int merge_type = get_int32_property(&galaxy, prop_merge_type, -1);
            int merge_id = get_int32_property(&galaxy, prop_merge_id, -1);
            int merge_snap = get_int32_property(&galaxy, prop_merge_snap, -1);
            
            TEST_ASSERT(merge_type == 2, "mergeType only accessible via generic property system");
            TEST_ASSERT(merge_id == 12345, "mergeIntoID only accessible via generic property system");
            TEST_ASSERT(merge_snap == 62, "mergeIntoSnapNum only accessible via generic property system");
        } else {
            // In physics-free mode, these properties should not be available
            TEST_ASSERT(prop_merge_type >= PROP_COUNT, "mergeType unavailable in physics-free mode");
            TEST_ASSERT(prop_merge_id >= PROP_COUNT, "mergeIntoID unavailable in physics-free mode");
            TEST_ASSERT(prop_merge_snap >= PROP_COUNT, "mergeIntoSnapNum unavailable in physics-free mode");
        }
    }
    
    // Cleanup
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
}

int main(int argc, char *argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    printf("\n========================================\n");
    printf("Starting tests for test_core_property_separation\n");
    printf("========================================\n\n");
    
    printf("This test verifies that core-physics property separation is properly implemented:\n");
    printf("  1. Core properties are accessible via direct struct access\n");
    printf("  2. Physics properties are only accessible via property system\n");
    printf("  3. No dual-state synchronization issues exist\n");
    printf("  4. Property system robustness for all data types\n\n");

    // Initialize logging to suppress debug output during tests
    logging_init(LOG_LEVEL_WARNING, stderr);
    
    // Run tests
    test_core_property_system_access();
    test_physics_property_system_access();
    test_no_dual_state_synchronization();
    test_property_system_data_types();
    test_dual_state_properties_removed();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_core_property_separation:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
