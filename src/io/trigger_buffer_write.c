#include "save_gals_hdf5_internal.h"

// Property-based buffer write function - restored HDF5 writing functionality
int32_t trigger_buffer_write(const int32_t snap_idx, const int32_t num_to_write, 
                           const int64_t num_already_written,
                           struct save_info *save_info_base, const struct params *run_params)
{
    // Get HDF5-specific save info from the buffer_output_gals field
    struct hdf5_save_info *save_info = (struct hdf5_save_info *)save_info_base->buffer_output_gals;
    if (save_info == NULL) {
        // Fallback - just update counters if no HDF5 save info available
        save_info_base->num_gals_in_buffer[snap_idx] = 0;
        save_info_base->tot_ngals[snap_idx] += num_to_write;
        return EXIT_SUCCESS;
    }
    
    herr_t status;
    
    // Set up dimensions for writing
    hsize_t dims_extend[1] = {(hsize_t)num_to_write};      // Length to extend the dataset by
    hsize_t old_dims[1] = {(hsize_t)num_already_written};  // Previous length of the dataset
    hsize_t new_dims[1] = {old_dims[0] + dims_extend[0]};  // New length
    
    // Write each property to file
    for (int i = 0; i < save_info->num_properties; i++) {
        struct property_buffer_info *buffer = &save_info->property_buffers[snap_idx][i];
        
        // Build the field name
        char full_field_name[2*MAX_STRING_LEN];
        snprintf(full_field_name, 2*MAX_STRING_LEN - 1, "Snap_%d/%s", 
                run_params->simulation.ListOutputSnaps[snap_idx], buffer->name);
        
        // Open the dataset
        hid_t dataset_id = H5Dopen2(save_info->file_id, full_field_name, H5P_DEFAULT);
        if (dataset_id < 0) {
            fprintf(stderr, "Could not access the %s dataset\n", buffer->name);
            return (int32_t)dataset_id;
        }
        
        // Extend the dataset dimensions
        status = H5Dset_extent(dataset_id, new_dims);
        if (status < 0) {
            fprintf(stderr, "Could not resize the dimensions of the %s dataset for output snapshot %d.\n", 
                    buffer->name, snap_idx);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        // Get the file space
        hid_t filespace = H5Dget_space(dataset_id);
        if (filespace < 0) {
            fprintf(stderr, "Could not retrieve the dataspace of the %s dataset for output snapshot %d.\n",
                    buffer->name, snap_idx);
            H5Dclose(dataset_id);
            return (int32_t)filespace;
        }
        
        // Select the hyperslab
        status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL);
        if (status < 0) {
            fprintf(stderr, "Could not select a hyperslab region for the %s dataset.\n", buffer->name);
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        // Create memory space
        hid_t memspace = H5Screate_simple(1, dims_extend, NULL);
        if (memspace < 0) {
            fprintf(stderr, "Could not create a new dataspace for the %s dataset.\n", buffer->name);
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return (int32_t)memspace;
        }
        
        // Write the data
        status = H5Dwrite(dataset_id, buffer->h5_dtype, memspace, filespace, H5P_DEFAULT, buffer->data);
        if (status < 0) {
            fprintf(stderr, "Could not write the dataset for the %s field for output snapshot %d.\n",
                    buffer->name, snap_idx);
            H5Sclose(memspace);
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        // Clean up
        H5Sclose(memspace);
        H5Sclose(filespace);
        H5Dclose(dataset_id);
    }
    
    // We've performed a write, so reset buffer counter and update totals
    save_info_base->num_gals_in_buffer[snap_idx] = 0;
    save_info_base->tot_ngals[snap_idx] += num_to_write;
    
    return EXIT_SUCCESS;
}