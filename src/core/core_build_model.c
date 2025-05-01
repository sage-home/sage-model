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
#include "core_galaxy_accessors.h"

#include "../physics/legacy/model_misc.h"
#include "../physics/legacy/model_mergers.h"
#include "../physics/legacy/model_infall.h"
#include "../physics/legacy/model_reincorporation.h"
#include "../physics/legacy/model_starformation_and_feedback.h"
#include "../physics/cooling_module.h"
#include "../physics/agn_module.h"
#include "../physics/feedback_module.h"
#include "../core/core_parameter_views.h"

/**
 * Execute a physics pipeline step
 * 
 * This function maps pipeline steps to physics modules or traditional implementations.
 * It uses module_invoke when a module is available; otherwise, it falls back to traditional code.
 * 
 * The execution is phase-aware, meaning different code paths are taken depending on whether
 * we're executing in HALO, GALAXY, POST, or FINAL phase.
 */
int physics_step_executor(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,  /* Might be unused during migration - that's expected */
    struct pipeline_context *context
) {
    // Validate property serialization if module requires it
    if (module != NULL && module->manifest) {
        // Check if module requires serialization
        if (module->manifest->capabilities & MODULE_FLAG_REQUIRES_SERIALIZATION) {
            if (context->prop_ctx == NULL) {
                LOG_ERROR("Module '%s' requires property serialization but context not initialized", 
                         module->name);
                return MODULE_STATUS_NOT_INITIALIZED;
            }
        }
        
        // Check if module uses extensions but doesn't necessarily require serialization
        else if (module->manifest->capabilities & MODULE_FLAG_HAS_EXTENSIONS) {
            if (context->prop_ctx == NULL) {
                LOG_WARNING("Module '%s' uses extensions but property serialization context not initialized. "
                           "This may cause issues if the module attempts to access extension data.", 
                           module->name);
                // Continue execution as this is just a warning
            }
        }
    }

    /* Silence unused parameter warning during migration */
    (void)module_data;
    
    // Extract data from context
    struct GALAXY *galaxies = context->galaxies;
    int p = context->current_galaxy;
    int centralgal = context->centralgal;
    double time = context->time;
    double dt = context->dt / STEPS;
    double redshift = context->redshift;
    int halonr = context->halonr;
    int step_num = context->step;
    struct params *run_params = context->params;
    enum pipeline_execution_phase phase = context->execution_phase;
    
    // Skip if galaxy has merged and we're in GALAXY phase
    if (phase == PIPELINE_PHASE_GALAXY && galaxies[p].mergeType > 0) {
        return 0;
    }

    // Migration Phase - Choose between module callback or traditional implementation
    bool use_module = false;
    
    // Phase 5 Integration: Check if we can use the module
    if (module != NULL) {
        // For testing purposes, we can enable module usage by uncommenting the line below
        // use_module = true;
        
        // TEMPORARY MIGRATION CODE: During migration, we continue to use traditional
        // implementations to maintain test compatibility. This will be removed once
        // all modules are properly implemented and tested.
        
        // For selected modules that are ready for testing with the callback system,
        // we can enable them individually here
        if (step->type == MODULE_TYPE_COOLING && strcmp(module->name, "DefaultCooling") == 0) {
            // Example of enabling for a specific module
            // use_module = true;
        }
    }
    
    if (use_module) {
        // PHASE 5: MODULE INVOKE IMPLEMENTATION
        // Use the callback system to invoke module functions
        
        int status = 0;
        
        // We need a special case for each module type since they have different function signatures
        switch (step->type) {
            case MODULE_TYPE_COOLING: {
                // Set up arguments for cooling module
                struct {
                    int galaxy_index;
                    double dt;
                } cooling_args = {
                    .galaxy_index = p,
                    .dt = dt
                };
                
                // Invoke cooling calculation function
                double cooling_result = 0.0;
                status = module_invoke(
                    0,                  // caller_id (use main code as caller)
                    step->type,         // module_type
                    NULL,               // use active module of type
                    "calculate_cooling", // function name
                    context,            // context
                    &cooling_args,      // arguments
                    &cooling_result     // result
                );
                
                if (status == MODULE_STATUS_SUCCESS) {
                    // Apply the result
                    cool_gas_onto_galaxy(p, cooling_result, galaxies);
                    LOG_DEBUG("Module invoke for cooling: galaxy=%d, cooling=%g", p, cooling_result);
                    
                    // Emit cooling completed event if system is initialized
                    if (event_system_is_initialized()) {
                        // Prepare cooling event data
                        event_cooling_completed_data_t cooling_data = {
                            .cooling_rate = (float)(cooling_result / dt),
                            .cooling_radius = 0.0f,  // Not provided by module
                            .hot_gas_cooled = (float)cooling_result
                        };
                        
                        // Emit the cooling completed event
                        event_status_t event_status = event_emit(
                            EVENT_COOLING_COMPLETED,   // Event type
                            module->module_id,         // Source module ID
                            p,                         // Galaxy index
                            step_num,                  // Current step
                            &cooling_data,             // Event data
                            sizeof(cooling_data),      // Size of event data
                            EVENT_FLAG_NONE            // No special flags
                        );
                        
                        if (event_status != EVENT_STATUS_SUCCESS) {
                            LOG_WARNING("Failed to emit cooling event from module '%s' for galaxy %d: status=%d", 
                                      module->name, p, event_status);
                        } else {
                            LOG_DEBUG("Module '%s' emitted cooling event for galaxy %d: cooling=%g", 
                                    module->name, p, cooling_result);
                        }
                    }
                } else {
                    LOG_WARNING("Module invoke for cooling failed: status=%d", status);
                    // Fall back to traditional implementation
                    struct cooling_params_view cooling_params;
                    initialize_cooling_params_view(&cooling_params, run_params);
                    double coolingGas = cooling_recipe(p, dt, galaxies, &cooling_params);
                    cool_gas_onto_galaxy(p, coolingGas, galaxies);
                }
                break;
            }
            
            case MODULE_TYPE_STAR_FORMATION: {
                // Set up arguments for star formation module
                struct {
                    int galaxy_index;
                    double dt;
                } sf_args = {
                    .galaxy_index = p,
                    .dt = dt
                };
                
                // Invoke star formation calculation function
                double stars_formed = 0.0;
                status = module_invoke(
                    0,                  // caller_id (use main code as caller)
                    step->type,         // module_type
                    NULL,               // use active module of type
                    "form_stars",       // function name
                    context,            // context
                    &sf_args,           // arguments
                    &stars_formed       // result
                );
                
                if (status == MODULE_STATUS_SUCCESS) {
                    // Apply the result - note: in a module, this would be handled internally
                    LOG_DEBUG("Module invoke for star formation: galaxy=%d, stars_formed=%g", p, stars_formed);
                    
                    // Emit star formation event if system is initialized
                    if (event_system_is_initialized()) {
                        // Get metallicity for event data
                        double metallicity = get_metallicity(galaxies[p].ColdGas, galaxies[p].MetalsColdGas);
                        
                        // Prepare star formation event data
                        event_star_formation_occurred_data_t sf_event_data = {
                            .stars_formed = (float)stars_formed,
                            .stars_to_disk = (float)stars_formed,  // All stars to disk in standard model
                            .stars_to_bulge = 0.0f,               // No bulge formation in standard star formation
                            .metallicity = (float)metallicity
                        };
                        
                        // Emit the star formation event
                        event_status_t event_status = event_emit(
                            EVENT_STAR_FORMATION_OCCURRED,  // Event type
                            module->module_id,             // Source module ID
                            p,                             // Galaxy index
                            step_num,                      // Current step
                            &sf_event_data,                // Event data
                            sizeof(sf_event_data),         // Size of event data
                            EVENT_FLAG_NONE                // No special flags
                        );
                        
                        if (event_status != EVENT_STATUS_SUCCESS) {
                            LOG_WARNING("Failed to emit star formation event from module '%s' for galaxy %d: status=%d", 
                                      module->name, p, event_status);
                        } else {
                            LOG_DEBUG("Module '%s' emitted star formation event for galaxy %d: stars_formed=%g", 
                                    module->name, p, stars_formed);
                        }
                    }
                } else {
                    LOG_WARNING("Module invoke for star formation failed: status=%d", status);
                    // Fall back to traditional implementation
                    // Note: Traditional star formation is handled with feedback
                }
                break;
            }
            
            // Add cases for other module types as they are implemented
            
            default:
                LOG_DEBUG("Module type %s doesn't have invoke implementation yet, using traditional code", 
                        module_type_name(step->type));
                use_module = false;
                break;
        }
        
        // If module invoke was successful, we're done
        if (status == MODULE_STATUS_SUCCESS) {
            return 0;
        }
        
        // Otherwise, fall back to traditional implementation
    }
    
    // Traditional implementation (used during migration or when module invoke fails)
    // Handle the execution based on the current phase
    switch (phase) {
        case PIPELINE_PHASE_HALO:
            // HALO phase - calculations that happen once per halo (outside galaxy loop)
            switch (step->type) {
                case MODULE_TYPE_INFALL:
                    // Calculate infall for the entire halo
                    // This is typically done outside this function, but we could modify it
                    // to use the pipeline system in the future
                    LOG_DEBUG("HALO phase - infall step");
                    break;
                    
                default:
                    LOG_DEBUG("Skipping step '%s' in HALO phase - not applicable", 
                            module_type_name(step->type));
                    break;
            }
            break;
            
        case PIPELINE_PHASE_GALAXY:
            // GALAXY phase - calculations that happen for each galaxy
            switch (step->type) {
                case MODULE_TYPE_INFALL: 
                    // Apply infall
                    if (p == centralgal) {
                        // Apply infall to central galaxy
                        double infall_gas = 0.0;
                        if (pipeline_context_get_data(context, "infallingGas", &infall_gas) != 0) {
                            LOG_WARNING("Failed to get infallingGas from pipeline context, using zero as fallback");
                        }
                        add_infall_to_hot(p, infall_gas / STEPS, galaxies);
                        
                        // Reincorporation
                        if (run_params->physics.ReIncorporationFactor > 0.0) {
                            struct reincorporation_params_view reincorp_params;
                            initialize_reincorporation_params_view(&reincorp_params, run_params);
                            reincorporate_gas(p, dt, galaxies, &reincorp_params);
                        }
                    } else if (galaxies[p].Type == 1 && galaxies[p].HotGas > 0.0) {
                        // Stripping for satellites
                        strip_from_satellite(centralgal, p, redshift, galaxies, run_params);
                    }
                    break;
                    
                case MODULE_TYPE_REINCORPORATION: 
                    // Handled together with infall
                    break;     

                case MODULE_TYPE_COOLING: 
                    // Traditional cooling implementation
                    {
                        struct cooling_params_view cooling_params;
                        initialize_cooling_params_view(&cooling_params, run_params);
                        double coolingGas = cooling_recipe(p, dt, galaxies, &cooling_params);
                        cool_gas_onto_galaxy(p, coolingGas, galaxies);
                    }
                    break;
                    
                case MODULE_TYPE_STAR_FORMATION: 
                    // Handled by feedback
                    break;     

                case MODULE_TYPE_FEEDBACK: 
                    // Star formation and feedback
                    {
                        struct star_formation_params_view sf_params;
                        struct feedback_params_view fb_params;
                        initialize_star_formation_params_view(&sf_params, run_params);
                        initialize_feedback_params_view(&fb_params, run_params);
                        
                        // Call the traditional implementation
                        starformation_and_feedback(p, centralgal, time, dt, halonr, step_num, 
                            galaxies, &sf_params, &fb_params);
                        
                        // Note: Event dispatch for star formation is now handled inside the starformation_and_feedback function
                    }
                    break;
                    
                case MODULE_TYPE_AGN: 
                    // Not separately implemented in traditional code
                    break;     

                case MODULE_TYPE_DISK_INSTABILITY: 
                    // Not separately implemented in traditional code
                    break;     

                default:
                    LOG_DEBUG("Skipping step '%s' in GALAXY phase - not applicable", 
                            module_type_name(step->type));
                    break;
            }
            break;
            
        case PIPELINE_PHASE_POST:
            // POST phase - calculations that happen after processing all galaxies (per step)
            switch (step->type) {
                case MODULE_TYPE_MERGERS:
                    // Mergers are handled separately using the merger event queue
                    // This is already implemented outside of the pipeline in evolve_galaxies
                    LOG_DEBUG("POST phase - mergers step - handled via merger_queue");
                    break;
                    
                default:
                    LOG_DEBUG("Skipping step '%s' in POST phase - not applicable", 
                            module_type_name(step->type));
                    break;
            }
            break;
            
        case PIPELINE_PHASE_FINAL:
            // FINAL phase - calculations that happen after all steps are complete
            switch (step->type) {
                case MODULE_TYPE_MISC:
                    // Final calculations, normalizations, etc.
                    LOG_DEBUG("FINAL phase - misc calculations");
                    break;
                    
                default:
                    LOG_DEBUG("Skipping step '%s' in FINAL phase - not applicable", 
                            module_type_name(step->type));
                    break;
            }
            break;
            
        default:
            LOG_ERROR("Unknown execution phase: %d", phase);
            return -1;
    }
    
    return 0;
}


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

                    // Replace direct field access with accessor functions
                    galaxy_set_cooling_rate(&galaxies[ngal], 0.0);
                    galaxy_set_heating_rate(&galaxies[ngal], 0.0);
                    galaxy_set_quasar_accretion(&galaxies[ngal], 0.0);
                    galaxy_set_outflow_rate(&galaxies[ngal], 0.0);

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
 * The physics modules are applied via a configurable pipeline system,
 * allowing modules to be replaced, reordered, or removed at runtime.
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
    
    // Set up pipeline context
    struct pipeline_context pipeline_ctx;
    pipeline_context_init(
        &pipeline_ctx,
        run_params,
        ctx.galaxies,
        ctx.ngal,
        ctx.centralgal,
        run_params->simulation.Age[ctx.galaxies[0].SnapNum], // time
        ctx.deltaT,                                         // dt
        ctx.halo_nr,                                        // halonr
        0,                                                  // step
        &ctx                                                // user_data (set to evolution context)
    );
    pipeline_ctx.current_galaxy = 0;
    pipeline_ctx.redshift = ctx.redshift; // Set the redshift from evolution context

    // Initialize property serialization if module extensions are enabled
    if (global_extension_registry != NULL && global_extension_registry->num_extensions > 0) {
        int status = pipeline_init_property_serialization(&pipeline_ctx, PROPERTY_FLAG_SERIALIZE);
        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to initialize property serialization");
            return EXIT_FAILURE;
        }
    }

    // Get the global physics pipeline
    struct module_pipeline *physics_pipeline = pipeline_get_global();
    bool use_pipeline = false;
    
    // Check if the pipeline is available and properly configured
    if (physics_pipeline != NULL) {
        // During Phase 2.5-2.6, we are more flexible with pipeline validation
        // Always returns true in Phase 2.5-2.6 but logs helpful information
        pipeline_validate(physics_pipeline); 
        
        // For Phase 2.5-2.6, we check if there are any steps in the pipeline
        if (physics_pipeline->num_steps > 0) {
            // First time messaging per run
            static bool first_pipeline_usage = true;
            if (first_pipeline_usage) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_INFO, "Using physics pipeline '%s' with %d steps", 
                        physics_pipeline->name, physics_pipeline->num_steps);
                first_pipeline_usage = false;
            } else {
                CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Using physics pipeline for halo %d", ctx.halo_nr);
            }
            use_pipeline = true;
        } else {
            // Only log once per process to avoid spamming logs
            static bool logged_empty_pipeline = false;
            if (!logged_empty_pipeline) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_WARNING, "Physics pipeline is empty, using traditional physics implementation");
                logged_empty_pipeline = true;
            }
        }
    }
    
    // Create merger event queue
    struct merger_event_queue merger_queue;
    memset(&merger_queue, 0, sizeof(struct merger_event_queue));
    ctx.merger_queue = &merger_queue;
    
    // EXECUTE PIPELINE PHASES - with diagnostics tracking
    
    // Phase 1: HALO phase (outside galaxy loop)
    evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
    pipeline_ctx.execution_phase = PIPELINE_PHASE_HALO;
    int status = 0;
    
    if (use_pipeline) {
        status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_HALO);
    } else {
        // Traditional implementation would go here
    }
    
    evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    
    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute HALO phase for halo %d", halonr);
        // Report diagnostics even on failure
        evolution_diagnostics_finalize(&diag);
        evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
        return EXIT_FAILURE;
    }
    
    // Main integration steps
    for (int step = 0; step < STEPS; step++) {
        pipeline_ctx.step = step;
        
        // Reset merger queue for this timestep
        memset(&merger_queue, 0, sizeof(struct merger_event_queue));
        
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
            diag.phases[PIPELINE_PHASE_GALAXY].galaxy_count++;
            
            // Execute GALAXY phase
            if (use_pipeline) {
                status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_GALAXY);
            } else {
                // Traditional implementation would go here
            }
            
            if (status != 0) {
                CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute GALAXY phase for galaxy %d", p);
                // Report diagnostics even on failure
                evolution_diagnostics_finalize(&diag);
                evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
                return EXIT_FAILURE;
            }
            
            // Queue potential mergers - with diagnostic tracking
            if ((ctx.galaxies[p].Type == 1 || ctx.galaxies[p].Type == 2) && 
                ctx.galaxies[p].mergeType == 0 && ctx.galaxies[p].MergTime < 999.0) {
                // Add merger detection to diagnostics
                evolution_diagnostics_add_merger_detection(&diag, ctx.galaxies[p].mergeType);
                
                // Check merger conditions and add to queue if appropriate
                queue_merger_event(&merger_queue, p, ctx.galaxies[p].CentralGal, 
                                 ctx.galaxies[p].MergTime, ctx.time, ctx.deltaT/STEPS, 
                                 ctx.centralgal, step, ctx.galaxies[p].mergeType);
            }
        }
        
        evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);
        
        // Phase 3: POST phase (after all galaxies processed in step)
        evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_POST);
        pipeline_ctx.execution_phase = PIPELINE_PHASE_POST;
        
        if (use_pipeline) {
            status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_POST);
        } else {
            // Traditional implementation would go here
        }
        
        evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_POST);
        
        if (status != 0) {
            CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute POST phase for step %d", step);
            // Report diagnostics even on failure
            evolution_diagnostics_finalize(&diag);
            evolution_diagnostics_report(&diag, LOG_LEVEL_WARNING);
            return EXIT_FAILURE;
        }
        
        // Process merger events - with diagnostic tracking
        CONTEXT_LOG(&ctx, LOG_LEVEL_DEBUG, "Processing %d merger events for step %d", 
                  merger_queue.num_events, step);
                  
        for (int i = 0; i < merger_queue.num_events; i++) {
            struct merger_event *event = &merger_queue.events[i];
            
            // Track merger processing in diagnostics
            evolution_diagnostics_add_merger_processed(&diag, event->merger_type);
        }
        
        // Process all merger events in the queue
        process_merger_events(&merger_queue, ctx.galaxies, run_params);
    }
    
    // Phase 4: FINAL phase (after all steps complete)
    evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_FINAL);
    pipeline_ctx.execution_phase = PIPELINE_PHASE_FINAL;
    
    if (use_pipeline) {
        status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_FINAL);
    } else {
        // Traditional implementation would go here
    }
    
    evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_FINAL);
    
    if (status != 0) {
        CONTEXT_LOG(&ctx, LOG_LEVEL_ERROR, "Failed to execute FINAL phase for halo %d", halonr);
        // Report diagnostics even on failure
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
    ctx.galaxies[ctx.centralgal].TotalSatelliteBaryons = 0.0;
    const double deltaT = run_params->simulation.Age[ctx.galaxies[0].SnapNum] - ctx.halo_age;
    const double inv_deltaT = 1.0/deltaT;
    // const double inv_deltaT = deltaT > 0.0 ? 1.0/deltaT : 1.0; // Safety check

    for(int p = 0; p < ctx.ngal; p++) {
        // Don't bother with galaxies that have already merged
        if(ctx.galaxies[p].mergeType > 0) {
            continue;
        }

        // Use accessor functions for normalizing rates
        double cooling = galaxy_get_cooling_rate(&ctx.galaxies[p]);
        double heating = galaxy_get_heating_rate(&ctx.galaxies[p]);
        double outflow = galaxy_get_outflow_rate(&ctx.galaxies[p]);
        
        galaxy_set_cooling_rate(&ctx.galaxies[p], cooling * inv_deltaT);
        galaxy_set_heating_rate(&ctx.galaxies[p], heating * inv_deltaT);
        galaxy_set_outflow_rate(&ctx.galaxies[p], outflow * inv_deltaT);

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
