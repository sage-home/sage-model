#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core_logging.h"
#include "core_mymalloc.h"
#include "core_module_parameter.h"
#include "core_module_callback.h"
#include "core_dynamic_library.h"
#include "core_module_system.h"
#include "core_module_error.h"
#include "core_galaxy_extensions.h" // For galaxy_property_t, galaxy_extension_register, etc.
#include "core_property_types.h"    // For enum galaxy_property_type
#include "core_property_descriptor.h"  // For GalaxyPropertyInfo struct definition

/* Global module registry */
struct module_registry *global_module_registry = NULL;



/**
 * Parse a semantic version string
 * 
 * Converts a version string (e.g., "1.2.3") to a module_version structure.
 * 
 * @param version_str Version string to parse
 * @param version Output version structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_parse_version(const char *version_str, struct module_version *version) {
    if (version_str == NULL || version == NULL) {
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Initialize to zeros in case of partial version */
    version->major = 0;
    version->minor = 0;
    version->patch = 0;
    
    /* Parse the version string */
    int matched = sscanf(version_str, "%d.%d.%d", 
                        &version->major, &version->minor, &version->patch);
    
    if (matched < 1) {
        LOG_ERROR("Invalid version format: %s (expects major.minor.patch)", version_str);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Compare two semantic versions
 * 
 * Compares two version structures according to semver rules.
 * 
 * @param v1 First version to compare
 * @param v2 Second version to compare
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int module_compare_versions(const struct module_version *v1, const struct module_version *v2) {
    if (v1 == NULL || v2 == NULL) {
        return 0;  /* Treat as equal if invalid arguments */
    }
    
    /* Compare major version */
    if (v1->major < v2->major) return -1;
    if (v1->major > v2->major) return 1;
    
    /* Major versions are equal, compare minor versions */
    if (v1->minor < v2->minor) return -1;
    if (v1->minor > v2->minor) return 1;
    
    /* Minor versions are equal, compare patch versions */
    if (v1->patch < v2->patch) return -1;
    if (v1->patch > v2->patch) return 1;
    
    /* All components are equal */
    return 0;
}

/**
 * Parse versions for a dependency
 * 
 * Parses the minimum and maximum version strings in a dependency and
 * stores the results in the structured version fields.
 * 
 * @param dep Dependency to parse versions for
 */
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
    bool exact_match) {
    
    /* Check for invalid parameters */
    if (version == NULL || min_version == NULL) {
        return false;
    }
    
    /* For exact match, compare only with min_version */
    if (exact_match) {
        return module_compare_versions(version, min_version) == 0;
    }
    
    /* Check minimum version requirement */
    if (module_compare_versions(version, min_version) < 0) {
        return false;  /* Version is less than minimum required */
    }
    
    /* Check maximum version constraint if provided */
    if (max_version != NULL && module_compare_versions(version, max_version) > 0) {
        return false;  /* Version is greater than maximum allowed */
    }
    
    return true;  /* All constraints satisfied */
}

/**
 * Get module type name
 * 
 * Returns a string representation of a module type.
 * 
 * @param type Module type
 * @return String name of the module type
 */
const char *module_type_name(enum module_type type) {
    switch (type) {
        case MODULE_TYPE_UNKNOWN:
            return "unknown";
        default:
            /* For physics module types, return a generic name */
            return "physics_module";
    }
}

/**
 * Parse module type from string
 * 
 * Converts a string module type name to the enum value.
 * 
 * @param type_name String name of the type
 * @return Module type enum value, or MODULE_TYPE_UNKNOWN if not recognized
 */
enum module_type module_type_from_string(const char *type_name) {
    if (type_name == NULL) {
        return MODULE_TYPE_UNKNOWN;
    }
    
    /* Convert type name to lowercase for case-insensitive comparison */
    char type_lower[MAX_MODULE_NAME];
    size_t i;
    size_t len = strlen(type_name);
    
    if (len >= MAX_MODULE_NAME) {
        len = MAX_MODULE_NAME - 1;
    }
    
    for (i = 0; i < len; i++) {
        type_lower[i] = tolower(type_name[i]);
    }
    type_lower[i] = '\0';
    
    /* In physics-free mode, only recognize 'unknown' */
    if (strcmp(type_lower, "unknown") == 0) {
        return MODULE_TYPE_UNKNOWN;
    }
    
    /* All other types are treated as physics module types */
    /* Physics modules register their own types dynamically */
    return MODULE_TYPE_UNKNOWN;
}

/**
 * Initialize the module system
 * 
 * Sets up the global module registry and prepares it for module registration.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_initialize(void) {
    /* Check if already initialized */
    if (global_module_registry != NULL) {
        LOG_WARNING("Module system already initialized");
        return MODULE_STATUS_ALREADY_INITIALIZED;
    }
    
    /* Allocate memory for the registry */
    global_module_registry = (struct module_registry *)mymalloc(sizeof(struct module_registry));
    if (global_module_registry == NULL) {
        LOG_ERROR("Failed to allocate memory for module registry");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize all registry fields to zero */
    memset(global_module_registry, 0, sizeof(struct module_registry));
    
    /* Initialize type mapping */
    for (int i = 0; i < MODULE_TYPE_MAX; i++) {
        global_module_registry->active_modules[i].type = MODULE_TYPE_UNKNOWN;
        global_module_registry->active_modules[i].module_index = -1;
    }
    
    /* Default configuration */
    
    /* Initialize the module callback system */
    int status = module_callback_system_initialize();
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize module callback system");
        myfree(global_module_registry);
        global_module_registry = NULL;
        return status;
    }
    
    LOG_INFO("Module system initialized");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Configure the module system
 * 
 * Sets up the module system with the given parameters.
 * 
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_configure(struct params *params) {
    (void)params; /* Parameter preserved for API compatibility */
    
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Module system configuration is now simplified since modules self-register */
    LOG_INFO("Module system configured (modules self-register via constructors)");
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Find a module by name
 * 
 * Searches the registry for a module with the given name.
 * 
 * @param name Name of the module to find
 * @return Module index if found, -1 if not found
 */
int module_find_by_name(const char *name) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return -1;
    }
    
    if (name == NULL) {
        LOG_ERROR("NULL module name");
        return -1;
    }
    
    /* Search for the module */
    for (int i = 0; i < global_module_registry->num_modules; i++) {
        if (global_module_registry->modules[i].module != NULL) {
            if (strcmp(global_module_registry->modules[i].module->name, name) == 0) {
                return i;
            }
        }
    }
    
    return -1;  /* Not found */
}



/* Parameter system integration functions */

/**
 * Get a module's parameter registry
 * 
 * Retrieves the parameter registry for a specific module.
 * 
 * @param module_id ID of the module
 * @param registry Output pointer to store the registry
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_parameter_registry(int module_id, module_parameter_registry_t **registry) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    if (registry == NULL) {
        LOG_ERROR("NULL registry pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_MODULE_NOT_FOUND;
    }
    
    /* Check if module exists */
    if (global_module_registry->modules[module_id].module == NULL) {
        LOG_ERROR("Module %d not found", module_id);
        return MODULE_STATUS_MODULE_NOT_FOUND;
    }
    
    /* Check if parameter registry exists */
    if (global_module_registry->modules[module_id].parameter_registry == NULL) {
        /* Initialize parameter registry */
        global_module_registry->modules[module_id].parameter_registry = 
            (module_parameter_registry_t *)mymalloc(sizeof(module_parameter_registry_t));
        
        if (global_module_registry->modules[module_id].parameter_registry == NULL) {
            LOG_ERROR("Failed to allocate memory for parameter registry");
            return MODULE_STATUS_OUT_OF_MEMORY;
        }
        
        int result = module_parameter_registry_init(global_module_registry->modules[module_id].parameter_registry);
        if (result != MODULE_PARAM_SUCCESS) {
            LOG_ERROR("Failed to initialize parameter registry for module %d", module_id);
            myfree(global_module_registry->modules[module_id].parameter_registry);
            global_module_registry->modules[module_id].parameter_registry = NULL;
            return MODULE_STATUS_ERROR;
        }
        
        static int param_registry_init_count = 0;
        param_registry_init_count++;
        if (param_registry_init_count <= 5) {
            if (param_registry_init_count == 5) {
                LOG_DEBUG("Parameter registry initialized for module %d (init #%d - further messages suppressed)", module_id, param_registry_init_count);
            } else {
                LOG_DEBUG("Parameter registry initialized for module %d (init #%d)", module_id, param_registry_init_count);
            }
        }
    }
    
    *registry = global_module_registry->modules[module_id].parameter_registry;
    return MODULE_STATUS_SUCCESS;
}

/**
 * Register a parameter with a module
 * 
 * Adds a parameter to a module's parameter registry.
 * 
 * @param module_id ID of the module
 * @param param Parameter to register
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_register_parameter_with_module(int module_id, const module_parameter_t *param) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    /* Create a copy of the parameter with the module ID set */
    module_parameter_t param_copy = *param;
    param_copy.module_id = module_id;
    
    int result = module_register_parameter(registry, &param_copy);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to register parameter for module %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get a parameter from a module
 * 
 * Retrieves a parameter from a module's parameter registry.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param param Output parameter to store the result
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_parameter_from_module(int module_id, const char *name, module_parameter_t *param) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_get_parameter(registry, name, module_id, param);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to get parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get an integer parameter from a module
 * 
 * Retrieves an integer parameter value from a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Output pointer to store the value
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_int_parameter(int module_id, const char *name, int *value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_get_parameter_int(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to get int parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get a float parameter from a module
 * 
 * Retrieves a float parameter value from a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Output pointer to store the value
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_float_parameter(int module_id, const char *name, float *value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_get_parameter_float(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to get float parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get a double parameter from a module
 * 
 * Retrieves a double parameter value from a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Output pointer to store the value
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_double_parameter(int module_id, const char *name, double *value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_get_parameter_double(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to get double parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get a boolean parameter from a module
 * 
 * Retrieves a boolean parameter value from a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Output pointer to store the value
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_bool_parameter(int module_id, const char *name, bool *value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_get_parameter_bool(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to get bool parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get a string parameter from a module
 * 
 * Retrieves a string parameter value from a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Output buffer to store the value
 * @param max_len Maximum length of the output buffer
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_string_parameter(int module_id, const char *name, char *value, size_t max_len) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_get_parameter_string(registry, name, module_id, value, max_len);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to get string parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set an integer parameter in a module
 * 
 * Sets an integer parameter value in a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Value to set
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_int_parameter(int module_id, const char *name, int value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_set_parameter_int(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to set int parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set a float parameter in a module
 * 
 * Sets a float parameter value in a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Value to set
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_float_parameter(int module_id, const char *name, float value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_set_parameter_float(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to set float parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set a double parameter in a module
 * 
 * Sets a double parameter value in a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Value to set
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_double_parameter(int module_id, const char *name, double value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_set_parameter_double(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to set double parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set a boolean parameter in a module
 * 
 * Sets a boolean parameter value in a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Value to set
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_bool_parameter(int module_id, const char *name, bool value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_set_parameter_bool(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to set bool parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set a string parameter in a module
 * 
 * Sets a string parameter value in a module.
 * 
 * @param module_id ID of the module
 * @param name Name of the parameter
 * @param value Value to set
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_string_parameter(int module_id, const char *name, const char *value) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_set_parameter_string(registry, name, module_id, value);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to set string parameter %s for module %d", name, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Load parameters from a file for a module
 * 
 * Reads parameters from a file into a module's registry.
 * 
 * @param module_id ID of the module
 * @param filename Path to the parameter file
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_load_parameters(int module_id, const char *filename) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_load_parameters_from_file(registry, filename);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to load parameters from file %s for module %d", filename, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Save parameters to a file for a module
 * 
 * Writes parameters from a module's registry to a file.
 * 
 * @param module_id ID of the module
 * @param filename Path to the output file
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_save_parameters(int module_id, const char *filename) {
    module_parameter_registry_t *registry = NULL;
    int status = module_get_parameter_registry(module_id, &registry);
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    int result = module_save_parameters_to_file(registry, filename);
    if (result != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to save parameters to file %s for module %d", filename, module_id);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up the module system
 * 
 * Releases resources used by the module system and unregisters all modules.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_cleanup(void) {
    if (global_module_registry == NULL) {
        LOG_WARNING("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Cleanup all initialized modules */
    for (int i = 0; i < global_module_registry->num_modules; i++) {
        if (global_module_registry->modules[i].initialized) {
            module_cleanup(i);
        }
        
        /* Free function registry if allocated */
        struct base_module *module = global_module_registry->modules[i].module;
        if (module != NULL) {
            if (module->function_registry != NULL) {
                myfree(module->function_registry);
                module->function_registry = NULL;
            }
            
            if (module->dependencies != NULL) {
                myfree(module->dependencies);
                module->dependencies = NULL;
                module->num_dependencies = 0;
            }
            
            /* Reset module ID to indicate it's no longer registered */
            module->module_id = -1;
        }
        
        /* If this is a dynamically loaded module, unload it */
        if (global_module_registry->modules[i].dynamic && 
            global_module_registry->modules[i].handle != NULL) {
            dlclose(global_module_registry->modules[i].handle);
            global_module_registry->modules[i].handle = NULL;
        }
    }
    
    /* Clean up the module callback system */
    module_callback_system_cleanup();
    
    /* Free the registry */
    myfree(global_module_registry);
    global_module_registry = NULL;
    
    LOG_INFO("Module system cleaned up");
    return MODULE_STATUS_SUCCESS;
}



/**
 * Register a module with the system
 * 
 * Adds a module to the global registry and assigns it a unique ID.
 * 
 * @param module Pointer to the module interface to register
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_register(struct base_module *module) {
    LOG_DEBUG("Attempting to register module: %s", 
              module ? module->name : "NULL module");
           
    if (global_module_registry == NULL) {
        LOG_DEBUG("Global module registry is NULL, initializing...");
        /* Auto-initialize if not done already */
        int status = module_system_initialize();
        if (status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Module system initialization failed with status %d", status);
            return status;
        }
    }

    /* Validate module */
    if (!module_validate(module)) {
        LOG_ERROR("Module validation failed for %s", 
                  module ? module->name : "NULL module");
        return MODULE_STATUS_INVALID_ARGS;
    }

    /* Check for space in registry */
    if (global_module_registry->num_modules >= MAX_MODULES) {
        LOG_ERROR("Module registry is full (max %d modules)", MAX_MODULES);
        return MODULE_STATUS_ERROR;
    }

    /* Check if module with the same name already exists */
    for (int i = 0; i < global_module_registry->num_modules; i++) {
        if (global_module_registry->modules[i].module != NULL &&
            strcmp(global_module_registry->modules[i].module->name, module->name) == 0) {
            LOG_ERROR("Module with name '%s' already registered", module->name);
            return MODULE_STATUS_ERROR;
        }
    }

    /* Find an available slot */
    int module_id = global_module_registry->num_modules;

    /* Initialize module callback fields */
    module->function_registry = NULL;
    module->dependencies = NULL;
    module->num_dependencies = 0;

    /* Copy the module interface */
    global_module_registry->modules[module_id].module = module;
    global_module_registry->modules[module_id].module_data = NULL;
    global_module_registry->modules[module_id].initialized = false;
    global_module_registry->modules[module_id].active = false;
    global_module_registry->modules[module_id].dynamic = false;
    global_module_registry->modules[module_id].handle = NULL;
    global_module_registry->modules[module_id].path[0] = '\0';

    /* Assign ID to module */
    module->module_id = module_id;
    
    /* Increment module count */
    global_module_registry->num_modules++;
    
    static int module_register_count = 0;
    module_register_count++;
    if (module_register_count <= 5) {
        if (module_register_count == 5) {
            LOG_DEBUG("Registered module '%s' (type %d) with ID %d (registration #%d - further messages suppressed)", 
                      module->name, module->type, module_id, module_register_count);
        } else {
            LOG_DEBUG("Registered module '%s' (type %d) with ID %d (registration #%d)", 
                      module->name, module->type, module_id, module_register_count);
        }
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Unregister a module from the system
 * 
 * Removes a module from the global registry.
 * 
 * @param module_id ID of the module to unregister
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_unregister(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Clean up module if initialized */
    if (global_module_registry->modules[module_id].initialized) {
        module_cleanup(module_id);
    }
    
    /* If the module is active, deactivate it */
    if (global_module_registry->modules[module_id].active) {
        enum module_type type = global_module_registry->modules[module_id].module->type;
        
        /* Find and remove the active module entry */
        for (int i = 0; i < global_module_registry->num_active_types; i++) {
            if (global_module_registry->active_modules[i].type == type) {
                /* Shift remaining entries down */
                for (int j = i; j < global_module_registry->num_active_types - 1; j++) {
                    global_module_registry->active_modules[j] = global_module_registry->active_modules[j + 1];
                }
                global_module_registry->num_active_types--;
                break;
            }
        }
    }
    
    /* Remove the module entry */
    struct base_module *module = global_module_registry->modules[module_id].module;
    const char *module_name = module->name;
    enum module_type module_type = module->type;
    
    /* Free the function registry if it exists */
    if (module->function_registry != NULL) {
        free(module->function_registry);
        module->function_registry = NULL;
    }
    
    /* Clear the module entry */
    global_module_registry->modules[module_id].module = NULL;
    global_module_registry->modules[module_id].module_data = NULL;
    global_module_registry->modules[module_id].initialized = false;
    global_module_registry->modules[module_id].active = false;
    
    /* We don't reduce num_modules or reorganize the array to keep IDs stable */
    
    LOG_INFO("Unregistered module '%s' (type %d) with ID %d", 
             module_name, module_type, module_id);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Initialize a module
 * 
 * Calls the initialize function of a registered module.
 * 
 * @param module_id ID of the module to initialize
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_initialize(int module_id, struct params *params) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if already initialized */
    if (global_module_registry->modules[module_id].initialized) {
        LOG_WARNING("Module ID %d already initialized", module_id);
        return MODULE_STATUS_ALREADY_INITIALIZED;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("Module ID %d has NULL interface", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if initialization function exists */
    if (module->initialize == NULL) {
        LOG_ERROR("Module '%s' (ID %d) has no initialize function", 
                 module->name, module_id);
        return MODULE_STATUS_NOT_IMPLEMENTED;
    }
    
    /* Call module initialize function */
    void *module_data = NULL;
    int status = module->initialize(params, &module_data);
    
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize module '%s' (ID %d): %d", 
                 module->name, module_id, status);
        return status;
    }
    
    /* Store module data */
    global_module_registry->modules[module_id].module_data = module_data;
    
    /* Initialize error context if not already done */
    if (module->error_context == NULL) {
        int ec_status = module_error_context_init(&module->error_context);
        if (ec_status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Failed to initialize error context for module '%s' (ID %d)",
                    module->name, module_id);
            /* Non-fatal, continue */
        }
    }

    /* Initialize debug context if not already done */
    
    global_module_registry->modules[module_id].initialized = true;

    LOG_INFO("Initialized module '%s' (ID %d)", module->name, module_id);

    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up a module
 * 
 * Calls the cleanup function of a registered module.
 * 
 * @param module_id ID of the module to clean up
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_cleanup(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if initialized */
    if (!global_module_registry->modules[module_id].initialized) {
        LOG_WARNING("Module ID %d not initialized", module_id);
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("Module ID %d has NULL interface", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if cleanup function exists */
    if (module->cleanup == NULL) {
        LOG_WARNING("Module '%s' (ID %d) has no cleanup function", 
                   module->name, module_id);
        /* Not having a cleanup function is acceptable */
        global_module_registry->modules[module_id].initialized = false;
        global_module_registry->modules[module_id].module_data = NULL;
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Call module cleanup function */
    void *module_data = global_module_registry->modules[module_id].module_data;
    int status = module->cleanup(module_data);
    
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to clean up module '%s' (ID %d): %d", 
                 module->name, module_id, status);
        return status;
    }
    
    /* Clear module data */
    global_module_registry->modules[module_id].module_data = NULL;
    global_module_registry->modules[module_id].initialized = false;
    
    /* Clean up error context if it exists */
    if (module->error_context != NULL) {
        module_error_context_cleanup(module->error_context);
        module->error_context = NULL;
    }
    
    /* Clean up debug context if it exists */

    /* Clean up parameter registry if it exists */
    if (global_module_registry->modules[module_id].parameter_registry != NULL) {
        module_parameter_registry_free(global_module_registry->modules[module_id].parameter_registry);
        myfree(global_module_registry->modules[module_id].parameter_registry);
        global_module_registry->modules[module_id].parameter_registry = NULL;
        static int param_registry_free_count = 0;
        param_registry_free_count++;
        if (param_registry_free_count <= 5) {
            if (param_registry_free_count == 5) {
                LOG_DEBUG("Freed parameter registry for module '%s' (ID %d) (free #%d - further messages suppressed)", module->name, module_id, param_registry_free_count);
            } else {
                LOG_DEBUG("Freed parameter registry for module '%s' (ID %d) (free #%d)", module->name, module_id, param_registry_free_count);
            }
        }
    }
    
    LOG_INFO("Cleaned up module '%s' (ID %d)", module->name, module_id);
    
    return MODULE_STATUS_SUCCESS;
}

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
int module_get(int module_id, struct base_module **module, void **module_data) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if module exists */
    if (global_module_registry->modules[module_id].module == NULL) {
        LOG_ERROR("No module registered at ID %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Set output pointers */
    if (module != NULL) {
        *module = global_module_registry->modules[module_id].module;
    }
    
    if (module_data != NULL) {
        *module_data = global_module_registry->modules[module_id].module_data;
    }
    
    return MODULE_STATUS_SUCCESS;
}

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
int module_get_active_by_type(enum module_type type, struct base_module **module, void **module_data) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Find active module of the requested type, preferring modules with functions */
    struct base_module *best_module = NULL;
    void *best_module_data = NULL;
    struct base_module *fallback_module = NULL;
    void *fallback_module_data = NULL;
    
    for (int i = 0; i < global_module_registry->num_active_types; i++) {
        if (global_module_registry->active_modules[i].type == type) {
            int module_index = global_module_registry->active_modules[i].module_index;
            struct base_module *candidate = global_module_registry->modules[module_index].module;
            void *candidate_data = global_module_registry->modules[module_index].module_data;
            
            /* Prefer modules that have function registries with functions */
            if (candidate->function_registry != NULL && candidate->function_registry->num_functions > 0) {
                best_module = candidate;
                best_module_data = candidate_data;
                break; /* Found a module with functions, use it */
            } else if (fallback_module == NULL) {
                /* Keep the first module as fallback if no modules have functions */
                fallback_module = candidate;
                fallback_module_data = candidate_data;
            }
        }
    }
    
    /* Use the best module (with functions) or fallback to first available */
    struct base_module *selected_module = (best_module != NULL) ? best_module : fallback_module;
    void *selected_data = (best_module != NULL) ? best_module_data : fallback_module_data;
    
    if (selected_module == NULL) {
        return MODULE_STATUS_MODULE_NOT_FOUND; /* No modules of requested type found */
    }
    
    /* Set output pointers */
    if (module != NULL) {
        *module = selected_module;
    }
    
    if (module_data != NULL) {
        *module_data = selected_data;
    }
    
    return MODULE_STATUS_SUCCESS;
    
    /* During Phase 2.5-2.6 development, we're less verbose */
    static bool already_logged_type_errors[MODULE_TYPE_MAX] = {false};
    
    if (!already_logged_type_errors[type]) {
        LOG_DEBUG("No active module found for type %d (%s)", type, module_type_name(type));
        already_logged_type_errors[type] = true;
    }
    
    return MODULE_STATUS_ERROR;
}

/**
 * Validate a base module
 * 
 * Checks that a module interface is valid and properly formed.
 * 
 * @param module Pointer to the module interface to validate
 * @return true if the module is valid, false otherwise
 */
bool module_validate(const struct base_module *module) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer");
        return false;
    }

    /* Check required fields */
    if (module->name[0] == '\0') {
        LOG_ERROR("Module name cannot be empty");
        return false;
    }

    if (module->version[0] == '\0') {
        LOG_ERROR("Module version cannot be empty");
        return false;
    }

    if (module->type <= MODULE_TYPE_UNKNOWN || module->type >= MODULE_TYPE_MAX) {
        LOG_ERROR("Invalid module type: %d", module->type);
        return false;
    }

    /* Check required functions */
    if (module->initialize == NULL) {
        LOG_ERROR("Module '%s' missing initialize function", module->name);
        return false;
    }

    /* cleanup can be NULL if the module doesn't need cleanup */

    /* Validate dependency array if present */
    if (module->dependencies != NULL && module->num_dependencies <= 0) {
        LOG_WARNING("Module '%s' has dependencies array but num_dependencies is %d",
                  module->name, module->num_dependencies);
    }

    /* Validate function registry if present */
    if (module->function_registry != NULL && 
        (module->function_registry->num_functions < 0 || 
         module->function_registry->num_functions > MAX_MODULE_FUNCTIONS)) {
        LOG_ERROR("Module '%s' has invalid function registry (num_functions: %d)",
                module->name, module->function_registry->num_functions);
        return false;
    }

    return true;
}

/**
 * Validate module runtime dependencies
 * 
 * Checks that all dependencies of a module are satisfied at runtime.
 * 
 * @param module_id ID of the module to validate
 * @return MODULE_STATUS_SUCCESS if all dependencies are met, error code otherwise
 */
int module_validate_runtime_dependencies(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("NULL module pointer for ID %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Skip if no dependencies */
    if (module->dependencies == NULL || module->num_dependencies == 0) {
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Check each dependency */
    for (int i = 0; i < module->num_dependencies; i++) {
        const module_dependency_t *dep = &module->dependencies[i];
        
        /* If this is a type-based dependency, check for active module of this type */
        if (dep->type != MODULE_TYPE_UNKNOWN && dep->name[0] == '\0') {
            /* Find active module of the specified type */
            int active_index = -1;
            for (int j = 0; j < global_module_registry->num_active_types; j++) {
                if ((int)global_module_registry->active_modules[j].type == (int)dep->type) {
                    active_index = global_module_registry->active_modules[j].module_index;
                    break;
                }
            }
            
            if (active_index < 0) {
                if (dep->optional) {
                    LOG_INFO("Optional dependency on type %s not satisfied - no active module",
                             module_type_name(dep->type));
                    continue;
                } else {
                    LOG_ERROR("Required dependency on type %s not satisfied - no active module",
                             module_type_name(dep->type));
                    return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
                }
            }
            
            /* Get the active module */
            struct base_module *active_module = global_module_registry->modules[active_index].module;
            if (active_module == NULL) {
                LOG_ERROR("NULL module pointer for active module of type %s",
                         module_type_name(dep->type));
                return MODULE_STATUS_ERROR;
            }
            
            /* Check version compatibility */
            if (dep->min_version_str[0] != '\0') {
                struct module_version active_version;
                
                if (module_parse_version(active_module->version, &active_version) == MODULE_STATUS_SUCCESS) {
                    
                    /* Use parsed versions if available */
                    if (dep->has_parsed_versions) {
                        const struct module_version *max_version = 
                            dep->max_version_str[0] != '\0' ? &dep->max_version : NULL;
                        
                        if (!module_check_version_compatibility(
                                &active_version, 
                                &dep->min_version, 
                                max_version, 
                                dep->exact_match)) {
                            
                            /* Log appropriate error message based on the constraint that failed */
                            if (dep->exact_match) {
                                LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but exact version %s is required",
                                        module_type_name(dep->type), active_module->name, 
                                        active_module->version, dep->min_version_str);
                            } else if (max_version != NULL && module_compare_versions(&active_version, max_version) > 0) {
                                LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but maximum %s is allowed",
                                        module_type_name(dep->type), active_module->name, 
                                        active_module->version, dep->max_version_str);
                            } else {
                                LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but minimum %s is required",
                                        module_type_name(dep->type), active_module->name, 
                                        active_module->version, dep->min_version_str);
                            }
                            
                            return MODULE_STATUS_DEPENDENCY_CONFLICT;
                        }
                    }
                    /* Fall back to string-based approach if parsing failed */
                    else {
                        struct module_version dep_min_version;
                        
                        /* Parse minimum version */
                        if (module_parse_version(dep->min_version_str, &dep_min_version) == MODULE_STATUS_SUCCESS) {
                            /* Check minimum version requirement */
                            if (module_compare_versions(&active_version, &dep_min_version) < 0) {
                                LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but minimum %s is required",
                                        module_type_name(dep->type), active_module->name, 
                                        active_module->version, dep->min_version_str);
                                return MODULE_STATUS_DEPENDENCY_CONFLICT;
                            }
                            
                            /* Check maximum version constraint if specified */
                            if (dep->max_version_str[0] != '\0') {
                                struct module_version dep_max_version;
                                if (module_parse_version(dep->max_version_str, &dep_max_version) == MODULE_STATUS_SUCCESS) {
                                    if (module_compare_versions(&active_version, &dep_max_version) > 0) {
                                        LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but maximum %s is allowed",
                                                module_type_name(dep->type), active_module->name, 
                                                active_module->version, dep->max_version_str);
                                        return MODULE_STATUS_DEPENDENCY_CONFLICT;
                                    }
                                }
                            }
                            
                            /* Check for exact match if required */
                            if (dep->exact_match && strcmp(active_module->version, dep->min_version_str) != 0) {
                                LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but exact version %s is required",
                                        module_type_name(dep->type), active_module->name, 
                                        active_module->version, dep->min_version_str);
                                return MODULE_STATUS_DEPENDENCY_CONFLICT;
                            }
                        }
                    }
                }
            }
            
            static int dependency_satisfied_count = 0;
            dependency_satisfied_count++;
            if (dependency_satisfied_count <= 5) {
                if (dependency_satisfied_count == 5) {
                    LOG_DEBUG("Dependency on type %s satisfied by active module %s version %s (check #%d - further messages suppressed)",
                             module_type_name(dep->type), active_module->name, active_module->version, dependency_satisfied_count);
                } else {
                    LOG_DEBUG("Dependency on type %s satisfied by active module %s version %s (check #%d)",
                             module_type_name(dep->type), active_module->name, active_module->version, dependency_satisfied_count);
                }
            }
        }
        /* If this is a name-based dependency */
        else if (dep->name[0] != '\0') {
            /* Find module by name */
            int module_idx = module_find_by_name(dep->name);
            if (module_idx < 0) {
                if (dep->optional) {
                    LOG_INFO("Optional dependency on named module %s not satisfied - module not found",
                             dep->name);
                    continue;
                } else {
                    LOG_ERROR("Required dependency on named module %s not satisfied - module not found",
                             dep->name);
                    return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
                }
            }
            
            /* Check if dependency is active */
            if (!global_module_registry->modules[module_idx].active) {
                if (dep->optional) {
                    LOG_INFO("Optional dependency on named module %s not satisfied - module not active",
                             dep->name);
                    continue;
                } else {
                    LOG_ERROR("Required dependency on named module %s not satisfied - module not active",
                             dep->name);
                    return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
                }
            }
            
            /* Check type if specified */
            if (dep->type != MODULE_TYPE_UNKNOWN) {
                struct base_module *dep_module = global_module_registry->modules[module_idx].module;
                if ((int)dep_module->type != (int)dep->type) {
                    LOG_ERROR("Dependency on named module %s has wrong type: expected %s, got %s",
                             dep->name, module_type_name(dep->type), module_type_name(dep_module->type));
                    return MODULE_STATUS_DEPENDENCY_CONFLICT;
                }
            }
            
            static int named_dependency_count = 0;
            named_dependency_count++;
            if (named_dependency_count <= 5) {
                if (named_dependency_count == 5) {
                    LOG_DEBUG("Dependency on named module %s satisfied (check #%d - further messages suppressed)", dep->name, named_dependency_count);
                } else {
                    LOG_DEBUG("Dependency on named module %s satisfied (check #%d)", dep->name, named_dependency_count);
                }
            }
        }
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set a module as active
 * 
 * Marks a module as the active implementation for its type.
 * 
 * @param module_id ID of the module to activate
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_active(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if module exists */
    if (global_module_registry->modules[module_id].module == NULL) {
        LOG_ERROR("No module registered at ID %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if the module is initialized */
    if (!global_module_registry->modules[module_id].initialized) {
        LOG_ERROR("Module ID %d not initialized", module_id);
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate runtime dependencies */
    int status = module_validate_runtime_dependencies(module_id);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to validate runtime dependencies for module ID %d: %d", 
                 module_id, status);
        return status;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    enum module_type type = module->type;
    
    /* Check if there's already an active module of this type */
    int existing_index = -1;
    for (int i = 0; i < global_module_registry->num_active_types; i++) {
        if (global_module_registry->active_modules[i].type == type) {
            existing_index = i;
            break;
        }
    }
    
    if (existing_index >= 0) {
        /* Deactivate the existing module */
        int old_module_index = global_module_registry->active_modules[existing_index].module_index;
        global_module_registry->modules[old_module_index].active = false;
        
        /* Replace with the new module */
        global_module_registry->active_modules[existing_index].module_index = module_id;
        
        LOG_INFO("Replaced active module for type %d with '%s' (ID %d)",
                type, module->name, module_id);
    } else {
        /* Add as a new active module */
        if (global_module_registry->num_active_types >= MODULE_TYPE_MAX) {
            LOG_ERROR("Too many active module types");
            return MODULE_STATUS_ERROR;
        }
        
        int index = global_module_registry->num_active_types;
        global_module_registry->active_modules[index].type = type;
        global_module_registry->active_modules[index].module_index = module_id;
        global_module_registry->num_active_types++;
        
        LOG_INFO("Set module '%s' (ID %d) as active for type %d",
                module->name, module_id, type);
    }
    
    /* Mark the module as active */
    global_module_registry->modules[module_id].active = true;
    
    return MODULE_STATUS_SUCCESS;
}

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
int module_get_last_error(int module_id, char *error_message, size_t max_length) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        if (error_message != NULL && max_length > 0) {
            strncpy(error_message, "Module system not initialized", max_length - 1);
            error_message[max_length - 1] = '\0';
        }
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        if (error_message != NULL && max_length > 0) {
            snprintf(error_message, max_length, "Invalid module ID: %d", module_id);
        }
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if module exists */
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("No module registered at ID %d", module_id);
        if (error_message != NULL && max_length > 0) {
            snprintf(error_message, max_length, "No module registered at ID %d", module_id);
        }
        return MODULE_STATUS_ERROR;
    }
    
    /* Copy error message */
    if (error_message != NULL && max_length > 0) {
        strncpy(error_message, module->error_message, max_length - 1);
        error_message[max_length - 1] = '\0';
    }
    
    return module->last_error;
}

/**
 * Set error information in a module
 * 
 * Updates the error state of a module.
 * 
 * @param module Pointer to the module interface
 * @param error_code Error code to set
 * @param error_message Error message to set
 */
void module_set_error(struct base_module *module, int error_code, const char *error_message) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer");
        return;
    }
    
    /* Update the basic error fields for backward compatibility */
    module->last_error = error_code;
    
    if (error_message != NULL) {
        strncpy(module->error_message, error_message, MAX_ERROR_MESSAGE - 1);
        module->error_message[MAX_ERROR_MESSAGE - 1] = '\0';
    } else {
        module->error_message[0] = '\0';
    }
    
    /* Use the enhanced error system if available */
    if (error_message != NULL) {
        /* Record the error with enhanced context */
        module_record_error(module, error_code, LOG_LEVEL_ERROR, 
                          "<unknown>", 0, "<unknown>", "%s", error_message);
    }
}

/**
 * @brief Register properties defined by a module with the galaxy extension system.
 *
 * This function takes an array of GalaxyPropertyInfo structures, typically generated
 * from a module's property definition file (e.g., a YAML file), and registers
 * them with the core SAGE galaxy extension system. This allows module-specific
 * data to be attached to galaxy structures.
 *
 * @param module_id The ID of the module registering these properties. This ID
 *                  is used to associate the properties with the correct module.
 * @param properties_info_array A pointer to an array of GalaxyPropertyInfo
 *                              structures that describe the properties to be
 *                              registered.
 * @param num_properties_to_register The number of properties in the
 *                                   properties_info_array.
 *
 * @return MODULE_STATUS_SUCCESS if all properties were registered successfully.
 *         MODULE_STATUS_ERROR if one or more properties failed to register,
 *                             or if there were critical errors during the process.
 *         MODULE_STATUS_INVALID_ARGS if input parameters are invalid.
 *         MODULE_STATUS_NOT_INITIALIZED if the module or extension system is not ready.
 */
int module_register_properties(int module_id, const GalaxyPropertyInfo *properties_info_array, int num_properties_to_register) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized. Cannot register properties for module_id %d.", module_id);
        return MODULE_STATUS_NOT_INITIALIZED;
    }

    // It might be good to validate module_id against registered modules here if feasible.
    // For now, assume module_id is valid as it's typically called by the module itself during its init.

    if (properties_info_array == NULL) {
        LOG_ERROR("properties_info_array is NULL for module_id %d.", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }

    if (num_properties_to_register <= 0) {
        LOG_INFO("No properties to register for module_id %d (count = %d).", module_id, num_properties_to_register);
        return MODULE_STATUS_SUCCESS; // Not an error, just nothing to do.
    }

    // Ensure galaxy extension system is up. galaxy_extension_register will auto-initialize if needed.
    if (global_extension_registry == NULL) {
        LOG_INFO("Galaxy extension system not yet initialized. galaxy_extension_register will attempt to initialize it for module %d.", module_id);
    }

    int successful_registrations = 0;
    for (int i = 0; i < num_properties_to_register; ++i) {
        const GalaxyPropertyInfo *info = &properties_info_array[i];
        galaxy_property_t prop_to_register;
        memset(&prop_to_register, 0, sizeof(galaxy_property_t));

        // Name
        if (info->name[0] == '\0') {
            LOG_ERROR("Module %d property %d: Name is NULL or empty. Skipping.", module_id, i);
            continue;
        }
        strncpy(prop_to_register.name, info->name, MAX_PROPERTY_NAME - 1);
        prop_to_register.name[MAX_PROPERTY_NAME - 1] = '\0';

        // Module ID
        prop_to_register.module_id = module_id;

        // Description and Units
        if (info->description[0] != '\0') {
            strncpy(prop_to_register.description, info->description, MAX_PROPERTY_DESCRIPTION - 1);
            prop_to_register.description[MAX_PROPERTY_DESCRIPTION - 1] = '\0';
        }
        if (info->units[0] != '\0') {
            strncpy(prop_to_register.units, info->units, MAX_PROPERTY_UNITS - 1);
            prop_to_register.units[MAX_PROPERTY_UNITS - 1] = '\0';
        }

        // Flags
        prop_to_register.flags = PROPERTY_FLAG_INITIALIZE; // Default: initialize to zero/NULL
        if (info->output_field) {
            prop_to_register.flags |= PROPERTY_FLAG_SERIALIZE;
        }
        if (info->read_only_level > 0) { // Assuming 1 and 2 mean read-only
            prop_to_register.flags |= PROPERTY_FLAG_READONLY;
        }


        // Type and Size from GalaxyPropertyInfo.type_str
        if (info->type_str[0] == '\0') {
            LOG_ERROR("Module %d property '%s': type_str is NULL. Skipping.", module_id, info->name);
            continue;
        }

        if (info->is_array) {
            prop_to_register.type = PROPERTY_TYPE_ARRAY;
            // For arrays, 'size' in galaxy_property_t refers to the size of the data block if fixed,
            // or sizeof(void*) if it's a pointer to dynamically allocated data.
            // The GalaxyPropertyInfo's is_dynamic_array flag is key here.
            if (info->is_dynamic_array) {
                prop_to_register.size = sizeof(void*); // Dynamic arrays are stored as pointers in the extension data.
                                                       // Actual allocation handled by property system using size_param_name.
                // Note: Currently no PROPERTY_FLAG_DYNAMIC_ARRAY is defined in galaxy_property_flags
                // Will need to be handled specially in the galaxy extension system
            } else { // Fixed-size array
                size_t element_size = 0;
                if (strcmp(info->type_str, "float") == 0) element_size = sizeof(float);
                else if (strcmp(info->type_str, "double") == 0) element_size = sizeof(double);
                else if (strcmp(info->type_str, "int") == 0 || strcmp(info->type_str, "int32_t") == 0) element_size = sizeof(int32_t);
                else if (strcmp(info->type_str, "long") == 0 || strcmp(info->type_str, "int64_t") == 0) element_size = sizeof(int64_t);
                else if (strcmp(info->type_str, "bool") == 0) element_size = sizeof(bool);
                // Add other supported element types for fixed arrays
                else {
                    LOG_ERROR("Module %d property '%s': Unsupported element type_str '%s' for fixed array. Skipping.", module_id, info->name, info->type_str);
                    continue;
                }

                if (info->array_len <= 0) {
                     LOG_ERROR("Module %d property '%s': Fixed array has invalid length %d. Skipping.", module_id, info->name, info->array_len);
                     continue;
                }
                prop_to_register.size = element_size * info->array_len;
            }
        } else { // Scalar type
            if (strcmp(info->type_str, "float") == 0) {
                prop_to_register.type = PROPERTY_TYPE_FLOAT;
                prop_to_register.size = sizeof(float);
            } else if (strcmp(info->type_str, "double") == 0) {
                prop_to_register.type = PROPERTY_TYPE_DOUBLE;
                prop_to_register.size = sizeof(double);
            } else if (strcmp(info->type_str, "int") == 0 || strcmp(info->type_str, "int32_t") == 0) {
                prop_to_register.type = PROPERTY_TYPE_INT32;
                prop_to_register.size = sizeof(int32_t);
            } else if (strcmp(info->type_str, "long") == 0 || strcmp(info->type_str, "int64_t") == 0) {
                prop_to_register.type = PROPERTY_TYPE_INT64;
                prop_to_register.size = sizeof(int64_t);
            } else if (strcmp(info->type_str, "bool") == 0) {
                prop_to_register.type = PROPERTY_TYPE_BOOL;
                prop_to_register.size = sizeof(bool); // Or char, ensure consistency
            } else if (strcmp(info->type_str, "string") == 0) { // Assuming dynamic string (char*)
                prop_to_register.type = PROPERTY_TYPE_STRUCT; // Using PROPERTY_TYPE_STRUCT for strings
                prop_to_register.size = sizeof(char*); // Stored as a pointer
            } else if (strcmp(info->type_str, "struct") == 0 || strcmp(info->type_str, "object") == 0) {
                LOG_ERROR("Module %d property '%s': type_str '%s' suggests a custom struct. "
                          "Size cannot be determined from GalaxyPropertyInfo alone. "
                          "Module should register complex structs directly using galaxy_extension_register with explicit size. Skipping.",
                          module_id, info->name, info->type_str);
                continue;
            }
            // Add other scalar types like uint32, uint64, char if needed
            else {
                LOG_ERROR("Module %d property '%s': Unknown or unsupported scalar type_str '%s'. Skipping.", module_id, info->name, info->type_str);
                continue;
            }
        }

        // Serialization functions:
        // For properties registered via this generic mechanism, we assume they are simple types
        // that can use default serialization or don't need custom functions.
        // If a module has complex structs, it should register them directly with galaxy_extension_register
        // and provide its own serialize/deserialize functions.
        prop_to_register.serialize = NULL;
        prop_to_register.deserialize = NULL;

        // Register the property
        int extension_id = galaxy_extension_register(&prop_to_register);
        if (extension_id < 0) {
            LOG_ERROR("Module %d: Failed to register property '%s'. Error code from galaxy_extension_register: %d", module_id, info->name, extension_id);
        } else {
            LOG_INFO("Module %d: Successfully registered property '%s' with extension ID %d.", module_id, info->name, extension_id);
            successful_registrations++;
        }
    } // End loop over properties

    if (successful_registrations == num_properties_to_register) {
        return MODULE_STATUS_SUCCESS;
    } else if (successful_registrations > 0) {
        LOG_WARNING("Module %d: Partially registered properties (%d out of %d).", module_id, successful_registrations, num_properties_to_register);
        return MODULE_STATUS_ERROR; // Partial success is treated as an error for atomicity.
    } else {
        if (num_properties_to_register > 0) { // Only log error if there was something to register
            LOG_ERROR("Module %d: Failed to register any properties (attempted %d).", module_id, num_properties_to_register);
        }
        return MODULE_STATUS_ERROR;
    }
}

/* Parameter system integration functions */
