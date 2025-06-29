/**
 * @file    core_build_model.c
 * @brief   Core galaxy construction and evolution functions
 *
 * Main galaxy processing functions:
 * - process_fof_group(): Entry point for FOF group processing
 * - evolve_galaxies(): Time integration with 4-phase physics pipeline
 * - copy_galaxies_from_progenitors(): Galaxy inheritance from merger trees
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "core_allvars.h"
#include "core_build_model.h"
#include "core_init.h"
#include "core_mymalloc.h"
#include "core_logging.h"
#include "core_galaxy_extensions.h"
#include "core_pipeline_system.h"
#include "core_merger_queue.h"
#include "core_merger_processor.h"
#include "core_evolution_diagnostics.h"
#include "galaxy_array.h"

// Forward declarations
double get_virial_mass(const int halonr, struct halo_data *halos, struct params *run_params);
double get_virial_radius(const int halonr, struct halo_data *halos, struct params *run_params);
double get_virial_velocity(const int halonr, struct halo_data *halos, struct params *run_params);
void init_galaxy(const int p, const int halonr, int *galaxycounter, struct halo_data *halos,
                struct GALAXY *galaxies, struct params *run_params);

/**
 * @brief Lazy property allocation helper
 * @param galaxy Galaxy structure to check
 * @param run_params SAGE parameters structure
 * 
 * Called by: Evolution pipeline functions
 * Calls: allocate_galaxy_properties() - create property memory
 */
static inline void ensure_galaxy_properties(struct GALAXY *galaxy, struct params *run_params) {
    if (galaxy->properties == NULL) {
        if (allocate_galaxy_properties(galaxy, run_params) != 0) {
            LOG_WARNING("Failed to allocate properties for galaxy");
        }
    }
}

/**
 * @brief Safe galaxy duplication with complete property handling
 * @param dest Destination galaxy structure
 * @param src Source galaxy structure to copy
 * @param run_params SAGE parameters structure
 * 
 * Called by: Progenitor copying, evolution output
 * Calls: galaxy_extension_copy() - copy extension data
 *        copy_galaxy_properties() - duplicate property arrays
 */
void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params)
{
    dest->num_extensions = src->num_extensions;
    dest->extension_flags = src->extension_flags;
    dest->extension_data = NULL;
    dest->properties = NULL;
    
    if (src->extension_data != NULL && src->num_extensions > 0) {
        if (galaxy_extension_copy(dest, src) != 0) {
            LOG_ERROR("Failed to copy galaxy extension data. Source GalaxyIndex: %llu", GALAXY_PROP_GalaxyIndex(src));
        }
    }
    
    if (copy_galaxy_properties(dest, src, run_params) != 0) {
        LOG_ERROR("Failed to copy galaxy properties. Source GalaxyIndex: %llu", GALAXY_PROP_GalaxyIndex(src));
    }
}

/**
 * Find the most massive occupied progenitor for a given halo.
 * Returns the progenitor index, defaulting to FirstProgenitor
 */
static int find_most_massive_occupied_progenitor(int halonr,
                                                 struct halo_data* halos,
                                                 const GalaxyArray* prev_galaxies) {
    int first_occupied = halos[halonr].FirstProgenitor;
    int prog = halos[halonr].FirstProgenitor;
    
    // Check if FirstProgenitor has galaxies
    if (prog >= 0) {
        int galaxy_count = 0;
        int ngal = galaxy_array_get_count(prev_galaxies);
        
        for (int i = 0; i < ngal; i++) {
            struct GALAXY* g = galaxy_array_get((GalaxyArray*)prev_galaxies, i);
            if (g && GALAXY_PROP_HaloNr(g) == prog) {
                galaxy_count++;
            }
        }
        
        // If FirstProgenitor has galaxies, use it (no search needed)
        if (galaxy_count > 0) {
            return first_occupied;  // Early return - much more efficient!
        }
    }
    
    // FirstProgenitor has no galaxies - search for most massive occupied progenitor
    int lenoccmax = 0;
    while (prog >= 0) {
        // Count galaxies in this progenitor
        int galaxy_count = 0;
        int ngal = galaxy_array_get_count(prev_galaxies);
        
        for (int i = 0; i < ngal; i++) {
            struct GALAXY* g = galaxy_array_get((GalaxyArray*)prev_galaxies, i);
            if (g && GALAXY_PROP_HaloNr(g) == prog) {
                galaxy_count++;
            }
        }
        
        // Look for most massive progenitor that has galaxies
        if (halos[prog].Len > lenoccmax && galaxy_count > 0) {
            lenoccmax = halos[prog].Len;
            first_occupied = prog;
        }
        
        prog = halos[prog].NextProgenitor;
    }
    
    return first_occupied;  // Returns FirstProgenitor if no occupied progenitors found
}

/**
 * @brief Progenitor inheritance processor with type classification - direct galaxy scanning approach
 * @param halonr Current halo number
 * @param fof_halonr FOF group root halo number
 * @param temp_fof_galaxies Output FOF galaxy array (direct append)
 * @param galaxycounter Global galaxy counter
 * @param halos Halo data array
 * @param galaxies_prev_snap Previous snapshot galaxies
 * @param run_params SAGE parameters structure
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 * 
 * Called by: process_halo_galaxies()
 * Calls: deep_copy_galaxy() - duplicate galaxy safely
 *        get_virial_mass() - calculate halo virial mass
 *        init_galaxy() - create new galaxy
 *        galaxy_array_append() - add directly to FOF array
 * 
 */
static int copy_galaxies_from_progenitors(const int halonr, const int fof_halonr, GalaxyArray *temp_fof_galaxies,
                                         int32_t *galaxycounter, struct halo_data *halos,
                                         const GalaxyArray *galaxies_prev_snap, struct params *run_params, bool *processed_flags)
{
    if (!galaxies_prev_snap) {
        // No previous snapshot, create new galaxy if this is main FOF halo
        if (halonr == fof_halonr) {
            struct GALAXY temp_new_galaxy;
            memset(&temp_new_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_new_galaxy);
            init_galaxy(0, halonr, galaxycounter, halos, &temp_new_galaxy, run_params);

            if (galaxy_array_append(temp_fof_galaxies, &temp_new_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append new galaxy to FOF array");
                free_galaxy_properties(&temp_new_galaxy);
                return EXIT_FAILURE;
            }
            free_galaxy_properties(&temp_new_galaxy);
        }
        return EXIT_SUCCESS;
    }
    
    struct GALAXY *galaxies_prev_raw = galaxy_array_get_raw_data((GalaxyArray*)galaxies_prev_snap);
    int ngal_prev = galaxy_array_get_count(galaxies_prev_snap);

    // Pass 1: Find the most massive occupied progenitor
    int first_occupied = find_most_massive_occupied_progenitor(halonr, halos, galaxies_prev_snap);

    // Pass 2: Iterate through all progenitors again. Copy galaxies, classifying them
    // as central, satellite, or orphan based on whether they belong to the
    // 'first_occupied' progenitor halo.
    int galaxies_added_for_halo = 0;
    int prog = halos[halonr].FirstProgenitor;
    while (prog >= 0) {
        // Scan for galaxies belonging to this progenitor halo
        for (int i = 0; i < ngal_prev; i++) {
            if (GALAXY_PROP_HaloNr(&galaxies_prev_raw[i]) != prog) {
                continue;  // Galaxy doesn't belong to this progenitor
            }
            
            galaxies_added_for_halo++;
            const struct GALAXY* source_gal = &galaxies_prev_raw[i];

            // Create a temporary, deep copy of the progenitor galaxy to work with.
            struct GALAXY temp_galaxy;
            memset(&temp_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_galaxy);
            deep_copy_galaxy(&temp_galaxy, source_gal, run_params);

            GALAXY_PROP_HaloNr(&temp_galaxy) = halonr;
            GALAXY_PROP_dT(&temp_galaxy) = -1.0;
            
            ensure_galaxy_properties(&temp_galaxy, run_params);
            galaxy_extension_copy(&temp_galaxy, source_gal);

            if (GALAXY_PROP_Type(&temp_galaxy) == 0 || GALAXY_PROP_Type(&temp_galaxy) == 1) {

                const float previousMvir = GALAXY_PROP_Mvir(&temp_galaxy);
                const float previousVvir = GALAXY_PROP_Vvir(&temp_galaxy);
                const float previousVmax = GALAXY_PROP_Vmax(&temp_galaxy);

                if (prog == first_occupied) {
                    // Update galaxy properties for new host halo
                    for(int j = 0; j < 3; j++) {
                        GALAXY_PROP_Pos(&temp_galaxy)[j] = halos[halonr].Pos[j];
                        GALAXY_PROP_Vel(&temp_galaxy)[j] = halos[halonr].Vel[j];
                        GALAXY_PROP_Spin(&temp_galaxy)[j] = halos[halonr].Spin[j];
                    }
                    GALAXY_PROP_MostBoundID(&temp_galaxy) = halos[halonr].MostBoundID;
                    GALAXY_PROP_Len(&temp_galaxy) = halos[halonr].Len;
                    GALAXY_PROP_Vmax(&temp_galaxy) = halos[halonr].Vmax;

                    float new_mvir = get_virial_mass(halonr, halos, run_params);
                    GALAXY_PROP_deltaMvir(&temp_galaxy) = new_mvir - GALAXY_PROP_Mvir(&temp_galaxy);
                    
                    GALAXY_PROP_Mvir(&temp_galaxy) = new_mvir;
                    GALAXY_PROP_Rvir(&temp_galaxy) = get_virial_radius(halonr, halos, run_params);
                    GALAXY_PROP_Vvir(&temp_galaxy) = get_virial_velocity(halonr, halos, run_params);

                    if (halonr == fof_halonr) {
                        GALAXY_PROP_Type(&temp_galaxy) = 0;  // Central
                    } else {
                        if (GALAXY_PROP_Type(&temp_galaxy) == 0) {
                            // Record infall properties for central→satellite transition
                            GALAXY_PROP_infallMvir(&temp_galaxy) = previousMvir;
                            GALAXY_PROP_infallVvir(&temp_galaxy) = previousVvir;
                            GALAXY_PROP_infallVmax(&temp_galaxy) = previousVmax;
                        }
                        GALAXY_PROP_Type(&temp_galaxy) = 1;  // Satellite
                    }

                } else {
                    // Galaxy becomes orphan (lost its halo)
                    GALAXY_PROP_deltaMvir(&temp_galaxy) = -1.0 * GALAXY_PROP_Mvir(&temp_galaxy);
                    GALAXY_PROP_Mvir(&temp_galaxy) = 0.0;
                    
                    if (GALAXY_PROP_Type(&temp_galaxy) == 0) {
                        GALAXY_PROP_infallMvir(&temp_galaxy) = previousMvir;
                        GALAXY_PROP_infallVvir(&temp_galaxy) = previousVvir;
                        GALAXY_PROP_infallVmax(&temp_galaxy) = previousVmax;
                    }
                    
                    GALAXY_PROP_Type(&temp_galaxy) = 2;  // Orphan
                    GALAXY_PROP_merged(&temp_galaxy) = 1;
                }
            }
            
            if (galaxy_array_append(temp_fof_galaxies, &temp_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append galaxy to FOF array");
                free_galaxy_properties(&temp_galaxy);
                return EXIT_FAILURE;
            }
            free_galaxy_properties(&temp_galaxy);
            
            // Mark galaxy as processed
            if (processed_flags != NULL) {
                processed_flags[i] = true;
            }
        }
        prog = halos[prog].NextProgenitor;
    }

    // Create new galaxy if no progenitors and this is main FOF halo
    if (galaxies_added_for_halo == 0 && halonr == fof_halonr) {
        struct GALAXY temp_new_galaxy;
        memset(&temp_new_galaxy, 0, sizeof(struct GALAXY));
        galaxy_extension_initialize(&temp_new_galaxy);
        init_galaxy(0, halonr, galaxycounter, halos, &temp_new_galaxy, run_params);

        if (galaxy_array_append(temp_fof_galaxies, &temp_new_galaxy, run_params) < 0) {
            LOG_ERROR("Failed to append new galaxy to FOF array");
            free_galaxy_properties(&temp_new_galaxy);
            return EXIT_FAILURE;
        }
        free_galaxy_properties(&temp_new_galaxy);
    }
    
    return EXIT_SUCCESS;
}



// Internal function declarations
static int evolve_galaxies(const int fof_root_halonr, GalaxyArray* temp_fof_galaxies, int *numgals, 
                          struct halo_data *halos, struct halo_aux_data *haloaux, 
                          GalaxyArray *galaxies_this_snap, struct params *run_params);
static int process_halo_galaxies(const int halonr, const int fof_halonr, 
                                GalaxyArray* temp_fof_galaxies, int32_t *galaxycounter,
                                struct halo_data *halos, const GalaxyArray* galaxies_prev_snap, 
                                struct params *run_params, bool *processed_flags);


/**
 * @brief FORWARD-LOOKING ORPHAN DETECTION: Identifies galaxies from the previous 
 *        snapshot whose host halos were disrupted and reclassifies them as orphan galaxies.
 *
 * **Problem**: The standard SAGE processing looks "backward" from current snapshot halos
 * to find their progenitors. If a halo from the previous snapshot has NO descendant in 
 * the current snapshot (i.e., it was completely disrupted), its galaxies are never found
 * and are silently lost from the simulation. This violates mass conservation.
 *
 * **Solution**: This function performs a "forward-looking" scan of unprocessed galaxies
 * from the previous snapshot to identify those whose host halos disappeared. These lost
 * galaxies become "orphan" galaxies that continue to exist within the FOF group.
 *
 * **Orphan Host Assignment Logic**:
 * When a galaxy's host halo disappears, the galaxy doesn't just vanish - it becomes an
 * orphan that orbits within the larger FOF structure. To determine which FOF group should
 * host the orphan, we follow the merger tree: the orphan belongs to the FOF group that
 * contains the descendant of its original central galaxy's halo. This ensures orphans
 * end up in the correct gravitational environment.
 *
 * @param fof_halonr The root halo number of the current FOF group.
 * @param temp_fof_galaxies The galaxy array for the current FOF group, to which orphans will be appended.
 * @param galaxies_prev_snap The galaxy array from the previous snapshot.
 * @param processed_flags A boolean array tracking which galaxies from the previous snapshot have been processed.
 * @param halos The halo data for the entire forest.
 * @param run_params SAGE runtime parameters.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int identify_and_process_orphans(const int fof_halonr, GalaxyArray* temp_fof_galaxies,
                                 const GalaxyArray* galaxies_prev_snap, bool *processed_flags,
                                 struct halo_data *halos, struct params *run_params)
{
    // Check required parameters
    if (!temp_fof_galaxies) {
        LOG_ERROR("NULL temp_fof_galaxies passed to identify_and_process_orphans");
        return EXIT_FAILURE;
    }
    
    if (!galaxies_prev_snap || !processed_flags) {
        return EXIT_SUCCESS; // Nothing to do if there's no previous snapshot.
    }

    struct GALAXY *galaxies_prev_raw = galaxy_array_get_raw_data((GalaxyArray*)galaxies_prev_snap);
    int ngal_prev = galaxy_array_get_count(galaxies_prev_snap);

    for (int i = 0; i < ngal_prev; i++) {
        // If this galaxy wasn't processed, it's a potential orphan.
        if (!processed_flags[i]) {
            const struct GALAXY* source_gal = &galaxies_prev_raw[i];

            // An orphan's new host is the descendant of its old host's central.
            // We need to check if this FOF group is the correct new home.
            int old_central_halonr = GALAXY_PROP_CentralGal(source_gal);
            if (old_central_halonr < 0) continue; // Should not happen for valid galaxies.
            
            int descendant_of_old_central = halos[old_central_halonr].Descendant;
            if (descendant_of_old_central < 0) continue; // Old central has no descendant.
            
            // Check if the descendant of the old central belongs to the current FOF group.
            if (halos[descendant_of_old_central].FirstHaloInFOFgroup == fof_halonr) {
                
                struct GALAXY temp_orphan;
                memset(&temp_orphan, 0, sizeof(struct GALAXY));
                galaxy_extension_initialize(&temp_orphan);
                deep_copy_galaxy(&temp_orphan, source_gal, run_params);

                // Reclassify as an orphan.
                GALAXY_PROP_Type(&temp_orphan) = 2;
                GALAXY_PROP_merged(&temp_orphan) = 0; // CRITICAL: Keep it active for processing.
                GALAXY_PROP_Mvir(&temp_orphan) = 0.0; // Its host halo is gone.
                GALAXY_PROP_deltaMvir(&temp_orphan) = -1.0 * GALAXY_PROP_Mvir(source_gal);

                // Append to the current FOF group's galaxy list for evolution.
                if (galaxy_array_append(temp_fof_galaxies, &temp_orphan, run_params) < 0) {
                    LOG_ERROR("Failed to append orphan galaxy to FOF array.");
                    free_galaxy_properties(&temp_orphan);
                    return EXIT_FAILURE;
                }
                free_galaxy_properties(&temp_orphan);

                // Mark as processed to ensure it's only added once.
                processed_flags[i] = true;
            }
        }
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Complete FOF group processor with central assignment
 * @param fof_halonr FOF group root halo number
 * @param galaxies_prev_snap Previous snapshot galaxies
 * @param galaxies_this_snap Output current snapshot galaxies
 * @param halos Halo data array
 * @param haloaux Halo auxiliary data array
 * @param galaxycounter Global galaxy counter
 * @param run_params SAGE parameters structure
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 * 
 * Called by: sage_per_forest() (via direct call)
 * Calls: process_halo_galaxies() - handle individual halos
 *        evolve_galaxies() - execute physics pipeline
 *        galaxy_array_new() - create galaxy container
 *        galaxy_array_free() - release galaxy memory
 */
int process_fof_group(int fof_halonr, GalaxyArray* galaxies_prev_snap, 
                            GalaxyArray* galaxies_this_snap, struct halo_data *halos,
                            struct halo_aux_data *haloaux, int32_t *galaxycounter,
                            struct params *run_params, bool *processed_flags)
{
    GalaxyArray *temp_fof_galaxies = galaxy_array_new();
    if (!temp_fof_galaxies) {
        LOG_ERROR("Failed to create temporary galaxy array for FOF group %d", fof_halonr);
        return EXIT_FAILURE;
    }

    // Direct galaxy scanning approach - no auxiliary data reconstruction needed
    
    // Process all halos in FOF group
    int current_fof_halo = fof_halonr;
    while(current_fof_halo >= 0) {
        int status = process_halo_galaxies(current_fof_halo, fof_halonr, temp_fof_galaxies, 
                                          galaxycounter, halos, galaxies_prev_snap, run_params, processed_flags);
        if (status != EXIT_SUCCESS) {
            galaxy_array_free(&temp_fof_galaxies);
            return EXIT_FAILURE;
        }
        current_fof_halo = halos[current_fof_halo].NextHaloInFOFgroup;
    }

    // NEW STEP: Identify and add true orphans to this FOF group.
    int orphan_status = identify_and_process_orphans(fof_halonr, temp_fof_galaxies, galaxies_prev_snap, 
                                                     processed_flags, halos, run_params);
    if (orphan_status != EXIT_SUCCESS) {
        galaxy_array_free(&temp_fof_galaxies);
        return EXIT_FAILURE;
    }

    // Set central galaxy references for FOF group with enhanced error handling
    int ngal_fof = galaxy_array_get_count(temp_fof_galaxies);
    if (ngal_fof > 0) {
        struct GALAXY* galaxies_raw = galaxy_array_get_raw_data(temp_fof_galaxies);
        int central_for_fof = -1;
        int type0_count = 0;
        
        // Find Type 0 galaxy with comprehensive validation
        for(int i = 0; i < ngal_fof; ++i) {
            if(GALAXY_PROP_Type(&galaxies_raw[i]) == 0) {
                type0_count++;
                if(central_for_fof == -1) {
                    central_for_fof = i;
                }
            }
        }
        
        // Handle central galaxy assignment and mergers
        if (type0_count == 0) {
            LOG_ERROR("No Type 0 central galaxy found in FOF group %d with %d galaxies", fof_halonr, ngal_fof);
            galaxy_array_free(&temp_fof_galaxies);
            return EXIT_FAILURE;
        } else if (type0_count > 1) {
            // Handle merger scenario: multiple central galaxies from different progenitors
            LOG_DEBUG("Found %d Type 0 galaxies in FOF group %d - handling central galaxy merger", type0_count, fof_halonr);
            
            // Find the most massive galaxy to remain central
            int most_massive_idx = -1;
            float max_stellar_mass = -1.0f;
            
            for(int i = 0; i < ngal_fof; ++i) {
                if(GALAXY_PROP_Type(&galaxies_raw[i]) == 0) {
                    // Use Mvir as a proxy for galaxy mass in physics-free mode
                    // In full physics mode, this would be StellarMass
                    float galaxy_mass = GALAXY_PROP_Mvir(&galaxies_raw[i]);
                    if (galaxy_mass > max_stellar_mass) {
                        max_stellar_mass = galaxy_mass;
                        most_massive_idx = i;
                    }
                }
            }
            
            if (most_massive_idx == -1) {
                LOG_ERROR("Failed to find most massive galaxy in merger scenario for FOF group %d", fof_halonr);
                galaxy_array_free(&temp_fof_galaxies);
                return EXIT_FAILURE;
            }
            
            // Demote all other Type 0 galaxies to satellites (Type 1)
            int demoted_count = 0;
            for(int i = 0; i < ngal_fof; ++i) {
                if(GALAXY_PROP_Type(&galaxies_raw[i]) == 0 && i != most_massive_idx) {
                    // Record infall properties for central→satellite demotion
                    GALAXY_PROP_infallMvir(&galaxies_raw[i]) = GALAXY_PROP_Mvir(&galaxies_raw[i]);
                    GALAXY_PROP_infallVvir(&galaxies_raw[i]) = GALAXY_PROP_Vvir(&galaxies_raw[i]);
                    GALAXY_PROP_infallVmax(&galaxies_raw[i]) = GALAXY_PROP_Vmax(&galaxies_raw[i]);
                    
                    // Demote to satellite
                    GALAXY_PROP_Type(&galaxies_raw[i]) = 1;
                    demoted_count++;
                }
            }
            
            central_for_fof = most_massive_idx;
            LOG_DEBUG("Central galaxy merger resolved: kept galaxy %d as central, demoted %d galaxies to satellites", 
                     most_massive_idx, demoted_count);
        }
        
        // Set all galaxies to point to the single central galaxy
        for(int i = 0; i < ngal_fof; ++i) {
            GALAXY_PROP_CentralGal(&galaxies_raw[i]) = central_for_fof;
        }
        
        LOG_DEBUG("Set central galaxy index %d for FOF group %d with %d galaxies", 
                  central_for_fof, fof_halonr, ngal_fof);
    }

    // Evolve galaxies
    int current_total_galaxies = galaxy_array_get_count(galaxies_this_snap);
    int status = evolve_galaxies(fof_halonr, temp_fof_galaxies, &current_total_galaxies, 
                                halos, haloaux, galaxies_this_snap, run_params);
    
    galaxy_array_free(&temp_fof_galaxies);
    return status;
}

/**
 * @brief Individual halo galaxy processor within FOF group - optimized for direct FOF append
 * @param halonr Current halo number
 * @param fof_halonr FOF group root halo number
 * @param temp_fof_galaxies FOF group galaxy accumulator (direct append)
 * @param galaxycounter Global galaxy counter
 * @param halos Halo data array
 * @param prev_haloaux Previous snapshot auxiliary data
 * @param galaxies_prev_snap Previous snapshot galaxies
 * @param run_params SAGE parameters structure
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 * 
 * Called by: process_fof_group()
 * Calls: copy_galaxies_from_progenitors() - inherit from progenitors directly to FOF array
 */
static int process_halo_galaxies(const int halonr, const int fof_halonr, 
                                GalaxyArray* temp_fof_galaxies, int32_t *galaxycounter,
                                struct halo_data *halos, const GalaxyArray* galaxies_prev_snap, 
                                struct params *run_params, bool *processed_flags)
{
    // Direct append to FOF array - no intermediate allocation
    int status = copy_galaxies_from_progenitors(halonr, fof_halonr, temp_fof_galaxies, 
                                               galaxycounter, halos, galaxies_prev_snap, run_params, processed_flags);
    return status;
}

/**
 * @brief Four-phase physics pipeline executor for FOF group
 * @param fof_root_halonr FOF group root halo number
 * @param temp_fof_galaxies FOF group galaxies to evolve
 * @param numgals Output number of galaxies processed
 * @param halos Halo data array
 * @param haloaux Halo auxiliary data array
 * @param galaxies_this_snap Output snapshot galaxy array
 * @param run_params SAGE parameters structure
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 * 
 * Called by: process_fof_group()
 * Calls: initialize_evolution_context() - setup evolution state
 *        pipeline_execute_phase() - run physics modules
 *        core_process_merger_queue_agnostically() - handle mergers
 *        deep_copy_galaxy() - create output copies
 *        galaxy_array_append() - add to snapshot
 */
static int evolve_galaxies(const int fof_root_halonr, GalaxyArray* temp_fof_galaxies, int *numgals, 
                          struct halo_data *halos, struct halo_aux_data *haloaux, 
                          GalaxyArray *galaxies_this_snap, struct params *run_params)
{
    int ngal = galaxy_array_get_count(temp_fof_galaxies);
    struct GALAXY *galaxies = galaxy_array_get_raw_data(temp_fof_galaxies);

    // Initialize evolution context and diagnostics
    struct evolution_context ctx;
    initialize_evolution_context(&ctx, fof_root_halonr, galaxies, ngal, halos, run_params);
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, fof_root_halonr, ngal);
    ctx.diagnostics = &diag;

    if (!validate_evolution_context(&ctx)) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Evolution context validation failed for FOF group %d", fof_root_halonr);
        return EXIT_FAILURE;
    }

    // Validate central galaxy for FOF group
    if(GALAXY_PROP_Type(&galaxies[ctx.centralgal]) != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Invalid central galaxy: expected type=0 but found type=%d",
                    GALAXY_PROP_Type(&galaxies[ctx.centralgal]));
        return EXIT_FAILURE;
    }

    // Setup pipeline context
    struct pipeline_context pipeline_ctx;
    pipeline_context_init(&pipeline_ctx, run_params, ctx.galaxies, ctx.ngal, ctx.centralgal, 
                          0.0, 0.0, fof_root_halonr, 0, &ctx);
    pipeline_ctx.redshift = ctx.redshift;

    // Initialize property serialization for extensions
    if (global_extension_registry != NULL && global_extension_registry->num_extensions > 0) {
        if (pipeline_init_property_serialization(&pipeline_ctx, PROPERTY_FLAG_SERIALIZE) != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to initialize property serialization");
            return EXIT_FAILURE;
        }
    }

    // Get physics pipeline
    struct module_pipeline *physics_pipeline = pipeline_get_global();
    if (physics_pipeline == NULL) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "No physics pipeline available");
        core_evolution_diagnostics_finalize(&diag);
        core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }

    // Create merger event queue
    struct merger_event_queue merger_queue;
    init_merger_queue(&merger_queue);
    ctx.merger_queue = &merger_queue;
    pipeline_ctx.merger_queue = &merger_queue;

    // Phase 1: HALO phase
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
    pipeline_ctx.execution_phase = PIPELINE_PHASE_HALO;
    
    ensure_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params);
    
    int status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_HALO);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    
    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute HALO phase for FOF group %d", fof_root_halonr);
        core_evolution_diagnostics_finalize(&diag);
        core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }

    // Main integration loop
    for (int step = 0; step < STEPS; step++) {
        pipeline_ctx.step = step;
        init_merger_queue(&merger_queue);

        // Phase 2: GALAXY phase
        core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
        pipeline_ctx.execution_phase = PIPELINE_PHASE_GALAXY;

        for (int p = 0; p < ctx.ngal; p++) {
            ctx.deltaT = run_params->simulation.Age[GALAXY_PROP_SnapNum(&galaxies[p])] - 
                        run_params->simulation.Age[halos[fof_root_halonr].SnapNum];
            ctx.time = run_params->simulation.Age[GALAXY_PROP_SnapNum(&galaxies[p])] - 
                      (step + 0.5) * (ctx.deltaT / STEPS);
            
            // Set dT property if not already set (matching legacy behavior)
            if (GALAXY_PROP_dT(&galaxies[p]) < 0.0) {
                GALAXY_PROP_dT(&galaxies[p]) = ctx.deltaT;
            }
            
            pipeline_ctx.dt = ctx.deltaT / STEPS;
            pipeline_ctx.time = ctx.time;
            pipeline_ctx.current_galaxy = p;
            diag.phases[1].galaxy_count++;

            ensure_galaxy_properties(&ctx.galaxies[p], run_params);

            status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_GALAXY);
            if (status != 0) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute GALAXY phase for galaxy %d", p);
                core_evolution_diagnostics_finalize(&diag);
                core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
                return EXIT_FAILURE;
            }
        }
        core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);

        // Phase 3: POST phase
        core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_POST);
        pipeline_ctx.execution_phase = PIPELINE_PHASE_POST;
        
        ensure_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params);

        status = core_process_merger_queue_agnostically(&pipeline_ctx);
        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to process merger events for step %d", step);
            core_evolution_diagnostics_finalize(&diag);
            core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
            return EXIT_FAILURE;
        }

        status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_POST);
        core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_POST);

        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute POST phase for step %d", step);
            core_evolution_diagnostics_finalize(&diag);
            core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
            return EXIT_FAILURE;
        }

        // Track merger events for diagnostics
        for (int i = 0; i < merger_queue.num_events; i++) {
            core_evolution_diagnostics_add_merger_processed(&diag, merger_queue.events[i].merger_type);
        }
    }

    // Phase 4: FINAL phase
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_FINAL);
    pipeline_ctx.execution_phase = PIPELINE_PHASE_FINAL;
    
    ensure_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params);

    status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_FINAL);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_FINAL);

    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute FINAL phase for FOF group %d", fof_root_halonr);
        core_evolution_diagnostics_finalize(&diag);
        core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }

    // Cleanup
    if (pipeline_ctx.prop_ctx != NULL) {
        pipeline_cleanup_property_serialization(&pipeline_ctx);
    }
    core_evolution_diagnostics_finalize(&diag);
    core_evolution_diagnostics_report(&diag, LOG_LEVEL_INFO);

    // =================================================================
    // NEW, SAFE LOGIC STARTS HERE
    // =================================================================

    // Attach surviving galaxies to the final snapshot array.
    // We iterate through the evolved FOF group and deep-copy only the
    // galaxies that have not been merged.
    for(int p = 0; p < ctx.ngal; p++) {
        // Skip any galaxy that was marked as merged during the evolution step.
        if (GALAXY_PROP_merged(&ctx.galaxies[p]) == 0) {

            // Find the current halo for this galaxy to update auxiliary data.
            int currenthalo = GALAXY_PROP_HaloNr(&ctx.galaxies[p]);
            if (haloaux[currenthalo].NGalaxies == 0) {
                // This is the first galaxy for this halo in the output list.
                haloaux[currenthalo].FirstGalaxy = *numgals;
            }

            // Update galaxy SnapNum to final snapshot after evolution
            GALAXY_PROP_SnapNum(&ctx.galaxies[p]) = halos[currenthalo].SnapNum;

            // Perform a deep copy to the final output array for this snapshot.
            // This ensures a clean, separate copy with its own allocated properties.
            if (galaxy_array_append(galaxies_this_snap, &ctx.galaxies[p], run_params) < 0) {
                LOG_ERROR("Failed to append surviving galaxy to snapshot array.");
                return EXIT_FAILURE;
            }

            // Increment the total number of galaxies for this snapshot and for this halo.
            (*numgals)++;
            haloaux[currenthalo].NGalaxies++;
        }
    }

    // The temp_fof_galaxies array (which contains both survivors and merged galaxies)
    // will be freed by the calling function, `process_fof_group`. This is the correct
    // place to free it, as it ensures all properties (even for merged galaxies)
    // are freed exactly once.

    return EXIT_SUCCESS;
}

/**
 * @brief Wrapper function for evolve_galaxies to enable tree-based processing
 * 
 * This function provides access to the static evolve_galaxies function for use
 * in tree-based processing where physics needs to be applied to collected FOF galaxies.
 * 
 * @param fof_root_halonr FOF root halo number
 * @param temp_fof_galaxies Galaxy array containing FOF galaxies to evolve
 * @param numgals Pointer to galaxy count (updated)
 * @param halos Halo data array
 * @param haloaux Auxiliary halo data array
 * @param galaxies_this_snap Output galaxy array for this snapshot
 * @param run_params SAGE parameters
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int evolve_galaxies_wrapper(const int fof_root_halonr, 
                           GalaxyArray* temp_fof_galaxies, 
                           int *numgals, 
                           struct halo_data *halos, 
                           struct halo_aux_data *haloaux,
                           GalaxyArray *galaxies_this_snap, 
                           struct params *run_params) {
    return evolve_galaxies(fof_root_halonr, temp_fof_galaxies, numgals, 
                          halos, haloaux, galaxies_this_snap, run_params);
}
