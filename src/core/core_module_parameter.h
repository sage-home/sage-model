#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_allvars.h"
/* Do not include core_module_system.h here to avoid circular dependencies */

/**
 * @file core_module_parameter.h
 * @brief Parameter tuning system for SAGE modules
 * 
 * This file defines the parameter tuning system used by SAGE modules.
 * It provides functionality for registering, validating, and modifying
 * module-specific parameters at runtime, with type safety and bounds checking.
 */

/* Constants for parameter system */
#define MAX_PARAM_NAME 64
#define MAX_PARAM_STRING 256
#define MAX_PARAM_DESCRIPTION 256
#define MAX_PARAM_UNITS 32
#define MAX_MODULE_PARAMETERS 128
#define MAX_ERROR_MESSAGE_LENGTH 256

/**
 * Parameter status codes
 * 
 * Return codes used by parameter functions to indicate success or failure
 */
enum module_parameter_status {
    MODULE_PARAM_SUCCESS = 0,
    MODULE_PARAM_ERROR = -1,
    MODULE_PARAM_INVALID_ARGS = -2,
    MODULE_PARAM_NOT_FOUND = -3,
    MODULE_PARAM_TYPE_MISMATCH = -4,
    MODULE_PARAM_OUT_OF_BOUNDS = -5,
    MODULE_PARAM_OUT_OF_MEMORY = -6,
    MODULE_PARAM_ALREADY_EXISTS = -7,
    MODULE_PARAM_FILE_ERROR = -8,
    MODULE_PARAM_PARSE_ERROR = -9
};

/**
 * Parameter types
 * 
 * Supported parameter data types
 */
enum module_parameter_type {
    MODULE_PARAM_TYPE_INT = 0,
    MODULE_PARAM_TYPE_FLOAT = 1,
    MODULE_PARAM_TYPE_DOUBLE = 2,
    MODULE_PARAM_TYPE_BOOL = 3,
    MODULE_PARAM_TYPE_STRING = 4
};

/**
 * Parameter structure
 * 
 * Contains all information about a module parameter
 */
struct module_parameter {
    char name[MAX_PARAM_NAME];           /* Parameter name */
    enum module_parameter_type type;     /* Parameter data type */
    
    /* Value limits (for numeric types) */
    union {
        struct { int min; int max; } int_range;
        struct { float min; float max; } float_range;
        struct { double min; double max; } double_range;
    } limits;
    
    /* Parameter value */
    union {
        int int_val;
        float float_val;
        double double_val;
        bool bool_val;
        char string_val[MAX_PARAM_STRING];
    } value;
    
    bool has_limits;                     /* Whether bounds checking is enabled */
    char description[MAX_PARAM_DESCRIPTION]; /* Parameter description */
    char units[MAX_PARAM_UNITS];         /* Parameter units (e.g., "Mpc/h") */
    int module_id;                       /* ID of module that owns this parameter */
};
typedef struct module_parameter module_parameter_t;

/**
 * Parameter registry
 * 
 * Stores all parameters for a module
 */
struct module_parameter_registry {
    module_parameter_t *parameters;      /* Array of parameters */
    int num_parameters;                  /* Number of registered parameters */
    int capacity;                        /* Total capacity of the registry */
};
typedef struct module_parameter_registry module_parameter_registry_t;

/**
 * Initialize a parameter registry
 * 
 * Allocates and initializes a new parameter registry.
 * 
 * @param registry Pointer to the registry to initialize
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_parameter_registry_init(module_parameter_registry_t *registry);

/**
 * Free a parameter registry
 * 
 * Releases resources used by a parameter registry.
 * 
 * @param registry Pointer to the registry to free
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_parameter_registry_free(module_parameter_registry_t *registry);

/**
 * Create a parameter registry
 * 
 * Allocates and creates a new parameter registry.
 * 
 * @return Pointer to the new registry, or NULL on failure
 */
module_parameter_registry_t *module_parameter_registry_create(void);

/**
 * Destroy a parameter registry
 * 
 * Frees a parameter registry and all its resources.
 * 
 * @param registry Pointer to the registry to destroy
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_parameter_registry_destroy(module_parameter_registry_t *registry);

/**
 * Register a parameter with a registry
 * 
 * Adds a new parameter to a registry.
 * 
 * @param registry Pointer to the registry
 * @param param Pointer to the parameter to register
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_register_parameter(module_parameter_registry_t *registry, const module_parameter_t *param);

/**
 * Find a parameter in a registry
 * 
 * Looks up a parameter by name and module ID.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter to find
 * @param module_id ID of the module that owns the parameter
 * @return Parameter index if found, MODULE_PARAM_NOT_FOUND if not found
 */
int module_find_parameter(module_parameter_registry_t *registry, const char *name, int module_id);

/**
 * Get a parameter by index
 * 
 * Retrieves a parameter by its index in the registry.
 * 
 * @param registry Pointer to the registry
 * @param index Index of the parameter
 * @param param Output pointer to store the parameter
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_get_parameter_by_index(module_parameter_registry_t *registry, int index, module_parameter_t *param);

/**
 * Get a parameter by name
 * 
 * Retrieves a parameter by its name and module ID.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param param Output pointer to store the parameter
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_get_parameter(module_parameter_registry_t *registry, const char *name, int module_id, module_parameter_t *param);

/**
 * Validate a parameter
 * 
 * Checks that a parameter is valid.
 * 
 * @param param Pointer to the parameter to validate
 * @return true if the parameter is valid, false otherwise
 */
bool module_validate_parameter(const module_parameter_t *param);

/**
 * Check parameter bounds
 * 
 * Verifies that a parameter's value is within its defined bounds.
 * 
 * @param param Pointer to the parameter to check
 * @return true if within bounds or no bounds defined, false if out of bounds
 */
bool module_check_parameter_bounds(const module_parameter_t *param);

/**
 * Convert parameter type to string
 * 
 * Returns a string representation of a parameter type.
 * 
 * @param type Parameter type
 * @return String representation of the type
 */
const char *module_parameter_type_to_string(enum module_parameter_type type);

/**
 * Parse parameter type from string
 * 
 * Converts a string representation to a parameter type.
 * 
 * @param type_str String representation of the type
 * @return Parameter type, or MODULE_PARAM_TYPE_INT if not recognized
 */
enum module_parameter_type module_parameter_type_from_string(const char *type_str);

/* Type-specific parameter get functions */

/**
 * Get an integer parameter value
 * 
 * Retrieves an integer parameter value with type checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Output pointer to store the value
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_get_parameter_int(module_parameter_registry_t *registry, const char *name, int module_id, int *value);

/**
 * Get a float parameter value
 * 
 * Retrieves a float parameter value with type checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Output pointer to store the value
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_get_parameter_float(module_parameter_registry_t *registry, const char *name, int module_id, float *value);

/**
 * Get a double parameter value
 * 
 * Retrieves a double parameter value with type checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Output pointer to store the value
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_get_parameter_double(module_parameter_registry_t *registry, const char *name, int module_id, double *value);

/**
 * Get a boolean parameter value
 * 
 * Retrieves a boolean parameter value with type checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Output pointer to store the value
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_get_parameter_bool(module_parameter_registry_t *registry, const char *name, int module_id, bool *value);

/**
 * Get a string parameter value
 * 
 * Retrieves a string parameter value with type checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Output buffer to store the value
 * @param max_len Maximum length of the output buffer
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_get_parameter_string(module_parameter_registry_t *registry, const char *name, int module_id, char *value, size_t max_len);

/* Type-specific parameter set functions */

/**
 * Set an integer parameter value
 * 
 * Sets an integer parameter value with type and bounds checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Value to set
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_set_parameter_int(module_parameter_registry_t *registry, const char *name, int module_id, int value);

/**
 * Set a float parameter value
 * 
 * Sets a float parameter value with type and bounds checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Value to set
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_set_parameter_float(module_parameter_registry_t *registry, const char *name, int module_id, float value);

/**
 * Set a double parameter value
 * 
 * Sets a double parameter value with type and bounds checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Value to set
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_set_parameter_double(module_parameter_registry_t *registry, const char *name, int module_id, double value);

/**
 * Set a boolean parameter value
 * 
 * Sets a boolean parameter value with type checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Value to set
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_set_parameter_bool(module_parameter_registry_t *registry, const char *name, int module_id, bool value);

/**
 * Set a string parameter value
 * 
 * Sets a string parameter value with type checking.
 * 
 * @param registry Pointer to the registry
 * @param name Name of the parameter
 * @param module_id ID of the module that owns the parameter
 * @param value Value to set
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_set_parameter_string(module_parameter_registry_t *registry, const char *name, int module_id, const char *value);

/* Parameter import/export functions */

/**
 * Load parameters from a file
 * 
 * Reads parameters from a file into a registry.
 * 
 * @param registry Pointer to the registry
 * @param filename Path to the parameter file
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_load_parameters_from_file(module_parameter_registry_t *registry, const char *filename);

/**
 * Save parameters to a file
 * 
 * Writes parameters from a registry to a file.
 * 
 * @param registry Pointer to the registry
 * @param filename Path to the output file
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_save_parameters_to_file(module_parameter_registry_t *registry, const char *filename);

/* Helper functions for creating parameters */

/**
 * Create an integer parameter
 * 
 * Initializes a parameter with integer type.
 * 
 * @param param Output parameter to initialize
 * @param name Parameter name
 * @param value Initial value
 * @param min Minimum allowed value (for bounds checking)
 * @param max Maximum allowed value (for bounds checking)
 * @param description Parameter description
 * @param units Parameter units
 * @param module_id ID of the module that owns the parameter
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_create_parameter_int(module_parameter_t *param, const char *name, int value,
                                int min, int max, const char *description,
                                const char *units, int module_id);

/**
 * Create a float parameter
 * 
 * Initializes a parameter with float type.
 * 
 * @param param Output parameter to initialize
 * @param name Parameter name
 * @param value Initial value
 * @param min Minimum allowed value (for bounds checking)
 * @param max Maximum allowed value (for bounds checking)
 * @param description Parameter description
 * @param units Parameter units
 * @param module_id ID of the module that owns the parameter
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_create_parameter_float(module_parameter_t *param, const char *name, float value,
                                  float min, float max, const char *description,
                                  const char *units, int module_id);

/**
 * Create a double parameter
 * 
 * Initializes a parameter with double type.
 * 
 * @param param Output parameter to initialize
 * @param name Parameter name
 * @param value Initial value
 * @param min Minimum allowed value (for bounds checking)
 * @param max Maximum allowed value (for bounds checking)
 * @param description Parameter description
 * @param units Parameter units
 * @param module_id ID of the module that owns the parameter
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_create_parameter_double(module_parameter_t *param, const char *name, double value,
                                   double min, double max, const char *description,
                                   const char *units, int module_id);

/**
 * Create a boolean parameter
 * 
 * Initializes a parameter with boolean type.
 * 
 * @param param Output parameter to initialize
 * @param name Parameter name
 * @param value Initial value
 * @param description Parameter description
 * @param module_id ID of the module that owns the parameter
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_create_parameter_bool(module_parameter_t *param, const char *name, bool value,
                                 const char *description, int module_id);

/**
 * Create a string parameter
 * 
 * Initializes a parameter with string type.
 * 
 * @param param Output parameter to initialize
 * @param name Parameter name
 * @param value Initial value
 * @param description Parameter description
 * @param module_id ID of the module that owns the parameter
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_create_parameter_string(module_parameter_t *param, const char *name, const char *value,
                                   const char *description, int module_id);

#ifdef __cplusplus
}
#endif