# SAGE Unit Tests

## Overview

The SAGE test suite provides comprehensive validation of the codebase through categorised unit tests and specialised integration tests. Tests are organised to match the Makefile structure for consistency.

## Integrated Tests (Main Makefile)

These tests are integrated into the main Makefile and can be run with `make tests` or by individual category.

### Core Infrastructure Tests (`CORE_TESTS`)

| Test Name                       | Purpose                                                                       | Added in Phase           |
| ------------------------------- | ----------------------------------------------------------------------------- | ------------------------ |
| **test_pipeline**               | Tests the pipeline execution system with 4 phases (HALO, GALAXY, POST, FINAL) | Phase 2.5 (Mar 2025)     |
| **test_array_utils**            | Tests the array utility functions for dynamic array manipulation              | Phase 3.3 (Apr 2025)     |
| **test_evolution_diagnostics**  | Tests the diagnostics system for the galaxy evolution process                 | Phase 4.7 (Apr 2025)     |
| **test_evolve_integration**     | Tests the refactored evolve_galaxies loop with phase-based execution          | Phase 5.1 (Apr 2025)     |
| **test_core_property**          | Tests the property system's core functionality with mock implementations      | Phase 5.2.B (May 2025)   |
| **test_core_pipeline_registry** | Tests the pipeline registry for module registration and pipeline creation     | Phase 5.2.F (May 2025)   |
| **test_dispatcher_access**      | Tests the type-safe dispatcher functions for property access                  | Phase 5.2.F.4 (May 2025) |
| **test_memory_pool**            | Tests the memory pooling system with support for various property types and dynamic arrays | Phase 3.3 (Apr 2025) |
| **test_config_system**          | Tests JSON configuration loading, parsing, nested paths, and error handling   | Phase 5.2.F.3 (May 2025) |
| **test_merger_queue**           | Tests the merger event queue system for deferred merger processing            | Phase 5.1 (May 2025)     |
| **test_core_merger_processor**  | Tests physics-agnostic merger event handling and dispatching                  | Phase 5.2.G (May 2025)   |
| **test_physics_free_mode**      | Validates core-physics separation by running core infrastructure without physics modules | Phase 5.2.F.2 (May 2025) |
| **test_parameter_validation**   | Tests parameter file parsing, module discovery, and configuration errors      | Phase 5.2.F.3 (Jun 2025) |
| **test_resource_management**    | Comprehensive resource lifecycle validation (memory, HDF5 handles, file descriptors, module resources) | Phase 5.2.F.5 (Jun 2025) |

### Property System Tests (`PROPERTY_TESTS`)

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| **test_property_serialization** | Tests property serialization for I/O operations | Phase 3.2 (Apr 2025) |
| **test_property_validation** | Tests validation of property definitions | Phase 3.2 (Apr 2025) |
| **test_property_array_access** | Tests access to array properties | Phase 5.2.B (May 2025) |
| **test_property_system_hdf5** | Tests integration between the property system and HDF5 output with transformer functions | Phase 5.2.F.3 (May 2025) |
| **test_property_access_comprehensive** | Comprehensive validation of property system for core-physics separation | Phase 5.3 (Jun 2025) |

### I/O System Tests (`IO_TESTS`)

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

### Module System Tests (`MODULE_TESTS`)

| Test Name | Purpose | Added in Phase |
|-----------|---------|---------------|
| **test_pipeline_invoke** | Tests the pipeline invocation mechanisms | Phase 2.7 (Mar 2025) |
| **test_module_callback** | Tests the inter-module communication infrastructure | Phase 5.2.F.5 (May 2025) |
| **test_module_lifecycle** | Validates complete lifecycle of physics modules within SAGE architecture | Phase 5.3 (Jun 2025) |

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
```

### Individual Tests
```bash
make test_config_system    # Configuration system (includes intentional error message testing)
make test_pipeline
make test_property_serialization
make test_memory_pool
make test_io_memory_map
make test_empty_pipeline  # Test core-physics separation with placeholder modules
# etc.
```

### Important Test Notes

#### Configuration System Test (`test_config_system`)
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

## Test Categories Overview

- **Core Infrastructure (14 tests)**: Configuration system, pipeline execution, property core functionality, array utilities, evolution diagnostics, memory pooling, core-physics separation validation, merger queue system, parameter validation, resource management
- **Property System (5 tests)**: Property serialization, validation, HDF5 integration, array access, comprehensive property system validation  
- **I/O System (11 tests)**: All supported tree formats, endianness, validation, buffering, memory mapping
- **Module System (3 tests)**: Pipeline invocation, module callback system, module lifecycle management

The test suite provides comprehensive coverage of all major SAGE components whilst maintaining clear separation between unit tests (expected to pass during development) and scientific validation tests (may fail during refactoring phases).

**Note**: Several tests have been removed or replaced as part of the architectural evolution:
- `test_core_physics_separation` was removed in May 2025 as its functionality is now covered by more focused tests
- `test_output_preparation` was replaced by `test_property_system_hdf5` which tests the physics output transformers system (see `physics_output_transformers_guide.md`)
- The standalone Memory Tests Suite was integrated into the main Makefile with `test_memory_pool` and `test_io_memory_map`
- `test_empty_pipeline` was renamed to `test_physics_free_mode` to better reflect its purpose of validating core-physics separation
