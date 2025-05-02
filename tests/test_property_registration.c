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
static const float test_pos_value[3] = {10.0f, 20.0f, 30.0f};
static const float test_sfh_value[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
static const int test_sfh_size = sizeof(test_sfh_value) / sizeof(test_sfh_value[0]);

/* Helper functions */
static void test_scalar_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name);
static void test_fixed_array_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name);
static void test_dynamic_array_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name);
static void test_serialization_functions(void);
static void test_dynamic_array_memory(void);
static int setup_test_galaxy(struct GALAXY *galaxy);
static void cleanup_test_galaxy(struct GALAXY *galaxy);
static int get_property_type_size(property_id_t prop_id);
static void map_standard_properties_to_extensions(struct GALAXY *galaxy);

/**
 * Main test function
 */
int main(void)
{
    int status = 0;
    struct GALAXY test_galaxy;
    
    printf("==== Testing Standard Properties Registration ====\n");
    
    /* Initialize simulation parameters */
    memset(&SimulationParams, 0, sizeof(struct params));
    SimulationParams.simulation.NumSnapOutputs = 5;  // Set expected size for StarFormationHistory
    
    /* Initialize extension system */
    status = galaxy_extension_system_initialize();
    assert(status == 0 && "Failed to initialize galaxy extension system");
    
    /* Initialize property system */
    status = initialize_property_system(&SimulationParams);
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
    
    /* Test fixed-size array property */
    printf("\nTesting fixed-size array property access:\n");
    test_fixed_array_property_access(&test_galaxy, PROP_Pos, "Pos");
    
    /* Test dynamic array property */
    printf("\nTesting dynamic array property access:\n");
    test_dynamic_array_property_access(&test_galaxy, PROP_StarFormationHistory, "StarFormationHistory");
    
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
    
    /* Test serialization functions */
    test_serialization_functions();
    
    /* Test dynamic array memory management */
    test_dynamic_array_memory();
    
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
    
    /* Initialize simulation parameters for consistent sizes */
    SimulationParams.simulation.NumSnapOutputs = 5; // Match test_sfh_size
    
    /* Allocate properties for dynamic arrays with parameters */
    if (allocate_galaxy_properties(galaxy, &SimulationParams) != 0) {
        printf("Error: Failed to allocate galaxy properties\n");
        return 1;
    }
    
    /* Set some test values via direct access */
    galaxy->StellarMass = test_float_value;
    galaxy->BulgeMass = test_double_value;
    galaxy->BlackHoleMass = test_float_value * 2;
    
    /* Initialize Pos fixed array */
    memcpy(galaxy->Pos, test_pos_value, sizeof(test_pos_value));
    
    /* Verify StarFormationHistory was allocated to right size and copy test values */
    assert(galaxy->properties != NULL);
    assert(GALAXY_PROP_StarFormationHistory_SIZE(galaxy) == test_sfh_size);
    assert(galaxy->properties->StarFormationHistory != NULL);
    memcpy(galaxy->properties->StarFormationHistory, test_sfh_value, test_sfh_size * sizeof(float));
    
    /* Map standard properties to extensions */
    map_standard_properties_to_extensions(galaxy);
    
    return 0;
}

/**
 * Clean up test galaxy resources
 */
static void cleanup_test_galaxy(struct GALAXY *galaxy)
{
    /* Clear extension flags for standard properties to avoid double frees */
    int ext_id;
    
    /* StellarMass */
    ext_id = get_extension_id_for_standard_property(PROP_StellarMass);
    if (ext_id >= 0) galaxy->extension_flags &= ~(1ULL << ext_id);
    
    /* BulgeMass */
    ext_id = get_extension_id_for_standard_property(PROP_BulgeMass);
    if (ext_id >= 0) galaxy->extension_flags &= ~(1ULL << ext_id);
    
    /* BlackHoleMass */
    ext_id = get_extension_id_for_standard_property(PROP_BlackHoleMass);
    if (ext_id >= 0) galaxy->extension_flags &= ~(1ULL << ext_id);
    
    /* Pos */
    ext_id = get_extension_id_for_standard_property(PROP_Pos);
    if (ext_id >= 0) galaxy->extension_flags &= ~(1ULL << ext_id);
    
    /* StarFormationHistory */
    ext_id = get_extension_id_for_standard_property(PROP_StarFormationHistory);
    if (ext_id >= 0) galaxy->extension_flags &= ~(1ULL << ext_id);
    
    /* Set all these pointers to NULL to prevent freeing them */
    for (int i = 0; i < galaxy->num_extensions; i++) {
        if (!(galaxy->extension_flags & (1ULL << i))) {
            galaxy->extension_data[i] = NULL;
        }
    }
    
    /* Free galaxy properties (including dynamic arrays) */
    free_galaxy_properties(galaxy);
    
    /* Free extension data */
    galaxy_extension_cleanup(galaxy);
}

/**
 * Map galaxy properties to extension system
 * This function creates the correct mappings between standard properties
 * stored directly in the galaxy struct and their extension system representations.
 */
static void map_standard_properties_to_extensions(struct GALAXY *galaxy)
{
    int ext_id;
    
    /* Map StellarMass (float) */
    ext_id = get_extension_id_for_standard_property(PROP_StellarMass);
    if (ext_id >= 0) {
        galaxy->extension_data[ext_id] = &(galaxy->StellarMass);
        galaxy->extension_flags |= (1ULL << ext_id);
    }
    
    /* Map BulgeMass (double) */
    ext_id = get_extension_id_for_standard_property(PROP_BulgeMass);
    if (ext_id >= 0) {
        galaxy->extension_data[ext_id] = &(galaxy->BulgeMass);
        galaxy->extension_flags |= (1ULL << ext_id);
    }
    
    /* Map BlackHoleMass (float) */
    ext_id = get_extension_id_for_standard_property(PROP_BlackHoleMass);
    if (ext_id >= 0) {
        galaxy->extension_data[ext_id] = &(galaxy->BlackHoleMass);
        galaxy->extension_flags |= (1ULL << ext_id);
    }
    
    /* Map Pos (float[3]) */
    ext_id = get_extension_id_for_standard_property(PROP_Pos);
    if (ext_id >= 0) {
        galaxy->extension_data[ext_id] = galaxy->Pos;
        galaxy->extension_flags |= (1ULL << ext_id);
    }
    
    /* Map StarFormationHistory (float[]) - this one is a pointer */
    ext_id = get_extension_id_for_standard_property(PROP_StarFormationHistory);
    if (ext_id >= 0 && galaxy->properties != NULL) {
        galaxy->extension_data[ext_id] = &(galaxy->properties->StarFormationHistory);
        galaxy->extension_flags |= (1ULL << ext_id);
    }
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
 * Test access to fixed-size array property
 */
static void test_fixed_array_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name)
{
    /* Get extension ID for this property */
    int ext_id = get_extension_id_for_standard_property(prop_id);
    assert(ext_id >= 0 && "Property not registered with extension system");
    
    /* Print property name for clearer test output */
    printf("Testing fixed-size array property: %s\n", property_name);
    
    /* For Pos, we know it's float[3] */
    size_t expected_size = 3 * sizeof(float);
    printf("  Expected array size: %zu bytes (float[3])\n", expected_size);
    
    /* Allocate buffer for the array data */
    void *buffer = malloc(expected_size);
    assert(buffer != NULL && "Failed to allocate buffer");
    
    /* Read property via extension system */
    void *ext_data = galaxy_extension_get_data(galaxy, ext_id);
    assert(ext_data != NULL && "Failed to get extension data");
    
    /* Copy data from extension to buffer */
    memcpy(buffer, ext_data, expected_size);
    
    /* Get direct access pointer based on property ID */
    void *direct_ptr = NULL;
    if (prop_id == PROP_Pos) {
        direct_ptr = galaxy->Pos;
    } else {
        printf("Error: Unsupported fixed array property '%s' in test\n", property_name);
        free(buffer);
        return;
    }
    
    /* Compare values */
    if (memcmp(buffer, direct_ptr, expected_size) != 0) {
        printf("Error: Fixed array property '%s' extension value doesn't match direct access\n", 
               property_name);
        
        /* Print values for debugging */
        float *ext_values = (float*)buffer;
        float *direct_values = (float*)direct_ptr;
        printf("  Direct: [%f, %f, %f], Extension: [%f, %f, %f]\n",
               direct_values[0], direct_values[1], direct_values[2],
               ext_values[0], ext_values[1], ext_values[2]);
    } else {
        /* Print values for verification */
        float *values = (float*)buffer;
        printf("  Array contents verified: [%f, %f, %f]\n", 
               values[0], values[1], values[2]);
        printf("Fixed array property '%s' extension access verified!\n", property_name);
    }
    
    free(buffer);
}

/**
 * Test access to dynamic array property
 */
static void test_dynamic_array_property_access(struct GALAXY *galaxy, property_id_t prop_id, const char *property_name)
{
    /* Get extension ID for this property */
    int ext_id = get_extension_id_for_standard_property(prop_id);
    assert(ext_id >= 0 && "Property not registered with extension system");
    
    /* Print property name for clearer test output */
    printf("Testing dynamic array property: %s\n", property_name);
    
    /* Check the array size */
    int actual_size = 0;
    if (prop_id == PROP_StarFormationHistory) {
        actual_size = GALAXY_PROP_StarFormationHistory_SIZE(galaxy);
        assert(actual_size == test_sfh_size && "StarFormationHistory size mismatch");
        printf("  Array size verified: %d\n", actual_size);
    } else {
        printf("Error: Unsupported dynamic array property '%s' in test\n", property_name);
        return;
    }
    
    /* Get pointer to the array pointer via extension */
    void *ext_ptr_to_array_ptr = galaxy_extension_get_data(galaxy, ext_id);
    assert(ext_ptr_to_array_ptr != NULL && "Failed to get extension data pointer");
    
    /* Dereference to get the actual array pointer */
    float *ext_array_ptr = *(float**)ext_ptr_to_array_ptr;
    assert(ext_array_ptr != NULL && "Dynamic array pointer is NULL");
    
    /* Get direct access pointer */
    float *direct_array_ptr = NULL;
    if (prop_id == PROP_StarFormationHistory) {
        direct_array_ptr = galaxy->properties->StarFormationHistory;
    } else {
        printf("Error: Unsupported dynamic array property '%s' in test\n", property_name);
        return;
    }
    
    /* Compare array pointers - should be the same pointer */
    if (ext_array_ptr != direct_array_ptr) {
        printf("Error: Dynamic array property '%s' pointers don't match\n"
               "  Direct: %p, Extension: %p\n", 
               property_name, (void*)direct_array_ptr, (void*)ext_array_ptr);
        return;
    } else {
        printf("  Array pointers match: %p\n", (void*)ext_array_ptr);
    }
    
    /* Compare array contents */
    if (memcmp(ext_array_ptr, test_sfh_value, actual_size * sizeof(float)) != 0) {
        printf("Error: Dynamic array property '%s' values don't match test data\n", property_name);
        
        /* Print a few values for debugging */
        printf("  Expected: [%f, %f, %f, ...], Actual: [%f, %f, %f, ...]\n",
               test_sfh_value[0], test_sfh_value[1], test_sfh_value[2],
               ext_array_ptr[0], ext_array_ptr[1], ext_array_ptr[2]);
    } else {
        printf("  Array contents verified: [%f, %f, %f, ...]\n", 
               ext_array_ptr[0], ext_array_ptr[1], ext_array_ptr[2]);
        printf("Dynamic array property '%s' extension access verified!\n", property_name);
    }
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

/**
 * Test serialization functions for properties
 */
static void test_serialization_functions(void)
{
    printf("\nTesting serialization functions:\n");
    
    /* Test scalar serialization (StellarMass - float) */
    {
        printf("Testing scalar (float) serialization for StellarMass:\n");
        
        const galaxy_property_t *prop_meta = galaxy_extension_find_property_by_id(PROP_StellarMass);
        assert(prop_meta != NULL && "Failed to find property metadata");
        assert(prop_meta->serialize != NULL && "Serialization function is NULL");
        assert(prop_meta->deserialize != NULL && "Deserialization function is NULL");
        
        float src_val = test_float_value;
        float dest_val = 0.0f;
        float final_val = 0.0f;
        
        /* Serialize */
        prop_meta->serialize(&src_val, &dest_val, 1);
        
        /* Verify serialization */
        if (dest_val != src_val) {
            printf("Error: Serialization failed for StellarMass\n");
            printf("  Source: %f, Serialized: %f\n", src_val, dest_val);
        } else {
            printf("  Serialization successful: %f -> %f\n", src_val, dest_val);
        }
        
        /* Deserialize */
        prop_meta->deserialize(&dest_val, &final_val, 1);
        
        /* Verify deserialization */
        if (final_val != src_val) {
            printf("Error: Deserialization failed for StellarMass\n");
            printf("  Source: %f, Deserialized: %f\n", src_val, final_val);
        } else {
            printf("  Deserialization successful: %f -> %f\n", dest_val, final_val);
        }
    }
    
    /* Test fixed array serialization (Pos - float[3]) */
    {
        printf("Testing fixed array serialization for Pos:\n");
        
        const galaxy_property_t *prop_meta = galaxy_extension_find_property_by_id(PROP_Pos);
        assert(prop_meta != NULL && "Failed to find property metadata");
        assert(prop_meta->serialize != NULL && "Serialization function is NULL");
        assert(prop_meta->deserialize != NULL && "Deserialization function is NULL");
        
        /* Create test arrays */
        float src_arr[3];
        memcpy(src_arr, test_pos_value, sizeof(src_arr));
        float dest_arr[3] = {0};
        float final_arr[3] = {0};
        
        /* Serialize */
        prop_meta->serialize(src_arr, dest_arr, 3);
        
        /* Verify serialization */
        if (memcmp(src_arr, dest_arr, sizeof(src_arr)) != 0) {
            printf("Error: Array serialization failed for Pos\n");
            printf("  Source: [%f, %f, %f], Serialized: [%f, %f, %f]\n",
                   src_arr[0], src_arr[1], src_arr[2],
                   dest_arr[0], dest_arr[1], dest_arr[2]);
        } else {
            printf("  Array serialization successful: [%f, %f, %f]\n", 
                   dest_arr[0], dest_arr[1], dest_arr[2]);
        }
        
        /* Deserialize */
        prop_meta->deserialize(dest_arr, final_arr, 3);
        
        /* Verify deserialization */
        if (memcmp(src_arr, final_arr, sizeof(src_arr)) != 0) {
            printf("Error: Array deserialization failed for Pos\n");
            printf("  Source: [%f, %f, %f], Deserialized: [%f, %f, %f]\n",
                   src_arr[0], src_arr[1], src_arr[2],
                   final_arr[0], final_arr[1], final_arr[2]);
        } else {
            printf("  Array deserialization successful: [%f, %f, %f]\n", 
                   final_arr[0], final_arr[1], final_arr[2]);
        }
    }
}

/**
 * Test dynamic array memory management
 * 
 * This test verifies that the dynamic array memory management works correctly
 * for allocation, reallocation, and deep copying. It specifically tests
 * the parameter-based size determination and proper memory management.
 */
static void test_dynamic_array_memory(void)
{
    printf("\nTesting dynamic array memory management:\n");
    
    // Initialize simulation parameters for testing
    memset(&SimulationParams, 0, sizeof(struct params));
    SimulationParams.simulation.NumSnapOutputs = 10;  // Set expected size for StarFormationHistory
    
    // Create first test galaxy
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(struct GALAXY));
    
    // Allocate extensions and properties
    int status = galaxy_extension_initialize(&galaxy);
    assert(status == 0 && "Failed to initialize galaxy extensions");
    
    // Use the new parameter-based allocation
    status = allocate_galaxy_properties(&galaxy, &SimulationParams);
    assert(status == 0 && "Failed to allocate galaxy properties");
    assert(galaxy.properties != NULL && "Galaxy properties struct not allocated");
    
    // Check StarFormationHistory size is correctly set from parameters
    int expected_size = SimulationParams.simulation.NumSnapOutputs;
    int actual_size = GALAXY_PROP_StarFormationHistory_SIZE(&galaxy);
    printf("  StarFormationHistory array size: %d (expected %d)\n", actual_size, expected_size);
    assert(actual_size == expected_size && "StarFormationHistory size mismatch");
    
    // Check that the array was allocated correctly
    assert(galaxy.properties->StarFormationHistory != NULL && "StarFormationHistory array not allocated");
    printf("  StarFormationHistory array allocated successfully at %p\n", 
           (void*)galaxy.properties->StarFormationHistory);
    
    // Initialize array with test values
    for (int i = 0; i < actual_size; i++) {
        galaxy.properties->StarFormationHistory[i] = i * 0.1f;
    }
    
    // Test array access within bounds
    float test_val = GALAXY_PROP_StarFormationHistory_ELEM(&galaxy, 5);
    printf("  Array element access: index 5 = %f (expected %f)\n", test_val, 5 * 0.1f);
    assert(test_val == 5 * 0.1f && "Array element value incorrect");
    
    // Test array bounds checking (safely, without crashing)
    printf("  Testing safe array access with bounds checking...\n");
    float out_of_bounds_val = GALAXY_PROP_ARRAY_SAFE(&galaxy, StarFormationHistory, actual_size + 5, -1.0f);
    printf("  Out-of-bounds access returned: %f (expected -1.0)\n", out_of_bounds_val);
    assert(out_of_bounds_val == -1.0f && "Safe array accessor failed to handle out-of-bounds access");
    
    // Create second galaxy and test copy functionality
    struct GALAXY galaxy2;
    memset(&galaxy2, 0, sizeof(struct GALAXY));
    
    // Initialize extensions
    status = galaxy_extension_initialize(&galaxy2);
    assert(status == 0 && "Failed to initialize second galaxy extensions");
    
    // Copy properties from galaxy to galaxy2
    status = copy_galaxy_properties(&galaxy2, &galaxy, &SimulationParams);
    assert(status == 0 && "Failed to copy galaxy properties");
    assert(galaxy2.properties != NULL && "Copied galaxy properties struct not allocated");
    
    // Check size and array copying was done correctly
    assert(GALAXY_PROP_StarFormationHistory_SIZE(&galaxy2) == actual_size && 
           "Copied array size mismatch");
    assert(galaxy2.properties->StarFormationHistory != NULL && 
           "StarFormationHistory array not allocated in copied galaxy");
    assert(galaxy2.properties->StarFormationHistory != galaxy.properties->StarFormationHistory &&
           "Arrays should be deep copied, not sharing the same memory");
    
    // Compare array contents
    int content_match = (memcmp(galaxy.properties->StarFormationHistory, 
                               galaxy2.properties->StarFormationHistory, 
                               actual_size * sizeof(float)) == 0);
    printf("  Array contents match after copy: %s\n", content_match ? "Yes" : "No");
    assert(content_match && "Array contents don't match after copy");
    
    // Test array resizing
    int new_size = 20;  // Larger than the original
    printf("  Testing array resizing from %d to %d elements...\n", actual_size, new_size);
    status = galaxy_set_StarFormationHistory_size(&galaxy, new_size);
    assert(status == 0 && "Failed to resize array");
    assert(GALAXY_PROP_StarFormationHistory_SIZE(&galaxy) == new_size && 
           "Array size not updated after resize");
    assert(galaxy.properties->StarFormationHistory != NULL && 
           "StarFormationHistory array not allocated after resize");
    
    // Verify memory is properly initialized to zero
    bool initialized = true;
    for (int i = actual_size; i < new_size; i++) {
        if (galaxy.properties->StarFormationHistory[i] != 0.0f) {
            initialized = false;
            break;
        }
    }
    printf("  New array elements initialized to zero: %s\n", initialized ? "Yes" : "No");
    assert(initialized && "New array elements not initialized to zero");
    
    // Test zero-sizing
    printf("  Testing resizing to zero...\n");
    status = galaxy_set_StarFormationHistory_size(&galaxy, 0);
    assert(status == 0 && "Failed to resize array to zero");
    assert(GALAXY_PROP_StarFormationHistory_SIZE(&galaxy) == 0 && 
           "Array size not zero after resize to zero");
    assert(galaxy.properties->StarFormationHistory == NULL && 
           "StarFormationHistory array not NULL after resize to zero");
    
    // Clean up
    free_galaxy_properties(&galaxy);
    free_galaxy_properties(&galaxy2);
    galaxy_extension_cleanup(&galaxy);
    galaxy_extension_cleanup(&galaxy2);
    
    printf("  Dynamic array memory tests passed!\n");
}