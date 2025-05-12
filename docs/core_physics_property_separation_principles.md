# Core-Physics Property Separation Principles (May 2025)

The complete separation between core infrastructure and physics has been implemented to ensure a modular and maintainable SAGE codebase. This separation is primarily achieved through a well-defined property system.

## Key Principles:

1.  **Core Properties (`core_properties.yaml`)**:
    *   These properties are fundamental to the SAGE infrastructure (e.g., halo properties, galaxy identifiers, positions, velocities).
    *   They are defined in `src/core_properties.yaml`.
    *   Core code (typically in `src/core/` and `src/io/`) can directly access these properties using the `GALAXY_PROP_*` macros generated from `core_properties.yaml`.
    *   These properties are managed by the core infrastructure and are expected to always be present.

2.  **Physics Properties (e.g., `properties.yaml`, module-specific YAMLs)**:
    *   These properties are specific to particular physical processes (e.g., `HotGas`, `ColdGas`, `StellarMass`, `BlackHoleMass`).
    *   They are typically defined in `src/properties.yaml` or in YAML files specific to individual physics modules.
    *   Access to these properties from ANY part of the code (including physics modules themselves and core I/O routines) **MUST** be done through the generic property system utility functions found in `src/core/core_property_utils.h` (e.g., `get_float_property()`, `get_int32_property()`, `has_property()`).
    *   These properties may or may not be present depending on which physics modules are loaded and active in a given simulation.

3.  **Access Rules & Rationale**:
    *   **Core code MUST NOT use `GALAXY_PROP_*` macros for physics properties.** This is the cornerstone of the separation. Core infrastructure should have no compile-time or direct runtime knowledge of specific physics implementations or the properties they manage.
    *   **All code (core and physics modules) SHOULD use the generic property accessors (e.g., `get_float_property()`, `get_cached_property_id()`) when dealing with properties that are not strictly part of the `core_properties.yaml` definition.** This ensures that the code can adapt to different sets of available physics properties at runtime.
    *   Physics modules can use `GALAXY_PROP_*` macros for core properties if needed, as these are considered part of the stable infrastructure interface.

## Benefits:

*   **Modularity**: Physics modules can be developed, added, or removed without requiring changes to the core SAGE infrastructure.
*   **Flexibility**: Simulations can be run with varying sets of physics modules, and the I/O system can adapt to output only the available properties.
*   **Maintainability**: Clear separation of concerns makes the codebase easier to understand, debug, and extend.
*   **Reduced Coupling**: Core systems like HDF5 output are no longer tightly coupled to specific physics property names or data types.

This separation ensures that the SAGE core infrastructure remains robust and independent of the specific scientific models being implemented, paving the way for more complex and varied physics implementations in the future.
