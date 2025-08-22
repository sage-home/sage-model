# SAGE Command Quick Reference

## Build Commands (Work from Root Directory)
- `./build.sh` - Build SAGE executable and library (equivalent to old `make`)
- `./build.sh clean` - Remove compiled objects and executables  
- `./build.sh lib` - Build only the SAGE library
- `./build.sh tests` - Run complete test suite (unit + end-to-end)
- `./build.sh unit_tests` - Run unit tests only (fast development cycle)
- `./build.sh rebuild` - Complete clean rebuild
- `./build.sh help` - Show all available commands

## Testing Commands
- `./build.sh tests` - Run all tests (unit + end-to-end scientific validation)
- `./build.sh unit_tests` - Run unit tests only (fast, no downloads)
- `./build.sh core_tests` - Run core infrastructure tests
- `./build.sh property_tests` - Run property system tests  
- `./build.sh io_tests` - Run I/O system tests
- `./build.sh module_tests` - Run module system tests
- `./build.sh tree_tests` - Run tree processing tests

## Configuration Commands
- `./build.sh debug` - Configure debug build with memory checking
- `./build.sh release` - Configure optimized release build
- `./build.sh configure` - Reconfigure CMake (after changing options)
- `./build.sh status` - Show current build status

## Advanced CMake Options (for manual configuration)
- `cmake .. -DSAGE_USE_HDF5=ON` - Enable HDF5 support (default: ON)
- `cmake .. -DSAGE_USE_MPI=ON` - Enable MPI parallelization
- `cmake .. -DSAGE_USE_BUFFERED_WRITE=ON` - Enable buffered binary output (default: ON)
- `cmake .. -DSAGE_SHARED_LIBRARY=ON` - Create shared library (default: ON)
- `cmake .. -DSAGE_VERBOSE=ON` - Enable verbose output (default: ON)
- `cmake .. -DSAGE_MEMORY_CHECK=ON` - Enable AddressSanitizer for memory debugging
- `cmake .. -DCMAKE_BUILD_TYPE=Debug` - Debug build with symbols

## Configuration System Options
- `cmake .. -DSAGE_CONFIG_JSON_SUPPORT=ON` - Enable JSON configuration support (default: ON)
- `cmake .. -DSAGE_CONFIG_VALIDATION=ON` - Enable configuration validation (default: ON)

## Fresh Repository Setup
```bash
git clone <repository>
cd sage-model
./build.sh         # Automatically creates build/ directory and builds
```

## Alternative: Manual CMake Setup
```bash
mkdir build && cd build && cmake .. && make
```

## Running the Model
- `./first_run.sh` - Initialize directories and download Mini-Millennium test data
- `./build.sh` - Build SAGE after running first_run.sh
- `./build/sage input/millennium.par` - Run SAGE with parameter file
- `mpirun -np <N> ./build/sage input/millennium.par` - Run in parallel

## Python Interface
- `python sage.py` - Run SAGE through Python interface (auto-builds if needed)
- `make pyext` - Build Python extension manually

## Testing
- `./tests/test_sage.sh` - Full test suite comparing against reference output
- Tests both binary and HDF5 output formats
- Downloads test data automatically if not present
- **New unit tests**: Use template at `docs/templates/test_template.md` for standardized test development

## Plotting and Analysis
- `cd plotting/ && python allresults-local.py` - Plot z=0 results
- `cd plotting/ && python allresults-history.py` - Plot redshift evolution
- `python plotting/example.py` - Generate example plots using sage-analysis package

## Development Workflow Examples
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

## Configuration Examples

### JSON Configuration Example
```json
{
  "simulation": {
    "boxSize": 62.5,
    "omega": 0.25,
    "omegaLambda": 0.75,
    "hubble_h": 0.73
  },
  "io": {
    "treeDir": "./input/data/millennium/",
    "outputDir": "./output/",
    "outputFormat": "sage_hdf5"
  },
  "physics": {
    "sfPrescription": 0,
    "agnRecipeOn": 2
  }
}
```

### Configuration API Usage
```c
#include "config/config.h"

config_t *config = config_create();
int result = config_read_file(config, "input/config.json");  // or .par
if (result == CONFIG_SUCCESS) {
    config_validate(config);  // Optional validation
    // Access parameters via config->params
}
config_destroy(config);
```

## Testing Configuration
- `./build/test_config` - Test configuration system functionality
- `./build/test_memory` - Test memory abstraction layer