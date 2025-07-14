#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_starformation_and_feedback.h"
#include "model_misc.h"
#include "model_disk_instability.h"
#include "model_h2_formation.h"

double calculate_muratov_mass_loading(const int p, const double z, struct GALAXY *galaxies)
{
    // Get circular velocity in km/s
    double vc = galaxies[p].Vvir;  // Using virial velocity (already in km/s)
    
    // Add safety check to prevent division by zero
    if (vc <= 0.0) {
        return 0.0;  // Return zero mass loading if velocity is invalid
    }
    
    // Constants from Muratov et al. (2015) paper
    const double V_CRIT = 60.0;  // Critical velocity where the power law breaks
    const double NORM = 2.9;     // Normalization factor
    const double Z_EXP = 1.3;    // Redshift power-law exponent
    const double LOW_V_EXP = -3.2;  // Low velocity power-law exponent
    const double HIGH_V_EXP = -1.0; // High velocity power-law exponent
    
    // Calculate redshift term: (1+z)^1.3
    double z_term = pow(1.0 + z, Z_EXP);
    
    // Calculate velocity term with broken power law and smoother transition
    double v_term;
    if (vc < V_CRIT * 0.8) {
        v_term = pow(vc / V_CRIT, LOW_V_EXP);
    } else if (vc > V_CRIT * 1.2) {
        v_term = pow(vc / V_CRIT, HIGH_V_EXP);
    } else {
        // Interpolate between the two regimes for a smoother transition
        double frac = (vc - V_CRIT * 0.8) / (V_CRIT * 0.4);
        double v_term_low = pow(vc / V_CRIT, LOW_V_EXP);
        double v_term_high = pow(vc / V_CRIT, HIGH_V_EXP);
        v_term = v_term_low * (1.0 - frac) + v_term_high * frac;
    }
    
    double eta = NORM * z_term * v_term;
    
    // Cap the maximum mass-loading factor to prevent extreme feedback
    // if (eta > 50.0) {
    //     eta = 50.0;
    // }
    
    // Surface density suppression - FIRE2 shows that high gas surface density can suppress star formation
    const float re_pc = galaxies[p].DiskScaleRadius * 1.0e6 / 0.73;
    float disk_area_pc2 = 2.0 * M_PI * re_pc * re_pc;
    float gas_surface_density_center = (galaxies[p].ColdGas * 1.0e10 / 0.73) / disk_area_pc2; 

    if (gas_surface_density_center > 1000.0) {  // M☉/pc²
        float suppression = pow(gas_surface_density_center / 1000.0, -0.7);
        eta *= suppression;
    }

    // Safety check for the result
    if (!isfinite(eta)) {
        return 0.0;  // Return zero if result is NaN or infinity
    }

    return eta;
}

void starformation_and_feedback(const int p, const int centralgal, const double time, const double dt, const int halonr, const int step,
                                struct GALAXY *galaxies, const struct params *run_params)
{
    double reff, tdyn, strdot, stars, ejected_mass, metallicity;

    // Initialise variables
    strdot = 0.0;
    // Update the gas components to ensure H2 and HI are correctly calculated
    if (run_params->SFprescription >= 1) {
        update_gas_components(&galaxies[p], run_params);
    }

    // star formation recipes
    if(run_params->SFprescription == 0) {
        // we take the typical star forming region as 3.0*r_s using the Milky Way as a guide
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        // from Kauffmann (1996) eq7 x piR^2, (Vvir in km/s, reff in Mpc/h) in units of 10^10Msun/h
        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > cold_crit && tdyn > 0.0) {
            strdot = run_params->SfrEfficiency * (galaxies[p].ColdGas - cold_crit) / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription >= 1) {
        // H2-based star formation recipe
        // we take the typical star forming region as 3.0*r_s using the Milky Way as a guide
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        // Use H2 gas for star formation with a critical threshold
        const double h2_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > h2_crit && tdyn > 0.0) {
            strdot = run_params->SfrEfficiency * galaxies[p].H2_gas / tdyn;
        } else {
            strdot = 0.0;
        }
    } else {
        fprintf(stderr, "No star formation prescription selected!\n");
        ABORT(0);
    }

    stars = strdot * dt;
    // Remove stars from H2 gas
    if(stars > 0.0) {
        galaxies[p].H2_gas -= stars;
        
        // Recompute gas components after star formation
        update_gas_components(&galaxies[p], run_params);
    }
    if(stars < 0.0) {
        stars = 0.0;
    }

    // Choose between fixed feedback parameter or mass loading calculation
    double reheated_mass;
    if(run_params->SupernovaRecipeOn == 1) {
        if(run_params->MassLoadingOn) {
            // Use Muratov mass loading calculation
            double z = run_params->ZZ[galaxies[p].SnapNum];
            reheated_mass = calculate_muratov_mass_loading(p, z, galaxies) * stars;
            galaxies[p].MassLoading = reheated_mass / stars;  // Store mass loading factor in the galaxy structure
        } else {
            // Use traditional feedback parameter
            reheated_mass = run_params->FeedbackReheatingEpsilon * stars;
            galaxies[p].MassLoading = run_params->FeedbackReheatingEpsilon;  // Store fixed feedback parameter
        }
    } else {
        reheated_mass = 0.0;
    }

	XASSERT(reheated_mass >= 0.0, -1,
            "Error: Expected reheated gas-mass = %g to be >=0.0\n", reheated_mass);

    // cant use more cold gas than is available! so balance SF and feedback
    if((stars + reheated_mass) > galaxies[p].ColdGas && (stars + reheated_mass) > 0.0) {
        const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
        stars *= fac;
        reheated_mass *= fac;
    }

    // determine ejection
    if(run_params->SupernovaRecipeOn == 1) {
        if(galaxies[centralgal].Vvir > 0.0) {
            // For ejection calculation, use the appropriate reheating parameter
            double fb_reheat_for_ejection;
            if(run_params->MassLoadingOn) {
                double z = run_params->ZZ[galaxies[p].SnapNum];
                fb_reheat_for_ejection = calculate_muratov_mass_loading(p, z, galaxies);
            } else {
                fb_reheat_for_ejection = run_params->FeedbackReheatingEpsilon;
            }
            
            ejected_mass =
                (run_params->FeedbackEjectionEfficiency * (run_params->EtaSNcode * run_params->EnergySNcode) / (galaxies[centralgal].Vvir * galaxies[centralgal].Vvir) -
                 fb_reheat_for_ejection) * stars;
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
    update_from_star_formation(p, stars, metallicity, galaxies, run_params);

    // recompute the metallicity of the cold phase
    metallicity = get_metallicity(galaxies[p].ColdGas, galaxies[p].MetalsColdGas);

    // update from SN feedback
    update_from_feedback(p, centralgal, reheated_mass, ejected_mass, metallicity, galaxies, run_params);

    // check for disk instability
    if(run_params->DiskInstabilityOn) {
        check_disk_instability(p, centralgal, halonr, time, dt, step, galaxies, (struct params *) run_params);
    }

    // formation of new metals - instantaneous recycling approximation - only SNII
    if(galaxies[p].ColdGas > 1.0e-8) {
        const double FracZleaveDiskVal = run_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
        galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
        galaxies[centralgal].MetalsHotGas += run_params->Yield * FracZleaveDiskVal * stars;
        // galaxies[centralgal].MetalsEjectedMass += run_params->Yield * FracZleaveDiskVal * stars;
    } else {
        galaxies[centralgal].MetalsHotGas += run_params->Yield * stars;
        // galaxies[centralgal].MetalsEjectedMass += run_params->Yield * stars;
    }
}



void update_from_star_formation(const int p, const double stars, const double metallicity, struct GALAXY *galaxies, const struct params *run_params)
{
    const double RecycleFraction = run_params->RecycleFraction;
    // update gas and metals from star formation
    galaxies[p].ColdGas -= (1 - RecycleFraction) * stars;
    galaxies[p].MetalsColdGas -= metallicity * (1 - RecycleFraction) * stars;
    galaxies[p].StellarMass += (1 - RecycleFraction) * stars;
    galaxies[p].MetalsStellarMass += metallicity * (1 - RecycleFraction) * stars;
}



void update_from_feedback(const int p, const int centralgal, const double reheated_mass, double ejected_mass, const double metallicity,
                          struct GALAXY *galaxies, const struct params *run_params)
{

    XASSERT(reheated_mass >= 0.0, -1,
            "Error: For galaxy = %d (halonr = %d, centralgal = %d) with MostBoundID = %lld, the reheated mass = %g should be >=0.0",
            p, galaxies[p].HaloNr, centralgal, galaxies[p].MostBoundID, reheated_mass);
    XASSERT(reheated_mass <= galaxies[p].ColdGas, -1,
            "Error: Reheated mass = %g should be <= the coldgas mass of the galaxy = %g",
            reheated_mass, galaxies[p].ColdGas);

    XASSERT(reheated_mass >= 0.0, -1,
            "Error: For galaxy = %d (halonr = %d, centralgal = %d) with MostBoundID = %lld, the reheated mass = %g should be >=0.0",
            p, galaxies[p].HaloNr, centralgal, galaxies[p].MostBoundID, reheated_mass);
    XASSERT(reheated_mass <= galaxies[p].ColdGas, -1,
            "Error: Reheated mass = %g should be <= the coldgas mass of the galaxy = %g",
            reheated_mass, galaxies[p].ColdGas);

    if(run_params->SupernovaRecipeOn == 1) {
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
        galaxies[centralgal].CGMgas += ejected_mass;
        galaxies[centralgal].MetalsCGMgas += metallicityHot * ejected_mass;

        galaxies[p].OutflowRate += reheated_mass;
    }
}
