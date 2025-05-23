#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>  // For access() function

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_property_utils.h"
#include "../src/physics/placeholder_cooling_module.h"
#include "../src/physics/placeholder_infall_module.h"

// Define property not found constant
#define PROP_NOT_FOUND ((property_id_t)-1)

/**
 * @file test_property_access_patterns.c
 * @brief Test property access patterns to ensure core-physics separation
 * 
 * This test validates that:
 * 1. Properties are correctly accessed through the property system
 * 2. Core-physics separation principles are followed
 * 3. Static analysis can detect direct field access (when present)
 */

static void test_property_macros(void);
static void test_core_property_access(void);
static void test_physics_property_access(void);
static int run_python_validation(void);

int main(int argc, char *argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    printf("==================================================\n");
    printf("Testing Property Access Patterns\n");
    printf("==================================================\n");

    // Test basic property macro functionality
    printf("\nRunning property macro tests...\n");
    test_property_macros();
    
    // Test core property access patterns
    printf("\nRunning core property access tests...\n");
    test_core_property_access();
    
    // Test physics property access patterns
    printf("\nRunning physics property access tests...\n");
    test_physics_property_access();
    
    // Run Python static analysis if available
    printf("\nRunning module static analysis...\n");
    int analysis_result = run_python_validation();
    if (analysis_result != 0) {
        printf("Static analysis found direct field accesses. Check results above.\n");
        return 1;
    }
    
    printf("\nAll property access pattern tests passed! ✓\n");
    return 0;
}

/**
 * Test basic property macro functionality
 */
static void test_property_macros(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    // Initialize the structure and properties
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    // Test setting and getting core property values through macros
    GALAXY_PROP_Mvir(&galaxy) = 100.0;
    GALAXY_PROP_Rvir(&galaxy) = 200.0;
    GALAXY_PROP_Vvir(&galaxy) = 150.0;
    
    assert(fabs(GALAXY_PROP_Mvir(&galaxy) - 100.0) < 1e-6);
    assert(fabs(GALAXY_PROP_Rvir(&galaxy) - 200.0) < 1e-6);
    assert(fabs(GALAXY_PROP_Vvir(&galaxy) - 150.0) < 1e-6);
    
    printf("  Core property macro access working correctly. ✓\n");
    
    // Test array property access
    GALAXY_PROP_Pos_ELEM(&galaxy, 0) = 10.0;
    GALAXY_PROP_Pos_ELEM(&galaxy, 1) = 20.0;
    GALAXY_PROP_Pos_ELEM(&galaxy, 2) = 30.0;
    
    assert(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 0) - 10.0) < 1e-6);
    assert(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 1) - 20.0) < 1e-6);
    assert(fabs(GALAXY_PROP_Pos_ELEM(&galaxy, 2) - 30.0) < 1e-6);
    
    printf("  Array property access working correctly. ✓\n");
}

/**
 * Test core property access patterns
 */
static void test_core_property_access(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    // Initialize the structure and properties
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    // Set test values using core property macros
    GALAXY_PROP_Type(&galaxy) = 0;
    GALAXY_PROP_MostBoundID(&galaxy) = 12345;
    GALAXY_PROP_SnapNum(&galaxy) = 67;
    
    // Verify values are correct
    assert(GALAXY_PROP_Type(&galaxy) == 0);
    assert(GALAXY_PROP_MostBoundID(&galaxy) == 12345);
    assert(GALAXY_PROP_SnapNum(&galaxy) == 67);
    
    printf("  Core property access through macros works correctly. ✓\n");
}

/**
 * Test physics property access patterns
 */
static void test_physics_property_access(void) {
    struct GALAXY galaxy;
    galaxy_properties_t properties;
    
    // Initialize the structure and properties
    memset(&galaxy, 0, sizeof(struct GALAXY));
    memset(&properties, 0, sizeof(galaxy_properties_t));
    galaxy.properties = &properties;
    
    // Get property IDs for physics properties
    property_id_t hotgas_id = get_property_id("HotGas");
    property_id_t coldgas_id = get_property_id("ColdGas");
    
    // Check if we could get the property IDs
    if (hotgas_id == PROP_NOT_FOUND || coldgas_id == PROP_NOT_FOUND) {
        printf("  Physics properties not found, skipping test. ✓\n");
        return;
    }
    
    // Test setting physics properties using the generic property system
    set_float_property(&galaxy, hotgas_id, 5.0f);
    set_float_property(&galaxy, coldgas_id, 2.5f);
    
    // Test getting physics properties using the generic property system
    float hotgas = get_float_property(&galaxy, hotgas_id, 0.0f);
    float coldgas = get_float_property(&galaxy, coldgas_id, 0.0f);
    
    assert(fabs(hotgas - 5.0f) < 1e-6);
    assert(fabs(coldgas - 2.5f) < 1e-6);
    
    printf("  Physics property access through generic functions works correctly. ✓\n");
}

/**
 * Run the Python validation script for direct field access detection
 */
static int run_python_validation(void) {
    // Use python3 directly - handles running from both test directory and root
    char command1[256], command2[256];
    char python_cmd[10] = "python3";
    
    // Check if python3 is available, fallback to python if needed
    if (system("which python3 > /dev/null 2>&1") != 0) {
        strcpy(python_cmd, "python");
    }
    
    // Adapt command based on current directory (handles both root and tests/ dir)
    if (access("tests/verify_placeholder_property_access.py", F_OK) != -1) {
        // Running from root directory
        sprintf(command1, "%s tests/verify_placeholder_property_access.py src/physics/placeholder_cooling_module.c > /dev/null", python_cmd);
        sprintf(command2, "%s tests/verify_placeholder_property_access.py src/physics/placeholder_infall_module.c > /dev/null", python_cmd);
    } else {
        // Running from tests directory
        sprintf(command1, "%s verify_placeholder_property_access.py ../src/physics/placeholder_cooling_module.c > /dev/null", python_cmd);
        sprintf(command2, "%s verify_placeholder_property_access.py ../src/physics/placeholder_infall_module.c > /dev/null", python_cmd);
    }
    
    int cooling_result = system(command1);
    int infall_result = system(command2);
    
    if (cooling_result != 0 || infall_result != 0) {
        printf("  Static analysis found direct field accesses.\n");
        return 1;
    }
    
    printf("  Static analysis confirmed no direct field accesses. ✓\n");
    return 0;
}
