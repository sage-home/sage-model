# Today's Edits - 2025-05-17

## Core-Physics Separation: Pipeline Registry Refactoring (Phase 5.2.F)

This set of changes focuses on refactoring `core_pipeline_registry.c` to decouple it from specific physics module implementations, enabling a more dynamic, self-registering module system.

### Files Modified

- **`src/core/core_pipeline_registry.c`**:
    - Removed direct includes of placeholder physics module headers (`placeholder_infall_module.h`, `placeholder_cooling_module.h`).
    - Modified `pipeline_create_with_standard_modules()`:
        - It now iterates through `module_factories` (populated by self-registering modules).
        - For each factory, it creates the module instance.
        - It then adds the created module to the pipeline using `pipeline_add_step(pipeline, module->type, module->name, module->name, true, true)`. (Initially, `pipeline_add_module` was attempted, but it does not exist; `pipeline_add_step` is the correct function).
        - Success for `pipeline_add_step` is checked with `result == 0`.
        - Added extensive `LOG_DEBUG` messages for tracing module factory processing, module creation, and addition to the pipeline.
    - Modified `pipeline_register_standard_modules()`:
        - Added a `LOG_DEBUG` message to indicate it has been called and that module self-registration is active. This function is largely a placeholder now as modules register themselves via constructor functions.
- **`src/physics/placeholder_infall_module.c`**:
    - Added `#include "../core/core_pipeline_registry.h"` for `pipeline_register_module_factory`.
    - Defined `struct base_module *placeholder_infall_module_factory(void)` which returns a pointer to the module's static `struct base_module` instance (`placeholder_infall_module`).
    - Renamed `static void __attribute__((constructor)) register_module(void)` to `register_module_and_factory(void)`.
    - In `register_module_and_factory()`, added a call to `pipeline_register_module_factory(MODULE_TYPE_INFALL, "PlaceholderInfall", placeholder_infall_module_factory)` to register its factory function with the core pipeline registry.
- **`src/physics/placeholder_infall_module.h`**:
    - Added declaration: `struct base_module *placeholder_infall_module_factory(void);`
- **`src/physics/placeholder_cooling_module.c`**:
    - Added `#include "../core/core_pipeline_registry.h"` for `pipeline_register_module_factory`.
    - Defined `struct base_module *placeholder_cooling_module_factory(void)` which returns a pointer to the module's static `struct base_module` instance (`placeholder_cooling_module`).
    - Renamed `static void __attribute__((constructor)) register_module(void)` to `register_module_and_factory(void)`.
    - In `register_module_and_factory()`, added a call to `pipeline_register_module_factory(MODULE_TYPE_COOLING, "PlaceholderCooling", placeholder_cooling_module_factory)` to register its factory function with the core pipeline registry.
- **`src/physics/placeholder_cooling_module.h`**:
    - Added declaration: `struct base_module *placeholder_cooling_module_factory(void);`

### Changes Summary

1.  **Decoupled `core_pipeline_registry.c`**: Removed hardcoded dependencies on specific physics modules by eliminating direct header includes.
2.  **Implemented Module Self-Registration**:
    - Physics modules (`placeholder_infall_module`, `placeholder_cooling_module`) now define factory functions (`*_factory`).
    - These modules use constructor functions (`__attribute__((constructor))`) to call `pipeline_register_module_factory` at startup, registering their name, type, and factory function with `core_pipeline_registry.c`.
3.  **Dynamic Pipeline Population**: `pipeline_create_with_standard_modules()` now dynamically populates the pipeline by:
    - Iterating over the registered module factories.
    - Calling each factory to get a module instance.
    - Adding each module to the pipeline as a step using `pipeline_add_step`.
4.  **Enhanced Logging**: Added detailed `LOG_DEBUG` messages in `core_pipeline_registry.c` to trace the module registration and pipeline creation process.

### Next Steps
- Compile the codebase.
- Run tests to verify the new module registration and pipeline creation mechanism.
- Address any build or runtime issues.
