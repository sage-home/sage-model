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
│   ├── core/                   # Physics-agnostic infrastructure
│   │   ├── auxdata/            # Auxiliary data files (cooling functions)
│   │   ├── config/             # Configuration system (Task 1.4)
│   │   │   ├── config.h/.c     # Unified configuration API
│   │   │   ├── config_legacy.c # Legacy .par format support
│   │   │   ├── config_json.c   # JSON format support
│   │   │   └── config_validation.c # Parameter validation
│   │   ├── main.c              # Entry point
│   │   ├── sage.c              # Main model loop
│   │   ├── core_allvars.h      # Global variables and data structures
│   │   ├── core_build_model.c  # Galaxy evolution pipeline
│   │   ├── core_init.c         # Initialization routines
│   │   ├── core_io_tree.c      # Tree I/O coordination
│   │   ├── core_save.c         # Galaxy output coordination
│   │   ├── memory.h/.c         # Modern memory abstraction (Task 1.3)
│   │   └── [other core files]  # Utilities, parameter handling
│   ├── physics/                # Physics modules
│   │   ├── model_cooling_heating.c
│   │   ├── model_starformation_and_feedback.c
│   │   ├── model_mergers.c
│   │   ├── model_infall.c
│   │   ├── model_disk_instability.c
│   │   ├── model_reincorporation.c
│   │   └── model_misc.c
│   └── io/                     # Input/output handlers
│       ├── save_gals_*.c       # Galaxy output formats
│       └── read_tree_*.c       # Tree input formats
├── CMakeLists.txt              # Modern build system
├── schema/                     # Property system metadata (Task 2.1-2.2)
│   ├── properties.yaml         # Property definitions with module awareness
│   └── parameters.yaml         # Parameter definitions with validation
├── scripts/                    # Code generation tools (Task 2.2)
│   └── generate_property_headers.py # YAML-to-C property header generator
├── log/                        # Development tracking
├── tests/                      # Testing framework
├── input/                      # Parameter files
└── docs/                       # Documentation
```

## Core Infrastructure

### Build System
Modern CMake-based build system replacing legacy Makefile:
- Out-of-tree builds preventing source pollution
- Automatic dependency detection (HDF5, MPI, GSL)
- Platform-specific compiler optimization
- IDE integration support (VS Code, CLion)
- Configurable build options and debug modes

```
CMake Build Flow
┌─────────────────┐      ┌─────────────────┐
│ CMakeLists.txt  │─────▶│ Dependency      │
│ - Project cfg   │      │ Detection       │
│ - Source lists  │      │ - HDF5, MPI     │
└─────────────────┘      │ - Compiler opts │
                         └────────┬────────┘
                                  │
┌─────────────────┐      ┌────────▼────────┐
│ Build Targets   │◄─────│ Configuration   │
│ - sage (main)   │      │ - Debug/Release │
│ - tests         │      │ - Feature flags │
│ - libraries     │      └─────────────────┘
└─────────────────┘
```

### Galaxy Evolution Pipeline
Current monolithic processing model with direct physics calls:

```
Galaxy Evolution Flow
┌─────────────────┐
│ evolve_galaxies │
└─────┬───────────┘
      │
      ├─▶ model_infall
      ├─▶ model_cooling_heating  
      ├─▶ model_starformation_and_feedback
      ├─▶ model_disk_instability
      ├─▶ model_mergers
      ├─▶ model_reincorporation
      └─▶ model_misc
```

**Current Limitations:**
- Hardcoded physics sequence in core
- No runtime modularity
- Physics knowledge embedded in core infrastructure
- Direct coupling between core and physics modules

### Data Structures
**NEW: Metadata-Driven Property System (Task 2.2 Complete)**

Modern property system with core/physics separation:

```c
struct GALAXY {
    sage_galaxy_core_t core;           // Always available 
    sage_galaxy_physics_t* physics;    // Conditional allocation
    sage_galaxy_internal_t* internal;  // Calculation-only properties
    
    struct {
        float* SfrDisk;      // [STEPS] Dynamic arrays
        float* SfrBulge;     // [STEPS] Memory efficient
        int array_size;      // Actual allocation size
    } arrays;
};
```

**Property System Features:**
- YAML metadata drives C code generation (scripts/generate_property_headers.py)
- Type-safe macros: `GALAXY_GET_SNAPNUM(gal)`, `GALAXY_SET_MVIR(gal, val)`
- Compile-time availability checking with BUILD_BUG_OR_ZERO assertions
- Runtime modularity support with module-aware property masks
- Memory optimization with cache-aligned core structure (64-byte aligned)

## I/O System

### Input System
Multi-format tree reader with unified interface:
- LHalo (binary and HDF5)
- Gadget4 HDF5
- Genesis HDF5  
- ConsistentTrees (ASCII and HDF5)

```
Tree Reading Architecture
┌─────────────────┐      ┌─────────────────┐
│ core_io_tree.c  │─────▶│ Format Handlers │
│ - Format detect │      │ - read_tree_*.c │
│ - Unified API   │      │ - Format-specific│
└─────────────────┘      └─────────────────┘
```

### Output System  
Dual format support with hardcoded field lists:
- Binary format (legacy, being phased out)
- HDF5 format (preferred)

**Current Limitations:**
- Hardcoded output field lists in I/O modules
- No dynamic adaptation to available properties
- Manual synchronization between GALAXY struct and I/O

## Memory Management
Modern memory abstraction layer (Task 1.3 Complete):
- Standard malloc/free with optional tracking
- AddressSanitizer compatibility for debugging
- RAII-pattern memory scopes for automatic cleanup
- Backward compatible sage_malloc/sage_free API

## Configuration System
Unified configuration abstraction layer (Task 1.4 Complete):
- Dual format support: legacy .par and JSON
- Automatic format detection based on file extensions
- Comprehensive parameter validation with bounds checking
- Type-safe configuration API with error handling
- Optional cJSON integration for modern JSON configs

```
Configuration Architecture
┌─────────────────┐      ┌─────────────────┐
│ config_t        │─────▶│ Format Handlers │
│ - Unified API   │      │ - config_legacy │
│ - Auto detect   │      │ - config_json   │
│ - Validation    │      │ - Extensible    │
└─────────────────┘      └─────────────────┘
         │
         ▼
┌─────────────────┐
│ struct params   │
│ - Populated     │
│ - Validated     │
│ - Ready to use  │
└─────────────────┘
```

## Testing Framework
Comprehensive testing infrastructure:
- End-to-end scientific validation (test_sage.sh, sagediff.py)
- Unit testing framework following standardized template
- Automated CMake integration with CTest
- Category-based test organization (core, property, io, module, tree)
- Memory debugging support with AddressSanitizer integration

## Dependencies & Integration
**Required**: C99 compiler, Python 3.6+
**Optional**: GSL, HDF5, MPI, cJSON, LaTeX
**Python**: numpy, matplotlib, h5py, cffi, PyYAML, tqdm

**Python Integration**: `sage.py` wrapper using CFFI with auto-build and MPI support
**IDE Integration**: VS Code, CLion, Visual Studio via CMake

## Architecture Limitations

### Current Constraints:
- **Hardcoded Physics Coupling**: Evolution pipeline has direct calls to physics modules
- **Monolithic Data Structure**: GALAXY struct mixes core and physics properties  
- **Manual I/O Synchronization**: Output fields hardcoded and manually maintained

### Technical Architecture Debt:
- No separation between core infrastructure and physics knowledge
- String-based property access in some I/O operations
- Hardcoded array sizes (MAXSNAPS) limiting simulation flexibility
- Direct field access throughout codebase without abstraction

### Recently Addressed (Tasks 1.3-1.4):
- ✅ **Memory Management**: Modern abstraction with debugging tool compatibility
- ✅ **Configuration System**: Unified dual-format system with validation
- ✅ **Testing Infrastructure**: Standardized unit testing framework