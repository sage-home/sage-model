# Physics-Agnostic Merger Event Handling System

## Overview

SAGE implements a sophisticated merger event handling system that completely separates the **triggering and queuing** of merger events from the **physics implementation** of those mergers. This architecture enables runtime configuration of merger physics while maintaining scientific consistency and testability.

## System Architecture

The merger handling system consists of three key components:

### 1. Core Merger Queue (`src/core/core_merger_queue.c/h`)
- **Purpose**: Pure data structure for storing merger events
- **Responsibility**: Queue management (`init_merger_queue`, `queue_merger_event`)
- **Physics Knowledge**: None - completely agnostic

### 2. Core Merger Processor (`src/core/core_merger_processor.c/h`)
- **Purpose**: Dispatch queued events to configured physics handlers
- **Responsibility**: Event iteration and `module_invoke` calls
- **Physics Knowledge**: None - uses configuration to determine handlers

### 3. Physics Merger Modules (e.g., `src/physics/placeholder_mergers_module.c`)
- **Purpose**: Implement actual merger and disruption physics
- **Responsibility**: Register handler functions via `module_register_function`
- **Physics Knowledge**: Complete - contains all merger science

## How It Works

### Event Flow

```
1. Galaxy Evolution (GALAXY Phase)
   ├── Physics modules detect merger conditions
   ├── Call queue_merger_event() to defer processing
   └── Continue with other galaxies
   
2. Merger Processing (After GALAXY Phase)
   ├── core_process_merger_queue_agnostically() called
   ├── Reads configuration to determine handlers
   ├── Uses module_invoke() to call physics functions
   └── Clears queue when complete
   
3. POST Phase
   └── Other pipeline modules execute normally
```

### Configuration System

Merger handlers are configured via runtime parameters:

```c
// In struct runtime_params
char MergerHandlerModuleName[MAX_STRING_LEN];      // "PlaceholderMergersModule"
char MergerHandlerFunctionName[MAX_STRING_LEN];    // "HandleMerger" 
char DisruptionHandlerModuleName[MAX_STRING_LEN];  // "PlaceholderMergersModule"
char DisruptionHandlerFunctionName[MAX_STRING_LEN]; // "HandleDisruption"
```

**Default Configuration**: Points to `PlaceholderMergersModule` functions

### Runtime Dispatch

When processing events, the core merger processor:

1. **Examines each event**: Checks `merger_time` to determine type
2. **Selects handler**: Based on configuration (merger vs disruption)  
3. **Invokes physics**: Calls `module_invoke(handler_module, handler_function, event_data)`
4. **Error handling**: Continues processing other events if one fails

## Benefits

### Scientific Consistency
- All galaxies see identical pre-merger state during physics calculations
- Deferred processing maintains order independence
- Results are deterministic regardless of processing sequence

### Modularity  
- Physics modules can be swapped without touching core code
- Multiple merger implementations can coexist
- Runtime selection of merger physics

### Testability
- Core queue logic testable independently of physics
- Physics modules testable independently of core
- Mock handlers can be injected for testing

### Configurability
- Users can select merger implementations via parameter files
- Different physics for different simulation types
- No recompilation needed to change merger behavior

## Implementation Guide

### Creating Custom Merger Physics Modules

1. **Define Module Structure**:
```c
// In your_merger_module.c
struct your_merger_data {
    double merger_efficiency;
    // ... other parameters
};
```

2. **Implement Handler Functions**:
```c
static int handle_merger_physics(void *module_data, void *args_ptr, struct pipeline_context *ctx) {
    merger_handler_args_t *args = (merger_handler_args_t *)args_ptr;
    struct merger_event *event = &args->event;
    struct GALAXY *galaxies = args->pipeline_ctx->galaxies;
    
    // Implement your merger physics here
    // Access galaxies[event->satellite_index] and galaxies[event->central_index]
    
    return 0; // Success
}
```

3. **Register Functions in Module Init**:
```c
static int your_merger_init(struct params *params, void **data_ptr) {
    // ... allocation and setup
    
    module_register_function(
        your_merger_module.module_id,
        "YourMergerHandler",
        (void *)handle_merger_physics,
        FUNCTION_TYPE_INT,
        "int (void*, merger_handler_args_t*, struct pipeline_context*)",
        "Custom merger physics implementation"
    );
    
    return MODULE_STATUS_SUCCESS;
}
```

4. **Configure Runtime Parameters**:
```
MergerHandlerModuleName = YourMergerModule
MergerHandlerFunctionName = YourMergerHandler
```

### Handler Function Signature

All merger handler functions must follow this signature:

```c
int handler_function(void *module_data, void *args_ptr, struct pipeline_context *invoker_ctx);
```

**Parameters**:
- `module_data`: Module-specific data from initialization
- `args_ptr`: Pointer to `merger_handler_args_t` containing event details
- `invoker_ctx`: Pipeline context (usually not needed - use context from args)

**Return Value**: 0 for success, non-zero for error

### Accessing Event Data

```c
merger_handler_args_t *args = (merger_handler_args_t *)args_ptr;
struct merger_event *event = &args->event;
struct pipeline_context *ctx = args->pipeline_ctx;

// Event properties
int satellite_idx = event->satellite_index;
int central_idx = event->central_index;
double merger_time = event->merger_time;
int merger_type = event->merger_type;

// Galaxy access
struct GALAXY *galaxies = ctx->galaxies;
struct GALAXY *satellite = &galaxies[satellite_idx];
struct GALAXY *central = &galaxies[central_idx];

// Parameters
const struct params *params = ctx->params;
```

## Configuration Examples

### Using Different Modules for Mergers vs Disruptions

```
MergerHandlerModuleName = AdvancedMergerPhysics
MergerHandlerFunctionName = ProcessMajorMerger
DisruptionHandlerModuleName = TidalDisruptionModule  
DisruptionHandlerFunctionName = ProcessTidalStripping
```

### Parameter File Integration

```ini
# Standard SAGE parameter file
MergerHandlerModuleName = StandardMergerPhysics
MergerHandlerFunctionName = HandleGalaxyMerger
DisruptionHandlerModuleName = StandardMergerPhysics
DisruptionHandlerFunctionName = HandleSatelliteDisruption
```

## Error Handling

The system provides robust error handling:

- **Invalid Configuration**: Logs error if handler module/function not found
- **Handler Errors**: Logs error but continues processing other events  
- **Queue Overflow**: Logs error and rejects additional events
- **Invalid Indices**: Validates galaxy indices before calling handlers

## Testing Strategy

### Unit Testing Core Components
- `test_merger_queue.c`: Tests queue data structure operations
- `test_core_merger_processor.c`: Tests event dispatching logic
- Mock modules provide test handlers for validation

### Integration Testing Physics Modules  
- `test_placeholder_mergers_module.c`: Tests physics module functionality
- Direct function calls with prepared event data
- Galaxy state validation after merger processing

### System Testing
- End-to-end validation with real parameter files
- Scientific accuracy validation against baseline results
- Performance testing with various event loads

## Migration from Legacy System

### For Developers
- **Old**: `process_merger_events()` in core called physics directly
- **New**: `core_process_merger_queue_agnostically()` uses `module_invoke`
- **Migration**: Move physics logic from core to physics modules

### For Users  
- **Configuration**: Add merger handler parameters to parameter files
- **Defaults**: System works out-of-box with placeholder implementations
- **Customization**: Override parameters to use different physics modules

## Performance Considerations

- **Queue Processing**: Fixed-time iteration over events
- **Module Invoke Overhead**: Minimal function call overhead
- **Memory Usage**: Fixed-size queue (1000 events max)
- **Parallelization**: Queue operations are thread-safe with proper locking

## Future Extensions

- **Multiple Handler Types**: Support for different merger classifications
- **Handler Chains**: Sequential processing by multiple modules  
- **Dynamic Configuration**: Runtime switching of handlers
- **Event Prioritization**: Processing events by importance/type

---

*Last updated: January 2025*  
*System version: 2.0 (Physics-Agnostic)*
