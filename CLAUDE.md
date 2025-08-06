# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SAGE (Semi-Analytic Galaxy Evolution) is a C-based galaxy formation model that runs on N-body simulation merger trees. It's a modular, customizable framework for studying galaxy evolution in cosmological context. The code can read various tree formats (binary, HDF5) and output results in binary or HDF5 formats.

## Common Commands

### Building the Code
- `make` - Compile SAGE executable (produces `sage` binary)
- `make clean` - Remove compiled objects and executables
- `make lib` - Build only the SAGE library (libsage.so or libsage.a)
- `make tests` - Run test suite (requires GSL)

### Configuration Options (in Makefile)
- `USE-HDF5 := yes` - Enable HDF5 support for input/output
- `USE-MPI := yes` - Enable MPI parallelization
- `USE-BUFFERED-WRITE := yes` - Enable buffered binary output (better performance)
- `MAKE-SHARED-LIB := yes` - Create shared library instead of static

### Running the Model
- `./first_run.sh` - Initialize directories and download Mini-Millennium test data
- `./sage input/millennium.par` - Run SAGE with parameter file
- `mpirun -np <N> ./sage input/millennium.par` - Run in parallel

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
- **Main execution**: `src/main.c`, `src/sage.c` - Entry points and main model loop
- **Parameter handling**: `src/core_read_parameter_file.c` - Parse .par files
- **Tree I/O**: `src/core_io_tree.c` + `src/io/` directory - Read various tree formats
- **Galaxy output**: `src/core_save.c` + `src/io/save_gals_*.c` - Write galaxy catalogs
- **Physics modules**: `src/model_*.c` files - Galaxy formation physics

### Key Header Files
- `src/core_allvars.h` - Main data structures and global variables
- `src/sage.h` - API definitions for Python interface
- `src/macros.h` - Compile-time constants and macros

### Galaxy Formation Physics
The model includes separate modules for:
- **Infall**: `model_infall.c` - Gas accretion onto halos
- **Cooling/Heating**: `model_cooling_heating.c` - Gas thermal evolution
- **Star Formation**: `model_starformation_and_feedback.c` - Star formation and feedback
- **Mergers**: `model_mergers.c` - Galaxy-galaxy mergers
- **Disk Instability**: `model_disk_instability.c` - Bulge formation
- **Reincorporation**: `model_reincorporation.c` - Ejected gas return

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
- `src/` - All C source code
- `plotting/` - Python plotting scripts
- `tests/` - Test scripts and reference data
- `input/` - Parameter files and example data
- `docs/` - Sphinx documentation
- `scrap/` - Development notes and temporary files

### Testing Philosophy
The test suite compares output against reference "correct" results from Mini-Millennium simulation, supporting both exact binary comparison and numerical tolerance comparison for cross-platform compatibility.

### Build System
The Makefile automatically detects:
- Compiler type (gcc/clang) and adjusts flags accordingly
- Available libraries (GSL, HDF5, MPI) 
- Platform-specific requirements (macOS vs Linux)
- CI environments (enables stricter error checking)