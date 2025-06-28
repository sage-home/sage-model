# SAGE Architecture Guide

## Overview

SAGE (Semi-Analytic Galaxy Evolution) implements a **physics-agnostic core with modular physics extensions** architecture. This document provides a comprehensive overview of the architectural principles, components, and implementation strategies that enable SAGE's modular design.

## Core Architectural Principle

**Physics-Agnostic Core Infrastructure + Runtime-Configurable Physics Modules**

```
Physics Modules (Cooling, Star Formation, Feedback, Mergers, etc.)
    ↓
Property System (Dynamic property generation from YAML)
    ↓  
Pipeline System (HALO → GALAXY → POST → FINAL execution phases)
    ↓
Core Infrastructure (Memory, I/O, Events, Module System, Configuration)
```

## Core-Physics Separation

### Separation Principles

The complete separation between core infrastructure and physics has been successfully implemented and validated to ensure a modular and maintainable SAGE codebase.

**Key Principles:**

1. **Core Properties** (`properties.yaml` with `is_core: true`):
   - Fundamental to SAGE infrastructure (halo properties, galaxy identifiers, positions, velocities)
   - Core code can directly access using `GALAXY_PROP_*` macros
   - Always present and managed by core infrastructure

2. **Physics Properties** (`properties.yaml` with `is_core: false`):
   - Specific to physics processes (HotGas, ColdGas, StellarMass, BlackHoleMass)
   - **Must** use generic property accessors (`get_float_property()`, etc.) from ANY code
   - May or may not be present depending on loaded physics modules

3. **Access Rules**:
   - **Core code MUST NOT use `GALAXY_PROP_*` macros for physics properties**
   - **All code SHOULD use generic property accessors for physics properties**
   - Physics modules can use `GALAXY_PROP_*` macros for core properties

### Implementation Example

```c
// Core properties - direct macro access allowed in core code
GALAXY_PROP_Mvir(galaxy) = 1.5e12f;
int64_t index = GALAXY_PROP_GalaxyIndex(galaxy);
GALAXY_PROP_merged(galaxy) = 0;  // Core property for lifecycle management

// Physics properties - generic accessors only
property_id_t mergtime_id = get_property_id("MergTime");
if (has_property(galaxy, mergtime_id)) {
    set_float_property(galaxy, mergtime_id, 5.2f);
    float mergtime = get_float_property(galaxy, mergtime_id, 999.9f);
}
```

### Benefits

- **Modularity**: Physics modules can be added, removed, or replaced without affecting core
- **Testing**: Core functionality can be tested independently from physics
- **Development**: New physics modules without modifying core
- **Memory efficiency**: Only required properties are allocated
- **Runtime configuration**: Different physics pipelines selectable at runtime

## Property System

### Metadata-Driven Design

SAGE uses an explicit property build system that controls which galaxy properties are compiled at build-time, optimizing memory usage and execution performance.

**Build Targets:**
```bash
make physics-free      # Core properties only (fastest, minimal memory)
make full-physics      # All properties (maximum compatibility)
make custom-physics CONFIG=file.json  # Properties for specific modules
```

**Property Categories:**
- **Core Properties**: Always available, direct access via `GALAXY_PROP_*` macros
- **Physics Properties**: Module-dependent, generic accessor access only

### Type-Safe Property Access

The property system uses auto-generated type-safe dispatcher functions replacing flawed macro-based approaches:

```c
// Auto-generated dispatcher functions
float get_generated_float(const galaxy_properties_t *props, property_id_t prop_id, float default_value);
int32_t get_generated_int32(const galaxy_properties_t *props, property_id_t prop_id, int32_t default_value);
```

**Benefits:**
- Type safety through specific dispatcher functions
- Correct memory access via direct struct field access
- Centralized error handling and validation
- Better performance than pointer arithmetic approaches

## Module System

### Components Overview

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

### Base Module Interface

```c
struct base_module {
    // Metadata
    char name[MAX_MODULE_NAME];
    char version[MAX_MODULE_VERSION];
    char author[MAX_MODULE_AUTHOR];
    int module_id;
    enum module_type type;
    
    // Lifecycle functions
    int (*initialize)(struct params *params, void **module_data);
    int (*cleanup)(void *module_data);
    int (*configure)(void *module_data, const char *config_name, const char *config_value);
    
    // Phase execution functions
    int (*execute_halo_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_galaxy_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_post_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_final_phase)(void *module_data, struct pipeline_context *context);
    
    // Dependencies and phases
    uint32_t phases;  // Bitmap of supported phases
};
```

### Module Registration and Lifecycle

1. **Registration**: Module registered with module system via constructor attributes
2. **Factory Registration**: Module factory registered with pipeline system
3. **Pipeline Creation**: Module instances created by factories based on configuration
4. **Initialization**: Module's initialize function called with parameters
5. **Execution**: Phase-specific functions called during pipeline execution
6. **Cleanup**: Module's cleanup function called before termination

### Configuration-Driven Pipeline Creation

JSON configuration files determine which modules to activate:

```json
{
    "modules": {
        "instances": [
            {
                "name": "cooling_module",
                "enabled": true,
                "some_parameter": 1.5
            }
        ]
    }
}
```

## Pipeline System

### Four-Phase Execution Model

SAGE uses bit flags for phase identification:

```c
#define PIPELINE_PHASE_HALO   (1U << 0)  // Executed once per halo
#define PIPELINE_PHASE_GALAXY (1U << 1)  // Executed for each galaxy
#define PIPELINE_PHASE_POST   (1U << 2)  // Post-processing after galaxy calculations
#define PIPELINE_PHASE_FINAL  (1U << 3)  // Final processing for output
```

**Benefits:**
- Module-agnostic execution context
- Merger event queue for deferred processing
- Phase-specific module callbacks
- Runtime configurability

### Galaxy Processing Modes

SAGE supports two distinct galaxy processing modes, selectable at runtime via the `ProcessingMode` parameter:

1.  **Snapshot-Based Processing (Default)**: Evolves galaxies snapshot by snapshot. This mode is computationally efficient but can be less accurate in scenarios with complex merger trees.
2.  **Tree-Based Processing**: Evolves galaxies by traversing the entire merger tree. This mode is more scientifically accurate, especially for handling orphans and gaps in the merger tree, but can be more computationally intensive.

---

### Snapshot-Based FOF Processing

When `ProcessingMode` is set to `0` (or is omitted), SAGE uses a **pure snapshot-based FOF (Friends-of-Friends) processing model**. This architecture ensures optimal performance by processing all galaxies within a snapshot before moving to the next.

#### Processing Flow

```
For each snapshot:
  For each FOF group in the snapshot:
    1. FOF Group Assembly     → Inherit all progenitor galaxies into a single FOF galaxy list.
    2. Type Classification    → Assign Type 0 (central), Type 1 (satellite), Type 2 (orphan) status.
    3. FOF Group Evolution   → Execute 4-phase physics pipeline on the entire FOF group.
    4. Snapshot Attachment   → Add surviving galaxies to the final output list.
  
  Save all galaxies for the snapshot.
```

#### Key Design Principles

1. **FOF-Centric Evolution**: The FOF group is the fundamental unit of evolution.
2. **Single Central Assignment**: Exactly one Type 0 central galaxy per FOF group.
3. **Consistent Timing**: All galaxies within an FOF group use a consistent timestep (`deltaT`).
4. **Memory Optimization**: Direct append to FOF arrays minimizes memory operations.

#### Implementation Components

- **`process_fof_group()`**: Main entry point for FOF group processing.
- **`evolve_galaxies()`**: Four-phase physics pipeline executor.
- **`copy_galaxies_from_progenitors()`**: Optimized progenitor inheritance.
- **`identify_and_process_orphans()`**: Forward-looking orphan detection for mass conservation.
- **`GalaxyArray`**: Memory-safe dynamic arrays.

#### Orphan Galaxy Tracking

Snapshot-based mode uses **forward-looking orphan detection** to ensure mass conservation. It identifies unprocessed galaxies from the previous snapshot and reclassifies them as Type 2 orphans, assigning them to the FOF group of their descendant halo.

#### Benefits

- **Performance**: Optimized for speed with reduced memory allocations.
- **Architectural Consistency**: A snapshot-based outer loop matches the inner processing.
- **Maintainability**: Clear separation of concerns and robust error handling.

---

### Tree-Based Processing

When `ProcessingMode` is set to `1`, SAGE uses a **tree-based processing model** that traverses the entire merger tree history for each galaxy. This approach provides a more accurate treatment of galaxy evolution, especially in cases of halo disruption or gaps in the tree.

#### Processing Flow

```
For each forest (collection of trees):
  1. Tree Traversal         → Perform a depth-first traversal of the merger tree.
  2. Galaxy Inheritance     → Inherit galaxies from their progenitors, naturally handling orphans and gaps.
  3. FOF Group Assembly     → Collect all galaxies within an FOF group once all its progenitors are processed.
  4. Physics Application    → Execute the 4-phase physics pipeline on the assembled FOF group.
  5. Output Organization    → Collect all processed galaxies and organize them by snapshot for output.
```

#### Key Design Principles

1. **Scientific Accuracy**: Prioritizes correct galaxy evolution by following the full merger history.
2. **Natural Orphan Handling**: Orphans are naturally created when a progenitor's branch of the tree terminates.
3. **Gap Tolerance**: The depth-first traversal is resilient to missing snapshots in the merger tree.
4. **Modern Implementation**: Leverages SAGE's modern architecture, including the `GalaxyArray`, property system, and memory pools.

#### Implementation Components

- **`sage_tree_mode.h`**: Main entry point for tree-based processing.
- **`tree_context.h`**: Manages the state and data for tree processing.
- **`tree_traversal.h`**: Implements the depth-first traversal algorithm.
- **`tree_galaxies.h`**: Handles galaxy collection and inheritance.
- **`tree_fof.h`**: Manages FOF group processing within the tree.
- **`tree_physics.h`**: Integrates the physics pipeline.
- **`tree_output.h`**: Organizes galaxies for output.

#### Benefits

- **Scientific Accuracy**: Eliminates galaxy loss from halo disruptions and tree gaps.
- **Robustness**: More accurately models complex merger histories.
- **Maintainability**: A clean, modular implementation that is well-tested and documented.

### Physics-Free Mode

SAGE supports physics-free mode where core infrastructure runs with completely empty pipelines:

```bash
./sage millennium.par  # No config file = empty pipeline
```

This validates true core-physics separation where:
- No modules loaded whatsoever
- Core properties pass from input to output unchanged
- All pipeline phases execute gracefully
- Essential physics functions provide minimal implementations

## Parameter System

### Metadata-Driven Architecture

The parameter system achieves complete core-physics separation through auto-generated code from `parameters.yaml`:

**Core Parameter Infrastructure:**
- Physics-agnostic parameter processing
- Type-safe parameter setting with auto-generated validation
- Enum validation for special types (TreeType, ForestDistributionScheme)
- Backward compatibility with existing `.par` file format

**Parameter Categories:**
```yaml
parameters:
  core:                    # Always available to core infrastructure
    io: [FileNameGalaxies, OutputDir, TreeType, ...]
    simulation: [LastSnapshotNr, NumSnapOutputs, ...]
    cosmology: [Hubble_h, Omega, OmegaLambda, ...]
    
  physics:                 # Only used by physics modules
    general: [ThreshMajorMerger, RecycleFraction, ...]
    mergers: [DisruptionHandlerModuleName, ...]
```

### Auto-Generated Parameter System

Files generated from `parameters.yaml`:
- **`core_parameters.h`**: Parameter ID enumeration and accessor declarations
- **`core_parameters.c`**: Parameter metadata and type-safe setter implementations

## Event System Architecture

The event system maintains strict separation between core and physics concerns:

```
Core Event System (src/core/core_event_system.h)
├── PIPELINE_STARTED/COMPLETED     # Pipeline lifecycle
├── PHASE_STARTED/COMPLETED        # Phase transitions  
├── GALAXY_CREATED/COPIED/MERGED   # Galaxy lifecycle
└── MODULE_ACTIVATED/DEACTIVATED   # Module system events

Physics Event System (src/physics/physics_events.h)
├── COOLING_COMPLETED              # Physics processes
├── STAR_FORMATION_OCCURRED        # Physics calculations
├── FEEDBACK_APPLIED               # Physics interactions
└── [Custom Physics Events]        # Module-specific events
```

**Benefits:**
- **Core Infrastructure Monitoring**: Track pipeline performance and system health
- **Physics Module Communication**: Enable inter-module physics interactions

## Configuration System

### JSON-Based Runtime Configuration

The configuration system provides flexible runtime configuration without recompilation:

**Key Features:**
- Hierarchical configuration with dot-notation paths
- Type-safe value access with default values
- Command-line overrides for any configuration value
- Module-specific parameter configuration
- Memory management improvements (resolved memory corruption issues)

**Integration Points:**
- Module system integration for determining active modules
- Pipeline integration for execution order
- Parameter system integration for runtime parameter updates
- Command-line integration for configuration overrides

## Testing Architecture

### Test Categories

1. **Core Infrastructure Tests** (19 tests): Pipeline system, array utilities, diagnostics, evolution integration, memory management, configuration, core-physics separation
2. **Property System Tests** (7 tests): Property serialization, validation, HDF5 integration, array access, YAML validation
3. **I/O System Tests** (11 tests): I/O interface, endianness, format handlers, HDF5 output, buffering, validation
4. **Module System Tests** (3 tests): Pipeline invocation, module callback, module lifecycle

### Testing Philosophy

- **Separation of Concerns**: Tests categorized by component with clear boundaries
- **Evolution with Architecture**: Testing strategy evolves with architectural changes
- **Fail-Fast Development**: Tests fail early with clear guidance

## Development Guidelines

### Module Development

**Essential Components:**
1. **Base Module Structure**: Defines interface and metadata
2. **Module Data Structure**: Holds module-specific state
3. **Lifecycle Functions**: Initialize and cleanup
4. **Phase Execution Functions**: Implementation for supported phases
5. **Registration**: Constructor function for automatic registration
6. **Factory Function**: Creates and returns the module

**Input Validation Best Practices:**
- Always validate NULL pointers before dereferencing
- Use descriptive error messages with module name and parameter name
- Return appropriate MODULE_STATUS_* codes for different error conditions
- Handle edge cases gracefully

### Property Access Guidelines

1. **For Core Properties**: Direct access using `GALAXY_PROP_*` macros
2. **For Physics Properties**: Use generic accessors from `core_property_utils.h`

```c
// Core properties
int type = GALAXY_PROP_Type(galaxy);

// Physics properties
property_id_t prop_id = get_cached_property_id("ColdGas");
if (has_property(galaxy, prop_id)) {
    float cold_gas = get_float_property(galaxy, prop_id, 0.0f);
}
```

### Error Handling

SAGE provides multiple layers of error handling:
1. **Return Status Codes**: Standardized `MODULE_STATUS_*` constants
2. **Standard Logging**: Use logging system for immediate context
3. **Enhanced Error Context System**: Record detailed error information
4. **Parameter System Integration**: Leverage parameter system for validation

## Current Implementation Status

### ✅ Completed Achievements (June 2025)

**Core-Physics Separation:**
- Complete property migration with core vs physics categories
- Generic access compliance for all physics properties
- Core property system with "merged" lifecycle property
- Elimination of dual-state synchronization
- Function simplification using auto-generated property system

**Validation Results:**
- Physics-free mode successfully builds and runs
- No coupling violations between core and physics
- Property availability checking with `has_property()`
- Scientific accuracy maintained while achieving architectural separation

**Infrastructure Components:**
- Dual-layer parameter management (JSON + programmatic)
- Enhanced error tracking with detailed context
- Execution monitoring through callback infrastructure
- Configuration-driven activation allowing runtime module selection

## Future Development

The architecture provides a foundation for:
- Build-time module validation
- Automated property dependency analysis
- Performance profiling by property set
- Custom property subsets for specific science cases
- Complex inter-module communication for advanced physics