#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "io_interface.h"
#include "io_hdf5_utils.h"
#include "hdf5_read_utils.h"

/**
 * @file io_lhalo_hdf5.h
 * @brief I/O interface implementation for LHalo HDF5 format
 *
 * This file provides the handler implementation for reading and writing
 * merger trees in the LHalo HDF5 format, with proper HDF5 resource
 * management and support for extended properties.
 */

#ifdef HDF5

/**
 * @brief LHalo HDF5 format-specific data structure
 *
 * Contains additional data needed by the LHalo HDF5 handler
 */
struct lhalo_hdf5_data {
    hid_t *file_handles;       /**< Open HDF5 file handles (one per forest) */
    hid_t *unique_file_handles; /**< Open unique HDF5 file handles */
    int32_t num_open_files;     /**< Number of open files */
    int64_t *nhalos_per_forest; /**< Number of halos in each forest */
    struct HDF5_METADATA_NAMES metadata_names; /**< HDF5 metadata field names */
};

/**
 * @brief Initialize the LHalo HDF5 I/O handler
 *
 * Registers the handler with the I/O interface system.
 *
 * @return 0 on success, non-zero on failure
 */
extern int io_lhalo_hdf5_init(void);

/**
 * @brief Get the LHalo HDF5 handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
extern struct io_interface *io_get_lhalo_hdf5_handler(void);

/**
 * @brief Detect if a file is in LHalo HDF5 format
 *
 * @param filename File to examine
 * @return true if the file is in LHalo HDF5 format, false otherwise
 */
extern bool io_is_lhalo_hdf5(const char *filename);

#endif /* HDF5 */

#ifdef __cplusplus
}
#endif