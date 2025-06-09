#include "core_property_utils.h"
#include "core_mymalloc.h" // For mymalloc/myfree if used for cache
#include "core_utils.h"    // For sage_assert and LOG_ERROR etc.
#include "core_property_descriptor.h"  // For GalaxyPropertyInfo and property_meta_t
#include "core_logging.h"   // For LOG_ERROR, LOG_WARN etc.
#include "core_allvars.h"   // For struct GALAXY
#include "core_properties.h" // For property metadata access and generated dispatchers
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h> // for NAN

// Cache for property IDs
#define MAX_CACHED_PROPERTIES 64 // Adjust as needed
typedef struct {
    char name[MAX_STRING_LEN]; // Assuming MAX_STRING_LEN is defined in core_allvars.h or similar
    property_id_t id;
} CachedProperty;

static CachedProperty property_cache[MAX_CACHED_PROPERTIES];
static int32_t num_cached_properties = 0;
static bool property_cache_initialized = false;

// External declarations for generated property metadata
extern property_meta_t PROPERTY_META[];
extern const int32_t TotGalaxyProperties; // Total number of properties defined in properties.yaml
extern const int32_t CORE_PROP_COUNT;     // Number of core properties

// Initialize the cache (simple version)
static void init_property_cache_if_needed(void) {
    if (!property_cache_initialized) {
        for (int i = 0; i < MAX_CACHED_PROPERTIES; ++i) {
            property_cache[i].name[0] = '\0';
            property_cache[i].id = (property_id_t)-1; // Use -1 as an invalid/not-found ID
        }
        num_cached_properties = 0;
        property_cache_initialized = true;
    }
}

float get_float_property(const struct GALAXY *galaxy, property_id_t prop_id, float default_value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_float_property.");
        return default_value;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_float_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return default_value;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    return get_generated_float(galaxy->properties, prop_id, default_value);
}

int32_t get_int32_property(const struct GALAXY *galaxy, property_id_t prop_id, int32_t default_value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_int32_property.");
        return default_value;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_int32_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return default_value;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    return get_generated_int32(galaxy->properties, prop_id, default_value);
}

double get_double_property(const struct GALAXY *galaxy, property_id_t prop_id, double default_value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_double_property.");
        return default_value;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_double_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return default_value;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    return get_generated_double(galaxy->properties, prop_id, default_value);
}

int64_t get_int64_property(const struct GALAXY *galaxy, property_id_t prop_id, int64_t default_value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_int64_property.");
        return default_value;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_int64_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return default_value;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    // Check if the property is one of the known uint64_t core properties
    // These are direct members of `galaxy_properties_t` (or `struct GALAXY` itself if core_properties.yaml maps them so)
    // The GALAXY_PROP_ macros handle this correctly.
    // This function is for generic access.
    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta && strcmp(meta->type, "uint64_t") == 0) {
        // Assuming a get_generated_uint64 exists or the macro handles it.
        // For now, direct access for known uint64_t core properties:
        if (prop_id == PROP_GalaxyIndex) return (int64_t)GALAXY_PROP_GalaxyIndex(galaxy);
        if (prop_id == PROP_CentralGalaxyIndex) return (int64_t)GALAXY_PROP_CentralGalaxyIndex(galaxy);
        // MostBoundID is long long, which is int64_t
        if (prop_id == PROP_MostBoundID) return (int64_t)GALAXY_PROP_MostBoundID(galaxy);

        LOG_WARNING("get_int64_property called for uint64_t property ID %d ('%s') not explicitly handled. Returning default.", prop_id, meta->name);
        return default_value;
    } else if (meta && (strcmp(meta->type, "int64_t") == 0 || strcmp(meta->type, "long long") == 0)) {
        // If an actual int64_t property (not uint64_t)
        // This would ideally use a get_generated_int64 if it existed.
        // For now, handle known long long properties explicitly
        if (prop_id == PROP_MostBoundID) return (int64_t)GALAXY_PROP_MostBoundID(galaxy);
        if (prop_id == PROP_SimulationHaloIndex) return (int64_t)GALAXY_PROP_SimulationHaloIndex(galaxy);

        LOG_WARNING("get_int64_property called for int64_t property ID %d ('%s') not explicitly handled. Returning default.", prop_id, meta->name);
        return default_value;
    }


    LOG_WARNING("get_int64_property called for non-int64/uint64 property ID %d ('%s'). Type is '%s'. Returning default.", prop_id, meta ? meta->name : "unknown", meta ? meta->type : "unknown");
    return default_value;
}


int set_float_property(struct GALAXY *galaxy, property_id_t prop_id, float value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in set_float_property.");
        return EXIT_FAILURE;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in set_float_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    set_generated_float(galaxy->properties, prop_id, value);
    return EXIT_SUCCESS;
}

int set_int32_property(struct GALAXY *galaxy, property_id_t prop_id, int32_t value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in set_int32_property.");
        return EXIT_FAILURE;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in set_int32_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    set_generated_int32(galaxy->properties, prop_id, value);
    return EXIT_SUCCESS;
}

int set_double_property(struct GALAXY *galaxy, property_id_t prop_id, double value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in set_double_property.");
        return EXIT_FAILURE;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in set_double_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    set_generated_double(galaxy->properties, prop_id, value);
    return EXIT_SUCCESS;
}

float get_float_array_element_property(const struct GALAXY *galaxy, property_id_t prop_id, int array_idx, float default_value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_float_array_element_property.");
        return default_value;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_float_array_element_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return default_value;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL || !meta->is_array) {
        LOG_ERROR("Property '%s' (ID %d) is not an array property but was accessed as one for galaxy %llu.",
                  meta ? meta->name : "unknown", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    // Bounds check is handled by get_generated_float_array_element
    return get_generated_float_array_element(galaxy->properties, prop_id, array_idx, default_value);
}

int32_t get_int32_array_element_property(const struct GALAXY *galaxy, property_id_t prop_id, int array_idx, int32_t default_value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_int32_array_element_property.");
        return default_value;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_int32_array_element_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return default_value;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL || !meta->is_array) {
        LOG_ERROR("Property '%s' (ID %d) is not an array property but was accessed as one for galaxy %llu.",
                  meta ? meta->name : "unknown", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    // Bounds check is handled by get_generated_int32_array_element
    return get_generated_int32_array_element(galaxy->properties, prop_id, array_idx, default_value);
}

double get_double_array_element_property(const struct GALAXY *galaxy, property_id_t prop_id, int array_idx, double default_value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_double_array_element_property.");
        return default_value;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_double_array_element_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return default_value;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu.", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }

    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL || !meta->is_array) {
        LOG_ERROR("Property '%s' (ID %d) is not an array property but was accessed as one for galaxy %llu.",
                  meta ? meta->name : "unknown", prop_id, galaxy->GalaxyIndex);
        return default_value;
    }
    // Bounds check is handled by get_generated_double_array_element
    return get_generated_double_array_element(galaxy->properties, prop_id, array_idx, default_value);
}

int set_float_array_element_property(struct GALAXY *galaxy, property_id_t prop_id, int array_idx, float value) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in set_float_array_element_property.");
        return EXIT_FAILURE;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in set_float_array_element_property (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu in set_float_array_element_property.",
                 prop_id, galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL || !meta->is_array) {
        LOG_ERROR("Property '%s' (ID %d) is not an array property but was set as one for galaxy %llu.",
                  meta ? meta->name : "unknown", prop_id, galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    int array_size = get_property_array_size(galaxy, prop_id);
    if (array_idx < 0 || array_idx >= array_size) {
        LOG_ERROR("Array index %d out of bounds for property '%s' (ID %d, size %d) for galaxy %llu.",
                  array_idx, meta->name, prop_id, array_size, galaxy->GalaxyIndex);
        return EXIT_FAILURE;
    }

    set_generated_float_array_element(galaxy->properties, prop_id, array_idx, value);
    return EXIT_SUCCESS;
}

bool has_property(const struct GALAXY *galaxy, property_id_t prop_id) {
    if (galaxy == NULL) {
        LOG_DEBUG("Galaxy pointer is NULL in has_property check.");
        return false;
    }
    if (galaxy->properties == NULL) {
        LOG_DEBUG("Galaxy properties pointer is NULL in has_property check (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return false;
    }

    if (prop_id >= 0 && prop_id < (property_id_t)TotGalaxyProperties) {
        return true;
    }
    return false;
}

property_id_t get_cached_property_id(const char *name) {
    if (name == NULL || name[0] == '\0') {
         LOG_ERROR("Property name cannot be NULL or empty in get_cached_property_id.");
         return PROP_COUNT; // Return PROP_COUNT for invalid input, consistent with get_property_id()
    }

    init_property_cache_if_needed();

    // Check cache first
    for (int32_t i = 0; i < num_cached_properties; ++i) {
        if (strcmp(property_cache[i].name, name) == 0) {
            return property_cache[i].id;
        }
    }

    // If not in cache, find in the property metadata
    // Assuming PROPERTY_META and TotGalaxyProperties are available (from core_properties.c)
    for (int32_t i = 0; i < TotGalaxyProperties; ++i) {
        if (PROPERTY_META[i].name != NULL && strcmp(PROPERTY_META[i].name, name) == 0) {
            property_id_t found_id = (property_id_t)i; // The index is the ID

            // Add to cache if there's space
            if (num_cached_properties < MAX_CACHED_PROPERTIES) {
                strncpy(property_cache[num_cached_properties].name, name, MAX_STRING_LEN - 1);
                property_cache[num_cached_properties].name[MAX_STRING_LEN - 1] = '\0';
                property_cache[num_cached_properties].id = found_id;
                num_cached_properties++;
            } else {
                LOG_WARNING("Property ID cache is full. Consider increasing MAX_CACHED_PROPERTIES.");
            }
            return found_id;
        }
    }

    LOG_ERROR("Property with name '%s' not found in property metadata.", name);
    return PROP_COUNT; // Return PROP_COUNT for "not found" to align with get_property_id()
}

bool is_core_property(property_id_t prop_id) {
    // CORE_PROP_COUNT is defined in the generated core_properties.c
    return prop_id >= 0 && prop_id < (property_id_t)CORE_PROP_COUNT;
}

const property_meta_t* get_property_meta(property_id_t prop_id) {
    if (prop_id >= 0 && prop_id < (property_id_t)TotGalaxyProperties) { // Use TotGalaxyProperties
        return &PROPERTY_META[prop_id];
    }
    LOG_WARNING("Requested metadata for invalid property ID %d.", prop_id);
    return NULL;
}

int get_property_array_size(const struct GALAXY *galaxy, property_id_t prop_id) {
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL in get_property_array_size.");
        return 0;
    }
    if (galaxy->properties == NULL) {
        LOG_ERROR("Galaxy properties pointer cannot be NULL in get_property_array_size (GalaxyIndex: %llu).", galaxy->GalaxyIndex);
        return 0;
    }

    if (prop_id < 0 || prop_id >= (property_id_t)TotGalaxyProperties) { // Use TotGalaxyProperties
        LOG_ERROR("Invalid property ID %d requested for galaxy %llu in get_property_array_size.",
                 prop_id, galaxy->GalaxyIndex);
        return 0;
    }

    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL || !meta->is_array) {
        LOG_ERROR("Property '%s' (ID %d) is not an array property but queried for array size for galaxy %llu.",
                  meta ? meta->name : "unknown", prop_id, galaxy->GalaxyIndex);
        return 0;
    }

    return get_generated_array_size(galaxy->properties, prop_id);
}