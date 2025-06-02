# SAGE Module System and Configuration

## Overview

The SAGE module system provides a flexible, extensible framework for implementing, registering, and executing physics modules. The system maintains strict separation between core infrastructure and physics implementations, allowing for runtime configuration of active modules without recompiling. This document provides both high-level architecture and detailed implementation specifics to aid debugging and development.

## Module System Architecture in Detail

### Key Components and Their Interactions

The module system consists of several interconnected components:

```
┌───────────────────────┐       ┌────────────────────────┐
│  Module Registry      │◄──────►  Module System         │
│ (core_module_system.c)│       │ (module_register,      │
└───────────┬───────────┘       │  module_initialize)    │
            │                   └────────────┬───────────┘
            │                                │
            ▼                                ▼
┌───────────────────────┐       ┌────────────────────────┐
│  Pipeline Registry    │◄──────►  Pipeline System       │
│ (core_pipeline_        │       │ (pipeline_execute,     │
│  registry.c)          │       │  pipeline_execute_phase)│
└───────────┬───────────┘       └────────────┬───────────┘
            │                                │
            ▼                                ▼
┌───────────────────────┐       ┌────────────────────────┐
│  Module Factories     │       │  Pipeline Context      │
│ (module_factory_fn    │───────► (execution state,      │
│  functions)           │       │  galaxy data access)   │
└───────────────────────┘       └────────────────────────┘
```

## Module Infrastructure Components

The SAGE module system includes several sophisticated infrastructure components that provide comprehensive support for module development, execution monitoring, and parameter management:

### Parameter Management System

The module system uses a **dual-layer parameter approach** that combines JSON configuration with programmatic parameter registration:

**1. JSON Configuration Layer:**
- Controls pipeline-level module activation and configuration
- Defines which modules are enabled in a simulation run
- Provides high-level simulation parameters

**2. Programmatic Parameter Layer:**
- Each module maintains its own dedicated parameter registry (`module_parameter_registry_t`)
- Supports typed parameters (int, float, double, bool, string) with bounds checking
- Enables runtime parameter modification and validation
- Provides module-specific parameter namespacing

```c
// Example of module parameter registration
module_parameter_t cooling_efficiency = {
    .name = "cooling_efficiency",
    .type = PARAM_TYPE_DOUBLE,
    .value.double_val = 1.0,
    .limits.double_range = {0.1, 10.0},
    .has_limits = true,
    .description = "Cooling efficiency multiplier"
};

// Register with module system
module_register_parameter_with_module(module_id, &cooling_efficiency);
```

**Key Functions:**
- `module_parameter_registry_init()` - Initialize parameter registry for a module
- `module_register_parameter()` - Register a parameter with type safety and bounds
- `module_get_parameter()` / `module_set_parameter()` - Type-safe parameter access

### Error Context System

Beyond simple return codes, SAGE provides an **enhanced error tracking system** that maintains detailed error context and history:

**Features:**
- **Error History:** Maintains history of errors with timestamps and context
- **Detailed Context:** Captures function names, file locations, and error descriptions
- **Integration:** Works alongside the standard logging system for comprehensive debugging
- **Module-Specific:** Each module can maintain its own error context

```c
// Example of enhanced error reporting
module_record_error(module_id, MODULE_STATUS_INVALID_INPUT, 
                   "Invalid galaxy mass in cooling calculation", 
                   __func__, __FILE__, __LINE__);
```

**Key Functions:**
- `module_error_context_init()` - Initialize error tracking for a module
- `module_record_error()` - Record detailed error with context
- `module_get_error_history()` - Retrieve error history for diagnostics

### Callback Infrastructure

The callback system provides **controlled inter-module communication** and **execution monitoring**:

**Current Usage:**
- **Call Stack Tracking:** Monitors module execution flow for diagnostics
- **Execution Context:** Tracks which module is currently executing and why
- **Future Inter-Module Communication:** Infrastructure ready for physics modules that need to communicate

**Architecture Benefits:**
- **Controlled Communication:** Prevents ad-hoc module dependencies
- **Circular Dependency Detection:** Prevents infinite call loops
- **Execution Diagnostics:** Provides detailed execution traces for debugging

```c
// Example: Pipeline executor using call stack tracking
int status = module_call_stack_push(caller_id, callee_id, function_name, context);
// Execute module function
int result = func(module_data, context);
module_call_stack_pop();
```

**Key Functions:**
- `module_callback_system_initialize()` - Initialize callback infrastructure
- `module_call_stack_push()` / `module_call_stack_pop()` - Track execution flow
- `module_invoke()` - Infrastructure for controlled inter-module calls (future use)

**Integration Points:**
- **Pipeline Executor:** Uses call stack tracking in `physics_pipeline_executor.c`
- **Module Lifecycle:** Callback-related fields managed during module registration/cleanup
- **Base Module Interface:** Callback structures embedded in `struct base_module`

### Infrastructure vs. Legacy Distinction

**Active Infrastructure Components:**
- ✅ `core_module_callback` - Call stack tracking and inter-module communication framework
- ✅ `core_module_parameter` - Module-specific parameter management
- ✅ `core_module_error` - Enhanced error context and tracking

**Removed Legacy Components:**
- ❌ `core_module_debug` - Specialized debugging (replaced by standard logging)
- ❌ `core_module_validation` - Module validation (minimal usage in current system)

This infrastructure provides a **robust foundation** for both current placeholder modules and future physics modules requiring sophisticated parameter management, error tracking, and inter-module communication.

### Module Registry

The module registry (`core_module_system.c/h`) is the central component that tracks all registered modules and their state. Key elements:

- `struct module_registry` - Holds arrays of modules, their initialization state, and search paths
- `global_module_registry` - Single instance of the registry used throughout the system
- `module_register()` - Adds a module to the registry, assigning a unique `module_id`
- Maximum of `MAX_MODULES` (64) modules can be registered
- Module initialization state (initialized/active) is tracked separately from registration

```c
// Actual implementation structure
struct module_registry {
    int num_modules;
    
    struct {
        struct base_module *module;
        void *module_data;
        bool initialized;
        bool active;
        bool dynamic;
        module_library_handle_t handle;
        char path[MAX_MODULE_PATH];
        module_parameter_registry_t *parameter_registry;
    } modules[MAX_MODULES];
    
    // Quick lookup for modules by type
    struct {
        enum module_type type;
        int module_index;
    } active_modules[MODULE_TYPE_MAX];
    int num_active_types;
    
    // Search paths configuration
    char module_paths[MAX_MODULE_PATHS][MAX_MODULE_PATH];
    int num_module_paths;
    bool discovery_enabled;
};
```

### Pipeline System

The pipeline system (`core_pipeline_system.c/h`) manages the execution flow of modules. Key elements:

- `struct module_pipeline` - Contains ordered steps defining module execution sequence
- `pipeline_execute()` - Runs all steps in the pipeline
- `pipeline_execute_phase()` - Executes only steps that support a specific phase
- `struct pipeline_context` - Holds runtime state during execution (galaxies, params, etc.)
- Supports four execution phases: HALO, GALAXY, POST, FINAL
- Maximum of `MAX_PIPELINE_STEPS` (32) steps in a single pipeline

```c
// Pipeline structure implementation
struct module_pipeline {
    struct pipeline_step steps[MAX_PIPELINE_STEPS];  // Pipeline steps
    int num_steps;                        // Number of steps in the pipeline
    char name[MAX_MODULE_NAME];           // Pipeline name
    bool initialized;                     // Whether pipeline is initialized
    int current_step_index;               // Current execution step (during execution)
};

// Pipeline execution phases (defined as bit flags)
#define PIPELINE_PHASE_HALO   (1U << 0)  // Executed once per halo
#define PIPELINE_PHASE_GALAXY (1U << 1)  // Executed for each galaxy
#define PIPELINE_PHASE_POST   (1U << 2)  // Post-processing after galaxy calculations
#define PIPELINE_PHASE_FINAL  (1U << 3)  // Final processing for output
```

### Pipeline Registry

The pipeline registry (`core_pipeline_registry.c/h`) connects module factories with the pipeline system. Key elements:

- `module_factories[]` - Array of factory function entries
- `pipeline_register_module_factory()` - Registers a factory function by name
- `pipeline_create_with_standard_modules()` - Creates and populates a pipeline based on configuration
- Maximum of `MAX_MODULE_FACTORIES` (32) factories can be registered

```c
// Factory entry implementation
struct module_factory_entry {
    enum module_type type;
    char name[MAX_MODULE_NAME];
    module_factory_fn factory;
} module_factories[MAX_MODULE_FACTORIES];
int num_factories = 0;

// Factory function type
typedef struct base_module* (*module_factory_fn)(void);
```

### Base Module Interface

The `struct base_module` defines the common interface for all physics modules:

```c
struct base_module {
    // Metadata
    char name[MAX_MODULE_NAME];           // Module name
    char version[MAX_MODULE_VERSION];     // Module version string
    char author[MAX_MODULE_AUTHOR];       // Module author(s)
    int module_id;                        // Unique runtime ID
    enum module_type type;                // Type of physics module
    
    // Lifecycle functions
    int (*initialize)(struct params *params, void **module_data);
    int (*cleanup)(void *module_data);
    
    // Configuration function
    int (*configure)(void *module_data, const char *config_name, const char *config_value);
    
    // Phase execution functions
    int (*execute_halo_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_galaxy_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_post_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_final_phase)(void *module_data, struct pipeline_context *context);
    
    // Error handling
    int last_error;
    char error_message[MAX_ERROR_MESSAGE];
    
    // Module manifest and function registry
    struct module_manifest *manifest;
    module_function_registry_t *function_registry;
    
    // Dependencies and phases
    module_dependency_t *dependencies;
    int num_dependencies;
    uint32_t phases;                      // Bitmap of supported phases
};
```

## Registration and Lifecycle Process

### Static Registration via Constructor

The most common module registration approach uses C constructor attributes:

```c
/* Register the module at startup */
static void __attribute__((constructor)) register_module(void) {
    module_register(&placeholder_output_module);
}
```

This ensures modules are registered automatically when the program starts, without requiring explicit calls. The constructor attribute is a GCC feature that runs the function before `main()`.

### Complete Module Lifecycle

A module's lifecycle follows this sequence:

1. **Registration**: Module is registered with the module system
   ```c
   int module_register(struct base_module *module)
   ```

2. **Factory Registration**: Module's factory function is registered with the pipeline system
   ```c
   int pipeline_register_module_factory(enum module_type type, const char *name, module_factory_fn factory)
   ```

3. **Pipeline Creation**: Module instances are created by factories based on configuration
   ```c
   struct module_pipeline *pipeline_create_with_standard_modules(void)
   ```

4. **Initialization**: Module's initialize function is called with parameters
   ```c
   int module_initialize(int module_id, struct params *params)
   ```

5. **Execution**: Module's phase-specific functions are called during pipeline execution
   ```c
   int pipeline_execute_phase(struct module_pipeline *pipeline, struct pipeline_context *context, enum pipeline_execution_phase phase)
   ```

6. **Cleanup**: Module's cleanup function is called before termination
   ```c
   int module_cleanup(int module_id)
   ```

## Configuration-Driven Pipeline Creation

### Configuration File Format

SAGE uses JSON configuration files (typically in `input/config.json`) to determine which modules to activate. The structure is:

```json
{
    "modules": {
        "discovery_enabled": true,
        "search_paths": [
            "./src/physics"
        ],
        "instances": [
            {
                "name": "cooling_module",  // Must match factory registration name
                "enabled": true,           // Whether to include in pipeline
                "some_parameter": 1.5      // Module-specific parameters
            },
            {
                "name": "infall_module",
                "enabled": true
            }
        ]
    }
}
```

### Pipeline Creation Process

The `pipeline_create_with_standard_modules()` function in `core_pipeline_registry.c` handles the configuration-driven pipeline creation:

1. Checks if global configuration is available
   ```c
   if (global_config == NULL || global_config->root == NULL) {
       // Fallback: Use all registered modules
       add_all_registered_modules_to_pipeline(pipeline);
       return pipeline;
   }
   ```

2. Looks for the "modules.instances" array in configuration
   ```c
   const struct config_value *modules_instances_val = config_get_value("modules.instances");
   ```

3. For each enabled module in configuration, creates and adds it to the pipeline
   ```c
   // Get module name and enabled status
   const char *config_module_name = get_string_from_object(module_config_val->u.object, "name", NULL);
   bool enabled = get_boolean_from_object(module_config_val->u.object, "enabled", false);
   
   // Find matching factory and create module
   struct base_module *module = module_factories[j].factory();
   
   // Add to pipeline
   pipeline_add_step(pipeline, module->type, module->name, module->name, true, false);
   ```

4. If no valid configuration is found, falls back to using all registered modules

### Module Deduplication

The pipeline creation process tracks already processed modules to prevent duplicates:

```c
// Track modules we've already processed to avoid duplicates
int processed_module_ids[MAX_MODULES] = {0};
int num_processed = 0;

// Check if we've already processed this module
bool already_processed = false;
for (int k = 0; k < num_processed; k++) {
    if (processed_module_ids[k] == module->module_id) {
        already_processed = true;
        break;
    }
}
```

This is important when multiple configuration entries could create the same module.

## Placeholder Module Implementation

The placeholder modules are minimal implementations that fulfill the module interface requirements. They're used in the physics-free model for testing the core infrastructure. Example:

```c
struct base_module placeholder_output_module = {
    .name = "placeholder_output_module",
    .type = MODULE_TYPE_MISC,
    .version = "1.0",
    .author = "SAGE Team",
    .initialize = placeholder_output_init,
    .cleanup = placeholder_output_cleanup,
    .execute_final_phase = placeholder_output_execute_final_phase,
    .phases = PIPELINE_PHASE_FINAL
};
```

The implementation declares which pipeline phases it participates in through the `.phases` bitmask. This module only participates in the FINAL phase.

## Common Debugging Issues

### Module Not Found in Pipeline

If a module is missing from the pipeline:

1. Check if the module's factory is registered
   ```c
   // Debug output during pipeline creation
   LOG_DEBUG("Found matching registered factory for '%s'", config_module_name);
   ```

2. Verify the module name in configuration matches the factory registration
   ```c
   // In config.json: "name": "cooling_module"
   // In registration: pipeline_register_module_factory(MODULE_TYPE_COOLING, "cooling_module", cooling_module_factory);
   ```

3. Ensure the module's constructor is being called
   ```c
   // Add debug output to constructor
   static void __attribute__((constructor)) register_module(void) {
       printf("Registering module: %s\n", my_module.name);
       module_register(&my_module);
   }
   ```

### Module Not Executing in Expected Phase

If a module is not being executed in the expected pipeline phase:

1. Check the module's `.phases` bitmask is correctly set
   ```c
   .phases = PIPELINE_PHASE_GALAXY | PIPELINE_PHASE_POST
   ```

2. Verify the phase-specific function pointer is set
   ```c
   .execute_galaxy_phase = my_module_execute_galaxy_phase
   ```

3. Check the pipeline is executing the correct phase
   ```c
   // Debug output during phase execution
   LOG_DEBUG("Executing phase %d", (int)phase);
   ```

### Module Factory Returns NULL

If a module factory is failing to create a module:

1. Check for memory allocation failures
   ```c
   // Factory function should handle allocation failures
   if (my_data == NULL) {
       LOG_ERROR("Failed to allocate memory for module data");
       return NULL;
   }
   ```

2. Verify static module instance is properly initialized
   ```c
   // All required fields should be set
   struct base_module my_module = {
       .name = "my_module",  // Required
       .type = MODULE_TYPE_MISC,  // Required
       // Other fields...
   };
   ```

## Module Development Guidelines

### Essential Module Components

Every module must have:

1. **Base Module Structure**: Defines the module interface and metadata
   ```c
   struct base_module my_module = {
       .name = "my_module",
       .type = MODULE_TYPE_COOLING,
       .version = "1.0",
       .author = "Developer Name",
       .initialize = my_module_init,
       .cleanup = my_module_cleanup,
       .phases = PIPELINE_PHASE_GALAXY,
       .execute_galaxy_phase = my_module_execute_galaxy
   };
   ```

2. **Module Data Structure**: Holds module-specific state
   ```c
   struct my_module_data {
       double parameter1;
       int parameter2;
       bool initialized;
   };
   ```

3. **Lifecycle Functions**: Initialize and cleanup
   ```c
   static int my_module_init(struct params *params, void **data_ptr) {
       struct my_module_data *data = calloc(1, sizeof(struct my_module_data));
       // Initialize data
       *data_ptr = data;
       return MODULE_STATUS_SUCCESS;
   }
   
   static int my_module_cleanup(void *data) {
       free(data);
       return MODULE_STATUS_SUCCESS;
   }
   ```

4. **Phase Execution Functions**: Implementation for each supported phase
   ```c
   static int my_module_execute_galaxy(void *data, struct pipeline_context *context) {
       struct my_module_data *my_data = (struct my_module_data *)data;
       // Process each galaxy
       for (int i = 0; i < context->ngal; i++) {
           struct GALAXY *galaxy = &context->galaxies[i];
           // Perform calculations
       }
       return MODULE_STATUS_SUCCESS;
   }
   ```

5. **Registration**: Constructor function for automatic registration
   ```c
   static void __attribute__((constructor)) register_my_module(void) {
       module_register(&my_module);
   }
   ```

6. **Factory Function**: Creates and returns the module
   ```c
   struct base_module* my_module_factory(void) {
       return &my_module;
   }
   
   static void __attribute__((constructor)) register_my_module_factory(void) {
       pipeline_register_module_factory(MODULE_TYPE_COOLING, "my_module", my_module_factory);
   }
   ```

### Input Validation Best Practices

**All modules must implement robust input validation** to ensure system stability and provide clear error feedback. This is critical defensive programming that prevents crashes and aids debugging.

#### Required Validation Patterns

1. **Module Initialization Functions** must validate the `data_ptr` parameter:
   ```c
   static int my_module_init(struct params *params, void **data_ptr) {
       /* Validate critical input parameters */
       if (data_ptr == NULL) {
           LOG_ERROR("Invalid NULL data pointer passed to %s initialization", MODULE_NAME);
           return MODULE_STATUS_INVALID_ARGS;
       }
       
       /* Note: params can be NULL if the module doesn't require parameters */
       (void)params;  // Mark as intentionally unused if not needed
       
       /* Allocate and initialize module data */
       struct my_module_data *data = calloc(1, sizeof(struct my_module_data));
       if (data == NULL) {
           LOG_ERROR("Failed to allocate memory for %s module data", MODULE_NAME);
           return MODULE_STATUS_OUT_OF_MEMORY;
       }
       
       /* Initialize module state */
       data->initialized = true;
       *data_ptr = data;
       
       return MODULE_STATUS_SUCCESS;
   }
   ```

2. **Phase Execution Functions** must validate context and data:
   ```c
   static int my_module_execute_galaxy(void *data, struct pipeline_context *context) {
       /* Validate module data */
       if (data == NULL) {
           LOG_ERROR("%s: NULL module data in galaxy phase execution", MODULE_NAME);
           return MODULE_STATUS_INVALID_ARGS;
       }
       
       /* Validate pipeline context */
       if (context == NULL) {
           LOG_ERROR("%s: NULL pipeline context in galaxy phase execution", MODULE_NAME);
           return MODULE_STATUS_INVALID_ARGS;
       }
       
       /* Validate galaxy array */
       if (context->galaxies == NULL && context->ngal > 0) {
           LOG_ERROR("%s: NULL galaxy array with ngal=%d", MODULE_NAME, context->ngal);
           return MODULE_STATUS_INVALID_ARGS;
       }
       
       struct my_module_data *module_data = (struct my_module_data *)data;
       
       /* Process galaxies with additional validation */
       for (int i = 0; i < context->ngal; i++) {
           struct GALAXY *galaxy = &context->galaxies[i];
           if (galaxy == NULL) {
               LOG_ERROR("%s: NULL galaxy at index %d", MODULE_NAME, i);
               continue;  // Skip this galaxy but continue processing
           }
           
           /* Perform module-specific galaxy processing */
           // ...
       }
       
       return MODULE_STATUS_SUCCESS;
   }
   ```

3. **Cleanup Functions** must handle NULL data gracefully:
   ```c
   static int my_module_cleanup(void *data) {
       /* Cleanup must handle NULL data gracefully */
       if (data == NULL) {
           LOG_DEBUG("%s: Cleanup called with NULL data (acceptable)", MODULE_NAME);
           return MODULE_STATUS_SUCCESS;
       }
       
       struct my_module_data *module_data = (struct my_module_data *)data;
       
       /* Perform any necessary cleanup of module state */
       // Free any allocated resources...
       
       /* Free the module data structure */
       free(module_data);
       
       return MODULE_STATUS_SUCCESS;
   }
   ```

#### Validation Macros

Consider defining validation macros for consistency:
```c
#define VALIDATE_NOT_NULL(param, name) \
    do { \
        if ((param) == NULL) { \
            LOG_ERROR("%s: " name " parameter is NULL", MODULE_NAME); \
            return MODULE_STATUS_INVALID_ARGS; \
        } \
    } while(0)

#define VALIDATE_POSITIVE(param, name) \
    do { \
        if ((param) <= 0) { \
            LOG_ERROR("%s: " name " parameter must be positive, got %d", MODULE_NAME, (param)); \
            return MODULE_STATUS_INVALID_ARGS; \
        } \
    } while(0)
```

#### Callback Function Validation

For modules that register callback functions:
```c
static int my_callback_function(void *module_data, void *args_ptr, struct pipeline_context *context) {
    /* Validate all callback parameters */
    VALIDATE_NOT_NULL(module_data, "module_data");
    VALIDATE_NOT_NULL(args_ptr, "args_ptr");
    VALIDATE_NOT_NULL(context, "pipeline_context");
    
    /* Cast and validate specific argument types */
    my_args_t *args = (my_args_t *)args_ptr;
    VALIDATE_NOT_NULL(args->event, "event");
    
    /* Perform callback logic */
    return MODULE_STATUS_SUCCESS;
}
```

#### Best Practices Summary

- **Always validate NULL pointers** before dereferencing
- **Use descriptive error messages** that include the module name and parameter name
- **Return appropriate MODULE_STATUS_* codes** for different error conditions
- **Handle edge cases gracefully** (e.g., empty galaxy arrays, zero counts)
- **Log errors at appropriate levels** (ERROR for serious issues, DEBUG for acceptable edge cases)
- **Continue processing when possible** rather than failing entire operations for single item errors
- **Document validation assumptions** in function comments

### Property Access Guidelines

Modules should follow these property access patterns:

1. **For Core Properties**: Direct access using `GALAXY_PROP_*` macros
   ```c
   int type = GALAXY_PROP_Type(galaxy);
   float mass = GALAXY_PROP_StellarMass(galaxy);
   ```

2. **For Physics Properties**: Use generic accessors from `core_property_utils.h`
   ```c
   // Get property ID (cached for performance)
   property_id_t prop_id = get_cached_property_id("ColdGas");
   
   // Check if property exists
   if (has_property(galaxy, prop_id)) {
       // Access property with appropriate type
       float cold_gas = get_float_property(galaxy, prop_id, 0.0f);
   }
   ```

### Error Handling

The SAGE module system provides **multiple layers of error handling** for comprehensive debugging and diagnostics:

#### 1. **Return Status Codes** 
Use standardized `MODULE_STATUS_*` constants for immediate error indication:
```c
if (data == NULL) {
    return MODULE_STATUS_ERROR;
}
return MODULE_STATUS_SUCCESS;
```

#### 2. **Standard Logging**
Use the logging system for immediate context and debugging:
```c
LOG_ERROR("Failed to allocate memory for module data");
LOG_DEBUG("Module parameter validation passed");
```

#### 3. **Enhanced Error Context System**
Record detailed error information with the module error context system:
```c
// Record detailed error with context
module_record_error(module_id, MODULE_STATUS_INVALID_INPUT,
                   "Galaxy mass out of valid range: %.2e Msun", 
                   invalid_mass);

// Set module error state with enhanced context
module_set_error_ex(module, MODULE_STATUS_ERROR, 
                   __func__, __FILE__, __LINE__,
                   "Failed to initialize cooling tables");
```

#### 4. **Parameter System Integration**
Leverage the parameter system for validation and bounds checking:
```c
// Parameter system automatically validates bounds
int status = module_set_parameter_double(module_id, "cooling_efficiency", 2.5);
if (status != MODULE_PARAM_SUCCESS) {
    LOG_ERROR("Parameter out of bounds: cooling_efficiency");
    return MODULE_STATUS_INVALID_ARGS;
}
```

#### **Best Practices for Module Error Handling:**
- **Layer appropriately**: Use return codes for immediate control flow, logging for debugging, error context for detailed diagnostics
- **Be specific**: Include relevant values and context in error messages
- **Use parameter validation**: Let the parameter system handle bounds checking automatically
- **Check error history**: Use `module_get_error_history()` for debugging complex issues

## Conclusion

The SAGE module system provides a **sophisticated, multi-layered framework** for implementing physics components while maintaining strict separation between core infrastructure and physics implementations. The system includes:

**Key Infrastructure Components:**
- **Dual-layer parameter management** (JSON configuration + programmatic parameters)
- **Enhanced error tracking** with detailed context and history
- **Execution monitoring** through callback infrastructure and call stack tracking
- **Configuration-driven activation** allowing runtime module selection

**Development Benefits:**
- **Runtime modularity**: Modules can be enabled, disabled, or replaced without recompilation
- **Comprehensive debugging**: Multiple error handling layers provide detailed diagnostics
- **Future-ready architecture**: Infrastructure supports inter-module communication for complex physics
- **Type-safe parameter management**: Bounds checking and validation built into the parameter system

**Architecture Maturity:**
The current system represents a **mature, sophisticated modular architecture** that goes far beyond simple plugin loading. Understanding the infrastructure components—particularly the parameter management, error context system, and callback infrastructure—is essential for effective module development and system debugging.

The placeholder modules demonstrate how to work within this infrastructure while the system prepares for migration to full physics modules that will leverage the complete capabilities of the framework.
