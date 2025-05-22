# SAGE Unit Tests

## Overview

The SAGE test suite provides comprehensive validation of the codebase through categorised unit tests and specialised integration tests. Tests are organised to match the Makefile structure for consistency.

## Integrated Tests (Main Makefile)

These tests are integrated into the main Makefile and can be run with `make tests` or by individual category.

### Core Infrastructure Tests (`CORE_TESTS`)

| Test Name                         | Purpose                                                                       | Added in Phase           |
| --------------------------------- | ----------------------------------------------------------------------------- | ------------------------ |
| ✅ **test_pipeline**               | Tests the pipeline execution system with 4 phases (HALO, GALAXY, POST, FINAL) | Phase 2.5 (Mar 2025)     |
| ✅ **test_array_utils**            | Tests the array utility functions for dynamic array manipulation              | Phase 3.3 (Apr 2025)     |
| ❌ **test_evolution_diagnostics**  | Tests the diagnostics system for the galaxy evolution process                 | Phase 4.7 (Apr 2025)     |
| ❌ **test_evolve_integration**     | Tests the refactored evolve_galaxies loop with phase-based execution          | Phase 5.1 (Apr 2025)     |
| ✅ **test_core_property**          | Tests the property system's core functionality with mock implementations      | Phase 5.2.B (May 2025)   |
| ✅ **test_core_pipeline_registry** | Tests the pipeline registry for module registration and pipeline creation     | Phase 5.2.F (May 2025)   |
| ✅ **test_dispatcher_access**      | Tests the type-safe dispatcher functions for property access                  | Phase 5.2.F.4 (May 2025) |

### Property System Tests (`PROPERTY_TESTS`)

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| ✅ **test_property_serialization** | Tests property serialization for I/O operations | Phase 3.2 (Apr 2025) |
| ✅ **test_property_validation** | Tests validation of property definitions | Phase 3.2 (Apr 2025) |
| ❌ **test_property_system** | Tests the core property system | Phase 5.2.B (May 2025) |
| ❌ **test_property_registration** | Tests registration of properties with the extension system | Phase 5.2.B (May 2025) |
| ✅ **test_property_array_access** | Tests access to array properties | Phase 5.2.B (May 2025) |
| ❌ **test_galaxy_property_macros** | Tests the GALAXY_PROP_* macros for property access | Phase 5.2.D (May 2025) |
| ✅ **test_property_system_hdf5** | Tests integration between the property system and HDF5 output with transformer functions | Phase 5.2.F.3 (May 2025) |

### I/O System Tests (`IO_TESTS`)

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| ✅ **test_io_interface** | Tests the I/O interface abstraction | Phase 3.1 (Apr 2025) |
| ✅ **test_endian_utils** | Tests cross-platform endianness handling | Phase 3.2 (Apr 2025) |
| ✅ **test_lhalo_binary** | Tests reading LHalo format merger trees | Phase 3.2 (Apr 2025) |
| ✅ **test_hdf5_output** | Tests the HDF5 output handler | Phase 3.2 (Apr 2025) |
| ✅ **test_lhalo_hdf5** | Tests reading LHalo format trees in HDF5 format | Phase 3.2 (Apr 2025) |
| ✅ **test_gadget4_hdf5** | Tests reading Gadget4 merger trees in HDF5 format | Phase 3.2 (Apr 2025) |
| ✅ **test_genesis_hdf5** | Tests reading Genesis merger trees in HDF5 format | Phase 3.2 (Apr 2025) |
| ✅ **test_consistent_trees_hdf5** | Tests reading ConsistentTrees in HDF5 format | Phase 3.2 (Apr 2025) |
| ❌ **test_io_validation** | Tests validation of I/O operations | Phase 3.2 (Apr 2025) |
| ✅ **test_memory_map** | Tests memory mapping for I/O operations | Phase 3.3 (Apr 2025) |
| ✅ **test_io_buffer_manager** | Tests the buffered I/O system | Phase 3.3 (Apr 2025) |

### Module System Tests (`MODULE_TESTS`)

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| ✅ **test_pipeline_invoke** | Tests the pipeline invocation mechanisms | Phase 2.7 (Mar 2025) |
| ✅ **test_dynamic_library** | Tests dynamic loading of modules | Phase 4.1 (Apr 2025) |

### Core-Physics Separation Tests (`SEPARATION_TESTS`)

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| ❌ **test_output_preparation** | Tests the output preparation module with transformer functions | Phase 5.2.E (May 2025) |
| ❌ **test_core_physics_separation** | Validates complete separation between core and physics components | Phase 5.2.F (May 2025) |

## Standalone Tests (Individual Makefiles/Scripts)

These tests require special setup or have complex integration requirements and use individual Makefiles or scripts.

| Test Name | Purpose | Added in Phase | Implementation Method |
|-----------|---------|---------------|----------------------|
| ❌ **Memory Tests Suite** | Comprehensive memory allocation, pool management, and leak detection | Phase 3.3+ (Apr 2025) | `Makefile.memory_tests` + `run_all_memory_tests.sh` |
| ❌ **test_empty_pipeline** | Verifies that the core can run with empty placeholder modules | Phase 5.2.F.2 (May 2025) | `run_empty_pipeline_test.sh` |

## Running Tests

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
make separation_tests  # Core-physics separation tests
```

### Individual Tests
```bash
make test_pipeline
make test_property_system
# etc.
```

### Standalone Tests
```bash
make test_extensions
cd tests && make -f Makefile.memory_tests
./tests/run_empty_pipeline_test.sh
```

## Test Categories Overview

- **Core Infrastructure (7 tests)**: Pipeline execution, property core functionality, array utilities, evolution diagnostics
- **Property System (7 tests)**: Property registration, serialization, validation, HDF5 integration, macro access
- **I/O System (11 tests)**: All supported tree formats, endianness, validation, buffering, memory mapping
- **Module System (2 tests)**: Dynamic loading and pipeline invocation
- **Core-Physics Separation (2 tests)**: Validation of architectural separation and output preparation

The test suite provides comprehensive coverage of all major SAGE components whilst maintaining clear separation between unit tests (expected to pass during development) and scientific validation tests (may fail during refactoring phases).
