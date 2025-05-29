/**
 * Comprehensive Core Property System Test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "core_allvars.h"
#include "core_properties.h"
#include "core_property_utils.h"
#include "core_mymalloc.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    printf("\n========================================\n");
    printf("Starting comprehensive SAGE Core Property System tests\n");
    printf("========================================\n\n");
    
    // Test 1: Property enumeration and constants
    printf("=== Testing property enumeration ===\n");
    TEST_ASSERT(PROP_COUNT > 0, "PROP_COUNT should be positive");
    TEST_ASSERT(PROP_COUNT < 1000, "PROP_COUNT should be reasonable");
    TEST_ASSERT(PROP_SnapNum >= 0, "PROP_SnapNum should be valid");
    TEST_ASSERT(PROP_Mvir >= 0, "PROP_Mvir should be valid");
    TEST_ASSERT(PROP_COUNT > PROP_Mvir, "PROP_Mvir should be less than PROP_COUNT");
    
    // Test 2: Property name access
    printf("=== Testing property name access ===\n");
    const char *name = get_property_name(PROP_Mvir);
    TEST_ASSERT(name != NULL, "get_property_name should return non-NULL for valid property");
    TEST_ASSERT(strcmp(name, "Mvir") == 0, "get_property_name should return correct name");
    
    const char *invalid_name = get_property_name(PROP_COUNT);
    TEST_ASSERT(invalid_name == NULL, "get_property_name should return NULL for invalid ID");
    
    // Test 3: Property ID lookup
    printf("=== Testing property ID lookup ===\n");
    property_id_t mvir_id = get_property_id("Mvir");
    TEST_ASSERT(mvir_id == PROP_Mvir, "get_property_id should return correct ID for Mvir");
    
    property_id_t invalid_id = get_property_id("NonExistentProperty");
    TEST_ASSERT(invalid_id == PROP_COUNT, "get_property_id should return PROP_COUNT for invalid property");
    
    // Test 4: Cached property ID lookup
    printf("=== Testing cached property ID lookup ===\n");
    property_id_t cached_mvir_id = get_cached_property_id("Mvir");
    TEST_ASSERT(cached_mvir_id == PROP_Mvir, "get_cached_property_id should return correct ID for Mvir");
    
    property_id_t cached_invalid_id = get_cached_property_id("NonExistentProperty");
    TEST_ASSERT(cached_invalid_id == PROP_COUNT, "get_cached_property_id should return PROP_COUNT for invalid property");
    
    // Test 5: GALAXY_PROP_* macro availability
    printf("=== Testing GALAXY_PROP_* macro availability ===\n");
    struct GALAXY test_galaxy;
    galaxy_properties_t test_properties;
    
    // Initialize the galaxy to point to our test properties
    memset(&test_galaxy, 0, sizeof(test_galaxy));
    memset(&test_properties, 0, sizeof(test_properties));
    test_galaxy.properties = &test_properties;
    
    // Test that GALAXY_PROP_* macros compile and work
    GALAXY_PROP_SnapNum(&test_galaxy) = 42;
    TEST_ASSERT(GALAXY_PROP_SnapNum(&test_galaxy) == 42, "GALAXY_PROP_SnapNum macro should work");
    
    GALAXY_PROP_Type(&test_galaxy) = 1;
    TEST_ASSERT(GALAXY_PROP_Type(&test_galaxy) == 1, "GALAXY_PROP_Type macro should work");
    
    GALAXY_PROP_Mvir(&test_galaxy) = 1.5e12f;
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&test_galaxy) - 1.5e12f) < 1e6f, "GALAXY_PROP_Mvir macro should work");
    
    // Test array properties
    GALAXY_PROP_Pos_ELEM(&test_galaxy, 0) = 100.0f;
    GALAXY_PROP_Pos_ELEM(&test_galaxy, 1) = 200.0f;
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&test_galaxy, 0) - 100.0f) < 1e-6f, "GALAXY_PROP_Pos_ELEM should work");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(&test_galaxy, 1) - 200.0f) < 1e-6f, "GALAXY_PROP_Pos_ELEM should work");
    
    // Test 6: Core-physics separation compliance
    printf("=== Testing core-physics separation compliance ===\n");
    TEST_ASSERT(PROP_SnapNum < PROP_COUNT, "Core property SnapNum should be defined");
    TEST_ASSERT(PROP_Type < PROP_COUNT, "Core property Type should be defined");
    TEST_ASSERT(PROP_GalaxyNr < PROP_COUNT, "Core property GalaxyNr should be defined");
    TEST_ASSERT(PROP_HaloNr < PROP_COUNT, "Core property HaloNr should be defined");
    TEST_ASSERT(PROP_Mvir < PROP_COUNT, "Halo property Mvir should be defined");
    TEST_ASSERT(PROP_Rvir < PROP_COUNT, "Halo property Rvir should be defined");
    TEST_ASSERT(PROP_Vvir < PROP_COUNT, "Halo property Vvir should be defined");
    
    // Test that we can get names for these properties without physics dependencies
    const char *snapnum_name = get_property_name(PROP_SnapNum);
    const char *mvir_name = get_property_name(PROP_Mvir);
    TEST_ASSERT(snapnum_name != NULL, "Core property names should be accessible");
    TEST_ASSERT(mvir_name != NULL, "Halo property names should be accessible");
    
    // Test 7: Property system type definitions
    printf("=== Testing property type definitions ===\n");
    property_id_t test_id = PROP_SnapNum;
    TEST_ASSERT(test_id >= 0, "property_id_t should be usable");
    
    // Test that galaxy_properties_t is defined and can be used
    galaxy_properties_t test_props;
    memset(&test_props, 0, sizeof(test_props));
    test_props.SnapNum = 10;
    TEST_ASSERT(test_props.SnapNum == 10, "galaxy_properties_t should be usable");
    
    // Test that GALAXY structure is compatible with properties
    struct GALAXY test_galaxy2;
    memset(&test_galaxy2, 0, sizeof(test_galaxy2));
    test_galaxy2.properties = &test_props;
    TEST_ASSERT(test_galaxy2.properties != NULL, "GALAXY should have properties pointer");
    TEST_ASSERT(test_galaxy2.properties->SnapNum == 10, "GALAXY should access properties correctly");
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for SAGE Core Property System:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
