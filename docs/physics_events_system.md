# Physics Events System

## Overview

The physics events system provides a structured mechanism for physics modules to communicate with each other and track physics-specific processes in SAGE. This system is completely separate from the core infrastructure events to maintain core-physics separation principles.

## Purpose and Design Goals

### Primary Objectives
1. **Inter-Module Communication**: Enable physics modules to notify each other of significant physics events
2. **Diagnostics Support**: Provide detailed tracking of physics processes for debugging and analysis
3. **Core-Physics Separation**: Maintain complete independence from core infrastructure events
4. **Extensibility**: Allow physics modules to define custom events for specialized interactions

### Architectural Principles
- **Separation of Concerns**: Physics events are isolated from core infrastructure events
- **Module Independence**: Physics modules communicate without direct dependencies
- **Runtime Flexibility**: Event handling adapts to available physics modules
- **Performance Efficiency**: Minimal overhead for event emission and handling

## Implementation Architecture

### File Structure
```
src/physics/physics_events.h    # Event type definitions and data structures
src/physics/physics_events.c    # Event utility functions and name lookup
```

### Core Components

#### Event Type Definitions
The system defines standardized physics event types in the reserved range 100-999:

**Major Physics Process Events**:
- `PHYSICS_EVENT_COOLING_COMPLETED`: Hot gas cooling processes completed
- `PHYSICS_EVENT_STAR_FORMATION_OCCURRED`: Star formation calculations completed
- `PHYSICS_EVENT_FEEDBACK_APPLIED`: Stellar feedback processes applied
- `PHYSICS_EVENT_AGN_ACTIVITY`: AGN feedback and black hole processes
- `PHYSICS_EVENT_DISK_INSTABILITY`: Disk instability calculations
- `PHYSICS_EVENT_MERGER_DETECTED`: Galaxy merger detection and processing
- `PHYSICS_EVENT_REINCORPORATION_COMPUTED`: Gas reincorporation calculations
- `PHYSICS_EVENT_INFALL_COMPUTED`: Fresh gas infall calculations

**Property Update Events**:
- `PHYSICS_EVENT_COLD_GAS_UPDATED`: Cold gas mass changes
- `PHYSICS_EVENT_HOT_GAS_UPDATED`: Hot gas mass changes  
- `PHYSICS_EVENT_STELLAR_MASS_UPDATED`: Stellar mass changes
- `PHYSICS_EVENT_METALS_UPDATED`: Metallicity changes
- `PHYSICS_EVENT_BLACK_HOLE_MASS_UPDATED`: Black hole mass changes

#### Event Data Structures
Each event type has an associated data structure containing relevant physics information:

```c
// Example: Cooling process completion
typedef struct {
    float cooling_rate;        /* Rate of cooling (10^10 Msun/h per timestep) */
    float cooling_radius;      /* Radius within which cooling occurs (Mpc/h) */
    float hot_gas_cooled;      /* Amount of gas cooled onto disk (10^10 Msun/h) */
} physics_event_cooling_completed_data_t;

// Example: Star formation occurrence  
typedef struct {
    float stars_formed;        /* Total stellar mass formed (10^10 Msun/h) */
    float stars_to_disk;       /* Stars added to disk component (10^10 Msun/h) */
    float stars_to_bulge;      /* Stars added to bulge component (10^10 Msun/h) */
    float metallicity;         /* Metal fraction of new stars (dimensionless) */
} physics_event_star_formation_occurred_data_t;
```

### Integration with Core Event System

The physics events system integrates with SAGE's core event infrastructure while maintaining strict separation:

```
Core Event System                Physics Event System
├── Infrastructure Events        ├── Physics Process Events
├── Pipeline Management          ├── Module Communication  
├── Module Lifecycle             ├── Property Updates
└── Performance Tracking         └── Scientific Calculations

        ↓                               ↓
    Handled by Core              Handled by Physics Modules
```

## Usage Patterns

### For Physics Module Developers

#### Emitting Physics Events
```c
#include "physics/physics_events.h"

// In a cooling module
void cooling_process_completed(struct GALAXY *galaxy, int galaxy_index, int step) {
    // Prepare event data
    physics_event_cooling_completed_data_t event_data = {
        .cooling_rate = calculated_cooling_rate,
        .cooling_radius = galaxy_cooling_radius,
        .hot_gas_cooled = amount_cooled
    };
    
    // Emit the physics event
    event_emit(PHYSICS_EVENT_COOLING_COMPLETED, 
               cooling_module_id, galaxy_index, step,
               &event_data, sizeof(event_data), 
               EVENT_FLAG_LOGGING);
}
```

#### Handling Physics Events
```c
// Register handler for star formation events in feedback module
static bool handle_star_formation_event(const struct event *event, void *user_data) {
    if (event->type == PHYSICS_EVENT_STAR_FORMATION_OCCURRED) {
        const physics_event_star_formation_occurred_data_t *data = 
            EVENT_DATA(event, physics_event_star_formation_occurred_data_t);
            
        // React to star formation (e.g., calculate feedback)
        calculate_stellar_feedback(event->galaxy_index, data->stars_formed);
        return true;
    }
    return false;
}

// During module initialization
event_register_handler(PHYSICS_EVENT_STAR_FORMATION_OCCURRED,
                      handle_star_formation_event,
                      module_data, feedback_module_id,
                      "feedback_star_formation_handler",
                      EVENT_PRIORITY_NORMAL);
```

### Event Naming and Debugging

The system provides utilities for debugging and logging:

```c
// Get human-readable event name
const char *event_name = physics_event_type_name(PHYSICS_EVENT_COOLING_COMPLETED);
// Returns: "COOLING_COMPLETED"

// Use in logging
LOG_DEBUG("Processing physics event: %s for galaxy %d", 
          event_name, galaxy_index);
```

## Development Guidelines

### Adding New Physics Events

When physics modules need new event types:

1. **Define Event Type**: Add new enum value to `physics_event_type_t` in the physics range (100-999)
2. **Create Data Structure**: Define associated data structure with relevant physics information
3. **Add Event Name**: Update `physics_event_type_names` array in `physics_events.c`
4. **Document Usage**: Add documentation explaining when and how the event should be used

Example:
```c
// In physics_events.h
typedef enum physics_event_type {
    // ... existing events ...
    PHYSICS_EVENT_CUSTOM_PROCESS = 150,
} physics_event_type_t;

typedef struct {
    float process_rate;
    float mass_affected;
} physics_event_custom_process_data_t;
```

### Best Practices

#### Event Design
- **Specific Purpose**: Each event should represent a well-defined physics process
- **Complete Information**: Include all relevant data that other modules might need
- **Consistent Units**: Use SAGE's standard units (10^10 Msun/h for mass, Mpc/h for length)
- **Documentation**: Clearly document when and why the event is emitted

#### Performance Considerations
- **Selective Emission**: Only emit events when there are registered handlers
- **Minimal Data**: Include only essential information in event data structures
- **Batch Processing**: Consider batching events for performance-critical modules

#### Module Communication
- **Loose Coupling**: Use events for notifications, not direct control flow
- **Error Handling**: Handle missing or failed event handlers gracefully
- **Priority Management**: Use appropriate event priorities for time-sensitive physics

## Current Status and Future Development

### Implementation Status
- ✅ **Event Type Definitions**: Complete set of physics events defined
- ✅ **Utility Functions**: Event naming and lookup functions implemented
- ✅ **Core Separation**: Complete separation from core infrastructure events
- 🔄 **Physics Module Integration**: Ready for physics module implementation in Phase 5.2.G

### Future Enhancements
As physics modules are developed, the event system may be enhanced with:
- **Event Filtering**: Module-specific event filtering for performance
- **Event Aggregation**: Batch processing of similar events
- **Event History**: Tracking event sequences for debugging
- **Custom Event Types**: Module-specific event type registration

The physics events system provides the foundation for sophisticated inter-module communication while maintaining the core-physics separation that is essential to SAGE's modular architecture.