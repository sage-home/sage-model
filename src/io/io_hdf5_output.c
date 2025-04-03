#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hdf5.h>

#include "../core/core_allvars.h"
#include "../core/core_utils.h"
#include "../core/core_mymalloc.h"
#include "../core/core_logging.h"
#include "../core/core_galaxy_extensions.h"
#include "../physics/model_misc.h"
#include "io_interface.h"
#include "io_hdf5_utils.h"
#include "io_property_serialization.h"
#include "io_hdf5_output.h"

#ifdef USE_SAGE_IN_MCMC_MODE
#define NUM_OUTPUT_FIELDS 2
#else
#define NUM_OUTPUT_FIELDS 54
#endif

/**
 * @brief Magic marker to identify the HDF5 output format with extended properties
 */
#define HDF5_OUTPUT_MAGIC 0x53414745  // "SAGE" in ASCII hex

/**
 * @brief Version identifier for the HDF5 output format
 */
#define HDF5_OUTPUT_VERSION 1

/**
 * Static handler definition
 */
// Forward declarations of interface functions
int hdf5_output_initialize(const char *filename, struct params *params, void **format_data);
int hdf5_output_write_galaxies(struct GALAXY *galaxies, int ngals, 
                              struct save_info *save_info, void *format_data);
int hdf5_output_cleanup(void *format_data);
int hdf5_output_close_handles(void *format_data);
int hdf5_output_get_handle_count(void *format_data);

// Static handler definition
static struct io_interface hdf5_output_handler = {
    .name = "HDF5 Output",
    .version = "1.0",
    .format_id = IO_FORMAT_HDF5_OUTPUT,
    .capabilities = IO_CAP_CHUNKED_WRITE | IO_CAP_EXTENDED_PROPS | IO_CAP_METADATA_ATTRS,
    
    .initialize = hdf5_output_initialize,
    .read_forest = NULL,  // Output format doesn't support reading forests
    .write_galaxies = hdf5_output_write_galaxies,
    .cleanup = hdf5_output_cleanup,
    
    .close_open_handles = hdf5_output_close_handles,
    .get_open_handle_count = hdf5_output_get_handle_count,
    
    .last_error = IO_ERROR_NONE,
    .error_message = {0}
};

/**
 * @brief Format-specific utility functions
 */
static int generate_field_metadata(char (*field_names)[MAX_STRING_LEN], 
                                  char (*field_descriptions)[MAX_STRING_LEN],
                                  char (*field_units)[MAX_STRING_LEN], 
                                  hsize_t *field_dtypes);
static int create_hdf5_groups(struct hdf5_output_data *format_data, struct params *params);
static int write_header(hid_t file_id, struct params *params);
static int allocate_galaxy_buffers(struct hdf5_output_data *format_data);
static int flush_galaxy_buffer(struct hdf5_output_data *format_data, int snap_idx);
static int close_all_groups(struct hdf5_output_data *format_data);

/**
 * @brief Initialize the HDF5 output handler
 *
 * @return 0 on success, non-zero on failure
 */
int io_hdf5_output_init(void) {
    return io_register_handler(&hdf5_output_handler);
}

/**
 * @brief Get the HDF5 output handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
struct io_interface *io_get_hdf5_output_handler(void) {
    return io_get_handler_by_id(IO_FORMAT_HDF5_OUTPUT);
}

/**
 * @brief Get the extension for HDF5 output files
 *
 * @return File extension string
 */
const char *io_hdf5_output_get_extension(void) {
    return ".hdf5";
}

/**
 * @brief Initialize the HDF5 output handler
 *
 * @param filename Base output filename
 * @param params Simulation parameters
 * @param format_data Pointer to format-specific data pointer
 * @return 0 on success, non-zero on failure
 */
int hdf5_output_initialize(const char *filename, struct params *params, void **format_data) {
    if (filename == NULL || params == NULL || format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL parameters passed to hdf5_output_initialize");
        return -1;
    }
    
    // Initialize HDF5 handle tracking system if not already initialized
    int ret = hdf5_tracking_init();
    if (ret != 0) {
        io_set_error(IO_ERROR_UNKNOWN, "Failed to initialize HDF5 handle tracking system");
        return -1;
    }
    
    // Allocate format-specific data
    struct hdf5_output_data *data = calloc(1, sizeof(struct hdf5_output_data));
    if (data == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate HDF5 output format data");
        return -1;
    }
    
    // Initialize format-specific data
    data->num_snapshots = params->simulation.NumSnapOutputs;
    data->file_id = -1;  // Invalid handle
    data->extended_props_enabled = (global_extension_registry != NULL && 
                                   global_extension_registry->num_extensions > 0);
    
    // Allocate snapshot group IDs array
    data->snapshot_group_ids = calloc(data->num_snapshots, sizeof(hid_t));
    if (data->snapshot_group_ids == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate snapshot group IDs array");
        free(data);
        return -1;
    }
    
    // Initialize all group IDs to -1 (invalid)
    for (int i = 0; i < data->num_snapshots; i++) {
        data->snapshot_group_ids[i] = -1;
    }
    
    // Allocate arrays for galaxy counts
    data->total_galaxies = calloc(data->num_snapshots, sizeof(int64_t));
    if (data->total_galaxies == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate total galaxies array");
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Create the HDF5 file
    char full_filename[MAX_STRING_LEN * 3];  // Extra space for path, name, and extension
    snprintf(full_filename, sizeof(full_filename), "%s/%s.hdf5", 
             params->io.OutputDir, params->io.FileNameGalaxies);
    
    // Create the file with proper tracking
    data->file_id = H5Fcreate(full_filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (data->file_id < 0) {
        io_set_error(IO_ERROR_FILE_NOT_FOUND, "Failed to create HDF5 output file");
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Track the file handle
    ret = HDF5_TRACK_FILE(data->file_id);
    if (ret != 0) {
        io_set_error(IO_ERROR_RESOURCE_LIMIT, "Failed to track HDF5 file handle");
        H5Fclose(data->file_id);
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Generate field metadata
    data->field_names = calloc(NUM_OUTPUT_FIELDS, sizeof(char[MAX_STRING_LEN]));
    data->field_descriptions = calloc(NUM_OUTPUT_FIELDS, sizeof(char[MAX_STRING_LEN]));
    data->field_units = calloc(NUM_OUTPUT_FIELDS, sizeof(char[MAX_STRING_LEN]));
    data->field_dtypes = calloc(NUM_OUTPUT_FIELDS, sizeof(hsize_t));
    
    if (data->field_names == NULL || data->field_descriptions == NULL || 
        data->field_units == NULL || data->field_dtypes == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate field metadata arrays");
        if (data->field_names) free(data->field_names);
        if (data->field_descriptions) free(data->field_descriptions);
        if (data->field_units) free(data->field_units);
        if (data->field_dtypes) free(data->field_dtypes);
        H5Fclose(data->file_id);
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Generate field metadata
    ret = generate_field_metadata(data->field_names, data->field_descriptions, 
                                data->field_units, data->field_dtypes);
    if (ret != 0) {
        io_set_error(IO_ERROR_UNKNOWN, "Failed to generate field metadata");
        free(data->field_dtypes);
        free(data->field_units);
        free(data->field_descriptions);
        free(data->field_names);
        H5Fclose(data->file_id);
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Number of standard fields
    data->num_fields = NUM_OUTPUT_FIELDS;
    
    // Create snapshot groups
    ret = create_hdf5_groups(data, params);
    if (ret != 0) {
        io_set_error(IO_ERROR_UNKNOWN, "Failed to create HDF5 groups");
        free(data->field_dtypes);
        free(data->field_units);
        free(data->field_descriptions);
        free(data->field_names);
        H5Fclose(data->file_id);
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Write header information
    ret = write_header(data->file_id, params);
    if (ret != 0) {
        io_set_error(IO_ERROR_UNKNOWN, "Failed to write HDF5 header");
        close_all_groups(data);
        free(data->field_dtypes);
        free(data->field_units);
        free(data->field_descriptions);
        free(data->field_names);
        H5Fclose(data->file_id);
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // If extended properties are enabled, initialize the serialization context
    if (data->extended_props_enabled) {
        ret = property_serialization_init(&data->prop_ctx, SERIALIZE_EXPLICIT);
        if (ret != 0) {
            io_set_error(IO_ERROR_UNKNOWN, "Failed to initialize property serialization context");
            close_all_groups(data);
            free(data->field_dtypes);
            free(data->field_units);
            free(data->field_descriptions);
            free(data->field_names);
            H5Fclose(data->file_id);
            free(data->total_galaxies);
            free(data->snapshot_group_ids);
            free(data);
            return -1;
        }
        
        ret = property_serialization_add_properties(&data->prop_ctx);
        if (ret != 0) {
            io_set_error(IO_ERROR_UNKNOWN, "Failed to add properties to serialization context");
            property_serialization_cleanup(&data->prop_ctx);
            close_all_groups(data);
            free(data->field_dtypes);
            free(data->field_units);
            free(data->field_descriptions);
            free(data->field_names);
            H5Fclose(data->file_id);
            free(data->total_galaxies);
            free(data->snapshot_group_ids);
            free(data);
            return -1;
        }
    }
    
    // Allocate snapshot buffers
    data->snapshot_buffers = calloc(data->num_snapshots, sizeof(*(data->snapshot_buffers)));
    if (data->snapshot_buffers == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate snapshot buffers");
        if (data->extended_props_enabled) {
            property_serialization_cleanup(&data->prop_ctx);
        }
        close_all_groups(data);
        free(data->field_dtypes);
        free(data->field_units);
        free(data->field_descriptions);
        free(data->field_names);
        H5Fclose(data->file_id);
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Initialize snapshot buffers
    for (int i = 0; i < data->num_snapshots; i++) {
        data->snapshot_buffers[i].buffer_size = HDF5_GALAXY_BUFFER_SIZE;
        data->snapshot_buffers[i].galaxies_in_buffer = 0;
        data->snapshot_buffers[i].num_properties = data->num_fields;
        
        if (data->extended_props_enabled) {
            data->snapshot_buffers[i].num_properties += data->prop_ctx.num_properties;
        }
    }
    
    // Allocate property buffers
    ret = allocate_galaxy_buffers(data);
    if (ret != 0) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate property buffers");
        free(data->snapshot_buffers);
        if (data->extended_props_enabled) {
            property_serialization_cleanup(&data->prop_ctx);
        }
        close_all_groups(data);
        free(data->field_dtypes);
        free(data->field_units);
        free(data->field_descriptions);
        free(data->field_names);
        H5Fclose(data->file_id);
        free(data->total_galaxies);
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    *format_data = data;
    return 0;
}

/**
 * @brief Write galaxy data to HDF5 output files
 *
 * @param galaxies Array of galaxies to write
 * @param ngals Number of galaxies
 * @param save_info Save information (file handles, etc.)
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
int hdf5_output_write_galaxies(struct GALAXY *galaxies, int ngals, 
                              struct save_info *save_info, void *format_data) {
    // Mark parameters as used to avoid warnings
    (void)galaxies;
    (void)ngals;
    (void)save_info;
    (void)format_data;
    
    // This function will be implemented in the next step
    return 0;
}

/**
 * @brief Clean up the HDF5 output handler
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
int hdf5_output_cleanup(void *format_data) {
    if (format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to hdf5_output_cleanup");
        return -1;
    }
    
    struct hdf5_output_data *data = (struct hdf5_output_data *)format_data;
    
    // Close all open handles
    int ret = hdf5_output_close_handles(format_data);
    if (ret != 0) {
        // Log error but continue cleanup
        fprintf(stderr, "Error closing HDF5 handles during cleanup: %d\n", ret);
    }
    
    // Free all allocated memory
    
    // Property buffers
    if (data->snapshot_buffers != NULL) {
        for (int i = 0; i < data->num_snapshots; i++) {
            if (data->snapshot_buffers[i].property_buffers != NULL) {
                for (int j = 0; j < data->snapshot_buffers[i].num_properties; j++) {
                    if (data->snapshot_buffers[i].property_buffers[j] != NULL) {
                        free(data->snapshot_buffers[i].property_buffers[j]);
                    }
                }
                free(data->snapshot_buffers[i].property_buffers);
            }
        }
        free(data->snapshot_buffers);
    }
    
    // Metadata arrays
    if (data->field_names != NULL) {
        free(data->field_names);
    }
    
    if (data->field_descriptions != NULL) {
        free(data->field_descriptions);
    }
    
    if (data->field_units != NULL) {
        free(data->field_units);
    }
    
    if (data->field_dtypes != NULL) {
        free(data->field_dtypes);
    }
    
    // Galaxy counts
    if (data->total_galaxies != NULL) {
        free(data->total_galaxies);
    }
    
    if (data->galaxies_per_forest != NULL) {
        for (int i = 0; i < data->num_snapshots; i++) {
            if (data->galaxies_per_forest[i] != NULL) {
                free(data->galaxies_per_forest[i]);
            }
        }
        free(data->galaxies_per_forest);
    }
    
    // Group IDs
    if (data->snapshot_group_ids != NULL) {
        free(data->snapshot_group_ids);
    }
    
    // Cleanup property serialization context if enabled
    if (data->extended_props_enabled) {
        property_serialization_cleanup(&data->prop_ctx);
    }
    
    // Free the format data itself
    free(data);
    
    return 0;
}

/**
 * @brief Close all open HDF5 handles
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
int hdf5_output_close_handles(void *format_data) {
    if (format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to hdf5_output_close_handles");
        return -1;
    }
    
    struct hdf5_output_data *data = (struct hdf5_output_data *)format_data;
    
    // Close all groups
    int ret = close_all_groups(data);
    if (ret != 0) {
        // Log error but continue cleanup
        fprintf(stderr, "Error closing HDF5 groups: %d\n", ret);
    }
    
    // Close file
    if (data->file_id >= 0) {
        herr_t status = H5Fclose(data->file_id);
        if (status < 0) {
            // Log error but continue cleanup
            fprintf(stderr, "Error closing HDF5 file: %d\n", (int)status);
            ret = -1;
        }
        data->file_id = -1;
    }
    
    return ret;
}

/**
 * @brief Get the number of open HDF5 handles
 *
 * @param format_data Format-specific data
 * @return Number of open HDF5 handles, -1 on error
 */
int hdf5_output_get_handle_count(void *format_data) {
    if (format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to hdf5_output_get_handle_count");
        return -1;
    }
    
    // Use the HDF5 utility function to get the handle count
    return hdf5_get_open_handle_count();
}

/**
 * @brief Generate field metadata
 * 
 * This function sets up the names, descriptions, units, and data types for all standard fields.
 * 
 * @param field_names Array of field names
 * @param field_descriptions Array of field descriptions
 * @param field_units Array of field units
 * @param field_dtypes Array of HDF5 data types
 * @return 0 on success, non-zero on failure
 */
static int generate_field_metadata(char (*field_names)[MAX_STRING_LEN], 
                                  char (*field_descriptions)[MAX_STRING_LEN],
                                  char (*field_units)[MAX_STRING_LEN], 
                                  hsize_t *field_dtypes) {
    // Initialize field index
    int field_idx = 0;
    
    // Define metadata for required standard fields
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Type", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Galaxy type: 0=central, 1=satellite", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "GalaxyIndex", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Unique ID for this galaxy", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT64;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "CentralGalaxyIndex", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Unique ID for the central galaxy of this galaxy's FoF group", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT64;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "SAGEHaloIndex", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Index of the dark matter halo in the simulation", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "SAGETreeIndex", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Index of the dark matter tree in the simulation", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "SimulationFOFHaloIndex", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Index of the dark matter FoF group in the simulation", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "mergeType", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Merger type: 0=none, 1=minor, 2=major", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "mergeIntoID", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Galaxy ID that this galaxy merged into", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "mergeIntoSnapNum", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Snapshot where this galaxy merged into another", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "dT", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Time between snapshots", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "Myr/h", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    // Physical properties
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Pos_x", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "X coordinate", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "cMpc/h", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Pos_y", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Y coordinate", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "cMpc/h", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Pos_z", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Z coordinate", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "cMpc/h", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Vel_x", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "X velocity", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "km/s", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Vel_y", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Y velocity", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "km/s", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Vel_z", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Z velocity", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "km/s", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    // Mass properties
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "Mvir", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Virial mass of halo", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "1e10 Msun/h", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    if (field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "CentralMvir", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Virial mass of central halo", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "1e10 Msun/h", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
        field_idx++;
    }
    
    // Add more fields as needed
    
    // Fill remaining fields with placeholders if we have less than NUM_OUTPUT_FIELDS
    for (; field_idx < NUM_OUTPUT_FIELDS; field_idx++) {
        snprintf(field_names[field_idx], MAX_STRING_LEN, "Field%d", field_idx);
        snprintf(field_descriptions[field_idx], MAX_STRING_LEN, "Description for Field%d", field_idx);
        snprintf(field_units[field_idx], MAX_STRING_LEN, "units");
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;  // Default to float for testing
    }
    
    return 0;
}

/**
 * @brief Create HDF5 groups for snapshots
 * 
 * @param format_data Format-specific data
 * @param params Simulation parameters
 * @return 0 on success, non-zero on failure
 */
static int create_hdf5_groups(struct hdf5_output_data *format_data, struct params *params) {
    char group_name[MAX_STRING_LEN];
    
    // Create snapshot groups
    for (int i = 0; i < format_data->num_snapshots; i++) {
        double redshift = params->simulation.ZZ[params->simulation.ListOutputSnaps[i]];
        
        // Create group name based on redshift
        snprintf(group_name, MAX_STRING_LEN, "Snap_z%.3f", redshift);
        
        // Create the group
        format_data->snapshot_group_ids[i] = H5Gcreate(format_data->file_id, group_name, 
                                                     H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (format_data->snapshot_group_ids[i] < 0) {
            return -1;
        }
        
        // Track the group handle
        int ret = HDF5_TRACK_GROUP(format_data->snapshot_group_ids[i]);
        if (ret != 0) {
            return -1;
        }
        
        // Create extended properties group if needed
        if (format_data->extended_props_enabled) {
            char ext_group_name[MAX_STRING_LEN];
            snprintf(ext_group_name, MAX_STRING_LEN, "%s/ExtendedProperties", group_name);
            
            hid_t ext_group_id = H5Gcreate(format_data->file_id, ext_group_name, 
                                          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            if (ext_group_id < 0) {
                return -1;
            }
            
            // Track the group handle
            ret = HDF5_TRACK_GROUP(ext_group_id);
            if (ret != 0) {
                return -1;
            }
            
            // Close the extended properties group
            herr_t status = H5Gclose(ext_group_id);
            if (status < 0) {
                return -1;
            }
            
            // Untrack the handle
            hdf5_untrack_handle(ext_group_id);
        }
    }
    
    return 0;
}

/**
 * @brief Write header information to the HDF5 file
 * 
 * @param file_id HDF5 file handle
 * @param params Simulation parameters
 * @return 0 on success, non-zero on failure
 */
static int write_header(hid_t file_id, struct params *params) {
    (void)params;  // Mark parameter as used to avoid warning
    // Create the Header group
    hid_t header_group_id = H5Gcreate(file_id, "Header", 
                                     H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (header_group_id < 0) {
        return -1;
    }
    
    // Track the group handle
    int ret = HDF5_TRACK_GROUP(header_group_id);
    if (ret != 0) {
        H5Gclose(header_group_id);
        return -1;
    }
    
    // Add version information
    char version_str[MAX_STRING_LEN];
    snprintf(version_str, MAX_STRING_LEN, "%d", HDF5_OUTPUT_VERSION);
    
    hid_t strtype = H5Tcopy(H5T_C_S1);
    H5Tset_size(strtype, strlen(version_str) + 1);
    
    hid_t space_id = H5Screate(H5S_SCALAR);
    
    hid_t attr_id = H5Acreate(header_group_id, "Version", strtype, space_id, 
                             H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0) {
        H5Sclose(space_id);
        H5Tclose(strtype);
        H5Gclose(header_group_id);
        return -1;
    }
    
    herr_t status = H5Awrite(attr_id, strtype, version_str);
    if (status < 0) {
        H5Aclose(attr_id);
        H5Sclose(space_id);
        H5Tclose(strtype);
        H5Gclose(header_group_id);
        return -1;
    }
    
    // Close the attribute and dataspace
    H5Aclose(attr_id);
    H5Sclose(space_id);
    H5Tclose(strtype);
    
    // Close the header group
    status = H5Gclose(header_group_id);
    if (status < 0) {
        return -1;
    }
    
    // Untrack the handle
    hdf5_untrack_handle(header_group_id);
    
    return 0;
}

/**
 * @brief Allocate galaxy buffers
 * 
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int allocate_galaxy_buffers(struct hdf5_output_data *format_data) {
    // For each snapshot, allocate property buffers
    for (int i = 0; i < format_data->num_snapshots; i++) {
        int num_props = format_data->snapshot_buffers[i].num_properties;
        int buffer_size = format_data->snapshot_buffers[i].buffer_size;
        
        // Allocate array of property buffer pointers
        format_data->snapshot_buffers[i].property_buffers = calloc(num_props, sizeof(void*));
        if (format_data->snapshot_buffers[i].property_buffers == NULL) {
            return -1;
        }
        
        // Allocate each property buffer
        for (int j = 0; j < num_props; j++) {
            // Determine property size
            size_t element_size = sizeof(float);  // Default to float size
            
            // For standard fields, use the data type size
            if (j < format_data->num_fields) {
                hid_t dtype = format_data->field_dtypes[j];
                element_size = H5Tget_size(dtype);
            }
            // For extended properties, use the property size from the context
            else if (format_data->extended_props_enabled) {
                int prop_idx = j - format_data->num_fields;
                if (prop_idx < format_data->prop_ctx.num_properties) {
                    element_size = format_data->prop_ctx.properties[prop_idx].size;
                }
            }
            
            // Allocate the buffer
            format_data->snapshot_buffers[i].property_buffers[j] = calloc(buffer_size, element_size);
            if (format_data->snapshot_buffers[i].property_buffers[j] == NULL) {
                // Free already allocated buffers
                for (int k = 0; k < j; k++) {
                    free(format_data->snapshot_buffers[i].property_buffers[k]);
                }
                free(format_data->snapshot_buffers[i].property_buffers);
                return -1;
            }
        }
    }
    
    return 0;
}

/**
 * @brief Flush galaxy buffer to disk
 * 
 * @param format_data Format-specific data
 * @param snap_idx Snapshot index
 * @return 0 on success, non-zero on failure
 */
static int flush_galaxy_buffer(struct hdf5_output_data *format_data, int snap_idx) {
    // Mark parameters as used to avoid warnings
    (void)format_data;
    (void)snap_idx;
    
    // This will be implemented in a future step
    return 0;
}

/**
 * @brief Close all snapshot groups
 * 
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int close_all_groups(struct hdf5_output_data *format_data) {
    int result = 0;
    
    if (format_data == NULL) {
        return -1;
    }
    
    // Close all snapshot groups
    for (int i = 0; i < format_data->num_snapshots; i++) {
        if (format_data->snapshot_group_ids[i] >= 0) {
            herr_t status = H5Gclose(format_data->snapshot_group_ids[i]);
            if (status < 0) {
                // Log error but continue closing other groups
                fprintf(stderr, "Error closing snapshot group %d: %d\n", i, (int)status);
                result = -1;
            }
            hdf5_untrack_handle(format_data->snapshot_group_ids[i]);
            format_data->snapshot_group_ids[i] = -1;
        }
    }
    
    return result;
}
