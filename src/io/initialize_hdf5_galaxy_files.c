#include "save_gals_hdf5_internal.h"
#include "../core/core_logging.h"

// Updated initialization function for HDF5 files using property system
int32_t initialize_hdf5_galaxy_files(const int filenr, struct save_info *save_info_base, const struct params *run_params)
{
    char buffer[3*MAX_STRING_LEN];
    struct hdf5_save_info *save_info;
    
    // Create and initialize the format-specific data
    save_info = malloc(sizeof(struct hdf5_save_info));
    if (save_info == NULL) {
        LOG_ERROR("Failed to allocate memory for HDF5 save info");
        return MALLOC_FAILURE;
    }
    
    // Initialize all pointers to NULL for proper cleanup on error
    memset(save_info, 0, sizeof(struct hdf5_save_info));
    
    // Store HDF5-specific data in save_info_base for later access
    // We'll use the buffer_output_gals field to store our hdf5_save_info pointer
    save_info_base->buffer_output_gals = (struct HDF5_GALAXY_OUTPUT *)save_info;
    
    // Create the file
    snprintf(buffer, 3*MAX_STRING_LEN-1, "%s/%s_%d.hdf5", run_params->io.OutputDir, run_params->io.FileNameGalaxies, filenr);
    
    hid_t file_id = H5Fcreate(buffer, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    
    // Also populate the save_info_base structure with key fields for compatibility
    save_info_base->file_id = file_id;
    CHECK_STATUS_AND_RETURN_ON_FAIL(file_id, FILE_NOT_FOUND,
                                    "Can't open file %s for initialization.\n", buffer);
    save_info->file_id = file_id;
    
    // Discover output properties based on metadata
    int status = discover_output_properties(save_info);
    if (status != EXIT_SUCCESS) {
        LOG_ERROR("Failed to discover output properties");
        H5Fclose(file_id);
        free(save_info);
        // Note: io_handler field removed as part of unified I/O interface cleanup
        return status;
    }
    
    // Generate field metadata from properties
    status = generate_field_metadata(save_info);
    if (status != EXIT_SUCCESS) {
        LOG_ERROR("Failed to generate field metadata");
        H5Fclose(file_id);
        free_property_discovery(save_info);
        free(save_info);
        // Note: io_handler field removed as part of unified I/O interface cleanup
        return status;
    }
    
    // We will have groups for each output snapshot, and then inside those groups, a dataset for
    // each field.
    save_info->group_ids = calloc(run_params->simulation.NumSnapOutputs, sizeof(save_info->group_ids[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->group_ids,
                                     "Failed to allocate %d elements of size %zu for save_info->group_ids", 
                                     run_params->simulation.NumSnapOutputs,
                                     sizeof(*(save_info->group_ids)));
    
    // Also set the group_ids in save_info_base for compatibility
    save_info_base->group_ids = save_info->group_ids;
    
    // Create groups and datasets for each snapshot
    for (int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        // Create a snapshot group
        char full_field_name[2*MAX_STRING_LEN];
        snprintf(full_field_name, 2*MAX_STRING_LEN - 1, "Snap_%d", run_params->simulation.ListOutputSnaps[snap_idx]);
        
        hid_t group_id = H5Gcreate2(file_id, full_field_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                        "Failed to create the %s group.\nThe file ID was %d\n", full_field_name,
                                        (int32_t) file_id);
        save_info->group_ids[snap_idx] = group_id;
        
        // Add redshift attribute to the group
        const float snap_redshift = run_params->simulation.ZZ[run_params->simulation.ListOutputSnaps[snap_idx]];
        CREATE_SINGLE_ATTRIBUTE(group_id, "redshift", snap_redshift, H5T_NATIVE_FLOAT);
        
        // Create datasets for each property
        for (int i = 0; i < save_info->num_properties; i++) {
            // Build field name
            snprintf(full_field_name, 2*MAX_STRING_LEN - 1, "Snap_%d/%s", 
                    run_params->simulation.ListOutputSnaps[snap_idx], save_info->prop_names[i]);
            
            // Create property for chunked dataset
            hid_t prop = H5Pcreate(H5P_DATASET_CREATE);
            CHECK_STATUS_AND_RETURN_ON_FAIL(prop, (int32_t) prop,
                                            "Could not create the property list for output snapshot number %d.\n", snap_idx);
            
            // Create initial dataspace with 0 dimension
            hsize_t dims[1] = {0};
            hsize_t maxdims[1] = {H5S_UNLIMITED};
            hsize_t chunk_dims[1] = {NUM_GALS_PER_BUFFER};
            
            hid_t dataspace_id = H5Screate_simple(1, dims, maxdims);
            CHECK_STATUS_AND_RETURN_ON_FAIL(dataspace_id, (int32_t) dataspace_id,
                                            "Could not create a dataspace for output snapshot number %d.\n"
                                            "The requested initial size was %d with an unlimited maximum upper bound.",
                                            snap_idx, (int32_t) dims[0]);
            
            // Set chunk size
            herr_t chunk_status = H5Pset_chunk(prop, 1, chunk_dims);
            CHECK_STATUS_AND_RETURN_ON_FAIL(chunk_status, (int32_t) chunk_status,
                                            "Could not set the HDF5 chunking for output snapshot number %d. Chunk size was %d.\n",
                                            snap_idx, (int32_t) chunk_dims[0]);
            
            // Create the dataset
            hid_t dataset_id = H5Dcreate2(file_id, full_field_name, save_info->prop_h5types[i], 
                                           dataspace_id, H5P_DEFAULT, prop, H5P_DEFAULT);
            CHECK_STATUS_AND_RETURN_ON_FAIL(dataset_id, (int32_t) dataset_id,
                                            "Could not create the '%s' dataset.\n", full_field_name);
            
            // Set metadata attributes
            CREATE_STRING_ATTRIBUTE(dataset_id, "Description", save_info->prop_descriptions[i], MAX_STRING_LEN);
            CREATE_STRING_ATTRIBUTE(dataset_id, "Units", save_info->prop_units[i], MAX_STRING_LEN);
            
            // Close resources
            status = H5Dclose(dataset_id);
            CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                            "Failed to close field number %d for output snapshot number %d\n"
                                            "The dataset ID was %d\n", i, snap_idx,
                                            (int32_t) dataset_id);
            
            status = H5Pclose(prop);
            CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                            "Failed to close the property list for output snapshot number %d.\n", snap_idx);
            
            status = H5Sclose(dataspace_id);
            CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                            "Failed to close the dataspace for output snapshot number %d.\n", snap_idx);
        }
    }
    
    // Initialize buffer management
    save_info->buffer_size = NUM_GALS_PER_BUFFER;
    save_info->num_gals_in_buffer = calloc(run_params->simulation.NumSnapOutputs, 
                                          sizeof(save_info->num_gals_in_buffer[0]));
    
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->num_gals_in_buffer,
                                     "Failed to allocate %d elements of size %zu for save_info->num_gals_in_buffer", 
                                     run_params->simulation.NumSnapOutputs,
                                     sizeof(save_info->num_gals_in_buffer[0]));
    
    // Also set buffer fields in save_info_base for compatibility
    save_info_base->buffer_size = save_info->buffer_size;
    save_info_base->num_gals_in_buffer = save_info->num_gals_in_buffer;
    
    // Initialize total galaxies counter
    save_info->tot_ngals = calloc(run_params->simulation.NumSnapOutputs, sizeof(save_info->tot_ngals[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->tot_ngals,
                                     "Failed to allocate %d elements of size %zu for save_info->tot_ngals", 
                                     run_params->simulation.NumSnapOutputs,
                                     sizeof(save_info->tot_ngals[0]));
    
    // Also set tot_ngals in save_info_base for compatibility
    save_info_base->tot_ngals = save_info->tot_ngals;
    
    // Allocate forest_ngals array
    save_info->forest_ngals = calloc(run_params->simulation.NumSnapOutputs, sizeof(save_info->forest_ngals[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->forest_ngals,
                                     "Failed to allocate %d elements of size %zu for save_info->forest_ngals", 
                                     run_params->simulation.NumSnapOutputs,
                                     sizeof(save_info->forest_ngals[0]));
    
    // Also set forest_ngals in save_info_base for compatibility  
    save_info_base->forest_ngals = save_info->forest_ngals;
    
    // Allocate property buffers array
    save_info->property_buffers = calloc(run_params->simulation.NumSnapOutputs, 
                                        sizeof(save_info->property_buffers[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->property_buffers,
                                     "Failed to allocate %d elements of size %zu for save_info->property_buffers", 
                                     run_params->simulation.NumSnapOutputs,
                                     sizeof(save_info->property_buffers[0]));
    
    // Allocate forest_ngals arrays for each snapshot
    const int32_t max_forests = 100000; // Reasonable default
    for (int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        save_info->forest_ngals[snap_idx] = calloc(max_forests, sizeof(save_info->forest_ngals[0][0]));
        CHECK_POINTER_AND_RETURN_ON_NULL(save_info->forest_ngals[snap_idx],
                                         "Failed to allocate %d elements for forest_ngals[%d]", 
                                         max_forests, snap_idx);
    }
    
    // Allocate property buffers for each snapshot
    for (int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        status = allocate_all_output_properties(save_info, snap_idx);
        if (status != EXIT_SUCCESS) {
            LOG_ERROR("Failed to allocate property buffers for snapshot %d", snap_idx);
            // Clean up previously allocated buffers
            for (int32_t i = 0; i < snap_idx; i++) {
                free_all_output_properties(save_info, i);
            }
            free(save_info->property_buffers);
            free(save_info->tot_ngals);
            free(save_info->num_gals_in_buffer);
            free(save_info->group_ids);
            free_property_discovery(save_info);
            free(save_info);
            // Note: io_handler field removed as part of unified I/O interface cleanup
            return status;
        }
    }
    
    return EXIT_SUCCESS;
}

// Helper function to clean up property discovery resources
void free_property_discovery(struct hdf5_save_info *save_info) {
    if (save_info == NULL) return;
    
    // Free property IDs
    if (save_info->prop_ids != NULL) {
        free(save_info->prop_ids);
        save_info->prop_ids = NULL;
    }
    
    // Free property names
    if (save_info->prop_names != NULL) {
        for (int i = 0; i < save_info->num_properties; i++) {
            if (save_info->prop_names[i] != NULL) {
                free(save_info->prop_names[i]);
            }
        }
        free(save_info->prop_names);
        save_info->prop_names = NULL;
    }
    
    // Free property units
    if (save_info->prop_units != NULL) {
        for (int i = 0; i < save_info->num_properties; i++) {
            if (save_info->prop_units[i] != NULL) {
                free(save_info->prop_units[i]);
            }
        }
        free(save_info->prop_units);
        save_info->prop_units = NULL;
    }
    
    // Free property descriptions
    if (save_info->prop_descriptions != NULL) {
        for (int i = 0; i < save_info->num_properties; i++) {
            if (save_info->prop_descriptions[i] != NULL) {
                free(save_info->prop_descriptions[i]);
            }
        }
        free(save_info->prop_descriptions);
        save_info->prop_descriptions = NULL;
    }
    
    // Free property HDF5 types
    if (save_info->prop_h5types != NULL) {
        free(save_info->prop_h5types);
        save_info->prop_h5types = NULL;
    }
    
    // Free property flags
    if (save_info->is_core_prop != NULL) {
        free(save_info->is_core_prop);
        save_info->is_core_prop = NULL;
    }
    
    // Free name_output_fields if allocated
    if (save_info->name_output_fields != NULL) {
        for (int i = 0; i < save_info->num_output_fields; i++) {
            if (save_info->name_output_fields[i] != NULL) {
                free(save_info->name_output_fields[i]);
            }
        }
        free(save_info->name_output_fields);
        save_info->name_output_fields = NULL;
    }
    
    // Free field_dtypes if allocated
    if (save_info->field_dtypes != NULL) {
        free(save_info->field_dtypes);
        save_info->field_dtypes = NULL;
    }
}
