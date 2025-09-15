# SAGE Performance Benchmarking Guide

**Purpose**: Comprehensive guide to measuring and tracking SAGE performance using built-in benchmarking tools  
**Audience**: Developers optimizing performance, researchers comparing configurations, CI/CD maintainers  
**Status**: Active performance measurement system  

---

## Overview

This guide explains how to use SAGE's performance benchmarking tools to measure and track the performance of the galaxy formation model over time.

**Navigation**: Return to [Quick Reference](quick-reference.md) for other documentation sections.

**ARCHITECTURAL PRINCIPLE ALIGNMENT**: Performance benchmarking supports core architectural principles ([detailed in Architectural Vision](architectural-vision.md)):

- **Principle 6: Memory Efficiency and Safety** - Ensures memory usage is bounded, predictable, and safe, with automatic leak detection and prevention
- **Principle 8: Type Safety and Validation** - Provides performance validation frameworks with fail-fast behavior on performance regressions and clear error reporting

## Overview

The SAGE benchmarking system provides comprehensive performance measurement capabilities to help developers:
- Track performance changes across code versions
- Compare different build configurations (Debug vs Release)
- Evaluate the impact of optimizations
- Monitor memory usage and runtime characteristics
- Test both serial and parallel execution performance

## Quick Start

### Basic Benchmark

```bash
# From the root directory
cd tests/
./benchmark_sage.sh
```

This runs a benchmark with default settings (binary output format) and saves results to `benchmarks/baseline_YYYYMMDD_HHMMSS.json`.

### Common Usage Patterns

```bash
# Verbose output with timing details
./benchmark_sage.sh --verbose

# Benchmark HDF5 output format
./benchmark_sage.sh --format hdf5

# Release build benchmark (optimal performance)
CMAKE_BUILD_TYPE=Release ./benchmark_sage.sh

# MPI parallel benchmark
MPI_RUN_COMMAND="mpirun -np 4" ./benchmark_sage.sh

# Get help and full documentation
./benchmark_sage.sh --help
```

## Understanding Benchmark Results

### JSON Output Structure

The benchmark creates a timestamped JSON file with the following structure:

```json
{
  "timestamp": "2025-01-01T12:00:00Z",
  "version": "abc123f",
  "system": {
    "uname": "Darwin 24.6.0 x86_64",
    "platform": "Darwin",
    "architecture": "x86_64", 
    "cpu_count": "8"
  },
  "test_case": {
    "name": "Mini-Millennium",
    "output_format": "binary",
    "num_processes": 1,
    "mpi_command": "none"
  },
  "configuration": {
    "build_type": "Debug",
    "cmake_build": true,
    "sage_executable": "/path/to/build/sage"
  },
  "overall_performance": {
    "wall_time_seconds": 45.23,
    "max_memory_mb": 124.5,
    "output_file_size_bytes": 2048576
  }
}
```

### Key Metrics

- **wall_time_seconds**: Total execution time (wall clock)
- **max_memory_mb**: Peak memory usage (resident set size)
- **output_file_size_bytes**: Size of generated output files
- **version**: Git commit hash for reproducibility
- **build_type**: CMake build configuration (Debug/Release/etc.)

## Comparing Performance

### Basic Comparison

```bash
# Compare two benchmark runs
diff benchmarks/baseline_old.json benchmarks/baseline_new.json

# Extract key metrics from multiple runs
grep "wall_time_seconds\|max_memory_mb" benchmarks/baseline_*.json

# View performance trends
ls -la benchmarks/ | head -10
```

### Performance Analysis Workflow

1. **Establish Baseline**: Run benchmark before making changes
2. **Make Changes**: Implement optimizations or new features
3. **Re-benchmark**: Run benchmark again with same configuration
4. **Compare Results**: Look for significant changes in timing/memory
5. **Investigate**: If performance degrades, profile specific components

### Example Comparison

```bash
# Before optimization
./benchmark_sage.sh
# Result: wall_time_seconds: 45.23, max_memory_mb: 124.5

# After optimization  
./benchmark_sage.sh
# Result: wall_time_seconds: 32.10, max_memory_mb: 118.2

# Improvement: ~29% faster, ~5% less memory
```

## Advanced Usage

### Build Configuration Testing

Test different build types to understand performance characteristics:

```bash
# Debug build (default) - includes debugging symbols, less optimization
CMAKE_BUILD_TYPE=Debug ./benchmark_sage.sh

# Release build - full optimization, no debug symbols
CMAKE_BUILD_TYPE=Release ./benchmark_sage.sh

# Release with debug info - optimized but debuggable
CMAKE_BUILD_TYPE=RelWithDebInfo ./benchmark_sage.sh
```

### MPI Performance Testing

Evaluate parallel scaling performance:

```bash
# Serial run
./benchmark_sage.sh

# Parallel runs with different process counts
MPI_RUN_COMMAND="mpirun -np 2" ./benchmark_sage.sh
MPI_RUN_COMMAND="mpirun -np 4" ./benchmark_sage.sh
MPI_RUN_COMMAND="mpirun -np 8" ./benchmark_sage.sh
```

### I/O Format Comparison

Compare performance of different output formats:

```bash
# Binary format (traditional)
./benchmark_sage.sh --format binary

# HDF5 format (modern, portable)
./benchmark_sage.sh --format hdf5
```

## Performance Optimization Guidelines

### Interpreting Results

**Good performance indicators:**
- Consistent runtime across multiple runs (< 5% variation)
- Memory usage scales appropriately with problem size
- Output file sizes match expectations

**Performance concerns:**
- Significant runtime increase (> 20%) without code changes
- Memory usage growing faster than problem size
- Large variations between runs (> 10%)

### Common Performance Issues

1. **Memory Leaks**: Steadily increasing memory usage over time
2. **I/O Bottlenecks**: Disproportionate time spent in file operations
3. **Inefficient Algorithms**: Poor scaling with forest/halo count
4. **Build Configuration**: Using Debug builds for production work

### Optimization Strategy

1. **Profile First**: Use tools like `gprof`, `perf`, or `valgrind` to identify hotspots
2. **Measure Impact**: Always benchmark before and after changes
3. **Incremental Changes**: Test one optimization at a time
4. **Consider Trade-offs**: Balance memory vs speed, accuracy vs performance

## Continuous Integration

### Automated Benchmarking

For CI/CD integration, run benchmarks automatically:

```bash
# CI-friendly benchmark (non-interactive)
#!/bin/bash
cd tests/
CMAKE_BUILD_TYPE=Release ./benchmark_sage.sh --verbose > benchmark_results.log 2>&1

# Extract key metrics for CI reporting
grep "Wall Clock Time\|Maximum Memory Usage" benchmark_results.log
```

### Performance Regression Detection

Set up automated alerts for performance regressions:

```bash
# Example: Check if runtime increased by more than 20%
BASELINE_TIME=45.23
CURRENT_TIME=$(grep "wall_time_seconds" benchmarks/baseline_latest.json | cut -d: -f2 | tr -d ' ,')
THRESHOLD=54.28  # 20% increase

if (( $(echo "$CURRENT_TIME > $THRESHOLD" | bc -l) )); then
    echo "Performance regression detected!"
    echo "Current: ${CURRENT_TIME}s, Baseline: ${BASELINE_TIME}s"
    exit 1
fi
```

## Troubleshooting

### Common Issues

**"Could not change to SAGE root directory"**
- Ensure you're running from the `tests/` directory
- Check that the script can find the parent directory

**"CMake configuration failed"**
- Verify CMake is installed and accessible
- Check that all dependencies (GSL, HDF5) are available
- Try running CMake manually: `cd build && cmake ..`

**"Build failed"**
- Check for compilation errors in the verbose output
- Ensure all source files are present and readable
- Verify compiler toolchain is properly installed

**Memory measurement showing 0**
- Check that `time` command is available and working
- On macOS, ensure you're using `/usr/bin/time` not shell builtin
- Try running with `--verbose` for detailed timing output

**Test data download fails**
- Verify internet connectivity
- Check that `wget` or `curl` is available
- Try downloading test data manually from the provided URLs

### Debug Mode

Run with verbose output to diagnose issues:

```bash
./benchmark_sage.sh --verbose
```

This provides detailed information about:
- Test data availability and download status
- CMake configuration and build process
- Parameter file modifications
- Timing and memory measurement details
- File creation and verification

### Performance Validation

To validate benchmark accuracy:

1. **Run Multiple Times**: Results should be consistent (< 5% variation)
2. **Check System Load**: Ensure no other intensive processes running
3. **Verify Configuration**: Confirm build type and MPI settings
4. **Compare Platforms**: Results may vary significantly between systems

## Best Practices

### Regular Benchmarking

- Run benchmarks before and after significant changes
- Keep a log of benchmark results with corresponding git commits
- Test both serial and parallel performance when applicable
- Use Release builds for performance-critical measurements

### Result Management

- Archive benchmark results for long-term trend analysis
- Include system information when sharing results
- Note any special configuration or environment factors
- Use consistent hardware/software for comparable results

### Development Workflow

1. **Baseline**: Establish performance baseline before starting work
2. **Develop**: Make incremental changes with frequent testing
3. **Benchmark**: Run performance tests before committing
4. **Document**: Note any expected performance changes in commits
5. **Monitor**: Track long-term performance trends

## Related Tools

- **Test Suite**: Use `./build.sh tests` for correctness validation
- **Memory Profiling**: Tools like `valgrind` for detailed memory analysis
- **CPU Profiling**: Tools like `gprof` or `perf` for hotspot identification
- **Build System**: See `CLAUDE.md` for comprehensive build instructions

## Support

If you encounter issues with benchmarking:

1. Check this documentation and the script's `--help` output
2. Review the troubleshooting section above
3. Run with `--verbose` for detailed diagnostic information
4. Report persistent issues on the GitHub repository

Remember that performance characteristics can vary significantly between different hardware, operating systems, and build configurations. Always benchmark on the target platform for the most relevant results.