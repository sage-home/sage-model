#include "save_gals_hdf5_internal.h"
#include "../core/core_property_utils.h" // Added for proper array property accessors

// Refactored prepare_galaxy_for_hdf5_output function using the property system
int32_t prepare_galaxy_for_hdf5_output(const struct GALAXY *g, struct save_info *save_info_base,
                                     const int32_t output_snap_idx, const struct halo_data *halos,
                                     const int64_t task_forestnr, const int64_t original_treenr,
                                     const struct params *run_params)
{
    // Convert the base save_info to hdf5_save_info
    struct hdf5_save_info *save_info = (struct hdf5_save_info *)save_info_base->io_handler.format_data;
    int64_t gals_in_buffer = save_info->num_gals_in_buffer[output_snap_idx];
    
    // Process each property
    for (int i = 0; i < save_info->num_properties; i++) {
        struct property_buffer_info *buffer = &save_info->property_buffers[output_snap_idx][i];
        property_id_t prop_id = buffer->prop_id;
        
        // Handle core properties directly
        if (buffer->is_core_prop) {
            // Direct core property access using macros
            if (strcmp(buffer->name, "SnapNum") == 0) {
                ((int32_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_SnapNum(g);
            }
            else if (strcmp(buffer->name, "Type") == 0) {
                if (GALAXY_PROP_Type(g) < SHRT_MIN || GALAXY_PROP_Type(g) > SHRT_MAX) {
                    fprintf(stderr, "Error: Galaxy type = %d can not be represented in 2 bytes\n", GALAXY_PROP_Type(g));
                    fprintf(stderr, "Converting galaxy type while saving from integer to short will result in data corruption");
                    return EXIT_FAILURE;
                }
                ((int32_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Type(g);
            }
            else if (strcmp(buffer->name, "GalaxyIndex") == 0) {
                ((uint64_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_GalaxyIndex(g);
            }
            else if (strcmp(buffer->name, "CentralGalaxyIndex") == 0) {
                ((uint64_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_CentralGalaxyIndex(g);
            }
            else if (strcmp(buffer->name, "SAGEHaloIndex") == 0) {
                ((int32_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_HaloNr(g);
            }
            else if (strcmp(buffer->name, "SAGETreeIndex") == 0) {
                ((int32_t*)buffer->data)[gals_in_buffer] = original_treenr;
            }
            else if (strcmp(buffer->name, "SimulationHaloIndex") == 0) {
                ((int64_t*)buffer->data)[gals_in_buffer] = llabs(halos[GALAXY_PROP_HaloNr(g)].MostBoundID);
            }
            else if (strcmp(buffer->name, "TaskForestNr") == 0) {
                ((int64_t*)buffer->data)[gals_in_buffer] = task_forestnr;
            }
            else if (strcmp(buffer->name, "mergeType") == 0) {
                ((int32_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_mergeType(g);
            }
            else if (strcmp(buffer->name, "mergeIntoID") == 0) {
                ((int32_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_mergeIntoID(g);
            }
            else if (strcmp(buffer->name, "mergeIntoSnapNum") == 0) {
                ((int32_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_mergeIntoSnapNum(g);
            }
            else if (strcmp(buffer->name, "dT") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_dT(g) * run_params->units.UnitTime_in_s / SEC_PER_MEGAYEAR;
            }
            // Position components (special handling)
            else if (strcmp(buffer->name, "Posx") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Pos_ELEM(g, 0);
            }
            else if (strcmp(buffer->name, "Posy") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Pos_ELEM(g, 1);
            }
            else if (strcmp(buffer->name, "Posz") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Pos_ELEM(g, 2);
            }
            // Velocity components (special handling)
            else if (strcmp(buffer->name, "Velx") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Vel_ELEM(g, 0);
            }
            else if (strcmp(buffer->name, "Vely") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Vel_ELEM(g, 1);
            }
            else if (strcmp(buffer->name, "Velz") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Vel_ELEM(g, 2);
            }
            // Spin components (special handling)
            else if (strcmp(buffer->name, "Spinx") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = halos[GALAXY_PROP_HaloNr(g)].Spin[0];
            }
            else if (strcmp(buffer->name, "Spiny") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = halos[GALAXY_PROP_HaloNr(g)].Spin[1];
            }
            else if (strcmp(buffer->name, "Spinz") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = halos[GALAXY_PROP_HaloNr(g)].Spin[2];
            }
            else if (strcmp(buffer->name, "Len") == 0) {
                ((int32_t*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Len(g);
            }
            else if (strcmp(buffer->name, "Mvir") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Mvir(g);
            }
            else if (strcmp(buffer->name, "CentralMvir") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = get_virial_mass(halos[GALAXY_PROP_HaloNr(g)].FirstHaloInFOFgroup, halos, run_params);
            }
            else if (strcmp(buffer->name, "Rvir") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = get_virial_radius(GALAXY_PROP_HaloNr(g), halos, run_params);
            }
            else if (strcmp(buffer->name, "Vvir") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = get_virial_velocity(GALAXY_PROP_HaloNr(g), halos, run_params);
            }
            else if (strcmp(buffer->name, "Vmax") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_Vmax(g);
            }
            else if (strcmp(buffer->name, "VelDisp") == 0) {
                ((float*)buffer->data)[gals_in_buffer] = halos[GALAXY_PROP_HaloNr(g)].VelDisp;
            }
            else if (strcmp(buffer->name, "infallMvir") == 0) {
                // Check if this is a satellite (Type != 0)
                if (GALAXY_PROP_Type(g) != 0) {
                    ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_infallMvir(g);
                } else {
                    ((float*)buffer->data)[gals_in_buffer] = 0.0;
                }
            }
            else if (strcmp(buffer->name, "infallVvir") == 0) {
                if (GALAXY_PROP_Type(g) != 0) {
                    ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_infallVvir(g);
                } else {
                    ((float*)buffer->data)[gals_in_buffer] = 0.0;
                }
            }
            else if (strcmp(buffer->name, "infallVmax") == 0) {
                if (GALAXY_PROP_Type(g) != 0) {
                    ((float*)buffer->data)[gals_in_buffer] = GALAXY_PROP_infallVmax(g);
                } else {
                    ((float*)buffer->data)[gals_in_buffer] = 0.0;
                }
            }
        } 
        // Handle physics properties via the property system
        else {
            // Check if galaxy has this property
            if (has_property(g, prop_id)) {
                // Get property based on HDF5 datatype
                if (buffer->h5_dtype == H5T_NATIVE_FLOAT) {
                    float value = get_float_property(g, prop_id, 0.0f);
                    
                    // Special handling for certain properties that need unit conversion
                    if (strcmp(buffer->name, "Cooling") == 0 && value > 0.0) {
                        value = log10(value * run_params->units.UnitEnergy_in_cgs / run_params->units.UnitTime_in_s);
                    }
                    else if (strcmp(buffer->name, "Heating") == 0 && value > 0.0) {
                        value = log10(value * run_params->units.UnitEnergy_in_cgs / run_params->units.UnitTime_in_s);
                    }
                    else if (strcmp(buffer->name, "TimeOfLastMajorMerger") == 0) {
                        value = value * run_params->units.UnitTime_in_Megayears;
                    }
                    else if (strcmp(buffer->name, "TimeOfLastMinorMerger") == 0) {
                        value = value * run_params->units.UnitTime_in_Megayears;
                    }
                    else if (strcmp(buffer->name, "OutflowRate") == 0) {
                        value = value * run_params->units.UnitMass_in_g / run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS;
                    }
                    // Special handling for SFR properties which need to be summed from arrays
                    else if (strcmp(buffer->name, "SfrDisk") == 0 || 
                             strcmp(buffer->name, "SfrBulge") == 0 || 
                             strcmp(buffer->name, "SfrDiskZ") == 0 || 
                             strcmp(buffer->name, "SfrBulgeZ") == 0) {
                        // These properties require special processing from arrays
                        // They will be handled below
                        continue;
                    }
                    
                    ((float*)buffer->data)[gals_in_buffer] = value;
                }
                else if (buffer->h5_dtype == H5T_NATIVE_DOUBLE) {
                    double value = get_double_property(g, prop_id, 0.0);
                    ((double*)buffer->data)[gals_in_buffer] = value;
                }
                else if (buffer->h5_dtype == H5T_NATIVE_INT) {
                    int32_t value = get_int32_property(g, prop_id, 0);
                    ((int32_t*)buffer->data)[gals_in_buffer] = value;
                }
                else if (buffer->h5_dtype == H5T_NATIVE_LLONG) {
                    int64_t value = (int64_t)get_int32_property(g, prop_id, 0);
                    ((int64_t*)buffer->data)[gals_in_buffer] = value;
                }
            }
            else {
                // Property not found, set to default (zero)
                if (buffer->h5_dtype == H5T_NATIVE_FLOAT) {
                    ((float*)buffer->data)[gals_in_buffer] = 0.0f;
                }
                else if (buffer->h5_dtype == H5T_NATIVE_DOUBLE) {
                    ((double*)buffer->data)[gals_in_buffer] = 0.0;
                }
                else if (buffer->h5_dtype == H5T_NATIVE_INT) {
                    ((int32_t*)buffer->data)[gals_in_buffer] = 0;
                }
                else if (buffer->h5_dtype == H5T_NATIVE_LLONG) {
                    ((int64_t*)buffer->data)[gals_in_buffer] = 0;
                }
            }
        }
    }
    
    // Special handling for SFR history properties which need array processing
    // Get the property IDs for these special properties
    property_id_t sfr_disk_id = get_cached_property_id("SfrDisk");
    property_id_t sfr_bulge_id = get_cached_property_id("SfrBulge");
    property_id_t sfr_disk_cold_gas_id = get_cached_property_id("SfrDiskColdGas");
    property_id_t sfr_disk_cold_gas_metals_id = get_cached_property_id("SfrDiskColdGasMetals");
    property_id_t sfr_bulge_cold_gas_id = get_cached_property_id("SfrBulgeColdGas");
    property_id_t sfr_bulge_cold_gas_metals_id = get_cached_property_id("SfrBulgeColdGasMetals");
    
    // Find the output buffer indices for these properties
    int sfr_disk_idx = -1, sfr_bulge_idx = -1, sfr_disk_z_idx = -1, sfr_bulge_z_idx = -1;
    for (int i = 0; i < save_info->num_properties; i++) {
        if (strcmp(save_info->property_buffers[output_snap_idx][i].name, "SfrDisk") == 0) {
            sfr_disk_idx = i;
        }
        else if (strcmp(save_info->property_buffers[output_snap_idx][i].name, "SfrBulge") == 0) {
            sfr_bulge_idx = i;
        }
        else if (strcmp(save_info->property_buffers[output_snap_idx][i].name, "SfrDiskZ") == 0) {
            sfr_disk_z_idx = i;
        }
        else if (strcmp(save_info->property_buffers[output_snap_idx][i].name, "SfrBulgeZ") == 0) {
            sfr_bulge_z_idx = i;
        }
    }
    
    // Process SFR history if these properties are requested
    if (sfr_disk_idx >= 0 || sfr_bulge_idx >= 0 || sfr_disk_z_idx >= 0 || sfr_bulge_z_idx >= 0) {
        float tmp_SfrDisk = 0.0;
        float tmp_SfrBulge = 0.0;
        float tmp_SfrDiskZ = 0.0;
        float tmp_SfrBulgeZ = 0.0;
        
        // Check if the galaxy has these properties
        if (has_property(g, sfr_disk_id) && 
            has_property(g, sfr_bulge_id) && 
            has_property(g, sfr_disk_cold_gas_id) && 
            has_property(g, sfr_disk_cold_gas_metals_id) && 
            has_property(g, sfr_bulge_cold_gas_id) && 
            has_property(g, sfr_bulge_cold_gas_metals_id)) {
                
            // Access the array properties
            // In a real implementation, we would need to properly access arrays from the property system
            // For now, we can use a special array accessor function
            for (int step = 0; step < STEPS; step++) {
                // Note: This is a simplified example and would need to be replaced with proper array accessors
                // In a complete implementation, there should be functions to access array elements from properties
                float sfr_disk_val = 0.0;
                float sfr_bulge_val = 0.0;
                float sfr_disk_cold_gas_val = 0.0;
                float sfr_disk_cold_gas_metals_val = 0.0;
                float sfr_bulge_cold_gas_val = 0.0;
                float sfr_bulge_cold_gas_metals_val = 0.0;
                
                // Get array element values
                // These accessors would need to be implemented based on the property system
                property_id_t id = sfr_disk_id;
                if (has_property(g, id)) {
                    // Use the new properly implemented array accessor
                    sfr_disk_val = get_float_array_element_property(g, id, step, 0.0f);
                }
                
                id = sfr_bulge_id;
                if (has_property(g, id)) {
                    sfr_bulge_val = get_float_array_element_property(g, id, step, 0.0f);
                }
                
                id = sfr_disk_cold_gas_id;
                if (has_property(g, id)) {
                    sfr_disk_cold_gas_val = get_float_array_element_property(g, id, step, 0.0f);
                }
                
                id = sfr_disk_cold_gas_metals_id;
                if (has_property(g, id)) {
                    sfr_disk_cold_gas_metals_val = get_float_array_element_property(g, id, step, 0.0f);
                }
                
                id = sfr_bulge_cold_gas_id;
                if (has_property(g, id)) {
                    sfr_bulge_cold_gas_val = get_float_array_element_property(g, id, step, 0.0f);
                }
                
                id = sfr_bulge_cold_gas_metals_id;
                if (has_property(g, id)) {
                    sfr_bulge_cold_gas_metals_val = get_float_array_element_property(g, id, step, 0.0f);
                }
                
                // Calculate SFR values
                tmp_SfrDisk += sfr_disk_val * run_params->units.UnitMass_in_g / 
                               run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
                               
                tmp_SfrBulge += sfr_bulge_val * run_params->units.UnitMass_in_g / 
                                run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
                
                // Calculate metallicity values
                if (sfr_disk_cold_gas_val > 0.0) {
                    tmp_SfrDiskZ += sfr_disk_cold_gas_metals_val / sfr_disk_cold_gas_val / STEPS;
                }
                
                if (sfr_bulge_cold_gas_val > 0.0) {
                    tmp_SfrBulgeZ += sfr_bulge_cold_gas_metals_val / sfr_bulge_cold_gas_val / STEPS;
                }
            }
        }
        
        // Store the calculated values in output buffers
        if (sfr_disk_idx >= 0) {
            ((float*)save_info->property_buffers[output_snap_idx][sfr_disk_idx].data)[gals_in_buffer] = tmp_SfrDisk;
        }
        
        if (sfr_bulge_idx >= 0) {
            ((float*)save_info->property_buffers[output_snap_idx][sfr_bulge_idx].data)[gals_in_buffer] = tmp_SfrBulge;
        }
        
        if (sfr_disk_z_idx >= 0) {
            ((float*)save_info->property_buffers[output_snap_idx][sfr_disk_z_idx].data)[gals_in_buffer] = tmp_SfrDiskZ;
        }
        
        if (sfr_bulge_z_idx >= 0) {
            ((float*)save_info->property_buffers[output_snap_idx][sfr_bulge_z_idx].data)[gals_in_buffer] = tmp_SfrBulgeZ;
        }
    }
    
    return EXIT_SUCCESS;
}
