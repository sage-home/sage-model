#include "save_gals_hdf5_internal.h"

// Updated finalization function for HDF5 files using property system
int32_t finalize_hdf5_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info_base,
                                 const struct params *run_params)
{
    struct hdf5_save_info *save_info = (struct hdf5_save_info *)save_info_base->format_data;
    hid_t group_id;
    herr_t h5_status;
    
    // Create TreeInfo group
    group_id = H5Gcreate(save_info->file_id, "/TreeInfo", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                    "Failed to create the TreeInfo group.\nThe file ID was %d\n",
                                    (int32_t) save_info->file_id);

    h5_status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                    "Failed to close the /TreeInfo group."
                                    "The group ID was %d.\n", (int32_t) group_id);
    
    // Process each snapshot
    for (int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        // Create TreeInfo/Snap_N group
        char field_name[MAX_STRING_LEN];
        char description[MAX_STRING_LEN];
        char unit[MAX_STRING_LEN];

        snprintf(field_name, MAX_STRING_LEN - 1, "/TreeInfo/Snap_%d", run_params->simulation.ListOutputSnaps[snap_idx]);
        group_id = H5Gcreate2(save_info->file_id, field_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                        "Failed to create the '%s' group.\n"
                                        "The file ID was %d\n", field_name, (int32_t) save_info->file_id);

        h5_status = H5Gclose(group_id);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to close '%s' group."
                                        "The group ID was %d.\n", field_name, (int32_t) group_id);

        // Write any remaining galaxies in the buffer
        int32_t num_gals_to_write = save_info->num_gals_in_buffer[snap_idx];

        if (num_gals_to_write > 0) {
            h5_status = trigger_buffer_write(snap_idx, num_gals_to_write,
                                             save_info->tot_ngals[snap_idx], save_info_base, run_params);
            if (h5_status != EXIT_SUCCESS) {
                return h5_status;
            }
        }
        
        // Write attribute showing how many galaxies we wrote for this snapshot
        CREATE_SINGLE_ATTRIBUTE(save_info->group_ids[snap_idx], "num_gals", save_info->tot_ngals[snap_idx], H5T_NATIVE_LLONG);

        // Write NumGalsPerTreePerSnap dataset
        snprintf(field_name, MAX_STRING_LEN - 1, "TreeInfo/Snap_%d/NumGalsPerTreePerSnap", 
                 run_params->simulation.ListOutputSnaps[snap_idx]);
        snprintf(description, MAX_STRING_LEN - 1, "The number of galaxies per tree at this snapshot.");
        snprintf(unit, MAX_STRING_LEN - 1, "Unitless");

        hsize_t dims[1];
        dims[0] = forest_info->nforests_this_task;

        hid_t dataspace_id = H5Screate_simple(1, dims, NULL);
        CHECK_STATUS_AND_RETURN_ON_FAIL(dataspace_id, (int32_t) dataspace_id,
                                        "Could not create a dataspace for the number of galaxies per tree.\n"
                                        "The dimensions of the dataspace was %d\n", (int32_t) dims[0]);

        hid_t dataset_id = H5Dcreate2(save_info->file_id, field_name, H5T_NATIVE_INT, dataspace_id, 
                                      H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(dataset_id, (int32_t) dataset_id,
                                        "Could not create a dataset for the number of galaxies per tree at snapshot = %d.\n"
                                        "The dimensions of the dataset was %d\nThe file id was %d.\n",
                                        snap_idx, (int32_t) dims[0], (int32_t) save_info->file_id);

        h5_status = H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, 
                             save_info_base->forest_ngals[snap_idx]);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to write a dataset for the number of galaxies per tree at snapshot = %d.\n"
                                        "The dimensions of the dataset was %d.\nThe file ID was %d.\n"
                                        "The dataset ID was %d.",
                                        snap_idx, (int32_t) dims[0], (int32_t) save_info->file_id,
                                        (int32_t) dataset_id);

        h5_status = H5Dclose(dataset_id);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to close the dataset for the number of galaxies per tree.\n"
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n."
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) save_info->file_id,
                                        (int32_t) dataset_id);

        h5_status = H5Sclose(dataspace_id);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to close the dataspace for the number of galaxies per tree.\n"
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n."
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) save_info->file_id,
                                        (int32_t) dataset_id);
    }
    
    // Add specific attributes to TreeInfo
    group_id = H5Gopen2(save_info->file_id, "/TreeInfo", H5P_DEFAULT);

    // Add ID generation scheme attributes
    CREATE_SINGLE_ATTRIBUTE(group_id, "FileNr_Mulfac", run_params->runtime.FileNr_Mulfac, H5T_NATIVE_LLONG);
    CREATE_SINGLE_ATTRIBUTE(group_id, "ForestNr_Mulfac", run_params->runtime.ForestNr_Mulfac, H5T_NATIVE_LLONG);

    h5_status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                    "Failed to close the TreeInfo group."
                                    "The group ID was %d.\n", (int32_t) group_id);

    // Create Header group and write header
    group_id = H5Gcreate(save_info->file_id, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                    "Failed to create the Header group.\nThe file ID was %d\n",
                                    (int32_t) save_info->file_id);

    int status = write_header(save_info->file_id, forest_info, run_params);
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the Header group."
                                    "The group ID was %d.\n", (int32_t) group_id);

    // Close all HDF5 handles and free resources
    for (int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        // Close the group
        status = H5Gclose(save_info->group_ids[snap_idx]);
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                        "Failed to close the group for output snapshot number %d\n"
                                        "The group ID was %d\n", snap_idx, (int32_t) save_info->group_ids[snap_idx]);
    }

    // Close the file
    status = H5Fclose(save_info->file_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the HDF5 file.\nThe file ID was %d\n",
                                    (int32_t) save_info->file_id);

    // Free allocated resources
    free(save_info->group_ids);
    
    // Free property resources
    free_property_discovery(save_info);
    
    // Free property buffers
    for (int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        free_all_output_properties(save_info, snap_idx);
    }
    
    // Free arrays
    free(save_info->property_buffers);
    free(save_info->num_gals_in_buffer);
    free(save_info->tot_ngals);
    
    // Free the save_info struct itself
    free(save_info);
    save_info_base->format_data = NULL;

    return EXIT_SUCCESS;
}
