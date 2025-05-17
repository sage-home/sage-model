#include "save_gals_hdf5_internal.h"
#include "../core/core_property_utils.h" // For property accessors

// Refactored prepare_galaxy_for_hdf5_output function using the dispatch_property_transformer
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
                    LOG_ERROR("Galaxy type = %d can not be represented in 2 bytes", GALAXY_PROP_Type(g));
                    LOG_ERROR("Converting galaxy type while saving from integer to short will result in data corruption");
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
        // Handle physics properties via the dispatch_property_transformer
        else {
            // Determine the element size for pointer arithmetic
            size_t element_size = 0;
            if (buffer->h5_dtype == H5T_NATIVE_FLOAT) {
                element_size = sizeof(float);
            }
            else if (buffer->h5_dtype == H5T_NATIVE_DOUBLE) {
                element_size = sizeof(double);
            }
            else if (buffer->h5_dtype == H5T_NATIVE_INT) {
                element_size = sizeof(int32_t);
            }
            else if (buffer->h5_dtype == H5T_NATIVE_LLONG) {
                element_size = sizeof(int64_t);
            }
            
            if (element_size == 0) {
                LOG_ERROR("Unsupported HDF5 type for property '%s' in prepare_galaxy_for_hdf5_output", buffer->name);
                return EXIT_FAILURE;
            }
            
            // Get the pointer to the output buffer element for this galaxy's property
            void *output_buffer_element_ptr = ((char*)buffer->data) + (gals_in_buffer * element_size);
            
            // Call the dispatcher to transform the property value for output
            int transform_status = dispatch_property_transformer(g,
                                                               prop_id,
                                                               buffer->name,
                                                               output_buffer_element_ptr,
                                                               run_params,
                                                               buffer->h5_dtype);
            
            if (transform_status != 0) {
                LOG_ERROR("Error transforming property '%s' (ID: %d) for output.", buffer->name, prop_id);
                // Continue with other properties instead of failing the entire process
                // The dispatcher should have set a default value on error
            }
        }
    }
    
    return EXIT_SUCCESS;
}