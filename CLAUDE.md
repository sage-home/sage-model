# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## SAGE Model - Semi-Analytic Galaxy Evolution

SAGE is a modular C-based galaxy formation model designed for cosmological simulations. The codebase implements a **physics-agnostic core with modular physics extensions** architecture.

## Build System

**Primary Build Commands:**
```bash
make                    # Build with all properties (full-physics mode)
make physics-free      # Core properties only (fastest, minimal memory)  
make full-physics      # All properties (default, maximum compatibility)
make custom-physics CONFIG=input/config.json  # Properties for specific modules
make clean             # Clean build artifacts
make help              # Show all available targets
```

**Build Features:**
- Property system automatically generates C code from `src/properties.yaml`
- HDF5 support: `USE-HDF5 = yes` (enabled by default)
- MPI support: `USE-MPI = yes` for parallel execution
- Memory checking: `MEM-CHECK = yes` for debugging
- Different property configurations require recompilation

## Testing Framework

**Test Commands:**
```bash
make tests             # Run complete test suite (unit + end-to-end)
make unit_tests        # Run only unit tests (faster for development)
./tests/test_sage.sh   # End-to-end scientific validation
```

**Test Categories:**
- **Core Tests**: Pipeline system, memory management, array utilities, physics-free mode
- **Property Tests**: Property serialization, validation, HDF5 integration  
- **I/O Tests**: Multiple tree formats (LHalo Binary/HDF5, Gadget4, Genesis, ConsistentTrees), I/O interface abstraction
- **Module Tests**: Dynamic loading, lifecycle, callbacks, inter-module communication
- **End-to-End Tests**: HDF5 output validation, integration workflows, property system validation

## Architecture Overview

**Core Design Principle:** Physics-agnostic core infrastructure with runtime-configurable physics modules.

```
Physics Modules (Cooling, Star Formation, Feedback, Mergers, etc.)
    ↓
Property System (Dynamic property generation from YAML)
    ↓  
Pipeline System (HALO → GALAXY → POST → FINAL execution phases)
    ↓
Core Infrastructure (Memory, I/O, Events, Module System, Configuration)
```

**Key Architectural Components:**

1. **Modular Physics System** (`src/physics/`, `src/core/core_module_system.*`)
   - Runtime module registration and loading via C constructors
   - Physics-agnostic core can run independently (`make physics-free`)
   - Configurable pipeline execution through JSON configuration
   - Event-driven inter-module communication

2. **Property System** (`src/properties.yaml`, `src/core/core_properties.*`)
   - Central YAML definition drives automatic C code generation
   - Compile-time optimization (only needed properties included)
   - Core vs physics property separation
   - Runtime property validation and access control

3. **Pipeline Execution** (`src/core/core_pipeline_system.*`)
   - Four-phase execution model: HALO → GALAXY → POST → FINAL
   - Module-agnostic execution context
   - Merger event queue for deferred processing
   - Phase-specific module callbacks

4. **I/O Abstraction** (`src/io/`)
   - **Unified I/O Interface**: Format-agnostic tree reading and galaxy output (`src/io/io_interface.*`)
   - **Multiple Tree Formats**: LHalo Binary/HDF5, Gadget4 HDF5, Genesis HDF5, ConsistentTrees ASCII/HDF5
   - **Migration Status**: LHalo HDF5 fully migrated to I/O interface (Phase 5.3), others use legacy fallback
   - **Property-Based Output**: HDF5 galaxy output driven by property system with transformers
   - **Resource Management**: Built-in handle tracking and memory management

## Key Files & Patterns

**Core Architecture:**
- `src/core/core_allvars.h`: Central type definitions and structures
- `src/core/sage.h`: Main API interface  
- `src/properties.yaml`: Property definitions driving code generation
- `src/core/core_module_system.*`: Module registration and lifecycle
- `src/core/core_pipeline_system.*`: Pipeline execution engine
- `src/io/io_interface.*`: Unified I/O interface for tree reading and galaxy output

**Configuration:**
- `input/default_config.json`: Runtime module configuration
- Parameter files (`*.par`): Simulation parameters
- Property build system: Controls compile-time features

**Testing Patterns:**
- Individual test executables for each component
- `tests/sagediff.py`: Galaxy property comparison with tolerance handling
- Test data in `tests/test_data/` for validation
- Reference outputs for end-to-end comparison

## Running the Model

**Basic Usage:**
```bash
./first_run.sh                    # Initialize directories and download test data
./sage input/millennium.par       # Run simulation
mpirun -np N ./sage input/millennium.par  # Parallel execution
```

**Analysis:**
```bash
cd plotting/
python3 allresults-local.py       # Plot z=0 results
python3 allresults-history.py     # Plot higher redshift results
```

## Development Notes

- The property system requires recompilation when `properties.yaml` changes
- Physics modules use C constructor attributes for automatic registration
- Error handling uses comprehensive context tracking (`src/core/core_module_error.*`)
- Memory management uses custom allocators (`src/core/core_mymalloc.*`)
- Physics-free mode validates core infrastructure independence
- I/O interface migration: LHalo HDF5 fully migrated, others use graceful fallback to legacy functions
- Tree reading operations use `core_io_tree.c` which tries I/O interface first, then legacy functions
- See `docs/io_interface_migration_guide.md` for I/O development guidelines

## User Instructions for Claude

- Never commit to git unless explicitly asked
- Never delete files, always archive into the `ignore` directory
- Assume no persistent memory — rely on logs for all continuity
- When reporting progress in `log/progress.md` include EVERY FILE that was changed and created
- Prefer lldb for debugging (feed it a text file with commands for best results)
- NEVER create mock data or simplified components unless explicitly told to do so
- NEVER replace existing complex components with simplified versions - always fix the actual problem
- ALWAYS find and fix the root cause of issues instead of creating workarounds
- When asked to run sage, use `./sage ./input/millennium.par` unless prompted differently
- When you think you've finished your work, stop and ask if we should run `make clean && make unit_tests` to validate it
- All new unit tests should obey `docs/templates/test_template.c`
