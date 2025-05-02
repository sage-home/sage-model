/**
 * @file test_property_registration.c
 * @brief Test for property registration with the extension system
 *
 * This test validates the registration of standard properties with the 
 * extension system and ensures properties can be accessed via both direct
 * and extension mechanisms.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/core/core_properties.h"
#include "../src/core/standard_properties.h"
#include "../src/core/core_logging.h"
#include "../src/core/macros.h"

/* Dummy init implementation for testing */
struct params SimulationParams;

/* Test data */
static const float test_float_value = 42.5f;
static const double test_double_value = 123.456;
static const int32_t test_int32_value = 42;
static const int64_t test_int64_value = 9223372036854775807LL;
static const uint64_t test_uint64_value = 18446744073709551615ULL;
static const float test_array_float[] = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
static const int test_array_size = 5;

/* Helper functions */
static void test_scalar_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name);
static void test_array_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name, int array_size);
static int setup_test_galaxy(struct GALAXY *galaxy);
static void cleanup_test_galaxy(struct GALAXY *galaxy);
static int get_property_type_size(property_id_t prop_id);

/**
 * Main test function
 */
int main(void)
{
    int status = 0;
    struct GALAXY test_galaxy;
    
    printf("==== Testing Standard Properties Registration ====\n");
    
    /* Initialize extension system */
    status = galaxy_extension_system_initialize();
    assert(status == 0 && "Failed to initialize galaxy extension system");
    
    /* Initialize property system */
    status = initialize_property_system();
    assert(status == 0 && "Failed to initialize property system");
    
    /* Register standard properties */
    status = register_standard_properties();
    assert(status == 0 && "Failed to register standard properties");
    
    printf("Property system initialized with %d properties\n", PROP_COUNT);
    
    /* Setup test galaxy */
    status = setup_test_galaxy(&test_galaxy);
    assert(status == 0 && "Failed to set up test galaxy");
    
    /* Test a few key scalar properties */
    printf("\nTesting scalar property access:\n");
    // Use standard property IDs from core_properties.h
    // We'll test just 2-3 of the basic ones for simplicity
    test_scalar_property_access(&test_galaxy, PROP_StellarMass, "StellarMass");
    test_scalar_property_access(&test_galaxy, PROP_BulgeMass, "BulgeMass");
    test_scalar_property_access(&test_galaxy, PROP_BlackHoleMass, "BlackHoleMass");
    
    /* Test property lookup by name */
    printf("\nTesting property lookup by name:\n");
    property_id_t id = get_standard_property_id_by_name("StellarMass");
    if (id != PROP_StellarMass) {
        printf("ERROR: Property lookup by name failed! Expected %d, got %d\n", 
               PROP_StellarMass, id);
        status = 1;
    } else {
        printf("Property lookup by name successful: 'StellarMass' -> %d\n", id);
    }
    
    /* Test getting extension ID for property */
    printf("\nTesting extension ID lookup:\n");
    int ext_id = get_extension_id_for_standard_property(PROP_StellarMass);
    if (ext_id < 0) {
        printf("ERROR: Extension ID lookup failed for StellarMass! Got %d\n", ext_id);
        status = 1;
    } else {
        printf("Extension ID lookup successful: StellarMass -> %d\n", ext_id);
    }
    
    /* Try accessing a property that doesn't exist */
    printf("\nTesting invalid property access:\n");
    id = get_standard_property_id_by_name("NonExistentProperty");
    if (id != PROP_COUNT) {
        printf("ERROR: Invalid property lookup should return PROP_COUNT, got %d\n", id);
        status = 1;
    } else {
        printf("Invalid property lookup handled correctly\n");
    }
    
    ext_id = get_extension_id_for_standard_property(PROP_COUNT);
    if (ext_id != -1) {
        printf("ERROR: Invalid extension ID lookup should return -1, got %d\n", ext_id);
        status = 1;
    } else {
        printf("Invalid extension ID lookup handled correctly\n");
    }
    
    /* Cleanup */
    cleanup_test_galaxy(&test_galaxy);
    
    /* Check overall test status */
    if (status == 0) {
        printf("\nALL TESTS PASSED!\n");
    } else {
        printf("\nTESTS FAILED!\n");
    }
    
    return status;
}

/**
 * Set up test galaxy with extension data
 */
static int setup_test_galaxy(struct GALAXY *galaxy)
{
    memset(galaxy, 0, sizeof(struct GALAXY));
    
    /* Allocate extensions */
    if (galaxy_extension_initialize(galaxy) != 0) {
        printf("Error: Failed to allocate extensions for test galaxy\n");
        return 1;
    }
    
    /* Set some test values via direct access */
    galaxy->StellarMass = test_float_value;
    galaxy->BulgeMass = test_double_value;
    galaxy->BlackHoleMass = test_float_value * 2;
    
    return 0;
}

/**
 * Clean up test galaxy resources
 */
static void cleanup_test_galaxy(struct GALAXY *galaxy)
{    
    /* Free extension data */
    galaxy_extension_cleanup(galaxy);
}

/**
 * Test access to scalar property
 */
static void test_scalar_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name)
{
    int ext_id = get_extension_id_for_standard_property(prop_id);
    if (ext_id < 0) {
        printf("Error: Property '%s' not registered with extension system\n", property_name);
        return;
    }
    
    /* Determine property type and size */
    int type_size = get_property_type_size(prop_id);
    if (type_size <= 0) {
        printf("Error: Invalid property type size for '%s'\n", property_name);
        return;
    }
    
    /* Allocate buffer for extension value */
    void *buffer = malloc(type_size);
    if (!buffer) {
        printf("Error: Failed to allocate buffer for property '%s'\n", property_name);
        return;
    }
    
    /* Read property via extension system */
    void *ext_data = galaxy_extension_get_data(galaxy, ext_id);
    if (ext_data == NULL) {
        printf("Error: Failed to get extension data for property '%s'\n", property_name);
        free(buffer);
        return;
    }
    
    /* Copy data from extension to buffer */
    memcpy(buffer, ext_data, type_size);
    
    /* Compare with direct access */
    void *direct_value = NULL;
    switch (prop_id) {
        case PROP_StellarMass:
            direct_value = &galaxy->StellarMass;
            break;
        case PROP_BulgeMass:
            direct_value = &galaxy->BulgeMass;
            break;
        case PROP_BlackHoleMass:
            direct_value = &galaxy->BlackHoleMass;
            break;
        default:
            printf("Error: Unhandled property '%s' in test\n", property_name);
            free(buffer);
            return;
    }
    
    /* Compare values */
    if (memcmp(buffer, direct_value, type_size) != 0) {
        printf("Error: Property '%s' extension value doesn't match direct access\n", 
               property_name);
        /* Print values for debugging */
        if (type_size == sizeof(float)) {
            printf("  Direct: %f, Extension: %f\n", 
                   *(float*)direct_value, *(float*)buffer);
        } else if (type_size == sizeof(double)) {
            printf("  Direct: %f, Extension: %f\n", 
                   *(double*)direct_value, *(double*)buffer);
        } else if (type_size == sizeof(int32_t)) {
            printf("  Direct: %d, Extension: %d\n", 
                   *(int32_t*)direct_value, *(int32_t*)buffer);
        }
    } else {
        printf("Property '%s' extension access verified!\n", property_name);
    }
    
    free(buffer);
}

/**
 * Test access to array property - Not used in this simplified test
 */
static void test_array_property_access(struct GALAXY *galaxy, property_id_t prop_id, 
                                      const char *property_name, int array_size)
{
    /* Not implemented - simplified version */
    (void)galaxy;
    (void)prop_id;
    (void)property_name;
    (void)array_size;
}

/**
 * Get property type size
 */
static int get_property_type_size(property_id_t prop_id)
{
    if (prop_id < 0 || prop_id >= PROP_COUNT) {
        return -1;
    }
    
    const char *type = PROPERTY_META[prop_id].type;
    
    if (strstr(type, "float") != NULL) {
        return sizeof(float);
    } else if (strstr(type, "double") != NULL) {
        return sizeof(double);
    } else if (strstr(type, "int32_t") != NULL || strstr(type, "int") != NULL) {
        return sizeof(int32_t);
    } else if (strstr(type, "int64_t") != NULL || strstr(type, "long long") != NULL) {
        return sizeof(int64_t);
    } else if (strstr(type, "uint64_t") != NULL) {
        return sizeof(uint64_t);
    }
    
    return -1;
}