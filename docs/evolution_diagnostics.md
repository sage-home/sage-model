# Evolution Diagnostics System

This document provides an overview of the evolution diagnostics system in SAGE, explaining its purpose, how to interpret diagnostic reports, and how developers can extend the system.

## 1. System Purpose and Design

### 1.1 Overall Goals

The evolution diagnostics system provides comprehensive metrics and statistics on the galaxy evolution process in the SAGE model. Its key goals are to:

- **Enable Performance Analysis**: Identify computational bottlenecks and execution patterns
- **Support Debugging**: Track processing flow and capture key events during evolution
- **Facilitate Scientific Validation**: Compare galaxy property changes before and after evolution
- **Monitor Merger Activity**: Track merger detection and processing statistics

### 1.2 Architecture and Integration

The diagnostics system is integrated with the evolution pipeline but operates as a passive observer without affecting scientific results:

- **Core Components**:
  - `evolution_diagnostics` structure: Contains all metrics and counters
  - Lifecycle management functions: Initialize, finalize, and report functions
  - Integration hooks: Event capturing and phase timing functions

- **Integration Points**:
  - **Evolution Context**: Diagnostics are attached to the evolution context
  - **Pipeline Phases**: Phase entry/exit is instrumented to measure timings
  - **Event System**: Events are counted when dispatched
  - **Merger Processing**: Merger detection and processing are tracked

![Diagnostics Architecture](../images/evolution_diagnostics_arch.png)

### 1.3 Key Metrics Collected

The diagnostics system collects several categories of metrics:

- **Performance Metrics**:
  - Overall execution time per halo
  - Time spent in each pipeline phase
  - Galaxies processed per second
  - Pipeline steps and callbacks executed

- **Galaxy Statistics**:
  - Initial and final galaxy counts
  - Changes in total stellar mass, cold gas, hot gas, and bulge mass
  - Distribution of galaxy types

- **Event Statistics**:
  - Counts of each event type during evolution
  - Pattern of events through evolution phases

- **Merger Statistics**:
  - Number of mergers detected vs. processed
  - Distribution of major vs. minor mergers

## 2. Diagnostic Report Interpretation

The diagnostic report is organized into several sections. Here's how to interpret each one:

### 2.1 Basic Information

```
=== Evolution Diagnostics for Halo 34 ===
Galaxies: Initial=45, Final=42
Processing Time: 2.025 seconds (22.2 galaxies/second)
```

This section shows:
- **Halo Identifier**: The halo being processed
- **Galaxy Counts**: Initial and final galaxy counts (difference indicates mergers)
- **Processing Time**: Total execution time and processing rate

A low galaxies/second rate may indicate performance issues.

### 2.2 Phase Statistics

```
--- Phase Statistics ---
Phase HALO: 0.253 seconds (12.5%), 1 steps, 0 galaxies
Phase GALAXY: 1.542 seconds (76.2%), 20 steps, 342 galaxies
Phase POST: 0.189 seconds (9.3%), 20 steps, 0 galaxies
Phase FINAL: 0.041 seconds (2.0%), 1 steps, 0 galaxies
```

This section helps identify which phases consume the most processing time:
- **Time**: Total seconds spent in each phase
- **Percentage**: Fraction of total execution time
- **Steps**: Number of execution cycles for each phase
- **Galaxies**: Number of galaxies processed (only relevant for GALAXY phase)

Performance issues typically appear as unusually high percentages in specific phases.

### 2.3 Merger Statistics

```
--- Merger Statistics ---
Mergers: Detected=5, Processed=3 (Major=1, Minor=2)
```

This section shows:
- **Detected Mergers**: Potential mergers identified during evolution
- **Processed Mergers**: Mergers actually executed
- **Merger Types**: Breakdown of major vs. minor mergers

Discrepancies between detected and processed mergers may indicate merger timing issues.

### 2.4 Galaxy Property Changes

```
--- Galaxy Property Changes ---
Stellar Mass: Initial=3.452e+10, Final=3.687e+10, Change=2.350e+09
Cold Gas: Initial=2.134e+10, Final=1.875e+10, Change=-2.590e+09
Hot Gas: Initial=5.781e+11, Final=5.802e+11, Change=2.100e+09
Bulge Mass: Initial=1.234e+10, Final=1.458e+10, Change=2.240e+09
```

This section shows aggregate property changes across all galaxies:
- **Initial Value**: Sum across all galaxies at start of evolution
- **Final Value**: Sum across all galaxies after evolution
- **Change**: Net change during evolution

These values are useful for validating mass conservation and expected evolution patterns.

### 2.5 Event Statistics

```
--- Event Statistics ---
Event COOLING_COMPLETED: 342 occurrences
Event STAR_FORMATION_OCCURRED: 287 occurrences
Event FEEDBACK_APPLIED: 287 occurrences
Event DISK_INSTABILITY: 12 occurrences
Event MERGER_DETECTED: 5 occurrences
```

This section counts events that occurred during evolution:
- Each event type is listed with its occurrence count
- Only events that occurred at least once are shown

Unexpected event patterns may indicate logical issues in the evolution process.

## 3. Developer Extension Guide

### 3.1 Adding New Metrics

To add new metrics to the diagnostics system:

1. **Update Structure**: Add new fields to `evolution_diagnostics` in `core_evolution_diagnostics.h`
2. **Initialize Values**: Add initialization in `evolution_diagnostics_initialize()`
3. **Update Collection**: Add code to update metrics at appropriate points
4. **Add Reporting**: Update `evolution_diagnostics_report()` to include new metrics

Example for adding a new metric for average galaxy mass:

```c
// 1. Add to structure in header
struct evolution_diagnostics {
    // ...existing fields...
    double average_galaxy_mass_initial;
    double average_galaxy_mass_final;
};

// 2. Initialize in initialization function
diag->average_galaxy_mass_initial = 0.0;
diag->average_galaxy_mass_final = 0.0;

// 3. Update in property recording functions
if (ngal > 0) {
    diag->average_galaxy_mass_initial = diag->total_stellar_mass_initial / ngal;
}

// 4. Add to report function
LOG_INFO("Average Galaxy Mass: Initial=%.3e, Final=%.3e, Change=%.3e",
         diag->average_galaxy_mass_initial, 
         diag->average_galaxy_mass_final,
         diag->average_galaxy_mass_final - diag->average_galaxy_mass_initial);
```

### 3.2 Custom Event Tracking

To track custom events in the diagnostics system:

1. Define a custom event type in `core_event_system.h`
2. Trigger the event using `event_emit()` when appropriate
3. Event counts will be automatically tracked in diagnostics

Example:

```c
// 1. Define custom event
#define EVENT_CUSTOM_LOW_MASS_GALAXY (EVENT_CUSTOM_BEGIN + 1)

// 2. Emit the event when detected
if (galaxy->StellarMass < 1.0e7) {
    event_emit(EVENT_CUSTOM_LOW_MASS_GALAXY, MODULE_ID, galaxy_idx, step, NULL, 0, 0);
}
```

### 3.3 Best Practices for Performance-Sensitive Metrics

When adding metrics to the diagnostics system, follow these best practices:

1. **Minimize Overhead**:
   - Avoid expensive calculations in tight loops
   - Consider sampling instead of measuring every operation
   - Use compile-time flags for very expensive metrics

2. **Memory Efficiency**:
   - Prefer counters over storing full data series
   - Use statistical aggregation instead of raw data storage
   - Avoid allocating memory during the evolution process

3. **Thread Safety** (for future parallel implementations):
   - Use atomic operations for counters
   - Avoid shared state between diagnostics instances

4. **Validation**:
   - Add unit tests for new metrics
   - Verify metrics don't affect scientific results

By following these guidelines, you can extend the diagnostics system while maintaining its low-overhead design principles.