#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <hdf5.h>

#include "../core/core_allvars.h"
#include "../core/core_utils.h"
#include "../core/core_mymalloc.h"
#include "../core/core_logging.h"
#include "../core/core_galaxy_extensions.h"
#include "../core/core_properties.h"

#ifdef CORE_ONLY
#include "../physics/placeholder_hdf5_macros.h"
#else
#include "../physics/legacy/model_misc.h"
#endif
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
    
    // Allocate arrays for redshifts and galaxy counts
    data->redshifts = calloc(data->num_snapshots, sizeof(double));
    if (data->redshifts == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate redshifts array");
        free(data->snapshot_group_ids);
        free(data);
        return -1;
    }
    
    // Initialize redshifts
    for (int i = 0; i < data->num_snapshots; i++) {
        data->redshifts[i] = params->simulation.ZZ[params->simulation.ListOutputSnaps[i]];
    }
    
    data->total_galaxies = calloc(data->num_snapshots, sizeof(int64_t));
    if (data->total_galaxies == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate total galaxies array");
        free(data->redshifts);
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
    if (galaxies == NULL || ngals <= 0 || save_info == NULL || format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Invalid parameters passed to hdf5_output_write_galaxies");
        return -1;
    }
    
    struct hdf5_output_data *data = (struct hdf5_output_data *)format_data;
    
    // Process each galaxy
    for (int i = 0; i < ngals; i++) {
        // Get snapshot index for this galaxy
        int snap_idx = -1;
        for (int j = 0; j < data->num_snapshots; j++) {
            // Simple match by snapshot number
            if (galaxies[i].SnapNum == j) {
                snap_idx = j;
                break;
            }
        }
        
        // Skip galaxies that aren't at an output snapshot
        if (snap_idx < 0) {
            continue;
        }
        
        // Get the buffer for this snapshot
        int buffer_idx = data->snapshot_buffers[snap_idx].galaxies_in_buffer;
        
        // Check if buffer is full - if so, flush it
        if (buffer_idx >= data->snapshot_buffers[snap_idx].buffer_size) {
            int ret = flush_galaxy_buffer(data, snap_idx);
            if (ret != 0) {
                char error_message[MAX_STRING_LEN];
                snprintf(error_message, MAX_STRING_LEN, "Failed to flush galaxy buffer for snapshot %d", snap_idx);
                io_set_error(IO_ERROR_UNKNOWN, error_message);
                return -1;
            }
            buffer_idx = 0;  // Reset buffer index after flush
        }
        
        // Copy standard galaxy properties to the buffer
        for (int j = 0; j < data->num_fields; j++) {
            // Get destination buffer
            void *dest = data->snapshot_buffers[snap_idx].property_buffers[j];
            if (dest == NULL) {
                io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL property buffer found");
                return -1;
            }
            
            // Determine field type and copy appropriate data
            hid_t dtype = data->field_dtypes[j];
            size_t dtype_size = H5Tget_size(dtype);
            
            // Position in buffer based on buffer index and data type size
            char *dest_ptr = (char *)dest + (buffer_idx * dtype_size);
            
            // Copy appropriate field based on field name
            const char *field_name = data->field_names[j];
            struct GALAXY *galaxy = &galaxies[i];
            
            // Synchronize direct fields to properties for access through macros
            if (galaxy->properties == NULL) {
                LOG_WARNING("Galaxy %d has NULL properties pointer, skipping property access", i);
                continue;
            }
            
            // Special handling for derived or component fields
            if (strcmp(field_name, "SAGETreeIndex") == 0) {
                *(int32_t *)dest_ptr = 0;  // Default value - not stored in property system
            }
            // Handle position components
            else if (strcmp(field_name, "Pos_x") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Pos_ELEM(galaxy, 0);
            }
            else if (strcmp(field_name, "Pos_y") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Pos_ELEM(galaxy, 1);
            }
            else if (strcmp(field_name, "Pos_z") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Pos_ELEM(galaxy, 2);
            }
            // Handle velocity components
            else if (strcmp(field_name, "Vel_x") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Vel_ELEM(galaxy, 0);
            }
            else if (strcmp(field_name, "Vel_y") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Vel_ELEM(galaxy, 1);
            }
            else if (strcmp(field_name, "Vel_z") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Vel_ELEM(galaxy, 2);
            }
            // Handle other standard properties using property macros
            else if (strcmp(field_name, "Type") == 0) {
                *(int32_t *)dest_ptr = GALAXY_PROP_Type(galaxy);
            }
            else if (strcmp(field_name, "GalaxyIndex") == 0) {
                *(int64_t *)dest_ptr = GALAXY_PROP_GalaxyIndex(galaxy);
            }
            else if (strcmp(field_name, "CentralGalaxyIndex") == 0) {
                *(int64_t *)dest_ptr = GALAXY_PROP_CentralGalaxyIndex(galaxy);
            }
            else if (strcmp(field_name, "HaloNr") == 0 || strcmp(field_name, "SAGEHaloIndex") == 0) {
                *(int32_t *)dest_ptr = GALAXY_PROP_HaloNr(galaxy);
            }
            else if (strcmp(field_name, "MostBoundID") == 0 || strcmp(field_name, "SimulationFOFHaloIndex") == 0) {
                *(int64_t *)dest_ptr = GALAXY_PROP_MostBoundID(galaxy);
            }
            else if (strcmp(field_name, "mergeType") == 0) {
                *(int32_t *)dest_ptr = GALAXY_PROP_mergeType(galaxy);
            }
            else if (strcmp(field_name, "mergeIntoID") == 0) {
                *(int32_t *)dest_ptr = GALAXY_PROP_mergeIntoID(galaxy);
            }
            else if (strcmp(field_name, "mergeIntoSnapNum") == 0) {
                *(int32_t *)dest_ptr = GALAXY_PROP_mergeIntoSnapNum(galaxy);
            }
            else if (strcmp(field_name, "dT") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_dT(galaxy);
            }
            else if (strcmp(field_name, "Mvir") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Mvir(galaxy);
            }
            else if (strcmp(field_name, "CentralMvir") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_CentralMvir(galaxy);
            }
            else if (strcmp(field_name, "Rvir") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Rvir(galaxy);
            }
            else if (strcmp(field_name, "Vvir") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Vvir(galaxy);
            }
            else if (strcmp(field_name, "Vmax") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_Vmax(galaxy);
            }
            else if (strcmp(field_name, "ColdGas") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_ColdGas(galaxy);
            }
            else if (strcmp(field_name, "StellarMass") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_StellarMass(galaxy);
            }
            else if (strcmp(field_name, "BulgeMass") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_BulgeMass(galaxy);
            }
            else if (strcmp(field_name, "HotGas") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_HotGas(galaxy);
            }
            else if (strcmp(field_name, "EjectedMass") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_EjectedMass(galaxy);
            }
            else if (strcmp(field_name, "BlackHoleMass") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_BlackHoleMass(galaxy);
            }
            else if (strcmp(field_name, "DiskScaleRadius") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_DiskScaleRadius(galaxy);
            }
            else if (strcmp(field_name, "Cooling") == 0) {
                *(double *)dest_ptr = GALAXY_PROP_Cooling(galaxy);
            }
            else if (strcmp(field_name, "Heating") == 0) {
                *(double *)dest_ptr = GALAXY_PROP_Heating(galaxy);
            }
            else if (strcmp(field_name, "TimeOfLastMajorMerger") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_TimeOfLastMajorMerger(galaxy);
            }
            else if (strcmp(field_name, "TimeOfLastMinorMerger") == 0) {
                *(float *)dest_ptr = GALAXY_PROP_TimeOfLastMinorMerger(galaxy);
            }
            else {
                // For other fields, try to look up by property name
                property_id_t prop_id = get_property_id(field_name);
                
                if (prop_id != PROP_COUNT) {
                    // Property exists, extract it based on type
                    const property_meta_t *meta = &PROPERTY_META[prop_id];
                    
                    if (strcmp(meta->type, "int32_t") == 0) {
                        // For simplicity, we're not handling the full range of property types
                        // In a real implementation, you'd need proper macro generation for each type
                        LOG_WARNING("Property access for %s type not implemented", meta->type);
                        // Zero-fill as fallback
                        memset(dest_ptr, 0, dtype_size);
                    }
                    else if (strcmp(meta->type, "float") == 0) {
                        // For float type, we would need a generic accessor
                        LOG_WARNING("Generic property access for %s not implemented", field_name);
                        // Zero-fill as fallback
                        memset(dest_ptr, 0, dtype_size);
                    }
                    else {
                        // Other types not handled in this example
                        LOG_WARNING("Property access for %s type not implemented", meta->type);
                        // Zero-fill as fallback
                        memset(dest_ptr, 0, dtype_size);
                    }
                }
                else {
                    // Property not found, zero-fill
                    LOG_WARNING("Unknown property name: %s", field_name);
                    memset(dest_ptr, 0, dtype_size);
                }
            }
        }
        
        // Handle extended properties if enabled
        if (data->extended_props_enabled) {
            // Start after standard fields
            int base_idx = data->num_fields;
            
            // Get the size needed for all extended properties
            size_t ext_size = property_serialization_data_size(&data->prop_ctx);
            
            // Allocate temporary buffer for extended properties
            void *ext_buffer = malloc(ext_size);
            if (ext_buffer == NULL) {
                io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate extended property buffer");
                return -1;
            }
            
            // Serialize extended properties
            int ret = property_serialize_galaxy(&data->prop_ctx, &galaxies[i], ext_buffer);
            if (ret != 0) {
                free(ext_buffer);
                io_set_error(IO_ERROR_UNKNOWN, "Failed to serialize extended properties");
                return -1;
            }
            
            // Copy each property from serialized buffer to its respective property buffer
            for (int j = 0; j < data->prop_ctx.num_properties; j++) {
                // Destination buffer for this property
                void *dest = data->snapshot_buffers[snap_idx].property_buffers[base_idx + j];
                if (dest == NULL) {
                    free(ext_buffer);
                    io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL extended property buffer found");
                    return -1;
                }
                
                // Property metadata
                struct serialized_property_meta *prop = &data->prop_ctx.properties[j];
                
                // Copy property from ext_buffer to dest
                char *dest_ptr = (char *)dest + (buffer_idx * prop->size);
                char *src_ptr = (char *)ext_buffer + prop->offset;
                
                memcpy(dest_ptr, src_ptr, prop->size);
            }
            
            // Free temporary buffer
            free(ext_buffer);
        }
        
        // Increment galaxy counter for this snapshot buffer
        data->snapshot_buffers[snap_idx].galaxies_in_buffer++;
        data->total_galaxies[snap_idx]++;
    }
    
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
    
    // Galaxy counts and redshifts
    if (data->total_galaxies != NULL) {
        free(data->total_galaxies);
    }
    
    if (data->redshifts != NULL) {
        free(data->redshifts);
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
    
    // Generate field metadata from property metadata
    for (int prop_id = 0; prop_id < PROP_COUNT; prop_id++) {
        const property_meta_t *meta = &PROPERTY_META[prop_id];
        
        // Skip properties that aren't marked for output
        if (!meta->output) {
            continue;
        }
        
        // Skip if we've reached our maximum field count
        if (field_idx >= NUM_OUTPUT_FIELDS) {
            LOG_WARNING("Too many properties marked for output (limit: %d), skipping %s", 
                       NUM_OUTPUT_FIELDS, meta->name);
            break;
        }
        
        // Handle array properties with special naming conventions
        if (meta->is_array && strcmp(meta->type, "float") == 0) {
            // For fixed-size arrays like Pos[3], create separate fields for each component
            if (meta->array_dimension > 0) {
                if (strcmp(meta->name, "Pos") == 0) {
                    // Add Pos_x, Pos_y, Pos_z as separate fields
                    for (int dim = 0; dim < meta->array_dimension && field_idx < NUM_OUTPUT_FIELDS; dim++) {
                        char component = 'x' + dim; // x, y, z
                        snprintf(field_names[field_idx], MAX_STRING_LEN, "%s_%c", meta->name, component);
                        snprintf(field_descriptions[field_idx], MAX_STRING_LEN, "%s component", &component);
                        strncpy(field_units[field_idx], meta->units, MAX_STRING_LEN-1);
                        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
                        field_idx++;
                    }
                } 
                else if (strcmp(meta->name, "Vel") == 0) {
                    // Add Vel_x, Vel_y, Vel_z as separate fields
                    for (int dim = 0; dim < meta->array_dimension && field_idx < NUM_OUTPUT_FIELDS; dim++) {
                        char component = 'x' + dim; // x, y, z
                        snprintf(field_names[field_idx], MAX_STRING_LEN, "%s_%c", meta->name, component);
                        snprintf(field_descriptions[field_idx], MAX_STRING_LEN, "%s component", &component);
                        strncpy(field_units[field_idx], meta->units, MAX_STRING_LEN-1);
                        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
                        field_idx++;
                    }
                }
                // Skip other fixed-size arrays for now - they'll be handled by extended properties
            }
            else {
                // Dynamic arrays handled via property serialization (extended properties)
                continue;
            }
        }
        else if (!meta->is_array) {
            // Standard scalar property
            strncpy(field_names[field_idx], meta->name, MAX_STRING_LEN-1);
            strncpy(field_descriptions[field_idx], meta->description, MAX_STRING_LEN-1);
            strncpy(field_units[field_idx], meta->units, MAX_STRING_LEN-1);
            
            // Set appropriate HDF5 datatype based on property type
            if (strcmp(meta->type, "int32_t") == 0) {
                field_dtypes[field_idx] = H5T_NATIVE_INT32;
            }
            else if (strcmp(meta->type, "int64_t") == 0 || strcmp(meta->type, "uint64_t") == 0) {
                field_dtypes[field_idx] = H5T_NATIVE_INT64;
            }
            else if (strcmp(meta->type, "float") == 0) {
                field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
            }
            else if (strcmp(meta->type, "double") == 0) {
                field_dtypes[field_idx] = H5T_NATIVE_DOUBLE;
            }
            else if (strcmp(meta->type, "long long") == 0) {
                field_dtypes[field_idx] = H5T_NATIVE_INT64;
            }
            else {
                // Default to float for unknown types
                field_dtypes[field_idx] = H5T_NATIVE_FLOAT;
                LOG_WARNING("Unknown property type '%s' for property '%s', defaulting to float", 
                           meta->type, meta->name);
            }
            
            field_idx++;
        }
    }
    
    // Make sure we have added at least some fields
    if (field_idx == 0) {
        LOG_ERROR("No properties marked for output in properties.yaml");
        return -1;
    }
    
    // Add specific fields that might not be in the property system but are required
    // for compatibility or analysis purposes
    
    // Check if SAGETreeIndex is already added
    bool has_tree_index = false;
    for (int i = 0; i < field_idx; i++) {
        if (strcmp(field_names[i], "SAGETreeIndex") == 0) {
            has_tree_index = true;
            break;
        }
    }
    
    if (!has_tree_index && field_idx < NUM_OUTPUT_FIELDS) {
        strncpy(field_names[field_idx], "SAGETreeIndex", MAX_STRING_LEN-1);
        strncpy(field_descriptions[field_idx], "Index of the dark matter tree in the simulation", MAX_STRING_LEN-1);
        strncpy(field_units[field_idx], "none", MAX_STRING_LEN-1);
        field_dtypes[field_idx] = H5T_NATIVE_INT32;
        field_idx++;
    }
    
    // Log field count
    LOG_INFO("Generated metadata for %d output fields from property system", field_idx);
    
    // Fill remaining fields with placeholders if we have less than NUM_OUTPUT_FIELDS
    for (; field_idx < NUM_OUTPUT_FIELDS; field_idx++) {
        snprintf(field_names[field_idx], MAX_STRING_LEN, "Field%d", field_idx);
        snprintf(field_descriptions[field_idx], MAX_STRING_LEN, "Description for Field%d", field_idx);
        snprintf(field_units[field_idx], MAX_STRING_LEN, "units");
        field_dtypes[field_idx] = H5T_NATIVE_FLOAT;  // Default to float for unused fields
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
    // Validate parameters
    if (format_data == NULL || snap_idx < 0 || snap_idx >= format_data->num_snapshots) {
        return -1;
    }
    
    // Verify there are galaxies to write
    int galaxies_to_write = format_data->snapshot_buffers[snap_idx].galaxies_in_buffer;
    if (galaxies_to_write <= 0) {
        return 0;  // Nothing to do
    }
    
    // Get base offset for writing (number of galaxies already written)
    hsize_t base_offset = format_data->total_galaxies[snap_idx] - galaxies_to_write;
    
    // Write standard fields
    for (int field_idx = 0; field_idx < format_data->num_fields; field_idx++) {
        // Construct the dataset path
        char dataset_path[MAX_STRING_LEN * 2];
        char group_name[MAX_STRING_LEN];
        
        // The group name should match the one used in create_hdf5_groups
        snprintf(group_name, MAX_STRING_LEN, "Snap_z%.3f", format_data->redshifts[snap_idx]);
        snprintf(dataset_path, sizeof(dataset_path), "%s/%s", 
                group_name, format_data->field_names[field_idx]);
        
        // Open the dataset
        hid_t dataset_id = H5Dopen2(format_data->file_id, dataset_path, H5P_DEFAULT);
        if (dataset_id < 0) {
            return -1;
        }
        
        // Track the dataset handle
        int ret = HDF5_TRACK_DATASET(dataset_id);
        if (ret != 0) {
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Get datatype
        hid_t dtype = format_data->field_dtypes[field_idx];
        
        // Get the current dimensions of the dataset
        hid_t filespace = H5Dget_space(dataset_id);
        if (filespace < 0) {
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Track the filespace handle
        ret = HDF5_TRACK_DATASPACE(filespace);
        if (ret != 0) {
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Get current dimensions
        hsize_t dims[1];
        H5Sget_simple_extent_dims(filespace, dims, NULL);
        
        // New dimensions after adding galaxies
        hsize_t new_dims[1] = {base_offset + galaxies_to_write};
        
        // Extend the dataset to accommodate new galaxies
        herr_t status = H5Dset_extent(dataset_id, new_dims);
        if (status < 0) {
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Get the new filespace
        H5Sclose(filespace);
        filespace = H5Dget_space(dataset_id);
        if (filespace < 0) {
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Select hyperslab for writing
        hsize_t start[1] = {base_offset};
        hsize_t count[1] = {galaxies_to_write};
        status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, start, NULL, count, NULL);
        if (status < 0) {
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Create dataspace for memory buffer
        hid_t memspace = H5Screate_simple(1, count, NULL);
        if (memspace < 0) {
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Track the memspace handle
        ret = HDF5_TRACK_DATASPACE(memspace);
        if (ret != 0) {
            H5Sclose(memspace);
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Write the data
        void *buffer = format_data->snapshot_buffers[snap_idx].property_buffers[field_idx];
        status = H5Dwrite(dataset_id, dtype, memspace, filespace, H5P_DEFAULT, buffer);
        if (status < 0) {
            H5Sclose(memspace);
            H5Sclose(filespace);
            H5Dclose(dataset_id);
            return -1;
        }
        
        // Clean up
        H5Sclose(memspace);
        hdf5_untrack_handle(memspace);
        
        H5Sclose(filespace);
        hdf5_untrack_handle(filespace);
        
        H5Dclose(dataset_id);
        hdf5_untrack_handle(dataset_id);
    }
    
    // Handle extended properties if enabled
    if (format_data->extended_props_enabled) {
        int base_idx = format_data->num_fields;
        
        for (int prop_idx = 0; prop_idx < format_data->prop_ctx.num_properties; prop_idx++) {
            // Get property metadata
            struct serialized_property_meta *prop = &format_data->prop_ctx.properties[prop_idx];
            
            // Construct dataset path
            char dataset_path[MAX_STRING_LEN * 3];
            char group_name[MAX_STRING_LEN];
            
            snprintf(group_name, MAX_STRING_LEN, "Snap_z%.3f", format_data->redshifts[snap_idx]);
            snprintf(dataset_path, sizeof(dataset_path), "%s/ExtendedProperties/%s", 
                    group_name, prop->name);
            
            // Check if the dataset exists, create if not
            htri_t exists = H5Lexists(format_data->file_id, dataset_path, H5P_DEFAULT);
            hid_t dataset_id;
            
            if (exists <= 0) {
                // Create property dataset
                hid_t space_id = H5Screate_simple(1, (hsize_t[]){0}, (hsize_t[]){H5S_UNLIMITED});
                if (space_id < 0) {
                    return -1;
                }
                
                // Create property for chunking (required for extendible datasets)
                hid_t prop_id = H5Pcreate(H5P_DATASET_CREATE);
                if (prop_id < 0) {
                    H5Sclose(space_id);
                    return -1;
                }
                
                // Set chunking
                hsize_t chunk_dims[1] = {1024};  // Choose appropriate chunk size
                herr_t status = H5Pset_chunk(prop_id, 1, chunk_dims);
                if (status < 0) {
                    H5Pclose(prop_id);
                    H5Sclose(space_id);
                    return -1;
                }
                
                // Determine appropriate HDF5 datatype
                hid_t dtype;
                switch (prop->type) {
                    case PROPERTY_TYPE_INT32:
                        dtype = H5T_NATIVE_INT32;
                        break;
                    case PROPERTY_TYPE_INT64:
                        dtype = H5T_NATIVE_INT64;
                        break;
                    case PROPERTY_TYPE_FLOAT:
                        dtype = H5T_NATIVE_FLOAT;
                        break;
                    case PROPERTY_TYPE_DOUBLE:
                        dtype = H5T_NATIVE_DOUBLE;
                        break;
                    case PROPERTY_TYPE_BOOL:
                        dtype = H5T_NATIVE_UINT8;  // Use uint8 for boolean
                        break;
                    default:
                        dtype = H5T_NATIVE_FLOAT;  // Default
                }
                
                // Create the dataset
                dataset_id = H5Dcreate2(format_data->file_id, dataset_path, dtype, space_id,
                                     H5P_DEFAULT, prop_id, H5P_DEFAULT);
                if (dataset_id < 0) {
                    H5Pclose(prop_id);
                    H5Sclose(space_id);
                    return -1;
                }
                
                // Add metadata attributes
                hid_t attr_space = H5Screate(H5S_SCALAR);
                hid_t str_type = H5Tcopy(H5T_C_S1);
                H5Tset_size(str_type, MAX_PROPERTY_DESCRIPTION);
                
                // Add description attribute
                hid_t desc_attr = H5Acreate2(dataset_id, "Description", str_type, attr_space,
                                          H5P_DEFAULT, H5P_DEFAULT);
                if (desc_attr >= 0) {
                    H5Awrite(desc_attr, str_type, prop->description);
                    H5Aclose(desc_attr);
                }
                
                // Add units attribute
                H5Tset_size(str_type, MAX_PROPERTY_UNITS);
                hid_t units_attr = H5Acreate2(dataset_id, "Units", str_type, attr_space,
                                           H5P_DEFAULT, H5P_DEFAULT);
                if (units_attr >= 0) {
                    H5Awrite(units_attr, str_type, prop->units);
                    H5Aclose(units_attr);
                }
                
                // Cleanup temporary objects
                H5Tclose(str_type);
                H5Sclose(attr_space);
                H5Pclose(prop_id);
                H5Sclose(space_id);
            }
            else {
                // Open existing dataset
                dataset_id = H5Dopen2(format_data->file_id, dataset_path, H5P_DEFAULT);
                if (dataset_id < 0) {
                    return -1;
                }
            }
            
            // Track the dataset handle
            int ret = HDF5_TRACK_DATASET(dataset_id);
            if (ret != 0) {
                H5Dclose(dataset_id);
                return -1;
            }
            
            // Get the current dimensions
            hid_t filespace = H5Dget_space(dataset_id);
            if (filespace < 0) {
                H5Dclose(dataset_id);
                return -1;
            }
            
            ret = HDF5_TRACK_DATASPACE(filespace);
            if (ret != 0) {
                H5Sclose(filespace);
                H5Dclose(dataset_id);
                return -1;
            }
            
            // Get current dimensions
            hsize_t dims[1];
            H5Sget_simple_extent_dims(filespace, dims, NULL);
            
            // New dimensions
            hsize_t new_dims[1] = {base_offset + galaxies_to_write};
            
            // Extend the dataset
            herr_t status = H5Dset_extent(dataset_id, new_dims);
            if (status < 0) {
                H5Sclose(filespace);
                H5Dclose(dataset_id);
                return -1;
            }
            
            // Get the new filespace
            H5Sclose(filespace);
            filespace = H5Dget_space(dataset_id);
            if (filespace < 0) {
                H5Dclose(dataset_id);
                return -1;
            }
            
            // Select hyperslab for writing
            hsize_t start[1] = {base_offset};
            hsize_t count[1] = {galaxies_to_write};
            status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, start, NULL, count, NULL);
            if (status < 0) {
                H5Sclose(filespace);
                H5Dclose(dataset_id);
                return -1;
            }
            
            // Create dataspace for memory buffer
            hid_t memspace = H5Screate_simple(1, count, NULL);
            if (memspace < 0) {
                H5Sclose(filespace);
                H5Dclose(dataset_id);
                return -1;
            }
            
            ret = HDF5_TRACK_DATASPACE(memspace);
            if (ret != 0) {
                H5Sclose(memspace);
                H5Sclose(filespace);
                H5Dclose(dataset_id);
                return -1;
            }
            
            // Get property buffer
            void *buffer = format_data->snapshot_buffers[snap_idx].property_buffers[base_idx + prop_idx];
            
            // Determine HDF5 datatype for writing
            hid_t dtype;
            switch (prop->type) {
                case PROPERTY_TYPE_INT32:
                    dtype = H5T_NATIVE_INT32;
                    break;
                case PROPERTY_TYPE_INT64:
                    dtype = H5T_NATIVE_INT64;
                    break;
                case PROPERTY_TYPE_FLOAT:
                    dtype = H5T_NATIVE_FLOAT;
                    break;
                case PROPERTY_TYPE_DOUBLE:
                    dtype = H5T_NATIVE_DOUBLE;
                    break;
                case PROPERTY_TYPE_BOOL:
                    dtype = H5T_NATIVE_UINT8;
                    break;
                default:
                    dtype = H5T_NATIVE_FLOAT;
            }
            
            // Write data
            status = H5Dwrite(dataset_id, dtype, memspace, filespace, H5P_DEFAULT, buffer);
            if (status < 0) {
                H5Sclose(memspace);
                H5Sclose(filespace);
                H5Dclose(dataset_id);
                return -1;
            }
            
            // Clean up
            H5Sclose(memspace);
            hdf5_untrack_handle(memspace);
            
            H5Sclose(filespace);
            hdf5_untrack_handle(filespace);
            
            H5Dclose(dataset_id);
            hdf5_untrack_handle(dataset_id);
        }
    }
    
    // Reset buffer counter
    format_data->snapshot_buffers[snap_idx].galaxies_in_buffer = 0;
    
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
