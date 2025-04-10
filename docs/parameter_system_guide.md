# Parameter Tuning System Guide

## Overview

The Parameter Tuning System provides a flexible mechanism for registering, validating, and modifying module-specific parameters at runtime. This guide explains how to use the parameter system in your modules.

## Key Concepts

- **Parameter Registry**: Each module has its own parameter registry that stores all parameters for that module.
- **Parameter Types**: Parameters can be integers, floats, doubles, booleans, or strings.
- **Bounds Checking**: Numeric parameters can have optional minimum and maximum bounds.
- **Type Safety**: Parameter access is type-checked at both compile time and runtime.
- **Module Scoping**: Parameters are scoped to their owning module to prevent naming collisions.

## Using the Parameter System

### Registering Parameters

Parameters must be registered with a module before they can be used. The recommended approach is to register parameters during module initialization:

```c
int my_module_initialize(struct params *params, void **module_data) {
    /* Create module data structure */
    my_module_data_t *data = malloc(sizeof(my_module_data_t));
    if (data == NULL) {
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize data */
    memset(data, 0, sizeof(my_module_data_t));
    
    /* Register parameters */
    module_parameter_t param;
    
    /* Integer parameter with bounds */
    module_create_parameter_int(&param, "max_iterations", 100, 1, 1000, 
                               "Maximum number of iterations", NULL, module_id);
    module_register_parameter_with_module(module_id, &param);
    
    /* Float parameter with bounds */
    module_create_parameter_float(&param, "convergence_threshold", 0.001f, 0.0f, 1.0f,
                                 "Convergence threshold for algorithm", NULL, module_id);
    module_register_parameter_with_module(module_id, &param);
    
    /* Boolean parameter */
    module_create_parameter_bool(&param, "enable_feature", true,
                               "Enable special feature", module_id);
    module_register_parameter_with_module(module_id, &param);
    
    /* String parameter */
    module_create_parameter_string(&param, "algorithm", "default",
                                 "Algorithm to use", module_id);
    module_register_parameter_with_module(module_id, &param);
    
    /* Store module data */
    *module_data = data;
    
    return MODULE_STATUS_SUCCESS;
}
```

### Accessing Parameters

Parameters can be accessed using type-specific getter functions:

```c
int my_module_function(int module_id, void *module_data) {
    int max_iterations;
    float threshold;
    bool feature_enabled;
    char algorithm[MAX_PARAM_STRING];
    
    /* Get parameter values */
    module_get_int_parameter(module_id, "max_iterations", &max_iterations);
    module_get_float_parameter(module_id, "convergence_threshold", &threshold);
    module_get_bool_parameter(module_id, "enable_feature", &feature_enabled);
    module_get_string_parameter(module_id, "algorithm", algorithm, MAX_PARAM_STRING);
    
    /* Use parameters */
    for (int i = 0; i < max_iterations; i++) {
        // ...
        if (result_error < threshold) {
            break;
        }
    }
    
    if (feature_enabled) {
        // ...
    }
    
    if (strcmp(algorithm, "advanced") == 0) {
        // ...
    }
    
    return MODULE_STATUS_SUCCESS;
}
```

### Modifying Parameters

Parameters can be modified using type-specific setter functions:

```c
int my_module_configure(module_id, const char *name, const char *value) {
    /* Check parameter name */
    if (strcmp(name, "max_iterations") == 0) {
        int iterations = atoi(value);
        return module_set_int_parameter(module_id, name, iterations);
    } 
    else if (strcmp(name, "convergence_threshold") == 0) {
        float threshold = atof(value);
        return module_set_float_parameter(module_id, name, threshold);
    }
    else if (strcmp(name, "enable_feature") == 0) {
        bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        return module_set_bool_parameter(module_id, name, enabled);
    }
    else if (strcmp(name, "algorithm") == 0) {
        return module_set_string_parameter(module_id, name, value);
    }
    
    /* Unknown parameter */
    return MODULE_STATUS_ERROR;
}
```

### Parameter Validation

The system automatically validates parameters:

- **Type Checking**: Ensures parameters are accessed with the correct type.
- **Bounds Checking**: Ensures numeric parameters are within their defined bounds.
- **NULL Checking**: Protects against NULL pointers and missing parameters.

Error codes from parameter operations indicate what went wrong:

- `MODULE_PARAM_SUCCESS`: Operation completed successfully.
- `MODULE_PARAM_INVALID_ARGS`: Invalid arguments (e.g., NULL pointers).
- `MODULE_PARAM_NOT_FOUND`: Parameter not found in registry.
- `MODULE_PARAM_TYPE_MISMATCH`: Parameter type doesn't match the accessor.
- `MODULE_PARAM_OUT_OF_BOUNDS`: Numeric value is outside defined bounds.
- `MODULE_PARAM_ALREADY_EXISTS`: Parameter already exists (during registration).

## Best Practices

1. **Register Early**: Register all parameters during module initialization.
2. **Use Descriptive Names**: Choose clear, descriptive parameter names.
3. **Add Descriptions**: Provide informative descriptions for all parameters.
4. **Define Bounds**: Set reasonable bounds for numeric parameters.
5. **Handle Errors**: Always check return codes from parameter operations.
6. **Use Type-Specific Functions**: Always use the type-specific getter and setter functions.

## Complete Example

Here's a complete example of a module using the parameter system:

```c
#include "core_module_system.h"
#include "core_module_parameter.h"
#include "core_logging.h"

typedef struct {
    int max_iterations;
    float threshold;
    bool feature_enabled;
    char algorithm[MAX_PARAM_STRING];
} my_module_data_t;

/* Module initialization */
int my_module_initialize(struct params *params, void **module_data) {
    /* Create module data */
    my_module_data_t *data = malloc(sizeof(my_module_data_t));
    if (data == NULL) {
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Get module ID */
    int module_id = my_module.module_id;
    
    /* Register parameters */
    module_parameter_t param;
    
    /* Integer parameter */
    module_create_parameter_int(&param, "max_iterations", 100, 1, 1000, 
                               "Maximum number of iterations", NULL, module_id);
    if (module_register_parameter_with_module(module_id, &param) != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register max_iterations parameter");
    }
    
    /* Float parameter */
    module_create_parameter_float(&param, "threshold", 0.001f, 0.0f, 1.0f,
                                "Convergence threshold", NULL, module_id);
    if (module_register_parameter_with_module(module_id, &param) != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register threshold parameter");
    }
    
    /* Boolean parameter */
    module_create_parameter_bool(&param, "feature_enabled", true,
                              "Enable special feature", module_id);
    if (module_register_parameter_with_module(module_id, &param) != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register feature_enabled parameter");
    }
    
    /* String parameter */
    module_create_parameter_string(&param, "algorithm", "default",
                                "Algorithm to use", module_id);
    if (module_register_parameter_with_module(module_id, &param) != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register algorithm parameter");
    }
    
    /* Set initial values */
    data->max_iterations = 100;
    data->threshold = 0.001f;
    data->feature_enabled = true;
    strcpy(data->algorithm, "default");
    
    /* Store module data */
    *module_data = data;
    
    return MODULE_STATUS_SUCCESS;
}

/* Module cleanup */
int my_module_cleanup(void *module_data) {
    if (module_data != NULL) {
        free(module_data);
    }
    return MODULE_STATUS_SUCCESS;
}

/* Module configuration */
int my_module_configure(void *module_data, const char *name, const char *value) {
    my_module_data_t *data = (my_module_data_t *)module_data;
    int module_id = my_module.module_id;
    
    if (strcmp(name, "max_iterations") == 0) {
        int iterations = atoi(value);
        int status = module_set_int_parameter(module_id, name, iterations);
        if (status == MODULE_PARAM_SUCCESS) {
            data->max_iterations = iterations;
            return MODULE_STATUS_SUCCESS;
        }
        return MODULE_STATUS_ERROR;
    } 
    else if (strcmp(name, "threshold") == 0) {
        float threshold = atof(value);
        int status = module_set_float_parameter(module_id, name, threshold);
        if (status == MODULE_PARAM_SUCCESS) {
            data->threshold = threshold;
            return MODULE_STATUS_SUCCESS;
        }
        return MODULE_STATUS_ERROR;
    }
    else if (strcmp(name, "feature_enabled") == 0) {
        bool enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        int status = module_set_bool_parameter(module_id, name, enabled);
        if (status == MODULE_PARAM_SUCCESS) {
            data->feature_enabled = enabled;
            return MODULE_STATUS_SUCCESS;
        }
        return MODULE_STATUS_ERROR;
    }
    else if (strcmp(name, "algorithm") == 0) {
        int status = module_set_string_parameter(module_id, name, value);
        if (status == MODULE_PARAM_SUCCESS) {
            strncpy(data->algorithm, value, MAX_PARAM_STRING - 1);
            data->algorithm[MAX_PARAM_STRING - 1] = '\0';
            return MODULE_STATUS_SUCCESS;
        }
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_ERROR;
}

/* Module interface */
struct base_module my_module = {
    .name = "MyModule",
    .version = "1.0.0",
    .author = "SAGE Developer",
    .module_id = -1,  /* Will be set by the module system */
    .type = MODULE_TYPE_MISC,
    .initialize = my_module_initialize,
    .cleanup = my_module_cleanup,
    .configure = my_module_configure
};
```

## Further Information

For more details, see the API documentation in `core_module_parameter.h` and `core_module_system.h`.