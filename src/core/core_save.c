#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "core_allvars.h"
#include "core_save.h"
#include "core_utils.h"
#include "core_logging.h"
#include "../physics/legacy/model_misc.h"
#include "core_mymalloc.h"
#include "core_galaxy_extensions.h"
#include "../io/io_galaxy_output.h"

// Binary output format has been removed

#ifdef HDF5
#include "../io/save_gals_hdf5.h"
#endif

/**
 * @brief Map I/O interface error codes to SAGE error types
 * 
 * @param io_error Error code from the I/O interface
 * @return Corresponding SAGE error type
 * 
 * This function maps error codes from the I/O interface system to SAGE's own
 * error codes, providing consistent error handling throughout the codebase.
 */
static int32_t map_io_error_to_sage_error(int io_error)
{
    switch (io_error) {
        case IO_ERROR_NONE:
            return EXIT_SUCCESS;
        case IO_ERROR_FILE_NOT_FOUND:
            return FILE_NOT_FOUND;
        case IO_ERROR_FORMAT_ERROR:
            return INVALID_OPTION_IN_PARAMS;
        case IO_ERROR_RESOURCE_LIMIT:
            return OUT_OF_MEMBLOCKS;
        case IO_ERROR_MEMORY_ALLOCATION:
            return MALLOC_FAILURE;
        case IO_ERROR_VALIDATION_FAILED:
            return INVALID_VALUE_READ_FROM_FILE;
#ifdef HDF5
        case IO_ERROR_HANDLE_INVALID:
            return HDF5_ERROR;
#endif
        case IO_ERROR_UNSUPPORTED_OP:
            return FILE_WRITE_ERROR;
        default:
            return INVALID_OPTION_IN_PARAMS;
    }
}

/**
 * @brief Log an I/O interface error with appropriate severity
 * 
 * @param context Context string for the error message
 * @param io_error Error code from the I/O interface
 * 
 * This function logs errors from the I/O interface system with appropriate
 * severity levels, providing context about where the error occurred.
 */
static void log_io_error(const char *context, int io_error)
{
    const char *error_msg = io_get_error_message();
    
    switch (io_error) {
        case IO_ERROR_NONE:
            // No error, nothing to log
            break;
        case IO_ERROR_FILE_NOT_FOUND:
        case IO_ERROR_FORMAT_ERROR:
        case IO_ERROR_MEMORY_ALLOCATION:
        case IO_ERROR_VALIDATION_FAILED:
            // Log as errors
            LOG_ERROR("%s: %s (code %d)", context, error_msg, io_error);
            break;
        case IO_ERROR_RESOURCE_LIMIT:
        case IO_ERROR_HANDLE_INVALID:
            // Log as warnings
            LOG_WARNING("%s: %s (code %d)", context, error_msg, io_error);
            break;
        default:
            // Unknown errors as errors
            LOG_ERROR("%s: Unknown error - %s (code %d)", context, error_msg, io_error);
            break;
    }
}

// Local Proto-Types //

// Externally Visible Functions //


// Open up all the required output files and remember their file handles.  These are placed into
// `save_info` for access later.
int32_t initialize_galaxy_files(const int rank, struct save_info *save_info, const struct params *run_params)
{
    
    int32_t status;

    if(run_params->simulation.NumSnapOutputs > ABSOLUTEMAXSNAPS) {
        char error_buffer[256];
        snprintf(error_buffer, sizeof(error_buffer),
                "Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots. "
                "To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile",
                run_params->simulation.NumSnapOutputs, ABSOLUTEMAXSNAPS);
        io_set_error(IO_ERROR_RESOURCE_LIMIT, error_buffer);
        log_io_error("initialize_galaxy_files", IO_ERROR_RESOURCE_LIMIT);
        return map_io_error_to_sage_error(IO_ERROR_RESOURCE_LIMIT);
    }

    // Use HDF5 as the only supported output format
#ifdef HDF5
    // HDF5 is the standard output format
    status = initialize_hdf5_galaxy_files(rank, save_info, run_params);
#else
    // HDF5 is required
    io_set_error(IO_ERROR_FORMAT_ERROR, "HDF5 support is required but not compiled in");
    log_io_error("initialize_galaxy_files", IO_ERROR_FORMAT_ERROR);
    status = map_io_error_to_sage_error(IO_ERROR_FORMAT_ERROR);
#endif

    return status;
}


// Write all the galaxy properties to file.
int32_t save_galaxies(const int64_t task_forestnr, const int numgals, struct halo_data *halos,
                      struct forest_info *forest_info,
                      struct halo_aux_data *haloaux, struct GALAXY *halogal,
                      struct save_info *save_info, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;

    // Use the modern I/O galaxy output interface to prepare galaxies
    struct galaxy_output_context output_ctx;
    status = prepare_galaxies_for_output(task_forestnr, halos, forest_info, haloaux, halogal,
                                        numgals, &output_ctx, run_params);
    if(status != EXIT_SUCCESS) {
        char error_buffer[256];
        snprintf(error_buffer, sizeof(error_buffer),
                "Failed to prepare galaxies for output (task_forestnr = %"PRId64")", task_forestnr);
        io_set_error(IO_ERROR_VALIDATION_FAILED, error_buffer);
        log_io_error("save_galaxies", IO_ERROR_VALIDATION_FAILED);
        return map_io_error_to_sage_error(IO_ERROR_VALIDATION_FAILED);
    }

    // HDF5 is the only supported output format
#ifdef HDF5
    status = save_hdf5_galaxies(task_forestnr, numgals, forest_info, halos, haloaux, halogal, save_info, run_params);
#else
    // HDF5 is required
    io_set_error(IO_ERROR_FORMAT_ERROR, "HDF5 support is required but not compiled in");
    log_io_error("save_galaxies", IO_ERROR_FORMAT_ERROR);
    status = map_io_error_to_sage_error(IO_ERROR_FORMAT_ERROR);
#endif

    // Clean up output context
    free_output_arrays(&output_ctx);
    return status;
}


// Write any remaining attributes or header information, close all the open files and free all the
// relevant dataspaces.
int32_t finalize_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;

    // HDF5 is the only supported output format
#ifdef HDF5
    status = finalize_hdf5_galaxy_files(forest_info, save_info, run_params);
#else
    // HDF5 is required
    io_set_error(IO_ERROR_FORMAT_ERROR, "HDF5 support is required but not compiled in");
    log_io_error("finalize_galaxy_files", IO_ERROR_FORMAT_ERROR);
    status = map_io_error_to_sage_error(IO_ERROR_FORMAT_ERROR);
#endif

    return status;
}
