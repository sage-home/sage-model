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
static inline void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params)
{
    // CRITICAL FIX: Avoid shallow copy that creates shared properties pointers
    // Instead, manually copy each field to prevent dangerous pointer aliasing
    
    // Core galaxy identification
    dest->SnapNum = src->SnapNum;
    dest->Type = src->Type;
    dest->GalaxyNr = src->GalaxyNr;
    dest->CentralGal = src->CentralGal;
    dest->HaloNr = src->HaloNr;
    dest->MostBoundID = src->MostBoundID;
    dest->GalaxyIndex = src->GalaxyIndex;
    dest->CentralGalaxyIndex = src->CentralGalaxyIndex;
    
    // Merger properties
    dest->mergeType = src->mergeType;
    dest->mergeIntoID = src->mergeIntoID;
    dest->mergeIntoSnapNum = src->mergeIntoSnapNum;
    dest->dT = src->dT;
    
    // Core halo properties
    for (int i = 0; i < 3; i++) {
        dest->Pos[i] = src->Pos[i];
        dest->Vel[i] = src->Vel[i];
    }
    dest->Len = src->Len;
    dest->Mvir = src->Mvir;
    dest->deltaMvir = src->deltaMvir;
    dest->CentralMvir = src->CentralMvir;
    dest->Rvir = src->Rvir;
    dest->Vvir = src->Vvir;
    dest->Vmax = src->Vmax;
    
    // Core merger tracking
    dest->MergTime = src->MergTime;
    
    // Core infall properties
    dest->infallMvir = src->infallMvir;
    dest->infallVvir = src->infallVvir;
    dest->infallVmax = src->infallVmax;
    
    // Extension mechanism
    dest->extension_data = src->extension_data;
    dest->num_extensions = src->num_extensions;
    dest->extension_flags = src->extension_flags;
    
    // CRITICAL: Initialize properties pointer to NULL before allocating
    dest->properties = NULL;
    
    // Deep copy the properties using the property system
    if (copy_galaxy_properties(dest, src, run_params) != 0) {
        LOG_ERROR("A failure occurred during the deep copy of galaxy properties. Source GalaxyIndex: %llu\n", src->GalaxyIndex);
        // In a production environment, you might want to add more robust error handling here,
        // such as returning an error code to the caller.
    }
}

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
static int copy_galaxies_from_progenitors(const int halonr, const int ngalstart, const int first_occupied,
                                         int *galaxycounter, struct halo_data *halos,
                                         struct halo_aux_data *haloaux, GalaxyArray *galaxies_arr,
                                         GalaxyArray *halogal_arr, struct params *run_params)
{
    int prog;
    struct GALAXY *halogal_raw = galaxy_array_get_raw_data(halogal_arr);

    prog = halos[halonr].FirstProgenitor;

    while (prog >= 0) {
        for (int i = 0; i < haloaux[prog].NGalaxies; i++) {
            // Get the source galaxy using the old index from the permanent array
            const struct GALAXY* source_gal = &halogal_raw[haloaux[prog].FirstGalaxy + i];

            // Create temporary galaxy for updates
            struct GALAXY temp_galaxy;
            memset(&temp_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_galaxy);
            deep_copy_galaxy(&temp_galaxy, source_gal, run_params);

            // Update galaxy properties
            temp_galaxy.HaloNr = halonr;
            temp_galaxy.dT = -1.0;
            
            /* Copy extension data (this will just copy flags since the data is accessed on demand) */
            galaxy_extension_copy(&temp_galaxy, source_gal);

            /* This deals with the central galaxies of (sub)halos */
            if (temp_galaxy.Type == 0 || temp_galaxy.Type == 1) {

                /* Remember properties from the last snapshot */
                const float previousMvir = temp_galaxy.Mvir;
                const float previousVvir = temp_galaxy.Vvir;
                const float previousVmax = temp_galaxy.Vmax;

                if (prog == first_occupied) {
                    /* Update properties of this galaxy with physical properties of halo */
                    temp_galaxy.MostBoundID = halos[halonr].MostBoundID;

                    for (int j = 0; j < 3; j++) {
                        temp_galaxy.Pos[j] = halos[halonr].Pos[j];
                        temp_galaxy.Vel[j] = halos[halonr].Vel[j];
                    }

                    temp_galaxy.Len = halos[halonr].Len;
                    temp_galaxy.Vmax = halos[halonr].Vmax;

                    temp_galaxy.deltaMvir = get_virial_mass(halonr, halos, run_params) - temp_galaxy.Mvir;

                    if (get_virial_mass(halonr, halos, run_params) > temp_galaxy.Mvir) {
                        temp_galaxy.Rvir = get_virial_radius(halonr, halos, run_params);  /* use the maximum Rvir in model */
                        temp_galaxy.Vvir = get_virial_velocity(halonr, halos, run_params);  /* use the maximum Vvir in model */
                    }
                    temp_galaxy.Mvir = get_virial_mass(halonr, halos, run_params);

                    if (halonr == halos[halonr].FirstHaloInFOFgroup) {
                        /* A central galaxy */
                        temp_galaxy.Type = 0;
                    } else {
                        /* A satellite with subhalo */
                        if (temp_galaxy.Type == 0) {  /* remember the infall properties before becoming a subhalo */
                            temp_galaxy.infallMvir = previousMvir;
                            temp_galaxy.infallVvir = previousVvir;
                            temp_galaxy.infallVmax = previousVmax;
                        }
                        temp_galaxy.Type = 1;
                    }
                } else {
                    /* An orphan satellite galaxy - in physics-free mode, just update properties */
                    temp_galaxy.deltaMvir = -1.0 * temp_galaxy.Mvir;
                    temp_galaxy.Mvir = 0.0;
                    
                    temp_galaxy.infallMvir = previousMvir;
                    temp_galaxy.infallVvir = previousVvir;
                    temp_galaxy.infallVmax = previousVmax;
                    
                    temp_galaxy.Type = 2;
                }
            }

            // Append the fully updated temporary galaxy to the safe array
            if (galaxy_array_append(galaxies_arr, &temp_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append galaxy in copy_galaxies_from_progenitors.");
                free_galaxy_properties(&temp_galaxy); // Clean up temp
                return -1;
            }

            // The temporary galaxy is no longer needed
            free_galaxy_properties(&temp_galaxy);
        }

        prog = halos[prog].NextProgenitor;
    }

    /* If we have no progenitors with galaxies, create a new galaxy if this is the main subhalo */
    if (galaxy_array_get_count(galaxies_arr) == ngalstart) {
        if (halonr == halos[halonr].FirstHaloInFOFgroup) {
            // Create temporary galaxy for new initialization
            struct GALAXY temp_new_galaxy;
            memset(&temp_new_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_new_galaxy);
            
            // Initialize the new galaxy using existing function
            init_galaxy(0, halonr, galaxycounter, halos, &temp_new_galaxy, run_params);
            
            // Append to safe array
            if (galaxy_array_append(galaxies_arr, &temp_new_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append new galaxy in copy_galaxies_from_progenitors.");
                free_galaxy_properties(&temp_new_galaxy);
                return -1;
            }
            
            // Clean up temporary galaxy
            free_galaxy_properties(&temp_new_galaxy);
        }
    }

    return galaxy_array_get_count(galaxies_arr);
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
        if (galaxies[i].Type == 0 || galaxies[i].Type == 1) {
            if (centralgal != -1) {
                LOG_ERROR("Expected to find centralgal=-1, instead centralgal=%d", centralgal);
                return -1;
            }
            centralgal = i;
        }
    }

    /* Set all galaxies to point to the central galaxy */
    for (i = ngalstart; i < ngal; i++) {
        galaxies[i].CentralGal = centralgal;
    }
    
    return 0;
}


static int evolve_galaxies(const int halonr, const int ngal, int *numgals, struct halo_data *halos,
                           struct halo_aux_data *haloaux, GalaxyArray *galaxies_arr, GalaxyArray *halogal_arr, struct params *run_params);
static int join_galaxies_of_progenitors(const int halonr, const int ngalstart, int *galaxycounter, struct halo_data *halos,
                                        struct halo_aux_data *haloaux, GalaxyArray *galaxies_arr, GalaxyArray *halogal_arr, struct params *run_params);

/**
 * @brief   Recursively constructs galaxies by traversing the merger tree
 *
 * @param   halonr    Index of the current halo in the halo array
 * @param   numgals   Pointer to total number of galaxies created
 * @param   galaxycounter Galaxy counter for unique ID assignment
 * @param   halos     Array of halo data structures
 * @param   haloaux   Array of halo auxiliary data structures
 * @param   galaxies_arr Safe dynamic array for temporary galaxies
 * @param   halogal_arr  Safe dynamic array for permanent galaxies
 * @param   run_params Simulation parameters and configuration
 * @return  EXIT_SUCCESS on success, EXIT_FAILURE on error
 *
 * This function traverses the merger tree in a depth-first manner to ensure
 * that galaxies are constructed from their progenitors before being evolved.
 * It follows these steps:
 *
 * 1. First processes all progenitors of the current halo recursively
 * 2. Then processes all halos in the same FOF group
 * 3. Finally, joins progenitor galaxies and evolves them forward in time
 *
 * The recursive approach ensures that galaxies are built in the correct
 * chronological order, preserving the flow of mass and properties from
 * high redshift to low redshift. The function maintains core-physics separation
 * by delegating physical processes to the configurable pipeline system.
 *
 * Error handling:
 * - Returns EXIT_FAILURE if any recursive call fails
 * - Logs detailed error messages for debugging
 * - Preserves system state for cleanup on failure
 */
/* the only externally visible function */
int construct_galaxies(const int halonr, int *numgals, int32_t *galaxycounter, struct halo_data *halos,
                       struct halo_aux_data *haloaux, GalaxyArray *galaxies_arr, GalaxyArray *halogal_arr,
                       struct params *run_params)
{
  int prog, fofhalo;

  haloaux[halonr].DoneFlag = 1;

  /* First, recursively process all progenitors of the current halo */
  prog = halos[halonr].FirstProgenitor;
  while(prog >= 0) {
      if(haloaux[prog].DoneFlag == 0) {
          int status = construct_galaxies(prog, numgals, galaxycounter, halos, haloaux, galaxies_arr, halogal_arr, run_params);

          if(status != EXIT_SUCCESS) {
              LOG_ERROR("Failed to construct galaxies for progenitor %d", prog);
              return status;
          }
      }
      prog = halos[prog].NextProgenitor;
  }

  /* Second, process all progenitors of other halos in the same FOF group */
  fofhalo = halos[halonr].FirstHaloInFOFgroup;
  if(haloaux[fofhalo].HaloFlag == 0) {
      haloaux[fofhalo].HaloFlag = 1;
      while(fofhalo >= 0) {
          prog = halos[fofhalo].FirstProgenitor;
          while(prog >= 0) {
              if(haloaux[prog].DoneFlag == 0) {
                  int status = construct_galaxies(prog, numgals, galaxycounter, halos, haloaux, galaxies_arr, halogal_arr, run_params);

                  if(status != EXIT_SUCCESS) {
                      LOG_ERROR("Failed to construct galaxies for FOF group progenitor %d", prog);
                      return status;
                  }
              }
              prog = halos[prog].NextProgenitor;
          }
          fofhalo = halos[fofhalo].NextHaloInFOFgroup;
      }
  }

  /* At this point, the galaxies for all progenitors of this halo have been
   * properly constructed. Also, the galaxies of the progenitors of all other
   * halos in the same FOF group have been constructed as well. We can hence go
   * ahead and construct all galaxies for the subhalos in this FOF halo, and
   * evolve them in time. */

  fofhalo = halos[halonr].FirstHaloInFOFgroup;
#ifdef USE_SAGE_IN_MCMC_MODE
  /* The extra condition stops sage from evolving any galaxies beyond the final output snapshot.
     This optimised processing reduces the values GalaxyIndex and CentralGalaxyIndex (since fewer galaxies are
     now processed). The values of mergetype, mergeintosnapnum and mergeintoid are all different that what
     would be the case if *all* snapshots were processed. This will lead to different SEDs compared to the
     fiducial runs -> however, for MCMC cases, presumably we are not interested in SED. This extra flag
     improves runtime *significantly* if only processing up to high-z (say for targeting JWST-like observations).
     - MS, DC: 25th Oct, 2023 */
  if(haloaux[fofhalo].HaloFlag == 1 && halos[fofhalo].SnapNum <= run_params->ListOutputSnaps[0]) {
#else
  if(haloaux[fofhalo].HaloFlag == 1 ) {
#endif
      int ngal = 0;
      haloaux[fofhalo].HaloFlag = 2;

      /* Third, join galaxies from all progenitors within the FOF group */
      while(fofhalo >= 0) {
          ngal = join_galaxies_of_progenitors(fofhalo, ngal, galaxycounter, halos, haloaux, galaxies_arr, halogal_arr, run_params);
          if(ngal < 0) {
              LOG_ERROR("Failed to join galaxies of progenitors for FOF halo %d", fofhalo);
              return EXIT_FAILURE;
          }
          fofhalo = halos[fofhalo].NextHaloInFOFgroup;
      }

      /* Finally, evolve all galaxies in the FOF group through time */
      LOG_DEBUG("Evolving %d galaxies in halo %d", ngal, halonr);
      int status = evolve_galaxies(halos[halonr].FirstHaloInFOFgroup, ngal, numgals, halos, haloaux, galaxies_arr, halogal_arr, run_params);

      if(status != EXIT_SUCCESS) {
          LOG_ERROR("Failed to evolve galaxies in FOF group %d", halos[halonr].FirstHaloInFOFgroup);
          return status;
      }
  }

  return EXIT_SUCCESS;
}
/* end of construct_galaxies*/


/**
 * @brief   Main function to join galaxies from progenitor halos
 *
 * @param   halonr       Index of the current halo in the halo array
 * @param   ngalstart    Starting index for galaxies in the galaxy array
 * @param   galaxycounter Galaxy counter for ID assignment
 * @param   halos       Array of halo data structures
 * @param   haloaux     Array of halo auxiliary data structures
 * @param   galaxies_arr Safe dynamic array for temporary galaxies
 * @param   halogal_arr Safe dynamic array for permanent galaxies
 * @param   run_params  Simulation parameters
 * @return  Updated number of galaxies after joining
 *
 * This function coordinates the process of integrating galaxies from
 * progenitor halos into the current halo. It performs three main steps:
 *
 * 1. Identifies the most massive progenitor with galaxies
 * 2. Copies and updates galaxies from all progenitors
 * 3. Establishes relationships between galaxies (central/satellite)
 *
 * The function ensures proper inheritance of galaxy properties while
 * maintaining the hierarchy of central and satellite galaxies.
 */
static int join_galaxies_of_progenitors(const int halonr, const int ngalstart, int *galaxycounter, struct halo_data *halos,
                                       struct halo_aux_data *haloaux, GalaxyArray *galaxies_arr, GalaxyArray *halogal_arr, struct params *run_params)
{
    int ngal, first_occupied;

    /* Find the most massive progenitor with galaxies */
    first_occupied = find_most_massive_progenitor(halonr, halos, haloaux);

    /* Copy galaxies from progenitors to the current snapshot */
    ngal = copy_galaxies_from_progenitors(halonr, ngalstart, first_occupied, galaxycounter, 
                                         halos, haloaux, galaxies_arr, halogal_arr, run_params);
    if (ngal < 0) {
        LOG_ERROR("Failed to copy galaxies from progenitors for halo %d", halonr);
        return -1;
    }

    /* Set up central galaxy relationships */
    struct GALAXY* galaxies_raw = galaxy_array_get_raw_data(galaxies_arr);
    if (set_galaxy_centrals(ngalstart, ngal, galaxies_raw) != 0) {
        LOG_ERROR("Failed to set central galaxy relationships for halo %d", halonr);
        return -1;
    }
    
    LOG_DEBUG("Joined progenitor galaxies for halo %d: ngal=%d", halonr, ngal);

    return ngal;
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
static int evolve_galaxies(const int halonr, const int ngal, int *numgals, struct halo_data *halos,
                          struct halo_aux_data *haloaux, GalaxyArray *galaxies_arr, GalaxyArray *halogal_arr,
                          struct params *run_params)
{
    struct GALAXY *galaxies = galaxy_array_get_raw_data(galaxies_arr);
    struct GALAXY *halogal_raw = galaxy_array_get_raw_data(halogal_arr);

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

    CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Starting evolution for halo %d with %d galaxies", halonr, ngal);

    // Validate central galaxy
    if(galaxies[ctx.centralgal].Type != 0 || galaxies[ctx.centralgal].HaloNr != halonr) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR,
                    "Invalid central galaxy: expected type=0, halonr=%d but found type=%d, halonr=%d",
                    halonr, galaxies[ctx.centralgal].Type, galaxies[ctx.centralgal].HaloNr);
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
    
    // Log pipeline usage
    static bool first_pipeline_usage = true;
    if (first_pipeline_usage) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_INFO, "Using physics pipeline '%s' with %d steps",
                physics_pipeline->name, physics_pipeline->num_steps);
        first_pipeline_usage = false;
    } else {
        CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Using physics pipeline for halo %d", ctx.halo_nr);
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
            ctx.deltaT = run_params->simulation.Age[galaxies[p].SnapNum] - run_params->simulation.Age[halos[halonr].SnapNum];
            ctx.time = run_params->simulation.Age[galaxies[p].SnapNum] - (step + 0.5) * (ctx.deltaT / STEPS);
            
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

        // Diagnostic tracking for processed mergers
        CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Processed %d merger events for step %d",
                  merger_queue.num_events, step);
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
    
    const double time_diff = run_params->simulation.Age[ctx.galaxies[0].SnapNum] - ctx.halo_age;
    const double inv_deltaT = (time_diff > 1e-10) ? 1.0 / time_diff : 0.0; // Avoid division by zero or very small numbers
    (void)inv_deltaT; // Mark as intentionally unused

    // Attach final galaxy list to halo
    for(int p = 0, currenthalo = -1; p < ctx.ngal; p++) {
        if(ctx.galaxies[p].HaloNr != currenthalo) {
            currenthalo = ctx.galaxies[p].HaloNr;
            haloaux[currenthalo].FirstGalaxy = *numgals;
            haloaux[currenthalo].NGalaxies = 0;
        }

        // In physics-free mode, save all galaxies without merger processing
        {
            ctx.galaxies[p].SnapNum = halos[currenthalo].SnapNum;

            // Copy galaxy to the permanent list using the new API
            struct GALAXY temp_galaxy;
            memset(&temp_galaxy, 0, sizeof(struct GALAXY));
            galaxy_extension_initialize(&temp_galaxy);
            deep_copy_galaxy(&temp_galaxy, &ctx.galaxies[p], run_params);
            temp_galaxy.SnapNum = halos[currenthalo].SnapNum;

            if (galaxy_array_append(halogal_arr, &temp_galaxy, run_params) < 0) {
                LOG_ERROR("Failed to append galaxy to final array");
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