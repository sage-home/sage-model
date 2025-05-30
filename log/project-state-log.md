<!-- Purpose: Snapshot of current codebase architecture -->
<!-- Update Rules: 
- 1000-word limit! 
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
│   │   └── legacy/          # Legacy modules for future migration
│   ├── io/                  # Input/output handlers
│   └── auxdata/             # Auxiliary data files
├── tests/                   # Testing framework
├── input/                   # Input parameter files and configurations
├── docs/                    # Documentation
└── log/                     # Project tracking logs
```

## Core Infrastructure

### Evolution Pipeline
The evolution pipeline uses a phase-based modular system:
- Pipeline phases (HALO, GALAXY, POST, FINAL) for different calculation scopes
- Modules declare which phases they support and are executed accordingly
- Event-based communication between modules

```
┌─────────────────────┐
│ evolve_galaxies     │◄────────┐
└─────────┬───────────┘         │
          │                     │
          ├─▶┌─────────────────┐│
          │  │ HALO phase      ││
          │  └─────────────────┘│
          │                     │
          ├─▶┌─────────────────┐│
          │  │ GALAXY phase    ││
          │  └─────────────────┘│
          │                     │
          ├─▶┌─────────────────┐│
          │  │ POST phase      ││
          │  └─────────────────┘│
          │                     │
          ├─▶┌─────────────────┐│
          │  │ FINAL phase     ││
          │  └─────────────────┘│
          │                     │
┌─────────▼───────────┐         │
│ evolution_          │         │
│ diagnostics_report  │─────────┘
└─────────────────────┘
```

### Configuration-Driven Pipeline System
The pipeline registry uses JSON configuration to determine module activation:

```
┌─────────────────────┐      ┌─────────────────────┐
│ Module Factories    │      │ JSON Configuration  │
│ - Self-registration │─────▶│ - modules.instances │
│ - Factory functions │      │ - enabled flags     │
└─────────┬───────────┘      └────────┬────────────┘
          │                           │
          │                           ▼
┌─────────▼───────────┐      ┌─────────────────────┐
│ pipeline_create_    │      │ Module Selection    │
│ with_standard_      │◄─────│ - Config filtering  │
│ modules()           │      │ - Fallback system   │
└─────────┬───────────┘      └─────────────────────┘
          │
          ▼
┌─────────────────────┐
│ Pipeline System     │
│ - Phase execution   │
│ - Module activation │
└─────────────────────┘
```

Key benefits:
1. **Runtime Modularity**: Module combinations definable through configuration
2. **Core-Physics Decoupling**: Core has no knowledge of physics implementations
3. **Robust Fallback**: Uses all registered modules if no configuration is available

### Module System
The module system enables plugin-based physics:
- Registration, lifecycle management, and inter-module communication
- Event system and callbacks for controlled module interactions
- Extension mechanism for module-specific galaxy data

```
Module System Architecture
┌───────────────────┐      ┌───────────────────┐
│ Module Registry   │      │ Module Callbacks  │
│ - Registration    │─────▶│ - Inter-module    │
│ - Lifecycle mgmt  │      │ - Call tracking   │
└─────────┬─────────┘      └─────────┬─────────┘
          │                          │
          ▼                          ▼
┌───────────────────┐      ┌───────────────────┐
│ Event System      │      │ Pipeline Executor │
│ - Event dispatch  │─────▶│ - Phase execution │
│ - Subscriptions   │      │ - Error handling  │
└───────────────────┘      └───────────────────┘
```

### Properties Module Architecture
The Properties Module enables complete core-physics decoupling:
- `properties.yaml`: Central definition of all galaxy properties
- Typed accessors for property access with core-physics separation
- Support for dynamic array properties with runtime-determined sizes

```
Properties Module Architecture
┌─────────────────────┐      ┌─────────────────────┐
│ properties.yaml     │      │ core_properties.h/c │
│ (Source of Truth)   │─────▶│ (Auto-generated)    │
└─────────────────────┘      └────────┬────────────┘
                                      │
┌─────────────────────┐      ┌────────▼────────────┐
│ Physics Modules     │      │ Property Accessors  │
│ - Use GALAXY_PROP_* │◄─────│ - Type-safe access  │
│ - Generic accessors │      │ - Core/physics      │
└─────────────────────┘      │   separation        │
                             └─────────────────────┘
```

### Core Galaxy Structure
The GALAXY struct contains only essential infrastructure fields:

```
struct GALAXY {
    // Core identifiers & tree structure
    int SnapNum;
    long long GalaxyIndex;
    int TreeIndex;
    int Type;
    long long HaloIndex;
    int MostBoundID;
    int NextGalaxy;
    
    // Properties & extension system
    galaxy_properties_t *properties;  // All physics properties
    void **extension_data;            // Module extensions
    int num_extensions;
    uint64_t extension_flags;
};
```

## I/O System
The I/O system uses property-based serialization:
- Standardized on HDF5 output format
- Property-agnostic, metadata-driven output
- Dynamic property discovery for field generation
- Output preparation implemented as FINAL phase module

```
I/O System Architecture
┌─────────────────────┐      ┌─────────────────────┐
│ io_interface        │      │ HDF5 Handler        │
│ - Unified interface │─────▶│ - Property-driven   │
│ - Format registry   │      │ - Metadata-based    │
└─────────────────────┘      └────────┬────────────┘
                                      │
┌─────────────────────┐      ┌────────▼────────────┐
│ Forest Handlers     │      │ Output Preparation  │
│ - LHalo/HDF5        │◄─────│ - FINAL phase       │
│ - ConsistentTrees   │      │ - Unit conversion   │
└─────────────────────┘      │ - Transformers      │
                             └─────────────────────┘
```

## Testing Framework
A comprehensive testing framework validates all aspects:
- Unit and integration tests for core components
- Scientific validation against reference outputs
- Core-physics separation verification
- Configuration-driven pipeline validation

## Current Development Status
The SAGE refactoring has achieved true runtime functional modularity through:

1. **Complete Core-Physics Separation:**
   - Physics-agnostic core with no knowledge of specific physics properties
   - Centralized property definition with type-safe access
   - Configuration-driven module activation
   - **Evolution diagnostics system now fully compliant** (May 23, 2025)
   - **Property system robustly handles all data types including uint64_t** (May 30, 2025)

2. **Phase-Based Pipeline Execution:**
   - Clear execution phases for different calculation scopes
   - Modules declare phase participation
   - Event-based communication between modules
   - **Separate event systems for core infrastructure and physics processes**

3. **Runtime Configurable Components:**
   - JSON configuration defines active modules
   - Property system adapts to available physics components
   - Dynamic property discovery for I/O operations
   - **Property accessors with robust NULL handling and error recovery** (May 30, 2025)

4. **Diagnostics and Monitoring:**
   - **Core diagnostics track only infrastructure metrics** (galaxy counts, phase timing, pipeline performance)
   - **Physics modules can register custom diagnostic metrics independently**
   - Comprehensive error handling and validation throughout

Next steps focus on implementing physics modules as pure add-ons to the core, with comprehensive validation to ensure scientific consistency with the original SAGE implementation.
