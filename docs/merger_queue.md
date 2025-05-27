# Merger Queue System

## Overview
The Merger Queue System manages galaxy merger events in SAGE using a deferred execution strategy. The queue collects merger events during galaxy evolution and processes them separately to maintain scientific consistency.

**Important**: As of SAGE v2.0, merger event *processing* has been moved to a physics-agnostic system. This document covers the core queue data structure. For complete merger handling documentation, see [Physics-Agnostic Merger Handling](physics_agnostic_merger_handling.md).

## Purpose
The merger queue ensures all galaxies see identical pre-merger states during physics calculations, preserving scientific accuracy regardless of processing order.

## Core Queue API

### Data Structures

#### Merger Event
```c
struct merger_event {
    int satellite_index;     /* Index of the satellite galaxy */
    int central_index;       /* Index of the central galaxy */
    double merger_time;      /* Remaining merger time */
    double time;             /* Current simulation time */
    double dt;               /* Timestep size */
    int halo_nr;             /* Halo number */
    int step;                /* Current timestep */
    int merger_type;         /* Type of merger (merger/disruption) */
};
```

#### Merger Event Queue
```c
struct merger_event_queue {
    struct merger_event events[MAX_GALAXIES_PER_HALO];  /* Fixed-size array */
    int num_events;                                     /* Number of events */
};
```

**Capacity**: Fixed at `MAX_GALAXIES_PER_HALO` (1000 events).

### Functions

#### `init_merger_queue()`
```c
void init_merger_queue(struct merger_event_queue *queue);
```
Initializes empty queue. Call at start of each timestep.

#### `queue_merger_event()`
```c
int queue_merger_event(
    struct merger_event_queue *queue,
    int satellite_index, int central_index,
    double merger_time, double time, double dt,
    int halo_nr, int step, int merger_type
);
```
**Returns**: 0 on success, -1 if queue full.

**Note**: Does not process events - only stores them for later processing.

## Integration with Physics-Agnostic System

The core queue works with the new merger processing system:

1. **GALAXY Phase**: Physics modules call `queue_merger_event()` 
2. **Processing**: `core_process_merger_queue_agnostically()` handles events
3. **Physics**: Configured modules implement actual merger physics

## Usage Example

```c
// Initialize at timestep start
struct merger_event_queue merger_queue;
init_merger_queue(&merger_queue);

// During galaxy evolution
if (merger_detected) {
    queue_merger_event(&merger_queue, sat_idx, cen_idx, 
                      merger_time, time, dt, halo_nr, step, type);
}

// Processing handled automatically by core_merger_processor
```

## Error Handling

- **Queue Overflow**: Returns -1 when `num_events >= MAX_GALAXIES_PER_HALO`
- **Null Pointers**: Logs error and returns -1
- **Index Validation**: Performed by processor, not queue itself

## Memory Management

- **Fixed Allocation**: No dynamic memory allocation
- **Reset Behavior**: Only counter reset, array memory reused
- **Thread Safety**: Requires external synchronization in parallel contexts

## See Also
- [Physics-Agnostic Merger Handling](physics_agnostic_merger_handling.md) - Complete system documentation
- [Module System](module_system_and_configuration.md) - Module registration and configuration
- [Pipeline Phases](pipeline_phases.md) - Pipeline execution phases

---

*Last updated: January 2025*  
*Component version: 2.0 (Core Queue Only)*