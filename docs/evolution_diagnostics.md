# Evolution Diagnostics System

This document provides an overview of the evolution diagnostics system in SAGE, explaining its purpose, how to interpret diagnostic reports, and the implemented core-physics separation architecture.

## 1. System Purpose and Design

### 1.1 Overall Goals

The evolution diagnostics system provides comprehensive metrics and statistics on the galaxy evolution process in the SAGE model. Its key goals are to:

- **Enable Performance Analysis**: Identify computational bottlenecks and execution patterns
- **Support Debugging**: Track processing flow and capture key events during evolution
- **Monitor Pipeline Execution**: Track phase timing, step counts, and module callbacks
- **Facilitate System Monitoring**: Monitor core infrastructure metrics and performance

### 1.2 Architecture and Integration

The diagnostics system follows strict core-physics separation principles, operating as a passive observer of infrastructure metrics without physics-specific knowledge:

- **Core Components**:
  - `core_evolution_diagnostics` structure: Contains **only** core infrastructure metrics and counters
  - Lifecycle management functions: Initialize, finalize, and report functions
  - Integration hooks: Phase timing and pipeline statistics

- **Integration Points**:
  - **Evolution Context**: Diagnostics are attached to the evolution context
  - **Pipeline Phases**: Phase entry/exit is instrumented to measure timings
  - **Core Event System**: **Only core infrastructure events** are counted when dispatched
  - **Module Execution**: Module callback and pipeline step tracking

### 1.3 Core-Physics Separation Compliance

**Important**: As of May 2025, the diagnostics system has been completely redesigned to achieve core-physics separation compliance:

- ✅ **No Physics Properties**: Core diagnostics contains no hardcoded physics property knowledge
- ✅ **Infrastructure Events Only**: Core event tracking limited to pipeline, phase, and module events
- ✅ **Separated Event Systems**: Physics events are defined separately in `src/physics/physics_events.h`
- ✅ **Generic Framework**: Physics modules can register their own diagnostic metrics independently

### 1.3 Current Core Metrics Collected

The diagnostics system currently collects infrastructure-focused metrics:

- **Performance Metrics**:
  - Overall execution time per halo
  - Time spent in each pipeline phase
  - Galaxies processed per second
  - Pipeline steps and callbacks executed

- **Infrastructure Statistics**:
  - Initial and final galaxy counts
  - Core pipeline execution metrics
  - Module callback execution counts

- **Core Event Statistics**:
  - Counts of core infrastructure events
  - Pattern of core events through evolution phases

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
- **Galaxy Counts**: Initial and final galaxy counts
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

### 2.3 Core Event Statistics

```
--- Core Event Statistics ---
Core Event PIPELINE_STARTED: 1 occurrences
Core Event PHASE_STARTED: 4 occurrences
Core Event GALAXY_CREATED: 12 occurrences
```

This section counts **core infrastructure events only** that occurred during evolution:
- Each core event type is listed with its occurrence count
- Only events that occurred at least once are shown
- **Physics events are tracked separately** by physics modules (see `src/physics/physics_events.h`)

Core infrastructure events include:
- `PIPELINE_STARTED/COMPLETED`: Pipeline execution lifecycle
- `PHASE_STARTED/COMPLETED`: Pipeline phase transitions
- `GALAXY_CREATED/COPIED/MERGED`: Galaxy lifecycle events
- `MODULE_ACTIVATED/DEACTIVATED`: Module system events

Unexpected core event patterns may indicate pipeline execution issues.

## 3. Current Implementation Status ✅ COMPLETED

### 3.1 Core-Physics Separation Achieved

**Status: COMPLETED** (May 2025)

The evolution diagnostics system has been successfully redesigned to achieve complete core-physics separation:

1. ✅ **Core Diagnostics Only**: Core diagnostics tracks only infrastructure and performance metrics
2. ✅ **Physics Module Independence**: Physics modules register their own diagnostic metrics independently  
3. ✅ **Generic Aggregation Framework**: Infrastructure supports module-registered metrics without hardcoded physics knowledge
4. ✅ **Separated Event Systems**: Core infrastructure events and physics-specific events are completely separated

### 3.2 Implementation Details

**Core Diagnostics (`src/core/core_evolution_diagnostics.*`)**:
- Tracks only infrastructure metrics: galaxy counts, phase timing, pipeline performance
- Uses `core_event_type_t` enum with infrastructure events only
- Function names use `core_evolution_diagnostics_*` prefix for clarity
- Zero physics property knowledge or hardcoded physics logic

**Physics Events (`src/physics/physics_events.*`)**:
- Dedicated files for physics-specific event definitions
- Includes events like `PHYSICS_EVENT_COOLING_COMPLETED`, `PHYSICS_EVENT_STAR_FORMATION_OCCURRED`
- Provides utilities for physics modules to emit and handle physics events
- Completely independent from core infrastructure events

**Generic Framework**:
- Physics modules can register custom diagnostic metrics
- Property-based aggregation without hardcoded property names
- Runtime adaptability to different physics module combinations

### 3.3 Testing and Validation

The redesigned system has been thoroughly tested:
- **Unit Tests**: `tests/test_evolution_diagnostics.c` validates core infrastructure functionality only
- **Integration Tests**: Core-physics separation compliance verified
- **Performance Tests**: No performance regression from architectural improvements
- **Scientific Validation**: All diagnostic information preserved through proper separation

## 4. Developer Guidelines

### 4.1 Using the Core Diagnostics System

The redesigned system provides clear guidelines for developers:

1. **Core Infrastructure Only**: Core diagnostics tracks only infrastructure and performance metrics
2. **Physics Module Responsibility**: Physics modules register their own diagnostic metrics independently
3. **Generic Framework**: Use property-based aggregation without hardcoded property names
4. **Event Separation**: Use core events for infrastructure, physics events for physics processes

### 4.2 Best Practices for Core Infrastructure Metrics

When working with core infrastructure metrics:

1. **Performance Focus**:
   - Track pipeline execution timing and phase performance
   - Monitor memory usage patterns and allocation efficiency
   - Measure module callback overhead and execution patterns

2. **Infrastructure Only**:
   - Track galaxy counts and structural changes
   - Monitor module activation, deactivation, and lifecycle
   - Record pipeline phase transitions and execution flow

3. **Generic Property Access**:
   - Use property system functions for any galaxy property access
   - Avoid hardcoded property names in core infrastructure
   - Support runtime adaptability to different physics module combinations

### 4.3 Physics Module Diagnostic Integration

Physics modules should implement their own diagnostic capabilities:

1. **Event Emission**: Use `src/physics/physics_events.h` definitions for physics-specific events
2. **Metric Registration**: Register custom diagnostic metrics with the module system
3. **Property Aggregation**: Use generic aggregation services for physics property summaries
4. **Inter-Module Communication**: Use physics events for diagnostic information sharing

### 4.4 Example Usage Patterns

**Core Diagnostics Usage**:
```c
// Initialize core diagnostics
struct core_evolution_diagnostics diag;
core_evolution_diagnostics_initialize(&diag, halo_nr, ngal);

// Track infrastructure events
core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PIPELINE_STARTED);

// Track phase timing
core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
// ... execute galaxy phase ...
core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);

// Generate report
core_evolution_diagnostics_report(&diag, LOG_LEVEL_DEBUG);
```

**Physics Event Usage** (for physics modules):
```c
#include "physics/physics_events.h"

// Emit physics event
physics_event_cooling_completed_data_t event_data = {
    .cooling_rate = calculated_rate,
    .hot_gas_cooled = gas_amount
};

event_emit(PHYSICS_EVENT_COOLING_COMPLETED, module_id, galaxy_index, step,
           &event_data, sizeof(event_data), EVENT_FLAG_LOGGING);
```

## 5. Architectural Success and Future Development

### 5.1 Successful Redesign Implementation

The evolution diagnostics system redesign demonstrates the effectiveness of the core-physics separation architecture:

1. ✅ **Complete Separation Achieved**: Core diagnostics contains zero physics-specific knowledge
2. ✅ **Functionality Preserved**: All original diagnostic capabilities maintained through proper separation
3. ✅ **Enhanced Modularity**: Physics modules can independently register and manage their own diagnostics
4. ✅ **Runtime Flexibility**: System adapts to any combination of physics modules without code changes

### 5.2 Pattern for Future Development

This successful redesign provides a template for addressing other core-physics separation issues:

1. **Systematic Analysis**: Identify hardcoded physics knowledge in core systems
2. **Clean Separation**: Extract physics-specific functionality to dedicated physics modules
3. **Generic Infrastructure**: Provide generic services that work with any physics module combination
4. **Comprehensive Testing**: Validate both infrastructure functionality and physics module integration

The diagnostics system now serves as an exemplar of proper core-physics separation in the SAGE architecture.