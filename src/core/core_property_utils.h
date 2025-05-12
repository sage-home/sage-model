#pragma once

#include "core_allvars.h"
#include "core_properties.h" // Will be generated, so might not exist yet

#ifndef GALAXY_PROP_BY_ID
#define GALAXY_PROP_BY_ID(g, pid, type) (((type*)(g)->properties)[(pid)])
#endif

// Forward declaration if core_properties.h or its dependencies are not yet fully available
// For example, if property_id_t is defined there.
// If GALAXY struct is defined in core_allvars.h, it's fine.
// If property_id_t is defined in a file included by core_properties.h, ensure that's robust.
// Consider a minimal forward declaration or include for property_id_t if not in core_allvars.h
// For now, assuming property_id_t will be available via core_properties.h -> core_galaxy_properties.h

/* Get property by ID with error checking */
float get_float_property(struct GALAXY *galaxy, property_id_t prop_id, float default_value);
int32_t get_int32_property(struct GALAXY *galaxy, property_id_t prop_id, int32_t default_value);
double get_double_property(struct GALAXY *galaxy, property_id_t prop_id, double default_value);

/* Check if property exists */
// Assuming property_id_t is an enum or integral type that can be checked against a known range
// or a special "NOT_FOUND" value. The current GALAXY_PROP_BY_ID macro handles out-of-bounds access
// by erroring or returning a default, so this function provides a cleaner check.
bool has_property(struct GALAXY *galaxy, property_id_t prop_id);

/* Get property ID by name with caching */
// This function will need access to the property metadata generated in core_properties.c
property_id_t get_cached_property_id(const char *name);

// It's good practice to also declare any helper static functions if they were in the .c file
// and might be useful for testing or other modules, though typically static functions are not in headers.
// For now, only public API.
