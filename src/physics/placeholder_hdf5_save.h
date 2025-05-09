/**
 * @file placeholder_hdf5_save.h
 * @brief Placeholder macros for physics properties in HDF5 save routines
 *
 * This file provides placeholder functions for HDF5 save routines that
 * reference physics properties. These are used in core-only mode.
 */

#ifndef PLACEHOLDER_HDF5_SAVE_H
#define PLACEHOLDER_HDF5_SAVE_H

struct GALAXY;
struct save_info;
struct halo_data;
struct params;

/* Empty functions in core-only mode to avoid physics member access */
int32_t placeholder_prepare_galaxy_for_hdf5_output(const struct GALAXY *g, 
                                                  struct save_info *save_info,
                                                  const int32_t output_snap_idx, 
                                                  const struct halo_data *halos,
                                                  const int64_t task_forestnr,
                                                  const int64_t original_treenr,
                                                  const struct params *run_params);

#endif /* PLACEHOLDER_HDF5_SAVE_H */