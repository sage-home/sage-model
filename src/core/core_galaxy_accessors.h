#ifndef CORE_GALAXY_ACCESSORS_H
#define CORE_GALAXY_ACCESSORS_H

#include "core_allvars.h" // Needed for struct GALAXY definition
#include <stdbool.h>      // Needed for bool type

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file core_galaxy_accessors.h
 * @brief Generic accessor interface for galaxy properties
 *
 * This file provides a modular accessor system for galaxy properties.
 * All physics-specific accessors should be defined in their respective modules,
 * not in the core. This file defines the infrastructure for module-based
 * property access AND provides accessors for standard SAGE properties.
 */

// Configuration for accessor behavior
extern int use_extension_properties;  // 0 = direct access, 1 = extensions

/**
 * Typedef for galaxy property get/set function pointers
 * These are used by modules to register their accessor functions
 */
typedef double (*galaxy_get_property_fn)(const struct GALAXY *galaxy);
typedef void (*galaxy_set_property_fn)(struct GALAXY *galaxy, double value);

/**
 * Runtime property registration structure
 * Used by modules to register their property access functions
 */
struct galaxy_property_accessor {
    const char *property_name;          // Name of the property
    galaxy_get_property_fn get_fn;      // Function to get property value
    galaxy_set_property_fn set_fn;      // Function to set property value
    int module_id;                      // ID of module that registered this accessor
};

// Function declarations for the accessor registry system
// These will be implemented in core_galaxy_accessors.c

/**
 * Register a property accessor
 *
 * @param accessor Accessor structure with function pointers
 * @return Accessor ID on success, negative value on failure
 */
int register_galaxy_property_accessor(const struct galaxy_property_accessor *accessor);

/**
 * Find a property accessor by name
 *
 * @param property_name Name of the property to find
 * @return Accessor ID on success, negative value if not found
 */
int find_galaxy_property_accessor(const char *property_name);

/**
 * Get a property value using a registered accessor
 *
 * @param galaxy Galaxy to get property from
 * @param accessor_id ID of the registered accessor
 * @return Property value
 */
double get_galaxy_property(const struct GALAXY *galaxy, int accessor_id);

/**
 * Set a property value using a registered accessor
 *
 * @param galaxy Galaxy to set property on
 * @param accessor_id ID of the registered accessor
 * @param value Value to set
 */
void set_galaxy_property(struct GALAXY *galaxy, int accessor_id, double value);

// ===== Standard Property Accessors (Added for Phase 5.2.C) =====
// These provide a bridge between direct field access and the property system.

extern double galaxy_get_stellar_mass(const struct GALAXY *galaxy);
extern double galaxy_get_blackhole_mass(const struct GALAXY *galaxy);
extern double galaxy_get_cold_gas(const struct GALAXY *galaxy);
extern double galaxy_get_hot_gas(const struct GALAXY *galaxy);
extern double galaxy_get_ejected_mass(const struct GALAXY *galaxy);
extern double galaxy_get_metals_stellar_mass(const struct GALAXY *galaxy);
extern double galaxy_get_metals_cold_gas(const struct GALAXY *galaxy);
extern double galaxy_get_metals_hot_gas(const struct GALAXY *galaxy);
extern double galaxy_get_metals_ejected_mass(const struct GALAXY *galaxy);
extern double galaxy_get_bulge_mass(const struct GALAXY *galaxy);
extern double galaxy_get_metals_bulge_mass(const struct GALAXY *galaxy);
extern double galaxy_get_ics(const struct GALAXY *galaxy);
extern double galaxy_get_metals_ics(const struct GALAXY *galaxy);
extern double galaxy_get_cooling_rate(const struct GALAXY *galaxy);
extern double galaxy_get_heating_rate(const struct GALAXY *galaxy);
extern double galaxy_get_outflow_rate(const struct GALAXY *galaxy);
extern double galaxy_get_totalsatellitebaryons(const struct GALAXY *galaxy);

extern void galaxy_set_stellar_mass(struct GALAXY *galaxy, double value);
extern void galaxy_set_blackhole_mass(struct GALAXY *galaxy, double value);
extern void galaxy_set_cold_gas(struct GALAXY *galaxy, double value);
extern void galaxy_set_hot_gas(struct GALAXY *galaxy, double value);
extern void galaxy_set_ejected_mass(struct GALAXY *galaxy, double value);
extern void galaxy_set_metals_stellar_mass(struct GALAXY *galaxy, double value);
extern void galaxy_set_metals_cold_gas(struct GALAXY *galaxy, double value);
extern void galaxy_set_metals_hot_gas(struct GALAXY *galaxy, double value);
extern void galaxy_set_metals_ejected_mass(struct GALAXY *galaxy, double value);
extern void galaxy_set_bulge_mass(struct GALAXY *galaxy, double value);
extern void galaxy_set_metals_bulge_mass(struct GALAXY *galaxy, double value);
extern void galaxy_set_ics(struct GALAXY *galaxy, double value);
extern void galaxy_set_metals_ics(struct GALAXY *galaxy, double value);
extern void galaxy_set_cooling_rate(struct GALAXY *galaxy, double value);
extern void galaxy_set_heating_rate(struct GALAXY *galaxy, double value);
extern void galaxy_set_outflow_rate(struct GALAXY *galaxy, double value);
extern void galaxy_set_totalsatellitebaryons(struct GALAXY *galaxy, double value);

// Add declarations for other standard properties as needed...

#ifdef __cplusplus
}
#endif

#endif // CORE_GALAXY_ACCESSORS_H