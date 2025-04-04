#pragma once

#ifdef HDF5

#include <stdbool.h>
#include "../core/core_allvars.h"
#include "io_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Data structure for ConsistentTrees HDF5 format handler
 */
struct consistent_trees_hdf5_data {
    char filename[512];        /**< HDF5 file name */
    int64_t num_forests;       /**< Number of forests in the file */
    int64_t *forest_offsets;   /**< Offsets to each forest in the file */
    bool initialized;          /**< Initialization flag */
};

/**
 * @brief Initialize the ConsistentTrees HDF5 handler
 * 
 * @param filename HDF5 file name
 * @param params SAGE parameters
 * @param format_data Pointer to handler data (will be allocated)
 * @return 0 on success, non-zero on failure
 */
extern int ctrees_hdf5_initialize(const char *filename, struct params *params, void **format_data);

/**
 * @brief Read a forest from the ConsistentTrees HDF5 file
 * 
 * @param forestnr Forest number to read
 * @param halos Pointer to halo data array (will be allocated)
 * @param forest_info Forest information structure
 * @param format_data Handler data
 * @return Number of halos read, or -1 on error
 */
extern int64_t ctrees_hdf5_read_forest(int64_t forestnr, struct halo_data **halos, 
                                     struct forest_info *forest_info, void *format_data);

/**
 * @brief Clean up the ConsistentTrees HDF5 handler
 * 
 * @param format_data Handler data
 * @return 0 on success, non-zero on failure
 */
extern int ctrees_hdf5_cleanup(void *format_data);

/**
 * @brief Close open HDF5 handles
 * 
 * @param format_data Handler data
 * @return 0 on success, non-zero on failure
 */
extern int ctrees_hdf5_close_open_handles(void *format_data);

/**
 * @brief Get the number of open HDF5 handles
 * 
 * @param format_data Handler data
 * @return Number of open handles
 */
extern int ctrees_hdf5_get_open_handle_count(void *format_data);

/**
 * @brief Initialize the ConsistentTrees HDF5 handler system
 * 
 * @return 0 on success, non-zero on failure
 */
extern int io_consistent_trees_hdf5_init(void);

/**
 * @brief Check if a file is a ConsistentTrees HDF5 file
 * 
 * @param filename File to check
 * @return true if the file is a ConsistentTrees HDF5 file, false otherwise
 */
extern bool io_is_consistent_trees_hdf5(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* HDF5 */