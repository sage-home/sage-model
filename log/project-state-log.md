<!-- Purpose: Snapshot of current codebase architecture -->
<!-- Update Rules: 
- 750-word limit! 
- Overwrite outdated content 
- Focus on active components 
- Use UML-like text diagrams
-->

# SAGE Current Architecture

## Directory Structure
```
sage-model/
├── src/
│   ├── core/                # Core infrastructure components
│   ├── physics/             # Physics modules 
│   │   ├── legacy/          # Traditional physics implementations to be migrated
│   │   └── migrated/        # Successfully migrated physics modules
│   ├── io/                  # Input/output handlers
│   └── auxdata/             # Auxiliary data files
├── tests/                   # Testing framework
├── input/                   # Input parameter files
├── docs/                    # Documentation
└── log/                     # Project tracking logs
```

## Core Infrastructure

### Evolution Pipeline
The evolution pipeline has been refactored into a modular system:
- `physics_pipeline_executor.c`: Manages phase-based execution
- `core_pipeline_system.c`: Configurable execution pipeline
- Pipeline phases (HALO, GALAXY, POST, FINAL) for different calculation scopes
- Event-based communication between modules

```
┌─────────────────────┐
│ evolve_galaxies     │◄────────┐
└─────────┬───────────┘         │
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ HALO phase      ││
          │  └─────────────────┘│
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ GALAXY phase    ││
          │  └─────────────────┘│
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ POST phase      ││
          │  └─────────────────┘│
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ FINAL phase     ││
          │  └─────────────────┘│
          │                     │
┌─────────▼───────────┐         │
│ evolution_          │         │
│ diagnostics_report  │─────────┘
└─────────────────────┘
```

### Merger Event Queue
A merger event queue system provides controlled processing of mergers:
- `core_merger_queue.c`: Collects and processes mergers
- Defers merger processing to maintain scientific consistency
- Integrated with the evolution diagnostics system

```
Merger Event Queue
    ┌─────────────────────┐
    │ merger_event_queue  │
    │ - events[]          │◄────────┐
    │ - num_events        │         │
    └─────────┬───────────┘         │
              │                     │
    ┌─────────▼───────────┐         │
    │ process_merger      │         │
    │ _events()           │         │
    └─────────────────────┘         │
                                    │
    ┌─────────────────────┐         │
    │ evolve_galaxies     │         │
    │ - merge detection   │─────────┘
    └─────────────────────┘
```

### Module System
A comprehensive module system enables plugin-based physics:
- `core_module_system.c`: Module registration, loading, and lifecycle
- `core_module_callback.c`: Inter-module communication
- `core_galaxy_extensions.h`: Galaxy property extension mechanism

```
Module System
    ┌───────────────────┐      
    │ Module Registry   │      
    │ - Loaded modules  │      
    │ - Function tables │      
    │ - Lifecycle mgmt  │      
    └─────────┬─────────┘      
              │               
    ┌─────────▼─────────┐     
    │ Module Callbacks  │     
    │ - Inter-module    │     
    │ - Call stack      │     
    │ - Error tracking  │     
    └─────────┬─────────┘     
              │               
    ┌─────────▼─────────┐
    │ Pipeline Executor │
    │ - Phase execution │
    │ - Event dispatch  │
    └───────────────────┘
```

### Properties Module Architecture
The Properties Module architecture provides complete core-physics decoupling:
- `properties.yaml`: Centralized definition of all galaxy properties
- `generate_property_headers.py`: Generates type-safe accessors (GALAXY_PROP_*)
- `core_properties.c/h`: Auto-generated property infrastructure
- `standard_properties.c/h`: Registration with extension system

```
Properties Module Architecture
┌─────────────────────┐      ┌─────────────────────┐
│ properties.yaml     │      │ core_properties.h/c │
│ (Source of Truth)   │─────▶│ (Auto-generated)    │
└─────────────────────┘      └────────┬────────────┘
                                      │
┌─────────────────────┐      ┌────────▼────────────┐
│ Physics Modules     │      │ standard_properties │
│ - Use GALAXY_PROP_* │◄─────│ - Registration      │
│ - No direct access  │      │ - Mapping to exts   │
└─────────────────────┘      └─────────────────────┘
```

### Core Galaxy Structure
The core GALAXY struct now contains only essential infrastructure:
- Removed all physics fields from `core_allvars.h`
- Added `properties` pointer to the generated properties struct
- Reduced direct field access, preferring GALAXY_PROP_* macros

```
struct GALAXY {
    // Core identifiers
    int SnapNum;
    long long GalaxyIndex;
    int TreeIndex;
    
    // Tree structure
    int Type;
    long long HaloIndex;
    int MostBoundID;
    int NextGalaxy;
    
    // Core infrastructure
    galaxy_properties_t *properties;  // All physics properties
    void **extension_data;            // Module extensions
    int num_extensions;
    uint64_t extension_flags;
};
```

## I/O System
The I/O system has been enhanced with property-based serialization:
- `io_interface.h`: Unified interface for all I/O operations
- Standardized on HDF5 output format (binary removed)
- `output_preparation_module.c`: FINAL phase module for output preparation
- HDF5 handler dynamically discovers properties for field generation
- Property-agnostic output with proper core-physics separation
- Enhanced property serialization for dynamic arrays

```
I/O System
┌─────────────────────┐      ┌─────────────────────┐
│ io_interface        │      │ HDF5 Handler        │
│ - Capabilities      │◄────▶│ - Property-driven   │
│ - Format registry   │      │ - Core/Physics      │
└─────────────────────┘      │   separation        │
                             └────────┬────────────┘
                                      │
┌─────────────────────┐      ┌────────▼────────────┐
│ Forest Handlers     │      │ Output Preparation  │
│ - LHalo             │      │ - FINAL phase       │
│ - ConsistentTrees   │      │ - Unit conversion   │
└─────────────────────┘      │ - Metadata-driven   │
                             └─────────────────────┘
```

## Physics Modules
Physics modules are in transition to the new architecture:
- Some modules migrated to use GALAXY_PROP_* macros (cooling, infall)
- Legacy modules remain operational but marked for migration
- Placeholder modules demonstrate core-physics separation
- Output preparation module implements final preparation in property system

```
Physics Module Types
┌─────────────────────┐      ┌─────────────────────┐
│ Migrated Modules    │      │ Legacy Modules      │
│ - GALAXY_PROP_*     │      │ - Direct field      │
│ - Phase declarations│      │ - Traditional code  │
└─────────────────────┘      └─────────────────────┘
        │                               │
        │                               │
        ▼                               ▼
┌─────────────────────┐      ┌─────────────────────┐
│ Pipeline System     │      │ evolve_galaxies     │
│ - Phase awareness   │      │ - Direct execution  │
│ - Module registry   │      │ - Synchronization   │
└─────────────────────┘      └─────────────────────┘
```

## Testing Framework
A comprehensive testing framework validates all changes:
- End-to-end tests against Mini-Millennium reference outputs
- Unit tests for core components
- Integration tests for pipeline, modules, and properties
- Property validation tests
- Core-physics separation tests
- Benchmark system for performance tracking

```
Testing Framework
┌─────────────────────┐      ┌─────────────────────┐
│ End-to-End Tests    │      │ Unit Tests          │
│ - Mini-Millennium   │      │ - Core components   │
│ - Scientific output │      │ - Module system     │
└─────────────────────┘      └─────────────────────┘
        │                               │
        │                               │
        ▼                               ▼
┌─────────────────────┐      ┌─────────────────────┐
│ Integration Tests   │      │ Validation Tests    │
│ - Pipeline          │      │ - Properties        │
│ - Phase transitions │      │ - Core separation   │
└─────────────────────┘      └─────────────────────┘
```

## Current Development Focus
The current development focus is on completing the core-physics separation:
1. Empty Pipeline Validation - Verifying the core functions without physics modules
2. Legacy Code Removal - Eliminating physics dependencies from core components
3. Physics Module Migration - Implementing physics as pure add-ons to the core

The Properties Module architecture represents a significant advancement toward true runtime functional modularity. By centralizing all persistent per-galaxy physical state in properties.yaml, the system achieves complete separation between core infrastructure and physics implementations. This enables independent development and runtime configuration of physics modules without requiring core changes.

Key benefits of the current architecture:
- Physics-agnostic core with no knowledge of specific physical properties
- Centralized definition of all galaxy properties in a single source of truth
- Type-safe property access via generated macros
- Clear phase-based execution model for different calculation scopes
- Runtime module registration and configuration
- Comprehensive event and callback systems for module interactions
- Metadata-driven HDF5 output based on property definitions
- Thorough testing and validation to maintain scientific accuracy

Next steps will include completing the empty pipeline validation, removing legacy physics code, and implementing all physics components as pure add-on modules to demonstrate complete runtime functional modularity.
## Recent Architectural Improvements: Configuration-Driven Pipeline

The pipeline registry has been enhanced with configuration-driven module activation, representing a significant architectural improvement:



This enhancement implements true runtime functional modularity by:

1. **Decoupling Module Registration from Activation**: Modules register their factories via constructor attributes, making them *available*, but they're only *activated* if enabled in the configuration.

2. **Eliminating Core Knowledge of Physics**: The pipeline registry now has zero hardcoded knowledge of which modules constitute a "standard" run - "standard" is defined entirely by configuration.

3. **Runtime Configuration**: Users can define entirely different sets of active physics modules by changing the JSON configuration, without recompiling the code.

4. **Robust Fallback Mechanism**: If no configuration is available, the system falls back to using all registered modules, preserving backward compatibility.

This architectural change completes the core-physics separation requirements by making module activation fully configurable at runtime, enhancing SAGE's modularity and flexibility while maintaining scientific accuracy.

## Recent Architectural Improvements: Configuration-Driven Pipeline

The pipeline registry has been enhanced with configuration-driven module activation, representing a significant architectural improvement:

The pipeline registry now uses the JSON configuration to determine which modules to activate, rather than hardcoding this knowledge in the core infrastructure. Modules register their factories via constructor attributes, making them *available*, but they are only *activated* if enabled in the configuration.

This enhancement implements true runtime functional modularity by:

1. **Decoupling Module Registration from Activation**: Modules register their factories via constructor attributes, making them *available*, but they're only *activated* if enabled in the configuration.

2. **Eliminating Core Knowledge of Physics**: The pipeline registry now has zero hardcoded knowledge of which modules constitute a "standard" run - "standard" is defined entirely by configuration.

3. **Runtime Configuration**: Users can define entirely different sets of active physics modules by changing the JSON configuration, without recompiling the code.

4. **Robust Fallback Mechanism**: If no configuration is available, the system falls back to using all registered modules, preserving backward compatibility.

This architectural change completes the core-physics separation requirements by making module activation fully configurable at runtime, enhancing SAGE's modularity and flexibility while maintaining scientific accuracy.
