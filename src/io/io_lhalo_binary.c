#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "io_lhalo_binary.h"
#include "io_endian_utils.h"
#include "forest_utils.h"
#include "../core/core_mymalloc.h"
#include "../core/core_utils.h"

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
    
    *format_data = data;
    return 0;
}

/**
 * @brief Read a forest from LHalo binary file
 *
 * Reads the halo data for a specific forest.
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
    struct halo_data *local_halos = mymalloc(sizeof(struct halo_data) * nhalos);
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
    
    // Read the halo data from the file
    ssize_t read_bytes = pread(fd, local_halos, sizeof(struct halo_data) * nhalos, offset);
    if (read_bytes != (ssize_t)(sizeof(struct halo_data) * nhalos)) {
        io_set_error(IO_ERROR_FORMAT_ERROR, "Failed to read all halo data");
        myfree(local_halos);
        return -1;
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
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_binary_cleanup(void *format_data) {
    if (format_data == NULL) {
        return 0;  // Nothing to clean up
    }
    
    struct lhalo_binary_data *data = (struct lhalo_binary_data *)format_data;
    
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
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_binary_close_handles(void *format_data) {
    if (format_data == NULL) {
        return 0;  // Nothing to close
    }
    
    struct lhalo_binary_data *data = (struct lhalo_binary_data *)format_data;
    
    // Close all open file descriptors
    for (int i = 0; i < data->num_open_files; i++) {
        if (data->open_file_descriptors[i] > 0) {
            close(data->open_file_descriptors[i]);
            data->open_file_descriptors[i] = -1;
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
