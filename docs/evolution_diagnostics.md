# Evolution Diagnostics System

This document provides an overview of the evolution diagnostics system in SAGE, explaining its purpose, how to interpret diagnostic reports, and current architectural considerations.

## 1. System Purpose and Design

### 1.1 Overall Goals

The evolution diagnostics system provides comprehensive metrics and statistics on the galaxy evolution process in the SAGE model. Its key goals are to:

- **Enable Performance Analysis**: Identify computational bottlenecks and execution patterns
- **Support Debugging**: Track processing flow and capture key events during evolution
- **Monitor Pipeline Execution**: Track phase timing, step counts, and module callbacks
- **Facilitate System Monitoring**: Monitor core infrastructure metrics and performance

### 1.2 Architecture and Integration

The diagnostics system is integrated with the evolution pipeline but operates as a passive observer without affecting scientific results:

- **Core Components**:
  - `evolution_diagnostics` structure: Contains core infrastructure metrics and counters
  - Lifecycle management functions: Initialize, finalize, and report functions
  - Integration hooks: Phase timing and pipeline statistics

- **Integration Points**:
  - **Evolution Context**: Diagnostics are attached to the evolution context
  - **Pipeline Phases**: Phase entry/exit is instrumented to measure timings
  - **Core Event System**: Core infrastructure events are counted when dispatched
  - **Module Execution**: Module callback and pipeline step tracking

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
Event PIPELINE_STEP_STARTED: 44 occurrences
Event PIPELINE_STEP_COMPLETED: 44 occurrences
Event PHASE_TRANSITION: 4 occurrences
```

This section counts core infrastructure events that occurred during evolution:
- Each core event type is listed with its occurrence count
- Only events that occurred at least once are shown

Unexpected core event patterns may indicate pipeline execution issues.

## 3. Architectural Considerations

### 3.1 Core-Physics Separation Compliance

**IMPORTANT**: The current diagnostics system has architectural issues that violate core-physics separation principles:

- **Physics Property Knowledge**: The diagnostics structure contains hardcoded physics property fields
- **Direct Property Access**: Some functions directly access physics properties by name
- **Mixed Concerns**: Physics-specific metrics are mixed with core infrastructure metrics

### 3.2 Recommended Architectural Changes

The diagnostics system should be redesigned to maintain core-physics separation:

1. **Core Diagnostics Only**: Keep only infrastructure and performance metrics in core diagnostics
2. **Physics Module Responsibility**: Physics modules should register their own diagnostic metrics
3. **Generic Aggregation Framework**: Provide generic services for property aggregation without hardcoded physics knowledge
4. **Separate Event Systems**: Distinguish between core infrastructure events and physics-specific events

### 3.3 Future Development Guidelines

When working with the diagnostics system:

1. **Avoid Physics Properties**: Do not add physics-specific properties to core diagnostic structures
2. **Use Generic Accessors**: When accessing galaxy properties, use the generic property system
3. **Separate Concerns**: Keep core infrastructure metrics separate from physics-specific metrics
4. **Module Registration**: Design physics modules to register their own diagnostic needs

## 4. Developer Guidelines

### 4.1 Current Limitations

Due to the architectural issues identified:

1. **Limited Extension**: Adding new metrics should be done carefully to avoid further core-physics violations
2. **Property Access**: Any galaxy property access must use the generic property system
3. **Event Tracking**: Only core infrastructure events should be tracked by core diagnostics

### 4.2 Best Practices for Core Infrastructure Metrics

When adding core infrastructure metrics:

1. **Performance Focus**:
   - Track pipeline execution timing
   - Monitor memory usage patterns
   - Measure module callback overhead

2. **Infrastructure Only**:
   - Track galaxy counts and structural changes
   - Monitor module activation and deactivation
   - Record pipeline phase transitions

3. **Physics-Agnostic**:
   - Avoid referencing specific physics properties by name
   - Use generic interfaces for any galaxy data access
   - Design metrics that work with any physics module combination

### 4.3 Recommended Approach for Physics Metrics

For physics-specific diagnostics:

1. **Module Responsibility**: Physics modules should handle their own diagnostics
2. **Registration Pattern**: Provide registration interfaces for modules to declare metrics
3. **Aggregation Services**: Offer generic aggregation services without hardcoded property knowledge
4. **Separate Reporting**: Keep physics diagnostic reporting separate from core reporting

## 5. Future Architecture Direction

The diagnostics system requires redesign to align with SAGE's modular architecture goals:

1. **Phase 1**: Extract physics-specific components from core diagnostics
2. **Phase 2**: Implement generic aggregation framework for physics modules
3. **Phase 3**: Separate core and physics event systems
4. **Phase 4**: Enable physics modules to register diagnostic metrics

This redesign will ensure the diagnostics system supports the core-physics separation principles while maintaining comprehensive monitoring capabilities.