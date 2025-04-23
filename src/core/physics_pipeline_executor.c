#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_allvars.h"
#include "core_logging.h"
#include "core_module_system.h"
#include "core_module_callback.h" 
#include "core_pipeline_system.h"
#include "physics_pipeline_executor.h"
#include "../physics/physics_modules.h"

/* Forward declarations for legacy compatibility */
double infall_recipe_compat(int centralgal, struct GALAXY *galaxies, int ngal, struct params *params);
void cooling_recipe_compat(int p, double dt, struct GALAXY *galaxies, struct params *params);
void starformation_and_feedback_compat(int p, int centralgal, double time, double dt, int halonr, 
                                    int step, struct GALAXY *galaxies, struct params *params);
void check_disk_instability_compat(int p, int centralgal, int halonr, double time, double dt, 
                                 int step, struct GALAXY *galaxies, struct params *params);
void process_mergers_from_queue(struct merger_event_queue *queue, double time, double dt, 
                              int halonr, int step, struct GALAXY *galaxies, struct params *params);

/* Execute a physics module step */
int physics_step_executor(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
) {
    if (step == NULL || context == NULL) {
        LOG_ERROR("Invalid arguments to physics step executor");
        return -1;
    }

    /* Get module's physics interface */
    struct physics_module_interface *physics_module = (struct physics_module_interface *)module;
    
    /* Handle case where module is not loaded - use legacy implementation */
    if (physics_module == NULL) {
        switch (step->type) {
            case MODULE_TYPE_INFALL:
                if (context->execution_phase == PIPELINE_PHASE_HALO) {
                    /* Legacy infall calculation */
                    context->infall_gas = infall_recipe_compat(
                        context->centralgal, context->galaxies, context->ngal, context->params);
                    return 0;
                }
                return 0;

            case MODULE_TYPE_COOLING:
                if (context->execution_phase == PIPELINE_PHASE_GALAXY) {
                    /* Legacy cooling calculation */
                    cooling_recipe_compat(context->current_galaxy, context->dt,
                                       context->galaxies, context->params);
                    return 0;
                }
                return 0;

            case MODULE_TYPE_STAR_FORMATION:
                if (context->execution_phase == PIPELINE_PHASE_GALAXY) {
                    /* Legacy star formation */
                    starformation_and_feedback_compat(
                        context->current_galaxy, context->centralgal,
                        context->time, context->dt, context->halonr,
                        context->step, context->galaxies, context->params);
                    return 0;
                }
                return 0;

            case MODULE_TYPE_FEEDBACK:
                /* Handled by star formation in legacy code */
                return 0;

            case MODULE_TYPE_AGN:
                if (context->execution_phase == PIPELINE_PHASE_GALAXY &&
                    context->params->physics.AGNrecipeOn) {
                    /* Legacy AGN feedback */
                    // AGN feedback handled during mergers in legacy code
                    return 0;
                }
                return 0;

            case MODULE_TYPE_DISK_INSTABILITY:
                if (context->execution_phase == PIPELINE_PHASE_GALAXY &&
                    context->params->physics.DiskInstabilityOn) {
                    /* Legacy disk instability check */
                    check_disk_instability_compat(
                        context->current_galaxy, context->centralgal,
                        context->halonr, context->time, context->dt,
                        context->step, context->galaxies, context->params);
                    return 0;
                }
                return 0;

            case MODULE_TYPE_MERGERS:
                if (context->execution_phase == PIPELINE_PHASE_POST) {
                    /* Legacy merger handling */
                    process_mergers_from_queue(context->merger_queue,
                                            context->time, context->dt,
                                            context->halonr, context->step,
                                            context->galaxies, context->params);
                    return 0;
                }
                return 0;

            default:
                LOG_ERROR("Unknown module type %d", step->type);
                return -1;
        }
    }

    /* Execute appropriate phase handler */
    switch (context->execution_phase) {
        case PIPELINE_PHASE_HALO:
            if (physics_module->execute_halo_phase != NULL) {
                return physics_module->execute_halo_phase(module_data, context);
            }
            break;

        case PIPELINE_PHASE_GALAXY:
            if (physics_module->execute_galaxy_phase != NULL) {
                return physics_module->execute_galaxy_phase(module_data, context);
            }
            break;

        case PIPELINE_PHASE_POST:
            if (physics_module->execute_post_phase != NULL) {
                return physics_module->execute_post_phase(module_data, context);
            }
            break;

        case PIPELINE_PHASE_FINAL:
            if (physics_module->execute_final_phase != NULL) {
                return physics_module->execute_final_phase(module_data, context);
            }
            break;

        default:
            LOG_ERROR("Invalid pipeline phase: %d", context->execution_phase);
            return -1;
    }

    return 0;
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

/* Initialize physics pipeline */
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

    LOG_INFO("Physics pipeline initialized with %d steps", pipeline->num_steps);
    return 0;
}