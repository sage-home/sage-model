#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_allvars.h"
#include "core_merger_queue.h"
#include "core_pipeline_system.h"
#include "core_module_system.h"
#include "core_module_callback.h"
#include "core_logging.h"
#include "core_merger_processor.h"

/* Module ID for core merger processor (negative to distinguish from physics modules) */
#define MODULE_ID_CORE_MERGER_PROCESSOR (-2)

int core_process_merger_queue_agnostically(struct pipeline_context *pipeline_ctx)
{
    if (pipeline_ctx == NULL) {
        LOG_ERROR("Null pipeline context in core merger processor");
        return -1;
    }
    
    /* Get merger queue from pipeline context */
    struct merger_event_queue *queue = pipeline_ctx->merger_queue;
    if (queue == NULL) {
        /* Try to get from evolution context via user_data */
        if (pipeline_ctx->user_data != NULL) {
            struct evolution_context *evolution_ctx = (struct evolution_context *)pipeline_ctx->user_data;
            queue = evolution_ctx->merger_queue;
        }
    }
    
    if (queue == NULL) {
        LOG_ERROR("Merger queue is NULL in pipeline context for core merger processor");
        return -1;
    }
    
    const struct params *run_params = pipeline_ctx->params;
    if (run_params == NULL) {
        LOG_ERROR("Run parameters are NULL in pipeline context");
        return -1;
    }
    
    LOG_DEBUG("Core merger processor handling %d events", queue->num_events);
    
    /* Process all events in the queue */
    for (int i = 0; i < queue->num_events; i++) {
        struct merger_event *event = &queue->events[i];
        
        /* Validate galaxy indices */
        if (event->satellite_index < 0 || event->satellite_index >= pipeline_ctx->ngal ||
            event->central_index < 0 || event->central_index >= pipeline_ctx->ngal) {
            LOG_WARNING("Invalid galaxy indices in merger event: satellite=%d, central=%d (ngal=%d) - skipping event", 
                       event->satellite_index, event->central_index, pipeline_ctx->ngal);
            continue;
        }
        
        /* Prepare arguments for physics handler */
        merger_handler_args_t handler_args;
        handler_args.event = *event;
        handler_args.pipeline_ctx = pipeline_ctx;
        
        /* Determine handler module and function names based on event type */
        const char *handler_module_name;
        const char *handler_function_name;
        
        if (event->merger_time > 0.0) {
            /* Disruption event */
            handler_module_name = run_params->runtime.DisruptionHandlerModuleName;
            handler_function_name = run_params->runtime.DisruptionHandlerFunctionName;
            LOG_DEBUG("Processing disruption event: satellite=%d, central=%d via %s::%s", 
                     event->satellite_index, event->central_index, 
                     handler_module_name, handler_function_name);
        } else {
            /* Merger event */
            handler_module_name = run_params->runtime.MergerHandlerModuleName;
            handler_function_name = run_params->runtime.MergerHandlerFunctionName;
            LOG_DEBUG("Processing merger event: satellite=%d, central=%d via %s::%s", 
                     event->satellite_index, event->central_index, 
                     handler_module_name, handler_function_name);
        }
        
        /* Invoke the physics handler */
        int error_code = 0;
        int invoke_status = module_invoke(
            MODULE_ID_CORE_MERGER_PROCESSOR, /* Use system-level caller_id */
            MODULE_TYPE_MERGERS,
            handler_module_name,
            handler_function_name,
            &error_code,
            &handler_args,
            NULL
        );
        
        if (invoke_status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Failed to invoke merger handler %s::%s (status=%d, error=%d)", 
                     handler_module_name, handler_function_name, invoke_status, error_code);
            /* Continue processing other events even if one fails */
        }
    }
    
    /* Clear the queue after processing */
    init_merger_queue(queue);
    
    return MODULE_STATUS_SUCCESS;
}
