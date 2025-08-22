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
│   │   ├── main.c              # Entry point
│   │   ├── sage.c              # Main model loop
│   │   ├── core_allvars.h      # Global variables and data structures
│   │   ├── core_build_model.c  # Galaxy evolution pipeline
│   │   ├── core_init.c         # Initialization routines
│   │   ├── core_io_tree.c      # Tree I/O coordination
│   │   ├── core_save.c         # Galaxy output coordination
│   │   └── [other core files]  # Utilities, memory, parameter handling
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
Primary galaxy data structure with direct field access:

```c
struct GALAXY {
    // Core properties
    int   Type;
    int   GalaxyIndex;
    int   HaloIndex;
    float Mvir;
    float Rvir;
    
    // Physics properties - direct coupling
    float ColdGas;
    float HotGas;
    float StellarMass;
    float BulgeMass;
    float BlackHoleMass;
    
    // Arrays with hardcoded sizing
    float SfrDisk[MAXSNAPS];
    float SfrBulge[MAXSNAPS];
    // ... many more hardcoded physics fields
};
```

**Current Architecture Issues:**
- No separation between core and physics properties
- Hardcoded array sizes limiting flexibility
- String-based property access in some I/O code
- No type safety for property access

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
Legacy custom allocator system:
- mymalloc/myfree wrapper around standard allocators
- Manual memory tracking and statistics
- Limited debugging tool compatibility

## Configuration System
Legacy parameter file system:
- .par format with manual parsing
- Hardcoded parameter validation
- No schema validation or type safety

## Testing Framework
Basic end-to-end testing:
- test_sage.sh script for scientific validation
- sagediff.py for output comparison
- Limited unit testing coverage

## Architecture Limitations

### Current Constraints:
- **Hardcoded Physics Coupling**: Evolution pipeline has direct calls to physics modules
- **Monolithic Data Structure**: GALAXY struct mixes core and physics properties
- **Manual I/O Synchronization**: Output fields hardcoded and manually maintained
- **Legacy Memory Management**: Custom allocator with limited debugging tool support
- **Parameter System**: Manual parsing without schema validation or type safety

### Technical Architecture Debt:
- No separation between core infrastructure and physics knowledge
- String-based property access in some I/O operations
- Hardcoded array sizes (MAXSNAPS) limiting simulation flexibility
- Direct field access throughout codebase without abstraction
- Manual memory tracking and cleanup patterns