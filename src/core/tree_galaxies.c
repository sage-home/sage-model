#include "tree_galaxies.h"
#include "core_build_model.h"
#include "core_logging.h"
#include "core_galaxy_extensions.h"

// Import from core_build_model.c
extern void init_galaxy(const int p, const int halonr, int *galaxycounter, 
                       struct halo_data *halos, struct GALAXY *galaxies, 
                       struct params *run_params);

extern double get_virial_mass(const int halonr, struct halo_data *halos, struct params *run_params);
extern double get_virial_radius(const int halonr, struct halo_data *halos, struct params *run_params);
extern double get_virial_velocity(const int halonr, struct halo_data *halos, struct params *run_params);

int measure_tree_gap(int descendant_snap, int progenitor_snap) {
    // Gaps occur when snapshots are not consecutive
    int gap = descendant_snap - progenitor_snap - 1;
    return (gap > 0) ? gap : 0;
}

void update_galaxy_for_new_halo(struct GALAXY* galaxy, int halo_nr, TreeContext* ctx) {
    // Update galaxy properties for inheritance to new halo
    struct halo_data* halo = &ctx->halos[halo_nr];
    
    // Update basic halo assignment
    GALAXY_PROP_HaloNr(galaxy) = halo_nr;
    GALAXY_PROP_SnapNum(galaxy) = halo->SnapNum;
    
    // Update masses and positions from new halo
    for (int j = 0; j < 3; j++) {
        GALAXY_PROP_Pos(galaxy)[j] = halo->Pos[j];
        GALAXY_PROP_Vel(galaxy)[j] = halo->Vel[j];
        GALAXY_PROP_Spin(galaxy)[j] = halo->Spin[j];
    }
    
    // Update virial properties using helper functions
    GALAXY_PROP_Mvir(galaxy) = get_virial_mass(halo_nr, ctx->halos, ctx->run_params);
    GALAXY_PROP_Rvir(galaxy) = get_virial_radius(halo_nr, ctx->halos, ctx->run_params);
    GALAXY_PROP_Vvir(galaxy) = get_virial_velocity(halo_nr, ctx->halos, ctx->run_params);
    GALAXY_PROP_Vmax(galaxy) = halo->Vmax;
}

int collect_halo_galaxies(int halo_nr, TreeContext* ctx) {
    struct halo_data* halo = &ctx->halos[halo_nr];
    
    // Count galaxies from all progenitors
    int total_prog_galaxies = 0;
    int prog = halo->FirstProgenitor;
    
    while (prog >= 0) {
        total_prog_galaxies += ctx->halo_galaxy_count[prog];
        
        // Check for gaps
        int gap = measure_tree_gap(halo->SnapNum, ctx->halos[prog].SnapNum);
        if (gap > 0) {
            ctx->total_gaps_spanned++;
            if (gap > ctx->max_gap_length) {
                ctx->max_gap_length = gap;
            }
            LOG_DEBUG("Spanning gap of %d snapshots: %d -> %d", 
                     gap, ctx->halos[prog].SnapNum, halo->SnapNum);
        }
        
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    if (total_prog_galaxies == 0 && halo->FirstProgenitor == -1) {
        // No progenitors - create primordial galaxy
        if (halo_nr == halo->FirstHaloInFOFgroup) {
            struct GALAXY new_galaxy;
            memset(&new_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&new_galaxy);
            
            init_galaxy(0, halo_nr, &ctx->galaxy_counter, ctx->halos, 
                       &new_galaxy, ctx->run_params);
            
            // Store in working array
            int idx = galaxy_array_append(ctx->working_galaxies, &new_galaxy, 
                                        ctx->run_params);
            
            // Update mapping
            ctx->halo_first_galaxy[halo_nr] = idx;
            ctx->halo_galaxy_count[halo_nr] = 1;
            
            free_galaxy_properties(&new_galaxy);
            
            LOG_DEBUG("Created primordial galaxy for halo %d", halo_nr);
        }
    }
    
    return EXIT_SUCCESS;
}

int inherit_galaxies_with_orphans(int halo_nr, TreeContext* ctx) {
    struct halo_data* halo = &ctx->halos[halo_nr];
    
    // Find most massive progenitor with galaxies (first_occupied)
    int first_occupied = -1;
    int max_len = 0;
    
    int prog = halo->FirstProgenitor;
    while (prog >= 0) {
        if (ctx->halo_galaxy_count[prog] > 0) {
            if (ctx->halos[prog].Len > max_len) {
                max_len = ctx->halos[prog].Len;
                first_occupied = prog;
            }
        }
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    if (first_occupied == -1) {
        return EXIT_SUCCESS;  // No galaxies to inherit
    }
    
    // Track where galaxies for this halo start in the working array
    int halo_start_idx = galaxy_array_get_count(ctx->working_galaxies);
    int inherited_count = 0;
    
    // Process all progenitors
    prog = halo->FirstProgenitor;
    while (prog >= 0) {
        if (ctx->halo_galaxy_count[prog] > 0) {
            // Get galaxies from this progenitor
            int start_idx = ctx->halo_first_galaxy[prog];
            
            for (int i = 0; i < ctx->halo_galaxy_count[prog]; i++) {
                struct GALAXY* src = galaxy_array_get(ctx->working_galaxies, 
                                                     start_idx + i);
                
                struct GALAXY inherited;
                deep_copy_galaxy(&inherited, src, ctx->run_params);
                
                if (prog == first_occupied) {
                    // Main branch - update properties for new halo
                    update_galaxy_for_new_halo(&inherited, halo_nr, ctx);
                } else {
                    // Other branches - create orphans
                    GALAXY_PROP_Type(&inherited) = 2;  // Orphan
                    GALAXY_PROP_merged(&inherited) = 1; // FIXED: Mark as merged to filter from output
                    GALAXY_PROP_Mvir(&inherited) = 0.0;
                    GALAXY_PROP_HaloNr(&inherited) = halo_nr;
                    GALAXY_PROP_SnapNum(&inherited) = halo->SnapNum;
                    ctx->total_orphans++;
                    LOG_DEBUG("Created orphan from disrupted halo %d", prog);
                }
                
                // Add to working array
                galaxy_array_append(ctx->working_galaxies, &inherited, 
                                  ctx->run_params);
                inherited_count++;
                free_galaxy_properties(&inherited);
            }
        }
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    // Update mapping for this halo
    if (inherited_count > 0) {
        ctx->halo_first_galaxy[halo_nr] = halo_start_idx;
        ctx->halo_galaxy_count[halo_nr] = inherited_count;
    }
    
    return EXIT_SUCCESS;
}