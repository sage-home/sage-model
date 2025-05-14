#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // working with c++ compiler //

#include "../core/core_allvars.h"
#include "../core/core_properties.h"

/* 
 * Dynamic Property Buffer Structure
 * This replaces the static HDF5_GALAXY_OUTPUT to support runtime property discovery
 */
struct property_buffer_info {
    char *name;             // Property name
    char *description;      // Property description
    char *units;            // Property units
    void *data;             // Buffer for the property data
    hid_t h5_dtype;         // HDF5 datatype
    property_id_t prop_id;  // Property ID for lookup
    bool is_core_prop;      // Flag indicating if this is a core property
    int index;              // Index in the original list of fields
};

/**
 * @brief Save information structure for HDF5 output
 * 
 * Replaces the previous static structure with a dynamic property-based system
 */
struct hdf5_save_info {
    hid_t file_id;                  // HDF5 file ID
    hid_t *group_ids;               // HDF5 group IDs for each snapshot
    int32_t num_output_fields;      // Number of fields to output
    
    /* Buffer management */
    int32_t buffer_size;            // Number of galaxies per buffer
    int32_t *num_gals_in_buffer;    // Current number of galaxies in buffer
    int64_t *tot_ngals;             // Total galaxies written per snapshot
    
    /* Dynamic property information */
    struct property_buffer_info **property_buffers; // [snap_idx][prop_idx]
    int num_properties;            // Total properties to output
    
    /* Property system information */
    property_id_t *prop_ids;       // Array of property IDs
    char **prop_names;             // Array of property names
    char **prop_units;             // Array of property units
    char **prop_descriptions;      // Array of property descriptions
    hid_t *prop_h5types;           // Array of HDF5 datatypes
    bool *is_core_prop;            // Array of flags indicating if properties are core
    
    /* Backward compatibility */
    char **name_output_fields;     // Array of field names for backward compatibility
    hsize_t *field_dtypes;         // Array of field datatypes for backward compatibility
};
    
// Proto-Types //
extern int32_t initialize_hdf5_galaxy_files(const int filenr, struct save_info *save_info, const struct params *run_params);

extern int32_t save_hdf5_galaxies(const int64_t task_forestnr, const int32_t num_gals, struct forest_info *forest_info,
                                  struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                                  struct save_info *save_info, const struct params *run_params);

extern int32_t finalize_hdf5_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info,
                                          const struct params *run_params);

extern int32_t create_hdf5_master_file(const struct params *run_params);

#ifdef __cplusplus
}
#endif
