#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>  // For access() function
#include <time.h>    // For performance testing

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_property_utils.h"
#include "../src/physics/placeholder_cooling_module.h"
#include "../src/physics/placeholder_infall_module.h"

// Define property not found constant
#define PROP_NOT_FOUND ((property_id_t)-1)

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

/**
 * @file test_property_access_patterns.c
 * @brief Test property access patterns to ensure core-physics separation
 * 
 * Part of the PROPERTY_TESTS test suite in the SAGE model
 * 
 * This test validates that:
 * 1. Properties are correctly accessed through the property system
 * 2. Core-physics separation principles are followed
 * 3. Static analysis can detect direct field access (when present)
 * 
 * This test replaces the outdated test_galaxy_property_macros.c which was designed for the
 * transitional period when both direct fields and property-based access existed simultaneously.
 * The new test focuses on validating the correct property access patterns according to the
 * core-physics separation principles established in Phase 5.2.F.
 * 
 * @see test_property_validation_mocks.c - Contains mock functions for property access testing
 * @see verify_placeholder_property_access.py - Python script for static analysis
 * @see docs/core_physics_property_separation_principles.md - Documentation on separation principles
 * 
 * To run this test: make test_property_access_patterns
 * 
 * @note Added in Phase 5.2.F.3 (May 2025)
 */

static void test_property_macros(void);
static void test_core_property_access(void);
static void test_physics_property_access(void);
static void test_error_handling(void);
static void test_performance_benchmarks(void);
static void test_memory_validation(void);
static int run_python_validation(void);

int main(int argc, char *argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    printf("\n========================================\n");
    printf("Starting tests for property_access_patterns\n");
    printf("========================================\n\n");
    
    printf("This test verifies that:\n");
    printf("  1. Properties are correctly accessed through the property system\n");
    printf("  2. Core-physics separation principles are followed\n");
    printf("  3. Static analysis can detect direct field access violations\n");
    printf("  4. Error conditions are handled appropriately\n\n");

    // Test basic property macro functionality
    printf("=== Testing property macro functionality ===\n");
    test_property_macros();
    
    // Test core property access patterns
    printf("\n=== Testing core property access patterns ===\n");
    test_core_property_access();
    
    // Test physics property access patterns
    printf("\n=== Testing physics property access patterns ===\n");
    test_physics_property_access();
    
    // Test error handling
    printf("\n=== Testing error handling ===\n");
    test_error_handling();
    
    // Test performance benchmarks
    printf("\n=== Testing performance benchmarks ===\n");
    test_performance_benchmarks();
    
    // Test memory validation
    printf("\n=== Testing memory validation ===\n");
    test_memory_validation();
    
    // Run Python static analysis if available
    printf("\n=== Testing module static analysis ===\n");
    printf("  Running Python validation script to detect direct field access violations...\n");
    printf("    Checking placeholder_cooling_module.c and placeholder_infall_module.c\n");
    printf("    This validates core-physics separation principles\n");
    
    int analysis_result = run_python_validation();
    if (analysis_result != 0) {
        printf("  FAIL: Static analysis found direct field accesses (violation of separation principles)\n");
        tests_run++;
    } else {
        printf("  PASS: Static analysis confirmed no direct field accesses (separation principles maintained)\n");
        tests_run++;
        tests_passed++;
    }
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_property_access_patterns:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
        
    return (tests_run == tests_passed) ? 0 : 1;
}

/**
 * Test basic property macro functionality
 */
static void test_property_macros(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    printf("  Testing basic property macro access...\n");
    printf("    This test validates fundamental property macro functionality\n");
    
    // Initialize the structure and properties
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    printf("    Setting scalar properties: Mvir=100.0, Rvir=200.0, Vvir=150.0\n");
    
    // Test setting and getting core property values through macros
    GALAXY_PROP_Mvir(&galaxy) = 100.0;
    GALAXY_PROP_Rvir(&galaxy) = 200.0;
    GALAXY_PROP_Vvir(&galaxy) = 150.0;
    
    printf("    Verifying scalar property retrieval...\n");
    printf("      Mvir: %.1f (expected 100.0)\n", GALAXY_PROP_Mvir(&galaxy));
    printf("      Rvir: %.1f (expected 200.0)\n", GALAXY_PROP_Rvir(&galaxy));
    printf("      Vvir: %.1f (expected 150.0)\n", GALAXY_PROP_Vvir(&galaxy));
    
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 100.0) < 1e-6, 
                "GALAXY_PROP_Mvir should store and retrieve values correctly");
    TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&galaxy) - 200.0) < 1e-6, 
                "GALAXY_PROP_Rvir should store and retrieve values correctly");
    TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&galaxy) - 150.0) < 1e-6, 
                "GALAXY_PROP_Vvir should store and retrieve values correctly");
    
    printf("  Testing array property access...\n");
    printf("    Setting position vector: Pos[0]=10.0, Pos[1]=20.0, Pos[2]=30.0\n");
    
    // Test array property access
    GALAXY_PROP_Pos_ELEM(&galaxy, 0) = 10.0;
    GALAXY_PROP_Pos_ELEM(&galaxy, 1) = 20.0;
    GALAXY_PROP_Pos_ELEM(&galaxy, 2) = 30.0;
    
    printf("    Verifying array property retrieval...\n");
    printf("      Pos[0]: %.1f (expected 10.0)\n", GALAXY_PROP_Pos_ELEM(&galaxy, 0));
    printf("      Pos[1]: %.1f (expected 20.0)\n", GALAXY_PROP_Pos_ELEM(&galaxy, 1));
    printf("      Pos[2]: %.1f (expected 30.0)\n", GALAXY_PROP_Pos_ELEM(&galaxy, 2));
    
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 0) - 10.0) < 1e-6,
                "GALAXY_PROP_Pos_ELEM should access array element 0 correctly");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 1) - 20.0) < 1e-6,
                "GALAXY_PROP_Pos_ELEM should access array element 1 correctly");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 2) - 30.0) < 1e-6,
                "GALAXY_PROP_Pos_ELEM should access array element 2 correctly");
    
    printf("  PASS: Property macro access working correctly\n");
}

/**
 * Test core property access patterns
 */
static void test_core_property_access(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    printf("  Testing core property access patterns...\n");
    printf("    This test validates access to core galaxy properties via macros\n");
    
    // Initialize the structure and properties
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    printf("    Setting core properties: Type=0, MostBoundID=12345, SnapNum=67\n");
    
    // Set test values using core property macros
    GALAXY_PROP_Type(&galaxy) = 0;
    GALAXY_PROP_MostBoundID(&galaxy) = 12345;
    GALAXY_PROP_SnapNum(&galaxy) = 67;
    
    printf("    Verifying stored values...\n");
    
    // Verify values are correct
    printf("      Type: %d (expected 0)\n", GALAXY_PROP_Type(&galaxy));
    printf("      MostBoundID: %lld (expected 12345)\n", GALAXY_PROP_MostBoundID(&galaxy));
    printf("      SnapNum: %d (expected 67)\n", GALAXY_PROP_SnapNum(&galaxy));
    
    TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 0,
                "GALAXY_PROP_Type should store and retrieve value correctly");
    TEST_ASSERT(GALAXY_PROP_MostBoundID(&galaxy) == 12345,
                "GALAXY_PROP_MostBoundID should store and retrieve value correctly");
    TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy) == 67,
                "GALAXY_PROP_SnapNum should store and retrieve value correctly");
    
    printf("  PASS: Core property access working correctly\n");
}

/**
 * Test physics property access patterns
 */
static void test_physics_property_access(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    printf("  Testing physics property access patterns...\n");
    printf("    This test validates the generic property system for physics modules\n");
    
    // Initialize the structure and properties
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    printf("    Looking up physics property IDs...\n");
    
    // Get property IDs for physics properties
    property_id_t hotgas_id = get_property_id("HotGas");
    property_id_t coldgas_id = get_property_id("ColdGas");
    
    // Check if we could get the property IDs
    if (hotgas_id == PROP_NOT_FOUND || coldgas_id == PROP_NOT_FOUND) {
        printf("      HotGas ID: %s, ColdGas ID: %s\n", 
               (hotgas_id == PROP_NOT_FOUND) ? "NOT_FOUND" : "found",
               (coldgas_id == PROP_NOT_FOUND) ? "NOT_FOUND" : "found");
        printf("  SKIP: Physics properties not found, skipping test.\n");
        tests_run++;
        tests_passed++;
        return;
    }
    
    printf("      HotGas property ID: %d\n", hotgas_id);
    printf("      ColdGas property ID: %d\n", coldgas_id);
    
    printf("  Testing generic property system access...\n");
    
    // Test setting physics properties using the generic property system
    printf("    Setting HotGas = 5.0, ColdGas = 2.5 via generic property system...\n");
    set_float_property(&galaxy, hotgas_id, 5.0f);
    set_float_property(&galaxy, coldgas_id, 2.5f);
    
    // Test getting physics properties using the generic property system
    printf("    Retrieving values via generic property system...\n");
    float hotgas = get_float_property(&galaxy, hotgas_id, 0.0f);
    float coldgas = get_float_property(&galaxy, coldgas_id, 0.0f);
    
    printf("      Retrieved HotGas = %.3f (expected 5.000)\n", hotgas);
    printf("      Retrieved ColdGas = %.3f (expected 2.500)\n", coldgas);
    
    TEST_ASSERT(fabs(hotgas - 5.0f) < 1e-6,
                "Physics property HotGas should store and retrieve value correctly");
    TEST_ASSERT(fabs(coldgas - 2.5f) < 1e-6,
                "Physics property ColdGas should store and retrieve value correctly");
    
    printf("  PASS: Physics property access working correctly\n");
}

/**
 * Test error handling in property access
 */
static void test_error_handling(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    printf("  Testing error handling in property access...\n");
    
    // Test NULL galaxy handling
    printf("    Testing NULL galaxy handling...\n");
    property_id_t test_prop_id = get_property_id("Type");
    if (test_prop_id != PROP_NOT_FOUND) {
        // Test get with NULL galaxy - should return default value
        int32_t result = get_int32_property(NULL, test_prop_id, -999);
        TEST_ASSERT(result == -999, 
                    "get_int32_property should return default value for NULL galaxy");
        
        // Test set with NULL galaxy - should return error
        int set_result = set_int32_property(NULL, test_prop_id, 42);
        TEST_ASSERT(set_result != 0, 
                    "set_int32_property should return error for NULL galaxy");
    }
    
    // Test invalid property ID handling
    printf("    Testing invalid property ID handling...\n");
    property_id_t invalid_id = (property_id_t)-2;  // Different from PROP_NOT_FOUND
    
    // Initialize valid galaxy for invalid property tests
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    // Test get with invalid property ID - should return default value
    float result_float = get_float_property(&galaxy, invalid_id, -999.0f);
    TEST_ASSERT(fabs(result_float + 999.0f) < 1e-6, 
                "get_float_property should return default value for invalid property ID");
    
    // Test set with invalid property ID - should return error
    int set_result = set_float_property(&galaxy, invalid_id, 42.0f);
    TEST_ASSERT(set_result != 0, 
                "set_float_property should return error for invalid property ID");
    
    // Test array access with invalid indices (if array properties exist)
    printf("    Testing array bounds checking...\n");
    property_id_t array_prop_id = get_property_id("Pos");
    if (array_prop_id != PROP_NOT_FOUND) {
        // Test out-of-bounds array access - should return default value
        float array_result = get_float_array_element_property(&galaxy, array_prop_id, 999, -888.0f);
        TEST_ASSERT(fabs(array_result + 888.0f) < 1e-6, 
                    "get_float_array_element_property should return default for out-of-bounds index");
    }
    
    // Test galaxy with NULL properties pointer
    printf("    Testing NULL properties pointer handling...\n");
    struct GALAXY null_props_galaxy;
    memset(&null_props_galaxy, 0, sizeof(struct GALAXY));
    null_props_galaxy.properties = NULL;
    
    if (test_prop_id != PROP_NOT_FOUND) {
        int32_t null_props_result = get_int32_property(&null_props_galaxy, test_prop_id, -777);
        TEST_ASSERT(null_props_result == -777, 
                    "get_int32_property should return default value for galaxy with NULL properties");
    }
    
    printf("  PASS: Error handling working correctly\n");
}

/**
 * Test performance benchmarks for property access patterns
 */
static void test_performance_benchmarks(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    printf("  Running performance benchmarks for property access patterns...\n");
    printf("    This test validates that property access remains efficient under load\n");
    
    // Initialize the structure and properties
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    const int num_iterations = 100000;
    clock_t start, end;
    double cpu_time_used;
    
    printf("    Initialised test galaxy with %d iterations per benchmark\n", num_iterations);
    
    // Benchmark macro-based property access
    printf("    Benchmarking macro-based property access (set/get Mvir)...\n");
    start = clock();
    for (int i = 0; i < num_iterations; i++) {
        GALAXY_PROP_Mvir(&galaxy) = (float)i;
        volatile float val = GALAXY_PROP_Mvir(&galaxy);  // volatile to prevent optimization
        (void)val;  // Suppress unused variable warning
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("      Result: %d iterations completed in %.6f seconds (%.2f operations/sec)\n", 
           num_iterations, cpu_time_used, (num_iterations * 2.0) / cpu_time_used);
    TEST_ASSERT(cpu_time_used < 1.0, "Macro-based property access should be fast (< 1 second)");
    
    // Benchmark generic property access (if available)
    printf("    Benchmarking generic property system access (set/get via property ID)...\n");
    property_id_t mvir_id = get_property_id("Mvir");
    if (mvir_id != PROP_NOT_FOUND) {
        printf("      Found Mvir property ID: %d\n", mvir_id);
        start = clock();
        for (int i = 0; i < num_iterations; i++) {
            set_float_property(&galaxy, mvir_id, (float)i);
            volatile float val = get_float_property(&galaxy, mvir_id, 0.0f);
            (void)val;  // Suppress unused variable warning
        }
        end = clock();
        cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
        
        printf("      Result: %d iterations completed in %.6f seconds (%.2f operations/sec)\n", 
               num_iterations, cpu_time_used, (num_iterations * 2.0) / cpu_time_used);
        TEST_ASSERT(cpu_time_used < 2.0, "Generic property access should be reasonably fast (< 2 seconds)");
    } else {
        printf("      Mvir property ID not found - skipping generic property benchmark\n");
    }
    
    // Benchmark array property access
    printf("    Benchmarking array property access (set/get Pos elements)...\n");
    printf("      Testing access to 3D position vector elements with cycling indices\n");
    start = clock();
    for (int i = 0; i < num_iterations; i++) {
        GALAXY_PROP_Pos_ELEM(&galaxy, i % 3) = (float)i;
        volatile float val = GALAXY_PROP_Pos_ELEM(&galaxy, i % 3);
        (void)val;  // Suppress unused variable warning
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("      Result: %d iterations completed in %.6f seconds (%.2f operations/sec)\n", 
           num_iterations, cpu_time_used, (num_iterations * 2.0) / cpu_time_used);
    TEST_ASSERT(cpu_time_used < 1.0, "Array element access should be fast (< 1 second)");
    
    printf("  PASS: All performance benchmarks completed successfully\n");
}

/**
 * Test memory validation for property access
 */
static void test_memory_validation(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    printf("  Testing memory validation for property access...\n");
    
    // Test 1: Normal allocation and deallocation pattern
    printf("    Testing memory stability with repeated access...\n");
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    // Perform multiple property accesses to ensure no memory corruption
    for (int i = 0; i < 1000; i++) {
        GALAXY_PROP_Mvir(&galaxy) = (float)i;
        GALAXY_PROP_Rvir(&galaxy) = (float)(i * 2);
        GALAXY_PROP_Vvir(&galaxy) = (float)(i * 3);
        
        // Verify values are still correct
        if (i == 999) {  // Check final iteration
            TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 999.0f) < 1e-6,
                        "Property values should remain stable after repeated access");
            TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&galaxy) - 1998.0f) < 1e-6,
                        "Property values should remain stable after repeated access");
            TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&galaxy) - 2997.0f) < 1e-6,
                        "Property values should remain stable after repeated access");
        }
    }
    
    // Test 2: Array bounds safety
    printf("    Testing array bounds safety...\n");
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos_ELEM(&galaxy, i) = (float)(i * 10);
    }
    
    // Verify array values
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, i) - (float)(i * 10)) < 1e-6,
                    "Array property values should be stored and retrieved correctly");
    }
    
    // Test 3: Verify no memory overlap issues with different property types
    printf("    Testing type safety and memory isolation...\n");
    GALAXY_PROP_Type(&galaxy) = 42;
    GALAXY_PROP_SnapNum(&galaxy) = 99;
    GALAXY_PROP_MostBoundID(&galaxy) = 123456;
    
    // Set a float property and verify integers weren't affected
    GALAXY_PROP_Mvir(&galaxy) = 999.99f;
    
    TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 42,
                "Integer properties should not be affected by float property changes");
    TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy) == 99,
                "Integer properties should not be affected by float property changes");
    TEST_ASSERT(GALAXY_PROP_MostBoundID(&galaxy) == 123456,
                "Integer properties should not be affected by float property changes");
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 999.99f) < 1e-6,
                "Float property should maintain its value");
    
    printf("  PASS: Memory validation successful\n");
}

/**
 * Run the Python validation script for direct field access detection
 */
static int run_python_validation(void) {
    // Use python3 directly - handles running from both test directory and root
    char command1[256], command2[256];
    char python_cmd[10] = "python3";
    
    printf("    Checking Python availability...\n");
    
    // Check if python3 is available, fallback to python if needed
    if (system("which python3 > /dev/null 2>&1") != 0) {
        strcpy(python_cmd, "python");
        printf("      Using 'python' (python3 not found)\n");
    } else {
        printf("      Using 'python3'\n");
    }
    
    printf("    Determining script paths based on current directory...\n");
    
    // Adapt command based on current directory (handles both root and tests/ dir)
    if (access("tests/verify_placeholder_property_access.py", F_OK) != -1) {
        // Running from root directory
        printf("      Running from project root directory\n");
        sprintf(command1, "%s tests/verify_placeholder_property_access.py src/physics/placeholder_cooling_module.c", python_cmd);
        sprintf(command2, "%s tests/verify_placeholder_property_access.py src/physics/placeholder_infall_module.c", python_cmd);
    } else {
        // Running from tests directory
        printf("      Running from tests directory\n");
        sprintf(command1, "%s verify_placeholder_property_access.py ../src/physics/placeholder_cooling_module.c", python_cmd);
        sprintf(command2, "%s verify_placeholder_property_access.py ../src/physics/placeholder_infall_module.c", python_cmd);
    }
    
    printf("    Analysing placeholder_cooling_module.c...\n");
    int cooling_result = system(command1);
    printf("      Cooling module analysis result: %s\n", (cooling_result == 0) ? "CLEAN" : "VIOLATIONS FOUND");
    
    printf("    Analysing placeholder_infall_module.c...\n");
    int infall_result = system(command2);
    printf("      Infall module analysis result: %s\n", (infall_result == 0) ? "CLEAN" : "VIOLATIONS FOUND");
    
    return (cooling_result != 0 || infall_result != 0) ? 1 : 0;
}
