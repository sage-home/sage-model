# SAGE Module Development Guide

## Overview

This guide provides detailed information on developing physics modules for the SAGE semi-analytic galaxy evolution model. SAGE's modular architecture allows for runtime insertion, replacement, reordering, and removal of physics modules without recompilation, enabling flexible experimentation with different physics implementations.

## Module Development Framework

SAGE provides a comprehensive development framework to simplify the creation of custom physics modules. The framework includes:

1. **Template Generation**: Automatically generate boilerplate code for new modules
2. **Validation System**: Validate module implementations against interface requirements
3. **Debugging Tools**: Monitor module performance and interactions during development
4. **Documentation**: Comprehensive guides for module development

## Module Structure

A SAGE physics module consists of the following components:

1. **Base Module Interface**: Common attributes and functions for all module types
2. **Type-Specific Interface**: Functions specific to the physics domain (cooling, star formation, etc.)
3. **Module Data**: Private data structure for module state
4. **Optional Features**: Galaxy extensions, event handlers, callbacks, etc.

### Base Module Interface

All modules must implement the base module interface defined in `core_module_system.h`:

```c
struct base_module {
    /* Metadata */
    char name[MAX_MODULE_NAME];
    char version[MAX_MODULE_VERSION];
    char author[MAX_MODULE_AUTHOR];
    int module_id;
    enum module_type type;
    
    /* Lifecycle functions */
    int (*initialize)(struct params *params, void **module_data);
    int (*cleanup)(void *module_data);
    
    /* Configuration functions */
    int (*configure)(void *module_data, const char *config_name, const char *config_value);
    
    /* Error handling */
    int last_error;
    char error_message[MAX_ERROR_MESSAGE];
    
    /* Module manifest */
    struct module_manifest *manifest;
    
    /* Function registry (for callback system) */
    module_function_registry_t *function_registry;
    
    /* Runtime dependencies */
    module_dependency_t *dependencies;
    int num_dependencies;
};
```

### Type-Specific Interfaces

Different physics domains have their own interfaces that extend the base module interface. For example, the cooling module interface:

```c
struct cooling_module {
    struct base_module base;
    
    /* Cooling specific functions */
    double (*calculate_cooling)(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data);
    double (*get_cooling_rate)(int gal_idx, struct GALAXY *galaxies, void *module_data);
};
```

## Creating a New Module

### Using the Template Generator

The easiest way to create a new module is to use the template generator:

```c
// Initialize template parameters
struct module_template_params params;
module_template_params_init(&params);

// Set template parameters
strcpy(params.module_name, "my_cooling_module");
strcpy(params.module_prefix, "mcm");
strcpy(params.author, "Your Name");
strcpy(params.email, "your.email@example.com");
strcpy(params.description, "Custom cooling implementation for SAGE");
strcpy(params.version, "1.0.0");
params.type = MODULE_TYPE_COOLING;

// Set template features
params.include_galaxy_extension = true;
params.include_event_handler = true;
params.include_callback_registration = true;
params.include_manifest = true;
params.include_makefile = true;
params.include_test_file = true;
params.include_readme = true;

// Set output directory
strcpy(params.output_dir, "modules/my_cooling_module");

// Generate the template
int result = module_generate_template(&params);
```

This will generate a complete module structure in the specified output directory.

### Module Implementation

After generating the template, you need to implement the module's functionality. Focus on the following key areas:

1. **Module Data Structure**: Define the data structure for your module's state
2. **Initialize Function**: Set up your module's data and resources
3. **Cleanup Function**: Release any resources allocated by your module
4. **Physics Functions**: Implement the type-specific functions for your physics domain

The template includes placeholder implementations for all required functions.

## Optional Features

### Galaxy Property Extensions

Modules can define custom properties for galaxies using the extension system:

```c
// In your initialize function
galaxy_property_t property = {
    .name = "cooling_radius",
    .size = sizeof(float),
    .module_id = getCurrentModuleId(),
    .serialize = serialize_float,
    .deserialize = deserialize_float,
    .description = "Cooling radius for galaxy",
    .units = "kpc"
};
data->property_id = register_galaxy_property(&property);

// In your physics functions
float *cooling_radius = galaxy_extension_get_data(&galaxies[gal_idx], data->property_id);
if (cooling_radius) {
    *cooling_radius = calculate_cooling_radius(...);
}
```

### Event Handling

Modules can register handlers for system events:

```c
// In your initialize function
event_register_handler(EVENT_GALAXY_CREATED, my_module_handle_event, data);

// Event handler implementation
int my_module_handle_event(const struct event *event, void *user_data) {
    if (event->type == EVENT_GALAXY_CREATED) {
        int gal_idx = *(int *)event->data;
        // Initialize extension data for the new galaxy
    }
    return 0;
}
```

### Module Callbacks

Modules can register functions for other modules to call:

```c
// In your initialize function
module_register_function(
    getCurrentModuleId(),
    "calculate_cooling_radius",
    (void *)my_module_calculate_cooling_radius,
    FUNCTION_TYPE_DOUBLE,
    "double (int, struct GALAXY *, void *)",
    "Calculate cooling radius for a galaxy"
);

// Function implementation
double my_module_calculate_cooling_radius(int gal_idx, struct GALAXY *galaxies, void *module_data) {
    // Implementation
    return radius;
}
```

## Module Validation

The validation system helps ensure your module meets the requirements:

```c
// Initialize validation
module_validation_options_t options;
module_validation_options_init(&options);
options.validate_interface = true;
options.validate_function_sigs = true;
options.validate_dependencies = true;
options.validate_manifest = true;
options.verbose = true;

module_validation_result_t result;
module_validation_result_init(&result);

// Validate a module by ID
bool valid = module_validate_by_id(module_id, &result, &options);

// Check for errors
if (module_validation_has_errors(&result, &options)) {
    char output[4096];
    module_validation_result_format(&result, output, sizeof(output));
    printf("Validation failed:\n%s\n", output);
} else {
    printf("Module validated successfully.\n");
}
```

## Debugging Tools

SAGE provides debugging tools to help during module development:

```c
// Enable call tracing
module_debug_set_tracing(true);

// Run some module operations...

// Get and display call trace
module_call_trace_t trace;
module_debug_get_call_trace(&trace);

char output[4096];
module_debug_format_call_trace(&trace, output, sizeof(output));
printf("Call trace:\n%s\n", output);

// Get performance metrics
module_performance_metrics_t metrics[MAX_MODULES];
int num_metrics;
module_debug_get_all_performance_metrics(metrics, MAX_MODULES, &num_metrics);

module_debug_format_performance_metrics(metrics, num_metrics, output, sizeof(output));
printf("Performance metrics:\n%s\n", output);
```

## Module Manifest

Each module should have a manifest file that describes its metadata:

```
# Manifest file for my_cooling_module
name: my_cooling_module
version: 1.0.0
author: Your Name
email: your.email@example.com
description: Custom cooling implementation for SAGE
type: cooling
library: my_cooling_module.so
api_version: 1
auto_initialize: true
auto_activate: false
capabilities: 0x0001
```

## Building and Installation

To build and install your module:

1. Make sure your module directory contains the source files and manifest
2. Use the provided Makefile (if using the template generator)
3. Build your module: `make`
4. Install your module to SAGE's modules directory: `make install`

Alternatively, manually build your module:

```bash
# Build as a shared library
gcc -fPIC -shared -O2 -Wall -I/path/to/sage/src -I/path/to/sage/src/core \
    my_module.c -o my_module.so

# Copy module and manifest to SAGE's modules directory
cp my_module.so my_module.manifest /path/to/sage/modules/
```

## Using Your Module in SAGE

To use your module in SAGE:

1. Ensure it's installed in the modules directory
2. Configure SAGE to use module discovery:
```
ModuleDir        ./modules
EnableModuleDiscovery 1
```
3. Run SAGE as normal

## Best Practices

1. **Memory Management**: Always clean up allocated resources
2. **Thread Safety**: Avoid global state, use module_data for state
3. **Error Handling**: Check for errors and report detailed error messages
4. **Documentation**: Document your module's purpose, interface, and usage
5. **Testing**: Create comprehensive tests for your module
6. **Performance**: Monitor and optimize your module's performance

## Module Development Tips

1. Start with the template generator to create the basic structure
2. Use the validation system to ensure your module meets the requirements
3. Implement the most critical functions first, then add optional features
4. Test your module thoroughly with the provided testing framework
5. Use the debugging tools to monitor performance and interactions

## Troubleshooting

Common issues and solutions:

1. **Module not found**: Check module path and manifest file
2. **Symbol not found**: Ensure all required functions are implemented
3. **Dependency not satisfied**: Check that required modules are loaded
4. **Memory leaks**: Use valgrind to check for memory leaks
5. **Performance issues**: Use the debugging tools to identify bottlenecks

## References

- SAGE Documentation: [Link to main SAGE documentation]
- Module System API: See `core_module_system.h`
- Template Generator: See `core_module_template.h`
- Validation System: See `core_module_validation.h`
- Debugging Tools: See `core_module_debug.h`
