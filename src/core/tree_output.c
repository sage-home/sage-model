#include "tree_output.h"
#include "core_save.h"
#include "core_logging.h"
#include "core_mymalloc.h"
#include "galaxy_array.h"

// External functions we need
extern void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params);
extern void free_galaxy_properties(struct GALAXY *galaxy);

int output_tree_galaxies(TreeContext* ctx, int64_t forestnr,
                        struct save_info* save_info,
                        struct forest_info* forest_info) {
    
    if (!ctx || !save_info || !forest_info) {
        LOG_ERROR("Invalid parameters to output_tree_galaxies");
        return EXIT_FAILURE;
    }
    
    // Group galaxies by snapshot for output
    int ngal_total = galaxy_array_get_count(ctx->output_galaxies);
    LOG_INFO("Outputting %d total galaxies from tree processing for forest %ld", 
             ngal_total, forestnr);
    
    if (ngal_total == 0) {
        LOG_INFO("No galaxies to output for forest %ld", forestnr);
        return EXIT_SUCCESS;
    }
    
    for (int snap = 0; snap < ctx->run_params->simulation.SimMaxSnaps; snap++) {
        // Count galaxies at this snapshot
        int snap_count = 0;
        for (int i = 0; i < ngal_total; i++) {
            struct GALAXY* gal = galaxy_array_get(ctx->output_galaxies, i);
            if (gal && GALAXY_PROP_SnapNum(gal) == snap) {
                snap_count++;
            }
        }
        
        if (snap_count == 0) {
            continue;  // No galaxies at this snapshot
        }
        
        LOG_DEBUG("Processing %d galaxies for snapshot %d", snap_count, snap);
        
        // Allocate arrays for this snapshot
        struct GALAXY* snap_galaxies = mymalloc(snap_count * sizeof(struct GALAXY));
        if (!snap_galaxies) {
            LOG_ERROR("Failed to allocate %d galaxies for snapshot %d output", 
                     snap_count, snap);
            return EXIT_FAILURE;
        }
        
        struct halo_aux_data* haloaux = mycalloc(ctx->nhalos, 
                                               sizeof(struct halo_aux_data));
        if (!haloaux) {
            LOG_ERROR("Failed to allocate halo auxiliary data for snapshot %d", snap);
            myfree(snap_galaxies);
            return EXIT_FAILURE;
        }
        
        // Collect galaxies for this snapshot and update haloaux
        int idx = 0;
        for (int i = 0; i < ngal_total; i++) {
            struct GALAXY* gal = galaxy_array_get(ctx->output_galaxies, i);
            if (gal && GALAXY_PROP_SnapNum(gal) == snap) {
                // Deep copy galaxy to output array
                deep_copy_galaxy(&snap_galaxies[idx], gal, ctx->run_params);
                
                // Update halo auxiliary data for save_galaxies
                int halo_nr = GALAXY_PROP_HaloNr(&snap_galaxies[idx]);
                if (halo_nr >= 0 && halo_nr < ctx->nhalos) {
                    if (haloaux[halo_nr].NGalaxies == 0) {
                        haloaux[halo_nr].FirstGalaxy = idx;
                    }
                    haloaux[halo_nr].NGalaxies++;
                } else {
                    LOG_WARNING("Galaxy %d has invalid halo number %d (max %ld)", 
                               idx, halo_nr, ctx->nhalos - 1);
                }
                
                idx++;
            }
        }
        
        if (idx != snap_count) {
            LOG_ERROR("Mismatch in galaxy count: expected %d, collected %d", 
                     snap_count, idx);
            // Continue anyway - this shouldn't be fatal
        }
        
        // Save using existing infrastructure
        int32_t save_status = save_galaxies(forestnr, snap_count, ctx->halos, forest_info,
                                          haloaux, snap_galaxies, save_info, ctx->run_params);
        
        if (save_status != EXIT_SUCCESS) {
            LOG_ERROR("Failed to save %d galaxies for snapshot %d in forest %ld", 
                     snap_count, snap, forestnr);
            
            // Cleanup before returning error
            for (int i = 0; i < snap_count; i++) {
                free_galaxy_properties(&snap_galaxies[i]);
            }
            myfree(snap_galaxies);
            myfree(haloaux);
            return save_status;
        }
        
        LOG_DEBUG("Successfully saved %d galaxies for snapshot %d", snap_count, snap);
        
        // Cleanup snapshot-specific allocations
        for (int i = 0; i < snap_count; i++) {
            free_galaxy_properties(&snap_galaxies[i]);
        }
        myfree(snap_galaxies);
        myfree(haloaux);
    }
    
    LOG_INFO("Successfully output all %d galaxies for forest %ld", ngal_total, forestnr);
    return EXIT_SUCCESS;
}