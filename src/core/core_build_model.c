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

#include "../physics/model_misc.h"
#include "../physics/model_mergers.h"
#include "../physics/model_infall.h"
#include "../physics/model_reincorporation.h"
#include "../physics/model_starformation_and_feedback.h"
#include "../physics/model_cooling_heating.h"
#include "../core/core_parameter_views.h"


static int evolve_galaxies(const int halonr, const int ngal, int *numgals, int *maxgals, struct halo_data *halos,
                           struct halo_aux_data *haloaux, struct GALAXY **ptr_to_galaxies, struct GALAXY **ptr_to_halogal, struct params *run_params);
static int join_galaxies_of_progenitors(const int halonr, const int ngalstart, int *galaxycounter, int *maxgals, struct halo_data *halos,
                                        struct halo_aux_data *haloaux, struct GALAXY **ptr_to_galaxies, struct GALAXY **ptr_to_halogal, struct params *run_params);



/* the only externally visible function */
int construct_galaxies(const int halonr, int *numgals, int *galaxycounter, int *maxgals, struct halo_data *halos,
                       struct halo_aux_data *haloaux, struct GALAXY **ptr_to_galaxies, struct GALAXY **ptr_to_halogal,
                       struct params *run_params)
{
  int prog, fofhalo;

  haloaux[halonr].DoneFlag = 1;

  prog = halos[halonr].FirstProgenitor;
  while(prog >= 0) {
      if(haloaux[prog].DoneFlag == 0) {
          int status = construct_galaxies(prog, numgals, galaxycounter, maxgals, halos, haloaux, ptr_to_galaxies, ptr_to_halogal, run_params);

          if(status != EXIT_SUCCESS) {
              LOG_ERROR("Failed to construct galaxies for progenitor %d", prog);
              return status;
          }
      }
      prog = halos[prog].NextProgenitor;
  }

  fofhalo = halos[halonr].FirstHaloInFOFgroup;
  if(haloaux[fofhalo].HaloFlag == 0) {
      haloaux[fofhalo].HaloFlag = 1;
      while(fofhalo >= 0) {
          prog = halos[fofhalo].FirstProgenitor;
          while(prog >= 0) {
              if(haloaux[prog].DoneFlag == 0) {
                  int status = construct_galaxies(prog, numgals, galaxycounter, maxgals, halos, haloaux, ptr_to_galaxies, ptr_to_halogal, run_params);

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

  // At this point, the galaxies for all progenitors of this halo have been
  // properly constructed. Also, the galaxies of the progenitors of all other
  // halos in the same FOF group have been constructed as well. We can hence go
  // ahead and construct all galaxies for the subhalos in this FOF halo, and
  // evolve them in time.

  fofhalo = halos[halonr].FirstHaloInFOFgroup;
#ifdef USE_SAGE_IN_MCMC_MODE
  /* The extra condition stops sage from evolving any galaxies beyond the final output snapshot.
     This optimised processing reduces the values GalaxyIndex and CentralGalaxyIndex (since fewer galaxies are
     now processed). The values of mergetype, mergeintosnapnum and mergeintoid are all different that what
     would be the case if *all* snapshots were processed. This will lead to different SEDs compared to the
     fiducial runs -> however, for MCMC cases, presumably we are not interested in SED. This extra flag
     improves runtime *significantly* if only processing up to high-z (say for targeting JWST-like observations).
     - MS, DC: 25th Oct, 2023
  */
  if(haloaux[fofhalo].HaloFlag == 1 && halos[fofhalo].SnapNum <= run_params->ListOutputSnaps[0]) {
#else
  if(haloaux[fofhalo].HaloFlag == 1 ) {
#endif
      int ngal = 0;
      haloaux[fofhalo].HaloFlag = 2;

      while(fofhalo >= 0) {
          ngal = join_galaxies_of_progenitors(fofhalo, ngal, galaxycounter, maxgals, halos, haloaux, ptr_to_galaxies, ptr_to_halogal, run_params);
          if(ngal < 0) {
              LOG_ERROR("Failed to join galaxies of progenitors for FOF halo %d", fofhalo);
              return EXIT_FAILURE;
          }
          fofhalo = halos[fofhalo].NextHaloInFOFgroup;
      }

      LOG_DEBUG("Evolving %d galaxies in halo %d", ngal, halonr);
      int status = evolve_galaxies(halos[halonr].FirstHaloInFOFgroup, ngal, numgals, maxgals, halos, haloaux, ptr_to_galaxies, ptr_to_halogal, run_params);

      if(status != EXIT_SUCCESS) {
          LOG_ERROR("Failed to evolve galaxies in FOF group %d", halos[halonr].FirstHaloInFOFgroup);
          return status;
      }
  }


  return EXIT_SUCCESS;
}
/* end of construct_galaxies*/


static int join_galaxies_of_progenitors(const int halonr, const int ngalstart, int *galaxycounter, int *maxgals, struct halo_data *halos,
                                       struct halo_aux_data *haloaux, struct GALAXY **ptr_to_galaxies, struct GALAXY **ptr_to_halogal, struct params *run_params)
{
    int ngal, prog,  first_occupied, lenmax, lenoccmax;
    struct GALAXY *galaxies = *ptr_to_galaxies;
    struct GALAXY *halogal = *ptr_to_halogal;

    lenmax = 0;
    lenoccmax = 0;
    first_occupied = halos[halonr].FirstProgenitor;
    prog = halos[halonr].FirstProgenitor;

    if(prog >=0) {
        if(haloaux[prog].NGalaxies > 0) {
            lenoccmax = -1;
        }
    }

    // Find most massive progenitor that contains an actual galaxy
    // Maybe FirstProgenitor never was FirstHaloInFOFGroup and thus has no galaxy

    while(prog >= 0) {
        if(halos[prog].Len > lenmax) {
            lenmax = halos[prog].Len;
        }

        if(lenoccmax != -1 && halos[prog].Len > lenoccmax && haloaux[prog].NGalaxies > 0) {
            lenoccmax = halos[prog].Len;
            first_occupied = prog;
        }
        prog = halos[prog].NextProgenitor;
    }

    ngal = ngalstart;
    prog = halos[halonr].FirstProgenitor;

    while(prog >= 0) {
        for(int i = 0; i < haloaux[prog].NGalaxies; i++) {
            if(ngal == (*maxgals - 1)) {
                *maxgals += 10000;

                *ptr_to_galaxies = myrealloc(*ptr_to_galaxies, *maxgals * sizeof(struct GALAXY));
                *ptr_to_halogal  = myrealloc(*ptr_to_halogal, *maxgals * sizeof(struct GALAXY));
                galaxies = *ptr_to_galaxies;
                halogal = *ptr_to_halogal;
            }

            XRETURN(ngal < *maxgals, -1,
                    "Error: ngal = %d exceeds the number of galaxies allocated = %d\n"
                    "This would result in invalid memory access...exiting\n",
                    ngal, *maxgals);

            // This is the crucial line in which the properties of the progenitor galaxies
            // are copied over (as a whole) to the (temporary) galaxies galaxies[xxx] in the current snapshot
            // After updating their properties and evolving them
            // they are copied to the end of the list of permanent galaxies halogal[xxx]

            // First initialize the extension data (to maintain binary compatibility)
            galaxy_extension_initialize(&galaxies[ngal]);
            
            // Then copy the basic GALAXY structure
            galaxies[ngal] = halogal[haloaux[prog].FirstGalaxy + i];
            galaxies[ngal].HaloNr = halonr;
            galaxies[ngal].dT = -1.0;
            
            // Copy extension data (this will just copy flags since the data is accessed on demand)
            galaxy_extension_copy(&galaxies[ngal], &halogal[haloaux[prog].FirstGalaxy + i]);

            // this deals with the central galaxies of (sub)halos
            if(galaxies[ngal].Type == 0 || galaxies[ngal].Type == 1) {
                // this halo shouldn't hold a galaxy that has already merged; remove it from future processing
                if(galaxies[ngal].mergeType != 0) {
                    galaxies[ngal].Type = 3;
                    continue;
                }

                // remember properties from the last snapshot
                const float previousMvir = galaxies[ngal].Mvir;
                const float previousVvir = galaxies[ngal].Vvir;
                const float previousVmax = galaxies[ngal].Vmax;

                if(prog == first_occupied) {
                    // update properties of this galaxy with physical properties of halo
                    galaxies[ngal].MostBoundID = halos[halonr].MostBoundID;

                    for(int j = 0; j < 3; j++) {
                        galaxies[ngal].Pos[j] = halos[halonr].Pos[j];
                        galaxies[ngal].Vel[j] = halos[halonr].Vel[j];
                    }

                    galaxies[ngal].Len = halos[halonr].Len;
                    galaxies[ngal].Vmax = halos[halonr].Vmax;

                    galaxies[ngal].deltaMvir = get_virial_mass(halonr, halos, run_params) - galaxies[ngal].Mvir;

                    if(get_virial_mass(halonr, halos, run_params) > galaxies[ngal].Mvir) {
                        galaxies[ngal].Rvir = get_virial_radius(halonr, halos, run_params);  // use the maximum Rvir in model
                        galaxies[ngal].Vvir = get_virial_velocity(halonr, halos, run_params);  // use the maximum Vvir in model
                    }
                    galaxies[ngal].Mvir = get_virial_mass(halonr, halos, run_params);

                    galaxies[ngal].Cooling = 0.0;
                    galaxies[ngal].Heating = 0.0;
                    galaxies[ngal].QuasarModeBHaccretionMass = 0.0;
                    galaxies[ngal].OutflowRate = 0.0;

                    for(int step = 0; step < STEPS; step++) {
                        galaxies[ngal].SfrDisk[step] = galaxies[ngal].SfrBulge[step] = 0.0;
                        galaxies[ngal].SfrDiskColdGas[step] = galaxies[ngal].SfrDiskColdGasMetals[step] = 0.0;
                        galaxies[ngal].SfrBulgeColdGas[step] = galaxies[ngal].SfrBulgeColdGasMetals[step] = 0.0;
                    }

                    if(halonr == halos[halonr].FirstHaloInFOFgroup) {
                        // a central galaxy
                        galaxies[ngal].mergeType = 0;
                        galaxies[ngal].mergeIntoID = -1;
                        galaxies[ngal].MergTime = 999.9f;

                        galaxies[ngal].DiskScaleRadius = get_disk_radius(halonr, ngal, halos, galaxies);

                        galaxies[ngal].Type = 0;
                    } else {
                        // a satellite with subhalo
                        galaxies[ngal].mergeType = 0;
                        galaxies[ngal].mergeIntoID = -1;

                        if(galaxies[ngal].Type == 0) {  // remember the infall properties before becoming a subhalo
                            galaxies[ngal].infallMvir = previousMvir;
                            galaxies[ngal].infallVvir = previousVvir;
                            galaxies[ngal].infallVmax = previousVmax;
                        }

                        if(galaxies[ngal].Type == 0 || galaxies[ngal].MergTime > 999.0f) {
                            // here the galaxy has gone from type 1 to type 2 or otherwise doesn't have a merging time.
                            galaxies[ngal].MergTime = estimate_merging_time(halonr, halos[halonr].FirstHaloInFOFgroup, ngal, halos, galaxies, run_params);
                        }

                        galaxies[ngal].Type = 1;
                    }
                } else {
                    // an orphan satellite galaxy - these will merge or disrupt within the current timestep
                    galaxies[ngal].deltaMvir = -1.0*galaxies[ngal].Mvir;
                    galaxies[ngal].Mvir = 0.0;

                    if(galaxies[ngal].MergTime > 999.0 || galaxies[ngal].Type == 0) {
                        // here the galaxy has gone from type 0 to type 2 - merge it!
                        galaxies[ngal].MergTime = 0.0;

                        galaxies[ngal].infallMvir = previousMvir;
                        galaxies[ngal].infallVvir = previousVvir;
                        galaxies[ngal].infallVmax = previousVmax;
                    }

                    galaxies[ngal].Type = 2;
                }
            }

            ngal++;
        }

        prog = halos[prog].NextProgenitor;
    }

    if(ngal == 0) {
        // We have no progenitors with galaxies. This means we create a new galaxy.
        init_galaxy(ngal, halonr, galaxycounter, halos, galaxies, run_params);
        ngal++;
    }

    // Per Halo there can be only one Type 0 or 1 galaxy, all others are Type 2  (orphan)
    // In fact, this galaxy is very likely to be the first galaxy in the halo if
    // first_occupied==FirstProgenitor and the Type0/1 galaxy in FirstProgenitor was also the first one
    // This cannot be guaranteed though for the pathological first_occupied!=FirstProgenitor case

    int centralgal = -1;
    for(int i = ngalstart; i < ngal; i++) {
        if(galaxies[i].Type == 0 || galaxies[i].Type == 1) {
            if(centralgal != -1) {
                LOG_ERROR("Expected to find centralgal=-1, instead centralgal=%d", centralgal);
                return -1;
            }

            centralgal = i;
        }
    }

    for(int i = ngalstart; i < ngal; i++) {
        galaxies[i].CentralGal = centralgal;
    }
    
    LOG_DEBUG("Joined progenitor galaxies for halo %d: ngal=%d", halonr, ngal);

    return ngal;

}
/* end of join_galaxies_of_progenitors */

/*
 * This function evolves galaxies and applies all physics modules.
 * 
 * REFACTORING NOTE: This function will be transformed into the physics module pipeline
 * controller in Phase 2 of the refactoring. The fixed sequence of physics processes
 * will be replaced with a configurable pipeline where modules can be dynamically loaded,
 * replaced, or reordered at runtime.
 */
static int evolve_galaxies(const int halonr, const int ngal, int *numgals, int *maxgals, struct halo_data *halos,
                          struct halo_aux_data *haloaux, struct GALAXY **ptr_to_galaxies, struct GALAXY **ptr_to_halogal,
                          struct params *run_params)
{
    struct GALAXY *galaxies = *ptr_to_galaxies;
    struct GALAXY *halogal = *ptr_to_halogal;

    // Initialize galaxy evolution context
    struct evolution_context ctx;
    initialize_evolution_context(&ctx, halonr, galaxies, ngal, halos, run_params);
    
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
    
    // PHYSICS MODULE: Infall - calculates gas falling into the halo
    // REFACTORING NOTE: Will become a pluggable module in Phase 2
    const double infallingGas = infall_recipe(ctx.centralgal, ctx.ngal, ctx.redshift, ctx.galaxies, run_params);

    // We integrate things forward by using a number of intervals equal to STEPS
    for(int step = 0; step < STEPS; step++) {

        // Loop over all galaxies in the halo
        for(int p = 0; p < ctx.ngal; p++) {
            // Don't treat galaxies that have already merged
            if(ctx.galaxies[p].mergeType > 0) {
                continue;
            }

            // Calculate time step and current time
            const double deltaT = run_params->simulation.Age[ctx.galaxies[p].SnapNum] - ctx.halo_age;
            const double time = run_params->simulation.Age[ctx.galaxies[p].SnapNum] - (step + 0.5) * (deltaT / STEPS);
            ctx.deltaT = deltaT;  // Store in context for potential future module use

            if(ctx.galaxies[p].dT < 0.0) {
                ctx.galaxies[p].dT = deltaT;
            }

            // PHYSICS MODULE: Infall - adds gas to the central galaxy
            // REFACTORING NOTE: Will become part of the infall module in Phase 2
            if(p == ctx.centralgal) {
                add_infall_to_hot(ctx.centralgal, infallingGas / STEPS, ctx.galaxies);

                // PHYSICS MODULE: Reincorporation - adds ejected gas back to the hot component
                // REFACTORING NOTE: Will become a pluggable module in Phase 2
                if(run_params->physics.ReIncorporationFactor > 0.0) {
                    struct reincorporation_params_view reincorp_params;
                    initialize_reincorporation_params_view(&reincorp_params, run_params);
                    reincorporate_gas(ctx.centralgal, deltaT / STEPS, ctx.galaxies, &reincorp_params);
                }
            } else {
                // PHYSICS MODULE: Stripping - removes gas from satellite galaxies
                // REFACTORING NOTE: Will become part of the environmental effects module in Phase 2
                if(ctx.galaxies[p].Type == 1 && ctx.galaxies[p].HotGas > 0.0) {
                    strip_from_satellite(ctx.centralgal, p, ctx.redshift, ctx.galaxies, run_params);
                }
            }

            // PHYSICS MODULE: Cooling - converts hot gas to cold gas
            // REFACTORING NOTE: Will become a pluggable module in Phase 2
            struct cooling_params_view cooling_params;
            initialize_cooling_params_view(&cooling_params, run_params);
            double coolingGas = cooling_recipe(p, deltaT / STEPS, ctx.galaxies, &cooling_params);
            cool_gas_onto_galaxy(p, coolingGas, ctx.galaxies);

            // PHYSICS MODULE: Star Formation and Feedback - forms stars and heats/ejects gas
            // REFACTORING NOTE: Will become a pluggable module in Phase 2
            struct star_formation_params_view sf_params;
            struct feedback_params_view fb_params;
            initialize_star_formation_params_view(&sf_params, run_params);
            initialize_feedback_params_view(&fb_params, run_params);
            starformation_and_feedback(p, ctx.centralgal, time, deltaT / STEPS, ctx.halo_nr, step, 
                                     ctx.galaxies, &sf_params, &fb_params);
        }

        // PHYSICS MODULE: Mergers and Disruption - handles satellite galaxy fate
        // REFACTORING NOTE: Will become a pluggable module in Phase 2
        for(int p = 0; p < ctx.ngal; p++) {
            // satellite galaxy!
            if((ctx.galaxies[p].Type == 1 || ctx.galaxies[p].Type == 2) && ctx.galaxies[p].mergeType == 0) {
                if(ctx.galaxies[p].MergTime >= 999.0) {
                    CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, 
                                "Galaxy %d has MergTime = %lf, which is too large! Should have been within the age of the Universe",
                                p, ctx.galaxies[p].MergTime);
                    return EXIT_FAILURE;
                }

                const double deltaT = run_params->simulation.Age[ctx.galaxies[p].SnapNum] - ctx.halo_age;
                ctx.galaxies[p].MergTime -= deltaT / STEPS;

                // only consider mergers or disruption for halo-to-baryonic mass ratios below the threshold
                // or for satellites with no baryonic mass (they don't grow and will otherwise hang around forever)
                double currentMvir = ctx.galaxies[p].Mvir - ctx.galaxies[p].deltaMvir * (1.0 - ((double)step + 1.0) / (double)STEPS);
                double galaxyBaryons = ctx.galaxies[p].StellarMass + ctx.galaxies[p].ColdGas;
                if((galaxyBaryons == 0.0) || (galaxyBaryons > 0.0 && (currentMvir / galaxyBaryons <= run_params->physics.ThresholdSatDisruption))) {

                    int merger_centralgal = ctx.galaxies[p].Type==1 ? ctx.centralgal : ctx.galaxies[p].CentralGal;

                    if(ctx.galaxies[merger_centralgal].mergeType > 0) {
                        merger_centralgal = ctx.galaxies[merger_centralgal].CentralGal;
                    }

                    ctx.galaxies[p].mergeIntoID = *numgals + merger_centralgal;  // position in output

                    if(isfinite(ctx.galaxies[p].MergTime)) {
                        // PHYSICS MODULE: Disruption - satellite is disrupted and stars added to ICS
                        // REFACTORING NOTE: Will become part of the merger module in Phase 2
                        if(ctx.galaxies[p].MergTime > 0.0) {
                            disrupt_satellite_to_ICS(merger_centralgal, p, ctx.galaxies);
                        } else {
                            // PHYSICS MODULE: Mergers - satellite merges with central galaxy
                            // REFACTORING NOTE: Will become part of the merger module in Phase 2
                            double time = run_params->simulation.Age[ctx.galaxies[p].SnapNum] - (step + 0.5) * (deltaT / STEPS);
                            deal_with_galaxy_merger(p, merger_centralgal, ctx.centralgal, time, deltaT / STEPS, ctx.halo_nr, step, ctx.galaxies, run_params);
                        }
                    }
                }
            }
        }
    } // Go on to the next STEPS substep


    // Extra miscellaneous stuff before finishing this halo
    ctx.galaxies[ctx.centralgal].TotalSatelliteBaryons = 0.0;
    const double deltaT = run_params->simulation.Age[ctx.galaxies[0].SnapNum] - ctx.halo_age;
    const double inv_deltaT = 1.0/deltaT;

    for(int p = 0; p < ctx.ngal; p++) {
        // Don't bother with galaxies that have already merged
        if(ctx.galaxies[p].mergeType > 0) {
            continue;
        }

        ctx.galaxies[p].Cooling *= inv_deltaT;
        ctx.galaxies[p].Heating *= inv_deltaT;
        ctx.galaxies[p].OutflowRate *= inv_deltaT;

        if(p != ctx.centralgal) {
            ctx.galaxies[ctx.centralgal].TotalSatelliteBaryons +=
                (ctx.galaxies[p].StellarMass + ctx.galaxies[p].BlackHoleMass + 
                 ctx.galaxies[p].ColdGas + ctx.galaxies[p].HotGas);
        }
    }

    // Attach final galaxy list to halo
    for(int p = 0, currenthalo = -1; p < ctx.ngal; p++) {
        if(ctx.galaxies[p].HaloNr != currenthalo) {
            currenthalo = ctx.galaxies[p].HaloNr;
            haloaux[currenthalo].FirstGalaxy = *numgals;
            haloaux[currenthalo].NGalaxies = 0;
        }

        // Merged galaxies won't be output. So go back through its history and find it
        // in the previous timestep. Then copy the current merger info there.
        int offset = 0;
        int i = p-1;
        while(i >= 0) {
            if(ctx.galaxies[i].mergeType > 0) {
                if(ctx.galaxies[p].mergeIntoID > ctx.galaxies[i].mergeIntoID) {
                    offset++;  // these galaxies won't be kept so offset mergeIntoID below
                }
            }
            i--;
        }

        i = -1;
        if(ctx.galaxies[p].mergeType > 0) {
            i = haloaux[currenthalo].FirstGalaxy - 1;
            while(i >= 0) {
                if(halogal[i].GalaxyNr == ctx.galaxies[p].GalaxyNr) {
                    break;
                }
                i--;
            }

            if(i < 0) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to find galaxy in halogal array: i=%d should be >=0", i);
                return EXIT_FAILURE;
            }

            halogal[i].mergeType = ctx.galaxies[p].mergeType;
            halogal[i].mergeIntoID = ctx.galaxies[p].mergeIntoID - offset;
            halogal[i].mergeIntoSnapNum = halos[currenthalo].SnapNum;
        }

        if(ctx.galaxies[p].mergeType == 0) {
            /* realloc if needed */
            if(*numgals == (*maxgals - 1)) {
                *maxgals += 10000;

                *ptr_to_galaxies = myrealloc(*ptr_to_galaxies, *maxgals * sizeof(struct GALAXY));
                *ptr_to_halogal  = myrealloc(*ptr_to_halogal, *maxgals * sizeof(struct GALAXY));
                ctx.galaxies = *ptr_to_galaxies; // Update context pointer after realloc
                halogal = *ptr_to_halogal;
            }

            if(*numgals >= *maxgals) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, 
                            "Memory error: numgals = %d exceeds the number of galaxies allocated = %d",
                            *numgals, *maxgals);
                return INVALID_MEMORY_ACCESS_REQUESTED;
            }

            ctx.galaxies[p].SnapNum = halos[currenthalo].SnapNum;
            
            // Initialize extension data in the destination
            galaxy_extension_initialize(&halogal[*numgals]);
            
            // Copy galaxy including extensions to the permanent list
            halogal[*numgals] = ctx.galaxies[p];
            
            // Copy extension flags (data will be allocated on demand)
            galaxy_extension_copy(&halogal[*numgals], &ctx.galaxies[p]);
            
            (*numgals)++;
            haloaux[currenthalo].NGalaxies++;
        }
    }

    return EXIT_SUCCESS;
}
