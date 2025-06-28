#include "tree_physics.h"
#include "core_build_model.h"
#include "core_logging.h"
#include "core_mymalloc.h"
#include "galaxy_array.h"

// External functions we need
extern void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params);
extern void free_galaxy_properties(struct GALAXY *galaxy);

int apply_physics_to_fof(int fof_root, TreeContext* ctx) {
    // Handle NULL context gracefully
    if (!ctx) {
        return EXIT_FAILURE;
    }
    
    LOG_DEBUG("Applying physics to FOF group %d", fof_root);
    
    // Collect FOF galaxies into temporary array
    GalaxyArray* fof_galaxies = galaxy_array_new();
    if (!fof_galaxies) {
        LOG_ERROR("Failed to create FOF galaxies array for group %d", fof_root);
        return EXIT_FAILURE;
    }
    
    // Traverse all halos in the FOF group and collect their galaxies
    int current = fof_root;
    int total_galaxies = 0;
    
    while (current >= 0) {
        int start = ctx->halo_first_galaxy[current];
        int count = ctx->halo_galaxy_count[current];
        
        // Add galaxies from this halo to the FOF collection
        for (int i = 0; i < count; i++) {
            struct GALAXY* gal = galaxy_array_get(ctx->working_galaxies, start + i);
            if (gal) {
                int append_result = galaxy_array_append(fof_galaxies, gal, ctx->run_params);
                if (append_result < 0) {
                    LOG_ERROR("Failed to append galaxy to FOF array for halo %d", current);
                    galaxy_array_free(&fof_galaxies);
                    return EXIT_FAILURE;
                }
                total_galaxies++;
            }
        }
        
        current = ctx->halos[current].NextHaloInFOFgroup;
    }
    
    if (total_galaxies == 0) {
        LOG_DEBUG("No galaxies to evolve in FOF group %d", fof_root);
        galaxy_array_free(&fof_galaxies);
        return EXIT_SUCCESS;
    }
    
    LOG_DEBUG("Collected %d galaxies for FOF group %d physics evolution", total_galaxies, fof_root);
    
    // **CRITICAL: Set central galaxy indices for physics validation**
    // Physics pipeline requires all galaxies to have valid CentralGal assignments
    int central_idx = -1;
    
    // Find the central galaxy (Type == 0) in the FOF group
    for (int i = 0; i < total_galaxies; i++) {
        struct GALAXY* gal = galaxy_array_get(fof_galaxies, i);
        if (gal && GALAXY_PROP_Type(gal) == 0) {
            central_idx = i;
            break;
        }
    }
    
    // Assign central galaxy indices to all galaxies in the FOF group
    for (int i = 0; i < total_galaxies; i++) {
        struct GALAXY* gal = galaxy_array_get(fof_galaxies, i);
        if (gal) {
            GALAXY_PROP_CentralGal(gal) = central_idx;
        }
    }
    
    LOG_DEBUG("Assigned central galaxy index %d for FOF group %d", central_idx, fof_root);
    
    // Create auxiliary data structure (required by evolve_galaxies)
    struct halo_aux_data* temp_aux = mycalloc(ctx->nhalos, sizeof(struct halo_aux_data));
    if (!temp_aux) {
        LOG_ERROR("Failed to allocate auxiliary data for FOF group %d", fof_root);
        galaxy_array_free(&fof_galaxies);
        return EXIT_FAILURE;
    }
    
    // Apply real physics using the wrapper function
    int numgals = 0;  // Will be updated by evolve_galaxies_wrapper
    int status = evolve_galaxies_wrapper(fof_root, fof_galaxies, &numgals,
                                        ctx->halos, temp_aux, 
                                        ctx->output_galaxies, 
                                        ctx->run_params);
    
    if (status == EXIT_SUCCESS) {
        LOG_DEBUG("Successfully evolved %d galaxies in FOF group %d using full physics pipeline", 
                 numgals, fof_root);
    } else {
        LOG_ERROR("Physics evolution failed for FOF group %d", fof_root);
    }
    
    // Cleanup
    galaxy_array_free(&fof_galaxies);
    myfree(temp_aux);
    
    if (status == EXIT_SUCCESS) {
        LOG_DEBUG("Successfully applied physics to FOF group %d", fof_root);
    } else {
        LOG_ERROR("Physics application failed for FOF group %d", fof_root);
    }
    
    return status;
}