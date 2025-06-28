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

The SAGE test suite contains **58 individual unit tests** organized into 5 main categories:

### Core Infrastructure Tests (`CORE_TESTS`) - 31 tests

Tests that validate the fundamental components of SAGE:

- **test_pipeline** - Tests the execution phases (HALO, GALAXY, POST, FINAL)
- **test_array_utils** - Tests dynamic array utilities with geometric growth
- **test_core_property** - Tests the property system's core functionality
- **test_core_pipeline_registry** - Tests module registration and pipeline creation
- **test_dispatcher_access** - Tests type-safe dispatcher functions for property access
- **test_evolution_diagnostics** - Tests the collection of evolution metrics
- **test_evolve_integration** - Tests the refactored evolve_galaxies loop
- **test_memory_pool** - Tests memory allocation/management
- **test_merger_queue** - Tests the merger event queue system
- **test_core_merger_processor** - Tests physics-agnostic merger event handling
- **test_config_system** - Tests configuration loading and parsing
- **test_physics_free_mode** - Tests core-physics separation
- **test_parameter_validation** - Tests parameter file parsing and validation
- **test_resource_management** - Comprehensive resource lifecycle validation
- **test_integration_workflows** - Tests multi-system integration workflows
- **test_error_recovery** - Tests system resilience and recovery
- **test_dynamic_memory_expansion** - Tests dynamic memory expansion system
- **test_data_integrity_physics_free** - Tests data integrity in physics-free mode
- **test_galaxy_array** - Tests the galaxy array data structure and component management
- **test_galaxy_array_component** - Tests individual galaxy array component access and manipulation
- **test_halo_progenitor_integrity** - Validates halo progenitor relationships and data integrity
- **test_core_property_separation** - Tests separation between core and physics properties
- **test_property_separation_scientific_accuracy** - Tests scientific accuracy with property separation
- **test_property_separation_memory_safety** - Tests memory safety with property separation
- **test_hdf5_output_validation** - Tests HDF5 output validation
- **test_fof_group_assembly** - Tests FOF group galaxy type assignment and central identification
- **test_fof_evolution_context** - Tests FOF-centric timing and merger tree continuity
- **test_fof_memory_management** - Tests memory management for large FOF groups and leak detection
- **test_orphan_tracking** - Tests comprehensive orphan galaxy tracking for mass conservation when host halos disappear
- **test_orphan_tracking_simple** - Tests simplified orphan galaxy tracking functionality with basic scenarios
- **test_orphan_fof_disruption** - Tests critical orphan galaxy handling during FOF group disruption scenarios

### Property System Tests (`PROPERTY_TESTS`) - 7 tests

Tests focused on the property system that enables core-physics separation:

- **test_property_serialization** - Tests property serialization for I/O
- **test_property_array_access** - Tests access to array properties
- **test_property_system_hdf5** - Tests HDF5 output with the property system
- **test_property_validation** - Tests validation of property definitions
- **test_property_access_comprehensive** - Comprehensive validation of property system
- **test_property_yaml_validation** - Tests YAML property definition validation
- **test_parameter_yaml_validation** - Tests YAML parameter definition validation

### I/O System Tests (`IO_TESTS`) - 11 tests

Tests for reading merger trees and writing galaxy catalogs:

- **test_io_interface** - Tests the I/O abstraction layer
- **test_endian_utils** - Tests cross-platform endianness handling
- **test_lhalo_binary** - Tests LHalo binary format reading
- **test_hdf5_output** - Tests HDF5 output handler
- **test_lhalo_hdf5** - Tests LHalo HDF5 format reading
- **test_gadget4_hdf5** - Tests Gadget4 HDF5 format reading
- **test_genesis_hdf5** - Tests Genesis HDF5 format reading
- **test_consistent_trees_hdf5** - Tests ConsistentTrees HDF5 format reading
- **test_io_memory_map** - Tests memory mapping for I/O optimization
- **test_io_buffer_manager** - Tests buffered I/O operations
- **test_validation_framework** - Tests I/O validation mechanisms

### Module System Tests (`MODULE_TESTS`) - 3 tests

Tests for the modular plugin architecture:

- **test_pipeline_invoke** - Tests inter-module communication
- **test_module_callback** - Tests module callback system
- **test_module_lifecycle** - Tests complete module lifecycle management

### Tree-Based Processing Tests (`TREE_TESTS`) - 6 tests

Tests for the new tree-based processing mode:

- **test_tree_infrastructure** - Validates the core tree infrastructure
- **test_galaxy_inheritance** - Tests galaxy inheritance and orphan creation
- **test_tree_fof_processing** - Validates FOF group processing in tree mode
- **test_tree_physics_integration** - Tests the integration of the physics pipeline
- **test_tree_physics_simple** - A simplified test for the physics pipeline integration
- **test_tree_mode_scientific_validation** - Validates the scientific accuracy of the tree mode


### Tree-Based Processing Tests (`TREE_TESTS`) - 6 tests

Tests for the new tree-based processing mode:

- **test_tree_infrastructure** - Validates the core tree infrastructure
- **test_galaxy_inheritance** - Tests galaxy inheritance and orphan creation
- **test_tree_fof_processing** - Validates FOF group processing in tree mode
- **test_tree_physics_integration** - Tests the integration of the physics pipeline
- **test_tree_physics_simple** - A simplified test for the physics pipeline integration
- **test_tree_mode_scientific_validation** - Validates the scientific accuracy of the tree mode

## End-to-End Scientific Tests

The `test_sage.sh` script provides comprehensive scientific validation for the **snapshot-based** processing mode by comparing SAGE outputs to reference results. These tests are automatically run as part of `make tests`.

### Tree-Based Mode Validation

The `test_tree_mode_validation.py` script provides a comprehensive validation suite for the **tree-based** processing mode. It compares the output of the tree mode with the snapshot mode and checks for:

-   **Mass Conservation**: Validates that both processing modes produce reasonable halo masses.
-   **Orphan Galaxy Handling**: Confirms that the tree mode correctly identifies orphan galaxies.
-   **Gap Tolerance**: Ensures the tree mode handles missing snapshots in merger trees.
-   **Performance Comparison**: Verifies that the tree mode meets performance requirements.

To run the tree mode validation script:
```bash
python3 tests/test_tree_mode_validation.py
```

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

## Test Suite Statistics

- **Total unit tests**: 58 individual test executables
- **Core Infrastructure**: 31 tests
- **Property System**: 7 tests
- **I/O System**: 11 tests
- **Module System**: 3 tests
- **Tree-Based Processing**: 6 tests
- **Test categories**: 5 main categories with specialized make targets
- **Scientific validation**: End-to-end testing via `test_sage.sh` and `test_tree_mode_validation.py`

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
