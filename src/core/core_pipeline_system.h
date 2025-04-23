#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "core_allvars.h"
#include "core_types.h"  /* For common type definitions */
#include "core_event_system.h"

/* Pipeline system constants */
#define MAX_PIPELINE_STEPS 32
#define MAX_STEP_NAME 64

/* Define pipeline constants */
#define MAX_PIPELINE_PHASES 16
#define MAX_PHASE_NAME 32

/* Pipeline phase definition */
struct pipeline_phase {
    char name[MAX_PHASE_NAME];
    uint32_t phase_id;
    int module_id;
};

/* Forward declarations */
struct base_module;
struct pipeline_context;
struct pipeline_step;
struct module_pipeline;

/**
 * @file core_pipeline_system.h
 * @brief Module pipeline system for SAGE
 * 
 * This file defines the pipeline infrastructure for SAGE's physics modules.
 * It provides a configurable execution pipeline that determines the sequence
 * of physics operations during galaxy evolution. The pipeline can be modified
 * at runtime, allowing modules to be inserted, replaced, reordered, or removed
 * without recompilation.
 */

/**
 * Pipeline step structure
 * 
 * Defines a single step in the physics pipeline
 */
struct pipeline_step {
    enum module_type type;               /* Type of module to execute */
    char module_name[MAX_MODULE_NAME];   /* Optional specific module name (empty for any) */
    char step_name[MAX_STEP_NAME];       /* Optional name for this step (for logging/config) */
    bool enabled;                        /* Whether this step is enabled */
    bool optional;                       /* Whether this step is optional (pipeline continues if missing) */
};

/**
 * Pipeline execution context
 * 
 * Contains runtime state for pipeline execution
 */
struct pipeline_context {
    struct params *params;               /* Global parameters */
    struct GALAXY *galaxies;             /* Galaxy array */
    int ngal;                            /* Number of galaxies */
    int centralgal;                      /* Index of central galaxy */
    double time;                         /* Current time */
    double dt;                           /* Time step */
    int halonr;                          /* Current halo number */
    int step;                            /* Current step number */
    void *user_data;                     /* Optional user data */
    int current_galaxy;                  /* Index of current galaxy being processed */
    double infall_gas;                   /* Result of infall calculation */
    double redshift;                     /* Current redshift */
    enum pipeline_execution_phase execution_phase; /* Current execution phase */

    // Add callback tracking
    int caller_module_id;              /* ID of module making callback */
    const char *current_function;      /* Name of function being called */
    void *callback_context;            /* Context data for current callback */
    
    // Property serialization context
    void *prop_ctx;                    /* Property serialization context */

    struct pipeline_phase phases[MAX_PIPELINE_PHASES];
    int num_phases;
    bool initialized;
};

/**
 * Physics module pipeline
 * 
 * Defines the sequence of physics operations during galaxy evolution
 */
struct module_pipeline {
    struct pipeline_step steps[MAX_PIPELINE_STEPS];  /* Pipeline steps */
    int num_steps;                        /* Number of steps in the pipeline */
    char name[MAX_MODULE_NAME];           /* Pipeline name */
    bool initialized;                     /* Whether pipeline is initialized */
    int current_step_index;               /* Current execution step (during execution) */
};

/**
 * Pipeline step execution callback type
 * 
 * Function signature for custom pipeline step execution
 */
typedef int (*pipeline_step_exec_fn)(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
);

/**
 * Pipeline event types
 */
enum pipeline_event_type {
    PIPELINE_EVENT_STARTED,              /* Pipeline execution started */
    PIPELINE_EVENT_STEP_BEFORE,          /* Before step execution */
    PIPELINE_EVENT_STEP_AFTER,           /* After step execution */
    PIPELINE_EVENT_STEP_ERROR,           /* Error during step execution */
    PIPELINE_EVENT_COMPLETED,            /* Pipeline execution completed */
    PIPELINE_EVENT_ABORTED               /* Pipeline execution aborted */
};

/**
 * Pipeline event data structure
 */
struct pipeline_event_data {
    enum pipeline_event_type type;       /* Event type */
    struct module_pipeline *pipeline;    /* Pipeline being executed */
    struct pipeline_step *step;          /* Current step (NULL for pipeline events) */
    struct pipeline_context *context;    /* Execution context */
    int step_index;                      /* Current step index */
    int status;                          /* Status code (for error events) */
};

/* Global default pipeline */
extern struct module_pipeline *global_pipeline;

/**
 * Initialize the pipeline system
 * 
 * Sets up the global pipeline and prepares it for execution.
 * 
 * @return 0 on success, error code on failure
 */
int pipeline_system_initialize(void);

/**
 * Clean up the pipeline system
 * 
 * Releases resources used by the pipeline system.
 * 
 * @return 0 on success, error code on failure
 */
int pipeline_system_cleanup(void);

/**
 * Create a new pipeline
 * 
 * Allocates and initializes a new module pipeline.
 * 
 * @param name Name for the new pipeline
 * @return Pointer to the new pipeline, or NULL on failure
 */
struct module_pipeline *pipeline_create(const char *name);

/**
 * Destroy a pipeline
 * 
 * Frees resources associated with a pipeline.
 * 
 * @param pipeline Pipeline to destroy
 */
void pipeline_destroy(struct module_pipeline *pipeline);

/**
 * Reset a pipeline
 * 
 * Clears all steps from a pipeline.
 * 
 * @param pipeline Pipeline to reset
 */
void pipeline_reset(struct module_pipeline *pipeline);

/**
 * Add a step to a pipeline
 * 
 * Appends a new step to the end of a pipeline.
 * 
 * @param pipeline Pipeline to modify
 * @param type Module type for the step
 * @param module_name Optional specific module name (can be NULL)
 * @param step_name Optional name for this step (can be NULL)
 * @param enabled Whether the step is initially enabled
 * @param optional Whether the step is optional
 * @return 0 on success, error code on failure
 */
int pipeline_add_step(
    struct module_pipeline *pipeline,
    enum module_type type,
    const char *module_name,
    const char *step_name,
    bool enabled,
    bool optional
);

/**
 * Insert a step into a pipeline
 * 
 * Inserts a new step at the specified position.
 * 
 * @param pipeline Pipeline to modify
 * @param index Position to insert the step (0-based)
 * @param type Module type for the step
 * @param module_name Optional specific module name (can be NULL)
 * @param step_name Optional name for this step (can be NULL)
 * @param enabled Whether the step is initially enabled
 * @param optional Whether the step is optional
 * @return 0 on success, error code on failure
 */
int pipeline_insert_step(
    struct module_pipeline *pipeline,
    int index,
    enum module_type type,
    const char *module_name,
    const char *step_name,
    bool enabled,
    bool optional
);

/**
 * Remove a step from a pipeline
 * 
 * Removes the step at the specified position.
 * 
 * @param pipeline Pipeline to modify
 * @param index Position of the step to remove (0-based)
 * @return 0 on success, error code on failure
 */
int pipeline_remove_step(struct module_pipeline *pipeline, int index);

/**
 * Move a step within a pipeline
 * 
 * Changes the position of a step within the pipeline.
 * 
 * @param pipeline Pipeline to modify
 * @param from_index Current position of the step (0-based)
 * @param to_index New position for the step (0-based)
 * @return 0 on success, error code on failure
 */
int pipeline_move_step(struct module_pipeline *pipeline, int from_index, int to_index);

/**
 * Enable or disable a pipeline step
 * 
 * Sets the enabled state of a step.
 * 
 * @param pipeline Pipeline to modify
 * @param index Position of the step to modify (0-based)
 * @param enabled New enabled state
 * @return 0 on success, error code on failure
 */
int pipeline_set_step_enabled(struct module_pipeline *pipeline, int index, bool enabled);

/**
 * Find step by name
 * 
 * Searches for a step with the given name.
 * 
 * @param pipeline Pipeline to search
 * @param step_name Name of the step to find
 * @return Index of the step if found, -1 if not found
 */
int pipeline_find_step_by_name(struct module_pipeline *pipeline, const char *step_name);

/**
 * Find steps by module type
 * 
 * Searches for all steps of a given module type.
 * 
 * @param pipeline Pipeline to search
 * @param type Module type to find
 * @param indices Array to store found step indices
 * @param max_indices Maximum number of indices to store
 * @return Number of steps found
 */
int pipeline_find_steps_by_type(
    struct module_pipeline *pipeline,
    enum module_type type,
    int *indices,
    int max_indices
);

/**
 * Validate a pipeline
 * 
 * Checks that a pipeline is correctly configured with valid steps.
 * 
 * @param pipeline Pipeline to validate
 * @return true if valid, false otherwise
 */
bool pipeline_validate(struct module_pipeline *pipeline);

/**
 * Initialize a pipeline context
 * 
 * Sets up a context for pipeline execution.
 * 
 * @param context Context to initialize
 * @param params Global parameters
 * @param galaxies Galaxy array
 * @param ngal Number of galaxies
 * @param centralgal Index of central galaxy
 * @param time Current time
 * @param dt Time step
 * @param halonr Current halo number
 * @param step Current step number
 * @param user_data Optional user data
 * 
 * Note: The redshift field in the context must be set separately.
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
);

/**
 * Execute a pipeline
 * 
 * Runs all enabled steps in a pipeline.
 * 
 * @param pipeline Pipeline to execute
 * @param context Execution context
 * @return 0 on success, error code on failure
 */
int pipeline_execute(struct module_pipeline *pipeline, struct pipeline_context *context);

/**
 * Execute a pipeline for a specific phase
 * 
 * Runs all enabled steps in a pipeline that support the given execution phase.
 * 
 * @param pipeline Pipeline to execute
 * @param context Execution context
 * @param phase Execution phase to run
 * @return 0 on success, error code on failure
 */
int pipeline_execute_phase(struct module_pipeline *pipeline, struct pipeline_context *context, enum pipeline_execution_phase phase);

/**
 * Execute a pipeline with a custom execution function
 * 
 * Runs all enabled steps using a custom execution function.
 * 
 * @param pipeline Pipeline to execute
 * @param context Execution context
 * @param exec_fn Custom execution function
 * @return 0 on success, error code on failure
 */
int pipeline_execute_custom(
    struct module_pipeline *pipeline,
    struct pipeline_context *context,
    pipeline_step_exec_fn exec_fn
);

/**
 * Create a default physics pipeline
 * 
 * Builds a pipeline with the standard physics steps.
 * 
 * @return Pointer to the new pipeline, or NULL on failure
 */
struct module_pipeline *pipeline_create_default(void);

/**
 * Clone a pipeline
 * 
 * Creates a copy of an existing pipeline.
 * 
 * @param source Pipeline to clone
 * @param new_name Name for the new pipeline (can be NULL to keep original)
 * @return Pointer to the new pipeline, or NULL on failure
 */
struct module_pipeline *pipeline_clone(struct module_pipeline *source, const char *new_name);

/**
 * Register pipeline event handlers
 * 
 * Sets up handlers for pipeline events.
 * 
 * @return 0 on success, error code on failure
 */
int pipeline_register_events(void);

/**
 * Get the global default pipeline
 * 
 * Retrieves the global pipeline instance.
 * 
 * @return Pointer to the global pipeline
 */
struct module_pipeline *pipeline_get_global(void);

/**
 * Set the global default pipeline
 * 
 * Changes the global pipeline instance.
 * 
 * @param pipeline New global pipeline
 * @return 0 on success, error code on failure
 */
int pipeline_set_global(struct module_pipeline *pipeline);

/**
 * Get module for pipeline step
 * 
 * Retrieves the module instance for a pipeline step.
 * 
 * @param step Pipeline step
 * @param module Output pointer to the module interface
 * @param module_data Output pointer to the module data
 * @return 0 on success, error code on failure
 */
int pipeline_get_step_module(
    struct pipeline_step *step,
    struct base_module **module,
    void **module_data
);

/**
 * Execute a function with pipeline callback tracking
 * 
 * Helper function that handles module callbacks within the pipeline context.
 * 
 * @param context Pipeline context
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @param function_name Name of the function being called
 * @param module_data Module-specific data
 * @param func Function to call
 * @return Result of the function call
 */
int pipeline_execute_with_callback(
    struct pipeline_context *context,
    int caller_id,
    int callee_id,
    const char *function_name,
    void *module_data,
    int (*func)(void *, struct pipeline_context *)
);

/**
 * Initialize property serialization for a pipeline
 * 
 * Sets up the property serialization context for pipeline execution.
 * 
 * @param context Pipeline context
 * @param flags Property serialization flags
 * @return 0 on success, error code on failure
 */
int pipeline_init_property_serialization(struct pipeline_context *context, uint32_t flags);

/**
 * Clean up property serialization for a pipeline
 * 
 * Releases resources used by property serialization.
 * 
 * @param context Pipeline context
 * @return 0 on success, error code on failure
 */
int pipeline_cleanup_property_serialization(struct pipeline_context *context);

#ifdef __cplusplus
}
#endif
