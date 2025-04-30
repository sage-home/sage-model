# SAGE Core Modularity Implementation Plan

## Overview

This implementation plan outlines the steps needed to complete the core refactoring for "Runtime Functional Modularity" as described in the overall SAGE refactoring plan. The primary goal is to transform the SAGE core codebase into a fully modular system that relies *exclusively* on the plugin architecture for executing physics, removing all legacy execution paths and fallbacks from the core logic.

**Current Status:** A partial plugin architecture exists with dual implementation paths. Cooling and infall modules have been migrated, but the core still contains legacy execution logic.

**Goal:** Create a clean, fully modular core that can run without any physics modules loaded (producing empty but valid output). The core will have no knowledge of specific physics implementations and will interact with physics solely through a well-defined module API and pipeline system. This refactoring prepares the codebase for the subsequent migration of remaining legacy physics implementations into the new modular structure.

**Benefits:**
-   Clean separation between core infrastructure and scientific models.
-   Core codebase becomes independent of specific physics implementations.
-   Enables runtime swapping, addition, or removal of physics modules without recompilation (once modules are fully migrated).
-   Simplifies future development and testing of new physics modules.
-   Provides a clear and robust framework for migrating remaining legacy physics code.
-   Improves maintainability and extensibility of the SAGE model.

## Step 1: Directory Restructuring ✅ COMPLETED

### Step 1.1: Reorganize Physics Directories ✅ COMPLETED
**WHERE:** File system operations
**WHY:** Clearly separate migrated modules from physics code yet to be migrated.
**WHAT:**
-   Create `src/physics/legacy/` directory for unmigrated physics files (renamed from `migrate` for clarity).
-   Move all `src/physics/model_*.c` and `src/physics/model_*.h` files into `src/physics/legacy/`.
-   Move already migrated modules (e.g., `cooling_module.c/h`, `infall_module.c/h`) from `src/physics/modules/` directly into `src/physics/`.
-   Update Makefile to reflect new paths (`LEGACY_PHYSICS_SRC`, `MODULE_PHYSICS_SRC`).

```bash
# Example commands (adjust based on actual file names)
mkdir -p src/physics/legacy
git mv src/physics/model_*.c src/physics/model_*.h src/physics/legacy/
git mv src/physics/modules/cooling_module.c src/physics/modules/cooling_module.h src/physics/
git mv src/physics/modules/infall_module.c src/physics/modules/infall_module.h src/physics/
# Update Makefile paths for legacy and module sources
```

### Step 1.2: Update Include Paths in Migrated Files ✅ COMPLETED
**WHERE:** All migrated module files now in `src/physics/` (e.g., `cooling_module.c/h`, `infall_module.c/h`).
**WHY:** Ensure imports work correctly with the new directory structure.
**WHAT:**
-   Update include paths referencing `../core/` to remain `../core/`.
-   Update include paths referencing other modules (e.g., `cooling_tables.h`) to use relative paths within `src/physics/`.
-   Remove any includes referencing files now in `src/physics/legacy/`.

```c
// Before (example from cooling_module.c)
#include "../../core/core_logging.h"
#include "../modules/cooling_tables.h" // Assuming cooling_tables was also in modules

// After
#include "../core/core_logging.h"
#include "cooling_tables.h" // Now cooling_tables is also in src/physics/
```

## Step 2: Core Execution Framework Update ⏳ IN PROGRESS

### Step 2.1: Clean Up Core Build Model Imports
**WHERE:** `src/core/core_build_model.c`
**WHY:** Remove all references to legacy physics implementations from the core evolution loop file.
**WHAT:**
-   Remove *all* `#include` directives pointing to files within `src/physics/legacy/`.
-   Keep only includes necessary for core functionality, the pipeline system, and potentially the *base* module interface (`core_module_system.h`). Do *not* include specific module headers (like `cooling_module.h`).

```c
// Remove these lines entirely
#include "../physics/legacy/model_misc.h"
#include "../physics/legacy/model_mergers.h"
#include "../physics/legacy/model_infall.h"
#include "../physics/legacy/model_reincorporation.h"
#include "../physics/legacy/model_starformation_and_feedback.h"
// ... and any other legacy physics includes ...

// Keep only core and potentially base module interface includes
#include "core_allvars.h"
#include "core_pipeline_system.h"
// #include "core_module_system.h" // If base_module struct is needed directly
```

### Step 2.2: Update Physics Pipeline Executor
**WHERE:** `src/core/physics_pipeline_executor.c`
**WHY:** Remove all legacy execution paths and fallbacks, making the executor rely *exclusively* on the module interface.
**WHAT:**
-   Remove any forward declarations for legacy compatibility functions (e.g., `cooling_recipe_compat`).
-   Remove *all* code blocks that check `if (physics_module == NULL)` and subsequently call legacy functions.
-   Ensure the executor *only* attempts to call module functions through the `physics_module_interface`.
-   Implement robust error handling for when a required module is not found (return `MODULE_STATUS_NOT_FOUND`).

```c
// Before (example showing legacy fallback)
/*
if (physics_module == NULL) {
    switch (step->type) {
        case MODULE_TYPE_COOLING:
            if (context->execution_phase == PIPELINE_PHASE_GALAXY) {
                // Legacy call
                cooling_recipe_compat(context->current_galaxy, context->dt,
                                   context->galaxies, context->params);
                return 0;
            }
            return 0;
        // Other legacy cases...
    }
}
*/

// After (pure module-based execution attempt)
if (physics_module == NULL) {
    // Module not found or not loaded
    // Error handling depends on whether the step is optional
    // This check might be better placed in the pipeline_execute_phase function
    // For now, ensure the executor *assumes* the module is present if called.
    // The calling function (pipeline_execute_phase) should handle missing modules.
    LOG_ERROR("Executor called with NULL module for step type %d (%s)",
             step->type, module_type_name(step->type));
    return MODULE_STATUS_ERROR; // Or a more specific error
}

// Proceed to call module phase handlers via physics_module interface...
switch (context->execution_phase) {
    case PIPELINE_PHASE_HALO:
        if (physics_module->execute_halo_phase) {
            return physics_module->execute_halo_phase(module_data, context);
        }
        break;
    // Other phases...
}
return 0; // Return 0 if the module doesn't implement the current phase
```

### Step 2.3: Update Pipeline Phase Execution
**WHERE:** `src/core/core_pipeline_system.c` (specifically `pipeline_execute_phase_with_executor` or similar)
**WHY:** Ensure proper phase execution logic when modules might be missing, differentiating between optional and required steps.
**WHAT:**
-   Modify the loop that iterates through pipeline steps for a given phase.
-   Before calling the executor, check if the module required by the step exists and is active.
    -   Use `pipeline_get_step_module` or similar logic.
-   If a required module is missing (`step->optional == false`), log an ERROR and return an appropriate error status (e.g., `MODULE_STATUS_MODULE_NOT_FOUND`) to halt execution for that phase/halo.
-   If an optional module is missing (`step->optional == true`), log a WARNING and continue to the next step.
-   If the executor returns an error status for a required step, propagate the error and break the loop.
-   If the executor returns an error status for an optional step, log a WARNING but continue execution.

```c
// Inside pipeline_execute_phase_with_executor loop over steps:
struct base_module *module = NULL;
void *module_data = NULL;
int module_status = pipeline_get_step_module(step, &module, &module_data);

if (module_status != 0 || module == NULL) {
    if (!step->optional) {
        LOG_ERROR("Required module for step '%s' (type %s) not found or inactive.",
                 step->step_name, module_type_name(step->type));
        result = MODULE_STATUS_MODULE_NOT_FOUND; // Use a specific error code
        break; // Halt execution for this phase
    } else {
        LOG_WARNING("Optional module for step '%s' (type %s) not found or inactive. Skipping step.",
                   step->step_name, module_type_name(step->type));
        continue; // Skip this optional step
    }
}

// Check if module supports the current phase before executing
if (!(module->phases & phase)) {
    LOG_DEBUG("Module '%s' does not support phase %d. Skipping step '%s'.",
             module->name, phase, step->step_name);
    continue;
}

// Execute the step using the executor
int status = exec_fn(step, module, module_data, context);

// Handle execution status
if (status != 0) {
    if (step->optional) {
        LOG_WARNING("Optional step '%s' failed with status %d. Continuing execution.",
                   step->step_name, status);
        // Optionally record the non-fatal error status if needed later
    } else {
        LOG_ERROR("Required step '%s' failed with status %d. Halting phase execution.",
                 step->step_name, status);
        result = status; // Propagate the error code
        break; // Exit the loop over steps
    }
}
```

### Step 2.4: Update Core Galaxy Accessors
**WHERE:** `src/core/core_galaxy_accessors.c` and `src/core/core_galaxy_accessors.h`
**WHY:** Ensure all galaxy property access goes through the accessor system, preparing for modules to potentially use extensions exclusively.
**WHAT:**
-   **2.4.1:** Update the global `use_extension_properties` flag to default to `1` (use extensions). This makes the extension path the default, simplifying the transition.
-   **2.4.2:** Add *all* remaining accessor functions (getters/setters) for every field in the `struct GALAXY` that represents a physical property (masses, metals, radii, SFRs, etc.). Ensure consistent naming (e.g., `galaxy_get_stellarmass`, `galaxy_set_stellarmass`).
-   **2.4.3:** Ensure consistent error handling within accessors (e.g., logging warnings or errors if an extension property is expected but not found when `use_extension_properties` is 1).

```c
// In core_galaxy_accessors.c
// Change from
// int use_extension_properties = 0;
// To
int use_extension_properties = 1; // Default to using extensions

// Add comprehensive accessors for all physical properties
// Example:
double galaxy_get_stellarmass(const struct GALAXY *galaxy) {
    if (use_extension_properties) {
        // Logic to get from extension property
        // ... return extension value or 0.0 if not found ...
    } else {
        return galaxy->StellarMass;
    }
}

void galaxy_set_stellarmass(struct GALAXY *galaxy, double value) {
    if (use_extension_properties) {
        // Logic to set extension property
        // ... handle allocation if needed ...
    } else {
        galaxy->StellarMass = value;
    }
}
// ... implement for ALL other physical properties ...
```

### Step 2.5: Update Module System Initialization
**WHERE:** `src/core/core_init.c`
**WHY:** Ensure the module system is configured correctly at startup, including auto-discovery and activation based on configuration or defaults.
**WHAT:**
-   Modify `initialize_module_system` to:
    -   Call `module_system_configure` using `run_params`.
    -   If discovery is enabled (`run_params->runtime.EnableModuleDiscovery`), call `module_discover`.
    -   Log the number of modules found or a warning if none are found/loaded.
    -   Implement logic to activate modules based on configuration (e.g., from a future JSON config) or default settings if no config is present. For now, it might just activate any discovered modules or specific default ones.
-   Improve error handling for module loading failures during discovery.
-   Add validation checks for the module system state after initialization (e.g., check if required core modules like 'misc' are present if expected).

```c
// In core_init.c
void initialize_module_system(struct params *run_params) {
    /* Initialize the module system */
    int status = module_system_initialize();
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize module system, status = %d", status);
        // Decide on error handling: abort or continue without modules?
        // For now, let's assume we can continue without modules.
        return;
    }

    /* Configure the module system */
    status = module_system_configure(run_params);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_WARNING("Failed to configure module system, status = %d. Using defaults.", status);
    }

    /* Auto-discover modules if enabled */
    if (global_module_registry && global_module_registry->discovery_enabled) {
        LOG_INFO("Starting module discovery...");
        int modules_found = module_discover(run_params);
        if (modules_found > 0) {
            LOG_INFO("Discovered %d potential modules", modules_found);
            // Activation logic will depend on config system (Phase 3)
            // For now, maybe activate all discovered modules for testing?
            // Or activate based on simple rules/defaults.
            // Example: Activate first found module of each type
            // activate_default_modules();
        } else if (modules_found == 0) {
            LOG_WARNING("No physics modules found or loaded during discovery.");
        } else {
            LOG_ERROR("Module discovery failed with status %d", modules_found);
        }
    } else {
        LOG_INFO("Module discovery is disabled.");
    }

    // Validate module system state (e.g., check if essential modules are active)
    // module_system_validate_state(); // Implement this function
}
```

### Step 2.6: Update Evolve Galaxies Loop
**WHERE:** `src/core/core_build_model.c`
**WHY:** Ensure the main galaxy evolution loop *exclusively* uses the pipeline system and its phases, removing any direct calls to legacy physics functions.
**WHAT:**
-   **2.6.1:** Before the main loop over `STEPS`, execute the `PIPELINE_PHASE_HALO` using `pipeline_execute_phase`. Handle errors appropriately.
-   **2.6.2:** Inside the main loop over `STEPS`:
    -   Set the current `step` in the `pipeline_ctx`.
    -   **2.6.3:** Inside the loop over galaxies (`for p = 0 to ctx.ngal`):
        -   Set `pipeline_ctx.current_galaxy = p`.
        -   Skip merged galaxies (`galaxies[p].mergeType > 0`).
        -   Execute the `PIPELINE_PHASE_GALAXY` using `pipeline_execute_phase`. Handle errors (abort halo evolution on error).
    -   **2.6.4:** After the galaxy loop (still inside the `STEPS` loop), execute the `PIPELINE_PHASE_POST` using `pipeline_execute_phase`. Handle errors.
-   **2.6.5:** After the main loop over `STEPS`, execute the `PIPELINE_PHASE_FINAL` using `pipeline_execute_phase`. Handle errors.
-   Remove *all* direct calls to physics functions (e.g., `infall_recipe_compat`, `cooling_recipe_compat`, `starformation_and_feedback_compat`, `reincorporate_gas_compat`, `check_disk_instability_compat`, `process_merger_events`). The pipeline execution handles invoking the correct modules (or logging errors if modules are missing).

```c
// Inside evolve_galaxies function in core_build_model.c

// Initialize pipeline context (ensure it includes redshift, etc.)
// ... pipeline_ctx setup ...
pipeline_ctx.redshift = ctx.redshift; // Make sure redshift is set

// Get the global pipeline
struct module_pipeline *physics_pipeline = pipeline_get_global();
if (!physics_pipeline) {
    CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to get global physics pipeline.");
    return EXIT_FAILURE;
}

// HALO phase - execute once per halo
CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Executing HALO phase for halo %d", ctx.halo_nr);
status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_HALO);
if (status != 0) {
    CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute HALO phase, status = %d", status);
    return EXIT_FAILURE; // Or handle error as appropriate
}

// Main integration steps loop
for (int step = 0; step < STEPS; step++) {
    pipeline_ctx.step = step;
    CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Starting step %d for halo %d", step, ctx.halo_nr);

    // GALAXY phase - for each galaxy
    for (int p = 0; p < ctx.ngal; p++) {
        pipeline_ctx.current_galaxy = p;

        // Skip merged galaxies
        if (galaxies[p].mergeType > 0) continue;

        CONTEXT_LOG(&ctx, LOG_LEVEL_TRACE, "Executing GALAXY phase for galaxy %d (step %d)", p, step);
        status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_GALAXY);
        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed GALAXY phase for galaxy %d (step %d), status = %d", p, step, status);
            // Decide on error handling: stop halo, stop run, or continue?
            // For now, let's stop processing this halo on critical failure.
            return EXIT_FAILURE;
        }
    } // End loop over galaxies

    // POST phase - after all galaxies processed in this step
    CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Executing POST phase for step %d", step);
    status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_POST);
    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed POST phase for step %d, status = %d", step, status);
        return EXIT_FAILURE;
    }

    // Process merger queue (assuming it's populated by a merger module in POST phase or similar)
    // process_merger_events(&merger_queue, ctx.galaxies, run_params); // This might now be part of a merger module's POST phase

} // End loop over steps

// FINAL phase - after all steps complete
CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Executing FINAL phase for halo %d", ctx.halo_nr);
status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_FINAL);
if (status != 0) {
    CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed FINAL phase for halo %d, status = %d", ctx.halo_nr, status);
    return EXIT_FAILURE;
}

// Final normalizations and cleanup (if not handled by FINAL phase modules)
// ...
```

## Step 3: Module API Refinement

### Step 3.1: Define Standard Physics Module Interface
**WHERE:** Create `src/physics/physics_module_interface.h`
**WHY:** Establish a clear, richer API contract for all physics modules, including parameters and dependencies.
**WHAT:**
-   Define a standard interface structure (`physics_module_interface`) that inherits from `base_module`.
-   Include function pointers for all execution phases (`execute_halo_phase`, `execute_galaxy_phase`, `execute_post_phase`, `execute_final_phase`). Modules implement only the phases they support.
-   Add function pointers for module-specific parameter handling (`configure_parameters`, `get_parameter`).
-   Add function pointer for declaring dependencies (`declare_dependencies`).
-   Document required function signatures, expected behaviors, and return codes.

```c
#pragma once

#include "../core/core_module_system.h" // Includes base_module, module_dependency
#include "../core/core_pipeline_system.h" // Includes pipeline_context

/**
 * Standard physics module interface
 * All physics modules must implement this interface to be compatible
 * with the SAGE pipeline system.
 */
struct physics_module_interface {
    struct base_module base;           /* Base module properties (name, version, type, etc.) */

    /* Phase handlers - modules implement the phases they support */
    /* Return 0 on success, non-zero on failure */
    int (*execute_halo_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_galaxy_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_post_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_final_phase)(void *module_data, struct pipeline_context *context);

    /* Module-specific parameter handling */
    /* Return 0 on success, non-zero on failure */
    int (*configure_parameters)(void *module_data, const char *param_name, const void *param_value);
    int (*get_parameter)(void *module_data, const char *param_name, void *param_value);

    /* Module dependencies */
    /* Return number of dependencies declared, or negative on error */
    int (*declare_dependencies)(void *module_data, struct module_dependency *dependencies, int max_dependencies);
};
```

### Step 3.1.1: Refactor Migrated Modules
**WHERE:** `src/physics/cooling_module.c/h`, `src/physics/infall_module.c/h`
**WHY:** Ensure the already migrated modules conform to the new, richer `physics_module_interface`.
**WHAT:**
-   Update the module interface structures in `cooling_module.c` and `infall_module.c` to use `struct physics_module_interface`.
-   Implement the new required functions (even if just stubs initially): `configure_parameters`, `get_parameter`, `declare_dependencies`.
-   Map existing phase logic to the appropriate `execute_*_phase` functions.
-   Update the module registration in their respective `_create` functions to reflect the new interface type if necessary (though likely still uses `base_module` for registration).

### Step 3.2: Implement Module Callback System
**WHERE:** `src/core/core_module_callback.c` and `src/core/core_module_callback.h`
**WHY:** Enable standardized, controlled communication *between* modules, respecting declared dependencies.
**WHAT:**
-   Enhance the callback system to fully support the dependency declarations from Step 3.1.
-   Implement `module_invoke` to check dependencies before allowing a call.
-   Add robust error handling for failed calls, missing dependencies, or type mismatches.
-   Implement circular dependency detection within the call stack.
-   Add debug logging for module interactions (calls, returns, errors).

```c
// In core_module_callback.h

// Structure for module dependencies (ensure it matches or is compatible with physics_module_interface.h)
struct module_dependency {
    enum module_type type;         // Type of required module
    const char *name;              // Optional specific module name
    const char *version;           // Optional version requirement (string for now)
    bool required;                 // Whether dependency is required
    // Add parsed version fields if implementing version checking now
    // struct module_version min_version;
    // struct module_version max_version;
};

// Function to declare a dependency (maybe move declaration logic here or keep in module_system.c)
int module_declare_dependency(int module_id, const struct module_dependency *dependency);

// Function to invoke another module's function
int module_invoke(int caller_id, int callee_id_or_type, const char *callee_name,
                  const char *function_name, void *context, void *args, void *result);

// In core_module_callback.c
// Implement module_invoke:
// 1. Find target module (by ID, type, or name).
// 2. Check if caller has declared dependency on target.
// 3. Check for circular dependency using call stack.
// 4. Push call frame onto stack.
// 5. Look up function pointer in target module's registry.
// 6. Call the function.
// 7. Pop call frame from stack.
// 8. Return status/result.
```

### Step 3.2.1: Add Callback Integration Tests
**WHERE:** `tests/test_module_callback_integration.c` (New File), `tests/Makefile.test`
**WHY:** Verify that the callback system works correctly for inter-module communication, including dependency checks and error handling.
**WHAT:**
-   Create a test suite with multiple mock modules.
-   Test successful calls between modules with declared dependencies.
-   Test calls that should fail due to missing dependencies.
-   Test calls that should fail due to circular dependencies.
-   Test error propagation through the callback system.

### Step 3.3: Update Module Template Generator
**WHERE:** `src/core/core_module_template.c`
**WHY:** Ensure new modules generated by the template conform to the refined `physics_module_interface`.
**WHAT:**
-   Update the template generator to use `struct physics_module_interface`.
-   Include function stubs for all required functions, including phase handlers, parameter functions, and dependency declaration.
-   Add comments and examples for implementing these new functions.
-   Include examples of declaring dependencies and registering parameters.

### Step 3.4: Implement Module Configuration Functions
**WHERE:** `src/core/core_config_system.c`
**WHY:** Allow runtime configuration of module parameters via a structured configuration file (e.g., JSON).
**WHAT:**
-   Implement functions to load module configurations (e.g., from a specific section in a JSON file).
-   Add logic to bind loaded parameters to module instances using the `configure_parameters` function pointer from the module interface.
-   Ensure the configuration system properly initializes modules based on the loaded configuration (e.g., which modules to activate).

```c
// In core_config_system.c
int config_configure_modules(const char *config_path, struct params *run_params) {
    // Load configuration from config_path (e.g., using cJSON)
    // cJSON *root = load_json_config(config_path);
    // if (!root) return -1;

    // cJSON *modules_config = cJSON_GetObjectItem(root, "modules");
    // if (!modules_config) { /* Handle missing section */ }

    // For each module defined in the configuration:
    //   const char *module_name = ...;
    //   bool enabled = ...;
    //   cJSON *params_obj = ...;

    //   int module_id = module_find_by_name(module_name);
    //   if (module_id < 0) { /* Handle module not found */ continue; }

    //   struct base_module *module;
    //   void *module_data;
    //   module_get(module_id, &module, &module_data);

    //   if (module->configure_parameters) {
    //       // Iterate through params_obj and call configure_parameters
    //       cJSON *param = NULL;
    //       cJSON_ArrayForEach(param, params_obj) {
    //           const char *param_name = param->string;
    //           // Convert param value to appropriate type/string representation
    //           // void *param_value = ...;
    //           // module->configure_parameters(module_data, param_name, param_value);
    //       }
    //   }

    //   if (enabled) {
    //       module_set_active(module_id);
    //   }

    // cJSON_Delete(root);
    return 0;
}
```

### Step 3.4.1: Define Configuration Structure
**WHERE:** `docs/config_structure.md` (New File), `src/core/core_config_system.c`
**WHY:** Clearly define how modules and their parameters will be specified in the configuration file.
**WHAT:**
-   Document the proposed JSON structure for configuring modules (e.g., which modules to load, activate, and their specific parameters).
-   Specify how this JSON configuration interacts with the existing `.par` parameter file (e.g., JSON for module structure/activation, `.par` for core physics/cosmology).
-   Update `config_configure_modules` to parse this defined structure.

```json
// Example docs/config_structure.md content
{
  "core_parameters_file": "input/millennium.par", // Optional: Path to legacy param file
  "modules": [
    {
      "name": "StandardCooling", // Matches module->name
      "library": "modules/cooling.so", // Optional: path if not auto-discovered
      "enabled": true, // Whether this module should be active for its type
      "parameters": {
        "CoolingRateFactor": 1.1,
        "AGNFeedbackEfficiency": 0.05
        // Module-specific parameters
      }
    },
    {
      "name": "AnotherModule",
      "enabled": false,
      "parameters": { ... }
    }
    // ... other modules
  ],
  "pipeline": [ // Optional: Define pipeline steps explicitly
    {"type": "infall", "module": "StandardInfall", "optional": false},
    {"type": "cooling", "module": "StandardCooling", "optional": false},
    // ... other steps
  ]
}
```

### Step 3.5: Update Module Registry
**WHERE:** `src/core/core_module_system.c`
**WHY:** Ensure proper module registration, activation, and dependency handling based on the refined API and configuration.
**WHAT:**
-   Update `module_register` to handle the `physics_module_interface` (potentially just casting to `base_module` is sufficient).
-   Enhance `module_set_active` to perform dependency checks using `declare_dependencies` before activating a module.
-   Add validation checks to ensure the registry state is consistent after loading and activation.

### Step 3.6: Implement Module Parameter Views System
**WHERE:** `src/core/core_parameter_views.c` and module files (`src/physics/*.c`)
**WHY:** Move parameter handling *into* modules, removing it from core code and allowing modules to manage their own parameters via the new interface functions.
**WHAT:**
-   Remove parameter view initialization from `core_parameter_views.c`. Parameter views become obsolete as modules handle their own parameters.
-   In each module's `initialize` function, implement logic to:
    -   Register its parameters using `module_register_parameter_with_module`.
    -   Retrieve parameter values (e.g., from `run_params` initially, later from config system via `configure_parameters`).
    -   Perform parameter validation *within the module*.
-   Update physics functions within modules to access parameters from their `module_data` struct instead of passing parameter views.
-   Remove the `core_parameter_views.c/h` files entirely once all modules access parameters internally.

```c
// Example in cooling_module.c initialize function:
static int cooling_module_initialize(struct params *params, void **module_data) {
    struct cooling_module_data *data = malloc(sizeof(struct cooling_module_data));
    // ... error check ...
    *module_data = data;
    int module_id = cooling_module_interface.base.module_id; // Assuming interface struct is global

    // 1. Register parameters
    module_parameter_t p;
    module_create_parameter_double(&p, "CoolingRateFactor", 1.0, 0.1, 10.0, "Factor to scale cooling rate", "dimensionless", module_id);
    module_register_parameter_with_module(module_id, &p);
    // ... register other parameters ...

    // 2. Get initial values (e.g., from run_params for now)
    data->cooling_rate_factor = params->physics.CoolingRateFactor; // Example legacy access
    data->agn_efficiency = params->physics.RadioModeEfficiency; // Example legacy access
    // Later, this might use module_get_parameter or be set by config system via configure_parameters

    // 3. Validate parameters *within the module*
    if (data->cooling_rate_factor <= 0.0) {
        LOG_ERROR("CoolingRateFactor must be positive.");
        free(data);
        return MODULE_STATUS_INVALID_ARGS;
    }
    // ... other validations ...

    return MODULE_STATUS_SUCCESS;
}

// Example in cooling_module_calculate function:
// Remove cooling_params_view argument
double cooling_module_calculate(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data) {
    struct cooling_module_data *data = (struct cooling_module_data*)module_data;
    // Access parameters directly from module data
    double cooling_rate = calculate_internal_cooling(...) * data->cooling_rate_factor;
    // ...
}
```

## Step 4: Event System, Serialization, and Validation

### Step 4.1: Update Event System Integration
**WHERE:** `src/core/core_event_system.c` and `src/core/core_event_system.h`
**WHY:** Make the event system fully module-driven, removing any core-level event handling logic.
**WHAT:**
-   Update event type registration (`event_register_type`) to potentially associate types with owning modules (optional enhancement).
-   Remove any remaining core-level event handlers or hard-coded event logic.
-   Ensure modules register their own event handlers during their `initialize` phase.
-   Add standard event emission points within the *pipeline execution* logic (e.g., `PIPELINE_STEP_START`, `PIPELINE_STEP_END`) to allow modules to react to pipeline progression.
-   Ensure proper event propagation and error handling within the event dispatch mechanism.

```c
// Example in cooling_module.c initialize function:
event_register_handler(EVENT_AGN_FEEDBACK_APPLIED,
                     cooling_handle_agn_feedback, // Module's handler function
                     data, // Pass module data as user_data
                     module_id, // Module ID
                     "CoolingAGNFeedbackHandler", // Handler name
                     EVENT_PRIORITY_NORMAL);

// Example handler function in cooling_module.c:
bool cooling_handle_agn_feedback(const struct event *event, void *user_data) {
    struct cooling_module_data *data = (struct cooling_module_data*)user_data;
    // ... handle event using module data ...
    return true; // Indicate event was handled
}
```

### Step 4.1.1: Add Event System Integration Tests
**WHERE:** `tests/test_event_system_integration.c` (New File), `tests/Makefile.test`
**WHY:** Verify that modules can correctly register for, emit, and handle events, including custom events.
**WHAT:**
-   Create tests where one mock module emits an event.
-   Verify that another mock module, registered as a handler, receives and processes the event correctly.
-   Test event propagation flags.
-   Test custom event registration and handling.

### Step 4.2: Implement Module Data Serialization
**WHERE:** `src/io/io_property_serialization.c` and `src/io/io_property_serialization.h`
**WHY:** Ensure module-specific galaxy properties (registered via the extension system) are saved and loaded correctly by the I/O handlers.
**WHAT:**
-   Update the property serialization system (`property_serialization_context`) to correctly handle properties registered by different modules.
-   Ensure `property_serialization_add_properties` correctly identifies and includes module-specific properties based on filter flags.
-   Modify I/O handlers (Binary and HDF5) to use the serialization context to write/read both core and extended properties seamlessly.
-   Ensure backward compatibility for loading data files that may not contain extended properties.

```c
// In io_property_serialization.c (conceptual update to add_properties)
int property_serialization_add_properties(struct property_serialization_context *ctx) {
    // ... existing logic ...
    // Iterate through global_extension_registry->extensions
    for (int i = 0; i < global_extension_registry->num_extensions; i++) {
        const galaxy_property_t *prop = &global_extension_registry->extensions[i];
        // Apply filter flags (SERIALIZE_EXPLICIT, etc.)
        // ...
        // If property passes filter:
        //   Align offset
        //   Copy metadata to ctx->properties[prop_idx]
        //   Store mapping: ctx->property_id_map[prop_idx] = i;
        //   Update total_size
        //   prop_idx++;
    }
    // ... finalize context ...
}

// In I/O handlers (e.g., io_binary_output.c)
// During initialization:
// property_serialization_init(&data->prop_ctx, SERIALIZE_EXPLICIT);
// property_serialization_add_properties(&data->prop_ctx);
// Write header using property_serialization_create_header(...)

// During galaxy writing:
// property_serialize_galaxy(&data->prop_ctx, galaxy, prop_buffer);
// buffer_write(..., prop_buffer, property_serialization_data_size(&data->prop_ctx));
```

### Step 4.3: Create Empty Module Test
**WHERE:** `tests/test_empty_modules.c` (New File), `tests/Makefile.test`
**WHY:** Verify the core SAGE framework runs correctly and produces valid (but empty) output when *no* physics modules are loaded or active.
**WHAT:**
-   Create a test setup that initializes SAGE core systems but does not load or activate any physics modules.
-   Run the main evolution loop (`construct_galaxies` -> `evolve_galaxies`).
-   Verify the code executes without errors or crashes.
-   Check that output files (e.g., `sage_output_0.bin` or `.hdf5`) are created but contain zero galaxies.

### Step 4.4: Create Module Loading Test
**WHERE:** `tests/test_module_loading.c` (New File), `tests/Makefile.test`
**WHY:** Verify the module loading, registration, activation, and dependency resolution mechanisms.
**WHAT:**
-   Create mock module manifests and dummy library files (or use existing test modules if possible).
-   Configure SAGE to discover modules from a test directory.
-   Run `initialize_module_system` and `module_discover`.
-   Verify that modules are correctly discovered and registered.
-   Test activation of modules with satisfied dependencies.
-   Test that activation fails for modules with unsatisfied dependencies.
-   Verify error messages for loading/activation failures.

### Step 4.5: Create Pipeline Execution Test
**WHERE:** `tests/test_pipeline_execution.c` (New File), `tests/Makefile.test`
**WHY:** Verify the pipeline system correctly executes modules in the defined order and phases.
**WHAT:**
-   Create several mock modules implementing different phases (HALO, GALAXY, POST, FINAL).
-   Define a test pipeline with these modules.
-   Set the test pipeline as the global pipeline.
-   Run a simplified `evolve_galaxies`-like loop that calls `pipeline_execute_phase` for each phase.
-   Use counters or flags within the mock modules to verify:
    -   Modules are called in the correct order.
    -   Modules are called only during the phases they support.
    -   Context information (e.g., `current_galaxy`, `step`) is correctly passed.
-   Test error handling when a required step fails.

### Step 4.6: Update Makefile Tests
**WHERE:** `tests/Makefile.test` (or main Makefile if tests are integrated there)
**WHY:** Ensure all new tests are built and run as part of the standard testing process.
**WHAT:**
-   Add build rules for the new test files (`test_empty_modules.c`, `test_module_loading.c`, `test_pipeline_execution.c`, `test_event_system_integration.c`, `test_module_callback_integration.c`).
-   Add the new test executables to the list of tests run by the `make tests` target.
-   Ensure tests run correctly as part of CI/CD workflows.

```makefile
# Example additions to Makefile
# Add new test executables
TEST_EXECS += tests/test_empty_modules tests/test_module_loading tests/test_pipeline_execution tests/test_event_system_integration tests/test_module_callback_integration

# Add build rules for new tests
tests/test_empty_modules: tests/test_empty_modules.c $(SAGELIB)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L. -l$(LIBNAME) $(LIBFLAGS)

# ... rules for other new tests ...

# Update the 'tests' target dependencies and execution list
tests: $(EXEC) $(TEST_EXECS) # Add new test executables here
	# ... existing test commands ...
	./tests/test_empty_modules
	./tests/test_module_loading
	./tests/test_pipeline_execution
	./tests/test_event_system_integration
	./tests/test_module_callback_integration
	@echo "All tests completed successfully."
```

## Step 5: Documentation and Future Extensibility

### Step 5.1: Update Module Development Guide
**WHERE:** `docs/module_development.md`
**WHY:** Provide comprehensive guidance for developers creating new modules using the refined framework.
**WHAT:**
-   Document the standard `physics_module_interface`.
-   Explain how to implement phase handlers (`execute_*_phase`).
-   Detail the parameter registration and configuration process (`configure_parameters`, `get_parameter`).
-   Explain dependency declaration (`declare_dependencies`).
-   Document the module callback system (`module_invoke`) and inter-module communication patterns.
-   Provide examples of different module types and features (extensions, events).

### Step 5.2: Update Architecture Documentation
**WHERE:** `docs/architecture.md`
**WHY:** Document the new, fully modular core architecture for future developers.
**WHAT:**
-   Describe the pipeline system and its execution phases.
-   Explain the module system, including discovery, loading, activation, and lifecycle.
-   Document the extension property system and its integration with serialization.
-   Provide diagrams illustrating the core architecture and module interactions.
-   Explain the module callback system and dependency resolution mechanism.
-   Detail the event system integration for modules.
-   Clarify the configuration system (JSON + legacy `.par` file interaction).

### Step 5.3: Update User Guide
**WHERE:** `docs/user_guide.md`
**WHY:** Help users configure and run the model with the new modular system.
**WHAT:**
-   Document how to configure modules using the new system (e.g., JSON file).
-   Explain how to enable/disable modules and set module-specific parameters.
-   Provide examples of different configuration scenarios.
-   Describe how users can potentially extend the system with custom modules (referencing the development guide).

### Step 5.4: Future Extensibility Guidelines
**WHERE:** `docs/future_extensibility.md`
**WHY:** Document how the system can be extended beyond current capabilities, guiding future development.
**WHAT:**
-   Explain the process for adding entirely new physics processes as modules.
-   Document how new pipeline phases could be added if needed.
-   Provide guidance on creating module-specific output formats or data products.
-   Describe strategies for creating entirely new *types* of modules beyond the current physics domains.
-   Discuss potential future enhancements like parallel module execution or advanced dependency management.

## Conclusion

This revised implementation plan provides a comprehensive roadmap for achieving a fully modular SAGE core framework. By removing legacy execution paths and refining the module API, callback system, event handling, and configuration, this plan establishes a clean separation between the core infrastructure and physics implementations.

Upon completion, the SAGE core will be physics-agnostic, capable of running without any specific physics modules loaded. This provides a robust and flexible foundation for the *next* phase: migrating the remaining legacy physics code into the new modular structure. The resulting system will be significantly more maintainable, extensible, and adaptable to future scientific research needs.

Key benefits achieved by this plan:
1.  **True Core Modularity**: Core depends only on module interfaces, not implementations.
2.  **Clean Separation**: Infrastructure and science models are fully decoupled in the core.
3.  **Runtime Configuration Ready**: Core supports runtime module management (loading, activation, parameters via config).
4.  **Simplified Migration**: Provides a clear framework for converting legacy physics.
5.  **Enhanced Testability**: Core and modules can be tested more independently.
6.  **Improved Maintainability**: Clearer code organization and responsibilities.

---