# SAGE Testing Architecture Guide

## Overview

This document describes the overall testing architecture of SAGE, explaining the purpose and organization of different test categories, how the testing strategy has evolved with the architecture, and how to interpret test results in the context of ongoing refactoring.

## Testing Philosophy

The SAGE testing framework is built around three key principles:

1. **Separation of Concerns**: Tests are categorized based on the component they validate, with clear boundaries between core infrastructure, property system, I/O, module system, and core-physics separation.

2. **Evolution with Architecture**: The testing strategy evolves as the architecture changes, with new tests added to validate new components and outdated tests either updated or deprecated.

3. **Fail-Fast Development**: Tests should fail early and explicitly when assumptions are violated, providing clear guidance on what needs to be fixed.

## Test Categories and Their Purpose

SAGE tests are organized into several categories, each with a specific focus:

### Core Infrastructure Tests

These tests validate the fundamental infrastructure components of SAGE:

- **Pipeline System**: Tests the execution of modules in different phases
- **Array Utilities**: Tests dynamic array manipulation functions
- **Diagnostics**: Tests the collection and reporting of evolution metrics
- **Evolution Integration**: Tests the refactored evolution loop with phase-based execution

Core infrastructure tests should always pass, as they validate the stability of the foundation on which everything else is built.

### Property System Tests

These tests focus on the property system, which is central to SAGE's modular architecture:

- **Property Serialization**: Tests conversion of properties for I/O operations
- **Property Validation**: Tests validation of property definitions
- **Property Registration**: Tests registration of properties with the extension system
- **Property Access**: Tests access to properties via macros and generic functions
- **Property HDF5 Integration**: Tests integration between properties and HDF5 output

The property system tests validate both the correctness of property handling and the core-physics separation principle.

### I/O System Tests

These tests validate SAGE's input/output capabilities:

- **I/O Interface**: Tests the abstraction layer for different formats
- **Endianness**: Tests cross-platform compatibility
- **Format Handlers**: Tests reading and writing in various formats (LHalo, HDF5, etc.)
- **Buffering and Memory Mapping**: Tests performance optimizations for I/O, including cross-platform memory mapping and file buffering

I/O tests ensure that SAGE can correctly read merger trees and write galaxy catalogs in all supported formats.

### Module System Tests

These tests validate the module system that enables SAGE's pluggable architecture:

- **Dynamic Library Loading**: Tests loading modules at runtime
- **Pipeline Invocation**: Tests interaction between modules in the pipeline

Module system tests ensure that physics components can be added, removed, or replaced without breaking the core functionality.

### End-to-End Scientific Tests

These tests validate the scientific accuracy of SAGE by comparing outputs to reference results:

- **Binary Output Tests**: Verify that SAGE produces correct results in binary format
- **HDF5 Output Tests**: Verify that SAGE produces correct results in HDF5 format
- **Cross-Format Validation**: Ensures consistency between different output formats
- **Galaxy Property Verification**: Checks all galaxy properties against reference values

End-to-end tests provide holistic validation of SAGE's scientific calculations, ensuring that architectural changes don't affect the correctness of the galaxy formation model.

### Core-Physics Separation Tests

These tests specifically validate the separation between core infrastructure and physics implementations:

- **Core-Physics Separation**: Tests that core code has no direct dependencies on physics properties
- *(Removed)* **Output Preparation**: Previously tested the monolithic output preparation module, now replaced by Property System HDF5 test

These tests are crucial for ensuring the architectural goal of runtime functional modularity.

## Test Status Indicators

In the test documentation, tests are marked with status indicators:

- ✅ **Passing**: The test is currently passing and is aligned with the current architecture
- ❌ **Failing or Outdated**: The test is either failing due to architectural changes or is outdated and needs updating
- ⚠️ **Warning**: The test passes but might have issues or assumptions that could be problematic

## Evolution of Testing Strategy

The SAGE testing strategy has evolved significantly through the refactoring phases:

### Phase 1-2: Foundation and Interfaces

Initial tests focused on validating the core infrastructure and module interfaces. These tests established the baseline for the modular architecture.

### Phase 3: I/O Abstraction

Tests were added to validate the I/O abstraction layer and format handlers. These tests ensured that SAGE could read and write data in all supported formats.

### Phase 4: Plugin Infrastructure

Tests for the module system were expanded to validate dynamic loading and inter-module communication. These tests ensured that modules could be added and removed at runtime.

### Phase 5: Core-Physics Separation

The most significant evolution in testing strategy occurred in Phase 5, with the implementation of the Properties Module architecture. This introduced a clear separation between core and physics properties, requiring updates to existing tests and the addition of new tests specifically focused on this separation.

## Legacy Tests and Their Status

Some tests in the SAGE codebase were designed for earlier architectural approaches and are now considered legacy:

### Removed: test_property_registration

This test was designed to validate the registration of standard properties with the extension system in the original architecture, where:

1. Properties were stored as direct fields in the `GALAXY` structure
2. Extension data pointers directly referenced these fields

With the implementation of the Properties Module architecture:

1. Properties are now stored in the `galaxy_properties_t` structure
2. Direct fields for physics properties have been removed from the `GALAXY` structure
3. The test's assumptions about direct field access are no longer valid

This test has been removed from the codebase as its functionality is covered by newer tests like `test_property_array_access` and `test_property_system_hdf5`.

### test_galaxy_property_macros

This test was designed to validate the `GALAXY_PROP_*` macros for property access. With the core-physics separation principle, these macros are now only valid for core properties, while physics properties must be accessed through generic functions.

The test needs to be updated to reflect this distinction, or it could be consolidated with other property access tests.

## When Tests Fail

When a test fails, it's important to understand why:

1. **Architectural Change**: The test might be failing because the architecture has changed, and the test needs to be updated to reflect the new approach.

2. **Implementation Bug**: The test might be failing because there's a bug in the implementation that needs to be fixed.

3. **Specification Change**: The test might be failing because the requirements have changed, and the test needs to be updated to reflect the new expectations.

For tests that fail due to architectural changes, the correct approach is to either update the test to align with the new architecture or retire it if its functionality is covered by newer tests.

## Adding New Tests

When adding new tests to the SAGE codebase, follow these guidelines:

1. **Category Alignment**: Add the test to the appropriate category based on what it's testing.

2. **Core-Physics Separation**: Ensure that the test respects the core-physics separation principle.

3. **Minimal Dependencies**: Keep dependencies to a minimum to avoid cascading failures.

4. **Clear Assertions**: Use clear assertions that explain what's being tested and why.

5. **Documentation**: Update the test documentation to include the new test.

## Running Tests

Tests can be run in several ways:

1. **All Tests**: Run all tests to ensure overall system integrity.
   ```bash
   make tests
   ```

2. **Category Tests**: Run tests for a specific category.
   ```bash
   make core_tests
   make property_tests
   make io_tests
   make module_tests
   ```

3. **Individual Tests**: Run a specific test to focus on a particular component.
   ```bash
   make test_pipeline
   make test_property_array_access
   ```

4. **End-to-End Scientific Tests**: Run scientific validation against reference outputs.
   ```bash
   ./test_sage.sh                   # Basic run
   ./test_sage.sh --verbose         # Detailed comparison output
   ./test_sage.sh --compile         # Compile SAGE before testing
   ```

## End-to-End Scientific Tests

The `test_sage.sh` script provides comprehensive validation of SAGE's scientific accuracy by comparing outputs to reference results:

### Test Components

1. **Input Data**: Uses a subset of the Millennium Simulation merger trees, which are downloaded automatically if not present

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

### Integration with Main Testing Framework

The end-to-end scientific tests are automatically run as part of `make tests`, alongside the unit tests for individual components. This provides a holistic validation approach that ensures both architectural integrity and scientific accuracy.

## Conclusion

The SAGE testing architecture is designed to evolve with the codebase, providing continuous validation of both individual components and their integration. By understanding the purpose of each test category and how tests relate to the architectural principles, developers can effectively use the testing framework to maintain and extend SAGE while preserving its modular design.
