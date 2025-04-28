#pragma once

#include "core_allvars.h"

/**
 * @file core_galaxy_accessors.h
 * @brief Generic accessor interface for galaxy properties
 * 
 * This file provides a modular accessor system for galaxy properties.
 * All physics-specific accessors should be defined in their respective modules,
 * not in the core. This file only defines the infrastructure for module-based
 * property access.
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
