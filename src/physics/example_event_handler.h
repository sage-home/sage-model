#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file example_event_handler.h
 * @brief Example event handler for SAGE
 * 
 * This file demonstrates how to use the event system to handle events
 * emitted by physics modules.
 */

/**
 * Register example event handlers
 * 
 * This function registers handlers for various event types to demonstrate
 * the event system.
 * 
 * @return 0 on success, nonzero on failure
 */
int register_example_event_handlers(void);

/**
 * Unregister example event handlers
 * 
 * This function unregisters the event handlers.
 * 
 * @return 0 on success, nonzero on failure
 */
int unregister_example_event_handlers(void);

#ifdef __cplusplus
}
#endif