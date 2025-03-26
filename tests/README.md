# SAGE Model Testing Framework

This directory contains the testing framework for the SAGE (Semi-Analytic Galaxy Evolution) model. The framework provides tools to verify that model changes preserve the scientific accuracy of galaxy formation simulations, relative to a set of reference outputs.

## Overview

The testing framework consists of two main components:

1. **Main Test Suite**: End-to-end scientific validation
   - Downloads a small set of merger trees (Mini-Millennium) if not present
   - Sets up a Python environment with required dependencies
   - Runs SAGE with different output formats (binary and HDF5)
   - Compares outputs with reference results to ensure consistency

2. **Pipeline System Test**: Module infrastructure validation (test_pipeline.c)
   - Validates the Module Pipeline System (Phase 2.5)
   - Tests pipeline initialization, step execution, and event handling
   - Run with: `make -f Makefile.test && ./test_pipeline`

## Quick Start

To run the tests:

```bash
# Basic test run
./test_sage.sh

# Run tests with verbose output
./test_sage.sh --verbose

# Compile SAGE before running tests
./test_sage.sh --compile

# Compile SAGE and run tests with verbose output
./test_sage.sh --compile --verbose

# Show help message
./test_sage.sh --help
```

## Directory Structure

- `test_sage.sh` - Main test script that orchestrates the testing process
- `sagediff.py` - Python script for comparing SAGE output files
- `test_data/` - Contains test input data (merger trees) and reference SAGE outputs
- `test_results/` - Contains outputs generated during testing (created upon first run)
- `test_env/` - Python virtual environment with testing dependencies (created upon first run)

## How Testing Works

### Input Data

The tests use a subset of the Millennium Simulation merger trees, which are automatically downloaded if not present. These trees provide a simplified but realistic scenario for galaxy formation.

### Test Execution Flow

1. **Environment Setup**:
   - First-time setup creates a Python virtual environment with required dependencies
   - Subsequent runs reuse the existing environment

2. **Binary Output Test**:
   - Runs SAGE with binary output format
   - Compares output to reference binary files

3. **HDF5 Output Test**:
   - Runs SAGE with HDF5 output format
   - Compares output to reference binary files

4. **Output Validation**:
   - All galaxy properties are compared between test outputs and reference files
   - Floating-point comparisons use appropriate tolerances

### Output Comparison

The `sagediff.py` script performs detailed comparisons between outputs:

- **Binary-to-Binary Comparison**: Directly compares reference binary and run binary outputs
- **Binary-to-HDF5 Comparison**: Maps between reference binary and run HDF5 formats, matching snapshots by redshift
- **Property Verification**: Checks all galaxy properties, including positions, masses, and rates
- **Tolerance Settings**: Uses both relative and absolute tolerances for floating-point comparisons

## Command Line Options

The test script supports several options:

- **--verbose**: Enables verbose output during testing, showing detailed comparison information
- **--compile**: Compiles SAGE before running tests (useful when making code changes)
- **--help**: Displays help information and usage examples

## Using Tests During Development

When making changes to the SAGE model:

1. Make code changes to the model
2. Test your changes with `./test_sage.sh --compile`
3. If tests fail, use `--verbose` to see detailed comparison information
4. Fix any issues and repeat until all tests pass

## Required Dependencies

- **Base**: bash, C compiler, Python 3 (or virtualenv)
- **Automatic**: numpy, h5py (installed automatically in virtual environment)

## Adding New Tests

To add new test cases:

1. Add merger trees to `test_data/`
2. Run SAGE with the verified version to generate reference outputs
3. Place reference outputs in `test_data/` with the naming pattern `reference-sage-output_*`
4. Update `test_sage.sh` if needed to include the new tests

## Troubleshooting

- **Missing Test Data**: Run the test script, which will download missing data
- **Failed Tests**: Use the `--verbose` option to see detailed output
- **Environment Issues**: Remove the `test_env/` directory to force recreation
- **Manual Inspection**: Examine the generated outputs in `test_results/`