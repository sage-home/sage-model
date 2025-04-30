# SAGE Core Modularity Implementation Plan v2

## Overview

This implementation plan outlines the steps needed to complete the core refactoring for "Runtime Functional Modularity" as described in the overall SAGE refactoring plan. The primary goal is to transform the SAGE core codebase into a fully modular system that relies *exclusively* on the plugin architecture for executing physics, removing all legacy execution paths and fallbacks from the core logic.

**Current Status:** A partial plugin architecture exists with dual implementation paths. Cooling and infall modules have been migrated, but the core still contains legacy execution logic.

**Goal:** Create a clean, fully modular core that can run without any physics modules loaded (producing empty but valid output). The core will have no knowledge of specific physics implementations and will interact with physics solely through a well-defined module API and pipeline system. This refactoring prepares the codebase for the subsequent migration of remaining legacy physics implementations into the new modular structure in a separate effort.

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
-   Create `src/physics/legacy/` directory for unmigrated physics files.
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

## Step 2: Core Execution Framework Update

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
// ... and any other legacy physics includes ...

// Keep only core and potentially base module interface includes
#include "core_allvars.h"
#include "core_pipeline_system.h"
// #include "core_module_system.h" // If base_module struct is needed directly
```

### Step 2.2: Update Physics Pipeline Executor
**WHERE:** `src/core/physics_pipeline_executor.c`
**WHY:** Remove all legacy execution paths and fallbacks, making the executor rely *exclusively* on the module interface provided by the pipeline system.
**WHAT:**
-   Remove any forward declarations for legacy compatibility functions.
-   Remove *all* code blocks that check `if (physics_module == NULL)` and subsequently call legacy functions.
-   Ensure the executor *only* attempts to call module functions through the `physics_module_interface` pointer passed to it.
-   The responsibility for handling missing modules lies with the calling function (`pipeline_execute_phase`), not the executor itself. The executor assumes it receives a valid module pointer if called.

```c
// In physics_step_executor function

// Remove legacy compatibility function forward declarations if any remain

int physics_step_executor(
    struct pipeline_step *step,
    struct base_module *module, // This is now guaranteed non-NULL by the caller if called
    void *module_data,
    struct pipeline_context *context
) {
    if (step == NULL || context == NULL || module == NULL) { // Added module NULL check for safety
        LOG_ERROR("Invalid arguments to physics step executor");
        return MODULE_STATUS_INVALID_ARGS;
    }

    // Cast to the physics interface
    struct physics_module_interface *physics_module = (struct physics_module_interface *)module;

    // REMOVE ALL BLOCKS LIKE: if (physics_module == NULL) { /* call legacy compat function */ }

    // Execute appropriate phase handler directly
    switch (context->execution_phase) {
        case PIPELINE_PHASE_HALO:
            if (physics_module->execute_halo_phase) {
                return physics_module->execute_halo_phase(module_data, context);
            }
            break;
        case PIPELINE_PHASE_GALAXY:
            if (physics_module->execute_galaxy_phase) {
                // Skip merged galaxies within the module's phase handler if appropriate,
                // or ensure the pipeline context indicates this.
                // For simplicity, the check can remain in evolve_galaxies before calling pipeline_execute_phase.
                return physics_module->execute_galaxy_phase(module_data, context);
            }
            break;
        case PIPELINE_PHASE_POST:
            if (physics_module->execute_post_phase) {
                return physics_module->execute_post_phase(module_data, context);
            }
            break;
        case PIPELINE_PHASE_FINAL:
            if (physics_module->execute_final_phase) {
                return physics_module->execute_final_phase(module_data, context);
            }
            break;
        default:
            LOG_ERROR("Invalid pipeline phase: %d", context->execution_phase);
            return MODULE_STATUS_ERROR;
    }

    // Return success if the module doesn't implement the requested phase
    return MODULE_STATUS_SUCCESS;
}
```

### Step 2.3: Update Pipeline Phase Execution Logic
**WHERE:** `src/core/core_pipeline_system.c` (specifically `pipeline_execute_phase` or similar internal function)
**WHY:** Ensure the pipeline execution logic correctly handles missing or inactive modules, differentiating between optional and required steps.
**WHAT:**
-   Modify the loop that iterates through pipeline steps for a given phase.
-   Before calling the executor function (`exec_fn`), attempt to get the module for the step using `pipeline_get_step_module`.
-   If a required module is missing or inactive (`step->optional == false`), log an ERROR and return an appropriate error status (e.g., `MODULE_STATUS_MODULE_NOT_FOUND`) to halt execution for that phase/halo.
-   If an optional module is missing or inactive (`step->optional == true`), log a WARNING and continue to the next step.
-   If the module exists, check if it supports the current `phase` using `module->phases`. If not, skip the step (log DEBUG).
-   If the module exists and supports the phase, call the executor function (`exec_fn`).
-   Handle the status returned by the executor: propagate errors for required steps, log warnings for optional steps.

```c
// Inside the function that executes steps for a given phase (e.g., pipeline_execute_phase)
int result = MODULE_STATUS_SUCCESS;
for (int i = 0; i < pipeline->num_steps; i++) {
    struct pipeline_step *step = &pipeline->steps[i];

    if (!step->enabled) continue;

    struct base_module *module = NULL;
    void *module_data = NULL;
    int module_status = pipeline_get_step_module(step, &module, &module_data);

    if (module_status != 0 || module == NULL) {
        if (!step->optional) {
            LOG_ERROR("Required module for step '%s' (type %s) not found or inactive.",
                     step->step_name, module_type_name(step->type));
            result = MODULE_STATUS_MODULE_NOT_FOUND;
            break; // Halt execution for this phase
        } else {
            LOG_WARNING("Optional module for step '%s' (type %s) not found or inactive. Skipping step.",
                       step->step_name, module_type_name(step->type));
            continue; // Skip this optional step
        }
    }

    // Check if module supports the current phase
    if (!(module->phases & phase)) {
        LOG_DEBUG("Module '%s' does not support phase %d. Skipping step '%s'.",
                 module->name, phase, step->step_name);
        continue;
    }

    // Execute the step using the executor
    int status = exec_fn(step, module, module_data, context); // exec_fn is physics_step_executor

    // Handle execution status
    if (status != 0) {
        if (step->optional) {
            LOG_WARNING("Optional step '%s' failed with status %d. Continuing execution.",
                       step->step_name, status);
        } else {
            LOG_ERROR("Required step '%s' failed with status %d. Halting phase execution.",
                     step->step_name, status);
            result = status; // Propagate the error code
            break; // Exit the loop over steps
        }
    }
}
return result;
```

### Step 2.4: Update Core Galaxy Accessors (Revised)
**WHERE:** `src/core/core_galaxy_accessors.c` and `src/core/core_galaxy_accessors.h`
**WHY:** Provide a minimal, consistent way for the *core infrastructure* to access *essential, non-physics* galaxy properties, removing physics knowledge from the core.
**WHAT:**
-   **2.4.1:** Remove the global `use_extension_properties` flag and its usage entirely.
-   **2.4.2:** Implement accessor functions (getters/setters) in `core_galaxy_accessors.c/h` *only* for essential core properties needed by SAGE infrastructure (pipeline, mergers, I/O). Examples:
    -   `GalaxyIndex`, `CentralGalaxyIndex`
    -   `Type`
    -   `Pos`, `Vel`
    -   `HaloNr`, `SnapNum`
    -   `mergeType`, `mergeIntoID`, `mergeIntoSnapNum`
    -   *Do not* add accessors for physics properties (`Mvir`, `StellarMass`, `ColdGas`, `Metals*`, `Sfr*`, `Cooling`, etc.).
-   **2.4.3:** Implement these core accessors to directly get/set fields in the `struct GALAXY`.
-   **2.4.4:** Update core SAGE code (e.g., `core_save.c`, `core_build_model.c` for merger logic) to use these core accessors instead of direct struct field access where appropriate. Physics modules will manage their own properties.

```c
// In core_galaxy_accessors.h
// Example core accessor declarations
int galaxy_get_type(const struct GALAXY *galaxy);
void galaxy_set_type(struct GALAXY *galaxy, int type);
long long galaxy_get_galaxy_index(const struct GALAXY *galaxy);
void galaxy_set_galaxy_index(struct GALAXY *galaxy, long long index);
// ... declarations for other CORE properties ONLY ...

// In core_galaxy_accessors.c
// Example core accessor implementations
int galaxy_get_type(const struct GALAXY *galaxy) {
    if (!galaxy) return -1; // Or some error indicator
    return galaxy->Type;
}
void galaxy_set_type(struct GALAXY *galaxy, int type) {
    if (!galaxy) return;
    galaxy->Type = type;
}
// ... implementations for other CORE properties ONLY ...

// REMOVE the global use_extension_properties variable and all its checks.
```

### Step 2.5: Update Module System Initialization
**WHERE:** `src/core/core_init.c`
**WHY:** Ensure the module system is configured correctly at startup, including auto-discovery and activation based on configuration or defaults.
**WHAT:**
-   Modify `initialize_module_system` to:
    -   Call `module_system_configure` using `run_params`.
    -   If discovery is enabled (`run_params->runtime.EnableModuleDiscovery`), call `module_discover`.
    -   Log the number of modules found or a warning if none are found/loaded.
    -   Implement logic to activate modules based on configuration (e.g., from a future JSON config) or default settings if no config is present.
-   Improve error handling for module loading failures during discovery.
-   Add validation checks for the module system state after initialization.

```c
// In core_init.c (Conceptual - see previous plan for more detail)
void initialize_module_system(struct params *run_params) {
    // ... initialize module system ...
    // ... configure module system ...
    // ... discover modules if enabled ...
    // ... activate modules based on config/defaults ...
    // ... validate state ...
}
```

### Step 2.6: Update Evolve Galaxies Loop
**WHERE:** `src/core/core_build_model.c`
**WHY:** Ensure the main galaxy evolution loop *exclusively* uses the pipeline system and its phases, removing any direct calls to legacy physics functions.
**WHAT:**
-   **2.6.1:** Before the main loop over `STEPS`, execute the `PIPELINE_PHASE_HALO` using `pipeline_execute_phase`. Handle errors.
-   **2.6.2:** Inside the main loop over `STEPS`:
    -   Set the current `step` in the `pipeline_ctx`.
    -   **2.6.3:** Inside the loop over galaxies (`for p = 0 to ctx.ngal`):
        -   Set `pipeline_ctx.current_galaxy = p`.
        -   Skip merged galaxies.
        -   Execute the `PIPELINE_PHASE_GALAXY` using `pipeline_execute_phase`. Handle errors.
    -   **2.6.4:** After the galaxy loop, execute the `PIPELINE_PHASE_POST` using `pipeline_execute_phase`. Handle errors.
-   **2.6.5:** After the main loop over `STEPS`, execute the `PIPELINE_PHASE_FINAL` using `pipeline_execute_phase`. Handle errors.
-   Remove *all* direct calls to physics functions (e.g., `infall_recipe_compat`, `cooling_recipe_compat`, etc.).

```c
// Inside evolve_galaxies function in core_build_model.c
// (Conceptual - see previous plan for more detailed example)
// ... setup contexts ...
// ... execute HALO phase ...
// Loop over STEPS {
//   Loop over galaxies {
//     ... skip merged ...
//     ... execute GALAXY phase ...
//   }
//   ... execute POST phase ...
//   ... process merger queue (if still managed here, or handled by POST phase module) ...
// }
// ... execute FINAL phase ...
// ... final normalizations (if not handled by FINAL phase modules) ...
```

## Step 3: Module API Refinement

### Step 3.1: Define Standard Physics Module Interface
**WHERE:** Create `src/physics/physics_module_interface.h`
**WHY:** Establish a clear, richer API contract for all physics modules, including parameters and dependencies.
**WHAT:**
-   Define a standard interface structure (`physics_module_interface`) that inherits from `base_module`.
-   Include function pointers for all execution phases (`execute_*_phase`).
-   Add function pointers for module-specific parameter handling (`configure_parameters`, `get_parameter`).
-   Add function pointer for declaring dependencies (`declare_dependencies`).
-   Document required function signatures, expected behaviors, and return codes.

```c
// In src/physics/physics_module_interface.h (Conceptual - see previous plan for detail)
struct physics_module_interface {
    struct base_module base;
    int (*execute_halo_phase)(...);
    int (*execute_galaxy_phase)(...);
    int (*execute_post_phase)(...);
    int (*execute_final_phase)(...);
    int (*configure_parameters)(...);
    int (*get_parameter)(...);
    int (*declare_dependencies)(...);
};
```

### Step 3.1.1: Refactor Migrated Modules
**WHERE:** `src/physics/cooling_module.c/h`, `src/physics/infall_module.c/h`
**WHY:** Ensure the already migrated modules conform to the new, richer `physics_module_interface`.
**WHAT:**
-   Update the module interface structures to use `struct physics_module_interface`.
-   Implement the new required functions (stubs initially): `configure_parameters`, `get_parameter`, `declare_dependencies`.
-   Map existing phase logic to the appropriate `execute_*_phase` functions.

### Step 3.2: Implement Module Callback System
**WHERE:** `src/core/core_module_callback.c` and `src/core/core_module_callback.h`
**WHY:** Enable standardized, controlled communication *between* modules, respecting declared dependencies.
**WHAT:**
-   Enhance the callback system to fully support dependency declarations.
-   Implement `module_invoke` to check dependencies before allowing calls.
-   Add robust error handling and circular dependency detection.
-   Add debug logging for module interactions.

### Step 3.2.1: Add Callback Integration Tests
**WHERE:** `tests/test_module_callback_integration.c` (New File), `tests/Makefile.test`
**WHY:** Verify the callback system works correctly for inter-module communication.
**WHAT:**
-   Create tests with multiple mock modules calling each other.
-   Test dependency checks (success and failure cases).
-   Test circular dependency detection.
-   Test error propagation.

### Step 3.3: Update Module Template Generator
**WHERE:** `src/core/core_module_template.c`
**WHY:** Ensure new modules generated by the template conform to the refined `physics_module_interface`.
**WHAT:**
-   Update template generation to use `struct physics_module_interface`.
-   Include stubs for all required functions (phases, params, dependencies).
-   Add examples for declaring dependencies and registering parameters.

### Step 3.4: Implement Module Configuration Functions
**WHERE:** `src/core/core_config_system.c`
**WHY:** Allow runtime configuration of module parameters via a structured configuration file (e.g., JSON).
**WHAT:**
-   Implement functions to load module configurations from JSON.
-   Add logic to bind loaded parameters to modules via `configure_parameters`.
-   Ensure the config system activates modules based on the configuration.

### Step 3.4.1: Define Configuration Structure
**WHERE:** `docs/config_structure.md` (New File), `src/core/core_config_system.c`
**WHY:** Clearly define how modules and their parameters will be specified in the configuration file.
**WHAT:**
-   Document the proposed JSON structure for configuring modules, parameters, and the pipeline.
-   Clarify interaction with the legacy `.par` file (e.g., JSON for structure/modules, `.par` for core physics/cosmology).
-   Update `config_configure_modules` to parse this defined structure.

### Step 3.5: Update Module Registry
**WHERE:** `src/core/core_module_system.c`
**WHY:** Ensure proper module registration, activation, and dependency handling based on the refined API and configuration.
**WHAT:**
-   Update `module_register` if needed for the new interface.
-   Enhance `module_set_active` to perform dependency checks using `declare_dependencies`.
-   Add validation checks for registry consistency.

### Step 3.6: Implement Module Parameter System (Refined)
**WHERE:** Module files (`src/physics/*.c`) and potentially `core_module_parameter.c/h` (if helpers are kept).
**WHY:** Solidify the move of parameter handling *into* modules, removing reliance on core parameter views.
**WHAT:**
-   Ensure all parameter view usage is removed from core code.
-   Implement parameter registration (`module_register_parameter_with_module`) and validation *within* each module's `initialize` function.
-   Modules access parameters via their `module_data` struct or potentially `module_get_*_parameter` helpers.
-   Remove `core_parameter_views.c/h` files once confirmed obsolete.

## Step 4: Event System, Serialization, and Validation

### Step 4.1: Update Event System Integration
**WHERE:** `src/core/core_event_system.c` and `src/core/core_event_system.h`
**WHY:** Make the event system fully module-driven, removing core-level event handling.
**WHAT:**
-   Remove any remaining core-level event handlers.
-   Ensure modules register their own event handlers during `initialize`.
-   Add standard event emission points within the pipeline execution logic.
-   Ensure proper event propagation and error handling.

### Step 4.1.1: Add Event System Integration Tests
**WHERE:** `tests/test_event_system_integration.c` (New File), `tests/Makefile.test`
**WHY:** Verify modules can correctly register, emit, and handle events.
**WHAT:**
-   Test event emission from one module and handling by another.
-   Test event propagation flags and custom events.

### Step 4.2: Implement Module Data Serialization (Revised)
**WHERE:** `src/io/io_property_serialization.c`, `src/io/io_property_serialization.h`, I/O Handlers (`io_binary_output.c`, `io_hdf5_output.c`), Module Interface (`physics_module_interface.h`)
**WHY:** Ensure module-specific galaxy properties are saved/loaded correctly via a callback mechanism, keeping the core I/O agnostic.
**WHAT:**
-   **4.2.1:** Add `serialize_properties` and `deserialize_properties` function pointers to `physics_module_interface`.
    ```c
    // In physics_module_interface.h
    struct physics_module_interface {
        // ... other fields ...
        // Return size written/read or negative on error
        int (*serialize_properties)(void *module_data, int gal_idx, void *buffer, size_t buffer_size);
        int (*deserialize_properties)(void *module_data, int gal_idx, const void *buffer, size_t buffer_size);
        // Optional: function to report required buffer size per galaxy
        size_t (*get_serialized_size)(void *module_data);
    };
    ```
-   **4.2.2:** Update `property_serialization_context` to store information about *which module* owns each property, potentially linking `serialized_property_meta` back to a `module_id`.
-   **4.2.3:** Modify I/O handlers (`io_binary_output.c`, `io_hdf5_output.c`):
    -   During writing: Iterate through *active, serializable* modules. For each module, call its `serialize_properties` function, providing a buffer segment. The module writes its own data (likely its extension properties) into the buffer. The I/O handler writes the complete buffer segment.
    -   During reading: Iterate through modules listed in the file's property header. For each module, call its `deserialize_properties` function, providing the relevant buffer segment. The module reads its data and populates the galaxy's extensions.
-   **4.2.4:** Update `property_serialization_create_header` and `property_serialization_parse_header` to include module ownership information for each property.
-   **4.2.5:** Ensure backward compatibility for loading files without extended properties or from specific modules.

### Step 4.3: Create Empty Module Test
**WHERE:** `tests/test_empty_modules.c` (New File), `tests/Makefile.test`
**WHY:** Verify the core SAGE framework runs correctly and produces valid (but empty) output when *no* physics modules are loaded or active.
**WHAT:**
-   Create a test setup that initializes SAGE core systems but does not load or activate any physics modules.
-   Run the main evolution loop.
-   Verify no errors/crashes and valid, empty output files.

### Step 4.4: Create Module Loading Test
**WHERE:** `tests/test_module_loading.c` (New File), `tests/Makefile.test`
**WHY:** Verify the module loading, registration, activation, and dependency resolution mechanisms.
**WHAT:**
-   Create mock module manifests and dummy library files.
-   Configure SAGE to discover modules.
-   Run initialization and discovery.
-   Verify correct module registration, activation (based on dependencies), and error handling for failures.

### Step 4.5: Create Pipeline Execution Test
**WHERE:** `tests/test_pipeline_execution.c` (New File), `tests/Makefile.test`
**WHY:** Verify the pipeline system correctly executes modules in the defined order and phases.
**WHAT:**
-   Create mock modules implementing different phases.
-   Define a test pipeline.
-   Run a simplified `evolve_galaxies`-like loop calling `pipeline_execute_phase`.
-   Verify modules are called correctly based on phase support and order.
-   Test error handling for required vs. optional steps.

### Step 4.6: Update Makefile Tests
**WHERE:** `tests/Makefile.test` (or main Makefile)
**WHY:** Ensure all new tests are built and run as part of the standard testing process.
**WHAT:**
-   Add build rules for all new test files.
-   Add new test executables to the `make tests` target.
-   Ensure tests run correctly in CI/CD.

## Step 5: Documentation and Future Extensibility

### Step 5.1: Update Module Development Guide
**WHERE:** `docs/module_development.md`
**WHY:** Provide comprehensive guidance for developers creating new modules using the refined framework.
**WHAT:**
-   Document the standard `physics_module_interface`.
-   Explain phase handlers, parameter handling, dependency declaration, and serialization callbacks.
-   Document the module callback system (`module_invoke`).
-   Provide examples.

### Step 5.2: Update Architecture Documentation
**WHERE:** `docs/architecture.md`
**WHY:** Document the new, fully modular core architecture.
**WHAT:**
-   Describe the pipeline system, module system (discovery, loading, lifecycle), extension system, callback system, event system, configuration system, and serialization mechanism.
-   Provide diagrams.
-   Clarify JSON + `.par` config interaction.

### Step 5.3: Update User Guide
**WHERE:** `docs/user_guide.md`
**WHY:** Help users configure and run the model.
**WHAT:**
-   Document module configuration (JSON).
-   Explain enabling/disabling modules and setting parameters.
-   Provide examples.

### Step 5.4: Future Extensibility Guidelines
**WHERE:** `docs/future_extensibility.md`
**WHY:** Document how the system can be extended.
**WHAT:**
-   Explain adding new physics processes, pipeline phases, output formats, and module types.

## Conclusion

This revised plan focuses on creating a truly physics-agnostic SAGE core. By removing legacy paths, refining the module API with parameter/dependency handling, implementing a controlled callback system, ensuring module-driven serialization, and removing physics knowledge from core accessors, we establish a robust foundation. This prepares SAGE for the subsequent migration of legacy physics into this new modular framework, ultimately achieving the goal of runtime functional modularity. The plan includes necessary testing and documentation updates to support this significant architectural shift. The subdivision of larger steps should aid in incremental implementation and testing.