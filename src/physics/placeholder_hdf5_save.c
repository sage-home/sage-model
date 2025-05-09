/**
 * @file placeholder_hdf5_save.c
 * @brief Placeholder implementations for HDF5 save routines in core-only mode
 *
 * This file provides placeholder implementations for HDF5 save routines
 * that reference physics properties. These are used in core-only mode.
 */

#include <stdio.h>
#include <stdlib.h>
#include "../core/core_allvars.h"
#include "placeholder_hdf5_save.h"

/**
 * @brief Placeholder function for prepare_galaxy_for_hdf5_output
 * 
 * This is an empty implementation that just returns success to allow
 * core-only builds to compile.
 */
int32_t placeholder_prepare_galaxy_for_hdf5_output(const struct GALAXY *g, 
                                                  struct save_info *save_info,
                                                  const int32_t output_snap_idx, 
                                                  const struct halo_data *halos,
                                                  const int64_t task_forestnr,
                                                  const int64_t original_treenr,
                                                  const struct params *run_params)
{
    /* This function shouldn't be called directly in core-only mode */
    /* The real implementation is conditionally compiled in save_gals_hdf5.c */
    return EXIT_SUCCESS;
}
