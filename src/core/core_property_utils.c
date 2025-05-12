#include "core_property_utils.h"
#include "core_mymalloc.h" // For mymalloc/myfree if used for cache
#include "core_utils.h"    // For sage_assert, LOG_ERROR etc.
#include "core_property_descriptor.h"  // For GalaxyPropertyInfo and property_meta_t
#include <stdio.h>
#include <string.h>

// Cache for property IDs
// A simple approach: array of structs. For more advanced, consider a hash table.
#define MAX_CACHED_PROPERTIES 64 // Adjust as needed
typedef struct {
    char name[MAX_STRING_LEN];
    property_id_t id;
} CachedProperty;

static CachedProperty property_cache[MAX_CACHED_PROPERTIES];
static int32_t num_cached_properties = 0;
static bool property_cache_initialized = false;

// External declarations for generated property metadata
extern const property_meta_t PROPERTY_META[];
extern const int32_t TotGalaxyProperties;
extern const int32_t CORE_PROP_COUNT;

// Forward declaration for internal helper, if any.
// e.g. static property_id_t find_property_id_in_registry(const char *name);

// Initialize the cache (simple version)
static void init_property_cache_if_needed(void) {
    if (!property_cache_initialized) {
        for (int i = 0; i < MAX_CACHED_PROPERTIES; ++i) {
            property_cache[i].name[0] = '\0';
            property_cache[i].id = -1; // Assuming -1 is an invalid/not-found ID
        }
        num_cached_properties = 0;
        property_cache_initialized = true;
    }
}

float get_float_property(const struct GALAXY *galaxy, property_id_t prop_id, float default_value) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_float_property.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_float_property.");

    // Check if prop_id is valid. The GALAXY_PROP_BY_ID macro itself might do checks,
    // but an explicit check here can provide better context or avoid macro complexities.
    // This depends on how property_id_t and the generated macros are structured.
    // For now, we rely on GALAXY_PROP_BY_ID to handle validity.
    // A more robust check would involve knowing the max valid prop_id.
    if (prop_id < 0 || prop_id >= MAX_GALAXY_PROPERTIES) { // MAX_GALAXY_PROPERTIES should be defined in generated headers
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    
    // Assuming GALAXY_PROP_BY_ID handles type casting or direct access correctly.
    // The third argument to GALAXY_PROP_BY_ID is the type.
    return GALAXY_PROP_BY_ID(galaxy, prop_id, float);
}

int32_t get_int32_property(const struct GALAXY *galaxy, property_id_t prop_id, int32_t default_value) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_int32_property.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_int32_property.");

    if (prop_id < 0 || prop_id >= MAX_GALAXY_PROPERTIES) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    return GALAXY_PROP_BY_ID(galaxy, prop_id, int32_t);
}

double get_double_property(const struct GALAXY *galaxy, property_id_t prop_id, double default_value) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_double_property.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_double_property.");

    if (prop_id < 0 || prop_id >= MAX_GALAXY_PROPERTIES) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    return GALAXY_PROP_BY_ID(galaxy, prop_id, double);
}

bool has_property(const struct GALAXY *galaxy, property_id_t prop_id) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in has_property.");
    // If galaxy->properties can be NULL for some valid states, check that too.
    // sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in has_property.");

    // This check depends on how MAX_GALAXY_PROPERTIES is defined and if prop_id can be out of range
    // or if there's a specific mechanism to know if a property is "active" for a galaxy.
    // For a simple system where all defined properties always exist (even if zeroed),
    // this just checks if the ID is within the valid range of defined properties.
    if (prop_id >= 0 && prop_id < MAX_GALAXY_PROPERTIES) {
        // Further checks could be:
        // 1. Is the galaxy->properties struct itself initialized? (already asserted)
        // 2. Does the property system have a way to mark properties as "not set" or "inactive"?
        //    (The current SAGE property system implies properties always exist once defined in YAML).
        return true; 
    }
    return false;
}

// This function needs access to the global property metadata array,
// which is typically generated in core_properties.c (e.g., `GalaxyPropertyInfo[]`).
// We need to declare it as extern here if it's defined in another .c file.
extern const GalaxyPropertyInfo galaxy_property_info[MAX_GALAXY_PROPERTIES];
extern const int32_t TotGalaxyProperties; // Total number of actual properties

property_id_t get_cached_property_id(const char *name) {
    sage_assert(name != NULL && name[0] != '\0', "Property name cannot be NULL or empty in get_cached_property_id.");

    init_property_cache_if_needed();

    // Check cache first
    for (int32_t i = 0; i < num_cached_properties; ++i) {
        if (strcmp(property_cache[i].name, name) == 0) {
            return property_cache[i].id;
        }
    }

    // If not in cache, find in the global property registry
    extern const GalaxyPropertyInfo galaxy_property_info[MAX_GALAXY_PROPERTIES];
    
    for (int32_t i = 0; i < TotGalaxyProperties; ++i) {
        if (strcmp(galaxy_property_info[i].name, name) == 0) {
            // Use array index as property ID (not a field)
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
    return -1; // Or some defined INVALID_PROPERTY_ID
}

bool is_core_property(property_id_t prop_id) {
    // Core properties typically have IDs below a certain threshold
    // This threshold (CORE_PROP_COUNT) is defined in the generated code
    return prop_id >= 0 && prop_id < CORE_PROP_COUNT;
}

const property_meta_t* get_property_meta(property_id_t prop_id) {
    // Return metadata for a property by ID
    if (prop_id >= 0 && prop_id < TotGalaxyProperties) {
        return &PROPERTY_META[prop_id];
    }
    return NULL;
}

// Cleanup function for the cache, if necessary (e.g., if using mymalloc)
// void cleanup_property_cache(void) {
//     // If names were dynamically allocated:
//     // for (int i = 0; i < num_cached_properties; ++i) {
//     //     myfree(property_cache[i].name);
//     // }
//     // Reset cache state
//     num_cached_properties = 0;
//     property_cache_initialized = false;
// }
