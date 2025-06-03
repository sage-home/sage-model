# SAGE Model Testing Framework

This directory contains the testing framework for the SAGE (Semi-Analytic Galaxy Evolution) model. The framework provides comprehensive validation of both the scientific accuracy and architectural integrity of the model.

## Testing Architecture Overview

The SAGE testing framework is organized into multiple categories to validate different aspects of the codebase:

1. **Core Infrastructure Tests** - Validate foundational components
2. **Property System Tests** - Ensure correct property handling
3. **I/O System Tests** - Verify data reading/writing capabilities
4. **Module System Tests** - Test modular architecture
5. **End-to-End Scientific Tests** - Verify scientific accuracy

## Running Tests

All tests are now integrated into the main Makefile and can be run using standardized commands:

```bash
# Run all tests
make tests

# Run tests by category
make core_tests        # Core infrastructure tests
make property_tests    # Property system tests
make io_tests          # I/O system tests
make module_tests      # Module system tests

# Run individual tests
make test_pipeline
make test_array_utils
make test_property_system_hdf5
# etc.
```

## Test Categories and Components

### Core Infrastructure Tests (`CORE_TESTS`)

Tests that validate the fundamental components of SAGE:

- **test_pipeline** - Tests the execution phases (HALO, GALAXY, POST, FINAL)
- **test_array_utils** - Tests dynamic array utilities with geometric growth
- **test_evolution_diagnostics** - Tests the collection of evolution metrics
- **test_core_pipeline_registry** - Tests module registration and pipeline creation
- **test_config_system** - Tests configuration loading and parsing
- **test_memory_pool** - Tests memory allocation/management
- **test_merger_queue** - Tests the merger event queue system
- And more...

### Property System Tests (`PROPERTY_TESTS`)

Tests focused on the property system that enables core-physics separation:

- **test_property_serialization** - Tests property serialization for I/O
- **test_property_array_access** - Tests access to array properties
- **test_property_system_hdf5** - Tests HDF5 output with the property system
- **test_property_validation** - Tests validation of property definitions
- **test_property_access_patterns** - Tests compliance with core-physics separation

### I/O System Tests (`IO_TESTS`)

Tests for reading merger trees and writing galaxy catalogs:

- **test_io_interface** - Tests the I/O abstraction layer
- **test_endian_utils** - Tests cross-platform endianness handling
- **test_lhalo_binary**, **test_lhalo_hdf5**, etc. - Tests various tree formats
- **test_io_memory_map** - Tests memory mapping for I/O optimization
- **test_io_buffer_manager** - Tests buffered I/O operations

### Module System Tests (`MODULE_TESTS`)

Tests for the modular plugin architecture:

- **test_pipeline_invoke** - Tests inter-module communication
- **test_module_callback** - Tests module callback system

### Specialized Tests

These tests validate key architectural principles:

- **test_empty_pipeline** - Verifies that the core can run with no physics modules
  ```bash
  # Run via test script
  ./tests/run_empty_pipeline_test.sh
  
  # Or directly
  make test_empty_pipeline
  ./tests/test_empty_pipeline tests/test_data/test-mini-millennium.par
  ```

## End-to-End Scientific Tests

The `test_sage.sh` script provides comprehensive scientific validation by comparing SAGE outputs to reference results. These tests are automatically run as part of `make tests`.

### What the End-to-End Tests Do

1. **Environment Setup**
   - Creates a Python virtual environment with required dependencies if needed
   - Downloads test merger trees (Mini-Millennium subset) if not present

2. **Binary Output Test**
   - Runs SAGE with binary output format
   - Compares output to reference binary files

3. **HDF5 Output Test**
   - Runs SAGE with HDF5 output format
   - Compares output to reference HDF5 files

4. **Output Comparison**
   - Uses `sagediff.py` to compare all galaxy properties between test outputs and reference files
   - Applies appropriate tolerances for floating-point comparisons
   - Provides detailed diagnostics for any discrepancies

### Running End-to-End Tests

```bash
# Run via the main Makefile (includes unit tests)
make tests

# Run directly
./test_sage.sh

# Run with verbose output (shows detailed comparison information)
./test_sage.sh --verbose

# Compile SAGE before running tests
./test_sage.sh --compile

# Show help message
./test_sage.sh --help
```

### Test Data and Results

- `test_data/` - Contains test input merger trees and reference SAGE outputs
- `test_results/` - Contains outputs generated during testing (created upon first run)
- `test_env/` - Python virtual environment with testing dependencies (created upon first run)

When interpreting results, remember that `sagediff.py` provides detailed property-by-property comparisons to help identify any discrepancies between the test outputs and reference results.

## Directory Structure

- `*.c` - Unit test source files
- `stubs/` - Mock implementations for testing
- `test_data/` - Test input data and reference outputs
- `test_sage.sh` - End-to-end scientific validation script
- `sagediff.py` - Output comparison utility
- `run_*.sh` - Helper scripts for specialized tests

## Adding New Tests

When adding new tests:

1. Create a test file in this directory (e.g., `test_new_feature.c`)
2. Add a build target to the main Makefile
3. Add the test to the appropriate category (CORE_TESTS, PROPERTY_TESTS, etc.)
4. Add documentation to `docs/sage_unit_tests.md`

## Test Template

Use the template in `docs/templates/test_template.c` when creating new tests, which includes:

- Setup and teardown functions
- Test assertion macros
- Test categorization
- Standardized result reporting

## Understanding Test Failures

Tests may fail for several reasons:

1. **Implementation Bug** - The implementation has an issue that needs fixing
2. **Architectural Change** - The architecture has evolved, and the test needs updating
3. **Expected Failure** - Some tests (especially end-to-end) may fail during Phase 5 refactoring

During Phase 5 (Core Module Migration), unit tests should pass, but end-to-end scientific comparisons may fail due to architectural changes.

## Comprehensive Documentation

For more detailed information about the testing architecture, see:

- [Testing Architecture Guide](../docs/testing_architecture_guide.md) - Overall testing philosophy and organization
- [Unit Tests Documentation](../docs/sage_unit_tests.md) - Detailed list of all unit tests and their status
- [Core-Physics Separation](../docs/core_physics_separation.md) - Architectural principles being tested

## Required Dependencies

- **C Compiler**: clang or gcc
- **HDF5 Library**: For testing HDF5 input/output
- **Python 3**: For end-to-end scientific testing
- **Standard Libraries**: stdlib, stdio, etc.
