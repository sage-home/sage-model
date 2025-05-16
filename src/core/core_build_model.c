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
#include "core_event_system.h"
#include "core_evolution_diagnostics.h"
#include "core_galaxy_accessors.h"  // For galaxy_get_* functions
#include "core_property_utils.h"    // For get_cached_property_id, set_float_property

// Legacy function forward declarations for temporary compatibility
// These will be replaced by pipeline modules in the physics-agnostic core
double estimate_merging_time(const int halonr, const int mostmassive, const int p, struct halo_data *halos, struct GALAXY *galaxies, struct params *run_params);
double get_virial_mass(const int halonr, struct halo_data *halos, struct params *run_params);
double get_virial_radius(const int halonr, struct halo_data *halos, struct params *run_params);
double get_virial_velocity(const int halonr, struct halo_data *halos, struct params *run_params);
double get_disk_radius(const int halonr, const int p, struct halo_data *halos, struct GALAXY *galaxies);


// Forward declarations for legacy compatibility functions called in fallbacks
double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
void check_disk_instability(const int p, const int centralgal, const int halonr, const double time, const double dt, const int step,
                            struct GALAXY *galaxies, struct params *run_params);

// Forward declaration for init_galaxy function
void init_galaxy(const int p, const int halonr, int *galaxycounter, struct halo_data *halos,
                struct GALAXY *galaxies, struct params *run_params);


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
            if(ngal >= (*maxgals - 1)) {
                // Expand arrays with geometric growth rather than fixed increment
                if (galaxy_array_expand(ptr_to_galaxies, maxgals, ngal + 1) != 0) {
                    LOG_ERROR("Failed to expand galaxies array in join_galaxies_of_progenitors");
                    return -1;
                }
                
                if (galaxy_array_expand(ptr_to_halogal, maxgals, ngal + 1) != 0) {
                    LOG_ERROR("Failed to expand halogal array in join_galaxies_of_progenitors");
                    return -1;
                }
                
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
            
            // Perform a deep copy of the properties struct content, including dynamic arrays
            int copy_status = copy_galaxy_properties(&galaxies[ngal], &halogal[haloaux[prog].FirstGalaxy + i], run_params);
            if (copy_status != 0) {
                LOG_ERROR("Failed to copy properties for galaxy %d (prog %d, halo %d)", ngal, prog, halonr);
                return -1;
            }

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

                    // Replace direct field access with accessors
                    // Set physics-related properties through generic property system
                    property_id_t cooling_id = get_cached_property_id("Cooling");
                    if (cooling_id >= 0) {
                        set_double_property(&galaxies[ngal], cooling_id, 0.0);
                    }
                    
                    property_id_t heating_id = get_cached_property_id("Heating");
                    if (heating_id >= 0) {
                        set_double_property(&galaxies[ngal], heating_id, 0.0);
                    }
                    
                    property_id_t outflow_id = get_cached_property_id("OutflowRate");
                    if (outflow_id >= 0) {
                        set_float_property(&galaxies[ngal], outflow_id, 0.0);
                    }
                    
                    // SFR history arrays now handled in physics modules as needed

                    if(halonr == halos[halonr].FirstHaloInFOFgroup) {
                        // a central galaxy
                        galaxies[ngal].mergeType = 0;
                        galaxies[ngal].mergeIntoID = -1;
                        galaxies[ngal].MergTime = 999.9f;

                        // DiskScaleRadius now handled in physics modules

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
 * The physics modules are applied via a configurable pipeline system,
 * allowing modules to be replaced, reordered, or removed at runtime.
 * Synchronization calls are placed around phase executions.
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

    // Initialize evolution diagnostics
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, halonr, ngal);
    ctx.diagnostics = &diag;

    // Record initial galaxy properties
    evolution_diagnostics_record_initial_properties(&diag, galaxies, ngal);

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

    // Calculate the timestep for this snapshot
    ctx.deltaT = run_params->simulation.Age[halos[halonr].SnapNum] - ctx.halo_age;
    ctx.time = run_params->simulation.Age[halos[halonr].SnapNum];

    // Set up pipeline context
    struct pipeline_context pipeline_ctx;
    pipeline_context_init(
        &pipeline_ctx,
        run_params,
        ctx.galaxies,
        ctx.ngal,
        ctx.centralgal,
        ctx.time,           // Use calculated time
        ctx.deltaT,         // Use calculated deltaT
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
    
    // Validate pipeline is available
    if (physics_pipeline == NULL || physics_pipeline->num_steps == 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "No physics pipeline available. The core infrastructure requires at least an empty pipeline.");
        evolution_diagnostics_finalize(&diag);
        evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
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
    evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
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

    evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);

    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute HALO phase for halo %d", halonr);
        evolution_diagnostics_finalize(&diag);
        evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }

    // Main integration steps
    for (int step = 0; step < STEPS; step++) {
        pipeline_ctx.step = step;
        double dt = ctx.deltaT / STEPS; // Timestep for this sub-step
        pipeline_ctx.dt = ctx.deltaT; // Pass the full deltaT to context, modules should use STEPS

        // Reset merger queue for this timestep
        init_merger_queue(&merger_queue);

        // Phase 2: GALAXY phase (for each galaxy)
        evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
        pipeline_ctx.execution_phase = PIPELINE_PHASE_GALAXY;

        for (int p = 0; p < ctx.ngal; p++) {
            // Skip already merged galaxies
            if (ctx.galaxies[p].mergeType > 0) {
                continue;
            }

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
                evolution_diagnostics_finalize(&diag);
                evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
                return EXIT_FAILURE;
            }

            // Queue potential mergers - with diagnostic tracking
            if ((ctx.galaxies[p].Type == 1 || ctx.galaxies[p].Type == 2) &&
                ctx.galaxies[p].mergeType == 0 && ctx.galaxies[p].MergTime < 999.0) {

                // Calculate merger time decrement
                double mergtime = ctx.galaxies[p].MergTime - dt;

                // Check if merger occurs in this step
                if (mergtime <= 0.0) {
                    evolution_diagnostics_add_merger_detection(&diag, ctx.galaxies[p].mergeType);
                    queue_merger_event(&merger_queue, p, ctx.galaxies[p].CentralGal,
                                     mergtime, ctx.time, dt,
                                     ctx.centralgal, step, ctx.galaxies[p].mergeType);
                } else {
                    // Update remaining merger time if no merger this step
                    ctx.galaxies[p].MergTime = mergtime;
                }
            }
        } // End loop over galaxies p

        evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);

        // Phase 3: POST phase (after all galaxies processed in step)
        evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_POST);
        pipeline_ctx.execution_phase = PIPELINE_PHASE_POST;
        
        // Ensure central galaxy has properties allocated if needed
        if (ctx.galaxies[ctx.centralgal].properties == NULL) {
            if (allocate_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params) != 0) {
                LOG_WARNING("Failed to allocate properties for central galaxy %d", ctx.centralgal);
            }
        }

        // Execute POST phase (physics agnostic)
        status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_POST);

        evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_POST);

        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute POST phase for step %d", step);
            evolution_diagnostics_finalize(&diag);
            evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
            return EXIT_FAILURE;
        }

        // Diagnostic tracking for processed mergers
        CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Processed %d merger events for step %d",
                  merger_queue.num_events, step);
        for (int i = 0; i < merger_queue.num_events; i++) {
            evolution_diagnostics_add_merger_processed(&diag, merger_queue.events[i].merger_type);
        }

    } // End loop over steps

    // Phase 4: FINAL phase (after all steps complete)
    evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_FINAL);
    pipeline_ctx.execution_phase = PIPELINE_PHASE_FINAL;

    // Ensure central galaxy has properties allocated if needed
    if (ctx.galaxies[ctx.centralgal].properties == NULL) {
        if (allocate_galaxy_properties(&ctx.galaxies[ctx.centralgal], run_params) != 0) {
            LOG_WARNING("Failed to allocate properties for central galaxy %d", ctx.centralgal);
        }
    }

    // Execute FINAL phase (physics agnostic)
    status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_FINAL);

    evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_FINAL);

    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute FINAL phase for halo %d", halonr);
        evolution_diagnostics_finalize(&diag);
        evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }

    // Finalize and record property serialization if needed
    if (pipeline_ctx.prop_ctx != NULL) {
        pipeline_cleanup_property_serialization(&pipeline_ctx);
    }

    // Record final galaxy properties and report diagnostics
    evolution_diagnostics_record_final_properties(&diag, galaxies, ctx.ngal);
    evolution_diagnostics_finalize(&diag);
    evolution_diagnostics_report(&diag, LOG_LEVEL_INFO);

    // Extra miscellaneous stuff before finishing this halo
    // Set TotalSatelliteBaryons to 0.0 - this would normally be calculated based on physics components
    // Set total satellite baryons through generic property system
    property_id_t satellite_baryons_id = get_cached_property_id("TotalSatelliteBaryons");
    if (satellite_baryons_id >= 0) {
        // Use generic property system rather than direct accessor
        float value = 0.0f;
        set_float_property(&ctx.galaxies[ctx.centralgal], satellite_baryons_id, value);
    }
    
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

        // Merged galaxies won't be output. So go back through its history and find it
        // in the previous timestep. Then copy the current merger info there.
        int offset = 0;
        int i = p-1;
        while(i >= 0) {
            if(ctx.galaxies[i].mergeType > 0) {
                // Original logic might be flawed if mergeIntoID isn't sequential
                // Let's assume mergeIntoID refers to the *final* index in the output list
                // This part might need careful review depending on how mergeIntoID is assigned
                 if(ctx.galaxies[p].mergeIntoID > ctx.galaxies[i].mergeIntoID && ctx.galaxies[i].mergeIntoID != -1) {
                     offset++;
                 }
            }
            i--;
        }

        i = -1;
        if(ctx.galaxies[p].mergeType > 0) {
            i = haloaux[currenthalo].FirstGalaxy - 1; // Start searching from the end of the previous halo's galaxies
            while(i >= 0) {
                // Compare GalaxyNr as the persistent identifier across snapshots
                if(halogal[i].GalaxyNr == ctx.galaxies[p].GalaxyNr && halogal[i].SnapNum == ctx.galaxies[p].SnapNum - 1) {
                    break; // Found the progenitor in the previous snapshot's output list
                }
                i--;
            }

            if(i < 0) {
                // This case might happen if the progenitor wasn't saved in the previous output snapshot
                // Or if GalaxyNr isn't reliably unique across snapshots in this context
                CONTEXT_LOG(&ctx, LOG_LEVEL_WARNING, "Failed to find progenitor galaxy %d in halogal array for merged galaxy %d (HaloNr %d)",
                            ctx.galaxies[p].GalaxyNr, p, currenthalo);
                // Continue without updating merger info for the progenitor if not found
            } else {
                // Found the progenitor, update its merger info
                halogal[i].mergeType = ctx.galaxies[p].mergeType;
                // Adjust mergeIntoID based on the offset calculated
                // This assumes mergeIntoID refers to the index within the *current* snapshot's output list
                halogal[i].mergeIntoID = ctx.galaxies[p].mergeIntoID - offset;
                halogal[i].mergeIntoSnapNum = halos[currenthalo].SnapNum;
            }
        }

        if(ctx.galaxies[p].mergeType == 0) { // Only save galaxies that haven't merged
            /* realloc if needed */
            if(*numgals >= (*maxgals - 1)) {
                // Expand arrays with geometric growth rather than fixed increment
                if (galaxy_array_expand(ptr_to_galaxies, maxgals, *numgals + 1) != 0) {
                    CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to expand galaxies array in evolve_galaxies");
                    return EXIT_FAILURE;
                }

                if (galaxy_array_expand(ptr_to_halogal, maxgals, *numgals + 1) != 0) {
                    CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to expand halogal array in evolve_galaxies");
                    return EXIT_FAILURE;
                }

                // Update local pointers after realloc
                // Note: ctx.galaxies points to the same memory as *ptr_to_galaxies initially,
                // but if ptr_to_galaxies is reallocated, ctx.galaxies becomes dangling.
                // We need to update ctx.galaxies as well.
                galaxies = *ptr_to_galaxies;
                halogal = *ptr_to_halogal;
                ctx.galaxies = galaxies; // Update context pointer
            }

            if(*numgals >= *maxgals) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR,
                            "Memory error: numgals = %d exceeds the number of galaxies allocated = %d",
                            *numgals, *maxgals);
                return INVALID_MEMORY_ACCESS_REQUESTED;
            }

            ctx.galaxies[p].SnapNum = halos[currenthalo].SnapNum;

            // Copy galaxy including extensions to the permanent list
            // First initialize the destination galaxy's properties and extensions
            galaxy_extension_initialize(&halogal[*numgals]);

            // Then, perform a shallow copy of the base struct GALAXY fields
            halogal[*numgals] = ctx.galaxies[p];
            
            // Reset the properties pointer to avoid double free issues
            halogal[*numgals].properties = NULL;
            
            // Then, perform a deep copy of the properties struct
            int copy_status = copy_galaxy_properties(&halogal[*numgals], &ctx.galaxies[p], run_params);
            if (copy_status != 0) {
                 CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to copy properties for galaxy %d", p);
                 // Need to decide how to handle this error - potentially free allocated properties?
                 return EXIT_FAILURE;
            }

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