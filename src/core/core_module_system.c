#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_module_system.h"
#include "core_logging.h"
#include "core_mymalloc.h"

/* Global module registry */
struct module_registry *global_module_registry = NULL;

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
    
    /* Initialize registry fields */
    global_module_registry->num_modules = 0;
    global_module_registry->num_active_types = 0;
    
    /* Initialize module slots */
    for (int i = 0; i < MAX_MODULES; i++) {
        global_module_registry->modules[i].module = NULL;
        global_module_registry->modules[i].module_data = NULL;
        global_module_registry->modules[i].initialized = false;
        global_module_registry->modules[i].active = false;
    }
    
    /* Initialize type mapping */
    for (int i = 0; i < MODULE_TYPE_MAX; i++) {
        global_module_registry->active_modules[i].type = MODULE_TYPE_UNKNOWN;
        global_module_registry->active_modules[i].module_index = -1;
    }
    
    LOG_INFO("Module system initialized");
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
    }
    
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
    if (global_module_registry == NULL) {
        /* Auto-initialize if not done already */
        int status = module_system_initialize();
        if (status != MODULE_STATUS_SUCCESS) {
            return status;
        }
    }
    
    /* Validate module */
    if (!module_validate(module)) {
        LOG_ERROR("Invalid module interface provided");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check for space in registry */
    if (global_module_registry->num_modules >= MAX_MODULES) {
        LOG_ERROR("Module registry is full (max %d modules)", MAX_MODULES);
        return MODULE_STATUS_ERROR;
    }
    
    /* Find an available slot */
    int module_id = global_module_registry->num_modules;
    
    /* Copy the module interface */
    global_module_registry->modules[module_id].module = module;
    global_module_registry->modules[module_id].module_data = NULL;
    global_module_registry->modules[module_id].initialized = false;
    global_module_registry->modules[module_id].active = false;
    
    /* Assign ID to module */
    module->module_id = module_id;
    
    /* Increment module count */
    global_module_registry->num_modules++;
    
    LOG_INFO("Registered module '%s' (type %d) with ID %d", 
             module->name, module->type, module_id);
    
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
    
    /* Find active module of the requested type */
    for (int i = 0; i < global_module_registry->num_active_types; i++) {
        if (global_module_registry->active_modules[i].type == type) {
            int module_index = global_module_registry->active_modules[i].module_index;
            
            /* Set output pointers */
            if (module != NULL) {
                *module = global_module_registry->modules[module_index].module;
            }
            
            if (module_data != NULL) {
                *module_data = global_module_registry->modules[module_index].module_data;
            }
            
            return MODULE_STATUS_SUCCESS;
        }
    }
    
    LOG_ERROR("No active module found for type %d", type);
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
    
    return true;
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
    
    module->last_error = error_code;
    
    if (error_message != NULL) {
        strncpy(module->error_message, error_message, MAX_ERROR_MESSAGE - 1);
        module->error_message[MAX_ERROR_MESSAGE - 1] = '\0';
    } else {
        module->error_message[0] = '\0';
    }
}
