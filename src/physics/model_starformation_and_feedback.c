#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../core/core_allvars.h"
#include "../core/core_parameter_views.h"
#include "../core/core_event_system.h"

#include "../physics/model_starformation_and_feedback.h"
#include "../physics/model_misc.h"
#include "../physics/model_disk_instability.h"

/*
 * Main star formation and feedback function
 * 
 * Implements star formation processes and supernova feedback.
 * Uses parameter views for improved modularity.
 */
void starformation_and_feedback(const int p, const int centralgal, const double time, const double dt, 
                               const int halonr, const int step, struct GALAXY *galaxies,
                               const struct star_formation_params_view *sf_params,
                               const struct feedback_params_view *fb_params)
{
    double reff, tdyn, strdot, stars, ejected_mass, metallicity;

    // Initialise variables
    strdot = 0.0;

    // star formation recipes
    if(sf_params->SFprescription == 0) {
        // we take the typical star forming region as 3.0*r_s using the Milky Way as a guide
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        // from Kauffmann (1996) eq7 x piR^2, (Vvir in km/s, reff in Mpc/h) in units of 10^10Msun/h
        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > cold_crit && tdyn > 0.0) {
            strdot = sf_params->SfrEfficiency * (galaxies[p].ColdGas - cold_crit) / tdyn;
        } else {
            strdot = 0.0;
        }
    } else {
        fprintf(stderr, "No star formation prescription selected!\n");
        ABORT(0);
    }

    stars = strdot * dt;
    if(stars < 0.0) {
        stars = 0.0;
    }

    double reheated_mass = (fb_params->SupernovaRecipeOn == 1) ? 
                         fb_params->FeedbackReheatingEpsilon * stars : 0.0;

	XASSERT(reheated_mass >= 0.0, -1,
            "Error: Expected reheated gas-mass = %g to be >=0.0\n", reheated_mass);

    // cant use more cold gas than is available! so balance SF and feedback
    if((stars + reheated_mass) > galaxies[p].ColdGas && (stars + reheated_mass) > 0.0) {
        const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
        stars *= fac;
        reheated_mass *= fac;
    }

    // determine ejection
    if(fb_params->SupernovaRecipeOn == 1) {
        if(galaxies[centralgal].Vvir > 0.0) {
            ejected_mass =
                (fb_params->FeedbackEjectionEfficiency * 
                (fb_params->EtaSNcode * fb_params->EnergySNcode) / 
                (galaxies[centralgal].Vvir * galaxies[centralgal].Vvir) -
                fb_params->FeedbackReheatingEpsilon) * stars;
        } else {
            ejected_mass = 0.0;
        }

        if(ejected_mass < 0.0) {
            ejected_mass = 0.0;
        }
    } else {
        ejected_mass = 0.0;
    }

    // update the star formation rate
    galaxies[p].SfrDisk[step] += stars / dt;
    galaxies[p].SfrDiskColdGas[step] = galaxies[p].ColdGas;
    galaxies[p].SfrDiskColdGasMetals[step] = galaxies[p].MetalsColdGas;
    
    // update for star formation
    metallicity = get_metallicity(galaxies[p].ColdGas, galaxies[p].MetalsColdGas);
    update_from_star_formation(p, stars, metallicity, galaxies, sf_params);

    // Emit star formation event if system is initialized - placed after metallicity is calculated
    if (event_system_is_initialized()) {
        // Prepare star formation event data
        event_star_formation_occurred_data_t sf_event_data = {
            .stars_formed = (float)stars,
            .stars_to_disk = (float)stars,  // All stars go to disk in standard model
            .stars_to_bulge = 0.0f,         // No bulge formation in standard star formation
            .metallicity = (float)metallicity
        };
        
        // Emit the star formation event
        event_status_t status = event_emit(
            EVENT_STAR_FORMATION_OCCURRED, // Event type
            0,                             // Source module ID (0 = traditional code)
            p,                             // Galaxy index
            step,                          // Current step
            &sf_event_data,                // Event data
            sizeof(sf_event_data),         // Size of event data
            EVENT_FLAG_NONE                // No special flags
        );
        
        if (status != EVENT_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to emit star formation event for galaxy %d: status=%d\n", 
                   p, status);
        }
    }

    // recompute the metallicity of the cold phase
    metallicity = get_metallicity(galaxies[p].ColdGas, galaxies[p].MetalsColdGas);

    // update from SN feedback
    update_from_feedback(p, centralgal, reheated_mass, ejected_mass, metallicity, galaxies, fb_params);

    // check for disk instability
    const struct params *params = sf_params->full_params; // Get full params for disk instability check
    if(params->physics.DiskInstabilityOn) {
        check_disk_instability(p, centralgal, halonr, time, dt, step, galaxies, (struct params *)params);
    }

    // formation of new metals - instantaneous recycling approximation - only SNII
    if(galaxies[p].ColdGas > 1.0e-8) {
        const double FracZleaveDiskVal = sf_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
        galaxies[p].MetalsColdGas += sf_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
        galaxies[centralgal].MetalsHotGas += sf_params->Yield * FracZleaveDiskVal * stars;
        // galaxies[centralgal].MetalsEjectedMass += sf_params->Yield * FracZleaveDiskVal * stars;
    } else {
        galaxies[centralgal].MetalsHotGas += sf_params->Yield * stars;
        // galaxies[centralgal].MetalsEjectedMass += sf_params->Yield * stars;
    }
}

/*
 * Compatibility wrapper for starformation_and_feedback
 * 
 * Provides backwards compatibility with the old interface while
 * using the new parameter view-based implementation internally.
 */
void starformation_and_feedback_compat(const int p, const int centralgal, const double time, 
                                      const double dt, const int halonr, const int step,
                                      struct GALAXY *galaxies, const struct params *run_params)
{
    struct star_formation_params_view sf_params;
    struct feedback_params_view fb_params;
    
    initialize_star_formation_params_view(&sf_params, run_params);
    initialize_feedback_params_view(&fb_params, run_params);
    
    starformation_and_feedback(p, centralgal, time, dt, halonr, step, 
                              galaxies, &sf_params, &fb_params);
}



/*
 * Update galaxy properties from star formation
 * 
 * Updates cold gas, stellar mass, and their metal contents
 * after star formation events.
 */
void update_from_star_formation(const int p, const double stars, const double metallicity, 
                               struct GALAXY *galaxies, 
                               const struct star_formation_params_view *sf_params)
{
    const double RecycleFraction = sf_params->RecycleFraction;
    // update gas and metals from star formation
    galaxies[p].ColdGas -= (1 - RecycleFraction) * stars;
    galaxies[p].MetalsColdGas -= metallicity * (1 - RecycleFraction) * stars;
    galaxies[p].StellarMass += (1 - RecycleFraction) * stars;
    galaxies[p].MetalsStellarMass += metallicity * (1 - RecycleFraction) * stars;
}

/*
 * Compatibility wrapper for update_from_star_formation
 * 
 * Provides backwards compatibility with the old interface while
 * using the new parameter view-based implementation internally.
 */
void update_from_star_formation_compat(const int p, const double stars, const double metallicity, 
                                      struct GALAXY *galaxies, const struct params *run_params)
{
    struct star_formation_params_view sf_params;
    initialize_star_formation_params_view(&sf_params, run_params);
    update_from_star_formation(p, stars, metallicity, galaxies, &sf_params);
}



/*
 * Update galaxy properties from supernova feedback
 * 
 * Updates cold gas, hot gas, ejected gas and their metal contents
 * based on supernova feedback processes.
 */
void update_from_feedback(const int p, const int centralgal, const double reheated_mass, 
                         double ejected_mass, const double metallicity,
                         struct GALAXY *galaxies, const struct feedback_params_view *fb_params)
{
    XASSERT(reheated_mass >= 0.0, -1,
            "Error: For galaxy = %d (halonr = %d, centralgal = %d) with MostBoundID = %lld, the reheated mass = %g should be >=0.0",
            p, galaxies[p].HaloNr, centralgal, galaxies[p].MostBoundID, reheated_mass);
    XASSERT(reheated_mass <= galaxies[p].ColdGas, -1,
            "Error: Reheated mass = %g should be <= the coldgas mass of the galaxy = %g",
            reheated_mass, galaxies[p].ColdGas);

    if(fb_params->SupernovaRecipeOn == 1) {
        galaxies[p].ColdGas -= reheated_mass;
        galaxies[p].MetalsColdGas -= metallicity * reheated_mass;

        galaxies[centralgal].HotGas += reheated_mass;
        galaxies[centralgal].MetalsHotGas += metallicity * reheated_mass;

        if(ejected_mass > galaxies[centralgal].HotGas) {
            ejected_mass = galaxies[centralgal].HotGas;
        }
        const double metallicityHot = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);

        galaxies[centralgal].HotGas -= ejected_mass;
        galaxies[centralgal].MetalsHotGas -= metallicityHot * ejected_mass;
        galaxies[centralgal].EjectedMass += ejected_mass;
        galaxies[centralgal].MetalsEjectedMass += metallicityHot * ejected_mass;

        galaxies[p].OutflowRate += reheated_mass;
        
        // Emit feedback event if system is initialized
        if (event_system_is_initialized()) {
            // Prepare feedback event data
            event_feedback_applied_data_t feedback_data = {
                .energy_injected = 0.0f,  // Energy not directly tracked in current implementation
                .mass_reheated = (float)reheated_mass,
                .metals_ejected = (float)(metallicityHot * ejected_mass)
            };
            
            // Emit the feedback event
            event_status_t status = event_emit(
                EVENT_FEEDBACK_APPLIED,    // Event type
                0,                         // Source module ID (0 = traditional code)
                p,                         // Galaxy index
                -1,                        // Step not available in this context
                &feedback_data,            // Event data
                sizeof(feedback_data),     // Size of event data
                EVENT_FLAG_NONE            // No special flags
            );
            
            if (status != EVENT_STATUS_SUCCESS) {
                fprintf(stderr, "Failed to emit feedback event for galaxy %d: status=%d\n", 
                       p, status);
            }
        }
    }
}

/*
 * Compatibility wrapper for update_from_feedback
 * 
 * Provides backwards compatibility with the old interface while
 * using the new parameter view-based implementation internally.
 */
void update_from_feedback_compat(const int p, const int centralgal, const double reheated_mass, 
                               double ejected_mass, const double metallicity,
                               struct GALAXY *galaxies, const struct params *run_params)
{
    struct feedback_params_view fb_params;
    initialize_feedback_params_view(&fb_params, run_params);
    update_from_feedback(p, centralgal, reheated_mass, ejected_mass, metallicity, 
                       galaxies, &fb_params);
}
