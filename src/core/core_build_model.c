/**
 * @file    core_build_model.c
 * @brief   Core functions for building and evolving the galaxy formation model
 *
 * This file contains the core algorithms for constructing galaxies from merger
 * trees and evolving them through time. It implements the main semi-analytic
 * modeling framework including the construction of galaxies from their
 * progenitors, the application of physical processes (cooling, star formation,
 * feedback), handling of mergers, and the time integration scheme.
 *
 * Key functions:
 * - construct_galaxies(): Recursive function to build galaxies through merger trees
 * - join_galaxies_of_progenitors(): Integrates galaxies from progenitor halos
 * - evolve_galaxies(): Main time integration function for galaxy evolution
 * - find_most_massive_progenitor(): Identifies main progenitor branch
 * - copy_galaxies_from_progenitors(): Transfers galaxy properties through time
 * - set_galaxy_centrals(): Establishes central-satellite relationships
 *
 * Architecture:
 * This implementation maintains core-physics separation, where the core infrastructure
 * handles galaxy construction, memory management, and time integration, while physics
 * modules are applied through a configurable pipeline system. This allows the core
 * to operate independently of specific physics implementations.
 *
 * References:
 * - Croton et al. (2006) - Main semi-analytic model framework
 * - White & Frenk (1991) - Cooling model foundation
 * - Kauffmann et al. (1999) - Star formation implementation
 * - Somerville et al. (2001) - Merger model foundation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include "core_allvars.h"
#include "core_build_model.h"
#include "core_init.h"
#include "core_mymalloc.h"
#include "core_save.h"
#include "core_utils.h"
#include "core_logging.h"
#include "core_galaxy_extensions.h"
#include "core_pipeline_system.h"
#include "core_module_system.h"
#include "core_array_utils.h"
#include "core_merger_queue.h"
#include "core_merger_processor.h"
#include "core_event_system.h"
#include "core_evolution_diagnostics.h"
#include "core_galaxy_accessors.h"  // For galaxy_get_* functions
#include "core_property_utils.h"    // For get_cached_property_id, set_float_property
#include "galaxy_array.h"           // For GalaxyArray API

/* Core function forward declarations (halo/tree properties and essential functions) */
double get_virial_mass(const int halonr, struct halo_data *halos, struct params *run_params);
double get_virial_radius(const int halonr, struct halo_data *halos, struct params *run_params);
double get_virial_velocity(const int halonr, struct halo_data *halos, struct params *run_params);

/* Forward declaration for init_galaxy function */
void init_galaxy(const int p, const int halonr, int *galaxycounter, struct halo_data *halos,
                struct GALAXY *galaxies, struct params *run_params);

/**
 * @brief   Performs a safe deep copy of a GALAXY structure
 *
 * @param   dest        Destination galaxy structure (must be initialized)
 * @param   src         Source galaxy structure to copy from
 * @param   run_params  Runtime parameters needed for property copying
 *
 * This function performs a proper deep copy of a GALAXY structure, correctly
 * handling the properties pointer to avoid memory corruption. It replaces
 * direct structure assignment (=) which only performs shallow copying and
 * creates dangerous pointer aliasing.
 */
void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params)
{
    // SINGLE SOURCE OF TRUTH: Use property system for all galaxy data copying
    
    // Copy extension mechanism
    dest->num_extensions = src->num_extensions;
    dest->extension_flags = src->extension_flags;
    
    // Initialize extension data pointer
    dest->extension_data = NULL;
    
    // Deep copy extension data if present
    if (src->extension_data != NULL && src->num_extensions > 0) {
        // Use the galaxy extension copy mechanism instead of manual copying
        int status = galaxy_extension_copy(dest, src);
        if (status != 0) {
            LOG_ERROR("Failed to copy galaxy extension data. Source GalaxyIndex: %llu", GALAXY_PROP_GalaxyIndex(src));
        }
    }
    
    // Initialize properties pointer to NULL before allocating
    dest->properties = NULL;
    
    // Let the auto-generated property system handle ALL property copying
    if (copy_galaxy_properties(dest, src, run_params) != 0) {
        LOG_ERROR("Failed to copy galaxy properties. Source GalaxyIndex: %llu\n", GALAXY_PROP_GalaxyIndex(src));
    }
}

/**
 * @brief   Finds the most massive progenitor halo that contains a galaxy
 *
 * @param   halonr    Index of the current halo in the halo array
 * @param   halos     Array of halo data structures
 * @param   haloaux   Array of halo auxiliary data structures
 * @return  Index of the most massive progenitor with a galaxy
 *
 * This function scans all progenitors of a halo to find the most massive one
 * that actually contains a galaxy. This is important because not all dark
 * matter halos necessarily host galaxies, and we need to identify the main
 * branch for inheriting galaxy properties.
 *
 * Two criteria are tracked:
 * 1. The most massive progenitor overall (by particle count)
 * 2. The most massive progenitor that contains a galaxy
 *
 * The function returns the index of the most massive progenitor containing a
 * galaxy, which is used to determine which galaxy should become the central
 * galaxy of the descendant halo.
 */
static int find_most_massive_progenitor(const int halonr, struct halo_data *halos, 
                                       struct halo_aux_data *haloaux)
{
    int prog, first_occupied, lenmax, lenoccmax;

    lenmax = 0;
    lenoccmax = 0;
    first_occupied = halos[halonr].FirstProgenitor;
    prog = halos[halonr].FirstProgenitor;

    if (prog >= 0) {
        if (haloaux[prog].NGalaxies > 0) {
            lenoccmax = -1;
        }
    }

    /* Find most massive progenitor that contains an actual galaxy
     * Maybe FirstProgenitor never was FirstHaloInFOFGroup and thus has no galaxy */
    while (prog >= 0) {
        if (halos[prog].Len > lenmax) {
            lenmax = halos[prog].Len;
            /* mother_halo = prog; */
        }
        if (lenoccmax != -1 && halos[prog].Len > lenoccmax && haloaux[prog].NGalaxies > 0) {
            lenoccmax = halos[prog].Len;
            first_occupied = prog;
        }
        prog = halos[prog].NextProgenitor;
    }

    return first_occupied;
}

/**
 * @brief   Copies and updates galaxies from progenitor halos to the current snapshot
 *
 * @param   halonr          Index of the current halo in the halo array
 * @param   ngalstart       Starting index for galaxies in the galaxy array
 * @param   first_occupied  Index of the most massive progenitor with galaxies
 * @param   galaxycounter   Galaxy counter for ID assignment
 * @param   halos          Array of halo data structures
 * @param   haloaux        Array of halo auxiliary data structures
 * @param   galaxies_arr   Safe dynamic array for temporary galaxies
 * @param   halogal_arr    Safe dynamic array for permanent galaxies
 * @param   run_params     Simulation parameters
 * @return  Updated number of galaxies after copying
 *
 * This function transfers galaxies from progenitor halos to the current
 * snapshot, updating their properties based on the new halo structure. It
 * handles:
 *
 * 1. Copying galaxies from all progenitors to the temporary galaxy array
 * 2. Updating galaxy properties based on their new host halo
 * 3. Handling type transitions (central → satellite → orphan)
 * 4. Setting appropriate merger times for satellites
 * 5. Creating new galaxies when a halo has no progenitor galaxies
 *
 * The function maintains the continuity of galaxy evolution by preserving
 * their properties while updating their status based on the evolving
 * dark matter structures.
 */
static int copy_galaxies_from_progenitors(const int halonr, const int fof_halonr, GalaxyArray *galaxies_for_halo,
                                         int32_t *galaxycounter, struct halo_data *halos,
                                         struct halo_aux_data *haloaux, const GalaxyArray *galaxies_prev_snap,
                                         struct params *run_params)
{
    struct GALAXY *galaxies_prev_raw = galaxy_array_get_raw_data((GalaxyArray*)galaxies_prev_snap);
    int first_occupied = find_most_massive_progenitor(halonr, halos, haloaux);

    int prog = halos[halonr].FirstProgenitor;
    while (prog >= 0) {
        for (int i = 0; i < haloaux[prog].NGalaxies; i++) {
            const struct GALAXY* source_gal = &galaxies_prev_raw[haloaux[prog].FirstGalaxy + i];

            // Create a temporary, deep copy of the progenitor galaxy to work with.
            struct GALAXY temp_galaxy;
            memset(&temp_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_galaxy);
            deep_copy_galaxy(&temp_galaxy, source_gal, run_params);

            GALAXY_PROP_HaloNr(&temp_galaxy) = halonr;
            GALAXY_PROP_dT(&temp_galaxy) = -1.0;
            
            // Ensure properties are allocated before syncing
            if (temp_galaxy.properties == NULL) {
                if (allocate_galaxy_properties(&temp_galaxy, run_params) != 0) {
                    LOG_WARNING("Failed to allocate properties for temp galaxy");
                }
            }
            
            // This is a shallow copy of extension flags/pointers, which is fine as they are managed on demand.
            galaxy_extension_copy(&temp_galaxy, source_gal);

            if (GALAXY_PROP_Type(&temp_galaxy) == 0 || GALAXY_PROP_Type(&temp_galaxy) == 1) {

                const float previousMvir = GALAXY_PROP_Mvir(&temp_galaxy);
                const float previousVvir = GALAXY_PROP_Vvir(&temp_galaxy);
                const float previousVmax = GALAXY_PROP_Vmax(&temp_galaxy);

                if (prog == first_occupied) {
                    // This galaxy is from the most massive progenitor.

                    // Update its properties to match the new host halo.
                    GALAXY_PROP_MostBoundID(&temp_galaxy) = halos[halonr].MostBoundID;

                    for(int j = 0; j < 3; j++) {
                        GALAXY_PROP_Pos(&temp_galaxy)[j] = halos[halonr].Pos[j];
                        GALAXY_PROP_Vel(&temp_galaxy)[j] = halos[halonr].Vel[j];
                    }

                    GALAXY_PROP_Len(&temp_galaxy) = halos[halonr].Len;
                    GALAXY_PROP_Vmax(&temp_galaxy) = halos[halonr].Vmax;

                    float new_mvir = get_virial_mass(halonr, halos, run_params);
                    GALAXY_PROP_deltaMvir(&temp_galaxy) = new_mvir - GALAXY_PROP_Mvir(&temp_galaxy);
                    
                    GALAXY_PROP_Mvir(&temp_galaxy) = new_mvir;
                    GALAXY_PROP_Rvir(&temp_galaxy) = get_virial_radius(halonr, halos, run_params);
                    GALAXY_PROP_Vvir(&temp_galaxy) = get_virial_velocity(halonr, halos, run_params);
                    
                    // Ensure properties are allocated before syncing
                    if (temp_galaxy.properties == NULL) {
                        if (allocate_galaxy_properties(&temp_galaxy, run_params) != 0) {
                            LOG_WARNING("Failed to allocate properties for temp galaxy");
                        }
                    }

                    if (halonr == fof_halonr) {

                        // It remains a central galaxy (Type 0) of the main FOF.
                        GALAXY_PROP_Type(&temp_galaxy) = 0;
                        
                        // Ensure properties are allocated before syncing
                        if (temp_galaxy.properties == NULL) {
                            if (allocate_galaxy_properties(&temp_galaxy, run_params) != 0) {
                                LOG_WARNING("Failed to allocate properties for temp galaxy");
                            }
                        }

                    } else {

                        if (GALAXY_PROP_Type(&temp_galaxy) == 0) {
                            // It was a central but is now a satellite (Type 1). Record its properties at the point of infall.
                            GALAXY_PROP_infallMvir(&temp_galaxy) = previousMvir;
                            GALAXY_PROP_infallVvir(&temp_galaxy) = previousVvir;
                            GALAXY_PROP_infallVmax(&temp_galaxy) = previousVmax;
                        }

                        GALAXY_PROP_Type(&temp_galaxy) = 1;

                        // Ensure properties are allocated before syncing
                        if (temp_galaxy.properties == NULL) {
                            if (allocate_galaxy_properties(&temp_galaxy, run_params) != 0) {
                                LOG_WARNING("Failed to allocate properties for temp galaxy");
                            }
                        }
                    }

                } else {

                    GALAXY_PROP_deltaMvir(&temp_galaxy) = -1.0 * GALAXY_PROP_Mvir(&temp_galaxy);
                    GALAXY_PROP_Mvir(&temp_galaxy) = 0.0;
    
                    if (GALAXY_PROP_Type(&temp_galaxy) == 0) {
                        // If it was a central, we must record its properties at infall.
                        GALAXY_PROP_infallMvir(&temp_galaxy) = previousMvir;
                        GALAXY_PROP_infallVvir(&temp_galaxy) = previousVvir;
                        GALAXY_PROP_infallVmax(&temp_galaxy) = previousVmax;
                    }
    
                    // This was a satellite of the main progenitor. It becomes an orphan.
                    GALAXY_PROP_Type(&temp_galaxy) = 2;
                    GALAXY_PROP_merged(&temp_galaxy) = 1;

                    // Ensure properties are allocated before syncing
                    if (temp_galaxy.properties == NULL) {
                        if (allocate_galaxy_properties(&temp_galaxy, run_params) != 0) {
                            LOG_WARNING("Failed to allocate properties for temp galaxy");
                        }
                    }
                }
            }
            
            // Append the processed galaxy to the array for this halo.
            if (galaxy_array_append(galaxies_for_halo, &temp_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append galaxy.");
                free_galaxy_properties(&temp_galaxy);
                return EXIT_FAILURE;
            }

            // Free the temporary working copy.
            free_galaxy_properties(&temp_galaxy);
        }
        prog = halos[prog].NextProgenitor;
    }

    if (galaxy_array_get_count(galaxies_for_halo) == 0) {
        // No progenitors with galaxies. Create a new galaxy if this is a main FOF halo.
        if (halonr == fof_halonr) {
            struct GALAXY temp_new_galaxy;
            memset(&temp_new_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_new_galaxy);
            init_galaxy(0, halonr, galaxycounter, halos, &temp_new_galaxy, run_params);

            if (galaxy_array_append(galaxies_for_halo, &temp_new_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append new galaxy.");
                free_galaxy_properties(&temp_new_galaxy);
                return EXIT_FAILURE;
            }
            free_galaxy_properties(&temp_new_galaxy);
        }
    }

    return EXIT_SUCCESS;
}

/**
 * @brief   Sets the central galaxy reference for all galaxies in a halo
 *
 * @param   ngalstart    Starting index of galaxies for this halo
 * @param   ngal         Ending index (exclusive) of galaxies for this halo
 * @param   galaxies     Array of galaxy structures
 *
 * This function identifies the central galaxy (Type 0 or 1) for a halo
 * and sets all galaxies in the halo to reference this central galaxy.
 * Each halo can have only one Type 0 or 1 galaxy, with all others
 * being Type 2 (orphan) galaxies.
 */
static int set_galaxy_centrals(const int ngalstart, const int ngal, struct GALAXY *galaxies)
{
    int i, centralgal;

    /* Per Halo there can be only one Type 0 or 1 galaxy, all others are Type 2 (orphan)
     * Find the central galaxy for this halo */
    for (i = ngalstart, centralgal = -1; i < ngal; i++) {
        if (GALAXY_PROP_Type(&galaxies[i]) == 0 || GALAXY_PROP_Type(&galaxies[i]) == 1) {
            if (centralgal != -1) {
                LOG_ERROR("Multiple central galaxies found in halo");
                return -1;
            } else {
                centralgal = i;
            }
        }
    }

    /* Set all galaxies to point to the central galaxy */
    for (i = ngalstart; i < ngal; i++) {
        GALAXY_PROP_CentralGal(&galaxies[i]) = centralgal;
    }
    
    return 0;
}


static int evolve_galaxies(const int halonr, GalaxyArray* temp_fof_galaxies, int *numgals, struct halo_data *halos,
                           struct halo_aux_data *haloaux, GalaxyArray *galaxies_this_snap, struct params *run_params);
static int join_galaxies_of_progenitors(const int halonr, const int fof_halonr, GalaxyArray* temp_fof_galaxies, int32_t *galaxycounter, 
                                        struct halo_data *halos, struct halo_aux_data *haloaux,
                                        const GalaxyArray* galaxies_prev_snap, struct params *run_params);

// New specialized functions for Phase 3.1 refactoring
static int join_progenitors_from_prev_snapshot(int halonr, int fof_halonr, 
                                               GalaxyArray* temp_fof_galaxies, int32_t *galaxycounter,
                                               const GalaxyArray* galaxies_prev_snap, 
                                               struct halo_data *halos,
                                               struct halo_aux_data *haloaux,
                                               struct params *run_params);

static int process_fof_group_at_snapshot(int fof_halonr, int snapshot,
                                        GalaxyArray* galaxies_prev_snap, 
                                        GalaxyArray* galaxies_this_snap,
                                        struct halo_data *halos,
                                        struct halo_aux_data *haloaux,
                                        int32_t *galaxycounter,
                                        struct params *run_params);

/**
 * @brief   Main galaxy construction function - simplified with clean responsibilities
 *
 * @param   halonr    Index of the current halo in the halo array
 * @param   numgals   Pointer to total number of galaxies created
 * @param   galaxycounter Galaxy counter for unique ID assignment
 * @param   halos     Array of halo data structures
 * @param   haloaux   Array of halo auxiliary data structures
 * @param   galaxies_this_snap Safe dynamic array for galaxies of the current snapshot
 * @param   galaxies_prev_snap Safe dynamic array for galaxies from the previous snapshot
 * @param   run_params Simulation parameters and configuration
 * @return  EXIT_SUCCESS on success, EXIT_FAILURE on error
 *
 * Simplified main function that delegates to specialized functions with single responsibilities.
 * In the snapshot-based architecture, this function primarily coordinates FOF group processing.
 * The complex recursive logic has been eliminated in favor of clean functional separation.
 */
int construct_galaxies(const int halonr, int *numgals, int32_t *galaxycounter, struct halo_data *halos,
                       struct halo_aux_data *haloaux, GalaxyArray *galaxies_this_snap,
                       GalaxyArray *galaxies_prev_snap, struct params *run_params)
{
    // CLEAN ARCHITECTURE: Delegate to specialized function with single responsibility
    // In snapshot-based processing, we process entire FOF groups together
    int fofhalo = halos[halonr].FirstHaloInFOFgroup;
    
    // Use the specialized FOF group processing function
    // The snapshot parameter is not directly available here, but the function
    // can work without it as progenitors are handled by the snapshot loop
    int snapshot = halos[halonr].SnapNum; // Get snapshot from halo data
    
    return process_fof_group_at_snapshot(fofhalo, snapshot, galaxies_prev_snap, galaxies_this_snap,
                                        halos, haloaux, galaxycounter, run_params);
}
/* end of construct_galaxies*/

/**
 * @brief   Copy and classify progenitor galaxies from previous snapshot
 *
 * Single responsibility: copy progenitor galaxies from previous snapshot and 
 * classify them based on their new halo environment.
 */
static int join_progenitors_from_prev_snapshot(int halonr, int fof_halonr, 
                                               GalaxyArray* temp_fof_galaxies, int32_t *galaxycounter,
                                               const GalaxyArray* galaxies_prev_snap, 
                                               struct halo_data *halos,
                                               struct halo_aux_data *haloaux,
                                               struct params *run_params)
{
    // This function delegates to the existing join_galaxies_of_progenitors
    // but with a clear single responsibility: handling progenitor galaxy copying
    return join_galaxies_of_progenitors(halonr, fof_halonr, temp_fof_galaxies, galaxycounter, 
                                       halos, haloaux, galaxies_prev_snap, run_params);
}

/**
 * @brief   Process complete FOF group at specific snapshot  
 *
 * Single responsibility: process complete FOF group at specific snapshot,
 * handling galaxy evolution for the entire group.
 */
static int process_fof_group_at_snapshot(int fof_halonr, int snapshot,
                                        GalaxyArray* galaxies_prev_snap, 
                                        GalaxyArray* galaxies_this_snap,
                                        struct halo_data *halos,
                                        struct halo_aux_data *haloaux,
                                        int32_t *galaxycounter,
                                        struct params *run_params)
{
    // Create temporary FOF group galaxy array
    GalaxyArray *temp_fof_galaxies = galaxy_array_new();
    if (!temp_fof_galaxies) {
        LOG_ERROR("Failed to create temporary galaxy array for FOF group %d", fof_halonr);
        return EXIT_FAILURE;
    }

    // AGGREGATE STEP: Loop through all halos in the FOF group and join their
    // progenitor galaxies into our single temporary array.
    int current_fof_halo = fof_halonr;
    while(current_fof_halo >= 0) {
        int status = join_progenitors_from_prev_snapshot(current_fof_halo, fof_halonr, 
                                                        temp_fof_galaxies, galaxycounter,
                                                        galaxies_prev_snap, halos, haloaux, run_params);
        if (status != EXIT_SUCCESS) {
            galaxy_array_free(&temp_fof_galaxies);
            return EXIT_FAILURE;
        }
        current_fof_halo = halos[current_fof_halo].NextHaloInFOFgroup;
    }

    // FINAL FOF-LEVEL CENTRAL ASSIGNMENT: Find the single Type 0 galaxy and set all CentralGal pointers.
    int ngal_fof = galaxy_array_get_count(temp_fof_galaxies);
    if (ngal_fof > 0) {
        struct GALAXY* galaxies_raw = galaxy_array_get_raw_data(temp_fof_galaxies);
        int central_for_fof = -1;
        for(int i = 0; i < ngal_fof; ++i) {
            if(GALAXY_PROP_Type(&galaxies_raw[i]) == 0) {
                if(central_for_fof != -1) {
                    LOG_ERROR("Found multiple Type 0 galaxies in a single FOF group. This should not happen.");
                    galaxy_array_free(&temp_fof_galaxies);
                    return EXIT_FAILURE;
                }
                central_for_fof = i;
            }
        }
        // Set all galaxies to point to the central galaxy
        if(central_for_fof != -1) {
            for(int i = 0; i < ngal_fof; ++i) {
                GALAXY_PROP_CentralGal(&galaxies_raw[i]) = central_for_fof;
            }
        } else if (ngal_fof > 0) {
             LOG_WARNING("No central (Type 0) galaxy found for FOF group %d with %d galaxies.", fof_halonr, ngal_fof);
        }
    }

    // Evolve galaxies for this FOF group
    int numgals_dummy = 0; // This parameter is not used in the current implementation
    int status = evolve_galaxies(fof_halonr, temp_fof_galaxies, &numgals_dummy, halos, haloaux, galaxies_this_snap, run_params);
    
    // Clean up the temporary array for this FOF group.
    galaxy_array_free(&temp_fof_galaxies);
    
    return status;
}

/**
 * @brief   Main function to join galaxies from progenitor halos
 *
 * This function is now a wrapper that handles finding the most massive progenitor
 * and then calling the function to copy galaxies. It operates on the FOF-group-level
 * temporary galaxy array.
 */
static int join_galaxies_of_progenitors(const int halonr, const int fof_halonr, GalaxyArray* temp_fof_galaxies, int32_t *galaxycounter,
                                        struct halo_data *halos, struct halo_aux_data *haloaux,
                                        const GalaxyArray *galaxies_prev_snap, struct params *run_params)
{
    GalaxyArray *galaxies_for_this_halo = galaxy_array_new();
    if (!galaxies_for_this_halo) return EXIT_FAILURE;

    int status = copy_galaxies_from_progenitors(halonr, fof_halonr, galaxies_for_this_halo, galaxycounter,
                                               halos, haloaux, galaxies_prev_snap, run_params);
    if (status != EXIT_SUCCESS) {
        galaxy_array_free(&galaxies_for_this_halo);
        return EXIT_FAILURE;
    }

    int ngal_this_halo = galaxy_array_get_count(galaxies_for_this_halo);
    if (ngal_this_halo > 0) {
        struct GALAXY *raw_gals = galaxy_array_get_raw_data(galaxies_for_this_halo);
        if (set_galaxy_centrals(0, ngal_this_halo, raw_gals) != 0) {
             LOG_ERROR("Failed to set central galaxy for halo %d", halonr);
             galaxy_array_free(&galaxies_for_this_halo);
             return EXIT_FAILURE;
        }

        // Now append these correctly processed galaxies to the main FOF list.
        for (int i = 0; i < ngal_this_halo; ++i) {
            if (galaxy_array_append(temp_fof_galaxies, &raw_gals[i], run_params) < 0) {
                LOG_ERROR("Failed to append galaxy from halo %d to FOF list", halonr);
                galaxy_array_free(&galaxies_for_this_halo);
                return EXIT_FAILURE;
            }
        }
    }

    galaxy_array_free(&galaxies_for_this_halo);
    return EXIT_SUCCESS;
}
/* end of join_galaxies_of_progenitors */

/**
 * @brief   Main function to evolve galaxies through time using a configurable pipeline
 *
 * @param   halonr    Index of the FOF-background subhalo (main halo)
 * @param   ngal      Total number of galaxies to evolve
 * @param   numgals   Pointer to total galaxy count (updated as galaxies are finalized)
 * @param   halos     Array of halo data structures
 * @param   haloaux   Array of halo auxiliary data structures
 * @param   galaxies_arr Safe dynamic array for temporary galaxies
 * @param   halogal_arr  Safe dynamic array for permanent galaxies
 * @param   run_params Simulation parameters and configuration
 * @return  EXIT_SUCCESS on success, EXIT_FAILURE on error
 *
 * This function implements the time integration of galaxy properties between
 * two consecutive snapshots using a modular pipeline architecture. It:
 *
 * 1. Initializes the evolution context and diagnostics
 * 2. Sets up the physics pipeline from configuration
 * 3. Executes the evolution pipeline in four distinct phases:
 *    a. HALO phase: Processes calculations at the halo level
 *    b. GALAXY phase: Processes each galaxy individually through time steps
 *    c. POST phase: Handles inter-galaxy interactions and mergers
 *    d. FINAL phase: Finalizes properties and prepares output
 * 4. Manages memory allocation and property copying
 * 5. Updates final galaxy properties and attaches them to halos
 *
 * Pipeline Architecture:
 * The function uses a configurable pipeline system that maintains core-physics
 * separation. Physics modules are loaded at runtime and execute through 
 * standardized phase interfaces. This allows for:
 * - Runtime configurability of physics modules
 * - Complete physics-free operation for testing
 * - Modular physics development and testing
 * - Preservation of the original SAGE algorithm flow
 *
 * Time Integration:
 * The evolution uses the traditional STEPS-based time integration where each
 * snapshot interval is divided into STEPS substeps. Physics modules receive
 * the time step information and handle their own integration schemes.
 *
 * Error Handling:
 * - Comprehensive validation of evolution context
 * - Graceful degradation on pipeline failures
 * - Detailed diagnostic reporting
 * - Memory cleanup on error conditions
 */
/*
 * This function evolves galaxies and applies all physics modules.
 *
 * The physics modules are applied via a configurable pipeline system,
 * allowing modules to be replaced, reordered, or removed at runtime.
 * Synchronization calls are placed around phase executions.
 */
static int evolve_galaxies(const int halonr, GalaxyArray* temp_fof_galaxies, int *numgals, struct halo_data *halos,
                          struct halo_aux_data *haloaux, GalaxyArray *galaxies_this_snap, struct params *run_params)
{
    int ngal = galaxy_array_get_count(temp_fof_galaxies);
    struct GALAXY *galaxies = galaxy_array_get_raw_data(temp_fof_galaxies);

    // Initialize galaxy evolution context
    struct evolution_context ctx;
    initialize_evolution_context(&ctx, halonr, galaxies, ngal, halos, run_params);

    // Initialize core evolution diagnostics
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, halonr, ngal);
    ctx.diagnostics = &diag;

    // Perform comprehensive validation of the evolution context
    if (!validate_evolution_context(&ctx)) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Evolution context validation failed for halo %d", halonr);
        return EXIT_FAILURE;
    }

    /* Reduce noise - only log evolution starts for first 5 halos */
    static int evolution_start_count = 0;
    evolution_start_count++;
    if (evolution_start_count <= 5) {
        if (evolution_start_count == 5) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Starting evolution for halo %d with %d galaxies (start #%d - further messages suppressed)", halonr, ngal, evolution_start_count);
        } else {
            CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Starting evolution for halo %d with %d galaxies (start #%d)", halonr, ngal, evolution_start_count);
        }
    }

    // Validate central galaxy
    if(GALAXY_PROP_Type(&galaxies[ctx.centralgal]) != 0 || GALAXY_PROP_HaloNr(&galaxies[ctx.centralgal]) != halonr) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR,
                    "Invalid central galaxy: expected type=0, halonr=%d but found type=%d, halonr=%d",
                    halonr, GALAXY_PROP_Type(&galaxies[ctx.centralgal]), GALAXY_PROP_HaloNr(&galaxies[ctx.centralgal]));
        return EXIT_FAILURE;
    }


    // Set up pipeline context
    struct pipeline_context pipeline_ctx;
    pipeline_context_init(
        &pipeline_ctx,
        run_params,
        ctx.galaxies,
        ctx.ngal,
        ctx.centralgal,
        0.0,                // time (will be updated per galaxy)
        0.0,                // deltaT (will be updated per galaxy)
        ctx.halo_nr,        // halonr
        0,                  // step (will be updated in loop)
        &ctx                // user_data (set to evolution context)
    );
    pipeline_ctx.redshift = ctx.redshift; // Set the redshift from evolution context

    // Initialize property serialization if module extensions are enabled
    if (global_extension_registry != NULL && global_extension_registry->num_extensions > 0) {
        int status_prop = pipeline_init_property_serialization(&pipeline_ctx, PROPERTY_FLAG_SERIALIZE);
        if (status_prop != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to initialize property serialization");
            return EXIT_FAILURE;
        }
    }

    // Get the global physics pipeline
    struct module_pipeline *physics_pipeline = pipeline_get_global();
    
    // Validate pipeline is available (empty pipelines are valid in physics-free mode)
    if (physics_pipeline == NULL) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "No physics pipeline available. Unable to initialize pipeline system.");
        core_evolution_diagnostics_finalize(&diag);
        core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }
    
    static bool first_pipeline_usage = true;
    if (first_pipeline_usage) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_INFO, "Using physics pipeline '%s' with %d steps",
                physics_pipeline->name, physics_pipeline->num_steps);
        first_pipeline_usage = false;
    } else {
        static int pipeline_usage_count = 0;
        pipeline_usage_count++;
        if (pipeline_usage_count <= 5) {
            if (pipeline_usage_count == 5) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Using physics pipeline for halo %d (usage #%d - further messages suppressed)", ctx.halo_nr, pipeline_usage_count);
            } else {
                CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Using physics pipeline for halo %d (usage #%d)", ctx.halo_nr, pipeline_usage_count);
            }
        }
    }

    // Create merger event queue
    struct merger_event_queue merger_queue;
    init_merger_queue(&merger_queue);
    ctx.merger_queue = &merger_queue;
    
    // Set merger queue in pipeline context for physics modules
    pipeline_ctx.merger_queue = &merger_queue;

    /*
     * The galaxy evolution pipeline executes in four distinct phases:
     *
     * 1. HALO phase: 
     *    - Processes calculations at the halo level, outside the galaxy loop
     *    - Examples: gas infall onto halos, environment effects
     *    - Runs once per halo and affects all galaxies in the halo
     *
     * 2. GALAXY phase:
     *    - Processes each galaxy individually
     *    - Examples: star formation, cooling, AGN feedback
     *    - Runs for each non-merged galaxy
     *    - Mergers are detected but NOT executed (added to queue instead)
     *
     * 3. POST phase:
     *    - Processes after all galaxies have been evolved
     *    - Examples: multi-galaxy interactions, environment calculations
     *    - Runs once per halo after all galaxies processed
     *
     * 4. FINAL phase:
     *    - Handles final cleanup and output preparation
     *    - Examples: output unit conversion, derived property calculation
     *    - Runs once at the very end of evolution
     *
     * This phase organization maintains scientific consistency with the original
     * SAGE implementation while enabling modularity. Particularly, the merger
     * event queue ensures that all galaxies see the same pre-merger state during
     * physics calculations, just as in the original implementation.
     */
    
    // EXECUTE PIPELINE PHASES - with diagnostics tracking

    // Phase 1: HALO phase (outside galaxy loop)
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
    pipeline_ctx.execution_phase = PIPELINE_PHASE_HALO;
    int status = 0;

    // Ensure properties are available for the central galaxy if needed
    if (ctx.galaxies[ctx.centralgal].properties == NULL) {
        if (allocate_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params) != 0) {
            LOG_WARNING("Failed to allocate properties for central galaxy %d", ctx.centralgal);
        }
    }

    // Execute HALO phase (physics agnostic)
    status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_HALO);

    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);

    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute HALO phase for halo %d", halonr);
        core_evolution_diagnostics_finalize(&diag);
        core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }

    // Main integration steps
    for (int step = 0; step < STEPS; step++) {
        pipeline_ctx.step = step;

        // Reset merger queue for this timestep
        init_merger_queue(&merger_queue);

        // Phase 2: GALAXY phase (for each galaxy)
        core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
        pipeline_ctx.execution_phase = PIPELINE_PHASE_GALAXY;

        for (int p = 0; p < ctx.ngal; p++) {
            // Calculate the timestep for this galaxy (should be per-galaxy)
            ctx.deltaT = run_params->simulation.Age[GALAXY_PROP_SnapNum(&galaxies[p])] - run_params->simulation.Age[halos[halonr].SnapNum];
            ctx.time = run_params->simulation.Age[GALAXY_PROP_SnapNum(&galaxies[p])] - (step + 0.5) * (ctx.deltaT / STEPS);
            
            // Update pipeline context with current galaxy-specific time values
            pipeline_ctx.dt = ctx.deltaT / STEPS; // Per-step time interval
            pipeline_ctx.time = ctx.time;

            // Update context for current galaxy
            pipeline_ctx.current_galaxy = p;
            diag.phases[1].galaxy_count++; // Index 1 corresponds to GALAXY phase

            // Ensure properties are available for current galaxy if needed
            if (ctx.galaxies[p].properties == NULL) {
                if (allocate_galaxy_properties(&ctx.galaxies[p], run_params) != 0) {
                    LOG_WARNING("Failed to allocate properties for galaxy %d", p);
                }
            }

            // Execute GALAXY phase for current galaxy (physics agnostic)
            status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_GALAXY);

            if (status != 0) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute GALAXY phase for galaxy %d", p);
                core_evolution_diagnostics_finalize(&diag);
                core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
                return EXIT_FAILURE;
            }

            // In physics-free mode, no merger processing is performed
        } // End loop over galaxies p

        core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);

        // Phase 3: POST phase (after all galaxies processed in step)
        core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_POST);
        pipeline_ctx.execution_phase = PIPELINE_PHASE_POST;
        
        // Ensure central galaxy has properties allocated if needed
        if (ctx.galaxies[ctx.centralgal].properties == NULL) {
            if (allocate_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params) != 0) {
                LOG_WARNING("Failed to allocate properties for central galaxy %d", ctx.centralgal);
            }
        }

        // Phase 3: Process merger events using configurable physics handlers
        status = core_process_merger_queue_agnostically(&pipeline_ctx);
        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to process merger events for step %d", step);
            core_evolution_diagnostics_finalize(&diag);
            core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
            return EXIT_FAILURE;
        }

        // Phase 4: POST phase (physics agnostic)
        status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_POST);

        core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_POST);

        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute POST phase for step %d", step);
            core_evolution_diagnostics_finalize(&diag);
            core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
            return EXIT_FAILURE;
        }

        // Interval-based debug logging (first 5 merger processings only)
        static int merger_debug_count = 0;
        merger_debug_count++;
        if (merger_debug_count <= 5) {
            if (merger_debug_count == 5) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Processed %d merger events for step %d (further messages suppressed)",
                          merger_queue.num_events, step);
            } else {
                CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Processed %d merger events for step %d",
                          merger_queue.num_events, step);
            }
        }
        for (int i = 0; i < merger_queue.num_events; i++) {
            core_evolution_diagnostics_add_merger_processed(&diag, merger_queue.events[i].merger_type);
        }

    } // End loop over steps

    // Phase 4: FINAL phase (after all steps complete)
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_FINAL);
    pipeline_ctx.execution_phase = PIPELINE_PHASE_FINAL;

    // Ensure central galaxy has properties allocated if needed
    if (ctx.galaxies[ctx.centralgal].properties == NULL) {
        if (allocate_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params) != 0) {
            LOG_WARNING("Failed to allocate properties for central galaxy %d", ctx.centralgal);
        }
    }

    // Execute FINAL phase (physics agnostic)
    status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_FINAL);

    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_FINAL);

    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute FINAL phase for halo %d", halonr);
        core_evolution_diagnostics_finalize(&diag);
        core_evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }

    // Finalize and record property serialization if needed
    if (pipeline_ctx.prop_ctx != NULL) {
        pipeline_cleanup_property_serialization(&pipeline_ctx);
    }

    // Finalize and report core diagnostics
    core_evolution_diagnostics_finalize(&diag);
    core_evolution_diagnostics_report(&diag, LOG_LEVEL_INFO);

    // Extra miscellaneous stuff before finishing this halo
    
    const double time_diff = run_params->simulation.Age[GALAXY_PROP_SnapNum(&ctx.galaxies[0])] - ctx.halo_age;
    const double inv_deltaT = (time_diff > 1e-10) ? 1.0 / time_diff : 0.0; // Avoid division by zero or very small numbers
    (void)inv_deltaT; // Mark as intentionally unused

    // Remove merged galaxies from the galaxy array before attaching to final list
    // This compacts the array to only contain active galaxies (merged=0)
    int active_galaxies = 0;
    for(int p = 0; p < ctx.ngal; p++) {
        if (GALAXY_PROP_merged(&ctx.galaxies[p]) == 0) {
            if (active_galaxies != p) {
                // Move galaxy to compact position
                ctx.galaxies[active_galaxies] = ctx.galaxies[p];
            }
            active_galaxies++;
        } else {
            // Free resources for merged galaxy
            free_galaxy_properties(&ctx.galaxies[p]);
        }
    }
    ctx.ngal = active_galaxies; // Update galaxy count to active galaxies only

    // Attach final galaxy list to halo
    for(int p = 0, currenthalo = -1; p < ctx.ngal; p++) {
        if(GALAXY_PROP_HaloNr(&ctx.galaxies[p]) != currenthalo) {
            currenthalo = GALAXY_PROP_HaloNr(&ctx.galaxies[p]);
            haloaux[currenthalo].FirstGalaxy = *numgals;
            haloaux[currenthalo].NGalaxies = 0;
        }

        // After evolution, append survivors to the main array for this snapshot
        if (GALAXY_PROP_merged(&ctx.galaxies[p]) == 0) { // If it hasn't been merged
            GALAXY_PROP_SnapNum(&ctx.galaxies[p]) = halos[currenthalo].SnapNum;


            // Copy galaxy to the snapshot array using the new API
            struct GALAXY temp_galaxy;
            memset(&temp_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_galaxy);
            deep_copy_galaxy(&temp_galaxy, &ctx.galaxies[p], run_params);
            GALAXY_PROP_SnapNum(&temp_galaxy) = halos[currenthalo].SnapNum;
            


            if (galaxy_array_append(galaxies_this_snap, &temp_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append galaxy to snapshot array");
                free_galaxy_properties(&temp_galaxy);
                return EXIT_FAILURE;
            }
            free_galaxy_properties(&temp_galaxy);
            (*numgals)++;
            haloaux[currenthalo].NGalaxies++;
        }
    } // End loop attaching final galaxy list

    // Cleanup property serialization context if it was initialized
    if (pipeline_ctx.prop_ctx != NULL) {
        pipeline_cleanup_property_serialization(&pipeline_ctx);
    }

    return EXIT_SUCCESS;
}