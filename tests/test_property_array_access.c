/**
 * Test suite for Property System Array Access
 * 
 * Tests cover:
 * - All property types (float, int32, double, int64)
 * - Array and scalar properties
 * - Fixed and dynamic arrays
 * - Error handling and edge cases
 * - Integration with galaxy lifecycle
 * - Property registration and metadata
 * - Memory management validation
 * - Core-physics property separation compliance
 * 
 * ARCHITECTURAL COMPLIANCE:
 * - Core properties (is_core: true): Use direct GALAXY_PROP_* macros
 * - Physics properties (is_core: false): Use generic accessor functions only
 * - Tests demonstrate proper separation principles as documented
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_property_utils.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h"

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
    struct GALAXY *test_galaxy;
    struct params test_params;
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize logging system first
    logging_init(LOG_LEVEL_DEBUG, stdout);
    
    // Initialize test parameters with realistic values FIRST
    test_ctx.test_params.simulation.NumSnapOutputs = 15;  // For dynamic arrays (StarFormationHistory)
    test_ctx.test_params.cosmology.Omega = 0.3;
    test_ctx.test_params.cosmology.OmegaLambda = 0.7;
    test_ctx.test_params.cosmology.Hubble_h = 0.7;
    
    // Initialize property system after parameters are set
    if (initialize_property_system(&test_ctx.test_params) != 0) {
        printf("WARNING: Could not initialize property system, using minimal setup\n");
    }
    
    // Allocate test galaxy
    test_ctx.test_galaxy = calloc(1, sizeof(struct GALAXY));
    if (!test_ctx.test_galaxy) {
        printf("ERROR: Failed to allocate test galaxy\n");
        return -1;
    }
    
    // Set basic galaxy info
    test_ctx.test_galaxy->GalaxyIndex = 12345;
    test_ctx.test_galaxy->GalaxyNr = 1;
    
    test_ctx.initialized = 1;
    return 0;
}
// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.test_galaxy) {
        if (test_ctx.test_galaxy->properties) {
            free_galaxy_properties(test_ctx.test_galaxy);
        }
        free(test_ctx.test_galaxy);
        test_ctx.test_galaxy = NULL;
    }
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Property system initialisation with proper lifecycle
 */
static void test_property_initialisation(void) {
    printf("=== Testing property system initialisation ===\n");
    
    TEST_ASSERT(test_ctx.test_galaxy != NULL, "Test galaxy should be allocated");
    
    // Use proper property system initialization
    if (allocate_galaxy_properties(test_ctx.test_galaxy, &test_ctx.test_params) != 0) {
        printf("ERROR: Failed to allocate galaxy properties\n");
        return;
    }
    
    TEST_ASSERT(test_ctx.test_galaxy->properties != NULL, "Galaxy properties should be allocated");
    
    // Initialize with default values
    reset_galaxy_properties(test_ctx.test_galaxy);
    
    // Verify property access returns sensible initial values
    float mvir = GALAXY_PROP_Mvir(test_ctx.test_galaxy);
    int type = GALAXY_PROP_Type(test_ctx.test_galaxy);
    
    TEST_ASSERT(mvir >= 0.0f, "Initial Mvir should be non-negative");
    TEST_ASSERT(type >= 0, "Initial Type should be non-negative");
    
    printf("  Initial Mvir: %g, Type: %d\n", mvir, type);
}

/**
 * Test: Scalar property access across all types
 */
static void test_scalar_property_access(void) {
    printf("\n=== Testing scalar property access ===\n");
    
    // Test float properties
    // Use GALAXY_PROP_ macro for core properties (Mvir is core)
    GALAXY_PROP_Mvir(test_ctx.test_galaxy) = 1.5e12f;
    
    float mvir_direct = GALAXY_PROP_Mvir(test_ctx.test_galaxy);
    float mvir_by_fn = get_float_property(test_ctx.test_galaxy, PROP_Mvir, 0.0f);
    
    TEST_ASSERT(mvir_direct == mvir_by_fn, "Float property: direct and function access should match");
    TEST_ASSERT(fabsf(mvir_direct - 1.5e12f) < 1e6f, "Float property: Mvir value should be correct");
    
    // Test physics properties using generic accessors (architectural compliance)
    // ColdGas - physics property, must use generic accessors
    set_float_property(test_ctx.test_galaxy, PROP_ColdGas, 2.5e10f);
    float coldgas_value = get_float_property(test_ctx.test_galaxy, PROP_ColdGas, 0.0f);
    TEST_ASSERT(fabsf(coldgas_value - 2.5e10f) < 1e6f, "Physics property ColdGas should be accessible via generic functions");
    
    // Test additional physics baryonic properties
    set_float_property(test_ctx.test_galaxy, PROP_StellarMass, 1.2e11f);
    float stellar_mass = get_float_property(test_ctx.test_galaxy, PROP_StellarMass, 0.0f);
    TEST_ASSERT(fabsf(stellar_mass - 1.2e11f) < 1e6f, "Physics property StellarMass should work correctly");
    
    set_float_property(test_ctx.test_galaxy, PROP_HotGas, 3.4e10f);
    float hot_gas = get_float_property(test_ctx.test_galaxy, PROP_HotGas, 0.0f);
    TEST_ASSERT(fabsf(hot_gas - 3.4e10f) < 1e6f, "Physics property HotGas should work correctly");
    
    printf("  Float properties: Mvir=%g (core), ColdGas=%g, StellarMass=%g, HotGas=%g (all physics)\n", 
           mvir_direct, coldgas_value, stellar_mass, hot_gas);
    
    // Test int32 properties
    GALAXY_PROP_Type(test_ctx.test_galaxy) = 2;
    GALAXY_PROP_SnapNum(test_ctx.test_galaxy) = 63;
    
    int32_t type_direct = GALAXY_PROP_Type(test_ctx.test_galaxy);
    int32_t type_by_fn = get_int32_property(test_ctx.test_galaxy, PROP_Type, -1);
    int32_t snap_direct = GALAXY_PROP_SnapNum(test_ctx.test_galaxy);
    int32_t snap_by_fn = get_int32_property(test_ctx.test_galaxy, PROP_SnapNum, -1);
    
    TEST_ASSERT(type_direct == type_by_fn, "Int32 property: direct and function access should match");
    TEST_ASSERT(snap_direct == snap_by_fn, "Int32 property: SnapNum access should match");
    TEST_ASSERT(type_direct == 2, "Int32 property: Type value should be correct");
    
    printf("  Int32 properties: Type=%d, SnapNum=%d\n", type_direct, snap_direct);
    
    // Test double properties (physics properties - use generic accessors)
    set_double_property(test_ctx.test_galaxy, PROP_Cooling, 1.23e-15);
    set_double_property(test_ctx.test_galaxy, PROP_Heating, 4.56e-16);
    
    double cooling_value = get_double_property(test_ctx.test_galaxy, PROP_Cooling, 0.0);
    double heating_value = get_double_property(test_ctx.test_galaxy, PROP_Heating, 0.0);
    
    TEST_ASSERT(fabs(cooling_value - 1.23e-15) < 1e-20, "Physics property Cooling should work correctly via generic accessor");
    TEST_ASSERT(fabs(heating_value - 4.56e-16) < 1e-20, "Physics property Heating should work correctly via generic accessor");
    
    printf("  Double properties: Cooling=%g, Heating=%g (both physics)\n", cooling_value, heating_value);
    
    // Test int64 properties (using GalaxyIndex as example)
    // Set GalaxyIndex using the property system
    GALAXY_PROP_GalaxyIndex(test_ctx.test_galaxy) = 9876543210LL;
    
    int64_t index_direct = GALAXY_PROP_GalaxyIndex(test_ctx.test_galaxy);
    int64_t index_by_fn = get_int64_property(test_ctx.test_galaxy, PROP_GalaxyIndex, 0LL);
    
    TEST_ASSERT(index_direct == index_by_fn, "Int64 property: direct and function access should match");
    TEST_ASSERT(index_direct == 9876543210LL, "Int64 property: GalaxyIndex value should be correct");
    
    printf("  Int64 properties: GalaxyIndex=%lld\n", index_direct);
}

/**
 * Test: Fixed array property access
 */
static void test_fixed_array_access(void) {
    printf("\n=== Testing fixed array property access ===\n");
    
    // Test SfrDisk (fixed array with STEPS elements) - physics property, use generic accessors
    #define TEST_STEPS 10  // Fixed size for SFR arrays, matching STEPS in properties.yaml
    for (int i = 0; i < TEST_STEPS; i++) {
        set_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, i, 10.5f + i * 0.1f);
    }
    
    // Verify access patterns using generic accessors only
    for (int i = 0; i < 5; i++) {  // Test first 5 elements
        float value = get_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, i, -999.0f);
        float expected = 10.5f + i * 0.1f;
        
        TEST_ASSERT(fabsf(value - expected) < 1e-6f, "Fixed array SfrDisk: element value should be correct via generic accessor");
        
        printf("  SfrDisk[%d]: value=%g, expected=%g (physics property)\n", i, value, expected);
    }
    
    // Test array size retrieval (SfrDisk is now a fixed array sized by STEPS)
    int array_size = get_property_array_size(test_ctx.test_galaxy, PROP_SfrDisk);
    TEST_ASSERT(array_size == TEST_STEPS, "SfrDisk array: size should match STEPS");
    
    printf("  SfrDisk array size: %d (STEPS=%d)\n", array_size, TEST_STEPS);
}

/**
 * Test: Dynamic array property access
 */
static void test_dynamic_array_access(void) {
    printf("\n=== Testing dynamic array property access ===\n");
    
    // Test StarFormationHistory (dynamic array) - physics property, use generic accessors
    int expected_size = test_ctx.test_params.simulation.NumSnapOutputs;
    
    // Set values in the dynamic array using generic accessors
    for (int i = 0; i < expected_size; i++) {
        set_float_array_element_property(test_ctx.test_galaxy, PROP_StarFormationHistory, i, 100.0f + i * 5.0f);
    }
    
    // Verify access patterns using generic accessors only
    for (int i = 0; i < expected_size; i++) {
        float value = get_float_array_element_property(test_ctx.test_galaxy, PROP_StarFormationHistory, i, -999.0f);
        float expected = 100.0f + i * 5.0f;
        
        TEST_ASSERT(fabsf(value - expected) < 1e-6f, "Dynamic array StarFormationHistory: element value should be correct via generic accessor");
        
        if (i < 3) {  // Only print first few for brevity
            printf("  StarFormationHistory[%d]: value=%g, expected=%g (physics property)\n", i, value, expected);
        }
    }
    
    // Test array size retrieval
    int array_size = get_property_array_size(test_ctx.test_galaxy, PROP_StarFormationHistory);
    TEST_ASSERT(array_size == expected_size, "Dynamic array: size should match NumSnapOutputs");
    
    printf("  StarFormationHistory array size: %d (expected=%d)\n", array_size, expected_size);
}

/**
 * Test: Error handling
 */
static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    // Note: NULL galaxy tests will generate expected ERROR log messages
    printf("  Expected error messages for NULL pointer validation:\n");
    
    // Test NULL galaxy pointer
    float result_float = get_float_property(NULL, PROP_Mvir, -999.0f);
    TEST_ASSERT(result_float == -999.0f, "NULL galaxy should return default value");
    
    int32_t result_int = get_int32_property(NULL, PROP_Type, -888);
    TEST_ASSERT(result_int == -888, "NULL galaxy should return default value for int");
    
    double result_double = get_double_property(NULL, PROP_Cooling, -777.0);
    TEST_ASSERT(result_double == -777.0, "NULL galaxy should return default value for double");
    
    printf("  NULL galaxy tests completed (error messages above are expected)\n");
    
    // Test invalid property ID
    float invalid_result = get_float_property(test_ctx.test_galaxy, (property_id_t)9999, -666.0f);
    TEST_ASSERT(invalid_result == -666.0f, "Invalid property ID should return default value");
    
    // Test out-of-bounds array access
    float oob_result = get_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, STEPS + 10, -555.0f);
    TEST_ASSERT(oob_result == -555.0f, "Out-of-bounds array access should return default value");
    
    // Test negative array index
    float neg_result = get_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, -1, -444.0f);
    TEST_ASSERT(neg_result == -444.0f, "Negative array index should return default value");
    
    printf("  Error cases: invalid_prop=%g, oob_array=%g, neg_index=%g\n", 
           invalid_result, oob_result, neg_result);
}

/**
 * Test: Edge cases
 */
static void test_edge_cases(void) {
    printf("\n=== Testing edge cases ===\n");
    
    // Test setting extreme values
    GALAXY_PROP_Mvir(test_ctx.test_galaxy) = 0.0f;
    float zero_mvir = get_float_property(test_ctx.test_galaxy, PROP_Mvir, -1.0f);
    TEST_ASSERT(zero_mvir == 0.0f, "Zero value should be preserved");
    
    GALAXY_PROP_Mvir(test_ctx.test_galaxy) = 1e20f;
    float large_mvir = get_float_property(test_ctx.test_galaxy, PROP_Mvir, -1.0f);
    TEST_ASSERT(large_mvir == 1e20f, "Large value should be preserved");
    
    // Test negative values where appropriate
    GALAXY_PROP_dT(test_ctx.test_galaxy) = -0.5f;
    float neg_dt = get_float_property(test_ctx.test_galaxy, PROP_dT, 999.0f);
    TEST_ASSERT(neg_dt == -0.5f, "Negative dT should be preserved");
    
    // Test boundary array indices for physics properties (use generic accessors)
    int array_size = get_property_array_size(test_ctx.test_galaxy, PROP_SfrDisk);
    if (array_size > 0) {
        // Test last valid index using generic accessors
        set_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, array_size - 1, 42.0f);
        float last_elem = get_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, array_size - 1, -1.0f);
        TEST_ASSERT(last_elem == 42.0f, "Last array element should be accessible via generic accessor");
        
        // Test first element using generic accessors
        set_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, 0, 24.0f);
        float first_elem = get_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, 0, -1.0f);
        TEST_ASSERT(first_elem == 24.0f, "First array element should be accessible via generic accessor");
    }
    
    printf("  Edge cases: zero=%g, large=%g, negative=%g\n", zero_mvir, large_mvir, neg_dt);
}

/**
 * Test: Property metadata and registration
 */
static void test_property_metadata(void) {
    printf("\n=== Testing property metadata ===\n");
    
    // Test property ID validity
    TEST_ASSERT(PROP_Mvir >= 0 && PROP_Mvir < PROP_COUNT, "Mvir property ID should be valid");
    TEST_ASSERT(PROP_SfrDisk >= 0 && PROP_SfrDisk < PROP_COUNT, "SfrDisk property ID should be valid");
    TEST_ASSERT(PROP_StarFormationHistory >= 0 && PROP_StarFormationHistory < PROP_COUNT, "StarFormationHistory property ID should be valid");
    
    // Test has_property function
    bool has_mvir = has_property(test_ctx.test_galaxy, PROP_Mvir);
    bool has_invalid = has_property(test_ctx.test_galaxy, (property_id_t)9999);
    
    TEST_ASSERT(has_mvir == true, "Galaxy should have Mvir property");
    TEST_ASSERT(has_invalid == false, "Galaxy should not have invalid property");
    
    // Test array size consistency
    int sfr_size = get_property_array_size(test_ctx.test_galaxy, PROP_SfrDisk);
    int sfh_size = get_property_array_size(test_ctx.test_galaxy, PROP_StarFormationHistory);
    
    TEST_ASSERT(sfr_size == TEST_STEPS, "SfrDisk size should match STEPS (fixed array)");
    TEST_ASSERT(sfh_size == test_ctx.test_params.simulation.NumSnapOutputs, "StarFormationHistory size should match NumSnapOutputs (dynamic array)");
    
    printf("  Property validation: has_mvir=%s, has_invalid=%s\n", 
           has_mvir ? "true" : "false", has_invalid ? "true" : "false");
    printf("  Array sizes: SfrDisk=%d, StarFormationHistory=%d\n", sfr_size, sfh_size);
}

/**
 * Test: Memory management validation
 */
static void test_memory_management(void) {
    printf("\n=== Testing memory management ===\n");
    
    // Test galaxy copying with properties
    struct GALAXY copy_galaxy = {0};
    copy_galaxy.GalaxyIndex = 99999;
    
    // Allocate properties for copy
    if (allocate_galaxy_properties(&copy_galaxy, &test_ctx.test_params) != 0) {
        printf("ERROR: Failed to allocate properties for copy galaxy\n");
        return;
    }
    
    // Set some values in original (using appropriate access patterns)
    GALAXY_PROP_Mvir(test_ctx.test_galaxy) = 1.23e12f;  // Core property - direct access OK
    GALAXY_PROP_Type(test_ctx.test_galaxy) = 5;          // Core property - direct access OK
    set_float_array_element_property(test_ctx.test_galaxy, PROP_SfrDisk, 0, 7.89f);  // Physics property - generic accessor
    
    // Copy properties
    if (copy_galaxy_properties(&copy_galaxy, test_ctx.test_galaxy, &test_ctx.test_params) == 0) {
        // Verify copy worked (using appropriate access patterns)
        float copy_mvir = GALAXY_PROP_Mvir(&copy_galaxy);  // Core property - direct access OK
        int32_t copy_type = GALAXY_PROP_Type(&copy_galaxy); // Core property - direct access OK
        float copy_sfr = get_float_array_element_property(&copy_galaxy, PROP_SfrDisk, 0, -999.0f); // Physics property - generic accessor
        
        TEST_ASSERT(fabsf(copy_mvir - 1.23e12f) < 1e6f, "Copied Mvir should match original");
        TEST_ASSERT(copy_type == 5, "Copied Type should match original");
        TEST_ASSERT(fabsf(copy_sfr - 7.89f) < 1e-6f, "Copied SfrDisk should match original");
        
        printf("  Copy validation: Mvir=%g, Type=%d, SfrDisk[0]=%g\n", copy_mvir, copy_type, copy_sfr);
    } else {
        printf("  Copy operation failed\n");
    }
    
    // Clean up copy
    free_galaxy_properties(&copy_galaxy);
    
    // Test reset functionality
    reset_galaxy_properties(test_ctx.test_galaxy);
    
    float reset_mvir = GALAXY_PROP_Mvir(test_ctx.test_galaxy);
    int32_t reset_type = GALAXY_PROP_Type(test_ctx.test_galaxy);
    
    TEST_ASSERT(reset_mvir == 0.0f, "Reset Mvir should be default value");
    TEST_ASSERT(reset_type == 0, "Reset Type should be default value");
    
    printf("  Reset validation: Mvir=%g, Type=%d\n", reset_mvir, reset_type);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc;  // Suppress unused parameter warning
    (void)argv;  // Suppress unused parameter warning
    
    printf("\n========================================\n");
    printf("Starting tests for test_property_array_access\n");
    printf("========================================\n\n");
    
    printf("This test verifies that:\n");
    printf("  1. Property accessor functions correctly access all property types\n");
    printf("  2. Core properties use direct macro access (GALAXY_PROP_*)\n");
    printf("  3. Physics properties use only generic accessor functions\n");
    printf("  4. Fixed and dynamic arrays work correctly\n");
    printf("  5. Error handling works for invalid inputs\n");
    printf("  6. Property system lifecycle functions work correctly\n");
    printf("  7. Memory management is handled properly\n");
    printf("  8. Core-physics separation principles are properly demonstrated\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_property_initialisation();
    test_scalar_property_access();
    test_fixed_array_access();
    test_dynamic_array_access();
    test_error_handling();
    test_edge_cases();
    test_property_metadata();
    test_memory_management();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_property_array_access:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
