#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "io_interface.h"
#include "io_endian_utils.h"

/**
 * @file io_lhalo_binary.h
 * @brief I/O interface implementation for LHalo binary format
 *
 * This file provides the handler implementation for reading and writing
 * merger trees in the LHalo binary format, with cross-platform
 * endianness handling.
 */

/**
 * @brief LHalo binary format-specific data structure
 *
 * Contains additional data needed by the LHalo binary handler
 */
struct lhalo_binary_data {
    int *file_descriptors;       /**< Open file descriptors (one per forest) */
    int *open_file_descriptors;  /**< Open unique file descriptors */
    int32_t num_open_files;      /**< Number of open files */
    int64_t *nhalos_per_forest;  /**< Number of halos in each forest */
    off_t *offsets_per_forest;   /**< File offset for each forest */
    enum endian_type file_endianness; /**< Endianness of the binary file(s) */
    bool swap_needed;            /**< Flag indicating if byte swapping is needed */
};

/* Forward declarations are not needed in the header as these are internal to the implementation */

/**
 * @brief Initialize the LHalo binary I/O handler
 *
 * Registers the handler with the I/O interface system.
 *
 * @return 0 on success, non-zero on failure
 */
extern int io_lhalo_binary_init(void);

/**
 * @brief Get the LHalo binary handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
extern struct io_interface *io_get_lhalo_binary_handler(void);

/**
 * @brief Detect if a file is in LHalo binary format
 *
 * @param filename File to examine
 * @return true if the file is in LHalo binary format, false otherwise
 */
extern bool io_is_lhalo_binary(const char *filename);

#ifdef __cplusplus
}
#endif