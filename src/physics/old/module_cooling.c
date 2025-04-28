#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../core/core_allvars.h"
#include "../core/core_logging.h"
#include "../core/core_mymalloc.h"
#include "../core/core_module_system.h"
#include "../core/core_parameter_views.h"
#include "../core/core_event_system.h"

#include "module_cooling.h"
#include "model_cooling_heating.h"

/**
 * Cooling module data structure
 * 
 * Contains the state and parameters needed by the cooling module.
 */
struct cooling_module_data {
    struct cooling_params_view cooling_params;
    struct agn_params_view agn_params;
};

/* Default cooling module implementation */
static struct cooling_module default_cooling_module;

/**
 * Default cooling calculation function
 * 
 * Wrapper around the existing cooling_recipe function.
 */
static double default_calculate_cooling(
    const int gal, 
    const double dt, 
    struct GALAXY *galaxies, 
    void *module_data
) {
    struct cooling_module_data *data = (struct cooling_module_data *)module_data;
    return cooling_recipe(gal, dt, galaxies, &data->cooling_params);
}

/**
 * Default AGN heating function
 * 
 * Wrapper around the existing do_AGN_heating function.
 */
static double default_calculate_agn_heating(
    double cooling_gas,
    const int centralgal, 
    const double dt, 
    const double x, 
    const double rcool, 
    struct GALAXY *galaxies, 
    void *module_data
) {
    struct cooling_module_data *data = (struct cooling_module_data *)module_data;
    return do_AGN_heating(cooling_gas, centralgal, dt, x, rcool, galaxies, &data->agn_params);
}

/**
 * Default gas cooling onto galaxy function
 * 
 * Wrapper around the existing cool_gas_onto_galaxy function.
 */
static void default_cool_gas_onto_galaxy(
    const int centralgal, 
    const double cooling_gas, 
    struct GALAXY *galaxies,
    void *module_data __attribute__((unused))
) {
    /* The original function doesn't use module data */
    cool_gas_onto_galaxy(centralgal, cooling_gas, galaxies);
}

/**
 * Initialize the cooling module data
 * 
 * Initializes the cooling module with the provided parameters.
 */
static int default_cooling_initialize(struct params *params, void **module_data) {
    /* Allocate module data */
    struct cooling_module_data *data = (struct cooling_module_data *)mymalloc(sizeof(struct cooling_module_data));
    if (data == NULL) {
        LOG_ERROR("Failed to allocate memory for cooling module data");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize parameter views */
    initialize_cooling_params_view(&data->cooling_params, params);
    initialize_agn_params_view(&data->agn_params, params);
    
    /* Set the module data pointer */
    *module_data = data;
    
    LOG_INFO("Default cooling module initialized");
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up the cooling module data
 * 
 * Releases resources used by the cooling module.
 */
static int default_cooling_cleanup(void *module_data) {
    if (module_data != NULL) {
        myfree(module_data);
    }
    
    LOG_INFO("Default cooling module cleaned up");
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Create a cooling module that wraps the default SAGE cooling implementation.
 * 
 * This function creates a cooling module that uses the existing cooling_recipe 
 * and related functions from model_cooling_heating.c.
 * 
 * @return Pointer to a newly created cooling module
 */
struct cooling_module *create_default_cooling_module(void) {
    /* Initialize the static default cooling module */
    strncpy(default_cooling_module.base.name, "DefaultCooling", MAX_MODULE_NAME - 1);
    strncpy(default_cooling_module.base.version, "1.0.0", MAX_MODULE_VERSION - 1);
    strncpy(default_cooling_module.base.author, "SAGE Team", MAX_MODULE_AUTHOR - 1);
    default_cooling_module.base.type = MODULE_TYPE_COOLING;
    
    /* Set up lifecycle functions */
    default_cooling_module.base.initialize = default_cooling_initialize;
    default_cooling_module.base.cleanup = default_cooling_cleanup;
    
    /* Set up cooling-specific functions */
    default_cooling_module.calculate_cooling = default_calculate_cooling;
    default_cooling_module.calculate_agn_heating = default_calculate_agn_heating;
    default_cooling_module.cool_gas_onto_galaxy = default_cool_gas_onto_galaxy;
    
    /* Optional utility functions are not implemented in the default module */
    default_cooling_module.get_cooling_rate = NULL;
    default_cooling_module.get_cooling_radius = NULL;
    
    return &default_cooling_module;
}

/**
 * Initialize a cooling module
 * 
 * Helper function to initialize a cooling module from the module registry.
 * 
 * @param cooling_module Pointer to the cooling module interface
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int cooling_module_initialize(struct cooling_module *cooling_module, struct params *params) {
    if (cooling_module == NULL) {
        LOG_ERROR("NULL cooling module pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Register the module if not already registered */
    if (cooling_module->base.module_id < 0) {
        int status = cooling_module_register(cooling_module);
        if (status != MODULE_STATUS_SUCCESS) {
            return status;
        }
    }
    
    /* Initialize the module */
    return module_initialize(cooling_module->base.module_id, params);
}

/**
 * Register a cooling module with the module system
 * 
 * Helper function to register a cooling module with the global module registry.
 * 
 * @param cooling_module Pointer to the cooling module interface
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int cooling_module_register(struct cooling_module *cooling_module) {
    if (cooling_module == NULL) {
        LOG_ERROR("NULL cooling module pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Validate the cooling module */
    if (!cooling_module_validate(cooling_module)) {
        LOG_ERROR("Invalid cooling module");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Register with the module system */
    return module_register(&cooling_module->base);
}

/**
 * Validate a cooling module
 * 
 * Checks that a cooling module interface is valid and properly formed.
 * 
 * @param cooling_module Pointer to the cooling module interface
 * @return true if the module is valid, false otherwise
 */
bool cooling_module_validate(const struct cooling_module *cooling_module) {
    if (cooling_module == NULL) {
        LOG_ERROR("NULL cooling module pointer");
        return false;
    }
    
    /* Validate the base module */
    if (!module_validate(&cooling_module->base)) {
        return false;
    }
    
    /* Check that the module is of the correct type */
    if (cooling_module->base.type != MODULE_TYPE_COOLING) {
        LOG_ERROR("Module '%s' is not a cooling module (type = %d)",
                 cooling_module->base.name, cooling_module->base.type);
        return false;
    }
    
    /* Check required functions */
    if (cooling_module->calculate_cooling == NULL) {
        LOG_ERROR("Cooling module '%s' missing calculate_cooling function",
                 cooling_module->base.name);
        return false;
    }
    
    if (cooling_module->cool_gas_onto_galaxy == NULL) {
        LOG_ERROR("Cooling module '%s' missing cool_gas_onto_galaxy function",
                 cooling_module->base.name);
        return false;
    }
    
    /* AGN heating can be NULL if not supported */
    
    return true;
}

/**
 * Get the active cooling module
 * 
 * Retrieves the active cooling module from the global module registry.
 * 
 * @param cooling_module Output pointer to the cooling module interface
 * @param module_data Output pointer to the cooling module's data
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int get_active_cooling_module(struct cooling_module **cooling_module, void **module_data) {
    struct base_module *base_module;
    int status = module_get_active_by_type(MODULE_TYPE_COOLING, &base_module, module_data);
    
    if (status != MODULE_STATUS_SUCCESS) {
        return status;
    }
    
    /* Cast to cooling module */
    *cooling_module = (struct cooling_module *)base_module;
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Apply cooling to a galaxy
 * 
 * This function uses the active cooling module to calculate and apply cooling
 * to a galaxy. It provides a high-level interface for the core evolution code.
 * 
 * @param gal Galaxy index
 * @param dt Time step
 * @param galaxies Array of galaxies
 * @return Amount of gas that cooled
 */
double apply_cooling(const int gal, const double dt, struct GALAXY *galaxies) {
    struct cooling_module *cooling_module;
    void *module_data;
    
    /* Get the active cooling module */
    int status = get_active_cooling_module(&cooling_module, &module_data);
    
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to get active cooling module, status = %d", status);
        
        /* Fall back to the legacy implementation if no active module is found */
        if (status == MODULE_STATUS_ERROR) {
            LOG_WARNING("Falling back to legacy cooling implementation");
            return cooling_recipe_compat(gal, dt, galaxies, NULL);
        }
        
        return 0.0;
    }
    
    /* Calculate cooling amount */
    double cooling_gas = cooling_module->calculate_cooling(gal, dt, galaxies, module_data);
    
    /* Apply cooling to the galaxy */
    if (cooling_gas > 0.0) {
        int centralgal = galaxies[gal].CentralGal;
        cooling_module->cool_gas_onto_galaxy(centralgal, cooling_gas, galaxies, module_data);
        
        /* Emit cooling completed event */
        event_cooling_completed_data_t event_data = {
            .cooling_rate = cooling_gas / dt,
            .cooling_radius = cooling_module->get_cooling_radius != NULL ?
                             cooling_module->get_cooling_radius(gal, galaxies, module_data) : 0.0,
            .hot_gas_cooled = cooling_gas
        };
        
        event_emit(
            EVENT_COOLING_COMPLETED,               /* Event type */
            cooling_module->base.module_id,        /* Source module ID */
            gal,                                  /* Galaxy index */
            -1,                                   /* No specific step */
            &event_data,                          /* Event data */
            sizeof(event_data),                   /* Data size */
            EVENT_FLAG_PROPAGATE                  /* Allow all handlers to process */
        );
    }
    
    return cooling_gas;
}
