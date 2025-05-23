#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file physics_events.h
 * @brief Physics-specific event types and data structures
 * 
 * This file defines event types used by physics modules.
 * These events are separate from core infrastructure events to maintain
 * core-physics separation.
 * 
 * PURPOSE:
 * --------
 * This file was created during the evolution diagnostics refactoring to ensure
 * clean separation between core infrastructure events and physics-specific events.
 * Physics modules can use these events for inter-module communication while
 * maintaining independence from core infrastructure.
 * 
 * USAGE:
 * ------
 * Physics modules should use these event types when emitting events related to
 * physics processes. The event system will dispatch these events to registered
 * physics module handlers, enabling physics modules to respond to each other's
 * activities without direct coupling.
 * 
 * FUTURE DEVELOPMENT:
 * -------------------
 * As physics modules are implemented in Phase 5.2.G, they will register handlers
 * for relevant physics events and emit these events when physics processes occur.
 * This enables sophisticated physics module interactions while maintaining the
 * core-physics separation architecture.
 */

/**
 * Physics-specific event types
 * 
 * Physics modules can define their own events in the reserved range
 * EVENT_PHYSICS_BEGIN (100) to EVENT_PHYSICS_END (999).
 */
typedef enum physics_event_type {
    /* Major physics process events */
    PHYSICS_EVENT_COOLING_COMPLETED = 100,
    PHYSICS_EVENT_STAR_FORMATION_OCCURRED = 101,
    PHYSICS_EVENT_FEEDBACK_APPLIED = 102,
    PHYSICS_EVENT_AGN_ACTIVITY = 103,
    PHYSICS_EVENT_DISK_INSTABILITY = 104,
    PHYSICS_EVENT_MERGER_DETECTED = 105,
    PHYSICS_EVENT_REINCORPORATION_COMPUTED = 106,
    PHYSICS_EVENT_INFALL_COMPUTED = 107,
    
    /* Property update events */
    PHYSICS_EVENT_COLD_GAS_UPDATED = 120,
    PHYSICS_EVENT_HOT_GAS_UPDATED = 121,
    PHYSICS_EVENT_STELLAR_MASS_UPDATED = 122,
    PHYSICS_EVENT_METALS_UPDATED = 123,
    PHYSICS_EVENT_BLACK_HOLE_MASS_UPDATED = 124,
    
    PHYSICS_EVENT_TYPE_MAX = 200
} physics_event_type_t;

/**
 * Physics event data structures
 * 
 * These structures define the data payload for different physics event types.
 * All mass values are in the simulation's mass units (typically 10^10 Msun/h).
 * All radii are in the simulation's length units (typically Mpc/h).
 * All rates are per timestep unless otherwise specified.
 */

/* Physics process events */
typedef struct {
    float cooling_rate;        /* Rate at which hot gas is cooling in 10^10 Msun/h per timestep */
    float cooling_radius;      /* Radius within which gas can cool in Mpc/h */
    float hot_gas_cooled;      /* Total amount of hot gas that cooled onto the disk in 10^10 Msun/h */
} physics_event_cooling_completed_data_t;

typedef struct {
    float stars_formed;        /* Total stellar mass formed in this timestep in 10^10 Msun/h */
    float stars_to_disk;       /* Stellar mass added to the disk component in 10^10 Msun/h */
    float stars_to_bulge;      /* Stellar mass added to the bulge component in 10^10 Msun/h */
    float metallicity;         /* Metal fraction of the newly formed stars (dimensionless) */
} physics_event_star_formation_occurred_data_t;

typedef struct {
    float energy_injected;     /* Energy from supernovae in standard units */
    float mass_reheated;       /* Cold gas reheated to hot phase by SN feedback in 10^10 Msun/h */
    float metals_ejected;      /* Metal mass ejected by SN feedback in 10^10 Msun/h */
} physics_event_feedback_applied_data_t;

typedef struct {
    float energy_released;     /* Energy released by AGN feedback in standard units */
    float mass_accreted;       /* Mass accreted onto the black hole in 10^10 Msun/h */
    float mass_ejected;        /* Hot gas mass ejected from halo due to AGN feedback in 10^10 Msun/h */
} physics_event_agn_activity_data_t;

/* Property update events */
typedef struct {
    float old_value;           /* Previous value of the property before update */
    float new_value;           /* New value of the property after update */
    float delta;               /* Change in the property value (new - old) */
} physics_event_property_updated_data_t;

/**
 * Get the name of a physics event type
 * 
 * Returns a string description of a physics event type.
 * 
 * @param type Physics event type to describe
 * @return String description of the physics event type
 */
const char *physics_event_type_name(physics_event_type_t type);

#ifdef __cplusplus
}
#endif
