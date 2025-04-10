#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* 
 * Include guard to prevent typedef redefinition warnings
 * Define flag before including core_module_parameter.h
 */
#define MODULE_PARAM_TYPES_DEFINED

#include "core_module_parameter.h"
#include "core_logging.h"
#include "core_mymalloc.h"

/* Initial capacity for parameter registries */
#define INITIAL_REGISTRY_CAPACITY 16

/**
 * Create a parameter registry
 * 
 * Allocates and creates a new parameter registry.
 * 
 * @return Pointer to the new registry, or NULL on failure
 */
module_parameter_registry_t *module_parameter_registry_create(void) {
    module_parameter_registry_t *registry = (module_parameter_registry_t *)mymalloc(sizeof(module_parameter_registry_t));
    if (registry == NULL) {
        LOG_ERROR("Failed to allocate memory for parameter registry");
        return NULL;
    }
    
    if (module_parameter_registry_init(registry) != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to initialize parameter registry");
        myfree(registry);
        return NULL;
    }
    
    return registry;
}

/**
 * Destroy a parameter registry
 * 
 * Frees a parameter registry and all its resources.
 * 
 * @param registry Pointer to the registry to destroy
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_parameter_registry_destroy(module_parameter_registry_t *registry) {
    if (registry == NULL) {
        LOG_ERROR("NULL registry pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int status = module_parameter_registry_free(registry);
    if (status != MODULE_PARAM_SUCCESS) {
        LOG_ERROR("Failed to free parameter registry");
        return status;
    }
    
    myfree(registry);
    return MODULE_PARAM_SUCCESS;
}

/**
 * Initialize a parameter registry
 * 
 * Allocates and initializes a new parameter registry.
 * 
 * @param registry Pointer to the registry to initialize
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_parameter_registry_init(module_parameter_registry_t *registry) {
    if (registry == NULL) {
        LOG_ERROR("NULL registry pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Allocate initial memory for parameters */
    registry->parameters = (module_parameter_t *)mymalloc(INITIAL_REGISTRY_CAPACITY * sizeof(module_parameter_t));
    if (registry->parameters == NULL) {
        LOG_ERROR("Failed to allocate memory for parameter registry");
        return MODULE_PARAM_OUT_OF_MEMORY;
    }
    
    registry->num_parameters = 0;
    registry->capacity = INITIAL_REGISTRY_CAPACITY;
    
    LOG_DEBUG("Parameter registry initialized with capacity %d", registry->capacity);
    return MODULE_PARAM_SUCCESS;
}

/**
 * Free a parameter registry
 * 
 * Releases resources used by a parameter registry.
 * 
 * @param registry Pointer to the registry to free
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_parameter_registry_free(module_parameter_registry_t *registry) {
    if (registry == NULL) {
        LOG_ERROR("NULL registry pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    if (registry->parameters != NULL) {
        myfree(registry->parameters);
        registry->parameters = NULL;
    }
    
    registry->num_parameters = 0;
    registry->capacity = 0;
    
    LOG_DEBUG("Parameter registry freed");
    return MODULE_PARAM_SUCCESS;
}

/**
 * Register a parameter with a registry
 * 
 * Adds a new parameter to a registry.
 * 
 * @param registry Pointer to the registry
 * @param param Pointer to the parameter to register
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_register_parameter(module_parameter_registry_t *registry, const module_parameter_t *param) {
    if (registry == NULL || param == NULL) {
        LOG_ERROR("NULL registry or parameter pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Validate parameter */
    if (!module_validate_parameter(param)) {
        LOG_ERROR("Invalid parameter: %s", param->name);
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Check if parameter already exists */
    if (module_find_parameter(registry, param->name, param->module_id) >= 0) {
        LOG_WARNING("Parameter already exists: %s (module ID %d)", param->name, param->module_id);
        return MODULE_PARAM_ALREADY_EXISTS;
    }
    
    /* Check if we need to expand the registry */
    if (registry->num_parameters >= registry->capacity) {
        int new_capacity = registry->capacity * 2;
        module_parameter_t *new_params = (module_parameter_t *)myrealloc(
            registry->parameters, new_capacity * sizeof(module_parameter_t));
        
        if (new_params == NULL) {
            LOG_ERROR("Failed to reallocate memory for parameter registry");
            return MODULE_PARAM_OUT_OF_MEMORY;
        }
        
        registry->parameters = new_params;
        registry->capacity = new_capacity;
        LOG_DEBUG("Parameter registry expanded to capacity %d", registry->capacity);
    }
    
    /* Add the parameter to the registry */
    registry->parameters[registry->num_parameters] = *param;
    registry->num_parameters++;
    
    LOG_DEBUG("Parameter registered: %s (module ID %d)", param->name, param->module_id);
    return MODULE_PARAM_SUCCESS;
}

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
int module_find_parameter(module_parameter_registry_t *registry, const char *name, int module_id) {
    if (registry == NULL || name == NULL) {
        LOG_ERROR("NULL registry or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    for (int i = 0; i < registry->num_parameters; i++) {
        if (registry->parameters[i].module_id == module_id && 
            strcmp(registry->parameters[i].name, name) == 0) {
            return i;
        }
    }
    
    return MODULE_PARAM_NOT_FOUND;
}

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
int module_get_parameter_by_index(module_parameter_registry_t *registry, int index, module_parameter_t *param) {
    if (registry == NULL || param == NULL) {
        LOG_ERROR("NULL registry or parameter pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    if (index < 0 || index >= registry->num_parameters) {
        LOG_ERROR("Parameter index out of bounds: %d", index);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    *param = registry->parameters[index];
    return MODULE_PARAM_SUCCESS;
}

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
int module_get_parameter(module_parameter_registry_t *registry, const char *name, int module_id, module_parameter_t *param) {
    if (registry == NULL || name == NULL || param == NULL) {
        LOG_ERROR("NULL registry, name, or parameter pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    *param = registry->parameters[index];
    return MODULE_PARAM_SUCCESS;
}

/**
 * Validate a parameter
 * 
 * Checks that a parameter is valid.
 * 
 * @param param Pointer to the parameter to validate
 * @return true if the parameter is valid, false otherwise
 */
bool module_validate_parameter(const module_parameter_t *param) {
    if (param == NULL) {
        LOG_ERROR("NULL parameter pointer");
        return false;
    }
    
    /* Check that name is not empty */
    if (param->name[0] == '\0') {
        LOG_ERROR("Parameter name cannot be empty");
        return false;
    }
    
    /* Check that type is valid */
    if (param->type < MODULE_PARAM_TYPE_INT || param->type > MODULE_PARAM_TYPE_STRING) {
        LOG_ERROR("Invalid parameter type: %d", param->type);
        return false;
    }
    
    /* Check bounds for numeric types */
    if (param->has_limits) {
        switch (param->type) {
            case MODULE_PARAM_TYPE_INT:
                if (param->limits.int_range.min > param->limits.int_range.max) {
                    LOG_ERROR("Invalid int bounds: min (%d) > max (%d)", 
                             param->limits.int_range.min, param->limits.int_range.max);
                    return false;
                }
                break;
            case MODULE_PARAM_TYPE_FLOAT:
                if (param->limits.float_range.min > param->limits.float_range.max) {
                    LOG_ERROR("Invalid float bounds: min (%f) > max (%f)", 
                             param->limits.float_range.min, param->limits.float_range.max);
                    return false;
                }
                break;
            case MODULE_PARAM_TYPE_DOUBLE:
                if (param->limits.double_range.min > param->limits.double_range.max) {
                    LOG_ERROR("Invalid double bounds: min (%f) > max (%f)", 
                             param->limits.double_range.min, param->limits.double_range.max);
                    return false;
                }
                break;
            default:
                /* Non-numeric types don't have bounds */
                LOG_WARNING("Bounds specified for non-numeric parameter type: %d", param->type);
                break;
        }
    }
    
    return true;
}

/**
 * Check parameter bounds
 * 
 * Verifies that a parameter's value is within its defined bounds.
 * 
 * @param param Pointer to the parameter to check
 * @return true if within bounds or no bounds defined, false if out of bounds
 */
bool module_check_parameter_bounds(const module_parameter_t *param) {
    if (param == NULL) {
        LOG_ERROR("NULL parameter pointer");
        return false;
    }
    
    /* If no bounds are defined, value is always within bounds */
    if (!param->has_limits) {
        return true;
    }
    
    /* Check bounds based on parameter type */
    switch (param->type) {
        case MODULE_PARAM_TYPE_INT:
            if (param->value.int_val < param->limits.int_range.min || 
                param->value.int_val > param->limits.int_range.max) {
                LOG_ERROR("Int parameter out of bounds: %d not in [%d, %d]", 
                         param->value.int_val, param->limits.int_range.min, param->limits.int_range.max);
                return false;
            }
            break;
        case MODULE_PARAM_TYPE_FLOAT:
            if (param->value.float_val < param->limits.float_range.min || 
                param->value.float_val > param->limits.float_range.max) {
                LOG_ERROR("Float parameter out of bounds: %f not in [%f, %f]", 
                         param->value.float_val, param->limits.float_range.min, param->limits.float_range.max);
                return false;
            }
            break;
        case MODULE_PARAM_TYPE_DOUBLE:
            if (param->value.double_val < param->limits.double_range.min || 
                param->value.double_val > param->limits.double_range.max) {
                LOG_ERROR("Double parameter out of bounds: %f not in [%f, %f]", 
                         param->value.double_val, param->limits.double_range.min, param->limits.double_range.max);
                return false;
            }
            break;
        default:
            /* Non-numeric types don't have bounds */
            return true;
    }
    
    return true;
}

/**
 * Convert parameter type to string
 * 
 * Returns a string representation of a parameter type.
 * 
 * @param type Parameter type
 * @return String representation of the type
 */
const char *module_parameter_type_to_string(enum module_parameter_type type) {
    switch (type) {
        case MODULE_PARAM_TYPE_INT:
            return "int";
        case MODULE_PARAM_TYPE_FLOAT:
            return "float";
        case MODULE_PARAM_TYPE_DOUBLE:
            return "double";
        case MODULE_PARAM_TYPE_BOOL:
            return "bool";
        case MODULE_PARAM_TYPE_STRING:
            return "string";
        default:
            return "unknown";
    }
}

/**
 * Parse parameter type from string
 * 
 * Converts a string representation to a parameter type.
 * 
 * @param type_str String representation of the type
 * @return Parameter type, or MODULE_PARAM_TYPE_INT if not recognized
 */
enum module_parameter_type module_parameter_type_from_string(const char *type_str) {
    if (type_str == NULL) {
        return MODULE_PARAM_TYPE_INT;
    }
    
    if (strcmp(type_str, "int") == 0) {
        return MODULE_PARAM_TYPE_INT;
    } else if (strcmp(type_str, "float") == 0) {
        return MODULE_PARAM_TYPE_FLOAT;
    } else if (strcmp(type_str, "double") == 0) {
        return MODULE_PARAM_TYPE_DOUBLE;
    } else if (strcmp(type_str, "bool") == 0 || strcmp(type_str, "boolean") == 0) {
        return MODULE_PARAM_TYPE_BOOL;
    } else if (strcmp(type_str, "string") == 0 || strcmp(type_str, "str") == 0) {
        return MODULE_PARAM_TYPE_STRING;
    }
    
    LOG_WARNING("Unknown parameter type: %s, defaulting to int", type_str);
    return MODULE_PARAM_TYPE_INT;
}

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
int module_get_parameter_int(module_parameter_registry_t *registry, const char *name, int module_id, int *value) {
    if (registry == NULL || name == NULL || value == NULL) {
        LOG_ERROR("NULL registry, name, or value pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_INT) {
        LOG_ERROR("Type mismatch: parameter %s is not an int", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    *value = param->value.int_val;
    return MODULE_PARAM_SUCCESS;
}

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
int module_get_parameter_float(module_parameter_registry_t *registry, const char *name, int module_id, float *value) {
    if (registry == NULL || name == NULL || value == NULL) {
        LOG_ERROR("NULL registry, name, or value pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_FLOAT) {
        LOG_ERROR("Type mismatch: parameter %s is not a float", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    *value = param->value.float_val;
    return MODULE_PARAM_SUCCESS;
}

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
int module_get_parameter_double(module_parameter_registry_t *registry, const char *name, int module_id, double *value) {
    if (registry == NULL || name == NULL || value == NULL) {
        LOG_ERROR("NULL registry, name, or value pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_DOUBLE) {
        LOG_ERROR("Type mismatch: parameter %s is not a double", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    *value = param->value.double_val;
    return MODULE_PARAM_SUCCESS;
}

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
int module_get_parameter_bool(module_parameter_registry_t *registry, const char *name, int module_id, bool *value) {
    if (registry == NULL || name == NULL || value == NULL) {
        LOG_ERROR("NULL registry, name, or value pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_BOOL) {
        LOG_ERROR("Type mismatch: parameter %s is not a bool", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    *value = param->value.bool_val;
    return MODULE_PARAM_SUCCESS;
}

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
int module_get_parameter_string(module_parameter_registry_t *registry, const char *name, int module_id, char *value, size_t max_len) {
    if (registry == NULL || name == NULL || value == NULL) {
        LOG_ERROR("NULL registry, name, or value pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_STRING) {
        LOG_ERROR("Type mismatch: parameter %s is not a string", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    strncpy(value, param->value.string_val, max_len - 1);
    value[max_len - 1] = '\0';  /* Ensure null termination */
    
    return MODULE_PARAM_SUCCESS;
}

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
int module_set_parameter_int(module_parameter_registry_t *registry, const char *name, int module_id, int value) {
    if (registry == NULL || name == NULL) {
        LOG_ERROR("NULL registry or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_INT) {
        LOG_ERROR("Type mismatch: parameter %s is not an int", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    /* Check bounds */
    if (param->has_limits) {
        if (value < param->limits.int_range.min || value > param->limits.int_range.max) {
            LOG_ERROR("Value out of bounds: %d not in [%d, %d]", 
                     value, param->limits.int_range.min, param->limits.int_range.max);
            return MODULE_PARAM_OUT_OF_BOUNDS;
        }
    }
    
    param->value.int_val = value;
    LOG_DEBUG("Parameter %s set to %d", name, value);
    
    return MODULE_PARAM_SUCCESS;
}

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
int module_set_parameter_float(module_parameter_registry_t *registry, const char *name, int module_id, float value) {
    if (registry == NULL || name == NULL) {
        LOG_ERROR("NULL registry or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_FLOAT) {
        LOG_ERROR("Type mismatch: parameter %s is not a float", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    /* Check bounds */
    if (param->has_limits) {
        if (value < param->limits.float_range.min || value > param->limits.float_range.max) {
            LOG_ERROR("Value out of bounds: %f not in [%f, %f]", 
                     value, param->limits.float_range.min, param->limits.float_range.max);
            return MODULE_PARAM_OUT_OF_BOUNDS;
        }
    }
    
    param->value.float_val = value;
    LOG_DEBUG("Parameter %s set to %f", name, value);
    
    return MODULE_PARAM_SUCCESS;
}

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
int module_set_parameter_double(module_parameter_registry_t *registry, const char *name, int module_id, double value) {
    if (registry == NULL || name == NULL) {
        LOG_ERROR("NULL registry or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_DOUBLE) {
        LOG_ERROR("Type mismatch: parameter %s is not a double", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    /* Check bounds */
    if (param->has_limits) {
        if (value < param->limits.double_range.min || value > param->limits.double_range.max) {
            LOG_ERROR("Value out of bounds: %f not in [%f, %f]", 
                     value, param->limits.double_range.min, param->limits.double_range.max);
            return MODULE_PARAM_OUT_OF_BOUNDS;
        }
    }
    
    param->value.double_val = value;
    LOG_DEBUG("Parameter %s set to %f", name, value);
    
    return MODULE_PARAM_SUCCESS;
}

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
int module_set_parameter_bool(module_parameter_registry_t *registry, const char *name, int module_id, bool value) {
    if (registry == NULL || name == NULL) {
        LOG_ERROR("NULL registry or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_BOOL) {
        LOG_ERROR("Type mismatch: parameter %s is not a bool", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    param->value.bool_val = value;
    LOG_DEBUG("Parameter %s set to %s", name, value ? "true" : "false");
    
    return MODULE_PARAM_SUCCESS;
}

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
int module_set_parameter_string(module_parameter_registry_t *registry, const char *name, int module_id, const char *value) {
    if (registry == NULL || name == NULL || value == NULL) {
        LOG_ERROR("NULL registry, name, or value pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    int index = module_find_parameter(registry, name, module_id);
    if (index < 0) {
        LOG_ERROR("Parameter not found: %s (module ID %d)", name, module_id);
        return MODULE_PARAM_NOT_FOUND;
    }
    
    module_parameter_t *param = &registry->parameters[index];
    if (param->type != MODULE_PARAM_TYPE_STRING) {
        LOG_ERROR("Type mismatch: parameter %s is not a string", name);
        return MODULE_PARAM_TYPE_MISMATCH;
    }
    
    strncpy(param->value.string_val, value, MAX_PARAM_STRING - 1);
    param->value.string_val[MAX_PARAM_STRING - 1] = '\0';  /* Ensure null termination */
    
    LOG_DEBUG("Parameter %s set to \"%s\"", name, value);
    
    return MODULE_PARAM_SUCCESS;
}

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
                                const char *units, int module_id) {
    if (param == NULL || name == NULL) {
        LOG_ERROR("NULL parameter or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Zero out the parameter structure */
    memset(param, 0, sizeof(module_parameter_t));
    
    /* Set parameter properties */
    strncpy(param->name, name, MAX_PARAM_NAME - 1);
    param->type = MODULE_PARAM_TYPE_INT;
    param->value.int_val = value;
    param->module_id = module_id;
    
    /* Set bounds if provided */
    if (min != max) {
        param->has_limits = true;
        param->limits.int_range.min = min;
        param->limits.int_range.max = max;
    } else {
        param->has_limits = false;
    }
    
    /* Set description if provided */
    if (description != NULL) {
        strncpy(param->description, description, MAX_PARAM_DESCRIPTION - 1);
    }
    
    /* Set units if provided */
    if (units != NULL) {
        strncpy(param->units, units, MAX_PARAM_UNITS - 1);
    }
    
    return MODULE_PARAM_SUCCESS;
}

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
                                  const char *units, int module_id) {
    if (param == NULL || name == NULL) {
        LOG_ERROR("NULL parameter or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Zero out the parameter structure */
    memset(param, 0, sizeof(module_parameter_t));
    
    /* Set parameter properties */
    strncpy(param->name, name, MAX_PARAM_NAME - 1);
    param->type = MODULE_PARAM_TYPE_FLOAT;
    param->value.float_val = value;
    param->module_id = module_id;
    
    /* Set bounds if provided */
    if (min != max) {
        param->has_limits = true;
        param->limits.float_range.min = min;
        param->limits.float_range.max = max;
    } else {
        param->has_limits = false;
    }
    
    /* Set description if provided */
    if (description != NULL) {
        strncpy(param->description, description, MAX_PARAM_DESCRIPTION - 1);
    }
    
    /* Set units if provided */
    if (units != NULL) {
        strncpy(param->units, units, MAX_PARAM_UNITS - 1);
    }
    
    return MODULE_PARAM_SUCCESS;
}

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
                                   const char *units, int module_id) {
    if (param == NULL || name == NULL) {
        LOG_ERROR("NULL parameter or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Zero out the parameter structure */
    memset(param, 0, sizeof(module_parameter_t));
    
    /* Set parameter properties */
    strncpy(param->name, name, MAX_PARAM_NAME - 1);
    param->type = MODULE_PARAM_TYPE_DOUBLE;
    param->value.double_val = value;
    param->module_id = module_id;
    
    /* Set bounds if provided */
    if (min != max) {
        param->has_limits = true;
        param->limits.double_range.min = min;
        param->limits.double_range.max = max;
    } else {
        param->has_limits = false;
    }
    
    /* Set description if provided */
    if (description != NULL) {
        strncpy(param->description, description, MAX_PARAM_DESCRIPTION - 1);
    }
    
    /* Set units if provided */
    if (units != NULL) {
        strncpy(param->units, units, MAX_PARAM_UNITS - 1);
    }
    
    return MODULE_PARAM_SUCCESS;
}

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
                                 const char *description, int module_id) {
    if (param == NULL || name == NULL) {
        LOG_ERROR("NULL parameter or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Zero out the parameter structure */
    memset(param, 0, sizeof(module_parameter_t));
    
    /* Set parameter properties */
    strncpy(param->name, name, MAX_PARAM_NAME - 1);
    param->type = MODULE_PARAM_TYPE_BOOL;
    param->value.bool_val = value;
    param->module_id = module_id;
    param->has_limits = false;  /* Boolean doesn't have limits */
    
    /* Set description if provided */
    if (description != NULL) {
        strncpy(param->description, description, MAX_PARAM_DESCRIPTION - 1);
    }
    
    return MODULE_PARAM_SUCCESS;
}

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
                                   const char *description, int module_id) {
    if (param == NULL || name == NULL) {
        LOG_ERROR("NULL parameter or name pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    /* Zero out the parameter structure */
    memset(param, 0, sizeof(module_parameter_t));
    
    /* Set parameter properties */
    strncpy(param->name, name, MAX_PARAM_NAME - 1);
    param->type = MODULE_PARAM_TYPE_STRING;
    if (value != NULL) {
        strncpy(param->value.string_val, value, MAX_PARAM_STRING - 1);
    }
    param->module_id = module_id;
    param->has_limits = false;  /* String doesn't have limits */
    
    /* Set description if provided */
    if (description != NULL) {
        strncpy(param->description, description, MAX_PARAM_DESCRIPTION - 1);
    }
    
    return MODULE_PARAM_SUCCESS;
}

/* Parameter import/export functions implementation */

/**
 * Load parameters from a file
 *
 * Reads parameters from a file into a registry.
 *
 * @param registry Pointer to the registry
 * @param filename Path to the parameter file
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_load_parameters_from_file(module_parameter_registry_t *registry, const char *filename) {
    if (registry == NULL || filename == NULL) {
        LOG_ERROR("NULL registry or filename pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        LOG_ERROR("Failed to open parameter file: %s", filename);
        return MODULE_PARAM_FILE_ERROR;
    }
    
    /* Read the entire file into memory */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = (char *)mymalloc(file_size + 1);
    if (buffer == NULL) {
        LOG_ERROR("Failed to allocate memory for file content");
        fclose(file);
        return MODULE_PARAM_OUT_OF_MEMORY;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        LOG_ERROR("Failed to read file: %s", filename);
        myfree(buffer);
        return MODULE_PARAM_FILE_ERROR;
    }
    
    buffer[file_size] = '\0';  /* Ensure null termination */
    
    /* Parse the JSON content */
    char *pos = strstr(buffer, "\"parameters\"");
    if (pos == NULL) {
        LOG_ERROR("Invalid parameter file format: missing 'parameters' array");
        myfree(buffer);
        return MODULE_PARAM_PARSE_ERROR;
    }
    
    pos = strchr(pos, '[');
    if (pos == NULL) {
        LOG_ERROR("Invalid parameter file format: missing opening bracket for parameters array");
        myfree(buffer);
        return MODULE_PARAM_PARSE_ERROR;
    }
    
    pos++;  /* Skip the opening bracket */
    
    /* Process each parameter object */
    while (*pos != '\0' && *pos != ']') {
        /* Skip whitespace */
        while (*pos != '\0' && isspace(*pos)) {
            pos++;
        }
        
        if (*pos == ']') {
            break;  /* End of array */
        }
        
        if (*pos != '{') {
            LOG_ERROR("Invalid parameter file format: missing opening brace for parameter object");
            myfree(buffer);
            return MODULE_PARAM_PARSE_ERROR;
        }
        
        pos++;  /* Skip the opening brace */
        
        /* Initialize a parameter for this object */
        module_parameter_t param;
        memset(&param, 0, sizeof(module_parameter_t));
        
        /* Parse parameter fields */
        while (*pos != '\0' && *pos != '}') {
            /* Skip whitespace */
            while (*pos != '\0' && isspace(*pos)) {
                pos++;
            }
            
            if (*pos == '}') {
                break;  /* End of object */
            }
            
            /* Parse field name */
            if (*pos != '"') {
                LOG_ERROR("Invalid parameter file format: missing opening quote for field name");
                myfree(buffer);
                return MODULE_PARAM_PARSE_ERROR;
            }
            
            pos++;  /* Skip the opening quote */
            char field[MAX_PARAM_NAME];
            int field_idx = 0;
            
            while (*pos != '\0' && *pos != '"' && field_idx < MAX_PARAM_NAME - 1) {
                field[field_idx++] = *pos++;
            }
            
            field[field_idx] = '\0';
            
            if (*pos != '"') {
                LOG_ERROR("Invalid parameter file format: missing closing quote for field name");
                myfree(buffer);
                return MODULE_PARAM_PARSE_ERROR;
            }
            
            pos++;  /* Skip the closing quote */
            
            /* Skip to the colon */
            while (*pos != '\0' && *pos != ':') {
                pos++;
            }
            
            if (*pos != ':') {
                LOG_ERROR("Invalid parameter file format: missing colon after field name");
                myfree(buffer);
                return MODULE_PARAM_PARSE_ERROR;
            }
            
            pos++;  /* Skip the colon */
            
            /* Skip whitespace */
            while (*pos != '\0' && isspace(*pos)) {
                pos++;
            }
            
            /* Parse field value based on field name */
            if (strcmp(field, "name") == 0) {
                /* Parse string value (name) */
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing opening quote for name value");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the opening quote */
                int name_idx = 0;
                
                while (*pos != '\0' && *pos != '"' && name_idx < MAX_PARAM_NAME - 1) {
                    param.name[name_idx++] = *pos++;
                }
                
                param.name[name_idx] = '\0';
                
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing closing quote for name value");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the closing quote */
            } else if (strcmp(field, "module_id") == 0) {
                /* Parse integer value (module_id) */
                char *end;
                param.module_id = (int)strtol(pos, &end, 10);
                pos = end;
            } else if (strcmp(field, "type") == 0) {
                /* Parse string value (type) */
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing opening quote for type value");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the opening quote */
                char type_str[MAX_PARAM_NAME];
                int type_idx = 0;
                
                while (*pos != '\0' && *pos != '"' && type_idx < MAX_PARAM_NAME - 1) {
                    type_str[type_idx++] = *pos++;
                }
                
                type_str[type_idx] = '\0';
                
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing closing quote for type value");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the closing quote */
                
                /* Convert type string to enum */
                param.type = module_parameter_type_from_string(type_str);
            } else if (strcmp(field, "value") == 0) {
                /* Parse value based on parameter type */
                switch (param.type) {
                    case MODULE_PARAM_TYPE_INT:
                        {
                            char *end;
                            param.value.int_val = (int)strtol(pos, &end, 10);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_FLOAT:
                        {
                            char *end;
                            param.value.float_val = (float)strtod(pos, &end);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_DOUBLE:
                        {
                            char *end;
                            param.value.double_val = strtod(pos, &end);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_BOOL:
                        if (strncmp(pos, "true", 4) == 0) {
                            param.value.bool_val = true;
                            pos += 4;
                        } else if (strncmp(pos, "false", 5) == 0) {
                            param.value.bool_val = false;
                            pos += 5;
                        } else {
                            /* Try to parse as 0/1 */
                            char *end;
                            param.value.bool_val = (strtol(pos, &end, 10) != 0);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_STRING:
                        if (*pos != '"') {
                            LOG_ERROR("Invalid parameter file format: missing opening quote for string value");
                            myfree(buffer);
                            return MODULE_PARAM_PARSE_ERROR;
                        }
                        
                        pos++;  /* Skip the opening quote */
                        int value_idx = 0;
                        
                        while (*pos != '\0' && *pos != '"' && value_idx < MAX_PARAM_STRING - 1) {
                            /* Handle escaped characters */
                            if (*pos == '\\' && *(pos + 1) != '\0') {
                                pos++;
                                switch (*pos) {
                                    case 'n': param.value.string_val[value_idx++] = '\n'; break;
                                    case 'r': param.value.string_val[value_idx++] = '\r'; break;
                                    case 't': param.value.string_val[value_idx++] = '\t'; break;
                                    case '\\': param.value.string_val[value_idx++] = '\\'; break;
                                    case '"': param.value.string_val[value_idx++] = '"'; break;
                                    default: param.value.string_val[value_idx++] = *pos; break;
                                }
                            } else {
                                param.value.string_val[value_idx++] = *pos;
                            }
                            pos++;
                        }
                        
                        param.value.string_val[value_idx] = '\0';
                        
                        if (*pos != '"') {
                            LOG_ERROR("Invalid parameter file format: missing closing quote for string value");
                            myfree(buffer);
                            return MODULE_PARAM_PARSE_ERROR;
                        }
                        
                        pos++;  /* Skip the closing quote */
                        break;
                    default:
                        /* Skip unknown value */
                        while (*pos != '\0' && *pos != ',' && *pos != '}') {
                            pos++;
                        }
                        break;
                }
            } else if (strcmp(field, "has_limits") == 0) {
                /* Parse boolean value (has_limits) */
                if (strncmp(pos, "true", 4) == 0) {
                    param.has_limits = true;
                    pos += 4;
                } else if (strncmp(pos, "false", 5) == 0) {
                    param.has_limits = false;
                    pos += 5;
                } else {
                    /* Try to parse as 0/1 */
                    char *end;
                    param.has_limits = (strtol(pos, &end, 10) != 0);
                    pos = end;
                }
            } else if (strcmp(field, "min") == 0) {
                /* Parse numeric min value based on parameter type */
                switch (param.type) {
                    case MODULE_PARAM_TYPE_INT:
                        {
                            char *end;
                            param.limits.int_range.min = (int)strtol(pos, &end, 10);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_FLOAT:
                        {
                            char *end;
                            param.limits.float_range.min = (float)strtod(pos, &end);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_DOUBLE:
                        {
                            char *end;
                            param.limits.double_range.min = strtod(pos, &end);
                            pos = end;
                        }
                        break;
                    default:
                        /* Skip value for non-numeric types */
                        while (*pos != '\0' && *pos != ',' && *pos != '}') {
                            pos++;
                        }
                        break;
                }
            } else if (strcmp(field, "max") == 0) {
                /* Parse numeric max value based on parameter type */
                switch (param.type) {
                    case MODULE_PARAM_TYPE_INT:
                        {
                            char *end;
                            param.limits.int_range.max = (int)strtol(pos, &end, 10);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_FLOAT:
                        {
                            char *end;
                            param.limits.float_range.max = (float)strtod(pos, &end);
                            pos = end;
                        }
                        break;
                    case MODULE_PARAM_TYPE_DOUBLE:
                        {
                            char *end;
                            param.limits.double_range.max = strtod(pos, &end);
                            pos = end;
                        }
                        break;
                    default:
                        /* Skip value for non-numeric types */
                        while (*pos != '\0' && *pos != ',' && *pos != '}') {
                            pos++;
                        }
                        break;
                }
            } else if (strcmp(field, "description") == 0) {
                /* Parse string value (description) */
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing opening quote for description");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the opening quote */
                int desc_idx = 0;
                
                while (*pos != '\0' && *pos != '"' && desc_idx < MAX_PARAM_DESCRIPTION - 1) {
                    /* Handle escaped characters */
                    if (*pos == '\\' && *(pos + 1) != '\0') {
                        pos++;
                        switch (*pos) {
                            case 'n': param.description[desc_idx++] = '\n'; break;
                            case 'r': param.description[desc_idx++] = '\r'; break;
                            case 't': param.description[desc_idx++] = '\t'; break;
                            case '\\': param.description[desc_idx++] = '\\'; break;
                            case '"': param.description[desc_idx++] = '"'; break;
                            default: param.description[desc_idx++] = *pos; break;
                        }
                    } else {
                        param.description[desc_idx++] = *pos;
                    }
                    pos++;
                }
                
                param.description[desc_idx] = '\0';
                
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing closing quote for description");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the closing quote */
            } else if (strcmp(field, "units") == 0) {
                /* Parse string value (units) */
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing opening quote for units");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the opening quote */
                int units_idx = 0;
                
                while (*pos != '\0' && *pos != '"' && units_idx < MAX_PARAM_UNITS - 1) {
                    param.units[units_idx++] = *pos++;
                }
                
                param.units[units_idx] = '\0';
                
                if (*pos != '"') {
                    LOG_ERROR("Invalid parameter file format: missing closing quote for units");
                    myfree(buffer);
                    return MODULE_PARAM_PARSE_ERROR;
                }
                
                pos++;  /* Skip the closing quote */
            } else {
                /* Skip unknown field */
                LOG_WARNING("Unknown parameter field: %s", field);
                while (*pos != '\0' && *pos != ',' && *pos != '}') {
                    pos++;
                }
            }
            
            /* Skip to the next field or end of object */
            while (*pos != '\0' && *pos != ',' && *pos != '}') {
                pos++;
            }
            
            if (*pos == ',') {
                pos++;  /* Skip the comma */
            }
        }
        
        if (*pos != '}') {
            LOG_ERROR("Invalid parameter file format: missing closing brace for parameter object");
            myfree(buffer);
            return MODULE_PARAM_PARSE_ERROR;
        }
        
        pos++;  /* Skip the closing brace */
        
        /* Register the parameter if it has a valid name and type */
        if (param.name[0] != '\0') {
            int status = module_register_parameter(registry, &param);
            if (status != MODULE_PARAM_SUCCESS && status != MODULE_PARAM_ALREADY_EXISTS) {
                LOG_WARNING("Failed to register parameter %s from file: %d", param.name, status);
            }
        }
        
        /* Skip to the next object or end of array */
        while (*pos != '\0' && *pos != ',' && *pos != ']') {
            pos++;
        }
        
        if (*pos == ',') {
            pos++;  /* Skip the comma */
        }
    }
    
    LOG_INFO("Loaded parameters from file: %s", filename);
    myfree(buffer);
    return MODULE_PARAM_SUCCESS;
}

/**
 * Save parameters to a file
 *
 * Writes parameters from a registry to a file.
 *
 * @param registry Pointer to the registry
 * @param filename Path to the output file
 * @return MODULE_PARAM_SUCCESS on success, error code on failure
 */
int module_save_parameters_to_file(module_parameter_registry_t *registry, const char *filename) {
    if (registry == NULL || filename == NULL) {
        LOG_ERROR("NULL registry or filename pointer");
        return MODULE_PARAM_INVALID_ARGS;
    }
    
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        LOG_ERROR("Failed to open output file: %s", filename);
        return MODULE_PARAM_FILE_ERROR;
    }
    
    /* Write JSON header */
    fprintf(file, "{\n  \"parameters\": [\n");
    
    /* Write each parameter */
    for (int i = 0; i < registry->num_parameters; i++) {
        module_parameter_t *param = &registry->parameters[i];
        
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"%s\",\n", param->name);
        fprintf(file, "      \"module_id\": %d,\n", param->module_id);
        fprintf(file, "      \"type\": \"%s\",\n", module_parameter_type_to_string(param->type));
        
        /* Write value based on type */
        switch (param->type) {
            case MODULE_PARAM_TYPE_INT:
                fprintf(file, "      \"value\": %d", param->value.int_val);
                break;
            case MODULE_PARAM_TYPE_FLOAT:
                fprintf(file, "      \"value\": %f", param->value.float_val);
                break;
            case MODULE_PARAM_TYPE_DOUBLE:
                fprintf(file, "      \"value\": %f", param->value.double_val);
                break;
            case MODULE_PARAM_TYPE_BOOL:
                fprintf(file, "      \"value\": %s", param->value.bool_val ? "true" : "false");
                break;
            case MODULE_PARAM_TYPE_STRING:
                fprintf(file, "      \"value\": \"%s\"", param->value.string_val);
                break;
        }
        
        /* Write limits if present */
        if (param->has_limits) {
            fprintf(file, ",\n      \"has_limits\": true,\n");
            
            switch (param->type) {
                case MODULE_PARAM_TYPE_INT:
                    fprintf(file, "      \"min\": %d,\n", param->limits.int_range.min);
                    fprintf(file, "      \"max\": %d", param->limits.int_range.max);
                    break;
                case MODULE_PARAM_TYPE_FLOAT:
                    fprintf(file, "      \"min\": %f,\n", param->limits.float_range.min);
                    fprintf(file, "      \"max\": %f", param->limits.float_range.max);
                    break;
                case MODULE_PARAM_TYPE_DOUBLE:
                    fprintf(file, "      \"min\": %f,\n", param->limits.double_range.min);
                    fprintf(file, "      \"max\": %f", param->limits.double_range.max);
                    break;
                default:
                    fprintf(file, "      \"has_limits\": false");
                    break;
            }
        }
        
        /* Write description if present */
        if (param->description[0] != '\0') {
            fprintf(file, param->has_limits ? ",\n" : ",\n");
            fprintf(file, "      \"description\": \"%s\"", param->description);
        }
        
        /* Write units if present */
        if (param->units[0] != '\0') {
            fprintf(file, ",\n      \"units\": \"%s\"", param->units);
        }
        
        /* Close parameter object */
        if (i < registry->num_parameters - 1) {
            fprintf(file, "\n    },\n");
        } else {
            fprintf(file, "\n    }\n");
        }
    }
    
    /* Write JSON footer */
    fprintf(file, "  ]\n}\n");
    
    fclose(file);
    LOG_INFO("Saved parameters to file: %s", filename);
    return MODULE_PARAM_SUCCESS;
}
