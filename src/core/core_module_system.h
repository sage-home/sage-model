#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "core_allvars.h"
#include "core_module_callback.h"

/**
 * @file core_module_system.h
 * @brief Base module system for SAGE
 * 
 * This file defines the core module system infrastructure for SAGE.
 * It provides the base module interface that all physics modules will extend,
 * along with utilities for module registration, validation, and lifecycle management.
 */

/* Maximum number of modules that can be registered */
#define MAX_MODULES 64
#define MAX_MODULE_NAME 64
#define MAX_MODULE_VERSION 32
#define MAX_MODULE_AUTHOR 64
#define MAX_MODULE_DESCRIPTION 256
#define MAX_ERROR_MESSAGE 256
#define MAX_DEPENDENCIES 16
#define MAX_DEPENDENCY_NAME 64
#define MAX_MODULE_PATH 256
#define MAX_MODULE_PATHS 10
#define MAX_MANIFEST_FILE_SIZE 8192
#define DEFAULT_MODULE_DIR "modules"

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
    MODULE_STATUS_OUT_OF_MEMORY = -8,
    MODULE_STATUS_MODULE_NOT_FOUND = -9,
    MODULE_STATUS_DEPENDENCY_NOT_FOUND = -10,
    MODULE_STATUS_DEPENDENCY_CONFLICT = -11,
    MODULE_STATUS_INVALID_MANIFEST = -12,
    MODULE_STATUS_MODULE_LOADING_FAILED = -13
};

/**
 * Version structure
 * 
 * Used for semantic versioning of modules
 */
struct module_version {
    int major;       /* Major version (incompatible API changes) */
    int minor;       /* Minor version (backwards-compatible functionality) */
    int patch;       /* Patch version (backwards-compatible bug fixes) */
};


/**
 * Module manifest structure
 * 
 * Contains metadata about a module from its manifest file
 */
struct module_manifest {
    char name[MAX_MODULE_NAME];           /* Module name */
    char version_str[MAX_MODULE_VERSION]; /* Version as a string */
    struct module_version version;        /* Parsed semantic version */
    char author[MAX_MODULE_AUTHOR];       /* Module author(s) */
    char description[MAX_MODULE_DESCRIPTION]; /* Module description */
    enum module_type type;                /* Type of physics module */
    char library_path[MAX_MODULE_PATH];   /* Path to the module library */
    
    /* Dependencies */
    struct module_dependency dependencies[MAX_DEPENDENCIES];
    int num_dependencies;
    
    /* Capabilities and API version */
    int api_version;                      /* Module API version */
    uint32_t capabilities;                /* Bitmap of module capabilities */
    
    /* Configuration */
    bool auto_initialize;                 /* Whether to auto-initialize on load */
    bool auto_activate;                   /* Whether to auto-activate on load */
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
    
    /* Configuration functions */
    int (*configure)(void *module_data, const char *config_name, const char *config_value);
    
    /* Error handling */
    int last_error;                       /* Last error code */
    char error_message[MAX_ERROR_MESSAGE]; /* Last error message */
    
    /* Module manifest */
    struct module_manifest *manifest;     /* Pointer to module manifest (if available) */
    
    /* Function registry (for callback system) */
    module_function_registry_t *function_registry; /* Functions available for calling by other modules */
    
    /* Runtime dependencies */
    module_dependency_t *dependencies;    /* Array of module dependencies */
    int num_dependencies;                 /* Number of dependencies */
};

/**
 * Module library handle
 * 
 * Platform-independent dynamic library handle
 */
typedef void* module_library_handle_t;

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
        bool dynamic;                     /* Whether this is a dynamically loaded module */
        module_library_handle_t handle;   /* Dynamic library handle (if dynamic) */
        char path[MAX_MODULE_PATH];       /* Path where the module was found */
    } modules[MAX_MODULES];
    
    /* Quick lookup for modules by type */
    struct {
        enum module_type type;            /* Module type */
        int module_index;                 /* Index into modules array */
    } active_modules[MODULE_TYPE_MAX];
    int num_active_types;                 /* Number of active module types */
    
    /* Module path configuration */
    char module_paths[MAX_MODULE_PATHS][MAX_MODULE_PATH]; /* Module search paths */
    int num_module_paths;                 /* Number of search paths */
    
    /* Configuration */
    bool discovery_enabled;               /* Whether module discovery is enabled */
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
 * Validate module runtime dependencies
 * 
 * Checks that all dependencies of a module are satisfied at runtime.
 * 
 * @param module_id ID of the module to validate
 * @return MODULE_STATUS_SUCCESS if all dependencies are met, error code otherwise
 */
int module_validate_runtime_dependencies(int module_id);

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

/**
 * Parse a semantic version string
 * 
 * Converts a version string (e.g., "1.2.3") to a module_version structure.
 * 
 * @param version_str Version string to parse
 * @param version Output version structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_parse_version(const char *version_str, struct module_version *version);

/**
 * Compare two semantic versions
 * 
 * Compares two version structures according to semver rules.
 * 
 * @param v1 First version to compare
 * @param v2 Second version to compare
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int module_compare_versions(const struct module_version *v1, const struct module_version *v2);

/**
 * Check version compatibility
 * 
 * Checks if a module version is compatible with required constraints.
 * 
 * @param version Version to check
 * @param min_version Minimum required version
 * @param max_version Maximum allowed version (can be NULL)
 * @param exact_match Whether an exact match is required
 * @return true if compatible, false otherwise
 */
bool module_check_version_compatibility(
    const struct module_version *version,
    const struct module_version *min_version,
    const struct module_version *max_version,
    bool exact_match);

/**
 * Load a module manifest from a file
 * 
 * Reads and parses a module manifest file.
 * 
 * @param filename Path to the manifest file
 * @param manifest Output manifest structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_load_manifest(const char *filename, struct module_manifest *manifest);

/**
 * Validate a module manifest
 * 
 * Checks that a manifest structure is valid and correctly formed.
 * 
 * @param manifest Manifest to validate
 * @return true if valid, false otherwise
 */
bool module_validate_manifest(const struct module_manifest *manifest);

/**
 * Configure the module system
 * 
 * Sets up the module system with the given parameters.
 * 
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_configure(struct params *params);

/**
 * Add a module search path
 * 
 * Adds a directory to the list of places to search for modules.
 * 
 * @param path Directory path to add
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_add_search_path(const char *path);

/**
 * Discover modules in search paths
 * 
 * Searches the configured paths for module manifests and loads them.
 * 
 * @param params Pointer to the global parameters structure
 * @return Number of modules discovered, or negative error code on failure
 */
int module_discover(struct params *params);

/**
 * Find a module by name
 * 
 * Searches the registry for a module with the given name.
 * 
 * @param name Name of the module to find
 * @return Module index if found, -1 if not found
 */
int module_find_by_name(const char *name);

/**
 * Load a module from a manifest
 * 
 * Loads a module based on its manifest information.
 * 
 * @param manifest Manifest describing the module
 * @param params Pointer to the global parameters structure
 * @return Module ID if successful, negative error code on failure
 */
int module_load_from_manifest(const struct module_manifest *manifest, struct params *params);

/**
 * Check and resolve module dependencies
 * 
 * Validates that all dependencies of a module are satisfied.
 * 
 * @param manifest Manifest containing the dependencies
 * @return MODULE_STATUS_SUCCESS if all dependencies are met, error code otherwise
 */
int module_check_dependencies(const struct module_manifest *manifest);

/**
 * Get module type name
 * 
 * Returns a string representation of a module type.
 * 
 * @param type Module type
 * @return String name of the module type
 */
const char *module_type_name(enum module_type type);

/**
 * Parse module type from string
 * 
 * Converts a string module type name to the enum value.
 * 
 * @param type_name String name of the type
 * @return Module type enum value, or MODULE_TYPE_UNKNOWN if not recognized
 */
enum module_type module_type_from_string(const char *type_name);

#ifdef __cplusplus
}
#endif
