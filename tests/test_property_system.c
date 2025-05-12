// filepath: tests/test_property_system.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>    // For int32_t

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h" // For GALAXY_PROP_* macros 
#include "../src/core/core_property_utils.h" // For generic property accessors

// Dummy property IDs for testing
#define PROP_ID_FLOAT   0
#define PROP_ID_INT32   1
#define PROP_ID_DOUBLE  2

// Initialize galaxy properties for testing
void allocate_galaxy_properties(struct GALAXY *galaxy) {
    if (!galaxy->properties) {
        galaxy->properties = calloc(1, sizeof(galaxy_properties_t));
        assert(galaxy->properties != NULL);
        galaxy->GalaxyIndex = 12345;
    }
}

// Reset galaxy properties to zero
void reset_galaxy_properties(struct GALAXY *galaxy) {
    if (galaxy->properties) {
        memset(galaxy->properties, 0, sizeof(galaxy_properties_t));
    }
}

// Mock setters: write directly into properties buffer at fixed offsets for physics properties
void set_mock_physics_property_float(struct GALAXY *g, property_id_t pid, float value) {
    allocate_galaxy_properties(g);
    float *buf = (float *)g->properties;
    buf[pid] = value;
}

void set_mock_physics_property_int32(struct GALAXY *g, property_id_t pid, int32_t value) {
    allocate_galaxy_properties(g);
    int32_t *buf = (int32_t *)g->properties;
    buf[pid] = value;
}

void set_mock_physics_property_double(struct GALAXY *g, property_id_t pid, double value) {
    allocate_galaxy_properties(g);
    double *buf = (double *)g->properties;
    buf[pid] = value;
}

// Test core property access using GALAXY_PROP_* macros
void test_core_property_access(void) {
    struct GALAXY mock_galaxy = {0};
    
    // Initialize properties
    allocate_galaxy_properties(&mock_galaxy);
    reset_galaxy_properties(&mock_galaxy);
    
    // Test setting/getting Type (core property)
    GALAXY_PROP_Type(&mock_galaxy) = 1;
    assert(GALAXY_PROP_Type(&mock_galaxy) == 1);
    
    // Test setting/getting Mvir (core property)
    GALAXY_PROP_Mvir(&mock_galaxy) = 1e10;
    assert(GALAXY_PROP_Mvir(&mock_galaxy) == 1e10);
    
    // Test setting/getting Rvir (core property)
    GALAXY_PROP_Rvir(&mock_galaxy) = 200.5;
    assert(GALAXY_PROP_Rvir(&mock_galaxy) == 200.5);
    
    // Test array elements (core property)
    GALAXY_PROP_Pos_ELEM(&mock_galaxy, 0) = 10.5;
    GALAXY_PROP_Pos_ELEM(&mock_galaxy, 1) = 20.5;
    GALAXY_PROP_Pos_ELEM(&mock_galaxy, 2) = 30.5;
    
    assert(GALAXY_PROP_Pos_ELEM(&mock_galaxy, 0) == 10.5);
    assert(GALAXY_PROP_Pos_ELEM(&mock_galaxy, 1) == 20.5);
    assert(GALAXY_PROP_Pos_ELEM(&mock_galaxy, 2) == 30.5);
    
    free(mock_galaxy.properties);
    printf("test_core_property_access PASSED\n");
}

void test_physics_property_access_float(void) {
    struct GALAXY gal = {0};
    float test_val = 3.14f;
    set_mock_physics_property_float(&gal, PROP_ID_FLOAT, test_val);

    float out = get_float_property(&gal, PROP_ID_FLOAT, -1.0f);
    assert(fabs(out - test_val) < 1e-6f);

    float def = get_float_property(&gal, 99, -2.5f);
    assert(fabs(def + 2.5f) < 1e-6f);
    free(gal.properties);
    printf("test_physics_property_access_float PASSED\n");
}

void test_physics_property_access_int32(void) {
    struct GALAXY gal = {0};
    int32_t test_val = 42;
    set_mock_physics_property_int32(&gal, PROP_ID_INT32, test_val);

    int32_t out = get_int32_property(&gal, PROP_ID_INT32, -1);
    assert(out == test_val);

    int32_t def = get_int32_property(&gal, 99, -7);
    assert(def == -7);
    free(gal.properties);
    printf("test_physics_property_access_int32 PASSED\n");
}

void test_physics_property_access_double(void) {
    struct GALAXY gal = {0};
    double test_val = 6.28;
    set_mock_physics_property_double(&gal, PROP_ID_DOUBLE, test_val);

    double out = get_double_property(&gal, PROP_ID_DOUBLE, -1.0);
    assert(fabs(out - test_val) < 1e-12);

    double def = get_double_property(&gal, 99, -3.5);
    assert(fabs(def + 3.5) < 1e-12);
    free(gal.properties);
    printf("test_physics_property_access_double PASSED\n");
}

void test_has_property(void) {
    struct GALAXY gal = {0};
    assert(has_property(&gal, PROP_ID_FLOAT) == false);
    set_mock_physics_property_float(&gal, PROP_ID_FLOAT, 1.0f);
    assert(has_property(&gal, PROP_ID_FLOAT) == true);
    assert(has_property(&gal, 99) == false);
    free(gal.properties);
    printf("test_has_property PASSED\n");
}

// Test interaction between core and physics properties
void test_property_system_integration(void) {
    struct GALAXY gal = {0};
    
    // Initialize properties
    allocate_galaxy_properties(&gal);
    reset_galaxy_properties(&gal);
    
    // Set core properties using GALAXY_PROP_* macros
    GALAXY_PROP_Mvir(&gal) = 1e12;
    GALAXY_PROP_Type(&gal) = 0;
    
    // Set physics properties using the generic system
    set_mock_physics_property_float(&gal, PROP_ID_FLOAT, 5.0f);
    
    // Verify all properties can be accessed correctly
    assert(GALAXY_PROP_Mvir(&gal) == 1e12);
    assert(GALAXY_PROP_Type(&gal) == 0);
    assert(get_float_property(&gal, PROP_ID_FLOAT, 0.0f) == 5.0f);
    
    free(gal.properties);
    printf("test_property_system_integration PASSED\n");
}

int main(void) {
    printf("Running property system tests...\n");
    
    // Test core property access
    test_core_property_access();
    
    // Test physics property access
    test_physics_property_access_float();
    test_physics_property_access_int32();
    test_physics_property_access_double();
    test_has_property();
    
    // Test integration between core and physics properties
    test_property_system_integration();
    
    printf("All property system tests PASSED\n");
    return EXIT_SUCCESS;
}

