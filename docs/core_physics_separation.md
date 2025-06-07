# Core-Physics Separation in SAGE

This document describes the architecture that achieves complete independence between core infrastructure and physics modules in the SAGE model. This separation allows the core to run without any physics-specific knowledge, while physics modules are implemented as pure add-ons.

## Architecture Overview

### Core Infrastructure

The core infrastructure is now completely physics-agnostic:

1. **Metadata-Driven Properties**: properties.yaml defines all galaxy properties with core/physics separation
2. **Metadata-Driven Parameters**: parameters.yaml defines all runtime parameters with physics-agnostic core
3. **Clean GALAXY struct**: Removed physics-dependent fields, keeping only core infrastructure fields
4. **Pipeline Execution**: Uses a purely abstract execution model with phases (HALO, GALAXY, POST, FINAL)
5. **Module System**: Allows physics modules to register themselves at runtime
6. **No direct field synchronization**: Removed synchronization between direct fields and properties
7. **Configuration-based**: Physics modules and pipeline steps are defined in configuration files
8. **Separated Event Systems**: Core infrastructure events and physics events are completely separated

### Physics Modules

Physics modules are now implemented as pure add-ons:

1. **Self-contained**: Each module contains its own implementation
2. **Registration**: Modules register themselves with the core at runtime
3. **Property access**: Modules use GALAXY_PROP_* macros to access properties
4. **Phase support**: Modules declare which pipeline phases they support
5. **No core dependencies**: Modules don't depend on core infrastructure details
6. **Independent Events**: Physics modules use dedicated physics event system (`src/physics/physics_events.h`)

### Event System Architecture

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

This separation enables:
- **Core Infrastructure Monitoring**: Track pipeline performance and system health
- **Physics Module Communication**: Enable inter-module physics interactions

## Parameter System Architecture

The parameter system achieves complete core-physics separation through metadata-driven design:

### Core Parameter Infrastructure

The core parameter reading system (`src/core/core_read_parameter_file.c`) is completely physics-agnostic:

```c
/* Physics-agnostic parameter processing */
parameter_id_t param_id = get_parameter_id(parameter_name);
if (param_id != PARAM_COUNT) {
    if (set_parameter_from_string(run_params, param_id, value) != 0) {
        LOG_ERROR("Failed to set parameter '%s' to value '%s'", parameter_name, value);
    }
}
```

Key architectural features:
- **No Hardcoded Physics Knowledge**: Core code uses generic parameter accessors
- **Type-Safe Parameter Setting**: Auto-generated validation for all parameter types
- **Enum Validation**: Special handling for TreeType and ForestDistributionScheme
- **Backward Compatibility**: Maintains existing `.par` file format

### Parameter Categories

Parameters are organized into clear categories in `src/parameters.yaml`:

```yaml
parameters:
  core:                    # Always available to core infrastructure
    io: [FileNameGalaxies, OutputDir, TreeType, ...]
    simulation: [LastSnapshotNr, NumSnapOutputs, ...]
    cosmology: [Hubble_h, Omega, OmegaLambda, ...]
    units: [UnitVelocity_in_cm_per_s, ...]
    runtime: [ForestDistributionScheme, ...]
    
  physics:                 # Only used by physics modules
    general: [ThreshMajorMerger, RecycleFraction, ...]
    mergers: [DisruptionHandlerModuleName, ...]
    gas_physics: [BaryonFrac, ...]
    # ... additional physics categories
```

### Auto-Generated Parameter System

The parameter system files are auto-generated from `parameters.yaml`:

- **`core_parameters.h`**: Parameter ID enumeration and accessor function declarations
- **`core_parameters.c`**: Parameter metadata and type-safe setter implementations

This ensures:
- **Compile-Time Safety**: Parameter IDs are validated at compile time
- **Runtime Validation**: Parameter values are validated with bounds checking
- **Extensibility**: New parameters can be added without modifying core code
- **Runtime Adaptability**: Support any combination of physics modules

## Testing Core-Physics Separation

Core-physics separation is validated through multiple focused tests:

- `test_property_access_patterns` - Validates property access follows separation principles
- `test_property_system_hdf5` - Tests property system integration with I/O separation  
- `test_evolve_integration` - Tests core pipeline works independently of physics
- `test_core_pipeline_registry` - Tests physics-agnostic pipeline registry
- `test_physics_free_mode` - Tests core runs with empty pipelines (zero modules)

These tests collectively verify that:

1. The core can run with completely empty pipelines
2. The pipeline executes all phases correctly with zero modules
3. Properties pass from input to output without physics
4. Essential physics functions provide minimal implementations

## Running in Physics-Free Mode

To run SAGE with a minimal empty pipeline:

```bash
# Run the physics-free mode test (integrated into main Makefile)
make test_physics_free_mode
./tests/test_physics_free_mode

# Or run as part of core infrastructure tests
make core_tests
```

The test validates core-physics separation by running the SAGE core infrastructure with empty pipelines that perform no physics calculations.

## Verifying Core Independence

To verify that the core is truly independent from physics:

1. **Run the physics-free mode test**: `make test_physics_free_mode && ./tests/test_physics_free_mode`
2. **Run core infrastructure tests**: `make core_tests` 
3. **Run property system tests**: `make property_tests`
4. **Run module system tests**: `make module_tests`

These tests collectively demonstrate that the core infrastructure operates independently of any physics implementation.

## Benefits

This architecture provides several advantages:

1. **Modularity**: Physics modules can be added, removed, or replaced without affecting the core
2. **Testing**: Core functionality can be tested independently from physics
3. **Development**: New physics modules can be developed without modifying the core
4. **Memory efficiency**: Only required properties are allocated
5. **Runtime configuration**: Different physics pipelines can be selected at runtime
