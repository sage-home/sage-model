# Configuration-Driven Pipeline Creation

## Overview

This document describes the implementation of configuration-driven module activation in the SAGE pipeline creation process. This feature allows users to define which modules are active in a simulation by specifying them in a JSON configuration file, rather than having them hardcoded in the pipeline registry. This completes the separation between core infrastructure and physics implementations by making module activation fully configurable at runtime.

## Background

In the original SAGE implementation, the `pipeline_create_with_standard_modules()` function in `src/core/core_pipeline_registry.c` would add all registered module factories to the pipeline regardless of configuration. This approach violated the principle of core-physics separation, as the core infrastructure had hardcoded knowledge of which modules constituted a "standard" run.

## Implementation

The configuration-driven pipeline creation implementation consists of several key components:

### Configuration File Format

The JSON configuration file contains a `modules.instances` array that defines which modules should be active:

```json
{
    "modules": {
        "instances": [
            {
                "name": "cooling_module", // Name matching the factory registration
                "enabled": true           // Whether to include in the pipeline
            },
            {
                "name": "infall_module",
                "enabled": true
            },
            {
                "name": "disabled_module",
                "enabled": false         // This module won't be added to the pipeline
            }
        ]
    }
}
```

### Module Factory Registration

Modules register their factories with the pipeline registry using the `pipeline_register_module_factory()` function:

```c
pipeline_register_module_factory(MODULE_TYPE_COOLING, "cooling_module", cooling_module_factory);
```

This is typically done automatically via constructor attributes:

```c
__attribute__((constructor))
static void register_cooling_module_factory(void) {
    pipeline_register_module_factory(MODULE_TYPE_COOLING, "cooling_module", cooling_module_factory);
}
```

### Pipeline Creation

The `pipeline_create_with_standard_modules()` function now follows this process:

1. Create an empty pipeline
2. Check if a global configuration is available
3. If available, read the `modules.instances` array from the configuration
4. For each enabled module in the configuration, look for a matching factory and create the module
5. If no configuration is available or the array is not found, fall back to using all registered modules

### Configuration Helper Functions

Two helper functions were implemented to assist with extracting values from the configuration objects:

```c
static bool get_boolean_from_object(const struct config_object *obj, const char *key, bool default_value);
static const char *get_string_from_object(const struct config_object *obj, const char *key, const char *default_value);
```

These functions handle type conversion and default values, making it easier to work with the configuration data.

### Fallback Mechanism

The implementation includes a robust fallback mechanism that ensures SAGE can still run even if no configuration is available or the `modules.instances` array is missing. In such cases, all registered module factories are added to the pipeline, preserving backward compatibility.

## Key Benefits

The configuration-driven pipeline creation implementation provides several key benefits:

1. **True Runtime Modularity**: Users can define entirely different sets of active physics modules by changing the configuration file, without recompiling the code.

2. **Complete Core-Physics Decoupling**: The core pipeline creation logic has no compile-time or hardcoded knowledge of which specific physics modules constitute a "standard" run. "Standard" becomes whatever is defined in the active configuration.

3. **Enhanced Flexibility**: This approach facilitates easier experimentation with different combinations of physics modules. Users can enable or disable modules without modifying code.

4. **Clear Separation of Concerns**: Module self-registration (via constructors) makes modules *available*, while the configuration *selects and enables* them for a specific pipeline. This separation of registration and activation enhances modularity.

## Testing

The implementation includes comprehensive tests in `tests/test_core_pipeline_registry.c` that verify:

1. The pipeline creation process works correctly with no configuration (fallback to all modules)
2. The pipeline creation process correctly filters modules based on the `enabled` flag in the configuration
3. The pipeline steps contain only the enabled modules from the configuration

## Usage

To use this feature:

1. Create a JSON configuration file with a `modules.instances` array
2. For each module, specify its name and whether it should be enabled
3. Load the configuration file before creating the pipeline
4. Call `pipeline_create_with_standard_modules()` to create a pipeline based on the configuration

Example:

```c
// Initialize configuration system
config_system_initialize();

// Load configuration file
config_load_file("path/to/config.json");

// Create pipeline based on configuration
struct module_pipeline *pipeline = pipeline_create_with_standard_modules();
```

## Conclusion

The configuration-driven pipeline creation implementation represents the final step in fully decoupling the core infrastructure from specific physics implementations. It completes the core-physics separation requirements (FR-1.1 and FR-1.2) by making module activation fully configurable at runtime, enhancing SAGE's flexibility and modularity.
