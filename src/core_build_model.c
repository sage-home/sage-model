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
    double infallingGas = infall_recipe(centralgal, ngal, Zcurr, galaxies, run_params);

    // ADAPTIVE TIMESTEP: Use dynamical time constraint
    const double Rvir = galaxies[centralgal].Rvir;
    const double Vvir = galaxies[centralgal].Vvir;
    const double deltaT = run_params->Age[galaxies[0].SnapNum] - halo_age;

    int nsteps;
    double actual_dt;

    // if (run_params->CGMrecipeOn > 0) {
    //     // Determine the CGM regime at the start of the timestep
    //     determine_and_store_regime(ngal, galaxies, run_params);
    // }
    

    if (Vvir > 0.0 && Rvir > 0.0) {
        // Calculate dynamical time: t_dyn = R_vir / V_vir
        const double t_dyn = Rvir / Vvir;
        
        // Target: no time step should be longer than t_dyn/N
        // where N is the resolution factor (5, 10, 20, etc.)
        const int resolution_factor = 10;  // 2 steps per dynamical time
        
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

        // Loop over all galaxies in the halo
        for(int p = 0; p < ngal; p++) {
            // Don't treat galaxies that have already merged
            if(galaxies[p].mergeType > 0) {
                continue;
            }

            // const double deltaT = run_params->Age[galaxies[p].SnapNum] - halo_age;
            const double time = run_params->Age[galaxies[p].SnapNum] - (step + 0.5) * (actual_dt);

            if(galaxies[p].dT < 0.0) {
                galaxies[p].dT = deltaT;
            }

            if (run_params->CGMrecipeOn > 0) {
                // Determine the CGM regime at the start of the timestep
                determine_and_store_regime(ngal, galaxies, run_params);
            }

            // // Handle CGM → HOT regime transitions with detailed logging
            // if (run_params->CGMrecipeOn > 0) {
            //     static long transition_debug_counter = 0;
                
            //     for(int p = 0; p < ngal; p++) {
            //         if(galaxies[p].mergeType > 0) continue;
                    
            //         transition_debug_counter++;
                    
            //         // Check for first-time transition to HOT regime
            //         if(galaxies[p].Regime == 1 && galaxies[p].HasTransitionedToHot == 0) {
            //             // This is the first time this galaxy entered HOT regime
            //             galaxies[p].HasTransitionedToHot = 1;
                        
            //             double cgm_mass_before = galaxies[p].CGMgas;
            //             double cgm_metals_before = galaxies[p].MetalsCGMgas;
            //             double hot_mass_before = galaxies[p].HotGas;   
            //             double hot_metals_before = galaxies[p].MetalsHotGas; 
                        
            //             if(galaxies[p].CGMgas > 0.0) {
            //                 galaxies[p].HotGas += galaxies[p].CGMgas;
            //                 galaxies[p].MetalsHotGas += galaxies[p].MetalsCGMgas;
            //                 galaxies[p].CGMgas = 0.0;
            //                 galaxies[p].MetalsCGMgas = 0.0;
                            
            //                 if(transition_debug_counter % 1 == 0) {
            //                     printf("\n=== CGM → HOT REGIME TRANSITION [Galaxy #%ld] ===\n", transition_debug_counter);
            //                     printf("GALAXY PROPERTIES:\n");
            //                     printf("  Mvir:         %.3e Msun\n", galaxies[p].Mvir);
            //                     printf("  Vvir:         %.2f km/s\n", galaxies[p].Vvir);
            //                     printf("  Redshift:     %.3f\n", run_params->ZZ[galaxies[p].SnapNum]);
            //                     printf("  StellarMass:  %.3e Msun\n", galaxies[p].StellarMass);
            //                     printf("  ColdGas:      %.3e Msun\n", galaxies[p].ColdGas);
                                
            //                     printf("\nREGIME TRANSITION:\n");
            //                     printf("  Previous:     CGM regime (0)\n");
            //                     printf("  Current:      HOT regime (1)\n");
                                
            //                     printf("\nGAS TRANSFER:\n");
            //                     printf("  CGM before:   %.3e Msun\n", cgm_mass_before);
            //                     printf("  CGM after:    %.3e Msun\n", galaxies[p].CGMgas);
            //                     printf("  HotGas before: %.3e Msun\n", hot_mass_before); 
            //                     printf("  HotGas after: %.3e Msun\n", galaxies[p].HotGas);
            //                     printf("  Metals CGM:   %.3e → %.3e Msun\n", cgm_metals_before, galaxies[p].MetalsCGMgas);
            //                     printf("  Metals Hot:   %.3e Msun\n", galaxies[p].MetalsHotGas);
                                
            //                     printf("\nMASS CONSERVATION CHECK:\n");
            //                     double total_before = cgm_mass_before + hot_mass_before;      // Fix this
            //                     double total_after = galaxies[p].CGMgas + galaxies[p].HotGas; // This stays
            //                     printf("  Total before: %.3e Msun\n", total_before);
            //                     printf("  Total after:  %.3e Msun\n", total_after);
            //                     printf("  Difference:   %.3e Msun", total_after - total_before);
            //                     if(fabs(total_after - total_before) > 1e-6) {
            //                         printf(" [WARNING: Mass not conserved!]");
            //                     }
            //                     printf("\n");
            //                     printf("================================================\n\n");
            //                 }
            //             } else {
            //                 if(transition_debug_counter % 1 == 0) {
            //                     printf("\n=== CGM → HOT REGIME TRANSITION [Galaxy #%ld] ===\n", transition_debug_counter);
            //                     printf("  No CGM gas to transfer (CGMgas = %.3e)\n", cgm_mass_before);
            //                     printf("  Vvir: %.2f km/s, Mvir: %.3e Msun\n", galaxies[p].Vvir, galaxies[p].Mvir);
            //                     printf("================================================\n\n");
            //                 }
            //             }
            //         }
            //     }
            // }

            static long infall_debug_counter = 0;

            // For the central galaxy only
            if(p == centralgal) {

                // // Apply dynamical timescale limitation for CGM regime
                // infall_debug_counter++;

                // // Store original values for diagnostics
                // double original_infallingGas = infallingGas;
                // double infall_efficiency = 1.0;
                // int reduction_applied = 0;  // Debug flag   

                // // Apply dynamical timescale limitation for CGM regime
                // if(run_params->CGMrecipeOn > 0 && galaxies[centralgal].Regime == 0) {
                //     const double t_dyn = (galaxies[centralgal].Vvir > 0.0) ? 
                //                         galaxies[centralgal].Rvir / galaxies[centralgal].Vvir : actual_dt;
                    
                //     // Simple scaling factor - adjust this value to control reduction
                //     double infall_scaling_factor = 13.0;  // Change this number as needed
                    
                //     infall_efficiency = (t_dyn > 0.0) ? (actual_dt / t_dyn) * infall_scaling_factor : 1.0;
                //     infall_efficiency = fmin(infall_efficiency, 1.0);  // Cap at 100%
                    
                //     // DEBUG: Print before reduction
                //     if(infall_debug_counter % 50000 == 0) {
                //         printf("DEBUG BEFORE: infallingGas=%.3e, efficiency=%.4f, scaling=%.1f\n", 
                //             infallingGas, infall_efficiency, infall_scaling_factor);
                //     }
                    
                //     // APPLY THE REDUCTION
                //     infallingGas = original_infallingGas * infall_efficiency;
                //     reduction_applied = 1;
                    
                //     // DEBUG: Print after reduction
                //     if(infall_debug_counter % 50000 == 0) {
                //         printf("DEBUG AFTER: infallingGas=%.3e, reduction_applied=%d\n", infallingGas, reduction_applied);
                //     }
                // }

                // // Debug output every 50,000 galaxies
                // if(infall_debug_counter % 50000 == 0) {
                //     printf("\n=== INFALL DIAGNOSTICS [Galaxy #%ld] ===\n", infall_debug_counter);
                //     printf("BASIC PROPERTIES:\n");
                //     printf("  Mvir:         %.3e Msun\n", galaxies[centralgal].Mvir);
                //     printf("  Vvir:         %.2f km/s\n", galaxies[centralgal].Vvir);
                //     printf("  Rvir:         %.3e Mpc/h\n", galaxies[centralgal].Rvir);
                //     printf("  Regime:       %d (%s)\n", galaxies[centralgal].Regime, 
                //         galaxies[centralgal].Regime == 0 ? "CGM" : "HOT");
                    
                //     printf("\nTIMESCALE ANALYSIS:\n");
                //     if(run_params->CGMrecipeOn > 0 && galaxies[centralgal].Regime == 0) {
                //         const double t_dyn = galaxies[centralgal].Rvir / galaxies[centralgal].Vvir;
                //         const double t_dyn_myr = t_dyn * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
                //         const double actual_dt_myr = actual_dt * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
                        
                //         printf("  t_dyn:        %.3e (%.2f Myr)\n", t_dyn, t_dyn_myr);
                //         printf("  actual_dt:    %.3e (%.2f Myr)\n", actual_dt, actual_dt_myr);
                //         printf("  dt/t_dyn:     %.4f\n", actual_dt / t_dyn);
                //         printf("  Infall eff:   %.4f\n", infall_efficiency);
                //         printf("  CGM REGIME:   REDUCTION %s\n", reduction_applied ? "APPLIED" : "FAILED");
                //     } else {
                //         printf("  Mode:         HOT regime or CGM off (no reduction)\n");
                //     }
                    
                //     printf("\nINFALL CALCULATION:\n");
                //     printf("  Original:     %.3e Msun/time_unit\n", original_infallingGas);
                //     printf("  Modified:     %.3e Msun/time_unit\n", infallingGas);
                    
                //     double actual_reduction = (original_infallingGas != 0.0) ? 
                //                             (1.0 - infallingGas/original_infallingGas) * 100.0 : 0.0;
                //     printf("  Reduction:    %.1f%% (%.3e Msun lost)\n", 
                //         actual_reduction, original_infallingGas - infallingGas);
                //     printf("  Expected red: %.1f%% (based on efficiency)\n", (1.0 - infall_efficiency) * 100.0);
                //     printf("  Final to disk: %.3e Msun (this timestep)\n", 
                //         infallingGas * actual_dt / deltaT);
                //     printf("========================================\n\n");
                // }

                add_infall_to_hot(centralgal, infallingGas * actual_dt / deltaT, galaxies, run_params);

                if(run_params->ReIncorporationFactor > 0.0) {
                    reincorporate_gas(centralgal, actual_dt, galaxies, run_params);
                }
            } else {
                if(galaxies[p].Type == 1 && galaxies[p].HotGas > 0.0) {
                    strip_from_satellite(centralgal, p, Zcurr, galaxies, run_params);
                }
            }

            // Determine the cooling gas given the halo properties
            double coolingGas;
            if(run_params->CGMrecipeOn == 1) {

                coolingGas = cooling_recipe_regime_aware(p, actual_dt, galaxies, run_params);
                cool_gas_onto_galaxy_regime_aware(p, coolingGas, galaxies);
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
                            double time = run_params->Age[galaxies[p].SnapNum] - (step + 0.5) * (actual_dt);
                            deal_with_galaxy_merger(p, merger_centralgal, centralgal, time, actual_dt, halonr, step, galaxies, run_params);
                        }
                    }
                }

            }
        }
    } // Go on to the next STEPS substep

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

            galaxies[p].SnapNum = halos[currenthalo].SnapNum;
            halogal[*numgals] = galaxies[p];
            (*numgals)++;
            haloaux[currenthalo].NGalaxies++;
        }
    }

    if(run_params->CGMrecipeOn > 0) {
        final_regime_check(ngal, galaxies, run_params);
    }

    return EXIT_SUCCESS;
}
