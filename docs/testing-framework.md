# SAGE Testing Framework Documentation

**Purpose**: Complete guide to SAGE's CMake-based testing system for validation and quality assurance  
**Audience**: Developers working with SAGE tests, contributors adding new tests, CI/CD maintainers  
**Status**: Implemented and Ready for Use  

---

## Overview

This document describes the CMake-based testing framework implemented for SAGE. The framework provides a structured approach to testing that supports both unit tests for individual components and end-to-end scientific validation.

**Navigation**: Return to [Quick Reference](quick-reference.md) for other documentation sections.

**ARCHITECTURAL PRINCIPLE ALIGNMENT**: The testing framework supports multiple core architectural principles ([detailed in Architectural Vision](architectural-vision.md)):

- **Principle 1: Physics-Agnostic Core Infrastructure** - Core tests validate that infrastructure operates independently of physics implementations, enabling independent development and simplified testing
- **Principle 3: Metadata-Driven Architecture** - Tests validate that system structure is defined by metadata rather than hardcoded implementations  
- **Principle 6: Memory Efficiency and Safety** - Memory validation and leak detection ensure bounded, predictable memory usage that doesn't grow with simulation size
- **Principle 8: Type Safety and Validation** - Comprehensive validation provides fail-fast behavior on errors with clear debugging information, covering both architecture and scientific accuracy

## Framework Architecture

### Core Components

1. **CMake Integration**: Tests are fully integrated with the CMake build system and CTest
2. **Automatic Test Discovery**: Adding a new test requires only creating the source file
3. **Category Organization**: Tests are organized into logical categories for efficient execution
4. **End-to-End Validation**: Existing scientific validation is preserved and integrated

### Test Categories

The framework organizes tests into 5 main categories:

#### 1. Core Infrastructure Tests (`core_tests`)
Tests for **physics-agnostic core components** (Principle 1: Physics-Agnostic Core Infrastructure):
- Pipeline execution system
- Memory management and allocation
- Configuration system and parameter validation
- Property system core functionality
- **Core-physics separation validation** (ensures core operates independently)
- Galaxy data structures and arrays
- Merger processing and event queues
- FOF processing and orphan tracking

#### 2. Property System Tests (`property_tests`)
Tests for the **metadata-driven property system** (Principle 3: Metadata-Driven Architecture):
- Property serialization and access patterns
- Array property handling
- HDF5 integration with properties
- Property validation and YAML processing
- Parameter system validation

#### 3. I/O System Tests (`io_tests`)
Tests for **format-agnostic I/O** (Principle 7: Format-Agnostic I/O):
- Unified I/O interface functionality
- Multiple tree format readers (LHalo, Gadget4, Genesis, ConsistentTrees)
- HDF5 output handling and validation
- Endianness handling and cross-platform compatibility
- Memory mapping and buffered I/O operations
- I/O validation frameworks

#### 4. Module System Tests (`module_tests`)
Tests for **runtime modularity** (Principle 2: Runtime Modularity):
- Inter-module communication
- Module callback systems
- Complete module lifecycle management

#### 5. Tree-Based Processing Tests (`tree_tests`)
Tests for **unified processing model** (Principle 5: Unified Processing Model):
- Galaxy inheritance in tree processing mode

### End-to-End Scientific Validation

The framework includes comprehensive end-to-end testing via the existing `test_sage.sh` script:
- Downloads Mini-Millennium test data automatically
- Runs SAGE with binary output format and compares to reference data
- Runs SAGE with HDF5 output format and compares to reference data
- Uses `sagediff.py` for detailed property-by-property comparison
- Supports both serial and MPI execution modes

## Using the Testing Framework

### Basic Commands

```bash
# Run complete test suite (unit tests + end-to-end scientific validation)
make test                        # Minimal output (only summary)

# To see detailed test output:
ctest --output-on-failure        # Show output on failure (recommended)
ctest -V                         # Verbose output (shows all output)
ctest --verbose                  # Same as -V (shows all test output)

# Alternative custom target with integrated output
cmake --build . --target tests   # Custom test target with more output
```

### Test Output Visibility

By default, `make test` shows only a summary. To see actual test execution details:

- **Recommended**: `ctest --output-on-failure` - Shows detailed output only when tests fail
- **Debug Mode**: `ctest -V` - Shows all test output including successful tests  
- **Specific Test**: `ctest -R test_name -V` - Run and show output for specific test
- **Custom Target**: `cmake --build . --target tests` - Uses custom target with enhanced output

### Development Testing (Fast Iteration)

```bash
# Run only unit tests (fast, no downloads or long simulations)
cmake --build . --target unit_tests
```

### Category-Specific Testing

```bash
# Test specific categories
cmake --build . --target core_tests
cmake --build . --target property_tests
cmake --build . --target io_tests
cmake --build . --target module_tests
cmake --build . --target tree_tests
```

### Individual Test Execution

```bash
# Run specific tests
ctest -R test_name                    # Run tests matching pattern
ctest -R test_pipeline               # Run specific test
ctest --output-on-failure           # Show detailed output on failure
ctest -V                            # Verbose output
```

### Available Tests

Currently implemented tests:
- **test_pipeline**: Example test demonstrating framework usage and core pipeline functionality
- **sage_end_to_end**: Complete scientific validation via test_sage.sh

The framework is ready for additional tests - simply create `tests/test_name.c` and it will be automatically detected and integrated.

## Executable Location and Build Systems

### Build System Architecture

SAGE uses a **single, modern build system**:

**CMake Build System** (out-of-tree builds in `build/` directory):
- Creates executable at `build/sage`
- Run with: `mkdir build && cd build && cmake .. && make`
- Generates CMake-managed Makefiles in `build/` directory
- Fully compatible with current modular source structure (`src/core/`, `src/physics/`, `src/io/`)
- Complete dependency detection (GSL, HDF5, MPI)
- Comprehensive testing framework integration
- Professional development workflow support

**Fresh Clone Setup**:
The `build/` directory is excluded from git (as it should be). For a fresh repository clone:

```bash
git clone <repository>
cd sage-model
mkdir build && cd build && cmake .. && make
```

**Legacy System Removed**:
- Previous Makefile moved to `scrap/` directory  
- CMakeLists.txt contains all necessary build information
- Cleaner codebase with single, maintained build system

### Test Script Intelligence

The testing framework automatically detects executables:

1. **CMake Integration**: When run via `make test` or `ctest`, uses `SAGE_EXECUTABLE` environment variable pointing to `build/sage`
2. **Direct Execution**: When run manually (e.g., `./tests/test_sage.sh`), intelligently searches:
   - `SAGE_EXECUTABLE` environment variable (if set)
   - `../build/sage` (CMake build when run from tests/ directory)  
   - `build/sage` (CMake build when run from root directory)
   - `./sage` (fallback for any manually placed executable)

### Build System Usage

**Standard Development Workflow**:

```bash
# Fresh clone setup
git clone <repository>
cd sage-model
mkdir build && cd build && cmake .. && make

# Daily development cycle
make                     # Build
make test               # Test (or ctest -V for verbose)
make clean              # Clean
```

**CMake Benefits**:
- Out-of-tree builds (keeps source clean, build/ excluded from git)
- Complete testing framework integration  
- Modern dependency detection and IDE support
- Compatible with modular source architecture
- Automatic test discovery and execution
- Single, maintained build system

## Adding New Tests

### Simple Process

1. **Create test file**: `tests/test_your_name.c` using the template
2. **Reconfigure CMake**: `cmake ..` (from build directory)
3. **Build and run**: `cmake --build . --target test_your_name && ctest -R test_your_name`

### Unit Test Template

For standardized test development, use the template at `docs/templates/test_template.md`. The template provides:

- **Professional structure**: Clear organization with proper documentation headers
- **Consistent output**: Standardized printf formatting and test result reporting
- **Test categories**: Organized approach covering initialization, functionality, error handling, edge cases, and integration
- **CMake integration**: Automatic detection and build system integration
- **Simple assertions**: Uses standard `assert()` for critical failures
- **Result tracking**: Proper test counting and pass/fail reporting

### Template Usage Guidelines

1. **Copy template structure**: Use the template at `docs/templates/test_template.md` as starting point
2. **Follow naming patterns**: Name tests `test_component_name.c` for automatic categorization
3. **Implement all sections**: Cover initialization, basic functionality, error handling, edge cases, and integration
4. **Use clear output**: Follow template's printf formatting for consistent test output
5. **Return proper codes**: 0 for success, non-zero for failure

### Basic Test Example

```c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
    printf("Running %s...\n", __FILE__);
    
    // Your test implementation here
    int result = 0;  // 0 = success, non-zero = failure
    
    // Example test logic
    assert(1 == 1);  // Use assert() for critical failures
    
    if (result == 0) {
        printf("%s PASSED\n", __FILE__);
    } else {
        printf("%s FAILED\n", __FILE__);
    }
    
    return result;
}
```

### Automatic Integration

The CMake system automatically:
- Detects new test files in the `tests/` directory
- Creates executable targets linking with the SAGE library
- Registers tests with CTest for individual execution
- Assigns tests to appropriate categories based on naming patterns
- Provides all necessary include directories and library links

## Test Execution Details

### Unit Test Behavior

- **Fast execution**: Unit tests should complete in seconds
- **No external dependencies**: Unit tests should not require downloads or external data
- **Isolated testing**: Each test should be independent and not rely on other tests
- **Clear output**: Tests should provide clear success/failure messages

### End-to-End Test Behavior

- **Comprehensive validation**: Tests complete SAGE simulation pipeline
- **Scientific accuracy**: Compares results against known-good reference data
- **Format validation**: Tests both binary and HDF5 output formats
- **Automatic setup**: Downloads required test data if not present
- **Longer execution**: May take several minutes to complete

### Test Dependencies

- **GSL Required**: End-to-end scientific tests require GSL (GNU Scientific Library)
- **HDF5 Optional**: Some tests may require HDF5 libraries for HDF5 format testing
- **MPI Optional**: Tests support MPI execution when available

## Framework Status

### Currently Available

- ✅ **Complete CMake integration** with CTest
- ✅ **Automatic test discovery** and build system integration
- ✅ **Category-based organization** for efficient test execution
- ✅ **End-to-end scientific validation** preserving existing test_sage.sh functionality
- ✅ **Example test implementation** (test_pipeline.c) demonstrating framework usage

### Ready for Development

- ✅ **Test placeholders** for all 5 categories - framework knows where to put new tests
- ✅ **Build system integration** - new tests automatically compile and link with SAGE
- ✅ **Documentation** - this guide provides everything needed to add tests
- ✅ **Professional workflow** - follows CMake/CTest best practices

## Troubleshooting

### Common Issues

1. **Test not found**: Ensure test file exists as `tests/test_name.c` and run `cmake ..` to reconfigure
2. **Build errors**: Check that test includes necessary SAGE headers and links properly
3. **GSL required for end-to-end**: Install GSL or run unit tests only with `cmake --build . --target unit_tests`

### Debug Mode

```bash
# Build in debug mode for better error messages
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Run tests with verbose output
ctest -V -R test_name
```

### Memory Checking

```bash
# Enable AddressSanitizer for memory error detection
cmake .. -DSAGE_MEMORY_CHECK=ON
```

## Integration with Development Workflow

### Daily Development Cycle

```bash
# Configure project (once per session)
mkdir build && cd build
cmake ..

# Fast development iteration
cmake --build . --target unit_tests          # Quick validation (~30 seconds)
# ... implement features ...
cmake --build . --target core_tests          # Category-specific validation
# ... refine implementation ...
make test                                     # Full validation before commits
```

### Pre-Commit Validation

Before committing changes, ensure:
1. All unit tests pass: `cmake --build . --target unit_tests`
2. Category tests pass: `cmake --build . --target <category>_tests`
3. Full test suite passes: `make test` (if time permits)

## Future Development

The framework is designed to grow with the SAGE codebase. As new features are implemented:

1. **Add corresponding tests** in the appropriate category
2. **Tests automatically integrate** into the build system and execution workflow
3. **Category organization** keeps testing manageable and efficient
4. **Professional standards** ensure tests are maintainable and reliable

## Summary

The SAGE testing framework provides a professional, scalable foundation for ensuring code quality throughout development. It combines the convenience of automatic test discovery with the rigor of comprehensive scientific validation, supporting both rapid development iteration and thorough pre-commit validation.

---

## Quick Reference

```bash
# Essential commands
make test                                # Complete test suite (minimal output)
ctest -V                                 # Complete test suite (verbose output)  
ctest --output-on-failure               # Complete test suite (show failures)
cmake --build . --target unit_tests     # Fast unit tests only
ctest -R test_name                       # Individual test execution

# Add new test
# 1. Create tests/test_name.c
# 2. Run: cmake .. && cmake --build . --target test_name
# 3. Test: ctest -R test_name -V (use -V to see output)
```