# Merger Queue System

## Overview
The Merger Queue System is a critical component that manages galaxy merger events in SAGE, implementing a deferred execution strategy to maintain scientific consistency in the modular architecture. The system collects potential merger events during the galaxy evolution process and executes them only after all galaxies have completed their physics calculations for a given timestep.

## Purpose
In semi-analytic galaxy formation models like SAGE, mergers between galaxies represent one of the most important physical processes that shape galaxy evolution. To maintain scientific accuracy, the precise order and timing of these events is crucial.

The Merger Queue addresses two key requirements:
1. **Scientific Consistency**: All galaxies must see the same pre-merger state when undergoing physics calculations, ensuring that results don't depend on the order of galaxy processing.
2. **Architecture Compatibility**: The queue bridges between the modular pipeline architecture (where modules execute in sequence) and the scientific requirement for uniform galaxy states.

By deferring merger execution until after all galaxy physics are calculated, the system preserves the original SAGE scientific model while enabling the modular architecture.

## Key Concepts
- **Deferred Execution**: Mergers are not executed immediately when detected, but collected and processed later
- **Queue Collection**: During galaxy evolution, potential mergers are identified and added to the queue
- **Batch Processing**: After all galaxies complete their physics calculations, queued mergers are processed in a single batch
- **Event Types**: The queue handles multiple types of merger events (complete mergers, disruptions, etc.)
- **Timestep Preservation**: Each queued event maintains its original timestep context for accurate processing

## Data Structures

### Merger Event
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

### Merger Event Queue
```c
struct merger_event_queue {
    struct merger_event events[MAX_GALAXIES_PER_HALO];  /* Fixed-size array of merger events */
    int num_events;                                     /* Number of events in the queue */
};
```

**Note**: The queue is **fixed-size** with a capacity limit of `MAX_GALAXIES_PER_HALO` (1000 events). This is a hard limit enforced by bounds checking in the queue operations.

## API Reference

### Initialization

#### `init_merger_queue()`
```c
void init_merger_queue(struct merger_event_queue *queue);
```
**Purpose**: Initializes an empty merger event queue.  
**Parameters**:
- `queue`: Pointer to the queue structure to initialize

**Notes**: 
- Sets the event counter to zero
- No memory allocation is required as the queue uses a fixed-size array
- Should be called at the beginning of each galaxy evolution step

### Queue Management

#### `queue_merger_event()`
```c
int queue_merger_event(
    struct merger_event_queue *queue,
    int satellite_index,
    int central_index,
    double merger_time,
    double time,
    double dt,
    int halo_nr,
    int step,
    int merger_type
);
```
**Purpose**: Adds a merger event to the queue for later processing.  
**Parameters**:
- `queue`: Pointer to the merger event queue
- `satellite_index`: Index of the satellite galaxy in the galaxies array
- `central_index`: Index of the central/target galaxy in the galaxies array
- `merger_time`: Remaining time until merger is complete
- `time`: Current simulation time
- `dt`: Current timestep size
- `halo_nr`: Halo identifier
- `step`: Current integration step
- `merger_type`: Type of merger event (1=minor merger, 2=major merger, 3=disruption)

**Returns**: 
- `0` on success
- `-1` if the queue capacity is exceeded (when `num_events >= MAX_GALAXIES_PER_HALO`)

**Implementation Details**:
- Uses fixed-size array with bounds checking to prevent overflow
- No dynamic memory allocation - fails if the limit is exceeded
- Thread-safe when used in parallel execution contexts (using appropriate locks)

**Example**:
```c
// During galaxy evolution, when a potential merger is identified
if (should_merge(satellite, central, dt)) {
    int status = queue_merger_event(
        merger_queue,
        satellite_index,
        central_index,
        satellite->MergerTime,
        time,
        dt,
        halo_nr,
        step,
        merger_type
    );
    
    if (status != 0) {
        LOG_ERROR("Failed to queue merger event - queue full");
    }
}
```

### Event Processing

#### `process_merger_events()`
```c
int process_merger_events(
    struct merger_event_queue *queue,
    struct GALAXY *galaxies,
    struct params *run_params
);
```
**Purpose**: Processes all queued merger events after galaxy physics calculations.  
**Parameters**:
- `queue`: Pointer to the merger event queue
- `galaxies`: Array of galaxies to process
- `run_params`: Simulation parameters

**Returns**: 
- `0` on success
- Non-zero error code on failure

**Implementation Details**:
- Called after all galaxies have had their physics processes calculated
- Processes events in the order they were added to the queue
- Handles different merger types with specific processing functions
- Updates galaxy properties according to the merger physics
- Resets the queue counter (but keeps the allocated array memory)
- Thread-safe in parallel execution contexts

**Algorithm**:
1. Validate parameters and queue status
2. For each event in the queue:
   a. Extract event details
   b. Validate galaxy indices
   c. Based on merger_type, call appropriate merger function:
      - For disruptions (`merger_time > 0.0`): `disrupt_satellite_to_ICS()`
      - For mergers (`merger_time <= 0.0`): `deal_with_galaxy_merger()`
3. Reset the queue counter (`num_events = 0`)
4. Return success code

**Example**:
```c
// After all galaxies have been processed through the physics pipeline
int status = process_merger_events(merger_queue, galaxies, run_params);
if (status != 0) {
    LOG_ERROR("Error processing merger events: %d", status);
}
```

## Implementation Details

### Memory Management

The merger queue uses a fixed-size allocation strategy:
1. **Static Allocation**: The queue uses a static array `events[MAX_GALAXIES_PER_HALO]` (1000 elements)
2. **No Dynamic Expansion**: Unlike the previous documentation claims, the queue does **not** expand dynamically
3. **Bounds Checking**: The `queue_merger_event()` function checks `num_events >= MAX_GALAXIES_PER_HALO` and fails if exceeded
4. **Memory Persistence**: The queue array is embedded in the struct, so no explicit memory management is required
5. **Reset Behavior**: Only the counter (`num_events`) is reset between calls, not the array memory

### Scientific Significance

The merger queue is critical for scientific accuracy because:

1. **Order Independence**: Galaxies undergo identical physics calculations regardless of the order they're processed in
2. **Consistent State**: All galaxies see the same pre-merger environment, preventing cascade effects where early mergers affect later physics
3. **Reproducibility**: Results are deterministic regardless of parallelization or processing order
4. **Physics Isolation**: Physics modules can focus on their specific processes without worrying about merger state

### Integration with Pipeline System

The merger queue integrates with the phase-based pipeline system in a specific sequence:

1. **Initialization**: The queue is initialized at the beginning of each timestep:
   ```c
   // In evolve_galaxies()
   init_merger_queue(&merger_queue);
   ```

2. **Collection**: During the GALAXY phase, potential mergers are identified and queued:
   ```c
   // Inside a physics module executing in GALAXY phase
   if (galaxies[p].MergerTime <= 0.0) {
       queue_merger_event(&merger_queue, p, central_idx, ...);
   }
   ```

3. **Processing**: After the POST phase but before the FINAL phase, queued events are processed:
   ```c
   // After all physics calculations but before final output
   process_merger_events(&merger_queue, galaxies, run_params);
   ```

4. **Output**: The FINAL phase sees the post-merger state for output preparation

This sequence ensures that the scientific model remains consistent while leveraging the flexibility of the pipeline architecture.

## Usage Patterns

### Basic Queue Management
```c
// Initialize the queue at the beginning of timestep
struct merger_event_queue merger_queue;
init_merger_queue(&merger_queue);

// During galaxy evolution, queue merger events
for (int p = 0; p < ngal; p++) {
    // Process galaxy physics
    
    // Check for mergers and queue them
    if (is_merger_ready(&galaxies[p])) {
        queue_merger_event(&merger_queue, p, central_idx, ...);
    }
}

// After all galaxies processed, execute merger events
process_merger_events(&merger_queue, galaxies, run_params);
```

### Integration with Pipeline System
```c
// In core_build_model.c (evolve_galaxies function)

// Initialize for this timestep
init_merger_queue(&merger_queue);

// Execute HALO phase
pipeline_execute_phase(pipeline, &pipeline_ctx, PIPELINE_PHASE_HALO);

// Execute GALAXY phase (modules might queue mergers)
for (int p = 0; p < ngal; p++) {
    pipeline_ctx.current_galaxy = p;
    pipeline_execute_phase(pipeline, &pipeline_ctx, PIPELINE_PHASE_GALAXY);
}

// Execute POST phase
pipeline_execute_phase(pipeline, &pipeline_ctx, PIPELINE_PHASE_POST);

// Process queued merger events
process_merger_events(&merger_queue, galaxies, run_params);

// Execute FINAL phase (sees post-merger state)
pipeline_execute_phase(pipeline, &pipeline_ctx, PIPELINE_PHASE_FINAL);
```

### Parallel Execution Context
```c
// In parallel mode, ensure thread safety
#pragma omp critical
{
    queue_merger_event(&merger_queue, p, central_idx, ...);
}

// Process events after parallel region
#pragma omp barrier
#pragma omp single
{
    process_merger_events(&merger_queue, galaxies, run_params);
}
```

## Error Handling

The merger queue system handles several error conditions:

1. **Queue Overflow**:
   ```c
   if (queue->num_events >= MAX_GALAXIES_PER_HALO) {
       LOG_ERROR("Merger event queue overflow: num_events=%d, MAX_GALAXIES_PER_HALO=%d",
                queue->num_events, MAX_GALAXIES_PER_HALO);
       return -1;
   }
   ```

2. **Index Validation**:
   ```c
   if (satellite_index < 0 || satellite_index >= ngal ||
       central_index < 0 || central_index >= ngal) {
       LOG_ERROR("Invalid galaxy indices in merger event");
       return -2;
   }
   ```

3. **Null Pointer Protection**:
   ```c
   if (queue == NULL || galaxies == NULL || run_params == NULL) {
       LOG_ERROR("Null pointer passed to process_merger_events");
       return -1;
   }
   ```

4. **Merger Type Validation**:
   ```c
   switch (event->merger_type) {
       case MERGER_TYPE_MINOR:
       case MERGER_TYPE_MAJOR:
       case MERGER_TYPE_DISRUPTION:
           // Valid types, process accordingly
           break;
       default:
           LOG_WARNING("Unknown merger type: %d", event->merger_type);
           // Use default processing
           break;
   }
   ```

## Performance Considerations

1. **Fixed-Size Array**: The queue uses a fixed-size array to avoid memory allocation overhead during simulation

2. **Capacity Limit**: The `MAX_GALAXIES_PER_HALO` limit (1000) is set to handle typical simulation scenarios:
   ```c
   // For simulations with many small halos, this limit should be sufficient
   #define MAX_GALAXIES_PER_HALO 1000
   ```

3. **Memory Reuse**: The queue reuses the same array between timesteps by only resetting the counter

4. **Parallel Overhead**: In parallel execution, critical sections for queue access can become bottlenecks. Using thread-local queues that are later merged can improve scalability.

5. **Cache Efficiency**: The fixed array layout provides better cache locality compared to dynamic allocation

## Queue Capacity Management

**Important**: The merger queue has a **hard limit** of `MAX_GALAXIES_PER_HALO` (1000) events:

- If a halo has more than 1000 potential merger events in a single timestep, `queue_merger_event()` will fail
- This is typically not a problem in realistic simulations, but should be considered for extreme cases
- The limit can be increased by modifying `MAX_GALAXIES_PER_HALO` in `core_allvars.h` if needed
- No dynamic expansion occurs - the queue will simply reject additional events once full

## Integration Points
- **Dependencies**: The merger queue depends on the galaxy structure and event handling system
- **Used By**: Core evolution loop in `core_build_model.c`
- **Events**: Emits no events but responds to merger detection in physics modules
- **Properties**: Processes galaxies with specific properties (MergerTime, etc.)

## See Also
- [Pipeline Phases](pipeline_phases.md)
- [Evolution Diagnostics](evolution_diagnostics.md)
- [Physics Pipeline Executor](physics_pipeline_executor.md)

---

*Last updated: May 26, 2025*  
*Component version: 1.0*