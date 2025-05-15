#include "save_gals_hdf5_internal.h"

// Property-based field metadata generation
int32_t generate_field_metadata(struct hdf5_save_info *save_info) {
    // Field metadata is now derived directly from property metadata
    
    // Property IDs, names, descriptions, units, and datatypes are already filled
    // during discover_output_properties(), so we just need to make them visible
    // to external code that needs this data
    
    // Copy names to name_output_fields for backward compatibility
    save_info->name_output_fields = malloc(save_info->num_properties * sizeof(char*));
    if (save_info->name_output_fields == NULL) {
        fprintf(stderr, "Failed to allocate memory for field names array\n");
        return -1;
    }
    
    for (int i = 0; i < save_info->num_properties; i++) {
        save_info->name_output_fields[i] = strdup(save_info->prop_names[i]);
        if (save_info->name_output_fields[i] == NULL) {
            fprintf(stderr, "Failed to allocate memory for field name %d\n", i);
            // Clean up previously allocated strings
            for (int j = 0; j < i; j++) {
                free(save_info->name_output_fields[j]);
            }
            free(save_info->name_output_fields);
            save_info->name_output_fields = NULL;
            return -1;
        }
    }
    
    // Set num_output_fields for backward compatibility
    save_info->num_output_fields = save_info->num_properties;
    
    // Allocate field_dtypes for backward compatibility
    save_info->field_dtypes = malloc(save_info->num_properties * sizeof(hsize_t));
    if (save_info->field_dtypes == NULL) {
        fprintf(stderr, "Failed to allocate memory for field datatypes array\n");
        // Clean up
        for (int i = 0; i < save_info->num_properties; i++) {
            free(save_info->name_output_fields[i]);
        }
        free(save_info->name_output_fields);
        save_info->name_output_fields = NULL;
        return -1;
    }
    
    // Copy datatypes
    for (int i = 0; i < save_info->num_properties; i++) {
        save_info->field_dtypes[i] = save_info->prop_h5types[i];
    }
    
    return EXIT_SUCCESS;
}
