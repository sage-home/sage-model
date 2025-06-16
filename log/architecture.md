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
The SAGE refactoring has achieved true runtime functional modularity through:

1. **Complete Core-Physics Separation:**
   - Physics-agnostic core with no knowledge of specific physics properties
   - Centralized property definition with type-safe access
   - Configuration-driven module activation with physics-free fallback
   - **All placeholder modules removed** - only template remains (June 2025)
   - **Manifest/discovery systems removed** - simplified self-registering architecture (June 2025)
   - **Evolution diagnostics system now fully compliant** (May 23, 2025)
   - **Property system robustly handles all data types including uint64_t** (May 30, 2025)

2. **Explicit Property Build System:**
   - **Build-time property control**: `make physics-free`, `make full-physics`, `make custom-physics`
   - **Memory optimisation**: Include only needed properties (25 core vs 57 total)
   - **Performance benefits**: Faster execution and reduced memory usage in physics-free mode
   - **Configuration validation**: Clear errors for missing config files

3. **Phase-Based Pipeline Execution:**
   - Clear execution phases for different calculation scopes
   - Modules declare phase participation
   - Event-based communication between modules
   - **Empty pipeline execution**: Graceful handling of zero physics modules
   - **Separate event systems for core infrastructure and physics processes**

4. **Runtime Configurable Components:**
   - JSON configuration defines active modules
   - Property system adapts to available physics components
   - Dynamic property discovery for I/O operations
   - **Property accessors with robust NULL handling and error recovery** (May 30, 2025)

5. **Diagnostics and Monitoring:**
   - **Core diagnostics track only infrastructure metrics** (galaxy counts, phase timing, pipeline performance)
   - **Physics modules can register custom diagnostic metrics independently**
   - Comprehensive error handling and validation throughout

Architecture is now ready for implementing physics modules as pure add-ons to the core, with comprehensive validation to ensure scientific consistency with the original SAGE implementation.

## Recent Update: Architectural Refactoring Plan Implementation (June 10, 2025) ✅ COMPLETED

The comprehensive architectural refactoring plan has been successfully implemented, transforming SAGE from a monolithic implementation into a modular, physics-agnostic system with complete core-physics separation.

**Major Architectural Achievements:**

**1. GalaxyArray Memory Management System:**
- **Safe Dynamic Memory**: Replaced static galaxy arrays with GalaxyArray system providing automatic expansion, bounds checking, and memory corruption prevention
- **Property-Based Deep Copying**: Implemented deep_copy_galaxy() function using property system to replace dangerous shallow pointer copying
- **Memory Safety**: Fixed critical memory corruption issues where multiple galaxies shared property pointers, causing segfaults during evolution

**2. Modular Galaxy Evolution Pipeline:**
- **Four-Phase Execution**: Established HALO → GALAXY → POST → FINAL phase model with configurable physics modules
- **Physics-Agnostic Core**: Core infrastructure operates independently from physics implementations, enabling runtime module configuration
- **Extension System Integration**: Added galaxy_extension_copy() for module-specific data while maintaining core-physics separation

**3. Complete Scientific Algorithm Preservation:**
- **Exact Original Logic**: Maintained scientific accuracy from original SAGE implementation while modernizing infrastructure
- **Helper Function Extraction**: Separated concerns with find_most_massive_progenitor(), copy_galaxies_from_progenitors(), set_galaxy_centrals()
- **Memory Safety Without Science Changes**: Resolved memory issues without altering galaxy evolution algorithms

**4. Enhanced Build System and Code Generation:**
- **Explicit Property Control**: Enhanced build targets (physics-free, full-physics, custom-physics) with automatic code generation
- **Metadata-Driven Systems**: Both properties.yaml and parameters.yaml drive automatic C code generation
- **Core-Physics Separation Compliance**: Zero hardcoded physics knowledge in core infrastructure

**Technical Impact:**
- **Runtime Configurability**: Physics modules can be enabled/disabled through JSON configuration
- **Memory Robustness**: Comprehensive memory management with automatic cleanup and corruption detection
- **Scientific Integrity**: Maintains exact scientific results while providing modular, extensible architecture
- **Development Efficiency**: Modular design enables independent physics module development and testing

The refactored architecture demonstrates complete separation between core galaxy evolution infrastructure and physics implementations, enabling true runtime functional modularity while preserving the scientific accuracy of the original SAGE model.