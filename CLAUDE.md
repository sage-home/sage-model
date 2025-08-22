# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SAGE (Semi-Analytic Galaxy Evolution) is a C-based galaxy formation model that runs on N-body simulation merger trees. It's a modular, customizable framework for studying galaxy evolution in cosmological context. The code can read various tree formats (binary, HDF5) and output results in binary or HDF5 formats.

## Common Commands

### Build Commands (Work from Root Directory)
- `./build.sh` - Build SAGE executable and library (equivalent to old `make`)
- `./build.sh clean` - Remove compiled objects and executables  
- `./build.sh lib` - Build only the SAGE library
- `./build.sh tests` - Run complete test suite (unit + end-to-end)
- `./build.sh unit_tests` - Run unit tests only (fast development cycle)
- `./build.sh rebuild` - Complete clean rebuild
- `./build.sh help` - Show all available commands

### Testing Commands
- `./build.sh tests` - Run all tests (unit + end-to-end scientific validation)
- `./build.sh unit_tests` - Run unit tests only (fast, no downloads)
- `./build.sh core_tests` - Run core infrastructure tests
- `./build.sh property_tests` - Run property system tests  
- `./build.sh io_tests` - Run I/O system tests
- `./build.sh module_tests` - Run module system tests
- `./build.sh tree_tests` - Run tree processing tests

### Configuration Commands
- `./build.sh debug` - Configure debug build with memory checking
- `./build.sh release` - Configure optimized release build
- `./build.sh configure` - Reconfigure CMake (after changing options)
- `./build.sh status` - Show current build status

### Advanced CMake Options (for manual configuration)
- `cmake .. -DSAGE_USE_HDF5=ON` - Enable HDF5 support (default: ON)
- `cmake .. -DSAGE_USE_MPI=ON` - Enable MPI parallelization
- `cmake .. -DSAGE_USE_BUFFERED_WRITE=ON` - Enable buffered binary output (default: ON)
- `cmake .. -DSAGE_SHARED_LIBRARY=ON` - Create shared library (default: ON)
- `cmake .. -DSAGE_VERBOSE=ON` - Enable verbose output (default: ON)
- `cmake .. -DSAGE_MEMORY_CHECK=ON` - Enable AddressSanitizer for memory debugging
- `cmake .. -DCMAKE_BUILD_TYPE=Debug` - Debug build with symbols

### Fresh Repository Setup
```bash
git clone <repository>
cd sage-model
./build.sh         # Automatically creates build/ directory and builds
```

**Simple as that!** The build wrapper handles all CMake setup automatically.

### Alternative: Manual CMake Setup
If you prefer working directly with CMake:
```bash
mkdir build && cd build && cmake .. && make
```

### Running the Model
- `./first_run.sh` - Initialize directories and download Mini-Millennium test data
- `./build.sh` - Build SAGE after running first_run.sh
- `./build/sage input/millennium.par` - Run SAGE with parameter file
- `mpirun -np <N> ./build/sage input/millennium.par` - Run in parallel

### Python Interface
- `python sage.py` - Run SAGE through Python interface (auto-builds if needed)
- `make pyext` - Build Python extension manually

### Testing
- `./tests/test_sage.sh` - Full test suite comparing against reference output
- Tests both binary and HDF5 output formats
- Downloads test data automatically if not present

### Plotting and Analysis
- `cd plotting/ && python allresults-local.py` - Plot z=0 results
- `cd plotting/ && python allresults-history.py` - Plot redshift evolution
- `python plotting/example.py` - Generate example plots using sage-analysis package

## Code Architecture

### Core C Components
- **Main execution**: `src/core/main.c`, `src/core/sage.c` - Entry points and main model loop
- **Parameter handling**: `src/core/core_read_parameter_file.c` - Parse .par files
- **Tree I/O**: `src/core/core_io_tree.c` + `src/io/` directory - Read various tree formats
- **Galaxy output**: `src/core/core_save.c` + `src/io/save_gals_*.c` - Write galaxy catalogs
- **Physics modules**: `src/physics/model_*.c` files - Galaxy formation physics

### Key Header Files
- `src/core/core_allvars.h` - Main data structures and global variables
- `src/core/sage.h` - API definitions for Python interface
- `src/core/macros.h` - Compile-time constants and macros

### Galaxy Formation Physics
The model includes separate modules for:
- **Infall**: `src/physics/model_infall.c` - Gas accretion onto halos
- **Cooling/Heating**: `src/physics/model_cooling_heating.c` - Gas thermal evolution
- **Star Formation**: `src/physics/model_starformation_and_feedback.c` - Star formation and feedback
- **Mergers**: `src/physics/model_mergers.c` - Galaxy-galaxy mergers
- **Disk Instability**: `src/physics/model_disk_instability.c` - Bulge formation
- **Reincorporation**: `src/physics/model_reincorporation.c` - Ejected gas return

### Input/Output Formats
**Supported Tree Formats**:
- `lhalo_binary` - LHalo binary format
- `lhalo_hdf5` - LHalo HDF5 format  
- `consistent_trees_ascii/hdf5` - Consistent Trees formats
- `genesis_hdf5` - Genesis HDF5 format
- `gadget4_hdf5` - Gadget4 HDF5 format

**Output Formats**:
- `sage_binary` - Binary format (will be deprecated)
- `sage_hdf5` - HDF5 format (recommended)

### Parameter Files
Parameter files (`.par`) control:
- Input tree specification (`TreeName`, `TreeType`, `SimulationDir`)
- Output configuration (`FileNameGalaxies`, `OutputDir`, `OutputFormat`)
- Physics parameters (star formation, feedback, etc.)
- Snapshot selection (`NumOutputs`)

### Python Integration
- `sage.py` - Python wrapper using CFFI for C library calls
- Automatically builds shared library if needed
- Supports MPI execution when mpi4py is available
- Main functions: `build_sage_pyext()`, `run_sage(paramfile)`

### Dependencies
**Required**:
- C99 compiler
- Python 3.6+ (for Python interface and plotting)

**Optional but Recommended**:
- GSL (GNU Scientific Library) - for some numerical functions
- HDF5 libraries - for HDF5 I/O support
- MPI implementation - for parallel execution
- LaTeX - for high-quality plot rendering

**Python Dependencies** (see requirements.txt):
- numpy, matplotlib, h5py (core scientific computing)
- cffi (Python-C interface)
- PyYAML (configuration processing)
- tqdm (progress bars)

## Development Notes

### File Organization
- `src/core/` - Core C source code (physics-agnostic)
- `src/physics/` - Physics module C source code  
- `src/io/` - Input/output C source code
- `log/` - Development tracking and AI support (decisions, progress, architecture)
- `plotting/` - Python plotting scripts
- `tests/` - Test scripts and reference data
- `input/` - Parameter files and example data
- `docs/` - Sphinx documentation
- `scrap/` - Development notes and obsolete files

### Testing Philosophy
The test suite compares output against reference "correct" results from Mini-Millennium simulation, supporting both exact binary comparison and numerical tolerance comparison for cross-platform compatibility.

### Build System
The CMake build system automatically detects:
- Compiler type (gcc/clang) and adjusts flags accordingly
- Available libraries (GSL, HDF5, MPI) 
- Platform-specific requirements (macOS vs Linux)
- CI environments (enables stricter error checking)

### Development Workflow
```bash
# Initial setup (from root directory)
./first_run.sh          # Download test data and setup directories (first time only)
./build.sh debug        # Configure for debugging with memory checking

# Development cycle  
./build.sh              # Build
./build.sh unit_tests   # Quick test cycle  
./build.sh tests        # Full test suite
./build/sage input/millennium.par  # Run

# Clean rebuild
./build.sh clean && ./build.sh     # Clean rebuild
./build.sh rebuild                 # Complete rebuild (removes build/ directory)
```

### IDE Integration
The CMake build system provides excellent IDE integration:
- **VS Code**: Open folder, CMake Tools extension auto-configures
- **CLion**: Open CMakeLists.txt directly
- **Visual Studio**: Open as CMake project

---

## Development Logging System

### Log Files (for AI Development Support)
- `log/decisions.md` - Critical architectural decisions with rationale and impact
- `log/phase.md` - Current development phase context and progress tracking
- `log/architecture.md` - Current system architecture snapshot  
- `log/progress.md` - Completed milestones and achievements
- `log/archive/` - Historical log files archived by phase

### Logging Guidelines
- **decisions.md**: Record key technical decisions affecting current/future development
- **progress.md**: Document completed milestones with file changes (append-only with `cat << EOF`)
- **phase.md**: Update current phase status and completion criteria
- **architecture.md**: Maintain current system architecture snapshot (overwrite when outdated)

---

# User Instructions to always follow

- Before beginning, read `log/sage-architecture-vision.md` and `sage-architecture-guide.md` for context
- All code and tests should be written to the highest professional coding standards 
- Never simplify a test just to make it pass; a failing test might indicate a problem with the codebase, which makes it a successful test!
- When reporting progress in `log/progress.md` include every file that was changed, created, and removed
- Always ask before committing finished work to git
- Before finalising a task consider if `CLAUDE.md` needs to be updated
- Assume no persistent memory — rely on logs and docs for all continuity
- When asked to write a report or similar, put it in the `obsidian-inbox` directory
- NEVER delete files, ALWAYS archive them into the `scrap` directory
