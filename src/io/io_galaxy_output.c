#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "io_galaxy_output.h"
#include "io_interface.h"
#include "../core/core_mymalloc.h"

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
int allocate_output_arrays(const int numgals,
                          const int max_snapshots,
                          struct galaxy_output_context *ctx) {
    
    // Allocate and initialize output galaxy count array
    ctx->output_gal_count = mymalloc(max_snapshots * sizeof(*(ctx->output_gal_count)));
    if (ctx->output_gal_count == NULL) {
        fprintf(stderr, "Error: Could not allocate memory for output galaxy count array\n");
        return MALLOC_FAILURE;
    }
    
    // Initialize all snapshot counts to 0
    for (int snap_idx = 0; snap_idx < max_snapshots; snap_idx++) {
        ctx->output_gal_count[snap_idx] = 0;
    }
    
    // Allocate output galaxy order array
    ctx->output_gal_order = mymalloc(numgals * sizeof(*(ctx->output_gal_order)));
    if (ctx->output_gal_order == NULL) {
        fprintf(stderr, "Error: Could not allocate memory for output galaxy order array\n");
        myfree(ctx->output_gal_count);
        ctx->output_gal_count = NULL;
        return MALLOC_FAILURE;
    }
    
    // Initialize all galaxy orders to -1 (invalid)
    for (int gal_idx = 0; gal_idx < numgals; gal_idx++) {
        ctx->output_gal_order[gal_idx] = -1;
    }
    
    return EXIT_SUCCESS;
}

/**
 * @brief Free output tracking arrays
 * 
 * Frees arrays allocated by allocate_output_arrays.
 * 
 * @param ctx Output context structure containing arrays to free
 */
void free_output_arrays(struct galaxy_output_context *ctx) {
    if (ctx->output_gal_count != NULL) {
        myfree(ctx->output_gal_count);
        ctx->output_gal_count = NULL;
    }
    
    if (ctx->output_gal_order != NULL) {
        myfree(ctx->output_gal_order);
        ctx->output_gal_order = NULL;
    }
}

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
int prepare_galaxies_for_output(const int64_t task_forestnr, 
                               struct halo_data *halos,
                               struct forest_info *forest_info,
                               struct halo_aux_data *haloaux, 
                               struct GALAXY *halogal,
                               int numgals,
                               struct galaxy_output_context *ctx,
                               const struct params *run_params) {
    
    int status;
    
    // Allocate output tracking arrays
    status = allocate_output_arrays(numgals, run_params->simulation.SimMaxSnaps, ctx);
    if (status != EXIT_SUCCESS) {
        return status;
    }
    
    // Get the tree and file numbers from the original simulation file
    ctx->original_forestnr = forest_info->original_treenr[task_forestnr];
    ctx->original_filenr = forest_info->FileNr[task_forestnr];
    ctx->file_mulfac = run_params->runtime.FileNr_Mulfac;
    ctx->forest_mulfac = run_params->runtime.ForestNr_Mulfac;
    
    // Reset the output snapshot count and tracking arrays
    for (int snap_idx = 0; snap_idx < run_params->simulation.SimMaxSnaps; snap_idx++) {
        ctx->output_gal_count[snap_idx] = 0;
    }
    
    for (int gal_idx = 0; gal_idx < numgals; gal_idx++) {
        ctx->output_gal_order[gal_idx] = -1;
        haloaux[gal_idx].output_snap_n = -1;
    }
    
    // For each output snapshot, count galaxies and assign output indices
    for (int snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        int output_snap = run_params->simulation.ListOutputSnaps[snap_idx];
        
        for (int gal_idx = 0; gal_idx < numgals; gal_idx++) {
            if (halogal[gal_idx].SnapNum == output_snap) {
                // Assign output index and track which snapshot this galaxy is in
                ctx->output_gal_order[gal_idx] = ctx->output_gal_count[snap_idx];
                ctx->output_gal_count[snap_idx]++;
                haloaux[gal_idx].output_snap_n = snap_idx;
            }
        }
    }
    
    // Update merge pointers to reference output indices
    status = update_merger_pointers_for_output(halogal, numgals, ctx->output_gal_order, task_forestnr);
    if (status != EXIT_SUCCESS) {
        free_output_arrays(ctx);
        return status;
    }
    
    // Generate unique IDs for each galaxy
    status = generate_unique_galaxy_indices(halos, haloaux, halogal, numgals,
                                          ctx->original_forestnr, ctx->original_filenr,
                                          ctx->file_mulfac, ctx->forest_mulfac,
                                          run_params->io.TreeType);
    if (status != EXIT_SUCCESS) {
        free_output_arrays(ctx);
        return status;
    }
    
    return EXIT_SUCCESS;
}

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
int update_merger_pointers_for_output(struct GALAXY *halogal,
                                     const int64_t numgals,
                                     const int32_t *output_gal_order,
                                     const int64_t task_forestnr) {
    
    for (int gal_idx = 0; gal_idx < numgals; gal_idx++) {
        const int mergeID = halogal[gal_idx].mergeIntoID;
        if (mergeID > -1) {
            if (!(mergeID >= 0 && mergeID < numgals)) {
                fprintf(stderr, "Error: For galaxy number %d, expected mergeintoID to be within [0, %"PRId64") "
                        "but found mergeintoID = %d instead\n", gal_idx, numgals, mergeID);
                fprintf(stderr, "Additional debugging info\n");
                fprintf(stderr, "task_forestnr = %"PRId64" snapshot = %d\n",
                        task_forestnr, halogal[gal_idx].SnapNum);
                return -1;
            }
            halogal[gal_idx].mergeIntoID = output_gal_order[halogal[gal_idx].mergeIntoID];
        }
    }
    
    return EXIT_SUCCESS;
}

/**
 * @brief Get error message for tree type
 * 
 * Returns a helpful error message specific to the tree type.
 * 
 * @param tree_type Tree type
 * @return Error message string
 */
static const char *get_tree_type_error_message(enum Valid_TreeTypes tree_type) {
    switch (tree_type) {
#ifdef HDF5
        case lhalo_hdf5:
            return "It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "
                   "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 264 in src/io/read_tree_lhalo_hdf5.c. "
                   "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "
                   "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.";
            
        case gadget4_hdf5:
            return "It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "
                   "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 536 in src/io/read_tree_gadget4_hdf5.c. "
                   "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "
                   "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.";
            
        case genesis_hdf5:
            return "It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "
                   "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 492 in src/io/read_tree_genesis_hdf5.c. "
                   "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "
                   "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.";
            
        case consistent_trees_hdf5:
            return "It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "
                   "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 389 in src/io/read_tree_consistentrees_hdf5.c. "
                   "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "
                   "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.";
#endif
            
        case lhalo_binary:
            return "It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "
                   "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 226 in src/io/read_tree_lhalo_binary.c. "
                   "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "
                   "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.";
            
        case consistent_trees_ascii:
            return "It is likely that you have a tree with too many galaxies. For consistent trees the number of galaxies per trees "
                   "is limited for the ID to to fit in 64 bits, see run_params->runtime.ForestNr_Mulfac at l. 319 in src/io/read_tree_consistentrees_ascii.c. "
                   "If you have not set a finite run_params->runtime.FileNr_Mulfac, this format may not be ideal for your purpose.";
            
        default:
            return "Your tree type has not been included in the switch statement. Please check your configuration.";
    }
}

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
int generate_unique_galaxy_indices(const struct halo_data *halos,
                                  const struct halo_aux_data *haloaux,
                                  struct GALAXY *halogal,
                                  const int64_t numgals,
                                  const int64_t forestnr,
                                  const int32_t filenr,
                                  const int64_t filenr_mulfac,
                                  const int64_t forestnr_mulfac,
                                  const enum Valid_TreeTypes tree_type) {
    
    // For each galaxy, generate a unique ID
    for (int64_t gal_idx = 0; gal_idx < numgals; ++gal_idx) {
        struct GALAXY *this_gal = &halogal[gal_idx];
        
        // Get galaxy number and central galaxy number
        int32_t GalaxyNr = this_gal->GalaxyNr;
        int32_t CentralGalaxyNr = halogal[haloaux[halos[this_gal->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr;
        
        // Check that the mechanism would produce unique galaxy index within this run
        if (GalaxyNr > forestnr_mulfac || (filenr_mulfac > 0 && forestnr*forestnr_mulfac > filenr_mulfac)) {
            fprintf(stderr, "When determining a unique Galaxy Number, we assume two things:\n"
                    "1. Current galaxy number = %d is less than multiplication factor for trees (=%"PRId64")\n"
                    "2. That (the total number of trees * tree multiplication factor = %"PRId64") is less than the file "
                    "multiplication factor = %"PRId64" (only relevant if file multiplication factor is non-zero).\n"
                    "At least one of these two assumptions have been broken.\n"
                    "Simulation trees file number %d\tOriginal tree number %"PRId64"\tGalaxy Number %d "
                    "forestnr_mulfac = %"PRId64" forestnr*forestnr_mulfac = %"PRId64"\n", 
                    GalaxyNr, forestnr_mulfac, forestnr*forestnr_mulfac, filenr_mulfac,
                    filenr, forestnr, GalaxyNr, forestnr_mulfac, forestnr*forestnr_mulfac);
            
            // Get tree-type specific error message
            fprintf(stderr, "%s\n", get_tree_type_error_message(tree_type));
            
            return EXIT_FAILURE;
        }
        
        // Check for overflow when computing IDs
        if ((forestnr > 0 && ((uint64_t)forestnr > (0xFFFFFFFFFFFFFFFFULL/(uint64_t)forestnr_mulfac))) ||
            (filenr > 0 && ((uint64_t)filenr > (0xFFFFFFFFFFFFFFFFULL/(uint64_t)filenr_mulfac)))) {
            fprintf(stderr, "Error: While generating an unique Galaxy Index. The multiplication required to "
                    "generate the ID will overflow 64-bit\n"
                    "forestnr = %"PRId64" forestnr_mulfac = %"PRId64" filenr = %d filenr_mulfac = %"PRId64"\n",
                    forestnr, forestnr_mulfac, filenr, filenr_mulfac);
            
            return EXIT_FAILURE;
        }
        
        // Compute intermediate values
        const uint64_t id_from_forestnr = forestnr_mulfac * forestnr;
        const uint64_t id_from_filenr = filenr_mulfac * filenr;
        
        // Check for overflow during addition
        if (id_from_forestnr > (0xFFFFFFFFFFFFFFFFULL - id_from_filenr)) {
            fprintf(stderr, "Error: While generating an unique Galaxy Index. The addition required to generate the ID will overflow 64-bits\n");
            fprintf(stderr, "id_from_forestnr = %"PRIu64" id_from_filenr = %"PRIu64"\n", id_from_forestnr, id_from_filenr);
            return EXIT_FAILURE;
        }
        
        // Compute combined base ID
        const uint64_t id_from_forest_and_file = id_from_forestnr + id_from_filenr;
        
        // Check for overflow when adding galaxy number
        if (((uint64_t)GalaxyNr > (0xFFFFFFFFFFFFFFFFULL - id_from_forest_and_file)) ||
            ((uint64_t)CentralGalaxyNr > (0xFFFFFFFFFFFFFFFFULL - id_from_forest_and_file))) {
            fprintf(stderr, "Error: While generating an unique Galaxy Index. The addition required to generate the ID will overflow 64-bits\n");
            fprintf(stderr, "id_from_forest_and_file = %"PRIu64" GalaxyNr = %d CentralGalaxyNr = %d\n", 
                    id_from_forest_and_file, GalaxyNr, CentralGalaxyNr);
            return EXIT_FAILURE;
        }
        
        // Generate the final unique indices
        this_gal->GalaxyIndex = GalaxyNr + id_from_forest_and_file;
        this_gal->CentralGalaxyIndex = CentralGalaxyNr + id_from_forest_and_file;
    }
    
    return EXIT_SUCCESS;
}