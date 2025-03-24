#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../core/core_allvars.h"
#include "../core/core_logging.h"
#include "../core/core_module_system.h"
#include "../core/core_event_system.h"

/**
 * @file example_event_handler.c
 * @brief Example event handler for SAGE
 * 
 * This file demonstrates how to use the event system to handle events
 * emitted by physics modules.
 */

/**
 * Handler for cooling completed events
 * 
 * This function is called when a cooling event is emitted.
 * It demonstrates how to access event data and perform actions
 * in response to an event.
 * 
 * @param event The event that occurred
 * @param user_data User data passed during handler registration
 * @return true if the event was handled successfully, false otherwise
 */
bool handle_cooling_event(const event_t *event, void *user_data) {
    /* Suppress unused parameter warning */
    (void)user_data;
    
    if (event == NULL) {
        LOG_ERROR("NULL event pointer in cooling event handler");
        return false;
    }
    
    /* Access event data */
    if (event->data_size < sizeof(event_cooling_completed_data_t)) {
        LOG_ERROR("Invalid event data size for cooling event");
        return false;
    }
    
    /* Cast the event data to the appropriate type */
    const event_cooling_completed_data_t *cooling_data = EVENT_DATA(event, event_cooling_completed_data_t);
    
    /* Print information about the cooling event */
    LOG_INFO("Cooling Event: galaxy=%d, cooling_rate=%.4e, cooling_radius=%.4e, hot_gas_cooled=%.4e",
            event->galaxy_index,
            cooling_data->cooling_rate,
            cooling_data->cooling_radius,
            cooling_data->hot_gas_cooled);
    
    return true;
}

/**
 * Handler for star formation events
 * 
 * This function is called when a star formation event is emitted.
 * It demonstrates how to handle multiple event types.
 * 
 * @param event The event that occurred
 * @param user_data User data passed during handler registration
 * @return true if the event was handled successfully, false otherwise
 */
bool handle_star_formation_event(const event_t *event, void *user_data) {
    /* Suppress unused parameter warning */
    (void)user_data;
    
    if (event == NULL) {
        LOG_ERROR("NULL event pointer in star formation event handler");
        return false;
    }
    
    /* Access event data */
    if (event->data_size < sizeof(event_star_formation_occurred_data_t)) {
        LOG_ERROR("Invalid event data size for star formation event");
        return false;
    }
    
    /* Cast the event data to the appropriate type */
    const event_star_formation_occurred_data_t *sf_data = EVENT_DATA(event, event_star_formation_occurred_data_t);
    
    /* Print information about the star formation event */
    LOG_INFO("Star Formation Event: galaxy=%d, stars_formed=%.4e, to_disk=%.4e, to_bulge=%.4e",
            event->galaxy_index,
            sf_data->stars_formed,
            sf_data->stars_to_disk,
            sf_data->stars_to_bulge);
    
    return true;
}

/**
 * Module ID for this example
 */
static int example_module_id = -1;

/**
 * Register event handlers
 * 
 * This function registers handlers for various event types to demonstrate
 * the event system.
 * 
 * @return 0 on success, nonzero on failure
 */
int register_example_event_handlers(void) {
    /* Generate a fake module ID for this example */
    if (example_module_id < 0) {
        example_module_id = 9999;
    }
    
    /* Register handler for cooling events */
    event_status_t status = event_register_handler(
        EVENT_COOLING_COMPLETED,        /* Event type */
        handle_cooling_event,          /* Handler function */
        NULL,                          /* User data (none in this example) */
        example_module_id,             /* Module ID */
        "ExampleCoolingHandler",       /* Handler name */
        EVENT_PRIORITY_NORMAL          /* Priority */
    );
    
    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register cooling event handler, status = %d", status);
        return -1;
    }
    
    /* Register handler for star formation events */
    status = event_register_handler(
        EVENT_STAR_FORMATION_OCCURRED,  /* Event type */
        handle_star_formation_event,    /* Handler function */
        NULL,                          /* User data (none in this example) */
        example_module_id,             /* Module ID */
        "ExampleStarFormationHandler", /* Handler name */
        EVENT_PRIORITY_NORMAL          /* Priority */
    );
    
    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register star formation event handler, status = %d", status);
        return -1;
    }
    
    /* Enable event logging */
    status = event_enable_logging(
        true,   /* Enable logging */
        0,      /* Log all event types */
        NULL    /* Use default logging function */
    );
    
    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to enable event logging, status = %d", status);
        return -1;
    }
    
    LOG_INFO("Example event handlers registered successfully");
    
    return 0;
}

/**
 * Unregister event handlers
 * 
 * This function unregisters the event handlers.
 * 
 * @return 0 on success, nonzero on failure
 */
int unregister_example_event_handlers(void) {
    /* Unregister handler for cooling events */
    event_status_t status = event_unregister_handler(
        EVENT_COOLING_COMPLETED,
        handle_cooling_event,
        example_module_id
    );
    
    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to unregister cooling event handler, status = %d", status);
        return -1;
    }
    
    /* Unregister handler for star formation events */
    status = event_unregister_handler(
        EVENT_STAR_FORMATION_OCCURRED,
        handle_star_formation_event,
        example_module_id
    );
    
    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to unregister star formation event handler, status = %d", status);
        return -1;
    }
    
    LOG_INFO("Example event handlers unregistered successfully");
    
    return 0;
}
