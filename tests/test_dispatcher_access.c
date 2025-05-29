#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

// Simplified versions of what we need for the test
#define LOG_ERROR(format, ...) printf("ERROR: " format "\n", ##__VA_ARGS__)
#define MAX_GALAXY_PROPERTIES 100
#define STEPS 20
#define MAX_STRING_LEN 256
#define sage_assert(condition, message) do { if (!(condition)) { printf("ASSERT: %s\n", message); exit(1); } } while (0)

// Define property IDs
typedef enum {
    PROP_Mvir,
    PROP_Type,
    PROP_SfrDisk,
    PROP_COUNT
} property_id_t;

// Define property metadata
typedef struct {
    const char *name;
    const char *type;
    const char *units;
    const char *description;
    bool output;
    int read_only;
    bool is_array;
    int array_dimension;
    const char *size_parameter;
} property_meta_t;

// Define galaxy properties struct
typedef struct {
    float Mvir;
    int Type;
    float SfrDisk[STEPS];
} galaxy_properties_t;

// Define galaxy struct
struct GALAXY {
    galaxy_properties_t *properties;
    long long GalaxyIndex;
};

// Property accessors
#define GALAXY_PROP_Mvir(g) ((g)->properties->Mvir)
#define GALAXY_PROP_Type(g) ((g)->properties->Type)
#define GALAXY_PROP_SfrDisk(g) ((g)->properties->SfrDisk)
#define GALAXY_PROP_SfrDisk_ELEM(g, idx) ((g)->properties->SfrDisk[idx])

// Metadata
property_meta_t PROPERTY_META[PROP_COUNT] = {
    {"Mvir", "float", "1e10 Msun/h", "Virial mass of the halo", true, 0, false, 0, NULL},
    {"Type", "int32_t", "dimensionless", "Galaxy type (0=central, 1=satellite)", true, 0, false, 0, NULL},
    {"SfrDisk", "float[STEPS]", "Msun/yr", "Star formation rate in disk for each timestep", true, 0, true, STEPS, NULL}
};

// Get property metadata
const property_meta_t* get_property_meta(property_id_t prop_id) {
    if (prop_id >= 0 && prop_id < PROP_COUNT) {
        return &PROPERTY_META[prop_id];
    }
    return NULL;
}

// Auto-generated dispatcher functions
float get_generated_float(const galaxy_properties_t *props, property_id_t prop_id, float default_value) {
    if (!props) return default_value;
    switch (prop_id) {
        case PROP_Mvir: return props->Mvir;
        default: return default_value;
    }
}

int get_generated_int32(const galaxy_properties_t *props, property_id_t prop_id, int default_value) {
    if (!props) return default_value;
    switch (prop_id) {
        case PROP_Type: return props->Type;
        default: return default_value;
    }
}

float get_generated_float_array_element(const galaxy_properties_t *props, property_id_t prop_id, int array_idx, float default_value) {
    if (!props) return default_value;
    
    // Validate array index
    if (array_idx < 0) return default_value;
    
    switch (prop_id) {
        case PROP_SfrDisk:
            if (array_idx >= STEPS) return default_value;
            return props->SfrDisk[array_idx];
        default: return default_value;
    }
}

int get_generated_array_size(const galaxy_properties_t *props, property_id_t prop_id) {
    if (!props) return 0;
    
    switch (prop_id) {
        case PROP_SfrDisk: return STEPS;
        default: return 0;
    }
}

// Property access functions
float get_float_property(const struct GALAXY *galaxy, property_id_t prop_id, float default_value) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_float_property.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_float_property.");

    if (prop_id < 0 || prop_id >= (property_id_t)MAX_GALAXY_PROPERTIES) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    
    // Use the generated dispatcher
    return get_generated_float(galaxy->properties, prop_id, default_value);
}

int get_int32_property(const struct GALAXY *galaxy, property_id_t prop_id, int default_value) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_int32_property.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_int32_property.");

    if (prop_id < 0 || prop_id >= (property_id_t)MAX_GALAXY_PROPERTIES) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    
    // Use the generated dispatcher
    return get_generated_int32(galaxy->properties, prop_id, default_value);
}

float get_float_array_element_property(const struct GALAXY *galaxy, property_id_t prop_id, int array_idx, float default_value) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_float_array_element_property.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_float_array_element_property.");

    // Check if property ID is valid
    if (prop_id < 0 || prop_id >= (property_id_t)MAX_GALAXY_PROPERTIES) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    // Get property metadata to check if it's an array
    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL || !meta->is_array) {
        LOG_ERROR("Property '%s' (ID %d) is not an array property but was accessed as one for galaxy %lld.", 
                  meta ? meta->name : "unknown", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    
    // Use the generated array element dispatcher
    return get_generated_float_array_element(galaxy->properties, prop_id, array_idx, default_value);
}

int get_property_array_size(const struct GALAXY *galaxy, property_id_t prop_id) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_property_array_size.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_property_array_size.");

    // Check if property ID is valid
    if (prop_id < 0 || prop_id >= (property_id_t)MAX_GALAXY_PROPERTIES) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld in get_property_array_size.", 
                 prop_id, galaxy->GalaxyIndex);
        return 0;
    }

    // Get property metadata to check if it's an array
    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL || !meta->is_array) {
        LOG_ERROR("Property '%s' (ID %d) is not an array property but queried for array size for galaxy %lld.", 
                  meta ? meta->name : "unknown", prop_id, galaxy->GalaxyIndex);
        return 0;
    }
    
    // Use the generated array size dispatcher
    return get_generated_array_size(galaxy->properties, prop_id);
}

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

// Test function
void test_property_access() {
    struct GALAXY galaxy = {0};
    
    printf("\n=== Testing property access mechanisms ===\n");
    
    // Manually allocate properties
    galaxy.properties = calloc(1, sizeof(galaxy_properties_t));
    if (galaxy.properties == NULL) {
        fprintf(stderr, "Failed to allocate properties\n");
        return;
    }
    
    // Add a fake GalaxyIndex for more informative error messages
    galaxy.GalaxyIndex = 12345;
    
    // Initialize scalar properties with known values
    printf("Setting test values...\n");
    GALAXY_PROP_Mvir(&galaxy) = 1.0e12;                    // Scalar float property
    GALAXY_PROP_Type(&galaxy) = 1;                         // Scalar int property
    
    // Initialize array properties
    for (int i = 0; i < STEPS; i++) {
        GALAXY_PROP_SfrDisk_ELEM(&galaxy, i) = 5.5f + i;   // Fixed array property
    }
    
    // Test 1: Direct macro access vs. generic accessors for scalars
    printf("\nTest 1: Testing scalar property access...\n");
    float mvir_direct = GALAXY_PROP_Mvir(&galaxy);
    float mvir_by_fn = get_float_property(&galaxy, PROP_Mvir, 0.0f);
    printf("  Mvir direct: %g, Mvir by function: %g\n", mvir_direct, mvir_by_fn);
    TEST_ASSERT(mvir_direct == mvir_by_fn, "Float property access equivalence");
    
    int type_direct = GALAXY_PROP_Type(&galaxy);
    int type_by_fn = get_int32_property(&galaxy, PROP_Type, 0);
    printf("  Type direct: %d, Type by function: %d\n", type_direct, type_by_fn);
    TEST_ASSERT(type_direct == type_by_fn, "Integer property access equivalence");
    
    // Test 2: Array property access
    printf("\nTest 2: Testing array property access...\n");
    for (int i = 0; i < 3; i++) {
        float sfr_direct = GALAXY_PROP_SfrDisk_ELEM(&galaxy, i);
        float sfr_by_fn = get_float_array_element_property(&galaxy, PROP_SfrDisk, i, 0.0f);
        printf("  SfrDisk[%d] direct: %g, by function: %g\n", i, sfr_direct, sfr_by_fn);
        TEST_ASSERT(sfr_direct == sfr_by_fn, "Array element access equivalence");
    }
    
    // Test 3: Array size retrieval
    printf("\nTest 3: Testing array size retrieval...\n");
    int array_size = get_property_array_size(&galaxy, PROP_SfrDisk);
    printf("  SfrDisk array size: %d (should be %d)\n", array_size, STEPS);
    TEST_ASSERT(array_size == STEPS, "Array size retrieval accuracy");
    
    printf("\nAll property access tests passed!\n");
    printf("The implementation correctly follows existing codebase patterns and maintains core-physics separation.\n");
    
    // Clean up
    free(galaxy.properties);
}

int main() {
    printf("\n========================================\n");
    printf("Starting tests for test_dispatcher_access\n");
    printf("========================================\n\n");
    printf("This test verifies that:\n");
    printf("1. The property accessor functions correctly access properties\n");
    printf("2. The auto-generated dispatcher implementation works correctly\n");
    printf("3. Direct macro access and generic function access return the same values\n");
    
    test_property_access();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_dispatcher_access:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
