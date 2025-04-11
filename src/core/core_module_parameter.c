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
#include "cJSON.h"

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

    /* Parse the JSON content using cJSON */
    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            /* Calculate line number for better error reporting */
            int line = 1;
            const char *ptr;
            for (ptr = buffer; ptr < error_ptr && *ptr != '\0'; ptr++) {
                if (*ptr == '\n') line++;
            }
            LOG_ERROR("JSON parse error near line %d: %s", line, error_ptr);
        } else {
            LOG_ERROR("Failed to parse JSON from file: %s", filename);
        }
        myfree(buffer);
        return MODULE_PARAM_PARSE_ERROR;
    }

    /* We don't need the buffer anymore since cJSON has parsed it */
    myfree(buffer);

    /* Get the parameters array */
    cJSON *params_array = cJSON_GetObjectItem(root, "parameters");
    if (params_array == NULL || !cJSON_IsArray(params_array)) {
        LOG_ERROR("Invalid parameter file format: missing or invalid 'parameters' array");
        cJSON_Delete(root);
        return MODULE_PARAM_PARSE_ERROR;
    }

    /* Process each parameter object in the array */
    int param_count = 0;
    int param_idx = 0;
    cJSON *param_obj = NULL;
    
    /* cJSON_ArrayForEach is a macro that expands to a for loop */
    for (param_obj = params_array ? params_array->child : NULL; 
         param_obj != NULL; 
         param_obj = param_obj->next) {
        
        if (!cJSON_IsObject(param_obj)) {
            LOG_WARNING("Parameter %d is not a JSON object, skipping", param_idx);
            param_idx++;
            continue;
        }

        /* Initialize a parameter structure */
        module_parameter_t param;
        memset(&param, 0, sizeof(module_parameter_t));
        
        /* Extract parameter properties */
        
        /* Name (required) */
        cJSON *name_item = cJSON_GetObjectItem(param_obj, "name");
        if (name_item == NULL || !cJSON_IsString(name_item)) {
            LOG_WARNING("Parameter %d missing required 'name' field, skipping", param_idx);
            param_idx++;
            continue;
        }
        strncpy(param.name, cJSON_GetStringValue(name_item), MAX_PARAM_NAME - 1);
        param.name[MAX_PARAM_NAME - 1] = '\0';

        /* Module ID (required) */
        cJSON *module_id_item = cJSON_GetObjectItem(param_obj, "module_id");
        if (module_id_item == NULL || !cJSON_IsNumber(module_id_item)) {
            LOG_WARNING("Parameter '%s' missing required 'module_id' field, skipping", param.name);
            param_idx++;
            continue;
        }
        param.module_id = (int)cJSON_GetNumberValue(module_id_item);

        /* Type (required) */
        cJSON *type_item = cJSON_GetObjectItem(param_obj, "type");
        if (type_item == NULL || !cJSON_IsString(type_item)) {
            LOG_WARNING("Parameter '%s' missing required 'type' field, skipping", param.name);
            param_idx++;
            continue;
        }
        param.type = module_parameter_type_from_string(cJSON_GetStringValue(type_item));

        /* Value (required, but type-dependent) */
        cJSON *value_item = cJSON_GetObjectItem(param_obj, "value");
        if (value_item == NULL) {
            LOG_WARNING("Parameter '%s' missing required 'value' field, skipping", param.name);
            param_idx++;
            continue;
        }

        /* Set value based on type */
        switch (param.type) {
            case MODULE_PARAM_TYPE_INT:
                if (!cJSON_IsNumber(value_item)) {
                    LOG_WARNING("Parameter '%s' has type int but value is not a number, skipping", param.name);
                    param_idx++;
                    continue;
                }
                param.value.int_val = (int)cJSON_GetNumberValue(value_item);
                break;
                
            case MODULE_PARAM_TYPE_FLOAT:
                if (!cJSON_IsNumber(value_item)) {
                    LOG_WARNING("Parameter '%s' has type float but value is not a number, skipping", param.name);
                    param_idx++;
                    continue;
                }
                param.value.float_val = (float)cJSON_GetNumberValue(value_item);
                break;
                
            case MODULE_PARAM_TYPE_DOUBLE:
                if (!cJSON_IsNumber(value_item)) {
                    LOG_WARNING("Parameter '%s' has type double but value is not a number, skipping", param.name);
                    param_idx++;
                    continue;
                }
                param.value.double_val = cJSON_GetNumberValue(value_item);
                break;
                
            case MODULE_PARAM_TYPE_BOOL:
                if (!cJSON_IsBool(value_item)) {
                    LOG_WARNING("Parameter '%s' has type bool but value is not a boolean, skipping", param.name);
                    param_idx++;
                    continue;
                }
                param.value.bool_val = cJSON_IsTrue(value_item);
                break;
                
            case MODULE_PARAM_TYPE_STRING:
                if (!cJSON_IsString(value_item)) {
                    LOG_WARNING("Parameter '%s' has type string but value is not a string, skipping", param.name);
                    param_idx++;
                    continue;
                }
                strncpy(param.value.string_val, cJSON_GetStringValue(value_item), MAX_PARAM_STRING - 1);
                param.value.string_val[MAX_PARAM_STRING - 1] = '\0';
                break;
                
            default:
                LOG_WARNING("Parameter '%s' has unknown type: %d, skipping", param.name, param.type);
                param_idx++;
                continue;
        }

        /* Has limits (optional) */
        cJSON *has_limits_item = cJSON_GetObjectItem(param_obj, "has_limits");
        if (has_limits_item != NULL && cJSON_IsBool(has_limits_item)) {
            param.has_limits = cJSON_IsTrue(has_limits_item);
            
            /* If has_limits is true, read min and max based on type */
            if (param.has_limits) {
                switch (param.type) {
                    case MODULE_PARAM_TYPE_INT:
                        {
                            cJSON *min_item = cJSON_GetObjectItem(param_obj, "min");
                            cJSON *max_item = cJSON_GetObjectItem(param_obj, "max");
                            
                            if (min_item && cJSON_IsNumber(min_item) && 
                                max_item && cJSON_IsNumber(max_item)) {
                                param.limits.int_range.min = (int)cJSON_GetNumberValue(min_item);
                                param.limits.int_range.max = (int)cJSON_GetNumberValue(max_item);
                            } else {
                                param.has_limits = false;
                                LOG_WARNING("Parameter '%s' has incomplete limit values, ignoring limits", param.name);
                            }
                        }
                        break;
                        
                    case MODULE_PARAM_TYPE_FLOAT:
                        {
                            cJSON *min_item = cJSON_GetObjectItem(param_obj, "min");
                            cJSON *max_item = cJSON_GetObjectItem(param_obj, "max");
                            
                            if (min_item && cJSON_IsNumber(min_item) && 
                                max_item && cJSON_IsNumber(max_item)) {
                                param.limits.float_range.min = (float)cJSON_GetNumberValue(min_item);
                                param.limits.float_range.max = (float)cJSON_GetNumberValue(max_item);
                            } else {
                                param.has_limits = false;
                                LOG_WARNING("Parameter '%s' has incomplete limit values, ignoring limits", param.name);
                            }
                        }
                        break;
                        
                    case MODULE_PARAM_TYPE_DOUBLE:
                        {
                            cJSON *min_item = cJSON_GetObjectItem(param_obj, "min");
                            cJSON *max_item = cJSON_GetObjectItem(param_obj, "max");
                            
                            if (min_item && cJSON_IsNumber(min_item) && 
                                max_item && cJSON_IsNumber(max_item)) {
                                param.limits.double_range.min = cJSON_GetNumberValue(min_item);
                                param.limits.double_range.max = cJSON_GetNumberValue(max_item);
                            } else {
                                param.has_limits = false;
                                LOG_WARNING("Parameter '%s' has incomplete limit values, ignoring limits", param.name);
                            }
                        }
                        break;
                        
                    default:
                        param.has_limits = false;
                        LOG_WARNING("Parameter '%s' has limits but is not a numeric type, ignoring limits", param.name);
                        break;
                }
            }
        }

        /* Description (optional) */
        cJSON *desc_item = cJSON_GetObjectItem(param_obj, "description");
        if (desc_item != NULL && cJSON_IsString(desc_item)) {
            strncpy(param.description, cJSON_GetStringValue(desc_item), MAX_PARAM_DESCRIPTION - 1);
            param.description[MAX_PARAM_DESCRIPTION - 1] = '\0';
        }

        /* Units (optional) */
        cJSON *units_item = cJSON_GetObjectItem(param_obj, "units");
        if (units_item != NULL && cJSON_IsString(units_item)) {
            strncpy(param.units, cJSON_GetStringValue(units_item), MAX_PARAM_UNITS - 1);
            param.units[MAX_PARAM_UNITS - 1] = '\0';
        }

        /* Register the parameter */
        int status = module_register_parameter(registry, &param);
        if (status == MODULE_PARAM_SUCCESS) {
            param_count++;
        } else if (status == MODULE_PARAM_ALREADY_EXISTS) {
            LOG_INFO("Parameter '%s' already exists, skipping", param.name);
        } else {
            LOG_WARNING("Failed to register parameter '%s' from file: %d", param.name, status);
        }
        
        param_idx++;
    }

    /* Clean up */
    cJSON_Delete(root);
    LOG_INFO("Loaded %d parameters from file: %s", param_count, filename);
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

    /* Create root JSON object */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        LOG_ERROR("Failed to create JSON root object");
        return MODULE_PARAM_OUT_OF_MEMORY;
    }

    /* Create parameters array */
    cJSON *params_array = cJSON_CreateArray();
    if (params_array == NULL) {
        LOG_ERROR("Failed to create parameters array");
        cJSON_Delete(root);
        return MODULE_PARAM_OUT_OF_MEMORY;
    }

    /* Add parameters array to root object */
    if (!cJSON_AddItemToObject(root, "parameters", params_array)) {
        LOG_ERROR("Failed to add parameters array to root object");
        cJSON_Delete(root);
        return MODULE_PARAM_OUT_OF_MEMORY;
    }

    /* Add each parameter to the array */
    for (int i = 0; i < registry->num_parameters; i++) {
        module_parameter_t *param = &registry->parameters[i];

        /* Create parameter object */
        cJSON *param_obj = cJSON_CreateObject();
        if (param_obj == NULL) {
            LOG_ERROR("Failed to create parameter object");
            cJSON_Delete(root);
            return MODULE_PARAM_OUT_OF_MEMORY;
        }

        /* Add parameter properties */
        if (!cJSON_AddStringToObject(param_obj, "name", param->name) ||
            !cJSON_AddNumberToObject(param_obj, "module_id", param->module_id) ||
            !cJSON_AddStringToObject(param_obj, "type", module_parameter_type_to_string(param->type))) {
            LOG_ERROR("Failed to add basic properties to parameter object");
            cJSON_Delete(param_obj);
            cJSON_Delete(root);
            return MODULE_PARAM_OUT_OF_MEMORY;
        }

        /* Add value based on type */
        switch (param->type) {
            case MODULE_PARAM_TYPE_INT:
                if (!cJSON_AddNumberToObject(param_obj, "value", param->value.int_val)) {
                    LOG_ERROR("Failed to add int value to parameter object");
                    cJSON_Delete(param_obj);
                    cJSON_Delete(root);
                    return MODULE_PARAM_OUT_OF_MEMORY;
                }
                break;
            case MODULE_PARAM_TYPE_FLOAT:
                if (!cJSON_AddNumberToObject(param_obj, "value", param->value.float_val)) {
                    LOG_ERROR("Failed to add float value to parameter object");
                    cJSON_Delete(param_obj);
                    cJSON_Delete(root);
                    return MODULE_PARAM_OUT_OF_MEMORY;
                }
                break;
            case MODULE_PARAM_TYPE_DOUBLE:
                if (!cJSON_AddNumberToObject(param_obj, "value", param->value.double_val)) {
                    LOG_ERROR("Failed to add double value to parameter object");
                    cJSON_Delete(param_obj);
                    cJSON_Delete(root);
                    return MODULE_PARAM_OUT_OF_MEMORY;
                }
                break;
            case MODULE_PARAM_TYPE_BOOL:
                if (!cJSON_AddBoolToObject(param_obj, "value", param->value.bool_val)) {
                    LOG_ERROR("Failed to add bool value to parameter object");
                    cJSON_Delete(param_obj);
                    cJSON_Delete(root);
                    return MODULE_PARAM_OUT_OF_MEMORY;
                }
                break;
            case MODULE_PARAM_TYPE_STRING:
                if (!cJSON_AddStringToObject(param_obj, "value", param->value.string_val)) {
                    LOG_ERROR("Failed to add string value to parameter object");
                    cJSON_Delete(param_obj);
                    cJSON_Delete(root);
                    return MODULE_PARAM_OUT_OF_MEMORY;
                }
                break;
        }

        /* Add limits if present */
        if (param->has_limits) {
            if (!cJSON_AddBoolToObject(param_obj, "has_limits", true)) {
                LOG_ERROR("Failed to add has_limits to parameter object");
                cJSON_Delete(param_obj);
                cJSON_Delete(root);
                return MODULE_PARAM_OUT_OF_MEMORY;
            }

            /* Add min/max values based on parameter type */
            switch (param->type) {
                case MODULE_PARAM_TYPE_INT:
                    if (!cJSON_AddNumberToObject(param_obj, "min", param->limits.int_range.min) ||
                        !cJSON_AddNumberToObject(param_obj, "max", param->limits.int_range.max)) {
                        LOG_ERROR("Failed to add int limits to parameter object");
                        cJSON_Delete(param_obj);
                        cJSON_Delete(root);
                        return MODULE_PARAM_OUT_OF_MEMORY;
                    }
                    break;
                case MODULE_PARAM_TYPE_FLOAT:
                    if (!cJSON_AddNumberToObject(param_obj, "min", param->limits.float_range.min) ||
                        !cJSON_AddNumberToObject(param_obj, "max", param->limits.float_range.max)) {
                        LOG_ERROR("Failed to add float limits to parameter object");
                        cJSON_Delete(param_obj);
                        cJSON_Delete(root);
                        return MODULE_PARAM_OUT_OF_MEMORY;
                    }
                    break;
                case MODULE_PARAM_TYPE_DOUBLE:
                    if (!cJSON_AddNumberToObject(param_obj, "min", param->limits.double_range.min) ||
                        !cJSON_AddNumberToObject(param_obj, "max", param->limits.double_range.max)) {
                        LOG_ERROR("Failed to add double limits to parameter object");
                        cJSON_Delete(param_obj);
                        cJSON_Delete(root);
                        return MODULE_PARAM_OUT_OF_MEMORY;
                    }
                    break;
                default:
                    /* Non-numeric types don't have meaningful limits */
                    break;
            }
        }

        /* Add description if present */
        if (param->description[0] != '\0') {
            if (!cJSON_AddStringToObject(param_obj, "description", param->description)) {
                LOG_ERROR("Failed to add description to parameter object");
                cJSON_Delete(param_obj);
                cJSON_Delete(root);
                return MODULE_PARAM_OUT_OF_MEMORY;
            }
        }

        /* Add units if present */
        if (param->units[0] != '\0') {
            if (!cJSON_AddStringToObject(param_obj, "units", param->units)) {
                LOG_ERROR("Failed to add units to parameter object");
                cJSON_Delete(param_obj);
                cJSON_Delete(root);
                return MODULE_PARAM_OUT_OF_MEMORY;
            }
        }

        /* Add parameter object to parameters array */
        if (!cJSON_AddItemToArray(params_array, param_obj)) {
            LOG_ERROR("Failed to add parameter object to parameters array");
            cJSON_Delete(param_obj);
            cJSON_Delete(root);
            return MODULE_PARAM_OUT_OF_MEMORY;
        }
    }

    /* Generate JSON string with formatting */
    char *json_str = cJSON_Print(root);
    if (json_str == NULL) {
        LOG_ERROR("Failed to generate JSON string");
        cJSON_Delete(root);
        return MODULE_PARAM_OUT_OF_MEMORY;
    }

    /* Write JSON string to file */
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        LOG_ERROR("Failed to open output file: %s", filename);
        cJSON_free(json_str);
        cJSON_Delete(root);
        return MODULE_PARAM_FILE_ERROR;
    }

    if (fputs(json_str, file) < 0) {
        LOG_ERROR("Failed to write to output file: %s", filename);
        fclose(file);
        cJSON_free(json_str);
        cJSON_Delete(root);
        return MODULE_PARAM_FILE_ERROR;
    }

    /* Clean up */
    fclose(file);
    cJSON_free(json_str);
    cJSON_Delete(root);

    LOG_INFO("Saved %d parameters to file: %s", registry->num_parameters, filename);
    return MODULE_PARAM_SUCCESS;
}