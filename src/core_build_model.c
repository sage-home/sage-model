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
#include "core_mymalloc.h"
#include "core_save.h"
#include "core_utils.h"

#include "model_misc.h"
#include "model_mergers.h"
#include "model_infall.h"
#include "model_reincorporation.h"
#include "model_starformation_and_feedback.h"
#include "model_cooling_heating.h"


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
              return EXIT_FAILURE;
          }
          fofhalo = halos[fofhalo].NextHaloInFOFgroup;
      }

      int status = evolve_galaxies(halos[halonr].FirstHaloInFOFgroup, ngal, numgals, maxgals, halos, haloaux, ptr_to_galaxies, ptr_to_halogal, run_params);

      if(status != EXIT_SUCCESS) {
          return status;
      }
  }


  return EXIT_SUCCESS;
}
/* end of construct_galaxies*/


int join_galaxies_of_progenitors(const int halonr, const int ngalstart, int *galaxycounter, int *maxgals, struct halo_data *halos,
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

            galaxies[ngal] = halogal[haloaux[prog].FirstGalaxy + i];
            galaxies[ngal].HaloNr = halonr;

            galaxies[ngal].dT = -1.0;

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
                    galaxies[ngal].ReincorporatedGas = 0.0;

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
            XRETURN(centralgal == -1, -1,
                    "Error: Expected to find centralgal=-1. instead centralgal=%d\n", centralgal);

            centralgal = i;
        }
    }

    for(int i = ngalstart; i < ngal; i++) {
        galaxies[i].CentralGal = centralgal;
    }

    return ngal;

}
/* end of join_galaxies_of_progenitors */

int evolve_galaxies(const int halonr, const int ngal, int *numgals, int *maxgals, struct halo_data *halos,
                    struct halo_aux_data *haloaux, struct GALAXY **ptr_to_galaxies, struct GALAXY **ptr_to_halogal,
                    struct params *run_params)
{
    struct GALAXY *galaxies = *ptr_to_galaxies;
    struct GALAXY *halogal = *ptr_to_halogal;

    const int centralgal = galaxies[0].CentralGal;
    XRETURN(galaxies[centralgal].Type == 0 && galaxies[centralgal].HaloNr == halonr,
            EXIT_FAILURE,
            "Error: For centralgal, halonr = %d, %d.\n"
            "Expected to find galaxy.type = 0, and found type = %d.\n"
            "Expected to find galaxies[halonr] = %d and found halonr = %d\n",
            centralgal, halonr, galaxies[centralgal].Type,
            halonr,  galaxies[centralgal].HaloNr);

    /*
      MS: Note save halo_snapnum and galaxy_snapnum to local variables
          and replace all instances of snapnum to those local variables
     */

    const int halo_snapnum = halos[halonr].SnapNum;
    const double Zcurr = run_params->ZZ[halo_snapnum];
    const double halo_age = run_params->Age[halo_snapnum];
    const double infallingGas = infall_recipe(centralgal, ngal, Zcurr, galaxies, run_params);

    // ADAPTIVE TIMESTEP: Use dynamical time constraint
    const double Rvir = galaxies[centralgal].Rvir;
    const double Vvir = galaxies[centralgal].Vvir;
    const double deltaT = run_params->Age[galaxies[0].SnapNum] - halo_age;

    int nsteps;
    double actual_dt;

    // REGIME DIAGNOSTICS - Static counter to track total galaxies processed
    static int total_galaxies_processed = 0;
    static int cgm_regime_count = 0;
    static int hot_regime_count = 0; 

    static int *previous_regimes = NULL;
    static int max_stored_galaxies = 0;

    // Initialize regimes ONCE before main evolution loop
    if(run_params->CGMrecipeOn > 0) {
        // Allocate/reallocate regime storage if needed
        if(ngal > max_stored_galaxies) {
            if(previous_regimes != NULL) {
                free(previous_regimes);
            }
            previous_regimes = malloc(ngal * sizeof(int));
            max_stored_galaxies = ngal;
        }
        
        // Determine and cache regimes
        determine_and_cache_regime(ngal, galaxies, run_params);
        
        // Store initial regimes and apply initial cleanup
        for(int p = 0; p < ngal; p++) {
            previous_regimes[p] = galaxies[p].Regime;
            handle_regime_transition(p, galaxies, run_params);
        }
    }

    if (Vvir > 0.0 && Rvir > 0.0) {
        // Calculate dynamical time: t_dyn = 0.1 * R_vir / V_vir
        const double t_dyn = 0.1 * Rvir / Vvir;
        
        // Target: no time step should be longer than t_dyn/N
        // where N is the resolution factor (5, 10, 20, etc.)
        const int resolution_factor = run_params->DynamicalTimeResolutionFactor;  // 10 steps per dynamical time
        
        // Maximum allowed time step
        const double max_allowed_dt = t_dyn / resolution_factor;
        
        // Calculate required number of steps
        nsteps = (int)ceil(deltaT / max_allowed_dt);
        
        // Ensure we have at least the default number of steps
        if (nsteps < STEPS) {
            nsteps = STEPS;
        }
        
        // Cap at reasonable maximum (should naturally be ~10-30 for most cases)
        const int max_steps = 30;
        if (nsteps > max_steps) {
            nsteps = max_steps;
        }
        
        actual_dt = deltaT / nsteps;
        
    } else {
        // Fallback to default if Vvir or Rvir are invalid
        nsteps = STEPS;
        actual_dt = deltaT / STEPS;
    }


    // We integrate things forward by using a number of intervals equal to nsteps
    for(int step = 0; step < nsteps; step++) {

        // Check for regime changes ONLY
        if(run_params->CGMrecipeOn > 0) {
            determine_and_cache_regime(ngal, galaxies, run_params);
            
            // Apply transitions ONLY when regime actually changes
            for(int p = 0; p < ngal; p++) {
                if(galaxies[p].Regime != previous_regimes[p]) {
                    handle_regime_transition(p, galaxies, run_params);
                    previous_regimes[p] = galaxies[p].Regime;  // Update stored regime
                }
            }
        }

        // REGIME DIAGNOSTICS - Print every 50,000th galaxy
    for(int p = 0; p < ngal; p++) {
        if(galaxies[p].mergeType > 0) continue; // Skip merged galaxies
        
        total_galaxies_processed++;
        
        // Count regimes
        if(run_params->CGMrecipeOn > 0) {
            if(galaxies[p].Regime == 0) {
                cgm_regime_count++;
            } else {
                hot_regime_count++;
            }
        }

        // Check for regime violations
        if(run_params->CGMrecipeOn > 0) {
            int violation = 0;
            if(galaxies[p].Regime == 0 && galaxies[p].HotGas > 1e-10) {
                printf("WARNING: CGM galaxy %d has HotGas=%.2e\n", p, galaxies[p].HotGas);
                violation = 1;
            }
            if(galaxies[p].Regime == 1 && galaxies[p].CGMgas > 1e-10) {
                printf("WARNING: HOT galaxy %d has CGMgas=%.2e\n", p, galaxies[p].CGMgas);
                violation = 1;
            }
            
            if(violation) {
                double Tvir = 35.9 * galaxies[p].Vvir * galaxies[p].Vvir;
                double Tvir_threshold = 2.5e5;
                double rcool_ratio = calculate_rcool_to_rvir_ratio(p, galaxies, run_params);
                printf("  Galaxy %d: Regime=%s, r_cool/R_vir=%.3f, Tmax/Tvir=%.3f, Mvir=%.2e\n", 
                    p, (galaxies[p].Regime == 0) ? "CGM" : "HOT", 
                    rcool_ratio, Tvir_threshold / Tvir, galaxies[p].Mvir); 
            }
        }
        
        // Print diagnostics every 50,000 galaxies
        if(total_galaxies_processed % 100000 == 0) {
            printf("\n=== REGIME DIAGNOSTICS (Galaxy #%d) ===\n", total_galaxies_processed);
            
            if(run_params->CGMrecipeOn > 0) {
                double rcool_ratio = calculate_rcool_to_rvir_ratio(p, galaxies, run_params);
                double Tvir = 35.9 * galaxies[p].Vvir * galaxies[p].Vvir;
                double Tvir_threshold = 2.5e5;
                
                printf("Galaxy %d: ", p);
                printf("Regime=%s ", (galaxies[p].Regime == 0) ? "CGM" : "HOT");
                printf("r_cool/R_vir=%.3f ", rcool_ratio);
                printf("Tmax/Tvir=%.3f ", Tvir_threshold/Tvir);
                printf("Mvir=%.2e ", galaxies[p].Mvir);
                printf("Vvir=%.1f ", galaxies[p].Vvir);
                printf("\n");
                
                printf("  Gas masses: ");
                printf("CGMgas=%.2e ", galaxies[p].CGMgas);
                printf("HotGas=%.2e ", galaxies[p].HotGas);
                printf("ColdGas=%.2e ", galaxies[p].ColdGas);
                printf("Total=%.2e", galaxies[p].CGMgas + galaxies[p].HotGas + galaxies[p].ColdGas);
                printf("\n");
                
                // Summary statistics
                double cgm_fraction = (double)cgm_regime_count / (cgm_regime_count + hot_regime_count);
                printf("  Regime stats: ");
                printf("CGM=%d (%.1f%%) ", cgm_regime_count, cgm_fraction * 100.0);
                printf("HOT=%d (%.1f%%) ", hot_regime_count, (1.0 - cgm_fraction) * 100.0);
                printf("\n");
                
                // Check for potential issues
                if(galaxies[p].CGMgas > 0.0 && galaxies[p].HotGas > 0.0) {
                    printf("  WARNING: Galaxy has both CGMgas AND HotGas!\n");
                }
                
                if(galaxies[p].Regime == 0 && galaxies[p].HotGas > 1e-10) {
                    printf("  WARNING: CGM regime galaxy has HotGas=%.2e\n", galaxies[p].HotGas);
                }
                
                if(galaxies[p].Regime == 1 && galaxies[p].CGMgas > 1e-10) {
                    printf("  WARNING: HOT regime galaxy has CGMgas=%.2e\n", galaxies[p].CGMgas);
                }
                
            } else {
                printf("Galaxy %d: CGM recipe OFF, HotGas=%.2e\n", p, galaxies[p].HotGas);
            }
            
            printf("========================================\n\n");
        }
    }

        // Loop over all galaxies in the halo
        for(int p = 0; p < ngal; p++) {
            // Don't treat galaxies that have already merged
            if(galaxies[p].mergeType > 0) {
                continue;
            }

            // const double deltaT = run_params->Age[galaxies[p].SnapNum] - halo_age;
            const double time = run_params->Age[galaxies[p].SnapNum] - (step + 0.5) * actual_dt;

            if(galaxies[p].dT < 0.0) {
                galaxies[p].dT = deltaT;
            }

            // For the central galaxy only
            if(p == centralgal) {
                
                if(run_params->CGMrecipeOn > 0) {
                    double infall_amount = infallingGas * actual_dt / deltaT;
                    
                    if(galaxies[p].Regime == 0) {
                        // Low-mass regime: r_cool > R_vir, all gas goes to CGM
                        add_infall_to_cgm(centralgal, infall_amount, galaxies);
                    } else {
                        // High-mass regime: r_cool < R_vir, all gas goes to hot halo (pure)
                        add_infall_to_hot_pure(centralgal, infall_amount, galaxies);
                    }
                } else {
                    // Original SAGE behavior: all gas goes to hot halo (with CGM interaction for backwards compatibility)
                    add_infall_to_hot(centralgal, infallingGas * actual_dt / deltaT, galaxies);
                }

                if(run_params->ReIncorporationFactor > 0.0) {
                    reincorporate_gas(centralgal, actual_dt, galaxies, run_params);
                }
            } else {
                if(galaxies[p].Type == 1) {
                    if(run_params->CGMrecipeOn > 0) {
                        // CGM recipe: check regime-appropriate gas reservoir
                        if((galaxies[p].Regime == 0 && galaxies[p].CGMgas > 0.0) ||
                        (galaxies[p].Regime == 1 && galaxies[p].HotGas > 0.0)) {
                            strip_from_satellite(centralgal, p, Zcurr, galaxies, run_params);
                        }
                    } else {
                        // Original SAGE behavior: only check HotGas (backwards compatibility)
                        if(galaxies[p].HotGas > 0.0 && galaxies[p].CGMgas > 0.0) {
                            strip_from_satellite(centralgal, p, Zcurr, galaxies, run_params);
                        }
                    }
                }
            }

            // Determine the cooling gas given the halo properties
            double coolingGas;
            if(run_params->CGMrecipeOn == 1) {
                // CGM inflow model: called for all systems to accumulate CGM mass 
                // based on local conditions (only accumulates if CGM regime)
                cgm_inflow_model(p, actual_dt, galaxies, run_params);
                
                coolingGas = cooling_recipe_regime_aware(p, actual_dt, galaxies, run_params);
                cool_gas_onto_galaxy_regime_aware(p, coolingGas, galaxies, run_params);
            } else {
                coolingGas = cooling_recipe(p, actual_dt, galaxies, run_params);
                cool_gas_onto_galaxy(p, coolingGas, galaxies);
            }

            // stars form and then explode!
            starformation_and_feedback(p, centralgal, time, actual_dt, halonr, step, galaxies, run_params);
        }

        // check for satellite disruption and merger events
        for(int p = 0; p < ngal; p++) {

            // satellite galaxy!
            if((galaxies[p].Type == 1 || galaxies[p].Type == 2) && galaxies[p].mergeType == 0) {
                XRETURN(galaxies[p].MergTime < 999.0,
                        EXIT_FAILURE,
                        "Error: galaxies[%d].MergTime = %lf is too large! Should have been within the age of the Universe\n",
                        p, galaxies[p].MergTime);

                // const double deltaT = run_params->Age[galaxies[p].SnapNum] - halo_age;
                galaxies[p].MergTime -= actual_dt;

                // only consider mergers or disruption for halo-to-baryonic mass ratios below the threshold
                // or for satellites with no baryonic mass (they don't grow and will otherwise hang around forever)
                double currentMvir = galaxies[p].Mvir - galaxies[p].deltaMvir * (1.0 - ((double)step + 1.0) / (double)nsteps);
                double galaxyBaryons = galaxies[p].StellarMass + galaxies[p].ColdGas;
                if((galaxyBaryons == 0.0) || (galaxyBaryons > 0.0 && (currentMvir / galaxyBaryons <= run_params->ThresholdSatDisruption))) {

                    int merger_centralgal = galaxies[p].Type==1 ? centralgal:galaxies[p].CentralGal;

                    if(galaxies[merger_centralgal].mergeType > 0) {
                        merger_centralgal = galaxies[merger_centralgal].CentralGal;
                    }

                    galaxies[p].mergeIntoID = *numgals + merger_centralgal;  // position in output

                    if(isfinite(galaxies[p].MergTime)) {
                        // disruption has occured!
                        if(galaxies[p].MergTime > 0.0) {
                            disrupt_satellite_to_ICS(merger_centralgal, p, galaxies, run_params);
                        } else {
                            // a merger has occured!
                            double time = run_params->Age[galaxies[p].SnapNum] - (step + 0.5) * actual_dt;
                            deal_with_galaxy_merger(p, merger_centralgal, centralgal, time, actual_dt, halonr, step, galaxies, run_params);
                        }
                    }
                }
            }
        }
    } // Go on to the next STEPS substep

    if(run_params->CGMrecipeOn > 0) {
        final_regime_mass_enforcement(ngal, galaxies, run_params);
    }

    // Extra miscellaneous stuff before finishing this halo
    galaxies[centralgal].TotalSatelliteBaryons = 0.0;
    // const double deltaT = run_params->Age[galaxies[0].SnapNum] - halo_age;
    const double inv_deltaT = 1.0/deltaT;

    for(int p = 0; p < ngal; p++) {

        // Don't bother with galaxies that have already merged
        if(galaxies[p].mergeType > 0) {
            continue;
        }

        galaxies[p].Cooling *= inv_deltaT;
        galaxies[p].Heating *= inv_deltaT;
        galaxies[p].OutflowRate *= inv_deltaT;

        if(p != centralgal) {
            galaxies[centralgal].TotalSatelliteBaryons +=
                (galaxies[p].StellarMass + galaxies[p].BlackHoleMass + galaxies[p].ColdGas + galaxies[p].HotGas);
        }
    }

    // Attach final galaxy list to halo
    for(int p = 0, currenthalo = -1; p < ngal; p++) {
        if(galaxies[p].HaloNr != currenthalo) {
            currenthalo = galaxies[p].HaloNr;
            haloaux[currenthalo].FirstGalaxy = *numgals;
            haloaux[currenthalo].NGalaxies = 0;
        }

        // Merged galaxies won't be output. So go back through its history and find it
        // in the previous timestep. Then copy the current merger info there.
        int offset = 0;
        int i = p-1;
        while(i >= 0) {
            if(galaxies[i].mergeType > 0) {
                if(galaxies[p].mergeIntoID > galaxies[i].mergeIntoID) {
                    offset++;  // these galaxies won't be kept so offset mergeIntoID below
                }
            }

            i--;
        }

        i = -1;
        if(galaxies[p].mergeType > 0) {
            i = haloaux[currenthalo].FirstGalaxy - 1;
            while(i >= 0) {
                if(halogal[i].GalaxyNr == galaxies[p].GalaxyNr) {
                    break;
                }

                i--;
            }

            XRETURN(i >= 0, EXIT_FAILURE, "Error: This should not happen - i=%d should be >=0", i);

            halogal[i].mergeType = galaxies[p].mergeType;
            halogal[i].mergeIntoID = galaxies[p].mergeIntoID - offset;
            halogal[i].mergeIntoSnapNum = halos[currenthalo].SnapNum;
        }

        if(galaxies[p].mergeType == 0) {

            // Calculate and store r_cool/R_vir ratio for output
            if(run_params->CGMrecipeOn > 0) {
                galaxies[p].RcoolToRvir = calculate_rcool_to_rvir_ratio(p, galaxies, run_params);
            } else {
                galaxies[p].RcoolToRvir = -1.0;  // Indicate not calculated
            }
            /* realloc if needed */
            if(*numgals == (*maxgals - 1)) {
                *maxgals += 10000;

                *ptr_to_galaxies = myrealloc(*ptr_to_galaxies, *maxgals * sizeof(struct GALAXY));
                *ptr_to_halogal  = myrealloc(*ptr_to_halogal, *maxgals * sizeof(struct GALAXY));
                galaxies = *ptr_to_galaxies;
                halogal = *ptr_to_halogal;
            }

            XRETURN(*numgals < *maxgals, INVALID_MEMORY_ACCESS_REQUESTED,
                    "Error: numgals = %d exceeds the number of galaxies allocated = %d\n"
                    "This would result in invalid memory access...exiting\n",
                    *numgals, *maxgals);


            // FINAL REGIME CONSISTENCY CHECK BEFORE OUTPUT (UPDATED FOR VVIR)
            if(run_params->CGMrecipeOn > 0) {

                // double final_rcool_ratio = calculate_rcool_to_rvir_ratio(p, galaxies, run_params);
                // const double Vvir_threshold = 90.0;  // km/s
                // const double rcool_to_rvir = calculate_rcool_to_rvir_ratio(p, galaxies, run_params);
                const double Tvir = 35.9 * galaxies[p].Vvir * galaxies[p].Vvir; // in Kelvin
                const double Tvir_threshold = 2.5e5; // K, corresponds to Vvir ~52.7 km/s
                const double Tvir_to_Tmax_ratio = Tvir_threshold / Tvir;

                
                // The ENFORCED regime should be based on velocity threshold
                int velocity_based_regime = (Tvir_to_Tmax_ratio > 1.0) ? 0 : 1;

                // Check for violations against the ENFORCED regime (velocity-based)
                if(galaxies[p].Regime != velocity_based_regime) {
                    printf("ENFORCEMENT FAILURE: Galaxy %d cached_regime=%d expected_velocity_regime=%d Vvir=%.1f\n", 
                        p, galaxies[p].Regime, velocity_based_regime, galaxies[p].Vvir);
                }
                
                // Check for gas violations against the ENFORCED regime
                if(galaxies[p].Regime == 0 && galaxies[p].HotGas > 1e-10) {
                    printf("FINAL VIOLATION: CGM regime galaxy has HotGas=%.2e\n", galaxies[p].HotGas);
                }
                if(galaxies[p].Regime == 1 && galaxies[p].CGMgas > 1e-10) {
                    printf("FINAL VIOLATION: HOT regime galaxy has CGMgas=%.2e\n", galaxies[p].CGMgas);
                }
            }

            galaxies[p].SnapNum = halos[currenthalo].SnapNum;
            halogal[*numgals] = galaxies[p];
            (*numgals)++;
            haloaux[currenthalo].NGalaxies++;
        }
    }

    return EXIT_SUCCESS;
}
