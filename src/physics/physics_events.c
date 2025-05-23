#include "physics_events.h"

/**
 * @file physics_events.c
 * @brief Implementation of physics event utility functions
 * 
 * PURPOSE:
 * --------
 * This file provides utility functions for physics events, primarily event type
 * name lookup for debugging and logging purposes. Created during the evolution
 * diagnostics refactoring to support clean separation between core infrastructure
 * events and physics-specific events.
 * 
 * CURRENT STATUS:
 * ---------------
 * This file currently provides only event name lookup functionality. As physics
 * modules are implemented in Phase 5.2.G, additional utility functions may be
 * added to support physics event handling, validation, and inter-module communication.
 * 
 * INTEGRATION:
 * ------------
 * Physics modules will use these utilities when emitting and handling physics events.
 * The event naming function is particularly useful for debugging physics module
 * interactions and event flow tracing.
 */

/**
 * Static physics event type names for debugging and logging
 */
static const char *physics_event_type_names[] = {
    [PHYSICS_EVENT_COOLING_COMPLETED] = "COOLING_COMPLETED",
    [PHYSICS_EVENT_STAR_FORMATION_OCCURRED] = "STAR_FORMATION_OCCURRED", 
    [PHYSICS_EVENT_FEEDBACK_APPLIED] = "FEEDBACK_APPLIED",
    [PHYSICS_EVENT_AGN_ACTIVITY] = "AGN_ACTIVITY",
    [PHYSICS_EVENT_DISK_INSTABILITY] = "DISK_INSTABILITY",
    [PHYSICS_EVENT_MERGER_DETECTED] = "MERGER_DETECTED",
    [PHYSICS_EVENT_REINCORPORATION_COMPUTED] = "REINCORPORATION_COMPUTED",
    [PHYSICS_EVENT_INFALL_COMPUTED] = "INFALL_COMPUTED",
    [PHYSICS_EVENT_COLD_GAS_UPDATED] = "COLD_GAS_UPDATED",
    [PHYSICS_EVENT_HOT_GAS_UPDATED] = "HOT_GAS_UPDATED",
    [PHYSICS_EVENT_STELLAR_MASS_UPDATED] = "STELLAR_MASS_UPDATED",
    [PHYSICS_EVENT_METALS_UPDATED] = "METALS_UPDATED",
    [PHYSICS_EVENT_BLACK_HOLE_MASS_UPDATED] = "BLACK_HOLE_MASS_UPDATED"
};

/**
 * Get the name of a physics event type
 */
const char *physics_event_type_name(physics_event_type_t type) {
    if (type >= PHYSICS_EVENT_COOLING_COMPLETED && type < PHYSICS_EVENT_TYPE_MAX) {
        if (type < sizeof(physics_event_type_names) / sizeof(physics_event_type_names[0])) {
            const char *name = physics_event_type_names[type];
            return name ? name : "UNDEFINED_PHYSICS_EVENT";
        }
    }
    return "UNKNOWN_PHYSICS_EVENT";
}
