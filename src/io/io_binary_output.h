#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "../core/core_save.h"
#include "io_interface.h"
#include "io_endian_utils.h"
#include "io_property_serialization.h"

/**
 * @file io_binary_output.h
 * @brief I/O interface implementation for binary galaxy output format
 *
 * This file provides the handler implementation for writing galaxy data
 * in the SAGE binary format, with support for extended properties and
 * cross-platform endianness handling.
 */

/**
 * @brief Binary output format-specific data structure
 *
 * Contains additional data needed by the binary output handler
 */
struct binary_output_data {
    int *file_descriptors;           /**< Open file descriptors (one per snapshot) */
    int num_snapshots;               /**< Number of output snapshots */
    int64_t *total_galaxies;         /**< Total number of galaxies per snapshot */
    int64_t **galaxies_per_forest;   /**< Number of galaxies per forest for each snapshot */
    int num_forests;                 /**< Number of forests */
    bool extended_props_enabled;     /**< Flag indicating if extended properties are enabled */
    struct property_serialization_context prop_ctx; /**< Property serialization context */
    enum endian_type output_endianness; /**< Endianness of the binary files */
    bool swap_needed;                /**< Flag indicating if byte swapping is needed */
};

/**
 * @brief Initialize the binary output handler
 *
 * Registers the handler with the I/O interface system.
 *
 * @return 0 on success, non-zero on failure
 */
extern int io_binary_output_init(void);

/**
 * @brief Get the binary output handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
extern struct io_interface *io_get_binary_output_handler(void);

/**
 * @brief Get the extension for binary output files
 *
 * @return File extension string
 */
extern const char *io_binary_output_get_extension(void);

/**
 * @brief Initialize the binary output handler
 *
 * @param filename Base output filename
 * @param params Simulation parameters
 * @param format_data Pointer to format-specific data pointer
 * @return 0 on success, non-zero on failure
 */
extern int binary_output_initialize(const char *filename, struct params *params, void **format_data);

/**
 * @brief Write galaxy data to binary output files
 *
 * @param galaxies Array of galaxies to write
 * @param ngals Number of galaxies
 * @param save_info Save information (file handles, etc.)
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
extern int binary_output_write_galaxies(struct GALAXY *galaxies, int ngals, 
                                      struct save_info *save_info, void *format_data);

/**
 * @brief Clean up the binary output handler
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
extern int binary_output_cleanup(void *format_data);

/**
 * @brief Close all open file handles
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
extern int binary_output_close_handles(void *format_data);

/**
 * @brief Get the number of open file handles
 *
 * @param format_data Format-specific data
 * @return Number of open file handles, -1 on error
 */
extern int binary_output_get_handle_count(void *format_data);

#ifdef __cplusplus
}
#endif