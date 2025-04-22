#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_pipeline_system.h"
#include "core_module_system.h"
#include "core_event_system.h"
#include "core_logging.h"
#include "core_galaxy_extensions.h"
#include "../io/io_property_serialization.h"

/**
 * @file core_pipeline_system.c
 * @brief Implementation of the module pipeline system
 */

/* Global default pipeline */
struct module_pipeline *global_pipeline = NULL;

/* Pipeline event type name lookup */
static const char *pipeline_event_type_names[] = {
    "PIPELINE_STARTED",
    "PIPELINE_STEP_BEFORE",
    "PIPELINE_STEP_AFTER",
    "PIPELINE_STEP_ERROR",
    "PIPELINE_COMPLETED",
    "PIPELINE_ABORTED"
};

/* Event ID for pipeline events */
static int pipeline_event_id = -1;

/* Phase name lookup for logging */
static const char *pipeline_phase_names[] = {
    "HALO",
    "GALAXY",
    "POST",
    "FINAL"
};

/* Static flags to prevent flooding logs with the same message across multiple calls */
static bool s_logged_missing_module_warning = false;        // Renamed from already_logged_missing_modules
static bool s_logged_migration_fallback_warning = false;    // Renamed from first_warning

/**
 * Convert phase value to index
 *
 * Converts a phase bit flag value to an array index.
 * PIPELINE_PHASE_HALO (1) -> 0
 * PIPELINE_PHASE_GALAXY (2) -> 1
 * PIPELINE_PHASE_POST (4) -> 2
 * PIPELINE_PHASE_FINAL (8) -> 3
 */
static int get_phase_index(enum pipeline_execution_phase phase) {
    switch (phase) {
        case PIPELINE_PHASE_HALO:   return 0;
        case PIPELINE_PHASE_GALAXY: return 1;
        case PIPELINE_PHASE_POST:   return 2;
        case PIPELINE_PHASE_FINAL:  return 3;
        default: return 0;  // Default to HALO if invalid
    }
}

/**
 * Initialize the pipeline system
 */
int pipeline_system_initialize(void) {
    /* Register pipeline event type if events are enabled */
    if (event_system_is_initialized()) {
        pipeline_event_id = event_register_type("pipeline", sizeof(struct pipeline_event_data));
        if (pipeline_event_id < 0) {
            LOG_ERROR("Failed to register pipeline event type");
            return -1;
        }

        if (pipeline_register_events() != 0) {
            LOG_ERROR("Failed to register pipeline event handlers");
            return -1;
        }
    }

    /* Create default global pipeline */
    global_pipeline = pipeline_create_default();
    if (global_pipeline == NULL) {
        LOG_ERROR("Failed to create default pipeline");
        return -1;
    }

    LOG_INFO("Pipeline system initialized successfully");
    return 0;
}

/**
 * Clean up the pipeline system
 */
int pipeline_system_cleanup(void) {
    if (global_pipeline != NULL) {
        pipeline_destroy(global_pipeline);
        global_pipeline = NULL;
    }

    LOG_INFO("Pipeline system cleaned up");
    return 0;
}

/**
 * Create a new pipeline
 */
struct module_pipeline *pipeline_create(const char *name) {
    struct module_pipeline *pipeline = calloc(1, sizeof(struct module_pipeline));
    if (pipeline == NULL) {
        LOG_ERROR("Failed to allocate memory for pipeline");
        return NULL;
    }

    if (name != NULL) {
        strncpy(pipeline->name, name, MAX_MODULE_NAME - 1);
        pipeline->name[MAX_MODULE_NAME - 1] = '\0';
    } else {
        strcpy(pipeline->name, "unnamed_pipeline");
    }

    pipeline->num_steps = 0;
    pipeline->initialized = true;
    pipeline->current_step_index = -1;

    LOG_DEBUG("Created new pipeline '%s'", pipeline->name);
    return pipeline;
}

/**
 * Destroy a pipeline
 */
void pipeline_destroy(struct module_pipeline *pipeline) {
    if (pipeline == NULL) {
        return;
    }

    LOG_DEBUG("Destroying pipeline '%s'", pipeline->name);
    free(pipeline);
}

/**
 * Reset a pipeline
 */
void pipeline_reset(struct module_pipeline *pipeline) {
    if (pipeline == NULL) {
        return;
    }

    pipeline->num_steps = 0;
    pipeline->current_step_index = -1;

    LOG_DEBUG("Reset pipeline '%s'", pipeline->name);
}

/**
 * Add a step to a pipeline
 */
int pipeline_add_step(
    struct module_pipeline *pipeline,
    enum module_type type,
    const char *module_name,
    const char *step_name,
    bool enabled,
    bool optional
) {
    if (pipeline == NULL) {
        LOG_ERROR("Pipeline is NULL");
        return -1;
    }

    if (pipeline->num_steps >= MAX_PIPELINE_STEPS) {
        LOG_ERROR("Pipeline '%s' already has maximum number of steps", pipeline->name);
        return -1;
    }

    struct pipeline_step *step = &pipeline->steps[pipeline->num_steps];
    memset(step, 0, sizeof(struct pipeline_step));

    step->type = type;
    step->enabled = enabled;
    step->optional = optional;

    if (module_name != NULL) {
        strncpy(step->module_name, module_name, MAX_MODULE_NAME - 1);
        step->module_name[MAX_MODULE_NAME - 1] = '\0';
    }

    if (step_name != NULL) {
        strncpy(step->step_name, step_name, MAX_STEP_NAME - 1);
        step->step_name[MAX_STEP_NAME - 1] = '\0';
    } else {
        /* Generate default step name if none provided */
        snprintf(step->step_name, MAX_STEP_NAME, "%s_%d",
                 module_type_name(type), pipeline->num_steps);
    }

    pipeline->num_steps++;

    LOG_DEBUG("Added step '%s' (type %s, module '%s') to pipeline '%s'",
             step->step_name, module_type_name(type),
             module_name ? module_name : "any", pipeline->name);

    return 0;
}

/**
 * Insert a step into a pipeline
 */
int pipeline_insert_step(
    struct module_pipeline *pipeline,
    int index,
    enum module_type type,
    const char *module_name,
    const char *step_name,
    bool enabled,
    bool optional
) {
    if (pipeline == NULL) {
        LOG_ERROR("Pipeline is NULL");
        return -1;
    }

    if (pipeline->num_steps >= MAX_PIPELINE_STEPS) {
        LOG_ERROR("Pipeline '%s' already has maximum number of steps", pipeline->name);
        return -1;
    }

    if (index < 0 || index > pipeline->num_steps) {
        LOG_ERROR("Invalid step index %d (pipeline has %d steps)", index, pipeline->num_steps);
        return -1;
    }

    /* Shift steps to make room for the new one */
    for (int i = pipeline->num_steps; i > index; i--) {
        pipeline->steps[i] = pipeline->steps[i - 1];
    }

    struct pipeline_step *step = &pipeline->steps[index];
    memset(step, 0, sizeof(struct pipeline_step));

    step->type = type;
    step->enabled = enabled;
    step->optional = optional;

    if (module_name != NULL) {
        strncpy(step->module_name, module_name, MAX_MODULE_NAME - 1);
        step->module_name[MAX_MODULE_NAME - 1] = '\0';
    }

    if (step_name != NULL) {
        strncpy(step->step_name, step_name, MAX_STEP_NAME - 1);
        step->step_name[MAX_STEP_NAME - 1] = '\0';
    } else {
        /* Generate default step name if none provided */
        snprintf(step->step_name, MAX_STEP_NAME, "%s_%d",
                 module_type_name(type), index);
    }

    pipeline->num_steps++;

    LOG_DEBUG("Inserted step '%s' (type %s, module '%s') at index %d in pipeline '%s'",
             step->step_name, module_type_name(type),
             module_name ? module_name : "any", index, pipeline->name);

    return 0;
}

/**
 * Remove a step from a pipeline
 */
int pipeline_remove_step(struct module_pipeline *pipeline, int index) {
    if (pipeline == NULL) {
        LOG_ERROR("Pipeline is NULL");
        return -1;
    }

    if (index < 0 || index >= pipeline->num_steps) {
        LOG_ERROR("Invalid step index %d (pipeline has %d steps)", index, pipeline->num_steps);
        return -1;
    }

    LOG_DEBUG("Removing step '%s' at index %d from pipeline '%s'",
             pipeline->steps[index].step_name, index, pipeline->name);

    /* Shift steps to close the gap */
    for (int i = index; i < pipeline->num_steps - 1; i++) {
        pipeline->steps[i] = pipeline->steps[i + 1];
    }

    pipeline->num_steps--;

    return 0;
}

/**
 * Move a step within a pipeline
 */
int pipeline_move_step(struct module_pipeline *pipeline, int from_index, int to_index) {
    if (pipeline == NULL) {
        LOG_ERROR("Pipeline is NULL");
        return -1;
    }

    if (from_index < 0 || from_index >= pipeline->num_steps) {
        LOG_ERROR("Invalid source index %d (pipeline has %d steps)", from_index, pipeline->num_steps);
        return -1;
    }

    if (to_index < 0 || to_index >= pipeline->num_steps) {
        LOG_ERROR("Invalid destination index %d (pipeline has %d steps)", to_index, pipeline->num_steps);
        return -1;
    }

    if (from_index == to_index) {
        /* Nothing to do */
        return 0;
    }

    /* Save the step being moved */
    struct pipeline_step temp = pipeline->steps[from_index];

    if (from_index < to_index) {
        /* Moving forward: shift steps backward */
        for (int i = from_index; i < to_index; i++) {
            pipeline->steps[i] = pipeline->steps[i + 1];
        }
    } else {
        /* Moving backward: shift steps forward */
        for (int i = from_index; i > to_index; i--) {
            pipeline->steps[i] = pipeline->steps[i - 1];
        }
    }

    /* Place the moved step at the destination */
    pipeline->steps[to_index] = temp;

    LOG_DEBUG("Moved step '%s' from index %d to %d in pipeline '%s'",
             temp.step_name, from_index, to_index, pipeline->name);

    return 0;
}

/**
 * Enable or disable a pipeline step
 */
int pipeline_set_step_enabled(struct module_pipeline *pipeline, int index, bool enabled) {
    if (pipeline == NULL) {
        LOG_ERROR("Pipeline is NULL");
        return -1;
    }

    if (index < 0 || index >= pipeline->num_steps) {
        LOG_ERROR("Invalid step index %d (pipeline has %d steps)", index, pipeline->num_steps);
        return -1;
    }

    pipeline->steps[index].enabled = enabled;

    LOG_DEBUG("%s step '%s' at index %d in pipeline '%s'",
             enabled ? "Enabled" : "Disabled",
             pipeline->steps[index].step_name, index, pipeline->name);

    return 0;
}

/**
 * Find step by name
 */
int pipeline_find_step_by_name(struct module_pipeline *pipeline, const char *step_name) {
    if (pipeline == NULL || step_name == NULL) {
        return -1;
    }

    for (int i = 0; i < pipeline->num_steps; i++) {
        if (strcmp(pipeline->steps[i].step_name, step_name) == 0) {
            return i;
        }
    }

    return -1;
}

/**
 * Find steps by module type
 */
int pipeline_find_steps_by_type(
    struct module_pipeline *pipeline,
    enum module_type type,
    int *indices,
    int max_indices
) {
    if (pipeline == NULL || indices == NULL || max_indices <= 0) {
        return 0;
    }

    int count = 0;

    for (int i = 0; i < pipeline->num_steps && count < max_indices; i++) {
        if (pipeline->steps[i].type == type) {
            indices[count++] = i;
        }
    }

    return count;
}

/**
 * Validate a pipeline
 */
bool pipeline_validate(struct module_pipeline *pipeline) {
    if (pipeline == NULL) {
        LOG_ERROR("Pipeline is NULL");
        return false;
    }

    if (pipeline->num_steps == 0) {
        LOG_WARNING("Pipeline '%s' has no steps", pipeline->name);
        /* Empty pipeline is technically valid, just not useful */
        return true;
    }

    /* Check for valid modules for each step */
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];

        /* Skip disabled steps */
        if (!step->enabled) {
            continue;
        }

        /* Check if step module exists */
        struct base_module *module;
        void *module_data;

        int status = pipeline_get_step_module(step, &module, &module_data);
        if (status != 0) {
            /* Check if the step is optional */
            if (!step->optional) {
                /* Only log the first time this specific warning occurs per run */
                if (!s_logged_missing_module_warning) {
                    LOG_WARNING("Required module not found for step '%s' (type %s). Pipeline might not function correctly.",
                               step->step_name, module_type_name(step->type));
                    s_logged_missing_module_warning = true;
                } else {
                    LOG_DEBUG("Required module not found for step '%s' (type %s).",
                               step->step_name, module_type_name(step->type));
                }
            } else {
                LOG_DEBUG("Optional module not found for step '%s' (type %s).",
                         step->step_name, module_type_name(step->type));
            }
            continue;
        }

        /* Validate module extensions if it has them */
        if (module != NULL && module->manifest && (module->manifest->capabilities & MODULE_FLAG_HAS_EXTENSIONS)) {
            /* This module uses extensions, ensure serialization is supported */
            if (module->manifest->capabilities & MODULE_FLAG_REQUIRES_SERIALIZATION) {
                /* Check if module has required serialization functions */
                const galaxy_property_t **properties = NULL;
                int max_properties = 32;  // Reasonable limit for temp array
                properties = calloc(max_properties, sizeof(galaxy_property_t*));
                
                if (properties != NULL) {
                    int num_props = galaxy_extension_find_properties_by_module(
                        module->module_id, properties, max_properties);
                    
                    for (int j = 0; j < num_props; j++) {
                        const galaxy_property_t *prop = properties[j];
                        if ((prop->flags & PROPERTY_FLAG_SERIALIZE) && 
                            (prop->serialize == NULL || prop->deserialize == NULL)) {
                            LOG_ERROR("Module '%s' property '%s' marked for serialization but missing required functions",
                                     module->name, prop->name);
                            free(properties);
                            return false;
                        }
                    }
                    free(properties);
                }
            }
        }
    }

    /* We've already validated extensions in the loop above, no need to check again */

    return true;
}

/**
 * Initialize a pipeline context
 */
void pipeline_context_init(
    struct pipeline_context *context,
    struct params *params,
    struct GALAXY *galaxies,
    int ngal,
    int centralgal,
    double time,
    double dt,
    int halonr,
    int step,
    void *user_data
) {
    if (context == NULL) {
        return;
    }

    context->params = params;
    context->galaxies = galaxies;
    context->ngal = ngal;
    context->centralgal = centralgal;
    context->time = time;
    context->dt = dt;
    context->halonr = halonr;
    context->step = step;
    context->user_data = user_data;
    context->redshift = 0.0;  // Initialize to default value
    context->execution_phase = 0;  // Initialize phase
    context->current_galaxy = -1;  // Initialize to invalid index
    context->infall_gas = 0.0;    // Initialize infall result
    
    // Initialize property serialization context
    context->prop_ctx = NULL;  // Will be initialized when needed by I/O handlers
}

/**
 * Initialize property serialization context for pipeline
 */
int pipeline_init_property_serialization(
    struct pipeline_context *context,
    uint32_t filter_flags
) {
    if (context == NULL) {
        LOG_ERROR("Pipeline context is NULL");
        return -1;
    }

    // Clean up existing context if any
    pipeline_cleanup_property_serialization(context);

    // Allocate new context
    context->prop_ctx = calloc(1, sizeof(struct property_serialization_context));
    if (context->prop_ctx == NULL) {
        LOG_ERROR("Failed to allocate property serialization context");
        return -1;
    }

    // Initialize context
    int status = property_serialization_init(context->prop_ctx, filter_flags);
    if (status != 0) {
        LOG_ERROR("Failed to initialize property serialization context");
        free(context->prop_ctx);
        context->prop_ctx = NULL;
        return status;
    }

    // Add properties from registry
    status = property_serialization_add_properties(context->prop_ctx);
    if (status != 0) {
        LOG_ERROR("Failed to add properties to serialization context");
        property_serialization_cleanup(context->prop_ctx);
        free(context->prop_ctx);
        context->prop_ctx = NULL;
        return status;
    }

    LOG_DEBUG("Initialized property serialization context with %d properties",
             context->prop_ctx->num_properties);
    return 0;
}

/**
 * Clean up property serialization context
 */
void pipeline_cleanup_property_serialization(struct pipeline_context *context) {
    if (context == NULL || context->prop_ctx == NULL) {
        return;
    }

    property_serialization_cleanup(context->prop_ctx);
    free(context->prop_ctx);
    context->prop_ctx = NULL;
}

/**
 * Emit pipeline event
 */
static void pipeline_emit_event(
    enum pipeline_event_type type,
    struct module_pipeline *pipeline,
    struct pipeline_step *step,
    struct pipeline_context *context,
    int step_index,
    int status
) {
    if (!event_system_is_initialized() || pipeline_event_id < 0) {
        return;
    }

    struct pipeline_event_data event_data;
    event_data.type = type;
    event_data.pipeline = pipeline;
    event_data.step = step;
    event_data.context = context;
    event_data.step_index = step_index;
    event_data.status = status;

    event_emit(
        pipeline_event_id,
        -1,  /* source_module_id */
        -1,  /* galaxy_index */
        -1,  /* step */
        &event_data,
        sizeof(event_data),
        0    /* flags */
    );
}

/**
 * Default pipeline step execution function (placeholder)
 */
static int pipeline_execute_step(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
) {
    /* Silence unused parameter warnings */
    (void)module;
    (void)module_data;
    (void)context;

    /* This is a placeholder and should be replaced by a custom executor */
    /* We log this error only once per module type to avoid flooding logs */
    static bool logged_error_types[MODULE_TYPE_MAX] = {false};
    static int error_log_count = 0;

    if (!logged_error_types[step->type]) {
        if (error_log_count < 5) { // Log first 5 distinct type errors
            LOG_ERROR("Placeholder pipeline_execute_step called. No implementation for module type '%s' (step '%s').",
                    module_type_name(step->type), step->step_name);
            error_log_count++;
        } else if (error_log_count == 5) {
            LOG_ERROR("Further placeholder execution errors will be suppressed.");
            error_log_count++;
        }
        logged_error_types[step->type] = true;
    } else {
        LOG_DEBUG("Repeated placeholder execution for module type '%s'.", module_type_name(step->type));
    }

    return -1; // Indicate error as this shouldn't normally be called
}

/**
 * Execute a pipeline using the default executor
 */
int pipeline_execute(struct module_pipeline *pipeline, struct pipeline_context *context) {
    return pipeline_execute_custom(pipeline, context, pipeline_execute_step);
}

/**
 * Execute a pipeline for a specific phase
 *
 * Runs all enabled steps in a pipeline that support the given execution phase.
 * Uses the provided custom execution function.
 *
 * @param pipeline Pipeline to execute
 * @param context Execution context
 * @param phase Execution phase to run
 * @param exec_fn Custom execution function
 * @return 0 on success, error code on failure
 */
static int pipeline_execute_phase_with_executor(
    struct module_pipeline *pipeline,
    struct pipeline_context *context,
    enum pipeline_execution_phase phase,
    pipeline_step_exec_fn exec_fn
) {
    if (pipeline == NULL || context == NULL || exec_fn == NULL) {
        LOG_ERROR("Invalid arguments for pipeline phase execution");
        return -1;
    }

    if (!pipeline->initialized) {
        LOG_ERROR("Attempt to execute uninitialized pipeline '%s'", pipeline->name);
        return -1;
    }

    /* Update context with the current phase */
    context->execution_phase = phase;

    LOG_DEBUG("Executing pipeline '%s' phase %s with %d steps",
             pipeline->name, pipeline_phase_names[get_phase_index(phase)], pipeline->num_steps);

    /* Check if any enabled step requires property serialization and initialize if needed */
    bool needs_property_serialization = false;
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];
        if (!step->enabled) continue;

        struct base_module *module;
        void *module_data;
        if (pipeline_get_step_module(step, &module, &module_data) == 0 && 
            module != NULL && module->manifest && 
            (module->manifest->capabilities & MODULE_FLAG_REQUIRES_SERIALIZATION)) {
            needs_property_serialization = true;
            break;
        }
    }

    /* Initialize property serialization context if needed */
    if (needs_property_serialization && context->prop_ctx == NULL) {
        LOG_DEBUG("Initializing property serialization for pipeline execution");
        /* Use the explicit flag from io_property_serialization.h to only serialize properties marked for serialization */
        int status = pipeline_init_property_serialization(context, SERIALIZE_EXPLICIT);
        if (status != 0) {
            LOG_ERROR("Failed to initialize property serialization context for pipeline execution");
            return status;
        }
    }

    /* Emit pipeline started event */
    pipeline_emit_event(PIPELINE_EVENT_STARTED, pipeline, NULL, context, -1, 0);

    int result = 0;
    // Use local flags to suppress repeated errors *within this phase execution*
    bool errors_logged_this_phase[MODULE_TYPE_MAX] = {false};

    /* Execute each enabled step */
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];

        /* Skip disabled steps */
        if (!step->enabled) {
            LOG_DEBUG("Skipping disabled step '%s'", step->step_name);
            continue;
        }

        /* Get the module for this step */
        struct base_module *module;
        void *module_data;

        int status = pipeline_get_step_module(step, &module, &module_data);
        if (status != 0) {
            /* TEMPORARY MIGRATION CODE: Fall back to original physics implementation */
            if (!s_logged_migration_fallback_warning) {
                LOG_WARNING("Migration: Using original implementation for step '%s' (module missing)",
                           step->step_name);
                s_logged_migration_fallback_warning = true; // Log only once globally
            } else {
                LOG_DEBUG("Migration: Using original implementation for step '%s' (module missing)",
                         step->step_name);
            }
            module = NULL;
            module_data = NULL;
            /* Continue execution with NULL module - executor must handle this */
        }

        /* Skip modules that don't support this phase (only if a module exists) */
        if (module != NULL && !(module->phases & phase)) {
            LOG_DEBUG("Skipping step '%s' as module '%s' does not support phase %d",
                     step->step_name, module->name, phase);
            continue;
        }

        LOG_DEBUG("Executing step '%s' with module '%s' in phase %d",
                 step->step_name, module ? module->name : "original", phase);

        /* Track current step for error handling */
        pipeline->current_step_index = i;

        /* Emit step before event */
        pipeline_emit_event(PIPELINE_EVENT_STEP_BEFORE, pipeline, step, context, i, 0);

        /* Execute the step using the custom executor */
        status = exec_fn(step, module, module_data, context);

        if (status != 0) {
            /* Log only the first error per module type within this phase call */
            if (!errors_logged_this_phase[step->type]) {
                LOG_ERROR("Step '%s' (type: %s) failed with status %d during phase %d",
                          step->step_name, module_type_name(step->type), status, phase);
                errors_logged_this_phase[step->type] = true;
            } else {
                LOG_DEBUG("Suppressed repeated error for step '%s' (type: %s), status: %d",
                          step->step_name, module_type_name(step->type), status);
            }

            /* Emit step error event */
            pipeline_emit_event(PIPELINE_EVENT_STEP_ERROR, pipeline, step, context, i, status);

            /* Abort pipeline if step is not optional */
            if (!step->optional) {
                result = status;
                break; // Exit the loop over steps
            }
        }

        /* Emit step after event */
        pipeline_emit_event(PIPELINE_EVENT_STEP_AFTER, pipeline, step, context, i, status);
    } // End of loop over steps

    /* Reset current step */
    pipeline->current_step_index = -1;

    /* Emit pipeline completed or aborted event */
    if (result == 0) {
        pipeline_emit_event(PIPELINE_EVENT_COMPLETED, pipeline, NULL, context, -1, 0);
        LOG_DEBUG("Pipeline '%s' phase %d completed successfully",
                 pipeline->name, phase);
    } else {
        pipeline_emit_event(PIPELINE_EVENT_ABORTED, pipeline, NULL, context, -1, result);
        LOG_ERROR("Pipeline '%s' phase %d aborted with status %d",
                 pipeline->name, phase, result);
    }

    /* Clean up property serialization if we initialized it for this execution */
    if (needs_property_serialization) {
        pipeline_cleanup_property_serialization(context);
        LOG_DEBUG("Cleaned up property serialization context");
    }

    return result;
}

/**
 * Execute a pipeline for a specific phase using the default executor
 */
int pipeline_execute_phase(struct module_pipeline *pipeline, struct pipeline_context *context, enum pipeline_execution_phase phase) {
    /* This function is defined in core_build_model.c and must be passed explicitly */
    /* Here we use the placeholder as a default, but in the real code, the caller
       (evolve_galaxies) passes its custom executor. */
    extern int physics_step_executor(struct pipeline_step *, struct base_module *, void *, struct pipeline_context *);
    return pipeline_execute_phase_with_executor(pipeline, context, phase, physics_step_executor);
}

/**
 * Execute a pipeline with a custom execution function
 */
int pipeline_execute_custom(
    struct module_pipeline *pipeline,
    struct pipeline_context *context,
    pipeline_step_exec_fn exec_fn
) {
    if (pipeline == NULL || context == NULL || exec_fn == NULL) {
        LOG_ERROR("Invalid arguments for pipeline execution");
        return -1;
    }

    if (!pipeline->initialized) {
        LOG_ERROR("Attempt to execute uninitialized pipeline '%s'", pipeline->name);
        return -1;
    }

    if (pipeline->num_steps == 0) {
        LOG_WARNING("Executing empty pipeline '%s'", pipeline->name);
        return 0;
    }

    LOG_DEBUG("Executing pipeline '%s' with %d steps using custom executor", pipeline->name, pipeline->num_steps);

    /* Emit pipeline started event */
    pipeline_emit_event(PIPELINE_EVENT_STARTED, pipeline, NULL, context, -1, 0);

    int result = 0;
    bool errors_logged_this_call[MODULE_TYPE_MAX] = {false}; // Track logged errors per type for this call

    /* Execute each enabled step */
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];

        /* Skip disabled steps */
        if (!step->enabled) {
            LOG_DEBUG("Skipping disabled step '%s'", step->step_name);
            continue;
        }

        /* Get the module for this step */
        struct base_module *module;
        void *module_data;

        int status = pipeline_get_step_module(step, &module, &module_data);
        if (status != 0) {
            // TEMPORARY MIGRATION CODE: Fall back to original physics implementation
            if (!s_logged_migration_fallback_warning) {
                LOG_WARNING("Migration: Using original implementation for step '%s' (module missing)",
                           step->step_name);
                s_logged_migration_fallback_warning = true; // Log only once globally
            } else {
                LOG_DEBUG("Migration: Using original implementation for step '%s' (module missing)",
                         step->step_name);
            }
            module = NULL;
            module_data = NULL;
            /* Continue execution with NULL module - executor must handle this */
        }

        LOG_DEBUG("Executing step '%s' with module '%s'",
                 step->step_name, module ? module->name : "original");

        /* Track current step for error handling */
        pipeline->current_step_index = i;

        /* Emit step before event */
        pipeline_emit_event(PIPELINE_EVENT_STEP_BEFORE, pipeline, step, context, i, 0);

        /* Execute the step */
        status = exec_fn(step, module, module_data, context);

        if (status != 0) {
            /* Log only the first error per module type within this call */
            if (!errors_logged_this_call[step->type]) {
                LOG_ERROR("Step '%s' (type: %s) failed with status %d",
                          step->step_name, module_type_name(step->type), status);
                errors_logged_this_call[step->type] = true;
            } else {
                 LOG_DEBUG("Suppressed repeated error for step '%s' (type: %s), status: %d",
                           step->step_name, module_type_name(step->type), status);
            }

            /* Emit step error event */
            pipeline_emit_event(PIPELINE_EVENT_STEP_ERROR, pipeline, step, context, i, status);

            /* Abort pipeline if step is not optional */
            if (!step->optional) {
                result = status;
                break; // Exit the loop over steps
            }
        }

        /* Emit step after event */
        pipeline_emit_event(PIPELINE_EVENT_STEP_AFTER, pipeline, step, context, i, status);
    }

    /* Reset current step */
    pipeline->current_step_index = -1;

    /* Emit pipeline completed or aborted event */
    if (result == 0) {
        pipeline_emit_event(PIPELINE_EVENT_COMPLETED, pipeline, NULL, context, -1, 0);
        LOG_DEBUG("Pipeline '%s' completed successfully", pipeline->name);
    } else {
        pipeline_emit_event(PIPELINE_EVENT_ABORTED, pipeline, NULL, context, -1, result);
        LOG_ERROR("Pipeline '%s' aborted with status %d", pipeline->name, result);
    }

    return result;
}

/**
 * Create a default physics pipeline
 */
struct module_pipeline *pipeline_create_default(void) {
    struct module_pipeline *pipeline = pipeline_create("default");
    if (pipeline == NULL) {
        return NULL;
    }

    /* Add standard physics steps in the canonical order */
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_REINCORPORATION, NULL, "reincorporation", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_STAR_FORMATION, NULL, "star_formation", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_FEEDBACK, NULL, "feedback", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_AGN, NULL, "agn", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_DISK_INSTABILITY, NULL, "disk_instability", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_MISC, NULL, "misc", true, true);

    LOG_DEBUG("Created default physics pipeline with %d steps", pipeline->num_steps);

    return pipeline;
}

/**
 * Clone a pipeline
 */
struct module_pipeline *pipeline_clone(struct module_pipeline *source, const char *new_name) {
    if (source == NULL) {
        LOG_ERROR("Source pipeline is NULL");
        return NULL;
    }

    const char *name = new_name != NULL ? new_name : source->name;
    struct module_pipeline *pipeline = pipeline_create(name);
    if (pipeline == NULL) {
        return NULL;
    }

    /* Copy all steps */
    for (int i = 0; i < source->num_steps; i++) {
        struct pipeline_step *step = &source->steps[i];
        pipeline_add_step(pipeline, step->type, step->module_name,
                         step->step_name, step->enabled, step->optional);
    }

    LOG_DEBUG("Cloned pipeline '%s' to '%s' with %d steps",
             source->name, pipeline->name, pipeline->num_steps);

    return pipeline;
}

/**
 * Pipeline event handler
 */
static bool pipeline_event_handler(const struct event *event, void *user_data) {
    (void)user_data; /* Suppress unused parameter warning */

    struct pipeline_event_data *data = (struct pipeline_event_data *)(event->data);

    /* Log the event at debug level */
    const char *event_name = pipeline_event_type_names[data->type];

    if (data->type == PIPELINE_EVENT_STEP_BEFORE ||
        data->type == PIPELINE_EVENT_STEP_AFTER ||
        data->type == PIPELINE_EVENT_STEP_ERROR) {
        LOG_DEBUG("Pipeline event: %s - Step '%s' (index %d)",
                 event_name, data->step->step_name, data->step_index);
    } else {
        LOG_DEBUG("Pipeline event: %s - Pipeline '%s'",
                 event_name, data->pipeline->name);
    }

    /* Return true to continue event processing */
    return true;
}

/**
 * Register pipeline event handlers
 */
int pipeline_register_events(void) {
    if (!event_system_is_initialized() || pipeline_event_id < 0) {
        return -1;
    }

    /* Register a handler for all pipeline events */
    int status = event_register_handler(
        pipeline_event_id,
        pipeline_event_handler,
        NULL,
        -1,  /* module_id (system handler) */
        "pipeline_event_handler",
        EVENT_PRIORITY_NORMAL
    );

    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register pipeline event handler");
        return -1;
    }

    LOG_DEBUG("Registered pipeline event handler");

    return 0;
}

/**
 * Get the global default pipeline
 */
struct module_pipeline *pipeline_get_global(void) {
    return global_pipeline;
}

/**
 * Set the global default pipeline
 */
int pipeline_set_global(struct module_pipeline *pipeline) {
    if (pipeline == NULL) {
        LOG_ERROR("Cannot set NULL as global pipeline");
        return -1;
    }

    global_pipeline = pipeline;
    LOG_DEBUG("Set global pipeline to '%s'", pipeline->name);

    return 0;
}

/**
 * Get module for pipeline step
 */
int pipeline_get_step_module(
    struct pipeline_step *step,
    struct base_module **module,
    void **module_data
) {
    if (step == NULL || module == NULL || module_data == NULL) {
        return -1;
    }

    /* If a specific module name is specified, try to find that module */
    if (step->module_name[0] != '\0') {
        int module_id = module_find_by_name(step->module_name);
        if (module_id < 0) {
            LOG_DEBUG("Named module '%s' not found for step '%s'",
                     step->module_name, step->step_name);
            return -1;
        }

        return module_get(module_id, module, module_data);
    }

    /* Otherwise, get the active module of the specified type */
    return module_get_active_by_type(step->type, module, module_data);
}