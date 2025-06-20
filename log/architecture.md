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
│   ├── physics/             # Essential physics functions and events only
│   │   └── placeholder_empty_module.c/.h  # Template for future physics modules
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
│ modules()           │      │ - Physics-free mode │
└─────────┬───────────┘      └─────────────────────┘
          │
          ▼
┌─────────────────────┐
│ Pipeline System     │
│ - Phase execution   │
│ - Empty pipeline OK │
└─────────────────────┘
```

Key benefits:
1. **Runtime Modularity**: Module combinations definable through configuration
2. **Core-Physics Decoupling**: Core has no knowledge of physics implementations
3. **Physics-Free Execution**: Empty pipeline provides core-only functionality when no config specified

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
The Properties Module enables complete core-physics decoupling with explicit build-time property control:
- `properties.yaml`: Central definition of all galaxy properties
- **Explicit Build Targets**: `make physics-free`, `make full-physics`, `make custom-physics CONFIG=file.json`
- Typed accessors for property access with core-physics separation
- Support for dynamic array properties with runtime-determined sizes

```
Properties Module Architecture
┌─────────────────────┐      ┌─────────────────────┐
│ properties.yaml     │      │ Build Targets       │
│ (Source of Truth)   │─────▶│ - physics-free      │
└─────────────────────┘      │ - full-physics      │
                             │ - custom-physics    │
                             └────────┬────────────┘
                                      │
┌─────────────────────┐      ┌────────▼────────────┐
│ Physics Modules     │      │ core_properties.h/c │
│ - Use GALAXY_PROP_* │◄─────│ (Auto-generated)    │
│ - Generic accessors │      │ - Optimised memory  │
└─────────────────────┘      │ - Type-safe access  │
                             └─────────────────────┘
```

**Build Target Benefits:**
- **physics-free**: Core properties only (25/57), minimal memory, fastest execution
- **full-physics**: All properties, maximum compatibility
- **custom-physics**: Module-specific properties, optimised for production

### Core Galaxy Structure
The GALAXY struct contains only extension mechanism and property system integration:

```
struct GALAXY {
    /* Extension mechanism - placed at the end for binary compatibility */
    void     **extension_data;    /* Array of pointers to module-specific data */
    int        num_extensions;    /* Number of registered extensions */
    uint64_t   extension_flags;   /* Bitmap to track which extensions are in use */
    
    /* Property system integration - SINGLE SOURCE OF TRUTH */
    galaxy_properties_t *properties; /* All properties managed by the property system */
};
```

**Key Architectural Features:**
- **Single Source of Truth**: All galaxy data (core + physics) stored in property system
- **Zero Direct Fields**: No direct field access - all data accessed via GALAXY_PROP_* macros
- **Property System Performance**: GALAXY_PROP_* macros compile to direct memory access
- **Extension Integration**: Module-specific data handled through extension mechanism

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
│ - Security enhanced │      │ - Multi-file support│
└─────────────────────┘      └────────┬────────────┘
                                      │
┌─────────────────────┐      ┌────────▼────────────┐
│ HDF5 Tree Readers   │      │ Output Preparation  │
│ - Gadget4 HDF5      │◄─────│ - FINAL phase       │
│ - Genesis HDF5      │      │ - Unit conversion   │
│ - ConsistentTrees   │      │ - Transformers      │
│ - LHalo HDF5        │      └─────────────────────┘
└─────────────────────┘
```

## Testing Framework
A comprehensive testing framework validates all aspects:
- Unit and integration tests for core components
- Scientific validation against reference outputs
- Core-physics separation verification
- Configuration-driven pipeline validation

## Current Development Status
The SAGE refactoring has achieved complete core-physics separation and runtime modularity:

1. **Core-Physics Separation:** Physics-agnostic core with centralized property system, configuration-driven module activation
2. **Build System:** Explicit property control (`make physics-free`, `full-physics`, `custom-physics`) optimizing memory (25 core vs 71 total properties)
3. **Pipeline Execution:** Phase-based execution (HALO→GALAXY→POST→FINAL) with event-driven module communication
4. **Runtime Configuration:** JSON-driven module activation with dynamic property discovery
5. **Diagnostics:** Core infrastructure metrics separate from physics module diagnostics

Architecture ready for physics modules as pure add-ons with scientific validation against original SAGE.

## Major Recent Achievements (June 2025)

**Architectural Refactoring Completion:**
- **GalaxyArray System**: Safe dynamic memory management with property-based deep copying replacing dangerous shallow pointer sharing
- **Modular Pipeline**: Four-phase execution (HALO→GALAXY→POST→FINAL) with physics-agnostic core and extension system integration
- **Scientific Preservation**: Maintained exact original SAGE algorithms while modernizing infrastructure for modularity
- **Enhanced Build System**: Metadata-driven code generation from properties.yaml and parameters.yaml with explicit property control

**Technical Impact:** Runtime physics module configurability, comprehensive memory management, preserved scientific integrity, and independent module development capability.