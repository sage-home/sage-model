#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../core/core_allvars.h"
#include "../core/core_utils.h"
#include "../core/core_mymalloc.h"
#include "../core/core_logging.h"
#include "../core/core_galaxy_extensions.h"
#include "../physics/legacy/model_misc.h"
#include "io_interface.h"
#include "io_endian_utils.h"
#include "io_property_serialization.h"
#include "io_binary_output.h"
#include "save_gals_binary.h"

#include "io_buffer_manager.h"

/**
 * @brief Magic marker to identify the binary output format with extended properties
 */
#define BINARY_OUTPUT_MAGIC 0x53414745  // "SAGE" in ASCII hex

/**
 * @brief Version identifier for the binary output format
 */
#define BINARY_OUTPUT_VERSION 1

/**
 * Static handler definition
 */
// Forward declarations of interface functions
int binary_output_initialize(const char *filename, struct params *params, void **format_data);
int binary_output_write_galaxies(struct GALAXY *galaxies, int ngals, 
                              struct save_info *save_info, void *format_data);
int binary_output_cleanup(void *format_data);
int binary_output_close_handles(void *format_data);
int binary_output_get_handle_count(void *format_data);

// Static handler definition
static struct io_interface binary_output_handler = {
    .name = "Binary Output",
    .version = "1.0",
    .format_id = IO_FORMAT_BINARY_OUTPUT,
    .capabilities = IO_CAP_APPEND | IO_CAP_EXTENDED_PROPS,
    
    .initialize = binary_output_initialize,
    .read_forest = NULL,  // Output format doesn't support reading forests
    .write_galaxies = binary_output_write_galaxies,
    .cleanup = binary_output_cleanup,
    
    .close_open_handles = binary_output_close_handles,
    .get_open_handle_count = binary_output_get_handle_count,
    
    .last_error = IO_ERROR_NONE,
    .error_message = {0}
};

/**
 * @brief Information about extended properties section
 */
struct extended_property_info {
    int64_t offset;        /**< Offset to extended property section */
    int64_t header_size;   /**< Size of extended property header */
    uint32_t magic;        /**< Magic marker */
    int version;           /**< Format version */
};

/**
 * @brief Format-specific utility functions
 */
static int write_extended_property_header(int fd, struct binary_output_data *format_data);
static int close_all_files(struct binary_output_data *format_data);

/**
 * @brief Write callback for buffer manager
 * 
 * Uses the mypwrite function from core_utils.h
 *
 * @param fd File descriptor
 * @param buffer Data buffer to write
 * @param size Number of bytes to write
 * @param offset File offset
 * @param context Optional context data (unused)
 * @return 0 on success, negative value on error
 */
static int binary_write_callback(int fd, const void* buffer, size_t size, off_t offset, void* context) {
    (void)context; // Suppress unused parameter warning
    ssize_t result = mypwrite(fd, buffer, size, offset);
    if (result < 0) {
        return result;
    }
    return 0;
}

/**
 * @brief Initialize the binary output handler
 *
 * @return 0 on success, non-zero on failure
 */
int io_binary_output_init(void) {
    return io_register_handler(&binary_output_handler);
}

/**
 * @brief Get the binary output handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
struct io_interface *io_get_binary_output_handler(void) {
    return io_get_handler_by_id(IO_FORMAT_BINARY_OUTPUT);
}

/**
 * @brief Get the extension for binary output files
 *
 * @return File extension string
 */
const char *io_binary_output_get_extension(void) {
    return "";  // Binary files don't have a standard extension
}

/**
 * @brief Initialize the binary output handler
 *
 * @param filename Base output filename
 * @param params Simulation parameters
 * @param format_data Pointer to format-specific data pointer
 * @return 0 on success, non-zero on failure
 */
int binary_output_initialize(const char *filename, struct params *params, void **format_data) {
    if (filename == NULL || params == NULL || format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL parameters passed to binary_output_initialize");
        return -1;
    }
    
    // Allocate format-specific data
    struct binary_output_data *data = calloc(1, sizeof(struct binary_output_data));
    if (data == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate binary output format data");
        return -1;
    }
    
    // Initialize format-specific data
    data->num_snapshots = params->simulation.NumSnapOutputs;
    data->output_endianness = ENDIAN_BIG;  // Use big-endian (network order) for output
    data->swap_needed = (get_system_endianness() != data->output_endianness);
    data->extended_props_enabled = (global_extension_registry != NULL && 
                                   global_extension_registry->num_extensions > 0);
    
    // Cache buffer parameters
    data->buffer_size_initial_mb = params->runtime.BufferSizeInitialMB;
    data->buffer_size_min_mb = params->runtime.BufferSizeMinMB;
    data->buffer_size_max_mb = params->runtime.BufferSizeMaxMB;
    
    // Allocate file descriptors array
    data->file_descriptors = calloc(data->num_snapshots, sizeof(int));
    if (data->file_descriptors == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate file descriptors array");
        free(data);
        return -1;
    }
    
    // Initialize all file descriptors to -1 (closed)
    for (int i = 0; i < data->num_snapshots; i++) {
        data->file_descriptors[i] = -1;
    }
    
    // Allocate buffer array
    data->output_buffers = calloc(data->num_snapshots, sizeof(struct io_buffer*));
    if (data->output_buffers == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate output buffers array");
        free(data->file_descriptors);
        free(data);
        return -1;
    }
    
    // Initialize all buffers to NULL
    for (int i = 0; i < data->num_snapshots; i++) {
        data->output_buffers[i] = NULL;
    }
    
    // Allocate arrays for galaxy counts
    data->total_galaxies = calloc(data->num_snapshots, sizeof(int64_t));
    if (data->total_galaxies == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate total galaxies array");
        free(data->file_descriptors);
        free(data);
        return -1;
    }
    
    // If extended properties are enabled, initialize the serialization context
    if (data->extended_props_enabled) {
        int ret = property_serialization_init(&data->prop_ctx, SERIALIZE_EXPLICIT);
        if (ret != 0) {
            io_set_error(IO_ERROR_UNKNOWN, "Failed to initialize property serialization context");
            free(data->total_galaxies);
            free(data->file_descriptors);
            free(data);
            return -1;
        }
        
        ret = property_serialization_add_properties(&data->prop_ctx);
        if (ret != 0) {
            io_set_error(IO_ERROR_UNKNOWN, "Failed to add properties to serialization context");
            property_serialization_cleanup(&data->prop_ctx);
            free(data->total_galaxies);
            free(data->file_descriptors);
            free(data);
            return -1;
        }
    }
    
    *format_data = data;
    return 0;
}

/**
 * @brief Write galaxy data to binary output files
 *
 * @param galaxies Array of galaxies to write
 * @param ngals Number of galaxies
 * @param save_info Save information (file handles, etc.)
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
/**
 * Modified version of binary_output_write_galaxies that uses parameters passed
 * directly instead of assuming they're in save_info
 */
int binary_output_write_galaxies(struct GALAXY *galaxies, int ngals, 
                              struct save_info *save_info, void *format_data) {
    LOG_ERROR("Binary output format is no longer supported. Please use HDF5 output format.");
    return EXIT_FAILURE;
}

/**
 * @brief Clean up the binary output handler
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
int binary_output_cleanup(void *format_data) {
    if (format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to binary_output_cleanup");
        return -1;
    }
    
    struct binary_output_data *data = (struct binary_output_data *)format_data;
    
    // Finalize output files
    for (int i = 0; i < data->num_snapshots; i++) {
        int fd = data->file_descriptors[i];
        
        // Destroy buffer if exists (this also flushes any remaining data)
        if (data->output_buffers[i] != NULL) {
            buffer_destroy(data->output_buffers[i]);
            data->output_buffers[i] = NULL;
        }
        
        if (fd >= 0) {
            // Write header information
            int32_t header[2];
            header[0] = data->num_forests;  // Number of forests
            header[1] = (int32_t)data->total_galaxies[i];  // Total number of galaxies
            
            if (data->swap_needed) {
                swap_endianness(header, sizeof(int32_t), 2);
            }
            
            // Write header
            if (pwrite(fd, header, sizeof(header), 0) != sizeof(header)) {
                io_set_error(IO_ERROR_FILE_NOT_FOUND, "Failed to write header information");
                return -1;
            }
            
            // Write galaxies per forest
            int32_t *galaxies_per_forest = malloc(data->num_forests * sizeof(int32_t));
            if (galaxies_per_forest == NULL) {
                io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate temporary galaxies per forest array");
                return -1;
            }
            
            // Copy to 32-bit array and handle endianness
            for (int j = 0; j < data->num_forests; j++) {
                galaxies_per_forest[j] = (int32_t)data->galaxies_per_forest[i][j];
            }
            
            if (data->swap_needed) {
                swap_endianness(galaxies_per_forest, sizeof(int32_t), data->num_forests);
            }
            
            size_t forest_array_size = data->num_forests * sizeof(int32_t);
            if (pwrite(fd, galaxies_per_forest, forest_array_size, sizeof(header)) != (ssize_t)forest_array_size) {
                io_set_error(IO_ERROR_FILE_NOT_FOUND, "Failed to write galaxies per forest information");
                free(galaxies_per_forest);
                return -1;
            }
            
            free(galaxies_per_forest);
            
            // If extended properties are enabled, write the offset
            if (data->extended_props_enabled) {
                // Get current file position (end of galaxies)
                off_t current_pos = lseek(fd, 0, SEEK_CUR);
                if (current_pos == (off_t)-1) {
                    io_set_error(IO_ERROR_FILE_NOT_FOUND, "Failed to get current file position");
                    return -1;
                }
                
                // Write extended property header
                int ret = write_extended_property_header(fd, data);
                if (ret != 0) {
                    io_set_error(IO_ERROR_FILE_NOT_FOUND, "Failed to write extended property header");
                    return -1;
                }
                
                // Create extended property info
                struct extended_property_info prop_info;
                prop_info.offset = current_pos;
                prop_info.header_size = ret;
                prop_info.magic = BINARY_OUTPUT_MAGIC;
                prop_info.version = BINARY_OUTPUT_VERSION;
                
                if (data->swap_needed) {
                    swap_endianness(&prop_info, sizeof(prop_info), 1);
                }
                
                // Write extended property info to header
                off_t prop_info_offset = sizeof(header) + forest_array_size;
                if (pwrite(fd, &prop_info, sizeof(prop_info), prop_info_offset) != sizeof(prop_info)) {
                    io_set_error(IO_ERROR_FILE_NOT_FOUND, "Failed to write extended property info");
                    return -1;
                }
            }
            
            // Close file
            close(fd);
            data->file_descriptors[i] = -1;
        }
    }
    
    // Clean up resources
    for (int i = 0; i < data->num_snapshots; i++) {
        if (data->galaxies_per_forest != NULL && data->galaxies_per_forest[i] != NULL) {
            free(data->galaxies_per_forest[i]);
        }
    }
    
    if (data->galaxies_per_forest != NULL) {
        free(data->galaxies_per_forest);
    }
    
    if (data->total_galaxies != NULL) {
        free(data->total_galaxies);
    }
    
    if (data->file_descriptors != NULL) {
        free(data->file_descriptors);
    }
    
    if (data->output_buffers != NULL) {
        free(data->output_buffers);
    }
    
    if (data->extended_props_enabled) {
        property_serialization_cleanup(&data->prop_ctx);
    }
    
    free(data);
    return 0;
}

/**
 * @brief Close all open file handles
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
int binary_output_close_handles(void *format_data) {
    if (format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to binary_output_close_handles");
        return -1;
    }
    
    struct binary_output_data *data = (struct binary_output_data *)format_data;
    return close_all_files(data);
}

/**
 * @brief Get the number of open file handles
 *
 * @param format_data Format-specific data
 * @return Number of open file handles, -1 on error
 */
int binary_output_get_handle_count(void *format_data) {
    if (format_data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to binary_output_get_handle_count");
        return -1;
    }
    
    struct binary_output_data *data = (struct binary_output_data *)format_data;
    int count = 0;
    
    for (int i = 0; i < data->num_snapshots; i++) {
        if (data->file_descriptors[i] >= 0) {
            count++;
        }
    }
    
    return count;
}


/**
 * @brief Write extended property header to file
 *
 * @param fd File descriptor
 * @param format_data Format-specific data
 * @return Header size on success, -1 on failure
 */
static int write_extended_property_header(int fd, struct binary_output_data *format_data) {
    if (fd < 0 || format_data == NULL) {
        return -1;
    }
    
    if (!format_data->extended_props_enabled) {
        return 0;
    }
    
    // Allocate buffer for header
    size_t buffer_size = 4096;  // Large enough for most headers
    void *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        return -1;
    }
    
    // Create header
    int64_t header_size = property_serialization_create_header(&format_data->prop_ctx, buffer, buffer_size);
    if (header_size < 0) {
        free(buffer);
        return -1;
    }
    
    // Write header
    ssize_t written = write(fd, buffer, header_size);
    free(buffer);
    
    if (written != header_size) {
        return -1;
    }
    
    return (int)header_size;
}


/**
 * @brief Close all open files
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int close_all_files(struct binary_output_data *format_data) {
    if (format_data == NULL) {
        return -1;
    }
    
    for (int i = 0; i < format_data->num_snapshots; i++) {
        // Destroy buffer if exists (this also flushes any remaining data)
        if (format_data->output_buffers[i] != NULL) {
            buffer_destroy(format_data->output_buffers[i]);
            format_data->output_buffers[i] = NULL;
        }
        
        // Close file descriptor
        if (format_data->file_descriptors[i] >= 0) {
            close(format_data->file_descriptors[i]);
            format_data->file_descriptors[i] = -1;
        }
    }
    
    return 0;
}