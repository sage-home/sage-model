# Implementation Guide: Integrating Pipeline System into core_build_model.c

## Overview

This document outlines the steps needed to integrate the new pipeline system into `core_build_model.c`, specifically modifying the `evolve_galaxies` function to use the pipeline architecture.

## Step 1: Update Includes

Add the following includes to the top of the file:
```c
#include "core_pipeline_system.h"
#include "core_event_system.h"
```

## Step 2: Create Pipeline Step Execution Function

Add the following function before the `evolve_galaxies` function:

```c
/**
 * Custom pipeline step execution function for the standard physics modules
 */
static int execute_physics_step(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
)
{
    // Extract the evolution context from the pipeline context
    struct GALAXY *galaxies = context->galaxies;
    struct params *run_params = context->params;
    int ngal = context->ngal;
    int centralgal = context->centralgal;
    double time = context->time;
    double dt = context->dt;
    int halonr = context->halonr;
    int substep = context->step;
    
    // Process based on module type
    switch (step->type) {
        case MODULE_TYPE_INFALL:
            // PHYSICS MODULE: Infall - calculates gas falling into the halo
            if (substep == 0) {  // Only calculate infall once per timestep
                const double infallingGas = infall_recipe(centralgal, ngal, 1.0/(1.0 + run_params->simulation.ZZ[galaxies[0].SnapNum]), galaxies, run_params);
                
                // Store in user_data for later substeps
                *((double *)context->user_data) = infallingGas;
            }
            
            // Add infall to hot gas for the central galaxy
            add_infall_to_hot(centralgal, (*((double *)context->user_data)) / STEPS, galaxies);
            
            // Process reincorporation here since it's closely related to infall
            if (run_params->physics.ReIncorporationFactor > 0.0) {
                struct reincorporation_params_view reincorp_params;
                initialize_reincorporation_params_view(&reincorp_params, run_params);
                
                // Apply reincorporation to central galaxy
                reincorporate_gas(centralgal, dt, galaxies, &reincorp_params);
            }
            
            // Process stripping for satellite galaxies
            for (int p = 0; p < ngal; p++) {
                if (p != centralgal && galaxies[p].Type == 1 && galaxies[p].HotGas > 0.0) {
                    strip_from_satellite(centralgal, p, 1.0/(1.0 + run_params->simulation.ZZ[galaxies[0].SnapNum]), galaxies, run_params);
                }
            }
            break;
            
        case MODULE_TYPE_COOLING:
            // PHYSICS MODULE: Cooling - converts hot gas to cold gas
            {
                struct cooling_params_view cooling_params;
                initialize_cooling_params_view(&cooling_params, run_params);
                
                // Process cooling for all active galaxies
                for (int p = 0; p < ngal; p++) {
                    if (galaxies[p].mergeType > 0) {
                        continue;  // Skip merged galaxies
                    }
                    
                    double coolingGas = cooling_recipe(p, dt, galaxies, &cooling_params);
                    cool_gas_onto_galaxy(p, coolingGas, galaxies);
                }
            }
            break;
            
        case MODULE_TYPE_STAR_FORMATION:
            // PHYSICS MODULE: Star Formation and Feedback
            {
                struct star_formation_params_view sf_params;
                struct feedback_params_view fb_params;
                initialize_star_formation_params_view(&sf_params, run_params);
                initialize_feedback_params_view(&fb_params, run_params);
                
                // Process star formation for all active galaxies
                for (int p = 0; p < ngal; p++) {
                    if (galaxies[p].mergeType > 0) {
                        continue;  // Skip merged galaxies
                    }
                    
                    starformation_and_feedback(p, centralgal, time, dt, halonr, substep, galaxies, &sf_params, &fb_params);
                }
            }
            break;
            
        case MODULE_TYPE_MERGERS:
            // PHYSICS MODULE: Mergers and Disruption - handles satellite galaxy fate
            for (int p = 0; p < ngal; p++) {
                // Process only satellites
                if ((galaxies[p].Type == 1 || galaxies[p].Type == 2) && galaxies[p].mergeType == 0) {
                    if (galaxies[p].MergTime >= 999.0) {
                        LOG_ERROR("Galaxy %d has MergTime = %lf, which is too large! Should have been within the age of the Universe",
                                p, galaxies[p].MergTime);
                        return -1;
                    }
                    
                    // Update merger clock
                    galaxies[p].MergTime -= dt;
                    
                    // Only consider mergers or disruption for halo-to-baryonic mass ratios below the threshold
                    // or for satellites with no baryonic mass (they don't grow and will otherwise hang around forever)
                    double currentMvir = galaxies[p].Mvir - galaxies[p].deltaMvir * (1.0 - ((double)substep + 1.0) / (double)STEPS);
                    double galaxyBaryons = galaxies[p].StellarMass + galaxies[p].ColdGas;
                    
                    if ((galaxyBaryons == 0.0) || (galaxyBaryons > 0.0 && (currentMvir / galaxyBaryons <= run_params->physics.ThresholdSatDisruption))) {
                        int merger_centralgal = galaxies[p].Type==1 ? centralgal : galaxies[p].CentralGal;
                        
                        if (galaxies[merger_centralgal].mergeType > 0) {
                            merger_centralgal = galaxies[merger_centralgal].CentralGal;
                        }
                        
                        // Store target of merger/disruption
                        galaxies[p].mergeIntoID = *((int *)context->user_data) + merger_centralgal;  // numgals + merger_centralgal
                        
                        if (isfinite(galaxies[p].MergTime)) {
                            if (galaxies[p].MergTime > 0.0) {
                                // Disruption - satellite is disrupted and stars added to ICS
                                disrupt_satellite_to_ICS(merger_centralgal, p, galaxies);
                            } else {
                                // Mergers - satellite merges with central galaxy
                                deal_with_galaxy_merger(p, merger_centralgal, centralgal, time, dt, halonr, substep, galaxies, run_params);
                            }
                        }
                    }
                }
            }
            break;
            
        default:
            // Other module types not yet implemented
            LOG_WARNING("Unhandled module type %d in execute_physics_step", step->type);
            break;
    }
    
    return 0;
}
```

## Step 3: Modify evolve_galaxies Function

Completely replace the `evolve_galaxies` function with the following pipeline-based implementation:

```c
/**
 * This function evolves galaxies and applies all physics modules using the pipeline system.
 * 
 * The fixed sequence of physics processes has been replaced with a configurable pipeline
 * where modules can be dynamically loaded, replaced, or reordered at runtime.
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
        LOG_ERROR("Evolution context validation failed for halo %d", halonr);
        return EXIT_FAILURE;
    }
    
    LOG_DEBUG("Starting evolution for halo %d with %d galaxies", halonr, ngal);

    // Validate central galaxy
    if(galaxies[ctx.centralgal].Type != 0 || galaxies[ctx.centralgal].HaloNr != halonr) {
        LOG_ERROR("Invalid central galaxy: expected type=0, halonr=%d but found type=%d, halonr=%d",
                halonr, galaxies[ctx.centralgal].Type, galaxies[ctx.centralgal].HaloNr);
        return EXIT_FAILURE;
    }
    
    // Storage for infall gas calculation (to be shared between timesteps)
    double infallingGas = 0.0;
    
    // We integrate things forward by using a number of intervals equal to STEPS
    for(int step = 0; step < STEPS; step++) {
        // Calculate time step and current time for this substep
        const double deltaT = run_params->simulation.Age[galaxies[0].SnapNum] - ctx.halo_age;
        const double time = run_params->simulation.Age[galaxies[0].SnapNum] - (step + 0.5) * (deltaT / STEPS);
        
        // Initialize dT for galaxies if needed
        for(int p = 0; p < ctx.ngal; p++) {
            if(galaxies[p].dT < 0.0) {
                galaxies[p].dT = deltaT;
            }
        }
        
        // Get the pipeline to use (could be from configuration)
        struct module_pipeline *pipeline = pipeline_get_global();
        if (pipeline == NULL) {
            LOG_ERROR("No global pipeline available");
            return EXIT_FAILURE;
        }
        
        // For merger steps, we need to pass the numgals pointer
        void *user_data = (step == 0) ? (void*)&infallingGas : (void*)numgals;
        
        // Setup the pipeline context for this timestep
        struct pipeline_context pipeline_ctx;
        pipeline_context_init(&pipeline_ctx, 
                             run_params,             // Global parameters 
                             galaxies,               // Galaxy array
                             ngal,                   // Number of galaxies
                             ctx.centralgal,         // Central galaxy index
                             time,                   // Current time
                             deltaT / STEPS,         // Time step
                             halonr,                 // Current halo number
                             step,                   // Current substep
                             user_data);             // User data (for sharing between modules)
        
        // Execute the pipeline with our custom step executor
        int status = pipeline_execute_custom(pipeline, &pipeline_ctx, execute_physics_step);
        if (status != 0) {
            LOG_ERROR("Pipeline execution failed for halo %d, status = %d", halonr, status);
            return EXIT_FAILURE;
        }
        
        // Emit event that this timestep is complete
        if (event_system_is_initialized()) {
            struct {
                int halonr;
                int step;
                int ngal;
                double time;
                double dt;
            } event_data = {
                .halonr = halonr,
                .step = step,
                .ngal = ngal,
                .time = time,
                .dt = deltaT / STEPS
            };
            
            event_emit_custom("timestep_completed", &event_data, sizeof(event_data));
        }
    } // Go on to the next STEPS substep

    // Extra miscellaneous stuff before finishing this halo
    galaxies[ctx.centralgal].TotalSatelliteBaryons = 0.0;
    const double deltaT = run_params->simulation.Age[galaxies[0].SnapNum] - ctx.halo_age;
    const double inv_deltaT = 1.0/deltaT;

    for(int p = 0; p < ctx.ngal; p++) {
        // Don't bother with galaxies that have already merged
        if(galaxies[p].mergeType > 0) {
            continue;
        }

        galaxies[p].Cooling *= inv_deltaT;
        galaxies[p].Heating *= inv_deltaT;
        galaxies[p].OutflowRate *= inv_deltaT;

        if(p != ctx.centralgal) {
            galaxies[ctx.centralgal].TotalSatelliteBaryons +=
                (galaxies[p].StellarMass + galaxies[p].BlackHoleMass + 
                 galaxies[p].ColdGas + galaxies[p].HotGas);
        }
    }

    // Attach final galaxy list to halo
    for(int p = 0, currenthalo = -1; p < ctx.ngal; p++) {
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

            if(i < 0) {
                LOG_ERROR("Failed to find galaxy in halogal array: i=%d should be >=0", i);
                return EXIT_FAILURE;
            }

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
                galaxies = *ptr_to_galaxies; // Update pointer after realloc
                halogal = *ptr_to_halogal;
            }

            if(*numgals >= *maxgals) {
                LOG_ERROR("Memory error: numgals = %d exceeds the number of galaxies allocated = %d",
                            *numgals, *maxgals);
                return INVALID_MEMORY_ACCESS_REQUESTED;
            }

            galaxies[p].SnapNum = halos[currenthalo].SnapNum;
            
            // Initialize extension data in the destination
            galaxy_extension_initialize(&halogal[*numgals]);
            
            // Copy galaxy including extensions to the permanent list
            halogal[*numgals] = galaxies[p];
            
            // Copy extension flags (data will be allocated on demand)
            galaxy_extension_copy(&halogal[*numgals], &galaxies[p]);
            
            (*numgals)++;
            haloaux[currenthalo].NGalaxies++;
        }
    }

    return EXIT_SUCCESS;
}
```

## Step 4: Add Example Event Support

Once the implementation is complete, we can also add an event that other modules can listen for to know when a timestep is completed. This event emission is already included in the new code.

## Testing

After implementation, test with:

```bash
make tests
```

This will compile SAGE and run the test suite to ensure the refactored code still produces the correct scientific results.

## Next Steps

1. Document the new pipeline-based architecture
2. Create example modules that demonstrate how to hook into the pipeline
3. Create a configuration file showing how to customize the pipeline order
