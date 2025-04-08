#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

#include "io_lhalo_binary.h"
#include "io_endian_utils.h"
#include "io_memory_map.h"
#include "forest_utils.h"
#include "../core/core_mymalloc.h"
#include "../core/core_utils.h"
#include "../core/core_logging.h"

/* Forward declaration of static functions */
static int io_lhalo_binary_initialize(const char *filename, struct params *params, void **format_data);
static int64_t io_lhalo_binary_read_forest(int64_t forestnr, struct halo_data **halos, 
                                         struct forest_info *forest_info, void *format_data);
static int io_lhalo_binary_cleanup(void *format_data);
static int io_lhalo_binary_close_handles(void *format_data);
static int io_lhalo_binary_get_handle_count(void *format_data);

/**
 * @brief LHalo binary handler interface
 *
 * Implementation of the I/O interface for LHalo binary format
 */
static struct io_interface lhalo_binary_handler = {
    .name = "LHalo Binary",
    .version = "1.0",
    .format_id = IO_FORMAT_LHALO_BINARY,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_MULTI_FILE,
    
    .initialize = io_lhalo_binary_initialize,
    .read_forest = io_lhalo_binary_read_forest,
    .write_galaxies = NULL, /* LHalo binary format is input-only */
    .cleanup = io_lhalo_binary_cleanup,
    
    .close_open_handles = io_lhalo_binary_close_handles,
    .get_open_handle_count = io_lhalo_binary_get_handle_count,
    
    .last_error = IO_ERROR_NONE,
    .error_message = {0}
};

/* Function removed: get_forests_filename_lhalo_binary - Not used in current implementation */

/**
 * @brief Initialize the LHalo binary I/O handler
 *
 * Registers the handler with the I/O interface system.
 *
 * @return 0 on success, non-zero on failure
 */
int io_lhalo_binary_init(void) {
    return io_register_handler(&lhalo_binary_handler);
}

/**
 * @brief Get the LHalo binary handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
struct io_interface *io_get_lhalo_binary_handler(void) {
    return io_get_handler_by_id(IO_FORMAT_LHALO_BINARY);
}

/**
 * @brief Detect if a file is in LHalo binary format
 *
 * Checks for the LHalo binary format structure.
 *
 * @param filename File to examine
 * @return true if the file is in LHalo binary format, false otherwise
 */
bool io_is_lhalo_binary(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        return false;
    }
    
    // LHalo binary files start with:
    // - 4 bytes for total number of forests (int32_t)
    // - 4 bytes for total number of halos (int32_t)
    // - N*4 bytes for number of halos per forest (N = total forests)
    
    int32_t totNforests, totNhalos;
    
    // Read the first 8 bytes
    size_t read_count = fread(&totNforests, sizeof(int32_t), 1, fp);
    if (read_count != 1) {
        fclose(fp);
        return false;
    }
    
    read_count = fread(&totNhalos, sizeof(int32_t), 1, fp);
    if (read_count != 1) {
        fclose(fp);
        return false;
    }
    
    fclose(fp);
    
    // Simple validation:
    // - Total forests should be positive and reasonable
    // - Total halos should be positive and greater than total forests
    // - Total halos shouldn't be unreasonably large
    return (totNforests > 0 && totNforests < 1000000 &&
            totNhalos > 0 && totNhalos >= totNforests &&
            totNhalos < 1000000000);
}

/**
 * @brief Initialize the LHalo binary handler
 *
 * Opens the files, reads header information, and prepares for reading forests.
 * Optionally sets up memory mapping if enabled in runtime parameters.
 *
 * @param filename Tree file pattern or base name
 * @param params Run parameters
 * @param format_data Pointer to store format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_binary_initialize(const char *filename, struct params *params, void **format_data) {
    if (filename == NULL || params == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL filename or params passed to io_lhalo_binary_initialize");
        return -1;
    }
    
    struct lhalo_binary_data *data = calloc(1, sizeof(struct lhalo_binary_data));
    if (data == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for lhalo_binary_data");
        return -1;
    }
    
    // Determine file endianness (assume network byte order, i.e., big-endian)
    data->file_endianness = ENDIAN_BIG;
    data->swap_needed = (get_system_endianness() != data->file_endianness);
    
    // Check if memory mapping is enabled
    data->use_mmap = (params->runtime.EnableMemoryMapping != 0 && mmap_is_available());
    if (data->use_mmap) {
        LOG_DEBUG("Memory mapping enabled for LHalo binary files");
        
        // Determine how many files we need to map
        int num_files = params->io.LastFile - params->io.FirstFile + 1;
        if (num_files <= 0) {
            LOG_ERROR("Invalid file range: FirstFile=%d, LastFile=%d", 
                     params->io.FirstFile, params->io.LastFile);
            free(data);
            return -1;
        }
        
        // Allocate arrays for mapping information
        data->mapped_files = calloc(num_files, sizeof(struct mmap_region*));
        data->mapped_data = calloc(num_files, sizeof(void*));
        data->mapped_sizes = calloc(num_files, sizeof(size_t));
        data->filenames = calloc(num_files, sizeof(char*));
        
        if (data->mapped_files == NULL || data->mapped_data == NULL || 
            data->mapped_sizes == NULL || data->filenames == NULL) {
            LOG_ERROR("Failed to allocate memory for mapping arrays");
            free(data->mapped_files);
            free(data->mapped_data);
            free(data->mapped_sizes);
            free(data->filenames);
            free(data);
            return -1;
        }
        
        data->num_open_files = num_files;
    } else if (params->runtime.EnableMemoryMapping != 0) {
        LOG_WARNING("Memory mapping requested but not available on this platform");
    }
    
    *format_data = data;
    return 0;
}

/**
 * @brief Read a forest from LHalo binary file
 *
 * Reads the halo data for a specific forest. Uses memory mapping if enabled
 * and available, otherwise falls back to standard file I/O.
 *
 * @param forestnr Forest number to read
 * @param halos Pointer to store the halo data
 * @param forest_info Forest information structure
 * @param format_data Format-specific data
 * @return Number of halos read, or negative error code
 */
static int64_t io_lhalo_binary_read_forest(int64_t forestnr, struct halo_data **halos, 
                                         struct forest_info *forest_info, void *format_data) {
    struct lhalo_binary_data *data = (struct lhalo_binary_data *)format_data;
    
    if (data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to io_lhalo_binary_read_forest");
        return -1;
    }
    
    if (forestnr >= forest_info->nforests_this_task) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Forest number out of range");
        return -1;
    }
    
    const int64_t nhalos = forest_info->lht.nhalos_per_forest[forestnr];
    const size_t forest_size = sizeof(struct halo_data) * nhalos;
    struct halo_data *local_halos = mymalloc(forest_size);
    if (local_halos == NULL) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to allocate memory for halos in forestnr = %"PRId64, forestnr);
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, error_msg);
        return -1;
    }
    
    int fd = forest_info->lht.fd[forestnr];
    if (fd <= 0) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Invalid file descriptor for forest");
        myfree(local_halos);
        return -1;
    }
    
    const off_t offset = forest_info->lht.bytes_offset_for_forest[forestnr];
    if (offset < 0) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Negative offset for forest");
        myfree(local_halos);
        return -1;
    }
    
    // Determine the file index for this forest
    int file_index = forest_info->FileNr[forestnr] - forest_info->firstfile;
    if (file_index < 0 || file_index >= forest_info->lastfile - forest_info->firstfile + 1) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Invalid file index for forest");
        myfree(local_halos);
        return -1;
    }
    
    // Read data - either from memory mapping or direct file I/O
    bool success = false;
    
    if (data->use_mmap && data->mapped_files != NULL && data->mapped_data != NULL) {
        // Use memory mapping if available
        struct mmap_region *region = data->mapped_files[file_index];
        void *mapped_data = data->mapped_data[file_index];
        
        if (region != NULL && mapped_data != NULL) {
            size_t mapped_size = mmap_get_size(region);
            
            // Check if forest data is within mapped region
            if (offset + forest_size <= mapped_size) {
                // Copy data from mapped memory
                char *src = (char *)mapped_data + offset;
                memcpy(local_halos, src, forest_size);
                success = true;
                LOG_DEBUG("Read forest %"PRId64" using memory mapping", forestnr);
            } else {
                LOG_WARNING("Forest %"PRId64" extends beyond mapped region - falling back to standard I/O", forestnr);
            }
        }
    }
    
    // Fall back to standard I/O if memory mapping is not used or failed
    if (!success) {
        ssize_t read_bytes = pread(fd, local_halos, forest_size, offset);
        if (read_bytes != (ssize_t)forest_size) {
            io_set_error(IO_ERROR_FORMAT_ERROR, "Failed to read all halo data");
            myfree(local_halos);
            return -1;
        }
        success = true;
    }
    
    // Handle endianness conversion if needed
    if (data->swap_needed) {
        for (int64_t i = 0; i < nhalos; i++) {
            struct halo_data *h = &local_halos[i];
            
            // Convert selected integer fields from big-endian to host endianness
            h->Descendant = network_to_host_uint32(h->Descendant);
            h->FirstProgenitor = network_to_host_uint32(h->FirstProgenitor);
            h->NextProgenitor = network_to_host_uint32(h->NextProgenitor);
            h->FirstHaloInFOFgroup = network_to_host_uint32(h->FirstHaloInFOFgroup);
            h->NextHaloInFOFgroup = network_to_host_uint32(h->NextHaloInFOFgroup);
            
            h->Len = network_to_host_uint32(h->Len);
            h->Mvir = network_to_host_float(h->Mvir);
            
            // Convert position and velocity arrays
            for (int j = 0; j < 3; j++) {
                h->Pos[j] = network_to_host_float(h->Pos[j]);
                h->Vel[j] = network_to_host_float(h->Vel[j]);
            }
            
            // Additional fields would be handled here based on halo_data structure
        }
    }
    
    *halos = local_halos;
    return nhalos;
}

/**
 * @brief Clean up resources used by the LHalo binary handler
 *
 * Releases file handles and memory, including mapped regions.
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_binary_cleanup(void *format_data) {
    if (format_data == NULL) {
        return 0;  // Nothing to clean up
    }
    
    struct lhalo_binary_data *data = (struct lhalo_binary_data *)format_data;
    
    // Unmap any memory-mapped regions
    if (data->use_mmap && data->mapped_files != NULL) {
        int num_files = data->num_open_files;
        for (int i = 0; i < num_files; i++) {
            if (data->mapped_files[i] != NULL) {
                mmap_unmap(data->mapped_files[i]);
                data->mapped_files[i] = NULL;
                data->mapped_data[i] = NULL;
            }
        }
        free(data->mapped_files);
        free(data->mapped_data);
        free(data->mapped_sizes);
        
        // Free filenames array if allocated
        if (data->filenames != NULL) {
            for (int i = 0; i < num_files; i++) {
                free(data->filenames[i]);
            }
            free(data->filenames);
        }
    }
    
    // Close any open file descriptors
    io_lhalo_binary_close_handles(format_data);
    
    // Free allocated memory
    free(data->file_descriptors);
    free(data->open_file_descriptors);
    free(data->nhalos_per_forest);
    free(data->offsets_per_forest);
    free(data);
    
    return 0;
}

/**
 * @brief Close all open file handles
 *
 * When memory mapping is used, this keeps the mappings intact but closes
 * the underlying file descriptors if not needed for mapping.
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_binary_close_handles(void *format_data) {
    if (format_data == NULL) {
        return 0;  // Nothing to close
    }
    
    struct lhalo_binary_data *data = (struct lhalo_binary_data *)format_data;
    
    // Only close file descriptors if not using memory mapping
    // or if the mapping contains its own copy of the descriptors
    if (!data->use_mmap) {
        // Close all open file descriptors
        for (int i = 0; i < data->num_open_files; i++) {
            if (data->open_file_descriptors[i] > 0) {
                close(data->open_file_descriptors[i]);
                data->open_file_descriptors[i] = -1;
            }
        }
    }
    
    data->num_open_files = 0;
    
    return 0;
}

/**
 * @brief Get the number of open file handles
 *
 * @param format_data Format-specific data
 * @return Number of open file handles
 */
static int io_lhalo_binary_get_handle_count(void *format_data) {
    if (format_data == NULL) {
        return 0;
    }
    
    struct lhalo_binary_data *data = (struct lhalo_binary_data *)format_data;
    return data->num_open_files;
}

/* Function removed: setup_memory_mapping was integrated in io_lhalo_binary_initialize */
