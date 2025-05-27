#ifndef CORE_MERGER_PROCESSOR_H
#define CORE_MERGER_PROCESSOR_H

#include "core_allvars.h"
#include "core_pipeline_system.h"
#include "core_merger_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Argument structure for invoking merger physics handlers
 * 
 * This structure is passed to physics modules when they are invoked
 * to handle merger or disruption events.
 */
typedef struct {
    struct merger_event event;           /**< The merger event to process */
    struct pipeline_context *pipeline_ctx; /**< Full pipeline context */
} merger_handler_args_t;

/**
 * @brief Process all merger events in the queue using configured physics handlers
 * 
 * This function iterates through all queued merger events and dispatches them
 * to the appropriate physics modules via module_invoke. The specific modules
 * and functions to call are determined by configuration parameters.
 * 
 * @param pipeline_ctx Pipeline context containing the merger queue and configuration
 * @return 0 on success, non-zero error code on failure
 */
int core_process_merger_queue_agnostically(struct pipeline_context *pipeline_ctx);

#ifdef __cplusplus
}
#endif

#endif // CORE_MERGER_PROCESSOR_H
