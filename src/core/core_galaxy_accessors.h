#pragma once

#include "core_allvars.h"
#include "../physics/standard_physics_properties.h"

// Configuration for accessor behavior
extern int use_extension_properties;  // 0 = direct access, 1 = extensions

// Cooling accessors
// Direct access
static inline double galaxy_get_cooling_direct(const struct GALAXY *galaxy) {
    return galaxy->Cooling;
}
static inline void galaxy_set_cooling_direct(struct GALAXY *galaxy, double value) {
    galaxy->Cooling = value;
}
// Extension access
static inline double galaxy_get_cooling_extension(const struct GALAXY *galaxy) {
    return galaxy_get_cooling_rate(galaxy);
}
static inline void galaxy_set_cooling_extension(struct GALAXY *galaxy, double value) {
    galaxy_set_cooling_rate(galaxy, value);
}
// Unified accessors
static inline double galaxy_get_cooling(const struct GALAXY *galaxy) {
    return use_extension_properties ? galaxy_get_cooling_extension(galaxy) : galaxy_get_cooling_direct(galaxy);
}
static inline void galaxy_set_cooling(struct GALAXY *galaxy, double value) {
    if (use_extension_properties) {
        galaxy_set_cooling_extension(galaxy, value);
    } else {
        galaxy_set_cooling_direct(galaxy, value);
    }
}
// Heating accessors
// Direct access
static inline double galaxy_get_heating_direct(const struct GALAXY *galaxy) {
    return galaxy->Heating;
}
static inline void galaxy_set_heating_direct(struct GALAXY *galaxy, double value) {
    galaxy->Heating = value;
}
// Extension access
static inline double galaxy_get_heating_extension(const struct GALAXY *galaxy) {
    const struct cooling_property_ids *ids = get_cooling_property_ids();
    double *heating = galaxy_extension_get_data(galaxy, ids->heating_rate_id);
    return heating ? *heating : 0.0;
}
static inline void galaxy_set_heating_extension(struct GALAXY *galaxy, double value) {
    const struct cooling_property_ids *ids = get_cooling_property_ids();
    double *heating = galaxy_extension_get_data(galaxy, ids->heating_rate_id);
    if (heating) *heating = value;
}
// Unified accessors
static inline double galaxy_get_heating(const struct GALAXY *galaxy) {
    return use_extension_properties ? galaxy_get_heating_extension(galaxy) : galaxy_get_heating_direct(galaxy);
}
static inline void galaxy_set_heating(struct GALAXY *galaxy, double value) {
    if (use_extension_properties) {
        galaxy_set_heating_extension(galaxy, value);
    } else {
        galaxy_set_heating_direct(galaxy, value);
    }
}

// AGN accessors
// Direct access
static inline double galaxy_get_quasar_accretion_direct(const struct GALAXY *galaxy) {
    return galaxy->QuasarModeBHaccretionMass;
}
static inline void galaxy_set_quasar_accretion_direct(struct GALAXY *galaxy, double value) {
    galaxy->QuasarModeBHaccretionMass = value;
}
// Extension access
static inline double galaxy_get_quasar_accretion_extension(const struct GALAXY *galaxy) {
    return galaxy_get_quasar_accretion_rate(galaxy);
}
static inline void galaxy_set_quasar_accretion_extension(struct GALAXY *galaxy, double value) {
    galaxy_set_quasar_accretion_rate(galaxy, value);
}
// Unified accessors
static inline double galaxy_get_quasar_accretion(const struct GALAXY *galaxy) {
    return use_extension_properties ? galaxy_get_quasar_accretion_extension(galaxy) : galaxy_get_quasar_accretion_direct(galaxy);
}
static inline void galaxy_set_quasar_accretion(struct GALAXY *galaxy, double value) {
    if (use_extension_properties) {
        galaxy_set_quasar_accretion_extension(galaxy, value);
    } else {
        galaxy_set_quasar_accretion_direct(galaxy, value);
    }
}

// Outflow accessors
// Direct access
static inline double galaxy_get_outflow_rate_direct(const struct GALAXY *galaxy) {
    return galaxy->OutflowRate;
}
static inline void galaxy_set_outflow_rate_direct(struct GALAXY *galaxy, double value) {
    galaxy->OutflowRate = value;
}
// Extension access
static inline double galaxy_get_outflow_rate_extension(const struct GALAXY *galaxy) {
    return galaxy_get_outflow_value(galaxy);
}
static inline void galaxy_set_outflow_rate_extension(struct GALAXY *galaxy, double value) {
    galaxy_set_outflow_value(galaxy, value);
}
// Unified accessors
static inline double galaxy_get_outflow_rate(const struct GALAXY *galaxy) {
    return use_extension_properties ? galaxy_get_outflow_rate_extension(galaxy) : galaxy_get_outflow_rate_direct(galaxy);
}
static inline void galaxy_set_outflow_rate(struct GALAXY *galaxy, double value) {
    if (use_extension_properties) {
        galaxy_set_outflow_rate_extension(galaxy, value);
    } else {
        galaxy_set_outflow_rate_direct(galaxy, value);
    }
}
