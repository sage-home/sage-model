# SAGE Module System and Configuration

## Overview

The SAGE module system provides a flexible, extensible framework for implementing, registering, and executing physics modules. The system is designed to maintain a strict separation between core infrastructure and physics implementations, allowing for runtime configuration of which modules are active without recompiling the code. This document outlines the key components of the module system, the configuration file format, and how to implement, register, and activate modules.

## Table of Contents

1. [Module System Architecture](#module-system-architecture)
2. [Configuration File Format](#configuration-file-format)
3. [Module Registration and Activation](#module-registration-and-activation)
4. [Pipeline Execution Phases](#pipeline-execution-phases)
5. [Core-Physics Separation](#core-physics-separation)
6. [Property System Integration](#property-system-integration)
7. [Configuration-Driven Pipeline Creation](#configuration-driven-pipeline-creation)
8. [Module Development Guidelines](#module-development-guidelines)

## Module System Architecture

The SAGE module system consists of several key components:

### Module Registry
- Defined in `src/core/core_module_system.c/h`
- Manages module registration, initialization, and lifecycle
- Tracks active modules and their states
- Provides interfaces for module discovery and validation

### Pipeline System
- Defined in `src/core/core_pipeline_system.c/h`
- Creates and manages execution pipelines
- Registers modules as pipeline steps
- Controls execution flow and phase transitions

### Pipeline Registry
- Defined in `src/core/core_pipeline_registry.c/h`
- Registers module factories for runtime instantiation
- Creates standard pipelines based on configuration
- Manages the mapping between module names and factories

### Module Callbacks
- Defined in `src/core/core_module_callback.c/h`
- Enables controlled inter-module communication
- Provides call stack tracking and circular dependency detection
- Supports function registration for module-to-module calls

### Event System
- Defined in `src/core/core_event_system.c/h`
- Enables event-based communication between modules
- Supports event emission, subscription, and handling
- Preserves scientific dependencies between physics processes

### Property System
- Defined in `src/core/core_properties.c/h` (generated)
- Provides a centralized definition of all galaxy properties
- Generates type-safe accessors for property access (GALAXY_PROP_*)
- Separates core infrastructure properties from physics properties

### Galaxy Extensions
- Defined in `src/core/core_galaxy_extensions.c/h`
- Allows modules to attach arbitrary data to galaxies
- Provides memory management for module-specific data
- Supports deep copying and cleanup of extension data

## Configuration File Format

SAGE uses JSON configuration files to define module activation and parameters. The standard configuration file is located at `input/config.json`. The key sections relevant to the module system are:

```json
{
    "modules": {
        "discovery_enabled": true,
        "search_paths": [
            "./src/physics"
            // Additional module search paths
        ],
        "instances": [
            {
                "name": "cooling_module",  // Name used for factory registration
                "enabled": true,          // Whether to include in the pipeline
                // Module-specific parameters
            },
            // Additional module instances
        ]
    },
    "pipeline": {
        "name": "standard_physics",
        "use_as_global": true,
        "steps": [
            {"type": "infall", "name": "infall", "enabled": true, "optional": true},
            // Additional pipeline steps
        ]
    }
}
```

### Key Configuration Fields

- `modules.discovery_enabled`: Whether to enable runtime module discovery
- `modules.search_paths`: Directories to search for modules
- `modules.instances`: Array of module instances to activate
- `modules.instances[].name`: Name of the module factory to use
- `modules.instances[].enabled`: Whether to activate this module (also supports `active` for backward compatibility)
- `pipeline.name`: Name of the pipeline configuration
- `pipeline.steps`: Array of pipeline steps (alternative to modules.instances)

## Module Registration and Activation

Modules in SAGE can be registered and activated through several mechanisms:

### Static Registration via Constructor Attributes

```c
__attribute__((constructor))
static void register_cooling_module_factory(void) {
    pipeline_register_module_factory(MODULE_TYPE_COOLING, "cooling_module", cooling_module_factory);
}
```

This approach uses the C compiler's constructor attribute to automatically register module factories at program startup. The factory function returns a pointer to a base_module instance, which is then used to create pipeline steps.

### Dynamic Registration via Module Discovery

The module system can dynamically discover and register modules at runtime based on the `modules.search_paths` configuration. This approach enables plugins to be added without modifying the core code.

### Configuration-Driven Activation

Once modules are registered, they are activated based on the configuration file. The `pipeline_create_with_standard_modules()` function reads the `modules.instances` array from the configuration and activates only the modules that have `enabled` set to true.

## Pipeline Execution Phases

The SAGE pipeline system supports four execution phases:

1. **HALO phase**: Executed once per halo tree, used for calculations at the halo level
2. **GALAXY phase**: Executed for each galaxy, used for per-galaxy calculations
3. **POST phase**: Executed after all galaxies have been processed, used for post-processing
4. **FINAL phase**: Executed at the end of the simulation, used for output preparation and cleanup

Modules declare which phases they support, allowing them to participate in the appropriate execution contexts.

## Core-Physics Separation

SAGE maintains a strict separation between core infrastructure and physics implementations:

### Core Infrastructure

- Has no compile-time or direct runtime knowledge of specific physics implementations
- Uses the generic property system for accessing physics properties
- Provides the infrastructure for module registration, activation, and execution
- Manages memory, I/O, and other low-level operations

### Physics Modules

- Implement specific physics calculations as self-contained modules
- Register themselves with the module system
- Access galaxy properties via the property system
- Communicate with other modules through the callback and event systems

## Property System Integration

The property system is a central component of the module system:

### Property Definition

Properties are defined in `src/properties.yaml`, which serves as the single source of truth for all galaxy properties. The file distinguishes between core properties (essential for infrastructure) and physics properties (specific to physics implementations).

### Property Access

- Core code uses `GALAXY_PROP_*` macros for accessing core properties
- All code uses generic accessors (e.g., `get_float_property()`) for accessing physics properties
- Modules can access both core and physics properties with the appropriate accessors

## Configuration-Driven Pipeline Creation

The configuration-driven pipeline creation system allows for runtime configuration of which modules are active without recompiling the code. This is implemented in `src/core/core_pipeline_registry.c`:

### Process Flow

1. The `pipeline_create_with_standard_modules()` function is called to create a standard pipeline
2. The function checks if a global configuration is available
3. If available, it reads the `modules.instances` array from the configuration
4. For each enabled module in the configuration, it looks for a matching factory and creates the module
5. If no configuration is available or the array is not found, it falls back to using all registered modules

### Benefits

1. **True Runtime Modularity**: Users can define entirely different sets of active physics modules by changing the configuration file.
2. **Complete Core-Physics Decoupling**: Core pipeline creation logic has no compile-time or hardcoded knowledge of which specific physics modules constitute a "standard" run.
3. **Enhanced Flexibility**: Facilitates easier experimentation with different combinations of physics modules.
4. **Clear Separation of Concerns**: Module self-registration (via constructors) makes modules *available*, while the configuration *selects and enables* them for a specific pipeline.

## Module Development Guidelines

When developing new modules for SAGE, follow these guidelines:

### Module Structure

A typical module should include:

1. A factory function that creates and returns a module instance
2. Constructor attribute for automatic registration
3. Implementation of required module interface functions
4. Property access using the property system
5. Declaration of supported phases
6. Proper error handling and resource management

### Example Module Template

```c
#include "core_module_system.h"
#include "core_properties.h"
#include "core_property_utils.h"

// Module instance structure
static struct base_module my_module = {
    .name = "my_module",
    .version = "1.0",
    .author = "Author Name",
    .type = MODULE_TYPE_MY_TYPE,
    .supported_phases = PHASE_GALAXY | PHASE_POST,
    .initialize = my_module_initialize,
    .execute = my_module_execute,
    .cleanup = my_module_cleanup
};

// Factory function
struct base_module* my_module_factory(void) {
    return &my_module;
}

// Auto-registration via constructor
__attribute__((constructor))
static void register_my_module_factory(void) {
    pipeline_register_module_factory(MODULE_TYPE_MY_TYPE, "my_module", my_module_factory);
}

// Initialize function
static int my_module_initialize(struct params *params, void **data_ptr) {
    // Allocate module data
    *data_ptr = calloc(1, sizeof(struct my_module_data));
    if (*data_ptr == NULL) {
        return MODULE_STATUS_ERROR;
    }
    
    // Initialize module data
    struct my_module_data *data = *data_ptr;
    data->some_parameter = config_get_double("modules.instances.my_module.some_parameter", 1.0);
    
    return MODULE_STATUS_SUCCESS;
}

// Execute function
static int my_module_execute(enum pipeline_phase phase, struct halo_data *halo, 
                         struct galaxy_data *galaxy, void *data) {
    struct my_module_data *my_data = data;
    
    // Different behavior based on phase
    if (phase == PHASE_GALAXY) {
        // Per-galaxy calculations
        double mass = get_float_property(galaxy, GALAXY_PROP_StellarMass);
        // Perform calculations and update properties
        set_float_property(galaxy, GALAXY_PROP_SomeProperty, calculated_value);
    }
    else if (phase == PHASE_POST) {
        // Post-processing calculations
    }
    
    return MODULE_STATUS_SUCCESS;
}

// Cleanup function
static int my_module_cleanup(void *data) {
    if (data != NULL) {
        free(data);
    }
    return MODULE_STATUS_SUCCESS;
}
```

### Configuration Example

To enable your module in the configuration:

```json
{
    "modules": {
        "instances": [
            {
                "name": "my_module",
                "enabled": true,
                "some_parameter": 2.5
            }
        ]
    }
}
```

## Conclusion

The SAGE module system provides a flexible, extensible framework for implementing, registering, and executing physics modules while maintaining a strict separation between core infrastructure and physics implementations. The configuration-driven pipeline creation system allows for runtime configuration of which modules are active without recompiling the code, enabling users to experiment with different combinations of physics modules and facilitating collaborative development.

The recent implementation of configuration-driven module activation in pipeline creation represents the final step in fully decoupling the core infrastructure from specific physics implementations, completing the core-physics separation requirements (FR-1.1 and FR-1.2) by making module activation fully configurable at runtime.
