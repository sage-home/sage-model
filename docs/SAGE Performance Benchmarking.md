# SAGE Performance Benchmarking

This document describes the performance benchmarking process for the SAGE model before implementing the Properties Module architecture changes (Phase 5.2.B).

## Purpose

Before proceeding with significant architectural changes in Phase 5.2, we establish a performance baseline to:

1. Quantify current performance characteristics
2. Enable comparison with the new architecture after implementation
3. Identify any performance regressions or improvements
4. Provide data for the validation phase (5.3.3)

## Metrics Captured

The benchmark script captures key performance metrics:

### Overall Performance
- **Wall Clock Time**: Total execution time in seconds
- **Maximum Memory Usage**: Peak memory consumption in MB

### Phase Timing (Future)
The benchmark includes placeholders for phase-specific metrics that will be measurable once the full pipeline implementation is complete:
- **HALO Phase**
- **GALAXY Phase**
- **POST Phase**
- **FINAL Phase**

## Benchmark Process

### Test Data
The benchmark uses the Mini-Millennium test data, providing a standardized workload for performance measurement.

### Running the Benchmark

1. Ensure you're in the SAGE root directory
2. Execute the benchmark script:
   ```bash
   ./tests/benchmark_sage.sh
   ```
3. For detailed output, use:
   ```bash
   ./tests/benchmark_sage.sh --verbose
   ```

### Results Storage

Benchmark results are stored in JSON format in the `benchmarks/` directory. Each result file is named with a timestamp:
```
benchmarks/baseline_YYYYMMDD_HHMMSS.json
```

This format facilitates programmatic comparison using JSON diff tools.

## Interpreting Results

### Key Performance Indicators

When comparing benchmark results, focus on:

1. **Wall Clock Time**: Total execution time representing end-user experience
2. **Memory Usage**: Peak memory consumption 
3. **Phase Distribution**: (Available in future implementations) Shows time distribution across pipeline phases

### Expected Variations

Some natural variation in results may occur due to:
- System load fluctuations
- Filesystem caching effects
- Memory allocation patterns

Multiple runs may be needed to establish a reliable baseline.

## Example Result

```json
{
  "timestamp": "2025-05-01T02:59:52Z",
  "version": "ebacf00",
  "system": "Darwin HAW10012892 24.4.0 Darwin Kernel Version 24.4.0",
  "test_case": "Mini-Millennium",
  "overall_performance": {
    "real_time": "28",
    "max_memory_mb": 81.67
  },
  "phase_timing": {
    "halo_phase": {
      "time": "N/A",
      "percentage": "N/A"
    },
    "galaxy_phase": {
      "time": "N/A",
      "percentage": "N/A"
    },
    "post_phase": {
      "time": "N/A",
      "percentage": "N/A"
    },
    "final_phase": {
      "time": "N/A",
      "percentage": "N/A"
    }
  }
}
```