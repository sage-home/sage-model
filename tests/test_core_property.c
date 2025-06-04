/**
 * Comprehensive Core Property System Test
 * 
 * Tests cover:
 * - Property enumeration and constants validation
 * - Property name resolution and ID lookup (cached and non-cached)
 * - GALAXY_PROP_* macro functionality and type safety
 * - Core-physics separation compliance
 * - Property system initialization and lifecycle
 * - Error handling and boundary conditions
 * - Dynamic array property access
 * - Multi-galaxy property scenarios
 * - Property metadata and performance validation
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

// Test configuration constants
#define TEST_GALAXY_COUNT 3
#define TEST_SNAPNUM_BASE 42
#define TEST_MVIR_BASE 1.5e12f
#define MAX_REASONABLE_PROP_COUNT 1000
#define MAX_REASONABLE_NAME_LENGTH 64

// Test counter globals
static int tests_run = 0;
static int tests_passed = 0;

// Test context for complex scenarios
static struct test_context {
    struct GALAXY *test_galaxies;
    galaxy_properties_t *test_properties;
    int num_galaxies;
    bool initialized;
} test_ctx;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test setup function
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Allocate test galaxies for multi-galaxy scenarios
    test_ctx.num_galaxies = TEST_GALAXY_COUNT;
    test_ctx.test_galaxies = calloc(test_ctx.num_galaxies, sizeof(struct GALAXY));
    test_ctx.test_properties = calloc(test_ctx.num_galaxies, sizeof(galaxy_properties_t));
    
    if (!test_ctx.test_galaxies || !test_ctx.test_properties) {
        return -1; // Memory allocation failed
    }
    
    // Initialize galaxy property pointers
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        test_ctx.test_galaxies[i].properties = &test_ctx.test_properties[i];
        test_ctx.test_galaxies[i].GalaxyIndex = 1000 + i; // Set unique IDs
    }
    
    test_ctx.initialized = true;
    return 0;
}

// Test teardown function
static void teardown_test_context(void) {
    if (test_ctx.test_galaxies) {
        free(test_ctx.test_galaxies);
        test_ctx.test_galaxies = NULL;
    }
    if (test_ctx.test_properties) {
        free(test_ctx.test_properties);
        test_ctx.test_properties = NULL;
    }
    test_ctx.initialized = false;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: NULL safety and error boundary conditions
 */
static void test_null_safety_and_boundaries(void) {
    printf("\n=== Testing NULL safety and error boundaries ===\n");
    
    // Test NULL property name handling
    property_id_t null_name_id = get_property_id(NULL);
    TEST_ASSERT(null_name_id == PROP_COUNT, "get_property_id(NULL) should return PROP_COUNT");
    
    property_id_t cached_null_id = get_cached_property_id(NULL);
    TEST_ASSERT(cached_null_id == PROP_COUNT, "get_cached_property_id(NULL) should return PROP_COUNT");
    
    // Test empty string handling
    property_id_t empty_id = get_property_id("");
    TEST_ASSERT(empty_id == PROP_COUNT, "get_property_id(\"\") should return PROP_COUNT");
    
    property_id_t cached_empty_id = get_cached_property_id("");
    TEST_ASSERT(cached_empty_id == PROP_COUNT, "get_cached_property_id(\"\") should return PROP_COUNT");
    
    // Test property name bounds
    const char *invalid_name = get_property_name(PROP_COUNT);
    TEST_ASSERT(invalid_name == NULL, "get_property_name(PROP_COUNT) should return NULL");
    
    const char *negative_name = get_property_name(-1);
    TEST_ASSERT(negative_name == NULL, "get_property_name(-1) should return NULL");
    
    // Test very large property ID
    const char *large_name = get_property_name(999);
    TEST_ASSERT(large_name == NULL, "get_property_name(999) should return NULL");
}

/**
 * Test: Property enumeration and constants validation
 */
static void test_property_enumeration(void) {
    printf("\n=== Testing property enumeration and constants ===\n");
    
    TEST_ASSERT(PROP_COUNT > 0, "PROP_COUNT should be positive");
    TEST_ASSERT(PROP_COUNT < MAX_REASONABLE_PROP_COUNT, "PROP_COUNT should be reasonable");
    TEST_ASSERT(PROP_SnapNum >= 0, "PROP_SnapNum should be valid");
    TEST_ASSERT(PROP_Mvir >= 0, "PROP_Mvir should be valid");
    TEST_ASSERT(PROP_COUNT > PROP_Mvir, "PROP_Mvir should be less than PROP_COUNT");
    
    // Verify core properties are sequential from 0
    TEST_ASSERT(PROP_SnapNum == 0, "PROP_SnapNum should be first property (0)");
    TEST_ASSERT(PROP_Type == 1, "PROP_Type should be second property (1)");
    TEST_ASSERT(PROP_GalaxyNr == 2, "PROP_GalaxyNr should be third property (2)");
}

/**
 * Test: Property name resolution and ID lookup
 */
static void test_property_name_and_id_lookup(void) {
    printf("\n=== Testing property name resolution and ID lookup ===\n");
    
    // Test valid property name access
    const char *mvir_name = get_property_name(PROP_Mvir);
    TEST_ASSERT(mvir_name != NULL, "get_property_name should return non-NULL for valid property");
    TEST_ASSERT(strcmp(mvir_name, "Mvir") == 0, "get_property_name should return correct name");
    
    // Test property ID lookup
    property_id_t mvir_id = get_property_id("Mvir");
    TEST_ASSERT(mvir_id == PROP_Mvir, "get_property_id should return correct ID for Mvir");
    
    property_id_t invalid_id = get_property_id("NonExistentProperty");
    TEST_ASSERT(invalid_id == PROP_COUNT, "get_property_id should return PROP_COUNT for invalid property");
    
    // Test cached property ID lookup
    property_id_t cached_mvir_id = get_cached_property_id("Mvir");
    TEST_ASSERT(cached_mvir_id == PROP_Mvir, "get_cached_property_id should return correct ID for Mvir");
    
    property_id_t cached_invalid_id = get_cached_property_id("NonExistentProperty");
    TEST_ASSERT(cached_invalid_id == PROP_COUNT, "get_cached_property_id should return PROP_COUNT for invalid property");
    
    // Test round-trip consistency (name -> ID -> name)
    for (property_id_t i = 0; i < 5 && i < PROP_COUNT; i++) {
        const char *name = get_property_name(i);
        if (name != NULL) {
            property_id_t id = get_property_id(name);
            TEST_ASSERT(id == i, "Round-trip name->ID->name should be consistent");
        }
    }
}

/**
 * Test: GALAXY_PROP_* macro functionality and array access
 */
static void test_galaxy_prop_macros(void) {
    printf("\n=== Testing GALAXY_PROP_* macro functionality ===\n");
    
    if (!test_ctx.initialized) {
        printf("WARNING: Test context not initialized, skipping macro tests\n");
        return;
    }
    
    struct GALAXY *galaxy = &test_ctx.test_galaxies[0];
    
    // Test scalar property macros
    GALAXY_PROP_SnapNum(galaxy) = TEST_SNAPNUM_BASE;
    TEST_ASSERT(GALAXY_PROP_SnapNum(galaxy) == TEST_SNAPNUM_BASE, "GALAXY_PROP_SnapNum macro should work");
    
    GALAXY_PROP_Type(galaxy) = 1;
    TEST_ASSERT(GALAXY_PROP_Type(galaxy) == 1, "GALAXY_PROP_Type macro should work");
    
    GALAXY_PROP_Mvir(galaxy) = TEST_MVIR_BASE;
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(galaxy) - TEST_MVIR_BASE) < 1e6f, "GALAXY_PROP_Mvir macro should work");
    
    // Test array property element access
    GALAXY_PROP_Pos_ELEM(galaxy, 0) = 100.0f;
    GALAXY_PROP_Pos_ELEM(galaxy, 1) = 200.0f;
    GALAXY_PROP_Pos_ELEM(galaxy, 2) = 300.0f;
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(galaxy, 0) - 100.0f) < 1e-6f, "GALAXY_PROP_Pos_ELEM[0] should work");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(galaxy, 1) - 200.0f) < 1e-6f, "GALAXY_PROP_Pos_ELEM[1] should work");
    TEST_ASSERT(fabs(GALAXY_PROP_Pos_ELEM(galaxy, 2) - 300.0f) < 1e-6f, "GALAXY_PROP_Pos_ELEM[2] should work");
    
    // Test that array access preserves data across different elements
    float sum = GALAXY_PROP_Pos_ELEM(galaxy, 0) + GALAXY_PROP_Pos_ELEM(galaxy, 1) + GALAXY_PROP_Pos_ELEM(galaxy, 2);
    TEST_ASSERT(fabs(sum - 600.0f) < 1e-5f, "Array elements should preserve data independently");
}

/**
 * Test: Core-physics separation compliance
 */
static void test_core_physics_separation(void) {
    printf("\n=== Testing core-physics separation compliance ===\n");
    
    // Test that core infrastructure properties are defined
    TEST_ASSERT(PROP_SnapNum < PROP_COUNT, "Core property SnapNum should be defined");
    TEST_ASSERT(PROP_Type < PROP_COUNT, "Core property Type should be defined");
    TEST_ASSERT(PROP_GalaxyNr < PROP_COUNT, "Core property GalaxyNr should be defined");
    TEST_ASSERT(PROP_HaloNr < PROP_COUNT, "Core property HaloNr should be defined");
    
    // Test that halo properties (needed by core) are defined
    TEST_ASSERT(PROP_Mvir < PROP_COUNT, "Halo property Mvir should be defined");
    TEST_ASSERT(PROP_Rvir < PROP_COUNT, "Halo property Rvir should be defined");
    TEST_ASSERT(PROP_Vvir < PROP_COUNT, "Halo property Vvir should be defined");
    
    // Test that we can get names for these properties without physics dependencies
    const char *snapnum_name = get_property_name(PROP_SnapNum);
    const char *mvir_name = get_property_name(PROP_Mvir);
    TEST_ASSERT(snapnum_name != NULL, "Core property names should be accessible");
    TEST_ASSERT(mvir_name != NULL, "Halo property names should be accessible");
    
    // Verify expected property names
    TEST_ASSERT(strcmp(snapnum_name, "SnapNum") == 0, "SnapNum property should have correct name");
    TEST_ASSERT(strcmp(mvir_name, "Mvir") == 0, "Mvir property should have correct name");
}

/**
 * Test: Multi-galaxy property scenarios
 */
static void test_multi_galaxy_scenarios(void) {
    printf("\n=== Testing multi-galaxy property scenarios ===\n");
    
    if (!test_ctx.initialized || test_ctx.num_galaxies < 2) {
        printf("WARNING: Insufficient test galaxies, skipping multi-galaxy tests\n");
        return;
    }
    
    // Test independent property values across galaxies
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        struct GALAXY *galaxy = &test_ctx.test_galaxies[i];
        GALAXY_PROP_SnapNum(galaxy) = 10 + i;
        GALAXY_PROP_Type(galaxy) = i % 3;
        GALAXY_PROP_Mvir(galaxy) = 1.0e12f * (i + 1);
    }
    
    // Verify each galaxy maintains independent values
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        struct GALAXY *galaxy = &test_ctx.test_galaxies[i];
        TEST_ASSERT(GALAXY_PROP_SnapNum(galaxy) == 10 + i, "Galaxy SnapNum should be independent");
        TEST_ASSERT(GALAXY_PROP_Type(galaxy) == i % 3, "Galaxy Type should be independent");
        TEST_ASSERT(fabs(GALAXY_PROP_Mvir(galaxy) - 1.0e12f * (i + 1)) < 1e6f, "Galaxy Mvir should be independent");
    }
}

/**
 * Test: Property system type definitions and integration
 */
static void test_property_type_definitions(void) {
    printf("\n=== Testing property type definitions and integration ===\n");
    
    // Test that property_id_t is usable
    property_id_t test_id = PROP_SnapNum;
    TEST_ASSERT(test_id >= 0, "property_id_t should be usable");
    TEST_ASSERT(test_id == PROP_SnapNum, "property_id_t should maintain correct values");
    
    // Test that galaxy_properties_t is defined and usable
    galaxy_properties_t test_props;
    memset(&test_props, 0, sizeof(test_props));
    test_props.SnapNum = 10;
    TEST_ASSERT(test_props.SnapNum == 10, "galaxy_properties_t should be usable");
    
    // Test that GALAXY structure is compatible with properties
    struct GALAXY test_galaxy;
    memset(&test_galaxy, 0, sizeof(test_galaxy));
    test_galaxy.properties = &test_props;
    TEST_ASSERT(test_galaxy.properties != NULL, "GALAXY should have properties pointer");
    TEST_ASSERT(test_galaxy.properties->SnapNum == 10, "GALAXY should access properties correctly");
    
    // Test property ID range validation
    TEST_ASSERT(PROP_SnapNum >= 0 && PROP_SnapNum < PROP_COUNT, "PROP_SnapNum should be in valid range");
    TEST_ASSERT(PROP_Mvir >= 0 && PROP_Mvir < PROP_COUNT, "PROP_Mvir should be in valid range");
}

/**
 * Test: Performance validation for cached vs non-cached lookup
 */
static void test_performance_validation(void) {
    printf("\n=== Testing performance validation ===\n");
    
    // Test that cached and non-cached return same results
    const char *test_properties[] = {"Mvir", "Rvir", "Vvir", "SnapNum", "Type"};
    const int num_test_props = sizeof(test_properties) / sizeof(test_properties[0]);
    
    for (int i = 0; i < num_test_props; i++) {
        property_id_t standard_id = get_property_id(test_properties[i]);
        property_id_t cached_id = get_cached_property_id(test_properties[i]);
        TEST_ASSERT(standard_id == cached_id, "Cached and non-cached lookup should return same ID");
    }
    
    // Test multiple cached lookups for consistency
    property_id_t first_lookup = get_cached_property_id("Mvir");
    property_id_t second_lookup = get_cached_property_id("Mvir");
    TEST_ASSERT(first_lookup == second_lookup, "Multiple cached lookups should be consistent");
}

/**
 * Test: Property metadata validation
 */
static void test_property_metadata(void) {
    printf("\n=== Testing property metadata validation ===\n");
    
    // Test that property names are non-empty for valid IDs
    for (property_id_t i = 0; i < 10 && i < PROP_COUNT; i++) {
        const char *name = get_property_name(i);
        TEST_ASSERT(name != NULL, "Property name should not be NULL for valid ID");
        TEST_ASSERT(strlen(name) > 0, "Property name should not be empty");
        TEST_ASSERT(strlen(name) < 64, "Property name should be reasonable length");
    }
    
    // Test that property IDs are consistent
    TEST_ASSERT(PROP_SnapNum == 0, "SnapNum should be first property");
    TEST_ASSERT(PROP_Type > PROP_SnapNum, "Type should come after SnapNum");
    TEST_ASSERT(PROP_GalaxyNr > PROP_Type, "GalaxyNr should come after Type");
    
    // Test property name uniqueness for first few properties
    const char *name1 = get_property_name(PROP_SnapNum);
    const char *name2 = get_property_name(PROP_Type);
    const char *name3 = get_property_name(PROP_Mvir);
    
    TEST_ASSERT(strcmp(name1, name2) != 0, "Property names should be unique");
    TEST_ASSERT(strcmp(name1, name3) != 0, "Property names should be unique");
    TEST_ASSERT(strcmp(name2, name3) != 0, "Property names should be unique");
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    
    printf("\n========================================\n");
    printf("Starting tests for test_core_property\n");
    printf("========================================\n\n");
    
    printf("This test verifies the core property system functionality:\n");
    printf("  1. Property enumeration and constant validation\n");
    printf("  2. Property name resolution and ID lookup (cached and non-cached)\n");
    printf("  3. GALAXY_PROP_* macro compilation and functionality\n");
    printf("  4. Core-physics separation compliance\n");
    printf("  5. Multi-galaxy property independence\n");
    printf("  6. NULL safety and error boundary conditions\n");
    printf("  7. Property type definitions and system integration\n");
    printf("  8. Performance validation (cached vs non-cached lookup)\n");
    printf("  9. Property metadata validation\n\n");

    // Setup test context
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run comprehensive test suite
    test_null_safety_and_boundaries();
    test_property_enumeration();
    test_property_name_and_id_lookup();
    test_galaxy_prop_macros();
    test_core_physics_separation();
    test_multi_galaxy_scenarios();
    test_property_type_definitions();
    test_performance_validation();
    test_property_metadata();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_core_property:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
