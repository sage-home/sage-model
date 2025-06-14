#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_allvars.h" // Include for GALAXY struct needed by phase functions
#include "core_logging.h"
#include "core_module_system.h"
#include "core_module_callback.h" // Needed for pipeline_execute_with_callback
#include "core_pipeline_system.h"
#include "physics_pipeline_executor.h"

/**
 * @file physics_pipeline_executor.c
 * @brief Physics-agnostic pipeline execution system
 *
 * This component implements the core-physics separation pattern, where the
 * core infrastructure has no knowledge of specific physics implementations.
 * Key design principles:
 *
 * 1. Core infrastructure depends only on module interfaces, not implementations
 * 2. Physics modules register themselves with the pipeline at initialization
 * 3. The pipeline executes phases without knowing module internals
 * 4. Property validation replaces direct field synchronization
 * 5. The core can run with a completely empty physics pipeline
 *
 * This design enables:
 * - Complete independence between core and physics
 * - Runtime modularity where physics components can be added/removed
 * - Simplified testing through minimal placeholder modules
 * - Future optimizations through alternative memory layouts
 */

/**
 * Check if a galaxy is valid for property access
 * with thorough safety checks to prevent segmentation faults
 */
static bool galaxy_is_valid_for_properties(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("galaxy_is_valid_for_properties: galaxy pointer is NULL");
        return false;
    }
    
    if (galaxy->properties == NULL) {
        /* Reduce noise - only log properties issues for first 5 galaxies */
        static int null_props_count = 0;
        null_props_count++;
        if (null_props_count <= 5) {
            if (null_props_count == 5) {
                LOG_DEBUG("galaxy_is_valid_for_properties: galaxy->properties pointer is NULL for GalaxyNr %d (issue #%d - further messages suppressed)", 
                          galaxy->GalaxyNr, null_props_count);
            } else {
                LOG_DEBUG("galaxy_is_valid_for_properties: galaxy->properties pointer is NULL for GalaxyNr %d (issue #%d)", 
                          galaxy->GalaxyNr, null_props_count);
            }
        }
        return false;
    }
    
    // Only consider galaxies that haven't merged or been disrupted
    if (GALAXY_PROP_merged(galaxy) > 0) {
        /* Reduce noise - only log merge status issues for first 5 galaxies */
        static int merge_status_count = 0;
        merge_status_count++;
        if (merge_status_count <= 5) {
            if (merge_status_count == 5) {
                LOG_DEBUG("Galaxy %d is not valid for property access (merged=%d) (issue #%d - further messages suppressed)", 
                        galaxy->GalaxyNr, GALAXY_PROP_merged(galaxy), merge_status_count);
            } else {
                LOG_DEBUG("Galaxy %d is not valid for property access (merged=%d) (issue #%d)", 
                        galaxy->GalaxyNr, GALAXY_PROP_merged(galaxy), merge_status_count);
            }
        }
        return false;
    }
    
    return true;
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
                // Verify the central galaxy has valid properties before execution
                int centralgal = context->centralgal;
                bool valid = false;
                if (centralgal >= 0 && centralgal < context->ngal && context->galaxies != NULL) {
                    valid = galaxy_is_valid_for_properties(&context->galaxies[centralgal]);
                }
                
                if (valid) {
                    /* Reduce noise - only log halo phase execution for first 5 calls */
                    static int halo_exec_count = 0;
                    halo_exec_count++;
                    if (halo_exec_count <= 5) {
                        if (halo_exec_count == 5) {
                            LOG_DEBUG("Executing HALO phase for module '%s' (execution #%d - further messages suppressed)", module->name, halo_exec_count);
                        } else {
                            LOG_DEBUG("Executing HALO phase for module '%s' (execution #%d)", module->name, halo_exec_count);
                        }
                    }
                    status = module->execute_halo_phase(module_data, context);
                } else {
                    LOG_WARNING("HALO phase skipped for module '%s': central galaxy properties not available", module->name);
                    status = 0; // Skip but don't fail
                }
            } else {
                LOG_DEBUG("Module '%s' has no HALO phase implementation.", module->name);
            }
            break;

        case PIPELINE_PHASE_GALAXY:
            if (module->execute_galaxy_phase != NULL) {
                // Verify the current galaxy has valid properties before execution
                int galaxy_idx = context->current_galaxy;
                bool valid = false;
                if (galaxy_idx >= 0 && galaxy_idx < context->ngal && context->galaxies != NULL) {
                    valid = galaxy_is_valid_for_properties(&context->galaxies[galaxy_idx]);
                }
                
                if (valid) {
                    /* Reduce noise - only log galaxy phase execution for first 5 galaxies */
                    static int galaxy_exec_count = 0;
                    galaxy_exec_count++;
                    if (galaxy_exec_count <= 5) {
                        if (galaxy_exec_count == 5) {
                            LOG_DEBUG("Executing GALAXY phase for module '%s', galaxy %d (execution #%d - further messages suppressed)", module->name, context->current_galaxy, galaxy_exec_count);
                        } else {
                            LOG_DEBUG("Executing GALAXY phase for module '%s', galaxy %d (execution #%d)", module->name, context->current_galaxy, galaxy_exec_count);
                        }
                    }
                    status = module->execute_galaxy_phase(module_data, context);
                } else {
                    /* Reduce noise - only log skipped galaxies for first 5 occurrences */
                    static int galaxy_skip_count = 0;
                    galaxy_skip_count++;
                    if (galaxy_skip_count <= 5) {
                        if (galaxy_skip_count == 5) {
                            LOG_DEBUG("GALAXY phase skipped for module '%s', galaxy %d: properties not available (skip #%d - further messages suppressed)", 
                                    module->name, context->current_galaxy, galaxy_skip_count);
                        } else {
                            LOG_DEBUG("GALAXY phase skipped for module '%s', galaxy %d: properties not available (skip #%d)", 
                                    module->name, context->current_galaxy, galaxy_skip_count);
                        }
                    }
                    status = 0; // Skip but don't fail
                }
            } else {
                LOG_DEBUG("Module '%s' has no GALAXY phase implementation.", module->name);
            }
            break;

        case PIPELINE_PHASE_POST:
            if (module->execute_post_phase != NULL) {
                /* Reduce noise - only log post phase execution for first 5 calls */
                static int post_exec_count = 0;
                post_exec_count++;
                if (post_exec_count <= 5) {
                    if (post_exec_count == 5) {
                        LOG_DEBUG("Executing POST phase for module '%s' (execution #%d - further messages suppressed)", module->name, post_exec_count);
                    } else {
                        LOG_DEBUG("Executing POST phase for module '%s' (execution #%d)", module->name, post_exec_count);
                    }
                }
                status = module->execute_post_phase(module_data, context);
            } else {
                LOG_DEBUG("Module '%s' has no POST phase implementation.", module->name);
            }
            break;

        case PIPELINE_PHASE_FINAL:
            if (module->execute_final_phase != NULL) {
                /* Reduce noise - only log final phase execution for first 5 calls */
                static int final_exec_count = 0;
                final_exec_count++;
                if (final_exec_count <= 5) {
                    if (final_exec_count == 5) {
                        LOG_DEBUG("Executing FINAL phase for module '%s' (execution #%d - further messages suppressed)", module->name, final_exec_count);
                    } else {
                        LOG_DEBUG("Executing FINAL phase for module '%s' (execution #%d)", module->name, final_exec_count);
                    }
                }
                status = module->execute_final_phase(module_data, context);
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
        const char *phase_name = "UNKNOWN";
        switch (context->execution_phase) {
            case PIPELINE_PHASE_NONE:   phase_name = "NONE"; break;
            case PIPELINE_PHASE_HALO:   phase_name = "HALO"; break;
            case PIPELINE_PHASE_GALAXY: phase_name = "GALAXY"; break;
            case PIPELINE_PHASE_POST:   phase_name = "POST"; break;
            case PIPELINE_PHASE_FINAL:  phase_name = "FINAL"; break;
        }
        
        if (context->execution_phase == PIPELINE_PHASE_GALAXY) {
            LOG_ERROR("Module '%s' (step '%s') failed during %s phase execution for galaxy %d (type %d) with status %d",
                    module->name, step->step_name, phase_name, 
                    context->current_galaxy,
                    (context->current_galaxy >= 0 && context->current_galaxy < context->ngal) ? 
                        context->galaxies[context->current_galaxy].Type : -1,
                    status);
        } else {
            LOG_ERROR("Module '%s' (step '%s') failed during %s phase execution with status %d",
                    module->name, step->step_name, phase_name, status);
        }
        
        // Check for module-specific error message
        if (module->last_error != 0) {
            LOG_ERROR("Module error: %s", module->error_message);
        }
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

/* Initialize a minimal physics-agnostic pipeline */
int physics_pipeline_initialize(void) {
    struct module_pipeline *pipeline = pipeline_get_global();
    if (pipeline == NULL) {
        LOG_ERROR("Global pipeline not initialized");
        return -1;
    }

    /* Initialize with an empty pipeline */
    /* Physics modules will register themselves during their initialization */
    /* This keeps the core completely physics-agnostic */
    
    LOG_INFO("Core physics-agnostic pipeline initialized successfully");
    return 0;
}