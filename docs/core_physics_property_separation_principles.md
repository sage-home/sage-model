# Core-Physics Property Separation Principles (June 2025)

The complete separation between core infrastructure and physics has been successfully implemented and validated to ensure a modular and maintainable SAGE codebase. This separation is achieved through a well-defined property system with strict access control patterns.

## Key Principles:

1.  **Core Properties (`properties.yaml` with `is_core: true`)**:
    *   These properties are fundamental to the SAGE infrastructure (e.g., halo properties, galaxy identifiers, positions, velocities).
    *   Core code (typically in `src/core/` and `src/io/`) can directly access these properties using the `GALAXY_PROP_*` macros generated from `properties.yaml`.
    *   These properties are managed by the core infrastructure and are expected to always be present.

2.  **Physics Properties (e.g., `properties.yaml` with `is_core: false`, module-specific YAMLs)**:
    *   These properties are specific to particular physical processes (e.g., `HotGas`, `ColdGas`, `StellarMass`, `BlackHoleMass`).
    *   They are typically defined in `src/properties.yaml` or in YAML files specific to individual physics modules.
    *   Access to these properties from ANY part of the code (including physics modules themselves and core I/O routines) **MUST** be done through the generic property system utility functions found in `src/core/core_property_utils.h` (e.g., `get_float_property()`, `get_int32_property()`, `has_property()`).
    *   These properties may or may not be present depending on which physics modules are loaded and active in a given simulation.

3.  **Access Rules & Rationale**:
    *   **Core code MUST NOT use `GALAXY_PROP_*` macros for physics properties.** This is the cornerstone of the separation. Core infrastructure should have no compile-time or direct runtime knowledge of specific physics implementations or the properties they manage.
    *   **All code (core and physics modules) SHOULD use the generic property accessors (e.g., `get_float_property()`, `get_cached_property_id()`) when dealing with properties that are not strictly part of the `properties.yaml` with `is_core: true` definition.** This ensures that the code can adapt to different sets of available physics properties at runtime.
    *   Physics modules can use `GALAXY_PROP_*` macros for core properties if needed, as these are considered part of the stable infrastructure interface.

## Implementation Example

See `tests/test_property_array_access.c` for working examples of proper separation:

```c
// Core properties - direct macro access allowed in core code
GALAXY_PROP_Mvir(galaxy) = 1.5e12f;
int64_t index = GALAXY_PROP_GalaxyIndex(galaxy);
GALAXY_PROP_merged(galaxy) = 0;  // New core property for lifecycle management

// Physics properties - generic accessors only (including from core code)
property_id_t mergtime_id = get_property_id("MergTime");
if (has_property(galaxy, mergtime_id)) {
    set_float_property(galaxy, mergtime_id, 5.2f);
    float mergtime = get_float_property(galaxy, mergtime_id, 999.9f);
}

// Example of availability checking for physics properties
property_id_t coldgas_id = get_property_id("ColdGas"); 
if (has_property(galaxy, coldgas_id)) {
    set_float_property(galaxy, coldgas_id, 2.5e10f);
}
```

## Implementation Status

### ✅ Successfully Implemented (June 2025)

The core-physics property separation principles have been fully implemented and validated:

#### Achieved Milestones
- **Complete Property Migration**: All physics properties (including MergTime) moved from core to physics category
- **Generic Access Compliance**: All core code now uses generic property accessors for physics properties  
- **Core Property System**: Added "merged" core property for clean galaxy lifecycle management
- **Elimination of Dual-State**: Removed dangerous synchronization between direct fields and properties
- **Function Simplification**: Core functions now trust auto-generated property system for all data management

#### Validation Results
- **Physics-Free Mode**: Successfully builds and runs with `make physics-free` targeting core properties only
- **No Coupling Violations**: Core infrastructure operates independently of physics property definitions
- **Property Availability**: All physics property access includes proper availability checking with `has_property()`
- **Scientific Accuracy**: Maintains exact scientific algorithms while achieving complete architectural separation

## Benefits:

*   **Modularity**: Physics modules can be developed, added, or removed without requiring changes to the core SAGE infrastructure.
*   **Flexibility**: Simulations can be run with varying sets of physics modules, and the I/O system can adapt to output only the available properties.
*   **Maintainability**: Clear separation of concerns makes the codebase easier to understand, debug, and extend.
*   **Reduced Coupling**: Core systems like HDF5 output are no longer tightly coupled to specific physics property names or data types.
*   **Runtime Adaptability**: Core infrastructure adapts to any combination of physics modules without recompilation.

This separation ensures that the SAGE core infrastructure remains robust and independent of the specific scientific models being implemented, enabling complex and varied physics implementations with complete modularity.
