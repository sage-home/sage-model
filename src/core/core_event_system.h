#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core_allvars.h"

/**
 * @file core_event_system.h
 * @brief Event-Based Communication System for SAGE
 * 
 * This file defines the event system that allows physics modules to communicate
 * with each other without direct dependencies. It enables modules to emit events
 * when significant state changes occur, and other modules can register handlers
 * to respond to these events.
 */

/* Maximum number of event handlers per event type */
#define MAX_EVENT_HANDLERS 32
#define MAX_EVENT_TYPES 64
#define MAX_EVENT_TYPE_NAME 32
#define MAX_EVENT_HANDLER_NAME 64
#define MAX_EVENT_DATA_SIZE 256

/**
 * Event type identifiers
 * 
 * Each event has a unique type identifier that determines what
 * data it contains and which handlers will receive it.
 */
typedef enum event_type {
    EVENT_TYPE_UNKNOWN = 0,
    
    /* Galaxy lifecycle events */
    EVENT_GALAXY_CREATED = 1,
    EVENT_GALAXY_COPIED = 2,
    EVENT_GALAXY_MERGED = 3,
    
    /* Major physics process events */
    EVENT_COOLING_COMPLETED = 10,
    EVENT_STAR_FORMATION_OCCURRED = 11,
    EVENT_FEEDBACK_APPLIED = 12,
    EVENT_AGN_ACTIVITY = 13,
    EVENT_DISK_INSTABILITY = 14,
    EVENT_MERGER_DETECTED = 15,
    EVENT_REINCORPORATION_COMPUTED = 16,
    EVENT_INFALL_COMPUTED = 17,
    
    /* Property update events */
    EVENT_COLD_GAS_UPDATED = 30,
    EVENT_HOT_GAS_UPDATED = 31,
    EVENT_STELLAR_MASS_UPDATED = 32,
    EVENT_METALS_UPDATED = 33,
    EVENT_BLACK_HOLE_MASS_UPDATED = 34,
    
    /* System events */
    EVENT_MODULE_ACTIVATED = 50,
    EVENT_MODULE_DEACTIVATED = 51,
    EVENT_PARAMETER_CHANGED = 52,
    
    /* Custom/reserved events (for module-specific use) */
    EVENT_CUSTOM_BEGIN = 1000,
    EVENT_CUSTOM_END = 1999,
    
    EVENT_TYPE_MAX
} event_type_t;

/**
 * Event priority levels
 * 
 * These are used to control the order in which event handlers
 * are called for a given event.
 */
typedef enum event_priority {
    EVENT_PRIORITY_LOW = 0,
    EVENT_PRIORITY_NORMAL = 10,
    EVENT_PRIORITY_HIGH = 20,
    EVENT_PRIORITY_CRITICAL = 30
} event_priority_t;

/**
 * Event status codes
 * 
 * Return codes used by event functions to indicate success or failure
 */
typedef enum event_status {
    EVENT_STATUS_SUCCESS = 0,
    EVENT_STATUS_ERROR = -1,
    EVENT_STATUS_INVALID_ARGS = -2,
    EVENT_STATUS_NOT_IMPLEMENTED = -3,
    EVENT_STATUS_INITIALIZATION_FAILED = -4,
    EVENT_STATUS_NOT_INITIALIZED = -5,
    EVENT_STATUS_HANDLER_EXISTS = -6,
    EVENT_STATUS_HANDLER_NOT_FOUND = -7,
    EVENT_STATUS_OUT_OF_MEMORY = -8,
    EVENT_STATUS_MAX_HANDLERS = -9,
    EVENT_STATUS_MAX_EVENTS = -10
} event_status_t;

/**
 * Event flags
 * 
 * Flags that control how events are processed
 */
typedef enum event_flags {
    EVENT_FLAG_NONE = 0,
    EVENT_FLAG_PROPAGATE = (1 << 0),     /* Continue to call other handlers even if one returns false */
    EVENT_FLAG_SYNCHRONOUS = (1 << 1),   /* Handle the event immediately rather than queueing */
    EVENT_FLAG_LOGGING = (1 << 2),       /* Log this event for debugging */
    EVENT_FLAG_INTERNAL = (1 << 3)       /* Event is internal to the system */
} event_flags_t;

/* Forward declaration */
struct event;

/**
 * Event handler function type
 * 
 * Functions must match this signature to be registered as event handlers.
 * 
 * @param event Pointer to the event being handled
 * @param user_data User data passed when the handler was registered
 * @return true if the event was handled successfully, false otherwise
 */
typedef bool (*event_handler_fn)(const struct event *event, void *user_data);

/**
 * Event structure
 * 
 * Contains information about an event, including its type, source,
 * and any relevant data.
 */
typedef struct event {
    event_type_t type;                   /* Event type identifier */
    char type_name[MAX_EVENT_TYPE_NAME]; /* String name of the event type */
    uint32_t flags;                      /* Event flags */
    int source_module_id;                /* ID of the module that emitted the event */
    int galaxy_index;                    /* Index of the related galaxy, or -1 if not applicable */
    int step;                            /* Current timestep, or -1 if not applicable */
    
    /* Event-specific data */
    uint8_t data[MAX_EVENT_DATA_SIZE];   /* Raw event data */
    size_t data_size;                    /* Size of the event data in bytes */
} event_t;

/**
 * Event handler registration information
 * 
 * This structure contains information about a registered event handler.
 */
typedef struct event_handler {
    event_handler_fn handler;            /* Handler function */
    void *user_data;                     /* User data to pass to the handler */
    int module_id;                       /* ID of the module that registered the handler */
    char name[MAX_EVENT_HANDLER_NAME];   /* Name of the handler (for debugging) */
    event_priority_t priority;           /* Priority of the handler */
    bool enabled;                        /* Whether the handler is currently enabled */
} event_handler_t;

/**
 * Event system state
 * 
 * This structure maintains the global state of the event system,
 * including registered handlers and event queues.
 */
typedef struct event_system {
    bool initialized;                    /* Whether the event system is initialized */
    
    /* Registered event handlers by type */
    struct {
        event_type_t type;               /* Event type */
        event_handler_t handlers[MAX_EVENT_HANDLERS]; /* Handlers for this event type */
        int num_handlers;                /* Number of registered handlers */
    } event_handlers[MAX_EVENT_TYPES];
    int num_event_types;                 /* Number of event types with handlers */
    
    /* Event logging and debugging */
    bool logging_enabled;                /* Whether event logging is enabled */
    uint32_t log_filter;                 /* Bitmap of event types to log */
    void (*log_callback)(const event_t *event); /* Custom logging callback */
} event_system_t;

/* Global event system */
extern event_system_t *global_event_system;

/**
 * Initialize the event system
 * 
 * Sets up the global event system and prepares it for event handling.
 * 
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_system_initialize(void);

/**
 * Clean up the event system
 * 
 * Releases resources used by the event system and unregisters all handlers.
 * 
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_system_cleanup(void);

/**
 * Register an event handler
 * 
 * Adds a handler function that will be called when events of the specified
 * type are dispatched.
 * 
 * @param event_type Type of event to handle
 * @param handler Function to call when the event occurs
 * @param user_data User data to pass to the handler
 * @param module_id ID of the module registering the handler
 * @param handler_name Name of the handler (for debugging)
 * @param priority Priority of the handler (determines call order)
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_register_handler(
    event_type_t event_type,
    event_handler_fn handler,
    void *user_data,
    int module_id,
    const char *handler_name,
    event_priority_t priority);

/**
 * Unregister an event handler
 * 
 * Removes a previously registered handler function.
 * 
 * @param event_type Type of event the handler was registered for
 * @param handler Function that was registered
 * @param module_id ID of the module that registered the handler
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_unregister_handler(
    event_type_t event_type,
    event_handler_fn handler,
    int module_id);

/**
 * Create an event
 * 
 * Allocates and initializes a new event structure.
 * 
 * @param type Type of event to create
 * @param source_module_id ID of the module creating the event
 * @param galaxy_index Index of the related galaxy, or -1 if not applicable
 * @param step Current timestep, or -1 if not applicable
 * @param data Pointer to event-specific data
 * @param data_size Size of the event data in bytes
 * @param flags Event flags
 * @param event Output pointer to the created event
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_create(
    event_type_t type,
    int source_module_id,
    int galaxy_index,
    int step,
    const void *data,
    size_t data_size,
    uint32_t flags,
    event_t *event);

/**
 * Dispatch an event
 * 
 * Sends an event to all registered handlers of the appropriate type.
 * 
 * @param event Event to dispatch
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_dispatch(const event_t *event);

/**
 * Enable event logging
 * 
 * Turns on logging for events, optionally filtering by event type.
 * 
 * @param enabled Whether to enable logging
 * @param filter Bitmap of event types to log, or 0 for all
 * @param callback Custom logging callback, or NULL for default
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_enable_logging(bool enabled, uint32_t filter, void (*callback)(const event_t *event));

/**
 * Get the name of an event type
 * 
 * Returns a string description of an event type.
 * 
 * @param type Event type to describe
 * @return String description of the event type
 */
const char *event_type_name(event_type_t type);

/**
 * Create and dispatch an event (convenience function)
 * 
 * Creates and immediately dispatches an event.
 * 
 * @param type Type of event to create
 * @param source_module_id ID of the module creating the event
 * @param galaxy_index Index of the related galaxy, or -1 if not applicable
 * @param step Current timestep, or -1 if not applicable
 * @param data Pointer to event-specific data
 * @param data_size Size of the event data in bytes
 * @param flags Event flags
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_emit(
    event_type_t type,
    int source_module_id,
    int galaxy_index,
    int step,
    const void *data,
    size_t data_size,
    uint32_t flags);

/**
 * Event-specific data structures
 * 
 * These structures define the data payload for different event types.
 * They should be used when creating events to ensure type safety.
 */

/* Galaxy lifecycle events */
typedef struct {
    int32_t halo_index;
    float cooling_radius;
} event_galaxy_created_data_t;

typedef struct {
    int32_t source_index;
    int32_t source_halo_index;
} event_galaxy_copied_data_t;

typedef struct {
    int32_t primary_index;
    int32_t secondary_index;
    float mass_ratio;
    int32_t merger_type;  /* 0 = minor, 1 = major */
} event_galaxy_merged_data_t;

/* Physics process events */
typedef struct {
    float cooling_rate;
    float cooling_radius;
    float hot_gas_cooled;
} event_cooling_completed_data_t;

typedef struct {
    float stars_formed;
    float stars_to_disk;
    float stars_to_bulge;
    float metallicity;
} event_star_formation_occurred_data_t;

typedef struct {
    float energy_injected;
    float mass_reheated;
    float metals_ejected;
} event_feedback_applied_data_t;

typedef struct {
    float energy_released;
    float mass_accreted;
    float mass_ejected;
} event_agn_activity_data_t;

/* Property update events */
typedef struct {
    float old_value;
    float new_value;
    float delta;
} event_property_updated_data_t;

/* System events */
typedef struct {
    int module_id;
    int module_type;
    char module_name[MAX_EVENT_HANDLER_NAME];
} event_module_status_data_t;

typedef struct {
    char param_name[MAX_EVENT_HANDLER_NAME];
    char param_value[MAX_EVENT_HANDLER_NAME];
} event_parameter_changed_data_t;

/* Helper macros for working with event data */

/**
 * Get typed event data
 * 
 * Casts the event data to a specific type.
 * 
 * @param event Pointer to the event
 * @param type Type to cast the data to
 * @return Pointer to the typed event data
 */
#define EVENT_DATA(event, type) ((const type*)((event)->data))

/**
 * Check if an event is of a specific type
 * 
 * @param event Pointer to the event
 * @param event_type Event type to check for
 * @return true if the event is of the specified type, false otherwise
 */
#define EVENT_IS_TYPE(event, event_type) ((event)->type == (event_type))

#ifdef __cplusplus
}
#endif
