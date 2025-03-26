#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_pipeline_system.h"
#include "core_module_system.h"
#include "core_event_system.h"
#include "core_logging.h"
#include "core_logging.h"

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
    
    /* During Phase 2.5-2.6, we're less verbose about missing modules */
    static bool already_logged_missing_modules = false;
    
    /* Track if any required modules are missing */
    bool all_required_modules_present = true;
    
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
            if (!step->optional) {
                /* Only required modules affect validation */
                all_required_modules_present = false;
                
                /* Only log once per process to avoid spamming */
                if (!already_logged_missing_modules) {
                    LOG_ERROR("Required step '%s' (type %s) in pipeline '%s' references missing module '%s'",
                             step->step_name, module_type_name(step->type), 
                             pipeline->name, step->module_name[0] ? step->module_name : "any");
                }
            } else {
                /* For optional modules, just log at debug level */
                LOG_DEBUG("Optional step '%s' (type %s) is missing a module, but will be skipped during execution",
                         step->step_name, module_type_name(step->type));
            }
        }
    }
    
    /* During Phase 2.5-2.6, not all modules are expected to be present */
    if (!all_required_modules_present && !already_logged_missing_modules) {
        LOG_WARNING("Some required modules are missing in Phase 2.5-2.6. Consider marking all steps as optional.");
    }
    
    /* Remember that we've already logged to avoid repetition */
    already_logged_missing_modules = true;
    
    /* In Phase 2.5-2.6, we'll validate the pipeline even if modules are missing */
    /* This allows evolution to proceed with the traditional implementation */
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
 * Execute a single pipeline step
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
    
    /* This function should be overridden in module-specific code */
    LOG_ERROR("Default pipeline_execute_step called with no implementation for module type %s",
             module_type_name(step->type));
    return -1;
}

/**
 * Execute a pipeline
 */
int pipeline_execute(struct module_pipeline *pipeline, struct pipeline_context *context) {
    return pipeline_execute_custom(pipeline, context, pipeline_execute_step);
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
    
    LOG_DEBUG("Executing pipeline '%s' with %d steps", pipeline->name, pipeline->num_steps);
    
    /* Emit pipeline started event */
    pipeline_emit_event(PIPELINE_EVENT_STARTED, pipeline, NULL, context, -1, 0);
    
    int result = 0;
    
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
            if (step->optional) {
                // Use LOG_DEBUG for optional steps to reduce log spam
                // Only output the first time in each run to avoid overwhelming the logs
                static bool first_warning = true;
                if (first_warning) {
                    LOG_WARNING("Skipping optional step '%s' due to missing module", step->step_name);
                    // After first few warnings, only log as debug
                    if (i > 2) {
                        first_warning = false;
                        LOG_INFO("Further optional step skipping warnings will be at debug level only");
                    }
                } else {
                    LOG_DEBUG("Skipping optional step '%s' due to missing module", step->step_name);
                }
                continue;
            } else {
                LOG_ERROR("Required module missing for step '%s'", step->step_name);
                result = -1;
                break;
            }
        }
        
        LOG_DEBUG("Executing step '%s' with module '%s'", 
                 step->step_name, module->name);
        
        /* Track current step for error handling */
        pipeline->current_step_index = i;
        
        /* Emit step before event */
        pipeline_emit_event(PIPELINE_EVENT_STEP_BEFORE, pipeline, step, context, i, 0);
        
        /* Execute the step */
        status = exec_fn(step, module, module_data, context);
        
        if (status != 0) {
            LOG_ERROR("Step '%s' failed with status %d", step->step_name, status);
            
            /* Emit step error event */
            pipeline_emit_event(PIPELINE_EVENT_STEP_ERROR, pipeline, step, context, i, status);
            
            /* Abort pipeline if step is not optional */
            if (!step->optional) {
                result = status;
                break;
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
    /* In Phase 2.5-2.6, mark all physics steps as optional since only cooling is implemented as a module */
    /* NOTE: Once all modules are migrated, non-essential modules will remain optional but core ones will become required */
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_STAR_FORMATION, NULL, "star_formation", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_FEEDBACK, NULL, "feedback", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_AGN, NULL, "agn", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_DISK_INSTABILITY, NULL, "disk_instability", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers", true, true);
    pipeline_add_step(pipeline, MODULE_TYPE_REINCORPORATION, NULL, "reincorporation", true, true);
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
    int handler_id = event_register_handler(
        pipeline_event_id,
        pipeline_event_handler,
        NULL,
        -1,  /* module_id */
        "pipeline_event_handler",
        EVENT_PRIORITY_NORMAL
    );
    
    if (handler_id < 0) {
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
