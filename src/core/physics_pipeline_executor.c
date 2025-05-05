#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_allvars.h" // Include for GALAXY struct if needed by phase functions
#include "core_logging.h"
#include "core_module_system.h"
#include "core_module_callback.h" // Needed for pipeline_execute_with_callback
#include "core_pipeline_system.h"
#include "physics_pipeline_executor.h"
#include "core_properties_sync.h" // Include header for synchronization functions
// #include "../physics/physics_modules.h" // REMOVED - File does not exist and is not needed here

/**
 * Safely synchronize galaxy properties to/from direct fields
 * with thorough safety checks to prevent segmentation faults
 */
static void safe_sync_direct_to_properties(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("safe_sync_direct_to_properties: galaxy pointer is NULL");
        return;
    }
    
    if (galaxy->properties == NULL) {
        LOG_DEBUG("safe_sync_direct_to_properties: galaxy->properties pointer is NULL for GalaxyNr %d", 
                  galaxy->GalaxyNr);
        return;
    }
    
    // Only sync if the galaxy is not merged or disrupted
    if (galaxy->mergeType > 0) {
        LOG_DEBUG("Skipping sync for merged galaxy %d (mergeType=%d)", 
                galaxy->GalaxyNr, galaxy->mergeType);
        return;
    }
    
    // Call the actual sync function
    sync_direct_to_properties(galaxy);
}

/**
 * Safely synchronize galaxy properties from properties struct to direct fields
 * with thorough safety checks to prevent segmentation faults
 */
static void safe_sync_properties_to_direct(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("safe_sync_properties_to_direct: galaxy pointer is NULL");
        return;
    }
    
    if (galaxy->properties == NULL) {
        LOG_DEBUG("safe_sync_properties_to_direct: galaxy->properties pointer is NULL for GalaxyNr %d", 
                  galaxy->GalaxyNr);
        return;
    }
    
    // Only sync if the galaxy is not merged or disrupted
    if (galaxy->mergeType > 0) {
        LOG_DEBUG("Skipping sync for merged galaxy %d (mergeType=%d)", 
                galaxy->GalaxyNr, galaxy->mergeType);
        return;
    }
    
    // Call the actual sync function
    sync_properties_to_direct(galaxy);
}

/*
 * Execute a physics module step based on the current pipeline phase.
 * This function assumes the module pointer provided is valid for the step.
 * The calling code (e.g., pipeline_execute_phase) is responsible for
 * finding the correct module and handling cases where no module is found
 * (e.g., using legacy fallbacks or erroring if required).
 */
int physics_step_executor(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
) {
    if (step == NULL || context == NULL) {
        LOG_ERROR("Invalid arguments to physics step executor (step or context is NULL)");
        return -1;
    }

    /* If no module is provided (e.g., legacy fallback handled elsewhere), do nothing */
    if (module == NULL) {
        LOG_DEBUG("No module provided for step '%s', skipping execution in physics_step_executor.", step->step_name);
        return 0; // Not an error, just nothing to execute here
    }

    /* Check if the module supports the current execution phase */
    if (!(module->phases & context->execution_phase)) {
        LOG_DEBUG("Module '%s' does not support phase %d, skipping step '%s'",
                 module->name, context->execution_phase, step->step_name);
        return 0; // Not an error, just skip this step for this phase
    }

    /* Execute appropriate phase handler based on the context's current phase */
    int status = 0;
    switch (context->execution_phase) {
        case PIPELINE_PHASE_HALO:
            if (module->execute_halo_phase != NULL) {
                // For HALO phase, sync the central galaxy before module execution
                int centralgal = context->centralgal;
                if (centralgal >= 0 && centralgal < context->ngal && context->galaxies != NULL) {
                    safe_sync_direct_to_properties(&context->galaxies[centralgal]);
                }
                
                LOG_DEBUG("Executing HALO phase for module '%s'", module->name);
                status = module->execute_halo_phase(module_data, context);
                
                // Sync back after module execution
                if (centralgal >= 0 && centralgal < context->ngal && context->galaxies != NULL) {
                    safe_sync_properties_to_direct(&context->galaxies[centralgal]);
                }
            } else {
                 LOG_DEBUG("Module '%s' has no HALO phase implementation.", module->name);
            }
            break;

        case PIPELINE_PHASE_GALAXY:
            if (module->execute_galaxy_phase != NULL) {
                // For GALAXY phase, sync the current galaxy before module execution
                int galaxy_idx = context->current_galaxy;
                if (galaxy_idx >= 0 && galaxy_idx < context->ngal && context->galaxies != NULL) {
                    safe_sync_direct_to_properties(&context->galaxies[galaxy_idx]);
                }
                
                LOG_DEBUG("Executing GALAXY phase for module '%s', galaxy %d", module->name, context->current_galaxy);
                status = module->execute_galaxy_phase(module_data, context);
                
                // Sync back after module execution
                if (galaxy_idx >= 0 && galaxy_idx < context->ngal && context->galaxies != NULL) {
                    safe_sync_properties_to_direct(&context->galaxies[galaxy_idx]);
                }
            } else {
                 LOG_DEBUG("Module '%s' has no GALAXY phase implementation.", module->name);
            }
            break;

        case PIPELINE_PHASE_POST:
            if (module->execute_post_phase != NULL) {
                // For POST phase, sync the central galaxy before module execution
                int centralgal = context->centralgal;
                if (centralgal >= 0 && centralgal < context->ngal && context->galaxies != NULL) {
                    safe_sync_direct_to_properties(&context->galaxies[centralgal]);
                }
                
                LOG_DEBUG("Executing POST phase for module '%s'", module->name);
                status = module->execute_post_phase(module_data, context);
                
                // Sync back after module execution
                if (centralgal >= 0 && centralgal < context->ngal && context->galaxies != NULL) {
                    safe_sync_properties_to_direct(&context->galaxies[centralgal]);
                }
            } else {
                 LOG_DEBUG("Module '%s' has no POST phase implementation.", module->name);
            }
            break;

        case PIPELINE_PHASE_FINAL:
            if (module->execute_final_phase != NULL) {
                // For FINAL phase, sync the central galaxy before module execution
                int centralgal = context->centralgal;
                if (centralgal >= 0 && centralgal < context->ngal && context->galaxies != NULL) {
                    safe_sync_direct_to_properties(&context->galaxies[centralgal]);
                }
                
                LOG_DEBUG("Executing FINAL phase for module '%s'", module->name);
                status = module->execute_final_phase(module_data, context);
                
                // Sync back after module execution
                if (centralgal >= 0 && centralgal < context->ngal && context->galaxies != NULL) {
                    safe_sync_properties_to_direct(&context->galaxies[centralgal]);
                }
            } else {
                 LOG_DEBUG("Module '%s' has no FINAL phase implementation.", module->name);
            }
            break;

        default:
            LOG_ERROR("Invalid pipeline phase specified in context: %d", context->execution_phase);
            return -1; // Indicate an error for invalid phase
    }

    /* Check for errors reported by the module execution */
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Module '%s' (step '%s') failed during phase %d execution with status %d",
                  module->name, step->step_name, context->execution_phase, status);
        // Optionally propagate the error using module_set_error or similar
    }

    return status; // Return the status from the module's execution function
}

/**
 * Execute a function in a module with callback tracking
 *
 * This function wraps a function call with proper callback tracking for pipeline execution.
 * It handles the type conversion between module callbacks and pipeline execution functions.
 *
 * @param context Pipeline context
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @param function_name Name of the function being called
 * @param module_data Module-specific data
 * @param func Function to call with pipeline context
 * @return Result of the function call
 */
int pipeline_execute_with_callback(
    struct pipeline_context *context,
    int caller_id,
    int callee_id,
    const char *function_name,
    void *module_data,
    int (*func)(void *, struct pipeline_context *)
) {
    if (func == NULL || context == NULL) {
        LOG_ERROR("Null function pointer or context in pipeline_execute_with_callback");
        return -1;
    }

    /* Save current callback state */
    int prev_caller_id = context->caller_module_id;
    const char *prev_function = context->current_function;
    void *prev_context = context->callback_context;

    /* Set callback context for this execution */
    context->caller_module_id = caller_id;
    context->current_function = function_name;
    context->callback_context = module_data;

    /* Push to call stack */
    int status = module_call_stack_push(caller_id, callee_id, function_name, module_data);
    if (status != 0) {
        LOG_ERROR("Failed to push call stack frame: %d", status);

        /* Restore previous callback state */
        context->caller_module_id = prev_caller_id;
        context->current_function = prev_function;
        context->callback_context = prev_context;

        return status;
    }

    /* Execute the function with the module data and context */
    int result = func(module_data, context);

    /* Pop from call stack */
    module_call_stack_pop();

    /* Restore previous callback state */
    context->caller_module_id = prev_caller_id;
    context->current_function = prev_function;
    context->callback_context = prev_context;

    return result;
}

/* Initialize physics pipeline - Placeholder, real implementation might differ */
int physics_pipeline_initialize(void) {
    struct module_pipeline *pipeline = pipeline_get_global();
    if (pipeline == NULL) {
        LOG_ERROR("Global pipeline not initialized");
        return -1;
    }

    /* Add standard physics steps in the correct order */
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_REINCORPORATION, NULL, "reincorporation", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_STAR_FORMATION, NULL, "star_formation", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_FEEDBACK, NULL, "feedback", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_AGN, NULL, "agn", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_DISK_INSTABILITY, NULL, "disk_instability", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_MISC, NULL, "misc", true, true); // Example misc step

    LOG_INFO("Physics pipeline initialized with %d steps", pipeline->num_steps);
    return 0;
}