/**
 * @file physics_output_transformers.c
 * @brief Implementations of output transformer functions for physics properties
 *
 * This file contains the implementation of transformer functions that convert
 * internal property representations to output format (e.g., unit conversion,
 * log scaling, derived values).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../core/core_allvars.h"
#include "../core/core_module_system.h"
#include "../core/core_properties.h"
#include "../core/core_property_utils.h"
#include "../core/core_logging.h"
#include "../core/macros.h"
#include "physics_output_transformers.h"

/**
 * @brief Transform the Cooling property for output
 * 
 * Applies log10 scaling and unit conversion if value is positive
 */
int transform_output_Cooling(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                            void *output_buffer_element_ptr, const struct params *run_params) {
    
    if (output_prop_id == PROP_COUNT || !has_property(galaxy, output_prop_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    // Get the raw cooling value
    double raw_cooling = get_double_property(galaxy, output_prop_id, 0.0);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    if (raw_cooling > 0.0) {
        // Convert to log10 scale with unit conversion
        *output_val_ptr = (float)log10(raw_cooling * run_params->units.UnitEnergy_in_cgs / 
                                       run_params->units.UnitTime_in_s);
    } else {
        // Handle zero or negative values
        *output_val_ptr = 0.0f;
    }
    
    return 0;
}

/**
 * @brief Transform the Heating property for output
 * 
 * Applies log10 scaling and unit conversion if value is positive
 */
int transform_output_Heating(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                           void *output_buffer_element_ptr, const struct params *run_params) {
    
    if (output_prop_id == PROP_COUNT || !has_property(galaxy, output_prop_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    // Get the raw heating value
    double raw_heating = get_double_property(galaxy, output_prop_id, 0.0);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    if (raw_heating > 0.0) {
        // Convert to log10 scale with unit conversion
        *output_val_ptr = (float)log10(raw_heating * run_params->units.UnitEnergy_in_cgs / 
                                      run_params->units.UnitTime_in_s);
    } else {
        // Handle zero or negative values
        *output_val_ptr = 0.0f;
    }
    
    return 0;
}

/**
 * @brief Transform the TimeOfLastMajorMerger property for output
 * 
 * Applies unit conversion to Megayears
 */
int transform_output_TimeOfLastMajorMerger(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                         void *output_buffer_element_ptr, const struct params *run_params) {
    
    if (output_prop_id == PROP_COUNT || !has_property(galaxy, output_prop_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    // Get the raw value
    float raw_time = get_float_property(galaxy, output_prop_id, 0.0f);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    // Convert to Megayears
    *output_val_ptr = raw_time * run_params->units.UnitTime_in_Megayears;
    
    return 0;
}

/**
 * @brief Transform the TimeOfLastMinorMerger property for output
 * 
 * Applies unit conversion to Megayears
 */
int transform_output_TimeOfLastMinorMerger(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                         void *output_buffer_element_ptr, const struct params *run_params) {
    
    if (output_prop_id == PROP_COUNT || !has_property(galaxy, output_prop_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    // Get the raw value
    float raw_time = get_float_property(galaxy, output_prop_id, 0.0f);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    // Convert to Megayears
    *output_val_ptr = raw_time * run_params->units.UnitTime_in_Megayears;
    
    return 0;
}

/**
 * @brief Transform the OutflowRate property for output
 * 
 * Applies unit conversion to Msun/yr
 */
int transform_output_OutflowRate(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                               void *output_buffer_element_ptr, const struct params *run_params) {
    
    if (output_prop_id == PROP_COUNT || !has_property(galaxy, output_prop_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    // Get the raw value
    float raw_outflow = get_float_property(galaxy, output_prop_id, 0.0f);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    // Convert to Msun/yr
    *output_val_ptr = raw_outflow * run_params->units.UnitMass_in_g / 
                     run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS;
    
    return 0;
}

/**
 * @brief Derive SfrDisk scalar output from SfrDisk array
 * 
 * Calculates average SFR and applies unit conversion
 */
int derive_output_SfrDisk(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                        void *output_buffer_element_ptr, const struct params *run_params) {
    
    if (output_prop_id == PROP_COUNT || !has_property(galaxy, output_prop_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    float tmp_SfrDisk = 0.0f;
    
    // Sum over all array elements and calculate average
    for (int step = 0; step < STEPS; step++) {
        float sfr_disk_val = get_float_array_element_property(galaxy, output_prop_id, step, 0.0f);
        
        // Apply unit conversion for each step
        tmp_SfrDisk += sfr_disk_val * run_params->units.UnitMass_in_g / 
                     run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
    }
    
    *output_val_ptr = tmp_SfrDisk;
    return 0;
}

/**
 * @brief Derive SfrBulge scalar output from SfrBulge array
 * 
 * Calculates average SFR and applies unit conversion
 */
int derive_output_SfrBulge(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                         void *output_buffer_element_ptr, const struct params *run_params) {
    
    if (output_prop_id == PROP_COUNT || !has_property(galaxy, output_prop_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    float tmp_SfrBulge = 0.0f;
    
    // Sum over all array elements and calculate average
    for (int step = 0; step < STEPS; step++) {
        float sfr_bulge_val = get_float_array_element_property(galaxy, output_prop_id, step, 0.0f);
        
        // Apply unit conversion for each step
        tmp_SfrBulge += sfr_bulge_val * run_params->units.UnitMass_in_g / 
                       run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
    }
    
    *output_val_ptr = tmp_SfrBulge;
    return 0;
}

/**
 * @brief Derive SfrDiskZ (metallicity) scalar output from SfrDiskColdGas and SfrDiskColdGasMetals arrays
 * 
 * Calculates average metallicity across steps
 */
int derive_output_SfrDiskZ(const struct GALAXY *galaxy, property_id_t output_prop_id __attribute__((unused)), 
                         void *output_buffer_element_ptr, const struct params *run_params __attribute__((unused))) {
    // Get source property IDs
    property_id_t sfr_disk_cold_gas_id = get_cached_property_id("SfrDiskColdGas");
    property_id_t sfr_disk_cold_gas_metals_id = get_cached_property_id("SfrDiskColdGasMetals");
    
    if (sfr_disk_cold_gas_id == PROP_COUNT || !has_property(galaxy, sfr_disk_cold_gas_id) ||
        sfr_disk_cold_gas_metals_id == PROP_COUNT || !has_property(galaxy, sfr_disk_cold_gas_metals_id)) {
        // Properties not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    float tmp_SfrDiskZ = 0.0f;
    int valid_steps = 0;
    
    // Calculate metallicity for each step and average
    for (int step = 0; step < STEPS; step++) {
        float sfr_disk_cold_gas_val = get_float_array_element_property(galaxy, sfr_disk_cold_gas_id, step, 0.0f);
        float sfr_disk_cold_gas_metals_val = get_float_array_element_property(galaxy, sfr_disk_cold_gas_metals_id, step, 0.0f);
        
        if (sfr_disk_cold_gas_val > 0.0f) {
            tmp_SfrDiskZ += sfr_disk_cold_gas_metals_val / sfr_disk_cold_gas_val;
            valid_steps++;
        }
    }
    
    // Average the metallicity values from valid steps
    if (valid_steps > 0) {
        *output_val_ptr = tmp_SfrDiskZ / valid_steps;
    } else {
        *output_val_ptr = 0.0f;
    }
    
    return 0;
}

/**
 * @brief Derive SfrBulgeZ (metallicity) scalar output from SfrBulgeColdGas and SfrBulgeColdGasMetals arrays
 * 
 * Calculates average metallicity across steps
 */
int derive_output_SfrBulgeZ(const struct GALAXY *galaxy, property_id_t output_prop_id __attribute__((unused)), 
                          void *output_buffer_element_ptr, const struct params *run_params __attribute__((unused))) {
    // Get source property IDs
    property_id_t sfr_bulge_cold_gas_id = get_cached_property_id("SfrBulgeColdGas");
    property_id_t sfr_bulge_cold_gas_metals_id = get_cached_property_id("SfrBulgeColdGasMetals");
    
    if (sfr_bulge_cold_gas_id == PROP_COUNT || !has_property(galaxy, sfr_bulge_cold_gas_id) ||
        sfr_bulge_cold_gas_metals_id == PROP_COUNT || !has_property(galaxy, sfr_bulge_cold_gas_metals_id)) {
        // Properties not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    float tmp_SfrBulgeZ = 0.0f;
    int valid_steps = 0;
    
    // Calculate metallicity for each step and average
    for (int step = 0; step < STEPS; step++) {
        float sfr_bulge_cold_gas_val = get_float_array_element_property(galaxy, sfr_bulge_cold_gas_id, step, 0.0f);
        float sfr_bulge_cold_gas_metals_val = get_float_array_element_property(galaxy, sfr_bulge_cold_gas_metals_id, step, 0.0f);
        
        if (sfr_bulge_cold_gas_val > 0.0f) {
            tmp_SfrBulgeZ += sfr_bulge_cold_gas_metals_val / sfr_bulge_cold_gas_val;
            valid_steps++;
        }
    }
    
    // Average the metallicity values from valid steps
    if (valid_steps > 0) {
        *output_val_ptr = tmp_SfrBulgeZ / valid_steps;
    } else {
        *output_val_ptr = 0.0f;
    }
    
    return 0;
}