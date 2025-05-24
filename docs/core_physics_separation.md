# Core-Physics Separation in SAGE

This document describes the architecture that achieves complete independence between core infrastructure and physics modules in the SAGE model. This separation allows the core to run without any physics-specific knowledge, while physics modules are implemented as pure add-ons.

## Architecture Overview

### Core Infrastructure

The core infrastructure is now completely physics-agnostic:

1. **Minimal properties.yaml**: Only contains core infrastructure properties
2. **Clean GALAXY struct**: Removed physics-dependent fields, keeping only core infrastructure fields
3. **Pipeline Execution**: Uses a purely abstract execution model with phases (HALO, GALAXY, POST, FINAL)
4. **Module System**: Allows physics modules to register themselves at runtime
5. **No direct field synchronization**: Removed synchronization between direct fields and properties
6. **Configuration-based**: Physics modules and pipeline steps are defined in configuration files
7. **Separated Event Systems**: Core infrastructure events and physics events are completely separated

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
- **Runtime Adaptability**: Support any combination of physics modules

## Testing Core-Physics Separation

Core-physics separation is validated through multiple focused tests:

- `test_property_access_patterns` - Validates property access follows separation principles
- `test_property_system_hdf5` - Tests property system integration with I/O separation  
- `test_evolve_integration` - Tests core pipeline works independently of physics
- `test_core_pipeline_registry` - Tests physics-agnostic pipeline registry
- `test_empty_pipeline` - Tests core can run with empty placeholder modules

These tests collectively verify that:

1. The core can run with minimal "placeholder" modules that do nothing
2. The pipeline executes all phases correctly
3. Galaxies are processed without any physics calculations
4. No direct field synchronization is required

## Running with the Empty Pipeline

To run SAGE with a minimal empty pipeline:

```bash
# Option 1: Using the test script (recommended)
./tests/run_empty_pipeline_test.sh

# Option 2: Running directly with the test parameter file
make test_empty_pipeline
./tests/test_empty_pipeline tests/test_data/test-mini-millennium.par
```

The test uses a special parameter file (`tests/test_data/test-mini-millennium.par`) that includes:

1. `ModuleConfigFile` pointing to `tests/test_data/empty_pipeline_config.json`
2. The configuration file defines placeholder modules that implement the module interface but perform no physics calculations

## Verifying Core Independence

To verify that the core is truly independent from physics:

1. **Run the empty pipeline test**: `make test_empty_pipeline` or `./tests/run_empty_pipeline_test.sh`
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
