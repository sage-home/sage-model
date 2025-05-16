#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "core_property_types.h"  // For property_id_t

/**
 * @file core_property_descriptor.h
 * @brief Definition of property descriptor data structures for SAGE modules.
 * 
 * This file defines the data structures used by physics modules to describe
 * the properties they want to register with the core system. It was formerly
 * named core_properties.h but was renamed to avoid conflicts with the auto-generated
 * core_properties.h file that contains the core property macros.
 */

// Maximum number of galaxy properties allowed in the system
#define MAX_GALAXY_PROPERTIES 256

/**
 * Structure describing a galaxy property for registration by modules.
 * 
 * Modules use this structure to inform the core system about the properties
 * they want to register. The core system then creates a corresponding
 * galaxy_property_t based on this information and registers it.
 */
typedef struct {
    char name[64];           // Property name (e.g., "HotGas", "StellarMass")
    char units[32];          // Units for the property (e.g., "10^10 Msun/h")
    char description[256];   // Human-readable description of the property
    char type_str[32];       // Type string (e.g., "float", "double", "int32", "float[3]")
    bool is_array;           // Whether this is an array type
    bool is_dynamic_array;   // Whether this is a dynamic array (size determined at runtime)
    size_t element_size;     // Size of a single element (e.g., sizeof(float))
    int array_len;           // For fixed arrays, number of elements
    void *default_value;     // Optional default value (NULL if not provided)
    bool output_field;       // Whether this field should be included in output
    int read_only_level;     // Read-only level: 0=read-write, >0=read-only
} GalaxyPropertyInfo;

// Global array of property information defined by core_properties.c
extern const GalaxyPropertyInfo galaxy_property_info[];

#ifdef __cplusplus
}
#endif
