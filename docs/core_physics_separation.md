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

### Physics Modules

Physics modules are now implemented as pure add-ons:

1. **Self-contained**: Each module contains its own implementation
2. **Registration**: Modules register themselves with the core at runtime
3. **Property access**: Modules use GALAXY_PROP_* macros to access properties
4. **Phase support**: Modules declare which pipeline phases they support
5. **No core dependencies**: Modules don't depend on core infrastructure details

## Testing Core-Physics Separation

The `test_core_physics_separation.c` file provides a comprehensive test of the core-physics separation, verifying that:

1. The core can run with a minimal "placeholder" module that does nothing
2. The pipeline executes all phases correctly
3. Galaxies are processed without any physics calculations
4. No direct field synchronization is required

## Running with the Empty Pipeline

To run SAGE with a minimal empty pipeline:

```bash
cd tests
make -f Makefile.empty_pipeline
```

This will use the `input/empty_pipeline_parameters.par` parameter file, which specifies:

1. `ModuleConfigFile` pointing to `input/config_empty_pipeline.json`
2. The configuration file defines a single placeholder module that does nothing

## Verifying Core Independence

To verify that the core is truly independent from physics:

1. **Run the core separation test**: `make test_core_physics_separation`
2. **Run with empty pipeline**: `make -f tests/Makefile.empty_pipeline`
3. **Check the output**: Verify that the model runs successfully without errors
4. **Inspect memory usage**: The model should use significantly less memory than with full physics

## Benefits

This architecture provides several advantages:

1. **Modularity**: Physics modules can be added, removed, or replaced without affecting the core
2. **Testing**: Core functionality can be tested independently from physics
3. **Development**: New physics modules can be developed without modifying the core
4. **Memory efficiency**: Only required properties are allocated
5. **Runtime configuration**: Different physics pipelines can be selected at runtime
