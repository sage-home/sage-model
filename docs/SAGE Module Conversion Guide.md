# SAGE Module Conversion Guide

This guide provides instructions for converting legacy SAGE physics modules to use the new module callback system. Following these guidelines will ensure consistent implementation and proper integration with the refactored pipeline execution framework.

## Contents
1. [Module Structure Changes](#module-structure-changes)
2. [Function Registration Process](#function-registration-process)
3. [Inter-Module Communication](#inter-module-communication)
4. [Error Handling Patterns](#error-handling-patterns)
5. [Testing Converted Modules](#testing-converted-modules)
6. [Complete Conversion Example](#complete-conversion-example)

## Module Structure Changes

### Before: Legacy Module Structure
```c
// Legacy module typically used global functions
void infall_function(int gal_idx, double dt) {
    // Implementation
}

// Legacy init just set up internal state
void init_infall(void) {
    InfallVars.param1 = whatever;
}

// Legacy modules often used direct function calls to other modules
void process_galaxy(int gal_idx) {
    // Direct call to another module's function
    double cooling_rate = get_cooling_rate(gal_idx);
}
```

### After: New Module Structure
```c
// New module struct extending base_module
typedef struct {
    base_module base;         // Must be first member
    double param1;            // Module-specific parameters
    double param2;
    // Other module-specific data
} infall_module_t;

// Module initialization function registers with module system
int standard_infall_init(infall_module_t *module) {
    // Register the module with the system
    if (module_register((base_module*)module, "standard_infall", MODULE_TYPE_INFALL) != SUCCESS) {
        return ERROR_MODULE_REGISTRATION_FAILED;
    }
    
    // Register callable functions
    module_register_function((base_module*)module, "calculate_infall_rate", standard_infall_calculate);
    module_register_function((base_module*)module, "apply_infall", standard_infall_apply);
    
    // Declare dependencies
    module_declare_simple_dependency((base_module*)module, "cooling", 
                                    "get_cooling_rate", FALSE);  // FALSE = optional
    
    // Initialize module-specific data
    module->param1 = param_get_double("InfallModel", "Param1", DEFAULT_PARAM1);
    module->param2 = param_get_double("InfallModel", "Param2", DEFAULT_PARAM2);
    
    return SUCCESS;
}

// Module cleanup function
void standard_infall_cleanup(infall_module_t *module) {
    // Free any allocated resources
    // ...
}

// Phase execution functions called by pipeline
int standard_infall_execute_galaxy(infall_module_t *module, galaxy_t *galaxy, 
                                 evolution_context_t *context) {
    // Implementation using module_invoke for inter-module calls
}
```

## Function Registration Process

Each function that might be called by other modules must be registered:

```c
// During module initialization:
module_register_function((base_module*)module, "function_name", function_ptr);
```

Function signature must match `module_function_t` type:

```c
typedef int (*module_function_t)(void *output, ...);
```

Example function implementation:

```c
int standard_infall_calculate(void *output, int gal_idx, double dt) {
    double *infall_rate = (double*)output;
    
    // Calculate infall rate
    *infall_rate = calculate_internal_infall_rate(gal_idx, dt);
    
    return MODULE_INVOKE_SUCCESS;
}
```

## Inter-Module Communication

### Before: Direct Function Calls
```c
// Legacy direct function calls
double cooling_rate = get_cooling_rate(gal_idx, redshift);
process_with_cooling_rate(gal, cooling_rate);
```

### After: Module Invoke Pattern
```c
// Step 1: Declare variables to receive output
double cooling_rate;

// Step 2: Use module_invoke to call function from another module
int result = module_invoke("cooling", "get_cooling_rate", 
                          &cooling_rate, gal->index, context->current_redshift);

// Step 3: Check for errors and handle appropriately
if (result != MODULE_INVOKE_SUCCESS) {
    // Option 1: Use fallback value for non-critical functionality
    LOG_WARNING("Failed to get cooling rate, using default value");
    cooling_rate = DEFAULT_COOLING_RATE;
    
    // Option 2: For critical errors, propagate the error
    if (is_critical_error(result)) {
        return result;  // Propagate error code
    }
}

// Step 4: Continue with calculation using cooling_rate
process_with_cooling_rate(gal, cooling_rate);
```

## Error Handling Patterns

### Error Propagation
```c
int standard_infall_execute_galaxy(infall_module_t *module, galaxy_t *galaxy, 
                                 evolution_context_t *context) {
    // Call another module function
    double cooling_rate;
    int result = module_invoke("cooling", "get_cooling_rate", 
                              &cooling_rate, galaxy->index, context->current_redshift);
    
    // Critical error handling - propagate error
    if (result != MODULE_INVOKE_SUCCESS) {
        LOG_ERROR("Failed to get cooling rate for galaxy %d", galaxy->index);
        return result;  // Propagate error code upstream
    }
    
    // Continue execution...
    return SUCCESS;
}
```

### Non-Critical Error Recovery
```c
int standard_infall_execute_galaxy(infall_module_t *module, galaxy_t *galaxy, 
                                 evolution_context_t *context) {
    // Call another module function
    double cooling_rate;
    int result = module_invoke("cooling", "get_cooling_rate", 
                              &cooling_rate, galaxy->index, context->current_redshift);
    
    // Non-critical error handling - use fallback
    if (result != MODULE_INVOKE_SUCCESS) {
        LOG_WARNING("Using fallback cooling rate for galaxy %d", galaxy->index);
        cooling_rate = calculate_fallback_cooling_rate(galaxy);
        // Continue execution with fallback value
    }
    
    // Continue execution...
    return SUCCESS;
}
```

### Error Context Information
```c
// Getting detailed error information
if (result != MODULE_INVOKE_SUCCESS) {
    module_error_context_t *error = module_get_last_error();
    
    LOG_ERROR("Error in module '%s' function '%s': %s (code %d)", 
             error->module_name, error->function_name, 
             error->error_message, error->error_code);
             
    // Get call stack trace for debugging
    char stack_trace[1024];
    module_call_stack_get_trace_with_errors(stack_trace, sizeof(stack_trace));
    LOG_DEBUG("Call stack: %s", stack_trace);
}
```

## Testing Converted Modules

### Unit Testing
Create unit tests for each module function:

```c
void test_standard_infall_calculate(void) {
    // Setup test environment
    setup_test_modules();
    
    // Call the function directly for unit testing
    double result;
    int status = standard_infall_calculate(&result, test_galaxy.index, 0.1);
    
    // Verify expected results
    TEST_ASSERT_EQUAL(MODULE_INVOKE_SUCCESS, status);
    TEST_ASSERT_FLOAT_WITHIN(0.001, expected_rate, result);
}
```

### Integration Testing
Test interactions with other modules:

```c
void test_standard_infall_module_integration(void) {
    // Setup test environment with multiple modules
    setup_test_modules();
    register_test_cooling_module();
    
    // Execute the infall module's galaxy phase function
    int status = standard_infall_execute_galaxy(infall_module, &test_galaxy, &test_context);
    
    // Verify expected results and interactions
    TEST_ASSERT_EQUAL(SUCCESS, status);
    TEST_ASSERT_TRUE(cooling_function_was_called);
    TEST_ASSERT_FLOAT_WITHIN(0.001, expected_mass_change, test_galaxy.disk_mass - initial_disk_mass);
}
```

### Error Condition Testing
Test error handling and recovery mechanisms:

```c
void test_standard_infall_error_handling(void) {
    // Setup test with a cooling module that will fail
    setup_test_modules();
    register_failing_cooling_module();
    
    // Execute the infall module's galaxy phase function
    int status = standard_infall_execute_galaxy(infall_module, &test_galaxy, &test_context);
    
    // Verify appropriate error handling
    if (should_propagate_error) {
        TEST_ASSERT_NOT_EQUAL(SUCCESS, status);
    } else {
        TEST_ASSERT_EQUAL(SUCCESS, status);
        TEST_ASSERT_FLOAT_WITHIN(0.001, fallback_value, test_galaxy.result_value);
    }
}
```

## Complete Conversion Example

Here's a complete example of converting the Standard Infall module:

### Before: Legacy Infall Module
```c
// Global variables
struct infall_params {
    double efficiency;
    double reheating_factor;
} InfallVars;

// Initialization
void init_infall(void) {
    InfallVars.efficiency = 0.3;
    InfallVars.reheating_factor = 2.0;
}

// Direct execution function
void calculate_infall(int p, double dt) {
    // Get galaxy pointer
    struct GALAXY *gal = &Gal[p];
    
    // Direct call to cooling module
    double cooling_rate = get_cooling_rate(p, ZZ[Halo[gal->HaloNr].SnapNum], dt);
    
    // Calculate infall
    double infall_rate = InfallVars.efficiency * cooling_rate;
    
    // Apply infall directly
    gal->DiskMass += infall_rate * dt;
}
```

### After: Converted Infall Module

```c
// standard_infall_module.h
#ifndef STANDARD_INFALL_MODULE_H
#define STANDARD_INFALL_MODULE_H

#include "core/core_module_system.h"
#include "physics/physics_modules.h"

typedef struct {
    base_module base;
    double efficiency;
    double reheating_factor;
} standard_infall_module_t;

// Module lifecycle functions
int standard_infall_init(standard_infall_module_t *module);
void standard_infall_cleanup(standard_infall_module_t *module);

// Module phase functions
int standard_infall_execute_halo(standard_infall_module_t *module, halo_t *halo, evolution_context_t *context);
int standard_infall_execute_galaxy(standard_infall_module_t *module, galaxy_t *galaxy, evolution_context_t *context);

// Module-specific functions
int standard_infall_calculate(void *output, int galaxy_index, double dt);
int standard_infall_apply(void *output, int galaxy_index, double infall_rate, double dt);

#endif /* STANDARD_INFALL_MODULE_H */
```

```c
// standard_infall_module.c
#include "standard_infall_module.h"
#include "core/core_module_callback.h"
#include "core/core_allvars.h"
#include "core/core_logging.h"

// Module initialization
int standard_infall_init(standard_infall_module_t *module) {
    // Register module with system
    int status = module_register((base_module*)module, "standard_infall", MODULE_TYPE_INFALL);
    if (status != SUCCESS) {
        LOG_ERROR("Failed to register standard infall module");
        return status;
    }
    
    // Register callable functions
    module_register_function((base_module*)module, "calculate_infall_rate", standard_infall_calculate);
    module_register_function((base_module*)module, "apply_infall", standard_infall_apply);
    
    // Declare dependencies
    module_declare_simple_dependency((base_module*)module, "cooling", "get_cooling_rate", FALSE);
    
    // Initialize parameters from config
    module->efficiency = param_get_double("InfallModel", "Efficiency", 0.3);
    module->reheating_factor = param_get_double("InfallModel", "ReheatingFactor", 2.0);
    
    LOG_INFO("Standard infall module initialized with efficiency=%.2f, reheating_factor=%.2f",
             module->efficiency, module->reheating_factor);
             
    return SUCCESS;
}

// Module cleanup
void standard_infall_cleanup(standard_infall_module_t *module) {
    // No specific resources to clean up
    LOG_INFO("Standard infall module cleaned up");
}

// Calculate infall rate function (can be called by other modules)
int standard_infall_calculate(void *output, int galaxy_index, double dt) {
    double *infall_rate = (double*)output;
    galaxy_t *galaxy = &Gal[galaxy_index];
    
    // Get cooling rate from cooling module
    double cooling_rate;
    int result = module_invoke("cooling", "get_cooling_rate", 
                              &cooling_rate, galaxy_index, ZZ[Halo[galaxy->HaloNr].SnapNum], dt);
    
    // Handle potential errors from cooling module
    if (result != MODULE_INVOKE_SUCCESS) {
        LOG_WARNING("Failed to get cooling rate for galaxy %d, using fallback", galaxy_index);
        // Use simple fallback based on halo properties
        cooling_rate = 0.1 * galaxy->Mvir / (1.0e10);
    }
    
    // Standard infall calculation
    standard_infall_module_t *module = (standard_infall_module_t*)
                                     module_get_current_module_of_type(MODULE_TYPE_INFALL);
    *infall_rate = module->efficiency * cooling_rate;
    
    LOG_DEBUG("Calculated infall rate %.3e for galaxy %d", *infall_rate, galaxy_index);
    return MODULE_INVOKE_SUCCESS;
}

// Apply infall function (can be called by other modules)
int standard_infall_apply(void *output, int galaxy_index, double infall_rate, double dt) {
    double *mass_added = (double*)output;
    galaxy_t *galaxy = &Gal[galaxy_index];
    
    // Apply infall to disk
    *mass_added = infall_rate * dt;
    galaxy->DiskMass += *mass_added;
    
    LOG_DEBUG("Applied infall: added %.3e to galaxy %d", *mass_added, galaxy_index);
    return MODULE_INVOKE_SUCCESS;
}

// Module phase function for halos
int standard_infall_execute_halo(standard_infall_module_t *module, halo_t *halo, 
                                evolution_context_t *context) {
    // No halo-level operations in standard infall
    return SUCCESS;
}

// Module phase function for galaxies - called by pipeline executor
int standard_infall_execute_galaxy(standard_infall_module_t *module, galaxy_t *galaxy, 
                                  evolution_context_t *context) {
    LOG_DEBUG("Executing standard infall for galaxy %d", galaxy->index);
    
    // Calculate infall rate
    double infall_rate;
    int result = standard_infall_calculate(&infall_rate, galaxy->index, context->dt);
    if (result != MODULE_INVOKE_SUCCESS) {
        LOG_ERROR("Failed to calculate infall rate for galaxy %d", galaxy->index);
        return result;
    }
    
    // Apply infall
    double mass_added;
    result = standard_infall_apply(&mass_added, galaxy->index, infall_rate, context->dt);
    if (result != MODULE_INVOKE_SUCCESS) {
        LOG_ERROR("Failed to apply infall for galaxy %d", galaxy->index);
        return result;
    }
    
    // Update galaxy properties and tracking statistics
    context->stats.total_infall_mass += mass_added;
    
    return SUCCESS;
}
```

### Module Registration

Finally, register the module during simulation initialization:

```c
// In physics_modules.c or equivalent initialization code
void register_physics_modules(void) {
    // Other module registrations...
    
    // Register standard infall module
    standard_infall_module_t *infall_module = calloc(1, sizeof(standard_infall_module_t));
    if (infall_module == NULL) {
        LOG_CRITICAL("Failed to allocate memory for standard infall module");
        exit(1);
    }
    
    if (standard_infall_init(infall_module) != SUCCESS) {
        LOG_CRITICAL("Failed to initialize standard infall module");
        free(infall_module);
        exit(1);
    }
    
    // Other module registrations...
}
```

## Common Conversion Patterns

### Parameter Access
```c
// Legacy:
double param = Infall_params.efficiency;

// New:
double param = module->efficiency;
```

### Global State Access
```c
// Legacy:
extern struct global_data GlobalData;
double z = ZZ[Halo[gal->HaloNr].SnapNum];

// New:
double z = context->current_redshift;
```

### Error Handling
```c
// Legacy:
if (error_condition) {
    printf("Error: something went wrong\n");
    // Continue anyway or use goto
}

// New:
if (error_condition) {
    LOG_ERROR("Something went wrong: %s", specific_error_details);
    return ERROR_CODE_SPECIFIC_TO_ISSUE;
}
```

### Memory Management
```c
// Legacy:
void *data = malloc(size);
if (data == NULL) {
    // Error handling was inconsistent
}

// New:
void *data = calloc(1, size);
if (data == NULL) {
    LOG_ERROR("Memory allocation failed for %zu bytes", size);
    return ERROR_MEMORY_ALLOCATION_FAILED;
}
// Ensure cleanup in module_cleanup function
```

## Common Pitfalls to Avoid

1. **Forgetting to check return values** from `module_invoke` calls
2. **Circular dependencies** between modules
3. **Not registering functions** that other modules may need to call
4. **Inconsistent error handling** across module functions
5. **Accessing global state directly** instead of using context parameters
6. **Missing cleanup code** in the module cleanup function
7. **Confusing output parameters** in module functions
