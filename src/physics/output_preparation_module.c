#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "output_preparation_module.h"
#include "../core/core_galaxy_extensions.h"
#include "../core/core_logging.h"
#include "../core/core_module_system.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_properties.h"

// Module handle
static int module_id = -1;
static struct base_module output_module;

/**
 * @brief Initialize the output preparation module
 *
 * @return 0 on success, non-zero on failure
 */
int init_output_preparation_module(void) {
    // Initialize module structure
    memset(&output_module, 0, sizeof(struct base_module));
    
    // Set module information
    strncpy(output_module.name, "output_preparation", sizeof(output_module.name)-1);
    strncpy(output_module.version, "1.0", sizeof(output_module.version)-1);
    strncpy(output_module.author, "SAGE Team", sizeof(output_module.author)-1);
    output_module.type = MODULE_TYPE_MISC;  // Using MISC type
    
    // Set phase bitmask to specify that this module runs in FINAL phase
    output_module.phases = PIPELINE_PHASE_FINAL;
    
    // Set execution functions for FINAL phase
    output_module.execute_final_phase = output_preparation_execute;
    
    // Register module with the system
    int ret = module_register(&output_module);
    if (ret != 0) {
        LOG_ERROR("Failed to register output preparation module");
        return -1;
    }
    
    // Save module ID
    module_id = output_module.module_id;
    
    LOG_INFO("Output preparation module initialized");
    return 0;
}

/**
 * @brief Clean up the output preparation module
 *
 * @return 0 on success, non-zero on failure
 */
int cleanup_output_preparation_module(void) {
    if (module_id >= 0) {
        int ret = module_unregister(module_id);
        if (ret != 0) {
            LOG_ERROR("Failed to unregister output preparation module");
            return -1;
        }
        module_id = -1;
    }
    
    LOG_INFO("Output preparation module cleaned up");
    return 0;
}

/**
 * @brief Register the output preparation module with the pipeline
 *
 * @param pipeline Pipeline to register with
 * @return 0 on success, non-zero on failure
 */
int register_output_preparation_module(struct module_pipeline *pipeline) {
    if (pipeline == NULL) {
        LOG_ERROR("NULL pipeline passed to register_output_preparation_module");
        return -1;
    }
    
    // Add module to pipeline
    int ret = pipeline_add_step(pipeline, MODULE_TYPE_MISC, "output_preparation", 
                               "output_preparation", true, false);
    if (ret != 0) {
        LOG_ERROR("Failed to add output preparation module to pipeline");
        return -1;
    }
    
    LOG_INFO("Output preparation module registered with pipeline");
    return 0;
}

/**
 * @brief Prepare galaxies for output
 *
 * This function performs unit conversions and calculates derived properties
 * to prepare galaxies for output. It uses GALAXY_PROP_* macros to access
 * galaxy properties directly.
 *
 * @param module_data Module-specific data (unused in this module)
 * @param ctx Pipeline context
 * @return 0 on success, non-zero on failure
 */
int output_preparation_execute(void *module_data, struct pipeline_context *ctx) {
    (void)module_data; // Prevent unused parameter warning
    
    if (ctx == NULL || ctx->galaxies == NULL) {
        LOG_ERROR("NULL context or galaxies passed to output_preparation_execute");
        return -1;
    }
    
    struct GALAXY *galaxies = ctx->galaxies;
    int num_galaxies = ctx->ngal;
    
    LOG_DEBUG("Preparing %d galaxies for output", num_galaxies);
    
    // Process each galaxy
    for (int i = 0; i < num_galaxies; i++) {
        struct GALAXY *galaxy = &galaxies[i];
        
        // Skip galaxies that don't need output preparation
        if (GALAXY_PROP_Type(galaxy) == 3) {  // Merged
            continue;
        }
        
        // Apply unit conversions and calculate derived properties
        
        // Example: Convert disk scale radius to log scale if > 0
        if (GALAXY_PROP_DiskScaleRadius(galaxy) > 0) {
            GALAXY_PROP_DiskScaleRadius(galaxy) = log10(GALAXY_PROP_DiskScaleRadius(galaxy));
        }
        
        // Example: Calculate stellar-to-halo mass ratio (if halo mass > 0)
        if (GALAXY_PROP_Mvir(galaxy) > 0 && GALAXY_PROP_StellarMass(galaxy) > 0) {
            // Just calculate - we'll store this in a proper property once defined
            float stellar_to_halo_ratio = GALAXY_PROP_StellarMass(galaxy) / GALAXY_PROP_Mvir(galaxy);
            (void)stellar_to_halo_ratio; // Prevent unused variable warning
        }
        
        // Example: Calculate specific star formation rate (if stellar mass > 0)
        if (GALAXY_PROP_StellarMass(galaxy) > 0) {
            float sfr_total = 0.0;
            // Sum star formation rates from disk 
            for (int j = 0; j < STEPS; j++) {
                sfr_total += GALAXY_PROP_SfrDisk_ELEM(galaxy, j);
            }
            
            if (sfr_total > 0) {
                // Calculate, but don't store yet since we don't have a property defined
                float specific_sfr = sfr_total / GALAXY_PROP_StellarMass(galaxy);
                (void)specific_sfr; // Prevent unused variable warning
            }
        }
        
        // Example: Convert metallicities to specific metallicities where appropriate
        if (GALAXY_PROP_ColdGas(galaxy) > 0 && GALAXY_PROP_MetalsColdGas(galaxy) > 0) {
            // Calculate, but don't store yet since we don't have a property defined
            float specific_metals_cold = GALAXY_PROP_MetalsColdGas(galaxy) / GALAXY_PROP_ColdGas(galaxy);
            (void)specific_metals_cold; // Prevent unused variable warning
        }
        
        // Example: Process and normalize star formation history arrays if needed
        if (GALAXY_PROP_StarFormationHistory(galaxy) != NULL && 
            GALAXY_PROP_StarFormationHistory_SIZE(galaxy) > 0) {
            for (int j = 0; j < GALAXY_PROP_StarFormationHistory_SIZE(galaxy); j++) {
                // Process each element as needed
                if (GALAXY_PROP_StarFormationHistory_ELEM(galaxy, j) < 0) {
                    GALAXY_PROP_StarFormationHistory_ELEM(galaxy, j) = 0;  // Ensure non-negative SFH
                }
            }
        }
        
        // Add more conversions and property preparations as needed
    }
    
    LOG_DEBUG("Completed output preparation for %d galaxies", num_galaxies);
    return 0;
}