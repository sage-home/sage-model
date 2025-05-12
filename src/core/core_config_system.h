#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "core_allvars.h"
#include "core_property_types.h" // For property_id_t
#include "core_property_descriptor.h"  // For GalaxyPropertyInfo
#include "core_module_system.h"  // For module_list and module_functions
#include "core_pipeline_system.h"

/**
 * @file core_config_system.h
 * @brief Configuration system for SAGE
 * 
 * This file defines the configuration system for SAGE, which provides
 * a flexible way to configure the model's parameters, modules, and
 * pipeline. The configuration is loaded from JSON files and can be
 * overridden by command-line options.
 */

/* Maximum lengths for configuration paths and values */
#define MAX_CONFIG_PATH 256
#define MAX_CONFIG_VALUE 1024
#define MAX_CONFIG_FILE_SIZE (1024 * 1024) /* 1 MB */
#define MAX_CONFIG_OVERRIDE_ARGS 128

/**
 * Configuration value types
 */
enum config_value_type {
    CONFIG_VALUE_NULL,
    CONFIG_VALUE_BOOLEAN,
    CONFIG_VALUE_INTEGER,
    CONFIG_VALUE_DOUBLE,
    CONFIG_VALUE_STRING,
    CONFIG_VALUE_OBJECT,
    CONFIG_VALUE_ARRAY
};

/**
 * Forward declaration of configuration object
 */
struct config_object;

/**
 * Configuration value structure
 * 
 * Contains a typed value from the configuration
 */
struct config_value {
    enum config_value_type type;
    union {
        bool boolean;
        int64_t integer;
        double floating;
        char *string;
        struct config_object *object;
        struct {
            struct config_value **items;
            int count;
            int capacity;
        } array;
    } u;
};

/**
 * Configuration object structure
 * 
 * Represents a set of key-value pairs in the configuration
 */
struct config_object {
    struct {
        char *key;
        struct config_value value;
    } *entries;
    int count;
    int capacity;
};

/**
 * Configuration system
 * 
 * Manages the loaded configuration for the system
 */
struct config_system {
    struct config_object *root;       /* Root configuration object */
    char *filename;                   /* Path to the loaded config file */
    bool initialized;                 /* Whether the system is initialized */
    
    /* Override arguments */
    struct {
        char path[MAX_CONFIG_PATH];
        char value[MAX_CONFIG_VALUE];
    } overrides[MAX_CONFIG_OVERRIDE_ARGS];
    int num_overrides;
};

/* Global configuration system instance */
extern struct config_system *global_config;

/**
 * Initialize the configuration system
 * 
 * Sets up the configuration system and loads the default configuration.
 * 
 * @return 0 on success, error code on failure
 */
int config_system_initialize(void);

/**
 * Clean up the configuration system
 * 
 * Releases resources used by the configuration system.
 * 
 * @return 0 on success, error code on failure
 */
int config_system_cleanup(void);

/**
 * Load configuration from a file
 * 
 * Reads and parses a JSON configuration file.
 * 
 * @param filename Path to the configuration file
 * @return 0 on success, error code on failure
 */
int config_load_file(const char *filename);

/**
 * Save configuration to a file
 * 
 * Writes the current configuration to a JSON file.
 * 
 * @param filename Path where to save the configuration
 * @param pretty Whether to format the JSON with indentation
 * @return 0 on success, error code on failure
 */
int config_save_file(const char *filename, bool pretty);

/**
 * Get a boolean value from the configuration
 * 
 * Retrieves a boolean value at the specified path.
 * 
 * @param path Path to the configuration value
 * @param default_value Value to return if the path isn't found
 * @return The boolean value, or default_value if not found
 */
bool config_get_boolean(const char *path, bool default_value);

/**
 * Get an integer value from the configuration
 * 
 * Retrieves an integer value at the specified path.
 * 
 * @param path Path to the configuration value
 * @param default_value Value to return if the path isn't found
 * @return The integer value, or default_value if not found
 */
int config_get_integer(const char *path, int default_value);

/**
 * Get a double value from the configuration
 * 
 * Retrieves a floating-point value at the specified path.
 * 
 * @param path Path to the configuration value
 * @param default_value Value to return if the path isn't found
 * @return The double value, or default_value if not found
 */
double config_get_double(const char *path, double default_value);

/**
 * Get a string value from the configuration
 * 
 * Retrieves a string value at the specified path.
 * 
 * @param path Path to the configuration value
 * @param default_value Value to return if the path isn't found
 * @return The string value, or default_value if not found
 */
const char *config_get_string(const char *path, const char *default_value);

/**
 * Get a configuration value
 * 
 * Retrieves a config_value structure at the specified path.
 * 
 * @param path Path to the configuration value
 * @return Pointer to the config_value, or NULL if not found
 */
const struct config_value *config_get_value(const char *path);

/**
 * Get an array size
 * 
 * Returns the number of elements in an array at the specified path.
 * 
 * @param path Path to the array
 * @return Number of elements, or -1 if the path doesn't point to an array
 */
int config_get_array_size(const char *path);

/**
 * Get an array element
 * 
 * Retrieves a specific element from an array at the specified path.
 * 
 * @param path Path to the array
 * @param index Index of the element to retrieve
 * @return Pointer to the element, or NULL if not found
 */
const struct config_value *config_get_array_element(const char *path, int index);

/**
 * Set a boolean value in the configuration
 * 
 * Sets a boolean value at the specified path.
 * 
 * @param path Path where to set the value
 * @param value Boolean value to set
 * @return 0 on success, error code on failure
 */
int config_set_boolean(const char *path, bool value);

/**
 * Set an integer value in the configuration
 * 
 * Sets an integer value at the specified path.
 * 
 * @param path Path where to set the value
 * @param value Integer value to set
 * @return 0 on success, error code on failure
 */
int config_set_integer(const char *path, int value);

/**
 * Set a double value in the configuration
 * 
 * Sets a floating-point value at the specified path.
 * 
 * @param path Path where to set the value
 * @param value Double value to set
 * @return 0 on success, error code on failure
 */
int config_set_double(const char *path, double value);

/**
 * Set a string value in the configuration
 * 
 * Sets a string value at the specified path.
 * 
 * @param path Path where to set the value
 * @param value String value to set
 * @return 0 on success, error code on failure
 */
int config_set_string(const char *path, const char *value);

/**
 * Add a configuration value override
 * 
 * Adds a command-line override for a configuration value.
 * 
 * @param path Path to the configuration value
 * @param value String representation of the value
 * @return 0 on success, error code on failure
 */
int config_add_override(const char *path, const char *value);

/**
 * Apply configuration overrides
 * 
 * Applies all registered overrides to the configuration.
 * 
 * @return 0 on success, error code on failure
 */
int config_apply_overrides(void);

/**
 * Configure modules from the configuration
 * 
 * Sets up modules based on the current configuration.
 * 
 * @param params Global parameters structure
 * @return 0 on success, error code on failure
 */
int config_configure_modules(struct params *params);

/**
 * Configure pipeline from the configuration
 * 
 * Sets up the pipeline based on the current configuration.
 * 
 * @return 0 on success, error code on failure
 */
int config_configure_pipeline(void);

/**
 * Configure parameters from the configuration
 * 
 * Updates the params structure based on the current configuration.
 * 
 * @param params Global parameters structure
 * @return 0 on success, error code on failure
 */
int config_configure_params(struct params *params);

/**
 * Generate default configuration
 * 
 * Creates a default configuration structure.
 * 
 * @return Root configuration object
 */
struct config_object *config_generate_default(void);

/**
 * Create a configuration schema
 * 
 * Generates a JSON schema for the configuration.
 * 
 * @return 0 on success, error code on failure
 */
int config_create_schema(void);

#ifdef __cplusplus
}
#endif
