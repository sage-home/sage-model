#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file io_galaxy_output.h
 * @brief Utilities for preparing galaxy data for output
 *
 * This file provides functionality for preparing galaxy data for output,
 * including generating unique galaxy indices, mapping between internal and
 * output galaxy indices, and handling galaxy properties consistently.
 */

#include "../core/core_allvars.h"

/**
 * @brief Output context for galaxy preparation
 *
 * Contains information needed to prepare galaxies for output.
 */
struct galaxy_output_context {
    int32_t *output_gal_count;       /**< Count of galaxies per snapshot */
    int32_t *output_gal_order;       /**< Mapping from internal to output indices */
    int64_t file_mulfac;             /**< Multiplication factor for file IDs */
    int64_t forest_mulfac;           /**< Multiplication factor for forest IDs */
    int64_t original_forestnr;       /**< Original forest number from file */
    int32_t original_filenr;         /**< Original file number */
};

/**
 * @brief Prepare galaxies for output
 * 
 * Sets up output indices and mappings for all galaxies.
 * 
 * @param task_forestnr Task-local forest number
 * @param halos Halo data array
 * @param forest_info Forest information
 * @param haloaux Halo auxiliary data
 * @param halogal Galaxy data array
 * @param numgals Number of galaxies
 * @param ctx Output context structure (will be filled)
 * @param run_params Runtime parameters
 * @return 0 on success, non-zero on failure
 */
extern int prepare_galaxies_for_output(const int64_t task_forestnr, 
                                      struct halo_data *halos,
                                      struct forest_info *forest_info,
                                      struct halo_aux_data *haloaux, 
                                      struct GALAXY *halogal,
                                      int numgals,
                                      struct galaxy_output_context *ctx,
                                      const struct params *run_params);

/**
 * @brief Generate unique galaxy indices
 * 
 * Creates unique IDs for each galaxy based on file, forest, and galaxy numbers.
 * 
 * @param halos Halo data array
 * @param haloaux Halo auxiliary data
 * @param halogal Galaxy data array
 * @param numgals Number of galaxies
 * @param forestnr Forest number
 * @param filenr File number
 * @param filenr_mulfac File number multiplication factor
 * @param forestnr_mulfac Forest number multiplication factor
 * @param tree_type Type of merger tree
 * @return 0 on success, non-zero on failure
 */
extern int generate_unique_galaxy_indices(const struct halo_data *halos,
                                         const struct halo_aux_data *haloaux,
                                         struct GALAXY *halogal,
                                         const int64_t numgals,
                                         const int64_t forestnr,
                                         const int32_t filenr,
                                         const int64_t filenr_mulfac,
                                         const int64_t forestnr_mulfac,
                                         const enum Valid_TreeTypes tree_type);

/**
 * @brief Update merger pointers for output
 * 
 * Updates mergeIntoID values to point to the correct output indices.
 * 
 * @param halogal Galaxy data array
 * @param numgals Number of galaxies
 * @param output_gal_order Mapping from internal to output indices
 * @param task_forestnr Task-local forest number for debugging
 * @return 0 on success, non-zero on failure
 */
extern int update_merger_pointers_for_output(struct GALAXY *halogal,
                                            const int64_t numgals,
                                            const int32_t *output_gal_order,
                                            const int64_t task_forestnr);

/**
 * @brief Allocate output tracking arrays
 * 
 * Creates arrays for tracking output galaxies.
 * 
 * @param numgals Number of galaxies
 * @param max_snapshots Maximum number of snapshots
 * @param ctx Output context structure (arrays will be allocated here)
 * @return 0 on success, non-zero on failure
 */
extern int allocate_output_arrays(const int numgals,
                                 const int max_snapshots,
                                 struct galaxy_output_context *ctx);

/**
 * @brief Free output tracking arrays
 * 
 * Frees arrays allocated by allocate_output_arrays.
 * 
 * @param ctx Output context structure containing arrays to free
 */
extern void free_output_arrays(struct galaxy_output_context *ctx);

#ifdef __cplusplus
}
#endif