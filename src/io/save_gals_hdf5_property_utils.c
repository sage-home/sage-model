#include "save_gals_hdf5_internal.h"
#include "../core/core_logging.h"

// Helper function to extract base type from array syntax like "float[3]" -> "float"
static const char* extract_base_type(const char* type_str) {
    static char base_type_buffer[64];
    const char* bracket = strchr(type_str, '[');
    if (bracket) {
        size_t len = bracket - type_str;
        if (len < sizeof(base_type_buffer)) {
            strncpy(base_type_buffer, type_str, len);
            base_type_buffer[len] = '\0';
            return base_type_buffer;
        }
    }
    return type_str; // Return original if no brackets or too long
}

// Function to discover properties for output based on their metadata
int discover_output_properties(struct hdf5_save_info *save_info) {
    // Use the generated property metadata from core_properties.h
    extern property_meta_t PROPERTY_META[PROP_COUNT];
    
    // First count output properties, considering array decomposition
    int num_props = 0;
    for (int i = 0; i < PROP_COUNT; i++) {
        const property_meta_t *meta = &PROPERTY_META[i];
        if (!meta->output) {
            continue;
        }
        
        // Check if this is a 3-element array that should be decomposed
        if (meta->is_array && meta->array_dimension == 3) {
            // Special handling for 3-element arrays like Pos, Vel, Spin
            if (strcmp(meta->name, "Pos") == 0 || 
                strcmp(meta->name, "Vel") == 0 || 
                strcmp(meta->name, "Spin") == 0) {
                num_props += 3; // Add 3 components (x, y, z)
                continue;
            }
        }
        
        // Regular property (not decomposed)
        num_props++;
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
        LOG_ERROR("Failed to allocate memory for property discovery");
        return -1;
    }
    
    // Fill property info
    int idx = 0;
    for (int i = 0; i < PROP_COUNT; i++) {
        const property_meta_t *meta = &PROPERTY_META[i];
        if (!meta->output) {
            continue;
        }
        
        // Check if this is a 3-element array that should be decomposed
        if (meta->is_array && meta->array_dimension == 3) {
            if (strcmp(meta->name, "Pos") == 0 || 
                strcmp(meta->name, "Vel") == 0 || 
                strcmp(meta->name, "Spin") == 0) {
                
                // Add components x, y, z
                const char components[] = {'x', 'y', 'z'};
                for (int dim = 0; dim < 3; dim++) {
                    save_info->prop_ids[idx] = (property_id_t)i;
                    
                    // Create component name (e.g., "Posx", "Vely", "Spinz")
                    char *comp_name = malloc(64);
                    snprintf(comp_name, 64, "%s%c", meta->name, components[dim]);
                    save_info->prop_names[idx] = comp_name;
                    
                    save_info->prop_units[idx] = strdup(meta->units);
                    
                    // Create component description
                    char *comp_desc = malloc(256);
                    snprintf(comp_desc, 256, "%s component %c", meta->description, components[dim]);
                    save_info->prop_descriptions[idx] = comp_desc;
                    
                    save_info->is_core_prop[idx] = true; // These are core properties
                    
                    // Extract base type for HDF5 datatype
                    const char* base_type = extract_base_type(meta->type);
                    if (strcmp(base_type, "float") == 0) {
                        save_info->prop_h5types[idx] = H5T_NATIVE_FLOAT;
                    } else if (strcmp(base_type, "double") == 0) {
                        save_info->prop_h5types[idx] = H5T_NATIVE_DOUBLE;
                    } else {
                        // Default to float for arrays
                        save_info->prop_h5types[idx] = H5T_NATIVE_FLOAT;
                    }
                    
                    idx++;
                }
                continue;
            }
        }
        
        // Regular property (not decomposed)
        save_info->prop_ids[idx] = (property_id_t)i;
        save_info->prop_names[idx] = strdup(meta->name);
        save_info->prop_units[idx] = strdup(meta->units);
        save_info->prop_descriptions[idx] = strdup(meta->description);
        save_info->is_core_prop[idx] = true; // All core properties in this context
        
        // Determine HDF5 datatype - extract base type from array syntax
        const char* base_type = extract_base_type(meta->type);
        if (strcmp(base_type, "float") == 0) {
            save_info->prop_h5types[idx] = H5T_NATIVE_FLOAT;
        } else if (strcmp(base_type, "double") == 0) {
            save_info->prop_h5types[idx] = H5T_NATIVE_DOUBLE;
        } else if (strcmp(base_type, "int32_t") == 0 || strcmp(base_type, "int") == 0) {
            save_info->prop_h5types[idx] = H5T_NATIVE_INT;
        } else if (strcmp(base_type, "uint64_t") == 0) {
            save_info->prop_h5types[idx] = H5T_NATIVE_ULLONG;
        } else if (strcmp(base_type, "long long") == 0) {
            save_info->prop_h5types[idx] = H5T_NATIVE_LLONG;
        } else {
            // Default to float for unknown types
            LOG_WARNING("Unknown property type '%s' for property '%s'. Defaulting to float",
                       meta->type, meta->name);
            save_info->prop_h5types[idx] = H5T_NATIVE_FLOAT;
        }
        
        idx++;
    }
    
    return 0;
}

// Allocate memory for a single property buffer
int allocate_output_property(struct hdf5_save_info *save_info, int snap_idx, 
                            int prop_idx, int buffer_size) {
    property_id_t prop_id = save_info->prop_ids[prop_idx];
    const property_meta_t *meta = get_property_meta(prop_id);
    if (meta == NULL) {
        LOG_ERROR("Failed to get metadata for property ID %d", prop_id);
        return -1;
    }
    
    // Ensure property_buffers array is allocated (should be done by allocate_all_output_properties)
    if (save_info->property_buffers[snap_idx] == NULL) {
        LOG_ERROR("property_buffers[%d] not allocated before calling allocate_output_property", snap_idx);
        return -1;
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
    } else if (buffer->h5_dtype == H5T_NATIVE_ULLONG) {
        elem_size = sizeof(uint64_t);
    } else {
        LOG_ERROR("Unknown HDF5 datatype for property %s", buffer->name);
        return -1;
    }
    
    // Allocate buffer
    buffer->data = malloc(buffer_size * elem_size);
    if (buffer->data == NULL) {
        LOG_ERROR("Failed to allocate %d elements for property %s", 
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
    // First allocate the array of property_buffer_info structs for this snapshot
    save_info->property_buffers[snap_idx] = calloc(save_info->num_properties, sizeof(struct property_buffer_info));
    if (save_info->property_buffers[snap_idx] == NULL) {
        LOG_ERROR("Failed to allocate property_buffer_info array for snapshot %d", snap_idx);
        return -1;
    }
    
    // Allocate data buffers - metadata will be initialized when needed
    for (int i = 0; i < save_info->num_properties; i++) {
        int status = allocate_output_property(save_info, snap_idx, i, save_info->buffer_size);
        if (status != 0) {
            LOG_ERROR("Failed to allocate buffer for property index %d", i);
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
