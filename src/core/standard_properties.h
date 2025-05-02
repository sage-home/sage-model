/**
 * @file standard_properties.h
 * @brief Integration between property system and extension system
 *
 * This file provides the interface for registering standard properties
 * defined in core_properties.h with the extension system.
 */

#ifndef STANDARD_PROPERTIES_H
#define STANDARD_PROPERTIES_H

#include "core_properties.h"
#include "core_galaxy_extensions.h"

/**
 * @brief Mapping from property ID to extension ID
 */
extern int standard_property_to_extension_id[PROP_COUNT];

/**
 * @brief Get standard property ID by name
 * @param name Property name
 * @return property_id_t Property ID or PROP_COUNT if not found
 */
property_id_t get_standard_property_id_by_name(const char *name);

/**
 * @brief Get extension ID for a standard property
 * @param property_id Standard property ID
 * @return int Extension ID or -1 if not registered
 */
int get_extension_id_for_standard_property(property_id_t property_id);

/**
 * @brief Register all standard properties with the extension system
 * @return int 0 on success, negative error code on failure
 */
int register_standard_properties(void);

#endif /* STANDARD_PROPERTIES_H */
