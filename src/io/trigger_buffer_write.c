#include "save_gals_hdf5_internal.h"

// Property-agnostic buffer write function
int32_t trigger_buffer_write(const int32_t snap_idx, const int32_t num_to_write, 
                           const int64_t num_already_written,
                           struct save_info *save_info_base, const struct params *run_params)
{
    herr_t status;
    struct hdf5_save_info *save_info = (struct hdf5_save_info *)save_info_base->io_handler.format_data;
    
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
        
        // Verify datatype size is correct
        hid_t h5_dtype = H5Dget_type(dataset_id);
        size_t h5_size = H5Tget_size(h5_dtype);
        size_t expected_size = 0;
        
        if (buffer->h5_dtype == H5T_NATIVE_FLOAT) expected_size = sizeof(float);
        else if (buffer->h5_dtype == H5T_NATIVE_DOUBLE) expected_size = sizeof(double);
        else if (buffer->h5_dtype == H5T_NATIVE_INT) expected_size = sizeof(int32_t);
        else if (buffer->h5_dtype == H5T_NATIVE_LLONG) expected_size = sizeof(int64_t);
        
        if (h5_size != expected_size) {
            fprintf(stderr, "Error while writing field %s\n", buffer->name);
            fprintf(stderr, "The HDF5 dataset has size %zu bytes but the struct element has size = %zu bytes\n", 
                    h5_size, expected_size);
            fprintf(stderr, "Perhaps the size of the struct item needs to be updated?\n");
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Extend the dataset dimensions
        status = H5Dset_extent(dataset_id, new_dims);
        if (status < 0) {
            fprintf(stderr, "Could not resize the dimensions of the %s dataset for output snapshot %d.\n"
                    "The dataset ID value is %d. The new dimension values were %d\n", 
                    buffer->name, snap_idx, (int32_t)dataset_id, (int32_t)new_dims[0]);
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        // Get the file space
        hid_t filespace = H5Dget_space(dataset_id);
        if (filespace < 0) {
            fprintf(stderr, "Could not retrieve the dataspace of the %s dataset for output snapshot %d.\n"
                    "The dataset ID value is %d.\n", buffer->name, snap_idx, (int32_t)dataset_id);
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return (int32_t)filespace;
        }
        
        // Select the hyperslab
        status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL);
        if (status < 0) {
            fprintf(stderr, "Could not select a hyperslab region to add to the filespace of the %s dataset for output snapshot %d.\n"
                    "The dataset ID value is %d.\n"
                    "The old dimensions were %d and we attempted to extend this by %d elements.\n", 
                    buffer->name, snap_idx, (int32_t)dataset_id, 
                    (int32_t)old_dims[0], (int32_t)dims_extend[0]);
            H5Sclose(filespace);
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        // Create memory space
        hid_t memspace = H5Screate_simple(1, dims_extend, NULL);
        if (memspace < 0) {
            fprintf(stderr, "Could not create a new dataspace for the %s dataset for output snapshot %d.\n"
                    "The length of the dataspace we attempted to created was %d.\n", 
                    buffer->name, snap_idx, (int32_t)dims_extend[0]);
            H5Sclose(filespace);
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return (int32_t)memspace;
        }
        
        // Write the data
        status = H5Dwrite(dataset_id, buffer->h5_dtype, memspace, filespace, H5P_DEFAULT, buffer->data);
        if (status < 0) {
            fprintf(stderr, "Could not write the dataset for the %s field for output snapshot %d.\n"
                    "The dataset ID value is %d.\n"
                    "The old dimensions were %d and we attempting to extend (and write to) this by %d elements.\n", 
                    buffer->name, snap_idx, (int32_t)dataset_id, 
                    (int32_t)old_dims[0], (int32_t)dims_extend[0]);
            H5Sclose(memspace);
            H5Sclose(filespace);
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        // Clean up
        status = H5Sclose(memspace);
        if (status < 0) {
            fprintf(stderr, "Could not close the memory space for the %s dataset for output snapshot %d.\n"
                    "The dataset ID value is %d.\n", buffer->name, snap_idx, (int32_t)dataset_id);
            H5Sclose(filespace);
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        status = H5Sclose(filespace);
        if (status < 0) {
            fprintf(stderr, "Could not close the filespace for the %s dataset for output snapshot %d.\n"
                    "The dataset ID value is %d.\n", buffer->name, snap_idx, (int32_t)dataset_id);
            H5Tclose(h5_dtype);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        status = H5Tclose(h5_dtype);
        if (status < 0) {
            fprintf(stderr, "Error: Failed to close the datatype for the %s dataset for output snapshot %d.\n"
                    "The dataset ID value is %d.\n", buffer->name, snap_idx, (int32_t)dataset_id);
            H5Dclose(dataset_id);
            return (int32_t)status;
        }
        
        status = H5Dclose(dataset_id);
        if (status < 0) {
            fprintf(stderr, "Failed to close field %s for output snapshot number %d\n"
                    "The dataset ID was %d\n", buffer->name, snap_idx, (int32_t)dataset_id);
            return (int32_t)status;
        }
    }
    
    // We've performed a write, so future galaxies will overwrite the old data.
    save_info->num_gals_in_buffer[snap_idx] = 0;
    save_info->tot_ngals[snap_idx] += num_to_write;
    
    return EXIT_SUCCESS;
}
