/**
 * @file placeholder_empty_module.c
 * @brief A minimal empty module for validating pipeline functionality
 *
 * This module provides a skeleton implementation that does nothing except
 * register itself with the pipeline. It's used to verify that the core
 * infrastructure can run with minimal physics modules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../core/core_allvars.h"
#include "../core/core_module_system.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_logging.h"
#include "physics_modules.h"

/* Module data structure */
struct placeholder_module_data {
    int initialized;
};

/* Function prototypes */
static int placeholder_init(struct params *params, void **data_ptr);
static int placeholder_cleanup(void *data);
static int placeholder_execute_halo_phase(void *data, struct pipeline_context *context);
static int placeholder_execute_galaxy_phase(void *data, struct pipeline_context *context);
static int placeholder_execute_post_phase(void *data, struct pipeline_context *context);
static int placeholder_execute_final_phase(void *data, struct pipeline_context *context);

/**
 * @brief Initialize the placeholder module
 */
static int placeholder_init(struct params *params, void **data_ptr) {
    /* Allocate module data */
    struct placeholder_module_data *data = calloc(1, sizeof(struct placeholder_module_data));
    if (data == NULL) {
        LOG_ERROR("Failed to allocate memory for placeholder module data");
        return MODULE_STATUS_ERROR;
    }
    
    /* Initialize module data */
    data->initialized = 1;
    
    /* Set data pointer */
    *data_ptr = data;
    
    LOG_INFO("Placeholder empty module initialized");
    return MODULE_STATUS_SUCCESS;
}

/**
 * @brief Clean up the placeholder module
 */
static int placeholder_cleanup(void *data) {
    if (data != NULL) {
        free(data);
    }
    
    LOG_INFO("Placeholder empty module cleaned up");
    return MODULE_STATUS_SUCCESS;
}

/**
 * @brief Execute HALO phase (does nothing)
 */
static int placeholder_execute_halo_phase(void *data, struct pipeline_context *context) {
    LOG_DEBUG("Placeholder module HALO phase (no-op)");
    return MODULE_STATUS_SUCCESS;
}

/**
 * @brief Execute GALAXY phase (does nothing)
 */
static int placeholder_execute_galaxy_phase(void *data, struct pipeline_context *context) {
    LOG_DEBUG("Placeholder module GALAXY phase (no-op)");
    return MODULE_STATUS_SUCCESS;
}

/**
 * @brief Execute POST phase (does nothing)
 */
static int placeholder_execute_post_phase(void *data, struct pipeline_context *context) {
    LOG_DEBUG("Placeholder module POST phase (no-op)");
    return MODULE_STATUS_SUCCESS;
}

/**
 * @brief Execute FINAL phase (does nothing)
 */
static int placeholder_execute_final_phase(void *data, struct pipeline_context *context) {
    LOG_DEBUG("Placeholder module FINAL phase (no-op)");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Module definition
 */
struct base_module placeholder_module = {
    .name = "placeholder_module",              /* Module name */
    .type = MODULE_TYPE_MISC,                /* Module type */
    .version = "1.0",                        /* Version */
    .author = "SAGE Team",                   /* Author */
    .initialize = placeholder_init,          /* Initialize function */
    .cleanup = placeholder_cleanup,          /* Cleanup function */
    .configure = NULL,                       /* Configure function */
    .execute_halo_phase = placeholder_execute_halo_phase,
    .execute_galaxy_phase = placeholder_execute_galaxy_phase,
    .execute_post_phase = placeholder_execute_post_phase,
    .execute_final_phase = placeholder_execute_final_phase,
    .phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_GALAXY | PIPELINE_PHASE_POST | PIPELINE_PHASE_FINAL /* All phases */
};

/* Register the module at startup */
static void __attribute__((constructor)) register_module(void) {
    module_register(&placeholder_module);
}
