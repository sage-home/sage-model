// filepath: tests/test_property_system.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>    // For int32_t

#include "../src/core/core_property_utils.h" // Includes core_allvars.h and core_properties.h for GALAXY

// Dummy property IDs for testing
#define PROP_ID_FLOAT   0
#define PROP_ID_INT32   1
#define PROP_ID_DOUBLE  2

// Set up a mock galaxy->properties buffer large enough for our tests
void ensure_properties(struct GALAXY *g) {
    if (!g->properties) {
        g->properties = calloc(1, 256); // Enough for our mock offsets
        assert(g->properties != NULL);
        g->GalaxyIndex = 12345;
    }
}

// Mock setters: write directly into properties buffer at fixed offsets
void set_mock_physics_property_float(struct GALAXY *g, property_id_t pid, float value) {
    ensure_properties(g);
    float *buf = (float *)g->properties;
    buf[pid] = value;
}

void set_mock_physics_property_int32(struct GALAXY *g, property_id_t pid, int32_t value) {
    ensure_properties(g);
    int32_t *buf = (int32_t *)g->properties;
    buf[pid] = value;
}

void set_mock_physics_property_double(struct GALAXY *g, property_id_t pid, double value) {
    ensure_properties(g);
    double *buf = (double *)g->properties;
    buf[pid] = value;
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

int main(void) {
    printf("Running generic property system tests...\n");
    test_physics_property_access_float();
    test_physics_property_access_int32();
    test_physics_property_access_double();
    test_has_property();
    printf("All tests PASSED\n");
    return EXIT_SUCCESS;
}

