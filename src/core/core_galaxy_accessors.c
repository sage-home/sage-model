#include <string.h>
#include <math.h> // For isnan, isfinite
#include "core_galaxy_accessors.h"
#include "core_logging.h"
#include "core_allvars.h"    // For struct GALAXY definition
#include "core_properties.h" // For GALAXY_PROP_* macros

// Always use properties now
int use_extension_properties = 1;

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
        return NAN; // Return NaN for error
    }

    galaxy_get_property_fn get_fn = property_accessors[accessor_id].get_fn;
    if (get_fn == NULL) {
        LOG_ERROR("Getter function not registered for accessor ID: %d (%s)",
                  accessor_id, property_accessors[accessor_id].property_name);
        return NAN; // Return NaN for error
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
        LOG_ERROR("Setter function not registered for accessor ID: %d (%s)",
                  accessor_id, property_accessors[accessor_id].property_name);
        return;
    }

    set_fn(galaxy, value);
}

// Define stubs for physics-specific accessors
// These will be implemented in physics modules
#define STUB_GETTER(prop_name) \
double galaxy_get_##prop_name(const struct GALAXY *galaxy) { \
    LOG_WARNING("No implementation for galaxy_get_" #prop_name " (physics property)"); \
    return 0.0; \
}

#define STUB_SETTER(prop_name) \
void galaxy_set_##prop_name(struct GALAXY *galaxy, double value) { \
    LOG_WARNING("No implementation for galaxy_set_" #prop_name " (physics property)"); \
}

// Stub implementations for physics properties
STUB_GETTER(stellar_mass)
STUB_GETTER(blackhole_mass)
STUB_GETTER(cold_gas)
STUB_GETTER(hot_gas)
STUB_GETTER(ejected_mass)
STUB_GETTER(metals_stellar_mass)
STUB_GETTER(metals_cold_gas)
STUB_GETTER(metals_hot_gas)
STUB_GETTER(metals_ejected_mass)
STUB_GETTER(bulge_mass)
STUB_GETTER(metals_bulge_mass)
STUB_GETTER(ics)
STUB_GETTER(metals_ics)
STUB_GETTER(cooling_rate)
STUB_GETTER(heating_rate)
STUB_GETTER(outflow_rate)
STUB_GETTER(totalsatellitebaryons)

STUB_SETTER(stellar_mass)
STUB_SETTER(blackhole_mass)
STUB_SETTER(cold_gas)
STUB_SETTER(hot_gas)
STUB_SETTER(ejected_mass) 
STUB_SETTER(metals_stellar_mass)
STUB_SETTER(metals_cold_gas)
STUB_SETTER(metals_hot_gas)
STUB_SETTER(metals_ejected_mass)
STUB_SETTER(bulge_mass)
STUB_SETTER(metals_bulge_mass)
STUB_SETTER(ics)
STUB_SETTER(metals_ics)
STUB_SETTER(cooling_rate)
STUB_SETTER(heating_rate)
STUB_SETTER(outflow_rate)
STUB_SETTER(totalsatellitebaryons)

#undef STUB_GETTER
#undef STUB_SETTER