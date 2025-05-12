#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

// Simple property ID type
typedef int property_id_t;

// Mock property_meta_t struct
typedef struct {
    char name[64];
    char type[32];
    char description[256];
} property_meta_t;

// Mock GALAXY struct
struct GALAXY {
    int64_t GalaxyIndex;
    void *properties;
};

// Mock PROPERTY_META
property_meta_t PROPERTY_META[10] = {
    {"Type", "int32", "Galaxy type"},
    {"Mvir", "float", "Virial mass"},
    {"Rvir", "float", "Virial radius"},
    {"Pos", "float[3]", "Position"}
};

// Mock constant for core property count
const int32_t CORE_PROP_COUNT = 4;
const int32_t TotGalaxyProperties = 10;

// Mock galaxy_property_info
typedef struct {
    char name[64];
} GalaxyPropertyInfo;

const GalaxyPropertyInfo galaxy_property_info[10] = {
    {"Type"},
    {"Mvir"},
    {"Rvir"},
    {"Pos"},
    {"ColdGas"},
    {"HotGas"},
    {"StellarMass"},
    {"BulgeMass"},
    {"BlackHoleMass"},
    {"Metals"}
};

// Mock macros and functions from core framework
#define sage_assert(cond, msg) if (!(cond)) { fprintf(stderr, "Assertion failed: %s\n", msg); exit(1); }
#define LOG_ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define LOG_WARN(...) fprintf(stderr, "WARNING: " __VA_ARGS__)
#define MAX_STRING_LEN 256
#define MAX_GALAXY_PROPERTIES 100

// Implement the new functions from core_property_utils.c

// Cache for property IDs
#define MAX_CACHED_PROPERTIES 64
typedef struct {
    char name[MAX_STRING_LEN];
    property_id_t id;
} CachedProperty;

static CachedProperty property_cache[MAX_CACHED_PROPERTIES];
static int32_t num_cached_properties = 0;
static bool property_cache_initialized = false;

// Initialize the cache
static void init_property_cache_if_needed(void) {
    if (!property_cache_initialized) {
        for (int i = 0; i < MAX_CACHED_PROPERTIES; ++i) {
            property_cache[i].name[0] = '\0';
            property_cache[i].id = -1;
        }
        num_cached_properties = 0;
        property_cache_initialized = true;
    }
}

// Implementations of our functions
bool is_core_property(property_id_t prop_id) {
    return prop_id >= 0 && prop_id < CORE_PROP_COUNT;
}

const property_meta_t* get_property_meta(property_id_t prop_id) {
    if (prop_id >= 0 && prop_id < TotGalaxyProperties) {
        return &PROPERTY_META[prop_id];
    }
    return NULL;
}

property_id_t get_cached_property_id(const char *name) {
    sage_assert(name != NULL && name[0] != '\0', "Property name cannot be NULL or empty");

    init_property_cache_if_needed();

    // Check cache first
    for (int32_t i = 0; i < num_cached_properties; ++i) {
        if (strcmp(property_cache[i].name, name) == 0) {
            return property_cache[i].id;
        }
    }

    // If not in cache, find in the global property registry
    for (int32_t i = 0; i < TotGalaxyProperties; ++i) {
        if (strcmp(galaxy_property_info[i].name, name) == 0) {
            // Use array index as property ID
            property_id_t found_id = i;
            
            // Add to cache if there's space
            if (num_cached_properties < MAX_CACHED_PROPERTIES) {
                strncpy(property_cache[num_cached_properties].name, name, MAX_STRING_LEN - 1);
                property_cache[num_cached_properties].name[MAX_STRING_LEN - 1] = '\0';
                property_cache[num_cached_properties].id = found_id;
                num_cached_properties++;
            } else {
                LOG_WARN("Property ID cache is full. Consider increasing MAX_CACHED_PROPERTIES.");
            }
            return found_id;
        }
    }

    LOG_ERROR("Property with name '%s' not found in global property registry.", name);
    return -1;
}

// Simple tests
int main() {
    printf("Testing is_core_property function...\n");

    // Test core properties
    assert(is_core_property(0) == true);  // Type is a core property
    assert(is_core_property(1) == true);  // Mvir is a core property
    assert(is_core_property(3) == true);  // Pos is a core property
    
    // Test physics properties
    assert(is_core_property(4) == false); // ColdGas is not a core property
    assert(is_core_property(7) == false); // BulgeMass is not a core property
    
    // Test invalid properties
    assert(is_core_property(-1) == false);
    assert(is_core_property(100) == false);
    
    printf("Test for is_core_property passed!\n");
    
    printf("Testing get_property_meta function...\n");
    
    // Test valid properties
    const property_meta_t *type_meta = get_property_meta(0);
    assert(type_meta != NULL);
    assert(strcmp(type_meta->name, "Type") == 0);
    
    const property_meta_t *mvir_meta = get_property_meta(1);
    assert(mvir_meta != NULL);
    assert(strcmp(mvir_meta->name, "Mvir") == 0);
    
    // Test invalid properties
    assert(get_property_meta(-1) == NULL);
    assert(get_property_meta(100) == NULL);
    
    printf("Test for get_property_meta passed!\n");
    
    printf("Testing get_cached_property_id function...\n");
    
    // Test valid property names
    assert(get_cached_property_id("Type") == 0);
    assert(get_cached_property_id("Mvir") == 1);
    assert(get_cached_property_id("ColdGas") == 4);
    
    // Test caching
    assert(num_cached_properties > 0);
    
    // Test invalid property name
    assert(get_cached_property_id("NonExistentProperty") == -1);
    
    printf("Test for get_cached_property_id passed!\n");
    
    printf("All tests passed successfully!\n");
    return 0;
}
