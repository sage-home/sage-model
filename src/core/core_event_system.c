#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_event_system.h"
#include "core_logging.h"
#include "core_mymalloc.h"
#include "core_pipeline_system.h"  /* Include for pipeline_context definition */
#include "core_evolution_diagnostics.h"  /* Include for evolution_diagnostics_add_event */

/* Global event system */
event_system_t *global_event_system = NULL;

/* Forward declarations for internal functions */
static int find_event_type_index(event_type_t event_type);
static int ensure_event_type(event_type_t event_type);
static int compare_handlers_by_priority(const void *a, const void *b);
static void sort_handlers_by_priority(int event_type_index);
static void log_event(const event_t *event);

/* Static event type names for debugging and logging */
static const char *event_type_names[] = {
    "UNKNOWN",
    "GALAXY_CREATED",
    "GALAXY_COPIED",
    "GALAXY_MERGED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "COOLING_COMPLETED",
    "STAR_FORMATION_OCCURRED",
    "FEEDBACK_APPLIED",
    "AGN_ACTIVITY",
    "DISK_INSTABILITY",
    "MERGER_DETECTED",
    "REINCORPORATION_COMPUTED",
    "INFALL_COMPUTED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "COLD_GAS_UPDATED",
    "HOT_GAS_UPDATED",
    "STELLAR_MASS_UPDATED",
    "METALS_UPDATED",
    "BLACK_HOLE_MASS_UPDATED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "UNDEFINED",
    "MODULE_ACTIVATED",
    "MODULE_DEACTIVATED",
    "PARAMETER_CHANGED",
    "CUSTOM_EVENT"
};

/**
 * Get the name of an event type
 * 
 * Returns a string description of an event type.
 * 
 * @param type Event type to describe
 * @return String description of the event type
 */
const char *event_type_name(event_type_t type) {
    if (type >= EVENT_TYPE_UNKNOWN && type < EVENT_CUSTOM_BEGIN) {
        /* Use predefined names for built-in event types */
        if (type < sizeof(event_type_names) / sizeof(event_type_names[0])) {
            return event_type_names[type];
        } else {
            return "UNDEFINED";
        }
    } else if (type >= EVENT_CUSTOM_BEGIN && type <= EVENT_CUSTOM_END) {
        /* Custom event types */
        return "CUSTOM_EVENT";
    } else {
        /* Unknown type */
        return "UNKNOWN";
    }
}

/**
 * Initialize the event system
 * 
 * Sets up the global event system and prepares it for event handling.
 * 
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_system_initialize(void) {
    /* Check if already initialized */
    if (global_event_system != NULL) {
        LOG_WARNING("Event system already initialized");
        return EVENT_STATUS_INITIALIZATION_FAILED;
    }
    
    /* Allocate memory for the event system */
    global_event_system = (event_system_t *)mymalloc(sizeof(event_system_t));
    if (global_event_system == NULL) {
        LOG_ERROR("Failed to allocate memory for event system");
        return EVENT_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize event system fields */
    global_event_system->initialized = true;
    global_event_system->num_event_types = 0;
    global_event_system->logging_enabled = false;
    global_event_system->log_filter = 0;
    global_event_system->log_callback = NULL;
    
    /* Initialize handler arrays for each event type */
    for (int i = 0; i < MAX_EVENT_TYPES; i++) {
        global_event_system->event_handlers[i].type = EVENT_TYPE_UNKNOWN;
        global_event_system->event_handlers[i].num_handlers = 0;
        
        for (int j = 0; j < MAX_EVENT_HANDLERS; j++) {
            global_event_system->event_handlers[i].handlers[j].handler = NULL;
            global_event_system->event_handlers[i].handlers[j].user_data = NULL;
            global_event_system->event_handlers[i].handlers[j].module_id = -1;
            global_event_system->event_handlers[i].handlers[j].name[0] = '\0';
            global_event_system->event_handlers[i].handlers[j].priority = EVENT_PRIORITY_NORMAL;
            global_event_system->event_handlers[i].handlers[j].enabled = false;
        }
    }
    
    LOG_INFO("Event system initialized");
    return EVENT_STATUS_SUCCESS;
}

/**
 * Clean up the event system
 * 
 * Releases resources used by the event system and unregisters all handlers.
 * 
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_system_cleanup(void) {
    if (global_event_system == NULL) {
        LOG_WARNING("Event system not initialized");
        return EVENT_STATUS_NOT_INITIALIZED;
    }
    
    /* Free the event system */
    myfree(global_event_system);
    global_event_system = NULL;
    
    LOG_INFO("Event system cleaned up");
    return EVENT_STATUS_SUCCESS;
}

/**
 * Check if the event system is initialized
 * 
 * @return true if the event system is initialized, false otherwise
 */
bool event_system_is_initialized(void) {
    return global_event_system != NULL && global_event_system->initialized;
}

/**
 * Register a custom event type
 * 
 * Creates a new event type for custom events.
 * 
 * @param name Name of the event type
 * @param data_size Size of the event data structure (currently unused)
 * @return ID of the registered event type, or negative error code on failure
 */
int event_register_type(const char *name, size_t data_size) {
    (void)data_size; /* Suppress unused parameter warning */
    
    if (!event_system_is_initialized()) {
        /* Auto-initialize if not done already */
        event_status_t status = event_system_initialize();
        if (status != EVENT_STATUS_SUCCESS) {
            return -1;
        }
    }
    
    /* Validate arguments */
    if (name == NULL) {
        LOG_ERROR("Invalid event type name");
        return -1;
    }
    
    /* Find an available event ID in the custom range */
    event_type_t event_id = EVENT_CUSTOM_BEGIN;
    while (event_id <= EVENT_CUSTOM_END) {
        /* Check if this ID is already in use */
        bool found = false;
        for (int i = 0; i < global_event_system->num_event_types; i++) {
            if (global_event_system->event_handlers[i].type == event_id) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            /* This ID is available, use it */
            break;
        }
        
        event_id = (event_type_t)(event_id + 1);
    }
    
    if (event_id > EVENT_CUSTOM_END) {
        LOG_ERROR("No more custom event types available");
        return -1;
    }
    
    /* Ensure the event type exists in our registry */
    int event_index = ensure_event_type(event_id);
    if (event_index < 0) {
        LOG_ERROR("Failed to register event type '%s'", name);
        return -1;
    }
    
    LOG_INFO("Registered custom event type '%s' with ID %d", name, event_id);
    return (int)event_id;
}

/**
 * Find the index of an event type in the handler array
 * 
 * @param event_type Type of event to find
 * @return Index in the array, or -1 if not found
 */
static int find_event_type_index(event_type_t event_type) {
    if (global_event_system == NULL) {
        return -1;
    }
    
    for (int i = 0; i < global_event_system->num_event_types; i++) {
        if (global_event_system->event_handlers[i].type == event_type) {
            return i;
        }
    }
    
    return -1;
}

/**
 * Ensure an event type exists in the handler array
 * 
 * @param event_type Type of event to ensure exists
 * @return Index in the array, or -1 on error
 */
static int ensure_event_type(event_type_t event_type) {
    if (global_event_system == NULL) {
        return -1;
    }
    
    /* Check if the event type already exists */
    int index = find_event_type_index(event_type);
    if (index >= 0) {
        return index;
    }
    
    /* Add a new event type if there's room */
    if (global_event_system->num_event_types >= MAX_EVENT_TYPES) {
        LOG_ERROR("Too many event types");
        return -1;
    }
    
    /* Initialize the new event type entry */
    index = global_event_system->num_event_types;
    global_event_system->event_handlers[index].type = event_type;
    global_event_system->event_handlers[index].num_handlers = 0;
    
    /* Initialize handlers for this type */
    for (int j = 0; j < MAX_EVENT_HANDLERS; j++) {
        global_event_system->event_handlers[index].handlers[j].handler = NULL;
        global_event_system->event_handlers[index].handlers[j].user_data = NULL;
        global_event_system->event_handlers[index].handlers[j].module_id = -1;
        global_event_system->event_handlers[index].handlers[j].name[0] = '\0';
        global_event_system->event_handlers[index].handlers[j].priority = EVENT_PRIORITY_NORMAL;
        global_event_system->event_handlers[index].handlers[j].enabled = false;
    }
    
    global_event_system->num_event_types++;
    return index;
}

/**
 * Compare two event handlers for sorting by priority
 * 
 * @param a First handler
 * @param b Second handler
 * @return Comparison result (-1, 0, or 1)
 */
static int compare_handlers_by_priority(const void *a, const void *b) {
    const event_handler_t *handler_a = (const event_handler_t *)a;
    const event_handler_t *handler_b = (const event_handler_t *)b;
    
    /* Sort in descending order of priority (higher priority first) */
    if (handler_a->priority > handler_b->priority) {
        return -1;
    } else if (handler_a->priority < handler_b->priority) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Sort handlers for an event type by priority
 * 
 * @param event_type_index Index of the event type in the handler array
 */
static void sort_handlers_by_priority(int event_type_index) {
    if (global_event_system == NULL || event_type_index < 0 || 
        event_type_index >= global_event_system->num_event_types) {
        return;
    }
    
    int num_handlers = global_event_system->event_handlers[event_type_index].num_handlers;
    if (num_handlers <= 1) {
        return;  /* No need to sort */
    }
    
    /* Sort the handlers by priority */
    qsort(global_event_system->event_handlers[event_type_index].handlers,
          num_handlers,
          sizeof(event_handler_t),
          compare_handlers_by_priority);
}

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
    event_priority_t priority) {
    
    if (global_event_system == NULL) {
        /* Auto-initialize if not done already */
        event_status_t status = event_system_initialize();
        if (status != EVENT_STATUS_SUCCESS) {
            return status;
        }
    }
    
    /* Validate arguments */
    if (handler == NULL) {
        LOG_ERROR("Invalid handler function");
        return EVENT_STATUS_INVALID_ARGS;
    }
    
    /* Ensure the event type exists in our registry */
    int event_index = ensure_event_type(event_type);
    if (event_index < 0) {
        LOG_ERROR("Failed to register handler for event type %d", event_type);
        return EVENT_STATUS_ERROR;
    }
    
    /* Check if handler is already registered */
    for (int i = 0; i < global_event_system->event_handlers[event_index].num_handlers; i++) {
        if (global_event_system->event_handlers[event_index].handlers[i].handler == handler &&
            global_event_system->event_handlers[event_index].handlers[i].module_id == module_id) {
            LOG_WARNING("Handler already registered for event type %d", event_type);
            return EVENT_STATUS_HANDLER_EXISTS;
        }
    }
    
    /* Check if we have room for another handler */
    if (global_event_system->event_handlers[event_index].num_handlers >= MAX_EVENT_HANDLERS) {
        LOG_ERROR("Too many handlers for event type %d", event_type);
        return EVENT_STATUS_MAX_HANDLERS;
    }
    
    /* Add the new handler */
    int handler_index = global_event_system->event_handlers[event_index].num_handlers;
    global_event_system->event_handlers[event_index].handlers[handler_index].handler = handler;
    global_event_system->event_handlers[event_index].handlers[handler_index].user_data = user_data;
    global_event_system->event_handlers[event_index].handlers[handler_index].module_id = module_id;
    global_event_system->event_handlers[event_index].handlers[handler_index].priority = priority;
    global_event_system->event_handlers[event_index].handlers[handler_index].enabled = true;
    
    /* Copy the handler name */
    if (handler_name != NULL) {
        strncpy(global_event_system->event_handlers[event_index].handlers[handler_index].name,
                handler_name,
                MAX_EVENT_HANDLER_NAME - 1);
        global_event_system->event_handlers[event_index].handlers[handler_index].name[MAX_EVENT_HANDLER_NAME - 1] = '\0';
    } else {
        snprintf(global_event_system->event_handlers[event_index].handlers[handler_index].name,
                 MAX_EVENT_HANDLER_NAME,
                 "Handler_%d_%d",
                 module_id,
                 handler_index);
    }
    
    global_event_system->event_handlers[event_index].num_handlers++;
    
    /* Sort handlers by priority */
    sort_handlers_by_priority(event_index);
    
    LOG_INFO("Registered handler '%s' for event type %d with priority %d",
             global_event_system->event_handlers[event_index].handlers[handler_index].name,
             event_type,
             priority);
    
    return EVENT_STATUS_SUCCESS;
}

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
    int module_id) {
    
    if (global_event_system == NULL) {
        LOG_ERROR("Event system not initialized");
        return EVENT_STATUS_NOT_INITIALIZED;
    }
    
    /* Find the event type */
    int event_index = find_event_type_index(event_type);
    if (event_index < 0) {
        LOG_ERROR("Event type %d not found", event_type);
        return EVENT_STATUS_HANDLER_NOT_FOUND;
    }
    
    /* Find the handler */
    int handler_index = -1;
    for (int i = 0; i < global_event_system->event_handlers[event_index].num_handlers; i++) {
        if (global_event_system->event_handlers[event_index].handlers[i].handler == handler &&
            global_event_system->event_handlers[event_index].handlers[i].module_id == module_id) {
            handler_index = i;
            break;
        }
    }
    
    if (handler_index < 0) {
        LOG_ERROR("Handler not found for event type %d", event_type);
        return EVENT_STATUS_HANDLER_NOT_FOUND;
    }
    
    /* Remove the handler by shifting the remaining handlers down */
    const char *handler_name = global_event_system->event_handlers[event_index].handlers[handler_index].name;
    for (int i = handler_index; i < global_event_system->event_handlers[event_index].num_handlers - 1; i++) {
        global_event_system->event_handlers[event_index].handlers[i] = 
            global_event_system->event_handlers[event_index].handlers[i + 1];
    }
    
    /* Clear the last handler slot */
    int last_index = global_event_system->event_handlers[event_index].num_handlers - 1;
    global_event_system->event_handlers[event_index].handlers[last_index].handler = NULL;
    global_event_system->event_handlers[event_index].handlers[last_index].user_data = NULL;
    global_event_system->event_handlers[event_index].handlers[last_index].module_id = -1;
    global_event_system->event_handlers[event_index].handlers[last_index].name[0] = '\0';
    global_event_system->event_handlers[event_index].handlers[last_index].priority = EVENT_PRIORITY_NORMAL;
    global_event_system->event_handlers[event_index].handlers[last_index].enabled = false;
    
    global_event_system->event_handlers[event_index].num_handlers--;
    
    LOG_INFO("Unregistered handler '%s' for event type %d", handler_name, event_type);

    return EVENT_STATUS_SUCCESS;
}

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
    event_t *event) {
    
    if (event == NULL) {
        LOG_ERROR("NULL event pointer");
        return EVENT_STATUS_INVALID_ARGS;
    }
    
    /* Validate data size */
    if (data != NULL && data_size > MAX_EVENT_DATA_SIZE) {
        LOG_ERROR("Event data size %zu exceeds maximum %d", data_size, MAX_EVENT_DATA_SIZE);
        return EVENT_STATUS_INVALID_ARGS;
    }
    
    /* Initialize event structure */
    event->type = type;
    strncpy(event->type_name, event_type_name(type), MAX_EVENT_TYPE_NAME - 1);
    event->type_name[MAX_EVENT_TYPE_NAME - 1] = '\0';
    event->flags = flags;
    event->source_module_id = source_module_id;
    event->galaxy_index = galaxy_index;
    event->step = step;
    
    /* Copy event data if provided */
    if (data != NULL && data_size > 0) {
        memcpy(event->data, data, data_size);
        event->data_size = data_size;
    } else {
        event->data_size = 0;
    }
    
    return EVENT_STATUS_SUCCESS;
}

/**
 * Log an event
 * 
 * Records information about an event for debugging purposes.
 * 
 * @param event Event to log
 */
static void log_event(const event_t *event) {
    if (event == NULL) {
        return;
    }
    
    /* Use custom logging callback if provided */
    if (global_event_system->log_callback != NULL) {
        global_event_system->log_callback(event);
        return;
    }
    
    /* Standard logging */
    LOG_DEBUG("EVENT: type=%s, source_module=%d, galaxy=%d, step=%d, data_size=%zu",
              event->type_name,
              event->source_module_id,
              event->galaxy_index,
              event->step,
              event->data_size);
}

/**
 * Dispatch an event
 * 
 * Sends an event to all registered handlers of the appropriate type.
 * 
 * @param event Event to dispatch
 * @return EVENT_STATUS_SUCCESS on success, error code on failure
 */
event_status_t event_dispatch(const event_t *event) {
    // Input validation
    if (!event_system_is_initialized()) {
        LOG_ERROR("Event system not initialized");
        return EVENT_STATUS_NOT_INITIALIZED;
    }
    
    if (event == NULL) {
        LOG_ERROR("NULL event pointer");
        return EVENT_STATUS_INVALID_ARGS;
    }
    
    /* Log the event if logging is enabled */
    if (global_event_system->logging_enabled) {
        /* Check if this event type should be logged */
        if (global_event_system->log_filter == 0 || 
            (global_event_system->log_filter & (1 << (event->type % 32)))) {
            log_event(event);
        }
    }
    
    /* 
     * Track event in evolution diagnostics if available.
     * We need to handle this carefully to avoid accessing potentially invalid memory.
     * Events may come from different sources, and not all events will have
     * a pipeline context that contains an evolution context.
     */
    if (event->source_module_id >= 0 && event->galaxy_index >= 0) {
        /* Only attempt to extract diagnostics from events we know contain pipeline context
         * Currently, we only add pipeline context for specific event types emitted from
         * modular physics components rather than traditional code.
         * 
         * Events from traditional code (source_module_id == 0) with custom event data,
         * like infall_recipe() and others, don't contain pipeline context and should be skipped.
         */
        if (event->source_module_id > 0 && event->data_size >= sizeof(void*)) {
            void *possible_ctx = NULL;
            
            /* Using memcpy to safely extract the pointer */
            memcpy(&possible_ctx, event->data, sizeof(void*));
            
            if (possible_ctx != NULL) {
                /* Basic pointer validation - further validation done before dereferencing */
                struct pipeline_context *pipeline_ctx = NULL;
                
                /* Validate the pointer is valid before casting/using it */
                if (possible_ctx) {
                    pipeline_ctx = (struct pipeline_context*)possible_ctx;
                }
                
                /* Access user_data only if pipeline context looks valid */
                if (pipeline_ctx != NULL && pipeline_ctx->galaxies != NULL && 
                    pipeline_ctx->user_data != NULL) {
                    
                    /* Extract evolution context, checking for validity */
                    struct evolution_context *evolution_ctx = (struct evolution_context*)pipeline_ctx->user_data;
                    
                    /* Only proceed if the evolution context is valid */
                    if (evolution_ctx != NULL) {
                        /* Check if diagnostics is initialized */
                        if (evolution_ctx->diagnostics != NULL) {
                            /* Track the event safely */
                            evolution_diagnostics_add_event(evolution_ctx->diagnostics, event->type);
                        } else {
                            /* Diagnostics not initialized, but that's OK in some paths */
                            LOG_DEBUG("Evolution diagnostics not initialized for event type %d", event->type);
                        }
                    }
                }
            }
        }
    }
    
    /* Find handlers for this event type */
    int event_index = find_event_type_index(event->type);
    if (event_index < 0) {
        /* No handlers for this event type, which is acceptable */
        return EVENT_STATUS_SUCCESS;
    }
    
    int num_handlers = global_event_system->event_handlers[event_index].num_handlers;
    if (num_handlers == 0) {
        /* No handlers for this event type, which is acceptable */
        return EVENT_STATUS_SUCCESS;
    }
    
    /* Call all registered handlers in priority order */
    bool propagate = (event->flags & EVENT_FLAG_PROPAGATE) != 0;
    for (int i = 0; i < num_handlers; i++) {
        event_handler_t *handler = &global_event_system->event_handlers[event_index].handlers[i];
        
        /* Skip disabled handlers */
        if (!handler->enabled) {
            continue;
        }
        
        /* Call the handler */
        bool result = handler->handler(event, handler->user_data);
        
        /* Stop if a handler returns false and propagation is not enabled */
        if (!result && !propagate) {
            LOG_DEBUG("Event handling stopped by handler '%s'", handler->name);
            break;
        }
    }
    
    return EVENT_STATUS_SUCCESS;
}

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
event_status_t event_enable_logging(bool enabled, uint32_t filter, void (*callback)(const event_t *event)) {
    if (global_event_system == NULL) {
        LOG_ERROR("Event system not initialized");
        return EVENT_STATUS_NOT_INITIALIZED;
    }
    
    global_event_system->logging_enabled = enabled;
    global_event_system->log_filter = filter;
    global_event_system->log_callback = callback;
    
    LOG_INFO("Event logging %s", enabled ? "enabled" : "disabled");
    
    return EVENT_STATUS_SUCCESS;
}

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
    uint32_t flags) {
    
    // Input validation
    if (global_event_system == NULL) {
        // No event system, nothing to do
        LOG_DEBUG("Attempted to emit event with NULL event system");
        return EVENT_STATUS_NOT_INITIALIZED;
    }
    
  // Validate data size and pointer
  if (data_size > 0 && data == NULL) {
    LOG_ERROR("Invalid event data: data_size > 0 but data is NULL");
    return EVENT_STATUS_INVALID_ARGS;
  }
  
  if (data_size > MAX_EVENT_DATA_SIZE) {
    LOG_ERROR("Event data size %zu exceeds maximum %d", data_size, MAX_EVENT_DATA_SIZE);
    return EVENT_STATUS_INVALID_ARGS;
  }
    
    event_t event;
    
    /* Create the event */
    event_status_t status = event_create(
        type,
        source_module_id,
        galaxy_index,
        step,
        data,
        data_size,
        flags,
        &event);
    
    if (status != EVENT_STATUS_SUCCESS) {
        return status;
    }
    
    /* Dispatch the event */
    LOG_DEBUG("Emitting event type %d with data_size %zu", type, data_size);
    return event_dispatch(&event);
}
