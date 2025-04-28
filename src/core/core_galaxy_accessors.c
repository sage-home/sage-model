#include <string.h>
#include "core_galaxy_accessors.h"
#include "core_logging.h"

// Default to direct access initially
int use_extension_properties = 0;

// Define maximum number of registered accessors
#define MAX_PROPERTY_ACCESSORS 128

// Registry for property accessors
static struct galaxy_property_accessor property_accessors[MAX_PROPERTY_ACCESSORS];
static int num_accessors = 0;

int register_galaxy_property_accessor(const struct galaxy_property_accessor *accessor) {
    if (num_accessors >= MAX_PROPERTY_ACCESSORS) {
        LOG_ERROR("Cannot register more property accessors, maximum (%d) reached", MAX_PROPERTY_ACCESSORS);
        return -1;
    }
    
    // Check if property already registered
    for (int i = 0; i < num_accessors; i++) {
        if (strcmp(property_accessors[i].property_name, accessor->property_name) == 0) {
            LOG_WARNING("Property '%s' already registered, overwriting", accessor->property_name);
            // Update existing accessor
            property_accessors[i] = *accessor;
            return i;
        }
    }
    
    // Add new accessor
    property_accessors[num_accessors] = *accessor;
    LOG_DEBUG("Registered property accessor for '%s'", accessor->property_name);
    return num_accessors++;
}

int find_galaxy_property_accessor(const char *property_name) {
    for (int i = 0; i < num_accessors; i++) {
        if (strcmp(property_accessors[i].property_name, property_name) == 0) {
            return i;
        }
    }
    LOG_WARNING("Property accessor for '%s' not found", property_name);
    return -1;
}

double get_galaxy_property(const struct GALAXY *galaxy, int accessor_id) {
    if (accessor_id < 0 || accessor_id >= num_accessors) {
        LOG_ERROR("Invalid accessor ID: %d", accessor_id);
        return 0.0;
    }
    
    galaxy_get_property_fn get_fn = property_accessors[accessor_id].get_fn;
    if (get_fn == NULL) {
        LOG_ERROR("Getter function not registered for accessor ID: %d", accessor_id);
        return 0.0;
    }
    
    return get_fn(galaxy);
}

void set_galaxy_property(struct GALAXY *galaxy, int accessor_id, double value) {
    if (accessor_id < 0 || accessor_id >= num_accessors) {
        LOG_ERROR("Invalid accessor ID: %d", accessor_id);
        return;
    }
    
    galaxy_set_property_fn set_fn = property_accessors[accessor_id].set_fn;
    if (set_fn == NULL) {
        LOG_ERROR("Setter function not registered for accessor ID: %d", accessor_id);
        return;
    }
    
    set_fn(galaxy, value);
}