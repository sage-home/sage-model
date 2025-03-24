#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core_allvars.h"

/**
 * @file core_galaxy_extensions.h
 * @brief Galaxy property extension system for SAGE
 * 
 * This file defines the extension system that allows modules to add custom
 * properties to galaxies without modifying the core GALAXY structure.
 */

/* Forward declaration to avoid circular dependencies */
#define MAX_MODULES 32

/* Maximum number of extension properties that can be registered */
#define MAX_GALAXY_EXTENSIONS 64
#define MAX_PROPERTY_NAME 32
#define MAX_PROPERTY_DESCRIPTION 128
#define MAX_PROPERTY_UNITS 32

/**
 * Galaxy property data type identifiers
 * 
 * Used to identify the type of a galaxy property for serialization
 * and validation purposes.
 */
enum galaxy_property_type {
    PROPERTY_TYPE_FLOAT = 0,
    PROPERTY_TYPE_DOUBLE = 1,
    PROPERTY_TYPE_INT32 = 2,
    PROPERTY_TYPE_INT64 = 3,
    PROPERTY_TYPE_UINT32 = 4,
    PROPERTY_TYPE_UINT64 = 5,
    PROPERTY_TYPE_BOOL = 6,
    PROPERTY_TYPE_STRUCT = 7,  /* Custom struct types */
    PROPERTY_TYPE_ARRAY = 8,   /* Array types */
    PROPERTY_TYPE_MAX
};

/**
 * Galaxy property flags
 * 
 * Flags that can be set on galaxy properties to control
 * behavior during I/O, initialization, etc.
 */
enum galaxy_property_flags {
    PROPERTY_FLAG_NONE = 0,
    PROPERTY_FLAG_SERIALIZE = (1 << 0),  /* Property should be saved in output */
    PROPERTY_FLAG_INITIALIZE = (1 << 1), /* Property should be initialized to 0/NULL */
    PROPERTY_FLAG_REQUIRED = (1 << 2),   /* Property is required for module function */
    PROPERTY_FLAG_READONLY = (1 << 3),   /* Property should not be modified */
    PROPERTY_FLAG_DERIVED = (1 << 4)     /* Property is derived from other properties */
};

/**
 * Galaxy property registration information
 * 
 * This structure defines a galaxy property extension that can be
 * registered by modules to extend the GALAXY structure.
 */
typedef struct {
    char name[MAX_PROPERTY_NAME];            /* Property name */
    size_t size;                             /* Size in bytes */
    int module_id;                           /* Which module owns this */
    int extension_id;                        /* Assigned extension ID */
    enum galaxy_property_type type;          /* Property data type */
    uint32_t flags;                          /* Property flags */
    
    /* Serialization functions (can be NULL if not serializable) */
    void (*serialize)(const void *src, void *dest, int count);
    void (*deserialize)(const void *src, void *dest, int count);
    
    /* Metadata (optional) */
    char description[MAX_PROPERTY_DESCRIPTION];
    char units[MAX_PROPERTY_UNITS];
} galaxy_property_t;

/**
 * Galaxy extension registry
 * 
 * This structure keeps track of all registered galaxy properties and
 * manages the allocation of extension data for galaxies.
 */
struct galaxy_extension_registry {
    int num_extensions;                           /* Total number of registered extensions */
    galaxy_property_t extensions[MAX_GALAXY_EXTENSIONS]; /* Extension definitions */
    
    /* Extension data lookup by module ID */
    struct {
        int module_id;                      /* Module ID that owns the extension */
        int first_extension;                /* First extension ID for this module */
        int num_extensions;                 /* Number of extensions registered by module */
    } module_extensions[MAX_MODULES];
    int num_module_extensions;              /* Count of modules with registered extensions */
};

/* Global extension registry */
extern struct galaxy_extension_registry *global_extension_registry;

/**
 * Initialize the galaxy extension system
 * 
 * Sets up the global extension registry and prepares it for property registration.
 * 
 * @return 0 on success, error code on failure
 */
int galaxy_extension_system_initialize(void);

/**
 * Clean up the galaxy extension system
 * 
 * Releases resources used by the extension system.
 * 
 * @return 0 on success, error code on failure
 */
int galaxy_extension_system_cleanup(void);

/**
 * Register a galaxy property extension
 * 
 * Adds a property extension to the global registry and assigns it an ID.
 * 
 * @param property Pointer to the property definition
 * @return Extension ID on success, negative error code on failure
 */
int galaxy_extension_register(galaxy_property_t *property);

/**
 * Unregister a galaxy property extension
 * 
 * Removes a property extension from the global registry.
 * 
 * @param extension_id ID of the extension to unregister
 * @return 0 on success, error code on failure
 */
int galaxy_extension_unregister(int extension_id);

/**
 * Initialize extension data for a galaxy
 * 
 * Allocates and initializes extension data for a galaxy.
 * 
 * @param galaxy Pointer to the galaxy to initialize
 * @return 0 on success, error code on failure
 */
int galaxy_extension_initialize(struct GALAXY *galaxy);

/**
 * Clean up extension data for a galaxy
 * 
 * Releases extension data associated with a galaxy.
 * 
 * @param galaxy Pointer to the galaxy to clean up
 * @return 0 on success, error code on failure
 */
int galaxy_extension_cleanup(struct GALAXY *galaxy);

/**
 * Get extension data for a galaxy property
 * 
 * Retrieves a pointer to the extension data for a specific property.
 * 
 * @param galaxy Pointer to the galaxy
 * @param extension_id ID of the extension to retrieve
 * @return Pointer to the extension data, or NULL if not found
 */
void *galaxy_extension_get_data(const struct GALAXY *galaxy, int extension_id);

/**
 * Get extension data for a galaxy property by module ID
 * 
 * Retrieves a pointer to the extension data for a module.
 * 
 * @param galaxy Pointer to the galaxy
 * @param module_id ID of the module that owns the extension
 * @param extension_offset Offset within the module's extensions (0 for first extension)
 * @return Pointer to the extension data, or NULL if not found
 */
void *galaxy_extension_get_data_by_module(const struct GALAXY *galaxy, int module_id, int extension_offset);

/**
 * Get extension data for a galaxy property by name
 * 
 * Retrieves a pointer to the extension data for a named property.
 * 
 * @param galaxy Pointer to the galaxy
 * @param property_name Name of the property to retrieve
 * @return Pointer to the extension data, or NULL if not found
 */
void *galaxy_extension_get_data_by_name(const struct GALAXY *galaxy, const char *property_name);

/**
 * Find a galaxy property by name
 * 
 * Looks up a property in the extension registry by name.
 * 
 * @param property_name Name of the property to find
 * @return Pointer to the property definition, or NULL if not found
 */
const galaxy_property_t *galaxy_extension_find_property(const char *property_name);

/**
 * Find a galaxy property by extension ID
 * 
 * Looks up a property in the extension registry by ID.
 * 
 * @param extension_id ID of the extension to find
 * @return Pointer to the property definition, or NULL if not found
 */
const galaxy_property_t *galaxy_extension_find_property_by_id(int extension_id);

/**
 * Find galaxy properties by module ID
 * 
 * Gets all properties registered by a specific module.
 * 
 * @param module_id ID of the module
 * @param properties Output array to store property pointers
 * @param max_properties Maximum number of properties to return
 * @return Number of properties found, or negative error code
 */
int galaxy_extension_find_properties_by_module(int module_id, const galaxy_property_t **properties, int max_properties);

/**
 * Copy extension data from one galaxy to another
 * 
 * Copies all extension data from the source galaxy to the destination galaxy.
 * 
 * @param dest Pointer to the destination galaxy
 * @param src Pointer to the source galaxy
 * @return 0 on success, error code on failure
 */
int galaxy_extension_copy(struct GALAXY *dest, const struct GALAXY *src);

/**
 * Validate a galaxy property definition
 * 
 * Checks that a property definition is valid and properly formed.
 * 
 * @param property Pointer to the property definition to validate
 * @return true if the property is valid, false otherwise
 */
bool galaxy_extension_validate_property(const galaxy_property_t *property);

/* Convenience macro to access extension data with type casting */
#define GALAXY_EXT(galaxy, extension_id, type) ((type *)galaxy_extension_get_data(galaxy, extension_id))

/* Convenience macro to access extension data by module ID with type casting */
#define GALAXY_EXT_BY_MODULE(galaxy, module_id, extension_offset, type) \
    ((type *)galaxy_extension_get_data_by_module(galaxy, module_id, extension_offset))

/* Convenience macro to access extension data by name with type casting */
#define GALAXY_EXT_BY_NAME(galaxy, property_name, type) \
    ((type *)galaxy_extension_get_data_by_name(galaxy, property_name))

#ifdef __cplusplus
}
#endif