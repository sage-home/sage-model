/**
 * @file placeholder_cooling_module.c
 * @brief A placeholder for the cooling physics module
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
#include "../core/core_properties.h"
#include "physics_modules.h"
#include "../core/core_pipeline_registry.h" // Added for factory registration

/* Module data structure */
struct placeholder_cooling_data {
    int initialized;
};

/* Function prototypes */
static int placeholder_cooling_init(struct params *params, void **data_ptr);
static int placeholder_cooling_cleanup(void *data);
static int placeholder_cooling_execute_galaxy_phase(void *data, struct pipeline_context *context);

// Factory function for this module
struct base_module *placeholder_cooling_module_factory(void) {
    return &placeholder_cooling_module;
}

/**
 * @brief Initialize the placeholder cooling module
 */
static int placeholder_cooling_init(struct params *params, void **data_ptr) {
    /* Mark unused parameter */
    (void)params;
    
    /* Allocate module data */
    struct placeholder_cooling_data *data = calloc(1, sizeof(struct placeholder_cooling_data));
    if (data == NULL) {
        LOG_ERROR("Failed to allocate memory for placeholder cooling module data");
        return MODULE_STATUS_ERROR;
    }
    
    /* Initialize module data */
    data->initialized = 1;
    
    /* Set data pointer */
    *data_ptr = data;
    
    LOG_INFO("Placeholder cooling module initialized");
    return MODULE_STATUS_SUCCESS;
}

/**
 * @brief Clean up the placeholder cooling module
 */
static int placeholder_cooling_cleanup(void *data) {
    if (data != NULL) {
        free(data);
    }
    
    LOG_INFO("Placeholder cooling module cleaned up");
    return MODULE_STATUS_SUCCESS;
}

/**
 * @brief Execute GALAXY phase (does minimal operations to ensure stability)
 */
static int placeholder_cooling_execute_galaxy_phase(void *data, struct pipeline_context *context) {
    /* Mark unused parameters */
    (void)data;
    
    if (context == NULL || context->galaxies == NULL || context->current_galaxy < 0) {
        LOG_ERROR("Invalid context in placeholder cooling module");
        return MODULE_STATUS_ERROR;
    }
    
    LOG_DEBUG("Placeholder cooling module GALAXY phase executed for galaxy %d (no-op)", 
              context->current_galaxy);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Module definition
 */
struct base_module placeholder_cooling_module = {
    .name = "placeholder_cooling_module",
    .type = MODULE_TYPE_COOLING,
    .version = "1.0",
    .author = "SAGE Team",
    .initialize = placeholder_cooling_init,
    .cleanup = placeholder_cooling_cleanup,
    .configure = NULL,
    .execute_galaxy_phase = placeholder_cooling_execute_galaxy_phase,
    .phases = PIPELINE_PHASE_GALAXY
};

/* Register the module at startup */
static void __attribute__((constructor)) register_module_and_factory(void) {
    module_register(&placeholder_cooling_module); // Existing registration with module system
    // New: Register factory with the pipeline registry
    pipeline_register_module_factory(MODULE_TYPE_COOLING, 
                                     "placeholder_cooling_module", 
                                     placeholder_cooling_module_factory);
    LOG_DEBUG("PlaceholderCooling module factory registered with pipeline registry.");
}
