#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "core_allvars.h"

/**
 * @file core_module_system.h
 * @brief Base module system for SAGE
 * 
 * This file defines the core module system infrastructure for SAGE.
 * It provides the base module interface that all physics modules will extend,
 * along with utilities for module registration, validation, and lifecycle management.
 */

/* Maximum number of modules that can be registered */
#define MAX_MODULES 32
#define MAX_MODULE_NAME 64
#define MAX_MODULE_VERSION 32
#define MAX_MODULE_AUTHOR 64
#define MAX_ERROR_MESSAGE 256

/**
 * Module type identifiers
 * 
 * Each physics module has a unique type identifier that determines what
 * interface it implements and where it fits in the physics pipeline.
 */
enum module_type {
    MODULE_TYPE_UNKNOWN = 0,
    MODULE_TYPE_COOLING = 1,
    MODULE_TYPE_STAR_FORMATION = 2,
    MODULE_TYPE_FEEDBACK = 3,
    MODULE_TYPE_AGN = 4,
    MODULE_TYPE_MERGERS = 5,
    MODULE_TYPE_DISK_INSTABILITY = 6,
    MODULE_TYPE_REINCORPORATION = 7,
    MODULE_TYPE_INFALL = 8,
    MODULE_TYPE_MISC = 9,
    /* Add other types as needed */
    MODULE_TYPE_MAX
};

/**
 * Module status codes
 * 
 * Return codes used by module functions to indicate success or failure
 */
enum module_status {
    MODULE_STATUS_SUCCESS = 0,
    MODULE_STATUS_ERROR = -1,
    MODULE_STATUS_INVALID_ARGS = -2,
    MODULE_STATUS_NOT_IMPLEMENTED = -3,
    MODULE_STATUS_INCOMPATIBLE_VERSION = -4,
    MODULE_STATUS_INITIALIZATION_FAILED = -5,
    MODULE_STATUS_ALREADY_INITIALIZED = -6,
    MODULE_STATUS_NOT_INITIALIZED = -7,
    MODULE_STATUS_OUT_OF_MEMORY = -8
};

/**
 * Base module interface
 * 
 * This structure defines the common interface that all physics modules must implement.
 * It includes metadata, lifecycle functions, and error handling.
 */
struct base_module {
    /* Metadata */
    char name[MAX_MODULE_NAME];           /* Module name */
    char version[MAX_MODULE_VERSION];     /* Module version string */
    char author[MAX_MODULE_AUTHOR];       /* Module author(s) */
    int module_id;                        /* Unique runtime ID assigned during registration */
    enum module_type type;                /* Type of physics module */
    
    /* Lifecycle functions */
    int (*initialize)(struct params *params, void **module_data);
    int (*cleanup)(void *module_data);
    
    /* Error handling */
    int last_error;                       /* Last error code */
    char error_message[MAX_ERROR_MESSAGE]; /* Last error message */
};

/**
 * Module registry
 * 
 * This structure keeps track of all registered modules and their current state.
 */
struct module_registry {
    int num_modules;                      /* Total number of registered modules */
    
    /* Array of loaded modules */
    struct {
        struct base_module *module;       /* Pointer to the module interface */
        void *module_data;                /* Module-specific data */
        bool initialized;                 /* Whether the module has been initialized */
        bool active;                      /* Whether the module is active */
    } modules[MAX_MODULES];
    
    /* Quick lookup for modules by type */
    struct {
        enum module_type type;            /* Module type */
        int module_index;                 /* Index into modules array */
    } active_modules[MODULE_TYPE_MAX];
    int num_active_types;                 /* Number of active module types */
};

/* Global module registry */
extern struct module_registry *global_module_registry;

/**
 * Initialize the module system
 * 
 * Sets up the global module registry and prepares it for module registration.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_initialize(void);

/**
 * Clean up the module system
 * 
 * Releases resources used by the module system and unregisters all modules.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_cleanup(void);

/**
 * Register a module with the system
 * 
 * Adds a module to the global registry and assigns it a unique ID.
 * 
 * @param module Pointer to the module interface to register
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_register(struct base_module *module);

/**
 * Unregister a module from the system
 * 
 * Removes a module from the global registry.
 * 
 * @param module_id ID of the module to unregister
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_unregister(int module_id);

/**
 * Initialize a module
 * 
 * Calls the initialize function of a registered module.
 * 
 * @param module_id ID of the module to initialize
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_initialize(int module_id, struct params *params);

/**
 * Clean up a module
 * 
 * Calls the cleanup function of a registered module.
 * 
 * @param module_id ID of the module to clean up
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_cleanup(int module_id);

/**
 * Get a module by ID
 * 
 * Retrieves a pointer to a module's interface and data.
 * 
 * @param module_id ID of the module to retrieve
 * @param module Output pointer to the module interface
 * @param module_data Output pointer to the module's data
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get(int module_id, struct base_module **module, void **module_data);

/**
 * Get active module by type
 * 
 * Retrieves the active module of a specific type.
 * 
 * @param type Type of module to retrieve
 * @param module Output pointer to the module interface
 * @param module_data Output pointer to the module's data
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_active_by_type(enum module_type type, struct base_module **module, void **module_data);

/**
 * Validate a base module
 * 
 * Checks that a module interface is valid and properly formed.
 * 
 * @param module Pointer to the module interface to validate
 * @return true if the module is valid, false otherwise
 */
bool module_validate(const struct base_module *module);

/**
 * Set a module as active
 * 
 * Marks a module as the active implementation for its type.
 * 
 * @param module_id ID of the module to activate
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_active(int module_id);

/**
 * Get the last error from a module
 * 
 * Retrieves the last error code and message from a module.
 * 
 * @param module_id ID of the module to get the error from
 * @param error_message Buffer to store the error message
 * @param max_length Maximum length of the error message buffer
 * @return Last error code
 */
int module_get_last_error(int module_id, char *error_message, size_t max_length);

/**
 * Set error information in a module
 * 
 * Updates the error state of a module.
 * 
 * @param module Pointer to the module interface
 * @param error_code Error code to set
 * @param error_message Error message to set
 */
void module_set_error(struct base_module *module, int error_code, const char *error_message);

#ifdef __cplusplus
}
#endif
