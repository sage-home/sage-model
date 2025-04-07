#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <hdf5.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "../core/core_save.h"
#include "io_interface.h"
#include "io_hdf5_utils.h"
#include "io_property_serialization.h"
#include "io_buffer_manager.h"

/**
 * @file io_hdf5_output.h
 * @brief I/O interface implementation for HDF5 galaxy output format
 *
 * This file provides the handler implementation for writing galaxy data
 * in the SAGE HDF5 format, with support for extended properties and
 * proper resource management.
 */

/**
 * @brief Default buffer size for galaxy output
 */
#define HDF5_GALAXY_BUFFER_SIZE 8192

/**
 * @brief HDF5 output format-specific data structure
 *
 * Contains additional data needed by the HDF5 output handler
 */
struct hdf5_output_data {
    hid_t file_id;                   /**< HDF5 file handle */
    hid_t *snapshot_group_ids;       /**< HDF5 group handles for snapshots */
    int num_snapshots;               /**< Number of output snapshots */
    double *redshifts;               /**< Redshift for each snapshot */
    int64_t *total_galaxies;         /**< Total number of galaxies per snapshot */
    int64_t **galaxies_per_forest;   /**< Number of galaxies per forest for each snapshot */
    int num_forests;                 /**< Number of forests */
    
    /* Buffer for galaxy data */
    struct {
        int buffer_size;             /**< Size of buffer */
        int galaxies_in_buffer;      /**< Number of galaxies in buffer */
        void **property_buffers;     /**< Buffers for galaxy properties */
        int num_properties;          /**< Number of properties in buffer */
    } *snapshot_buffers;
    
    /* Buffer manager */
    struct io_buffer **io_buffers;   /**< I/O buffers for efficient disk access */
    int buffer_size_initial_mb;      /**< Initial buffer size in MB */
    int buffer_size_min_mb;          /**< Minimum buffer size in MB */
    int buffer_size_max_mb;          /**< Maximum buffer size in MB */
    
    /* Extended property support */
    bool extended_props_enabled;     /**< Flag indicating if extended properties are enabled */
    struct property_serialization_context prop_ctx; /**< Property serialization context */
    
    /* Field metadata */
    char (*field_names)[MAX_STRING_LEN];      /**< Array of field names */
    char (*field_descriptions)[MAX_STRING_LEN]; /**< Array of field descriptions */
    char (*field_units)[MAX_STRING_LEN];      /**< Array of field units */
    hsize_t *field_dtypes;                    /**< Array of HDF5 data types */
    int num_fields;                           /**< Number of standard fields */
};

/**
 * @brief Initialize the HDF5 output handler
 *
 * Registers the handler with the I/O interface system.
 *
 * @return 0 on success, non-zero on failure
 */
extern int io_hdf5_output_init(void);

/**
 * @brief Get the HDF5 output handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
extern struct io_interface *io_get_hdf5_output_handler(void);

/**
 * @brief Get the extension for HDF5 output files
 *
 * @return File extension string
 */
extern const char *io_hdf5_output_get_extension(void);

/**
 * @brief Initialize the HDF5 output handler
 *
 * @param filename Base output filename
 * @param params Simulation parameters
 * @param format_data Pointer to format-specific data pointer
 * @return 0 on success, non-zero on failure
 */
extern int hdf5_output_initialize(const char *filename, struct params *params, void **format_data);

/**
 * @brief Write galaxy data to HDF5 output files
 *
 * @param galaxies Array of galaxies to write
 * @param ngals Number of galaxies
 * @param save_info Save information (file handles, etc.)
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
extern int hdf5_output_write_galaxies(struct GALAXY *galaxies, int ngals, 
                                     struct save_info *save_info, void *format_data);

/**
 * @brief Clean up the HDF5 output handler
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
extern int hdf5_output_cleanup(void *format_data);

/**
 * @brief Close all open HDF5 handles
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
extern int hdf5_output_close_handles(void *format_data);

/**
 * @brief Get the number of open HDF5 handles
 *
 * @param format_data Format-specific data
 * @return Number of open HDF5 handles, -1 on error
 */
extern int hdf5_output_get_handle_count(void *format_data);

#ifdef __cplusplus
}
#endif