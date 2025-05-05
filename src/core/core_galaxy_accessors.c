#include <string.h>
#include <math.h> // For isnan, isfinite
#include "core_galaxy_accessors.h"
#include "core_logging.h"
#include "core_properties.h" // For GALAXY_PROP_* macros
#include "core_allvars.h"    // For struct GALAXY definition

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

// ===== Standard Property Accessor Implementations =====

// Helper macro for getter functions
// Returns double for consistency, handles potential NaN
#define DEFINE_GETTER(prop_name, return_type, direct_field, prop_macro) \
double galaxy_get_##prop_name(const struct GALAXY *galaxy) {            \
    if (galaxy == NULL) { LOG_ERROR("Null galaxy pointer in getter for %s", #prop_name); return NAN; } \
    if (use_extension_properties) {                                     \
        if (galaxy->properties == NULL) { LOG_ERROR("Galaxy properties not allocated for %s (GalaxyNr %d)", #prop_name, galaxy->GalaxyNr); return NAN; } \
        /* Cast result to double for consistent return type */          \
        return (return_type)prop_macro(galaxy);                         \
    } else {                                                            \
        /* Cast result to double for consistent return type */          \
        return (return_type)galaxy->direct_field;                       \
    }                                                                   \
}

// Helper macro for setter functions
// Takes double, casts to appropriate type for setting
#define DEFINE_SETTER(prop_name, value_type, direct_field, prop_macro) \
void galaxy_set_##prop_name(struct GALAXY *galaxy, double value) {      \
    if (galaxy == NULL) { LOG_ERROR("Null galaxy pointer in setter for %s", #prop_name); return; } \
    if (use_extension_properties) {                                     \
        if (galaxy->properties == NULL) { LOG_ERROR("Galaxy properties not allocated for %s (GalaxyNr %d)", #prop_name, galaxy->GalaxyNr); return; } \
        /* Cast input double to the correct storage type */             \
        prop_macro(galaxy) = (value_type)value;                         \
    } else {                                                            \
        /* Cast input double to the correct storage type */             \
        galaxy->direct_field = (value_type)value;                       \
    }                                                                   \
}

// --- Implementations for required accessors ---
DEFINE_GETTER(stellar_mass, double, StellarMass, GALAXY_PROP_StellarMass)
DEFINE_GETTER(blackhole_mass, double, BlackHoleMass, GALAXY_PROP_BlackHoleMass)
DEFINE_GETTER(cold_gas, double, ColdGas, GALAXY_PROP_ColdGas)
DEFINE_GETTER(hot_gas, double, HotGas, GALAXY_PROP_HotGas)
DEFINE_GETTER(ejected_mass, double, EjectedMass, GALAXY_PROP_EjectedMass)
DEFINE_GETTER(metals_stellar_mass, double, MetalsStellarMass, GALAXY_PROP_MetalsStellarMass)
DEFINE_GETTER(metals_cold_gas, double, MetalsColdGas, GALAXY_PROP_MetalsColdGas)
DEFINE_GETTER(metals_hot_gas, double, MetalsHotGas, GALAXY_PROP_MetalsHotGas)
DEFINE_GETTER(metals_ejected_mass, double, MetalsEjectedMass, GALAXY_PROP_MetalsEjectedMass)
DEFINE_GETTER(bulge_mass, double, BulgeMass, GALAXY_PROP_BulgeMass)
DEFINE_GETTER(metals_bulge_mass, double, MetalsBulgeMass, GALAXY_PROP_MetalsBulgeMass)
DEFINE_GETTER(ics, double, ICS, GALAXY_PROP_ICS)
DEFINE_GETTER(metals_ics, double, MetalsICS, GALAXY_PROP_MetalsICS)
DEFINE_GETTER(cooling_rate, double, Cooling, GALAXY_PROP_Cooling)
DEFINE_GETTER(heating_rate, double, Heating, GALAXY_PROP_Heating)
DEFINE_GETTER(outflow_rate, double, OutflowRate, GALAXY_PROP_OutflowRate)
DEFINE_GETTER(totalsatellitebaryons, double, TotalSatelliteBaryons, GALAXY_PROP_TotalSatelliteBaryons)

DEFINE_SETTER(stellar_mass, float, StellarMass, GALAXY_PROP_StellarMass)
DEFINE_SETTER(blackhole_mass, float, BlackHoleMass, GALAXY_PROP_BlackHoleMass)
DEFINE_SETTER(cold_gas, float, ColdGas, GALAXY_PROP_ColdGas)
DEFINE_SETTER(hot_gas, float, HotGas, GALAXY_PROP_HotGas)
DEFINE_SETTER(ejected_mass, float, EjectedMass, GALAXY_PROP_EjectedMass)
DEFINE_SETTER(metals_stellar_mass, float, MetalsStellarMass, GALAXY_PROP_MetalsStellarMass)
DEFINE_SETTER(metals_cold_gas, float, MetalsColdGas, GALAXY_PROP_MetalsColdGas)
DEFINE_SETTER(metals_hot_gas, float, MetalsHotGas, GALAXY_PROP_MetalsHotGas)
DEFINE_SETTER(metals_ejected_mass, float, MetalsEjectedMass, GALAXY_PROP_MetalsEjectedMass)
DEFINE_SETTER(bulge_mass, float, BulgeMass, GALAXY_PROP_BulgeMass)
DEFINE_SETTER(metals_bulge_mass, float, MetalsBulgeMass, GALAXY_PROP_MetalsBulgeMass)
DEFINE_SETTER(ics, float, ICS, GALAXY_PROP_ICS)
DEFINE_SETTER(metals_ics, float, MetalsICS, GALAXY_PROP_MetalsICS)
DEFINE_SETTER(cooling_rate, double, Cooling, GALAXY_PROP_Cooling) // Note: Cooling/Heating are double
DEFINE_SETTER(heating_rate, double, Heating, GALAXY_PROP_Heating) // Note: Cooling/Heating are double
DEFINE_SETTER(outflow_rate, float, OutflowRate, GALAXY_PROP_OutflowRate)
DEFINE_SETTER(totalsatellitebaryons, float, TotalSatelliteBaryons, GALAXY_PROP_TotalSatelliteBaryons)

// Add definitions for other standard properties as they become needed...

#undef DEFINE_GETTER
#undef DEFINE_SETTER