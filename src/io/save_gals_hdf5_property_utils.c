#include "save_gals_hdf5_internal.h"

// Function to discover properties for output based on their metadata
int discover_output_properties(struct hdf5_save_info *save_info) {
    // Ensure TotGalaxyProperties is properly linked from core_properties.c
    extern const int32_t TotGalaxyProperties;
    
    // First count output properties that have output=true
    int num_props = 0;
    for (property_id_t id = 0; id < TotGalaxyProperties; id++) {
        const property_meta_t *meta = get_property_meta(id);
        if (meta == NULL) continue;
        
        // Check if property should be output (based on output flag)
        if (meta->output) {
            num_props++;
        }
    }
    
    // Allocate memory for property info
    save_info->num_properties = num_props;
    save_info->prop_ids = malloc(num_props * sizeof(property_id_t));
    save_info->prop_names = malloc(num_props * sizeof(char*));
    save_info->prop_units = malloc(num_props * sizeof(char*));
    save_info->prop_descriptions = malloc(num_props * sizeof(char*));
    save_info->prop_h5types = malloc(num_props * sizeof(hid_t));
    save_info->is_core_prop = malloc(num_props * sizeof(bool));
    
    if (!save_info->prop_ids || !save_info->prop_names || !save_info->prop_units || 
        !save_info->prop_descriptions || !save_info->prop_h5types || !save_info->is_core_prop) {
        fprintf(stderr, "Failed to allocate memory for property discovery\n");
        return -1;
    }
    
    // Fill property info
    int idx = 0;
    for (property_id_t id = 0; id < TotGalaxyProperties; id++) {
        const property_meta_t *meta = get_property_meta(id);
        if (meta == NULL) continue;
        
        if (meta->output) {
            save_info->prop_ids[idx] = id;
            save_info->prop_names[idx] = strdup(meta->name);
            save_info->prop_units[idx] = strdup(meta->units);
            save_info->prop_descriptions[idx] = strdup(meta->description);
            save_info->is_core_prop[idx] = is_core_property(id);
            
            // Determine HDF5 datatype
            if (strcmp(meta->type, "float") == 0 || strcmp(meta->type, "float[]") == 0) {
                save_info->prop_h5types[idx] = H5T_NATIVE_FLOAT;
            } else if (strcmp(meta->type, "double") == 0 || strcmp(meta->type, "double[]") == 0) {
                save_info->prop_h5types[idx] = H5T_NATIVE_DOUBLE;
            } else if (strcmp(meta->type, "int32_t") == 0 || strcmp(meta->type, "int32_t[]") == 0 || 
                       strcmp(meta->type, "int") == 0) {
                save_info->prop_h5types[idx] = H5T_NATIVE_INT;
            } else if (strcmp(meta->type, "uint64_t") == 0 || strcmp(meta->type, "uint64_t[]") == 0) {
                save_info->prop_h5types[idx] = H5T_NATIVE_LLONG;
            } else if (strcmp(meta->type, "long long") == 0) {
                save_info->prop_h5types[idx] = H5T_NATIVE_LLONG;
            } else {
                // Default to float for unknown types
                fprintf(stderr, "Warning: Unknown property type '%s' for property '%s'. Defaulting to float.\n",
                        meta->type, meta->name);
                save_info->prop_h5types[idx] = H5T_NATIVE_FLOAT;
            }
            
            idx++;
        }
    }
    
    return 0;
}

// Allocate memory for a single property buffer
int allocate_output_property(struct hdf5_save_info *save_info, int snap_idx, 
                            int prop_idx, int buffer_size) {
    property_id_t prop_id = save_info->prop_ids[prop_idx];
    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL) {
        fprintf(stderr, "Failed to get metadata for property ID %d\n", prop_id);
        return -1;
    }
    
    // Create property buffer info if not exists
    if (save_info->property_buffers[snap_idx] == NULL) {
        save_info->property_buffers[snap_idx] = calloc(save_info->num_properties, 
                                                      sizeof(struct property_buffer_info));
        if (save_info->property_buffers[snap_idx] == NULL) {
            fprintf(stderr, "Failed to allocate property buffers for snapshot %d\n", snap_idx);
            return -1;
        }
    }
    
    // Set property buffer metadata
    struct property_buffer_info *buffer = &save_info->property_buffers[snap_idx][prop_idx];
    buffer->name = strdup(save_info->prop_names[prop_idx]);
    buffer->description = strdup(save_info->prop_descriptions[prop_idx]);
    buffer->units = strdup(save_info->prop_units[prop_idx]);
    buffer->h5_dtype = save_info->prop_h5types[prop_idx];
    buffer->prop_id = prop_id;
    buffer->is_core_prop = save_info->is_core_prop[prop_idx];
    buffer->index = prop_idx;
    
    // Allocate memory based on datatype
    size_t elem_size;
    if (buffer->h5_dtype == H5T_NATIVE_FLOAT) {
        elem_size = sizeof(float);
    } else if (buffer->h5_dtype == H5T_NATIVE_DOUBLE) {
        elem_size = sizeof(double);
    } else if (buffer->h5_dtype == H5T_NATIVE_INT) {
        elem_size = sizeof(int32_t);
    } else if (buffer->h5_dtype == H5T_NATIVE_LLONG) {
        elem_size = sizeof(int64_t);
    } else {
        fprintf(stderr, "Unknown HDF5 datatype for property %s\n", buffer->name);
        return -1;
    }
    
    // Allocate buffer
    buffer->data = malloc(buffer_size * elem_size);
    if (buffer->data == NULL) {
        fprintf(stderr, "Failed to allocate %d elements for property %s\n", 
                buffer_size, buffer->name);
        return -1;
    }
    
    return 0;
}

// Free memory for a single property buffer
int free_output_property(struct hdf5_save_info *save_info, int snap_idx, int prop_idx) {
    if (save_info->property_buffers[snap_idx] == NULL) {
        return 0; // Nothing to free
    }
    
    struct property_buffer_info *buffer = &save_info->property_buffers[snap_idx][prop_idx];
    
    // Free buffer data
    if (buffer->data != NULL) {
        free(buffer->data);
        buffer->data = NULL;
    }
    
    // Free strings
    if (buffer->name != NULL) {
        free(buffer->name);
        buffer->name = NULL;
    }
    
    if (buffer->description != NULL) {
        free(buffer->description);
        buffer->description = NULL;
    }
    
    if (buffer->units != NULL) {
        free(buffer->units);
        buffer->units = NULL;
    }
    
    return 0;
}

// Allocate all property buffers for a snapshot
int allocate_all_output_properties(struct hdf5_save_info *save_info, int snap_idx) {
    // Allocate memory for all properties
    for (int i = 0; i < save_info->num_properties; i++) {
        int status = allocate_output_property(save_info, snap_idx, i, save_info->buffer_size);
        if (status != 0) {
            fprintf(stderr, "Failed to allocate buffer for property index %d\n", i);
            return status;
        }
    }
    return 0;
}

// Free all property buffers for a snapshot
int free_all_output_properties(struct hdf5_save_info *save_info, int snap_idx) {
    if (save_info->property_buffers[snap_idx] == NULL) {
        return 0; // Nothing to free
    }
    
    // Free memory for all properties
    for (int i = 0; i < save_info->num_properties; i++) {
        free_output_property(save_info, snap_idx, i);
    }
    
    // Free the array itself
    free(save_info->property_buffers[snap_idx]);
    save_info->property_buffers[snap_idx] = NULL;
    
    return 0;
}

// Note: Special property handling for position, velocity and spin components
// is now handled directly in prepare_galaxy_for_hdf5_output.c
