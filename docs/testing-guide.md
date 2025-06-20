# SAGE Testing Guide

## Overview

The SAGE testing framework provides comprehensive validation of the codebase through categorized unit tests and specialized integration tests. This guide covers the testing philosophy, test organization, execution methods, and development practices for maintaining and extending the test suite.

## Testing Philosophy

The SAGE testing framework is built around three key principles:

1. **Separation of Concerns**: Tests are categorized based on the component they validate, with clear boundaries between core infrastructure, property system, I/O, module system, and core-physics separation.

2. **Evolution with Architecture**: The testing strategy evolves as the architecture changes, with new tests added to validate new components and outdated tests either updated or deprecated.

3. **Fail-Fast Development**: Tests should fail early and explicitly when assumptions are violated, providing clear guidance on what needs to be fixed.

## Test Categories

### Core Infrastructure Tests (25 tests)

These tests validate the fundamental infrastructure components of SAGE:

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| **test_pipeline** | Tests the pipeline execution system with 4 phases (HALO, GALAXY, POST, FINAL) | Phase 2.5 (Mar 2025) |
| **test_array_utils** | Tests the array utility functions for dynamic array manipulation | Phase 3.3 (Apr 2025) |
| **test_core_property** | Tests the property system's core functionality with mock implementations | Phase 5.2.B (May 2025) |
| **test_core_pipeline_registry** | Tests the pipeline registry for module registration and pipeline creation | Phase 5.2.F (May 2025) |
| **test_dispatcher_access** | Tests the type-safe dispatcher functions for property access | Phase 5.2.F.4 (May 2025) |
| **test_evolution_diagnostics** | Tests the diagnostics system for the galaxy evolution process | Phase 4.7 (Apr 2025) |
| **test_evolve_integration** | Tests the refactored evolve_galaxies loop with phase-based execution | Phase 5.1 (Apr 2025) |
| **test_memory_pool** | Tests the memory pooling system with support for various property types and dynamic arrays | Phase 3.3 (Apr 2025) |
| **test_merger_queue** | Tests the merger event queue system for deferred merger processing | Phase 5.1 (May 2025) |
| **test_core_merger_processor** | Tests physics-agnostic merger event handling and dispatching | Phase 5.2.G (May 2025) |
| **test_config_system** | Tests JSON configuration loading, parsing, nested paths, and error handling | Phase 5.2.F.3 (May 2025) |
| **test_physics_free_mode** | Validates core-physics separation by running core infrastructure without physics modules | Phase 5.2.F.2 (May 2025) |
| **test_parameter_validation** | Tests parameter file parsing, module discovery, and configuration errors | Phase 5.2.F.3 (Jun 2025) |
| **test_resource_management** | Comprehensive resource lifecycle validation | Phase 5.2.F.5 (Jun 2025) |
| **test_integration_workflows** | Comprehensive integration workflow validation | Phase 5.2.G (Jun 2025) |
| **test_error_recovery** | Tests system resilience and recovery from failures | Phase 5.2.G (June 2025) |
| **test_dynamic_memory_expansion** | Tests the dynamic memory expansion system for scaling to large simulations | Phase 5.3 (Jun 2025) |
| **test_data_integrity_physics_free** | Tests data integrity in physics-free mode with core properties only | Phase 5.3 (Jun 2025) |
| **test_galaxy_array** | Tests the galaxy array data structure and component management | Phase 5.3 (Jun 2025) |
| **test_galaxy_array_component** | Tests individual galaxy array component access and manipulation | Phase 5.3 (Jun 2025) |
| **test_halo_progenitor_integrity** | Validates halo progenitor relationships and data integrity | Phase 5.3 (Jun 2025) |
| **test_core_property_separation** | Tests separation between core and physics properties | Phase 5.3 (Jun 2025) |
| **test_property_separation_scientific_accuracy** | Tests scientific accuracy with property separation enabled | Phase 5.3 (Jun 2025) |
| **test_property_separation_memory_safety** | Tests memory safety with property separation enabled | Phase 5.3 (Jun 2025) |
| **test_hdf5_output_validation** | Validates HDF5 output format and property serialization | Phase 5.3 (Jun 2025) |

**Purpose**: Core infrastructure tests should always pass, as they validate the stability of the foundation on which everything else is built.

### Property System Tests (7 tests)

These tests focus on the property system, which is central to SAGE's modular architecture:

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| **test_property_serialization** | Tests property serialization for I/O operations | Phase 3.2 (Apr 2025) |
| **test_property_array_access** | Tests access to array properties | Phase 5.2.B (May 2025) |
| **test_property_system_hdf5** | Tests integration between the property system and HDF5 output with transformer functions | Phase 5.2.F.3 (May 2025) |
| **test_property_validation** | Tests validation of property definitions | Phase 3.2 (Apr 2025) |
| **test_property_access_comprehensive** | Comprehensive validation of property system for core-physics separation | Phase 5.3 (Jun 2025) |
| **test_property_yaml_validation** | Tests YAML property definition validation and parsing | Phase 5.3 (Jun 2025) |
| **test_parameter_yaml_validation** | Tests YAML parameter definition validation and parsing | Phase 5.3 (Jun 2025) |

**Purpose**: Property system tests validate both the correctness of property handling and the core-physics separation principle.

### I/O System Tests (11 tests)

These tests validate SAGE's input/output capabilities:

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| **test_io_interface** | Tests the I/O interface abstraction | Phase 3.1 (Apr 2025) |
| **test_endian_utils** | Tests cross-platform endianness handling | Phase 3.2 (Apr 2025) |
| **test_lhalo_binary** | Tests reading LHalo format merger trees | Phase 3.2 (Apr 2025) |
| **test_lhalo_hdf5** | Tests reading LHalo format trees in HDF5 format | Phase 3.2 (Apr 2025) |
| **test_gadget4_hdf5** | Tests reading Gadget4 merger trees in HDF5 format | Phase 3.2 (Apr 2025) |
| **test_genesis_hdf5** | Tests reading Genesis merger trees in HDF5 format | Phase 3.2 (Apr 2025) |
| **test_consistent_trees_hdf5** | Tests reading ConsistentTrees in HDF5 format | Phase 3.2 (Apr 2025) |
| **test_hdf5_output** | Tests the HDF5 output handler | Phase 3.2 (Apr 2025) |
| **test_io_memory_map** | Tests memory mapping | Phase 3.3 (Apr 2025), Enhanced (Jun 2025) |
| **test_io_buffer_manager** | Tests the buffered I/O system | Phase 3.3 (Apr 2025) |
| **test_validation_framework** | Tests validation framework | Phase 5.2.F.3 (May 2025) |

**Purpose**: I/O tests ensure that SAGE can correctly read merger trees and write galaxy catalogs in all supported formats.

### Module System Tests (3 tests)

These tests validate the module system that enables SAGE's pluggable architecture:

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| **test_pipeline_invoke** | Tests the pipeline invocation mechanisms | Phase 2.7 (Mar 2025) |
| **test_module_callback** | Tests the inter-module communication infrastructure | Phase 5.2.F.5 (May 2025) |
| **test_module_lifecycle** | Validates complete lifecycle of physics modules within SAGE architecture | Phase 5.3 (Jun 2025) |

**Purpose**: Module system tests ensure that physics components can be added, removed, or replaced without breaking the core functionality.

## Running Tests

### Important Note

If a test or test category fails to compile, run `make clean` first to properly update the required auto-generated property files.

### All Tests
```bash
make tests
```

### By Category
```bash
make core_tests        # Core infrastructure tests
make property_tests    # Property system tests
make io_tests          # I/O system tests
make module_tests      # Module system tests
```

### Individual Tests
```bash
make test_config_system          # Configuration system (includes intentional error message testing)
make test_pipeline
make test_property_serialization
make test_memory_pool
make test_io_memory_map
make test_integration_workflows  # Multi-system integration validation
make test_physics_free_mode      # Test core-physics separation with empty pipelines
```

### Test Execution Examples

```bash
# Run a specific test with output
make test_pipeline && ./tests/test_pipeline

# Run all core tests
make core_tests

# Run tests with verbose output
make test_integration_workflows && ./tests/test_integration_workflows --verbose
```

## Special Test Cases

### Configuration System Test (`test_config_system`)

This test **intentionally produces ERROR messages** as part of its validation process. The test exercises malformed JSON detection and error handling by testing invalid configurations such as:
- Unclosed objects: `{`
- Missing values: `{"key": }`  
- Trailing commas: `{"array": [1, 2, 3,]}`

**Expected output includes error messages like**:
```
[ERROR] [src/core/core_config_system.c:390 json_parse_array] Trailing comma in array at position 9
[ERROR] [src/core/core_config_system.c:696 config_load_file] Failed to parse configuration file: test_malformed_file.json
```

**A successful test run** exits with code 0 despite these error messages, which demonstrate correct validation of invalid JSON inputs.

### Physics-Free Mode Test (`test_physics_free_mode`)

This test validates core-physics separation by running the SAGE core infrastructure with empty pipelines that perform no physics calculations:

```c
// Test validates core-physics separation by running empty pipelines
int status = test_physics_free_execution();
// Verifies that:
// 1. Core can run with completely empty pipelines
// 2. Pipeline executes all phases correctly with zero modules
// 3. Properties pass from input to output without physics
// 4. Essential physics functions provide minimal implementations
```

## End-to-End Scientific Tests

### Test Script: `test_sage.sh`

The `test_sage.sh` script provides comprehensive validation of SAGE's scientific accuracy by comparing outputs to reference results:

#### Test Components

1. **Input Data**: Uses a subset of the Millennium Simulation merger trees, downloaded automatically if not present

2. **Execution Process**:
   - Sets up a Python environment with dependencies (numpy, h5py)
   - Runs SAGE with both binary and HDF5 output formats
   - Compares generated outputs to reference galaxy catalogs

3. **Comparison Mechanism**:
   - The `sagediff.py` script performs detailed comparisons between outputs
   - All galaxy properties (positions, masses, rates, etc.) are verified
   - Both binary-to-binary and binary-to-HDF5 comparisons are performed
   - Appropriate tolerances are applied for floating-point comparisons

4. **Output Analysis**:
   - Provides summary statistics of property differences
   - Identifies specific properties with discrepancies
   - Reports galaxy-by-galaxy differences when run with `--verbose`

#### Running End-to-End Tests

```bash
./test_sage.sh                   # Basic run
./test_sage.sh --verbose         # Detailed comparison output
./test_sage.sh --compile         # Compile SAGE before testing
```

#### Integration with Main Testing Framework

The end-to-end scientific tests are automatically run as part of `make tests`, alongside the unit tests for individual components. This provides a holistic validation approach that ensures both architectural integrity and scientific accuracy.

## Test Development Guidelines

### Adding New Tests

When adding new tests to the SAGE codebase, follow these guidelines:

1. **Category Alignment**: Add the test to the appropriate category based on what it's testing.
2. **Core-Physics Separation**: Ensure that the test respects the core-physics separation principle.
3. **Minimal Dependencies**: Keep dependencies to a minimum to avoid cascading failures.
4. **Clear Assertions**: Use clear assertions that explain what's being tested and why.
5. **Documentation**: Update the test documentation to include the new test.

### Test Template

Use the template in `docs/templates/test_template.c` for creating new tests:

```c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "core/core_allvars.h"

// Test function
static void test_new_functionality(void) {
    // Setup
    
    // Execute
    
    // Verify
    
    // Cleanup
}

int main(void) {
    printf("Starting test_new_functionality...\n");
    
    test_new_functionality();
    
    printf("All tests passed!\n");
    return EXIT_SUCCESS;
}
```

### Test Naming Conventions

- Test files: `test_<component_name>.c`
- Test functions: `test_<specific_functionality>()`
- Executable targets: `test_<component_name>`
- Make targets: `test_<component_name>`

### Validation Patterns

#### Input Validation
```c
// Always validate inputs
if (galaxy == NULL) {
    fprintf(stderr, "Error: galaxy pointer is NULL\n");
    return -1;
}
```

#### Error Handling
```c
// Test both success and failure cases
int result = function_under_test(valid_input);
assert(result == EXPECTED_SUCCESS);

int error_result = function_under_test(invalid_input);
assert(error_result == EXPECTED_ERROR);
```

#### Resource Management
```c
// Test resource cleanup
void *resource = allocate_resource();
assert(resource != NULL);

int cleanup_result = cleanup_resource(resource);
assert(cleanup_result == SUCCESS);
```

## Test Status Indicators

In test documentation, tests are marked with status indicators:

- ✅ **Passing**: The test is currently passing and aligned with current architecture
- ❌ **Failing or Outdated**: The test is failing due to architectural changes or is outdated
- ⚠️ **Warning**: The test passes but might have issues or assumptions that could be problematic

## Evolution of Testing Strategy

### Historical Changes

**Phase 1-2: Foundation and Interfaces**
- Initial tests focused on validating core infrastructure and module interfaces
- Established baseline for modular architecture

**Phase 3: I/O Abstraction**
- Added tests for I/O abstraction layer and format handlers
- Ensured SAGE could read and write data in all supported formats

**Phase 4: Plugin Infrastructure**
- Expanded module system tests for dynamic loading and inter-module communication
- Validated modules could be added and removed at runtime

**Phase 5: Core-Physics Separation**
- Most significant evolution with Properties Module architecture
- Introduced clear separation between core and physics properties
- Required updates to existing tests and addition of separation-specific tests

### Removed/Replaced Tests

**Removed Tests:**
- `test_core_physics_separation` - Functionality covered by more focused tests
- `test_output_preparation` - Replaced by `test_property_system_hdf5`
- `test_property_registration` - Incompatible with new property architecture
- `test_empty_pipeline` - Renamed to `test_physics_free_mode`

**Consolidated Tests:**
- Standalone Memory Tests Suite integrated into main Makefile
- Multiple configuration tests consolidated into `test_config_system`

## Debugging Failed Tests

### When Tests Fail

When a test fails, understand the cause:

1. **Architectural Change**: Test needs updating for new architecture
2. **Implementation Bug**: Bug in implementation needs fixing
3. **Specification Change**: Requirements changed, test needs updating

### Debugging Strategies

#### Compilation Failures
```bash
# Clean auto-generated files and rebuild
make clean
make test_<failing_test>
```

#### Runtime Failures
```bash
# Run with debugging
gdb ./tests/test_<failing_test>
(gdb) run
(gdb) bt  # Get backtrace on failure
```

#### Memory Issues
```bash
# Check for memory leaks
valgrind --leak-check=full ./tests/test_<failing_test>
```

#### Property Access Issues
```bash
# For property-related tests, verify build configuration
make clean
make full-physics  # Use maximum compatibility
make test_<failing_test>
```

### Common Issues and Solutions

**Problem**: Test fails to compile after switching build configurations
**Solution**: Run `make clean` before building tests

**Problem**: Property access errors in tests
**Solution**: Ensure test uses correct property access patterns (core vs physics properties)

**Problem**: Resource management test failures
**Solution**: Check for proper initialization and cleanup in test setup/teardown

**Problem**: Configuration test failures
**Solution**: Verify test data files exist and have correct format

## Performance Testing

### Benchmarking

SAGE includes a performance benchmarking system:

```bash
./tests/benchmark_sage.sh           # Run performance benchmark
./tests/benchmark_sage.sh --verbose # Detailed timing information
```

**Metrics Captured:**
- Wall clock time
- Maximum memory usage
- Phase-specific timing (when available)

**Results Storage:**
- JSON format in `benchmarks/` directory
- Timestamped for historical comparison
- Integration with CI/CD for regression detection

## Test Suite Statistics

The SAGE test suite contains:
- **Total tests**: 46 individual unit tests
- **Core Infrastructure**: 25 tests
- **Property System**: 7 tests  
- **I/O System**: 11 tests
- **Module System**: 3 tests
- **Test categories**: 4 main categories with specialized make targets
- **End-to-end tests**: Scientific validation via `test_sage.sh`

## Future Testing Enhancements

### Planned Improvements

**Test Coverage:**
- Automated coverage analysis
- Property-specific test generation
- Module interaction testing

**Performance Testing:**
- Automated performance regression detection
- Memory usage profiling
- Scalability testing for large datasets

**Integration Testing:**
- Cross-platform compatibility testing
- Multi-format I/O validation
- Configuration validation testing

**Quality Assurance:**
- Automated test result analysis
- Test flakiness detection
- Continuous integration improvements