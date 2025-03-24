#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "../core/core_module_system.h"

/**
 * @file module_cooling.h
 * @brief Cooling module interface for SAGE
 * 
 * This file defines the interface for cooling modules in SAGE.
 * Cooling modules handle gas cooling and AGN heating processes.
 */

/* Forward declaration for the cooling module data structure */
struct cooling_module_data;

/**
 * Cooling module interface
 * 
 * Extends the base_module interface with cooling-specific functions.
 */
struct cooling_module {
    struct base_module base;              /* Inherit from base_module */
    
    /* Core function that all cooling modules must implement */
    double (*calculate_cooling)(
        const int gal, 
        const double dt, 
        struct GALAXY *galaxies, 
        void *module_data
    );
    
    /* AGN heating function - can be NULL if not supported */
    double (*calculate_agn_heating)(
        double cooling_gas,
        const int centralgal, 
        const double dt, 
        const double x, 
        const double rcool, 
        struct GALAXY *galaxies, 
        void *module_data
    );
    
    /* Optional utility functions */
    double (*get_cooling_rate)(
        double temp, 
        double metallicity, 
        void *module_data
    );
    
    double (*get_cooling_radius)(
        const int gal, 
        struct GALAXY *galaxies, 
        void *module_data
    );
    
    /* Galaxy update function - applies cooling results to galaxy properties */
    void (*cool_gas_onto_galaxy)(
        const int centralgal, 
        const double cooling_gas, 
        struct GALAXY *galaxies,
        void *module_data
    );
};

/**
 * Create a cooling module that wraps the default SAGE cooling implementation.
 * 
 * This function creates a cooling module that uses the existing cooling_recipe 
 * and related functions from model_cooling_heating.c.
 * 
 * @return Pointer to a newly created cooling module
 */
struct cooling_module *create_default_cooling_module(void);

/**
 * Initialize a cooling module
 * 
 * Helper function to initialize a cooling module from the module registry.
 * 
 * @param cooling_module Pointer to the cooling module interface
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int cooling_module_initialize(struct cooling_module *cooling_module, struct params *params);

/**
 * Register a cooling module with the module system
 * 
 * Helper function to register a cooling module with the global module registry.
 * 
 * @param cooling_module Pointer to the cooling module interface
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int cooling_module_register(struct cooling_module *cooling_module);

/**
 * Validate a cooling module
 * 
 * Checks that a cooling module interface is valid and properly formed.
 * 
 * @param cooling_module Pointer to the cooling module interface
 * @return true if the module is valid, false otherwise
 */
bool cooling_module_validate(const struct cooling_module *cooling_module);

/**
 * Get the active cooling module
 * 
 * Retrieves the active cooling module from the global module registry.
 * 
 * @param cooling_module Output pointer to the cooling module interface
 * @param module_data Output pointer to the cooling module's data
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int get_active_cooling_module(struct cooling_module **cooling_module, void **module_data);

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
double apply_cooling(const int gal, const double dt, struct GALAXY *galaxies);

#ifdef __cplusplus
}
#endif
