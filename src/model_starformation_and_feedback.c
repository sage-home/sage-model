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

// Fix for model_starformation_and_feedback.c

double calculate_mass_dependent_sf_efficiency(const struct GALAXY *galaxy, const double base_efficiency, const struct params *run_params)
{
    // If mass-dependent SF is disabled, return base efficiency
    if (run_params->MassDependentSFEnabled != 1) {
        return base_efficiency;
    }

    // Get galaxy virial mass in 10^10 M_sun/h (SAGE's internal units)
    double mvir = galaxy->Mvir;
    
    // Use parameters from run_params instead of hardcoded values
    const double pivot_mass = run_params->SFMassPivot;
    const double low_mass_slope = run_params->SFLowMassSlope;
    const double high_mass_slope = run_params->SFHighMassSlope;
    
    // Calculate mass-dependent scaling
    double scaling_factor = 1.0;
    
    if (mvir < pivot_mass) {
        // For low-mass galaxies: reduce SF efficiency
        // Scales as (M/M_pivot)^slope where slope is positive
        scaling_factor = pow(mvir / pivot_mass, low_mass_slope);
    } else {
        // For high-mass galaxies: enhance SF efficiency
        // Scales as (M/M_pivot)^slope where slope is negative
        scaling_factor = pow(mvir / pivot_mass, high_mass_slope);
    }
    
    // Constrain scaling factor to reasonable bounds
    if (scaling_factor < 0.1) scaling_factor = 0.1;  // Lower limit
    if (scaling_factor > 3.0) scaling_factor = 3.0;  // Upper limit
    
    return base_efficiency * scaling_factor;
}

void starformation_and_feedback(const int p, const int centralgal, const double time, const double dt, const int halonr, const int step,
                struct GALAXY *galaxies, const struct params *run_params)
{
    double reff, tdyn, strdot, stars, ejected_mass, metallicity;
    double reheated_mass = 0.0; // initialise

    // Get current redshift for this galaxy
    double z = run_params->ZZ[galaxies[p].SnapNum];
    
    // Calculate redshift-dependent parameters
    double base_sfr_eff = get_redshift_dependent_parameter(run_params->SfrEfficiency, 
        run_params->SFR_Alpha, z);

    double sfr_eff = base_sfr_eff;
    // NEW: Apply virial velocity enhancement if enabled
    if (run_params->VvirEnhancementOn == 1 && galaxies[p].Vvir > 0.0) {
        if (galaxies[p].Vvir > run_params->VvirThreshold) {
            // Power-law scaling with virial velocity using configurable parameters
            double velocity_ratio = galaxies[p].Vvir / run_params->VvirThreshold;
            double v_enhancement = pow(velocity_ratio, run_params->VvirEnhancementPower);
            
            // Apply maximum enhancement cap
            if (v_enhancement > 20.0) v_enhancement = 20.0;
            
            // Apply the enhancement
            sfr_eff *= v_enhancement;
        }
    }
    double fb_reheat = get_redshift_dependent_parameter(run_params->FeedbackReheatingEpsilon, 
                                                      run_params->Reheating_Alpha, z);
    double fb_eject = get_redshift_dependent_parameter(run_params->FeedbackEjectionEfficiency, 
                                                     run_params->Ejection_Alpha, z);

    // Star formation rate tracking
    galaxies[p].SfrDiskColdGas[step] = galaxies[p].ColdGas;
    galaxies[p].SfrDiskColdGasMetals[step] = galaxies[p].MetalsColdGas;

    // First, update the gas components to ensure H2 and HI are correctly calculated
    if (run_params->SFprescription == 1 || run_params->SFprescription == 2 || run_params->SFprescription == 3) {
        update_gas_components(&galaxies[p], run_params);
    }

    // Initialise variables
    strdot = 0.0;

    // star formation recipes
    if(run_params->SFprescription == 0) {
        // Original SAGE recipe (Kauffmann 1996)
        // we take the typical star forming region as 3.0*r_s using the Milky Way as a guide
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        // from Kauffmann (1996) eq7 x piR^2, (Vvir in km/s, reff in Mpc/h) in units of 10^10Msun/h
        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > cold_crit && tdyn > 0.0) {
            strdot = sfr_eff * (galaxies[p].ColdGas - cold_crit) / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription == 1) {
        // Blitz & Rosolowsky (2006)
        // H2-based star formation recipe
        // we take the typical star forming region as 3.0*r_s using the Milky Way as a guide
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        // Use H2 gas for star formation with a critical threshold
        const double h2_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].H2_gas > h2_crit && tdyn > 0.0) {
            strdot = sfr_eff * (galaxies[p].H2_gas - h2_crit) / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription == 2) {
        // Krumholz & Dekel (2012) model
        if(galaxies[p].H2_gas > 0.0 && galaxies[p].Vvir > 0.0) {
            // For K&D model, star formation efficiency depends directly on H2 mass
            // Calculate dynamical time
            reff = 3.0 * galaxies[p].DiskScaleRadius;
            tdyn = reff / galaxies[p].Vvir;
            
            if(tdyn > 0.0) {
                // Base star formation on H2 density following Bigiel et al. (2008)
                // SFR = 0.04 * Σ_H2/(10 M⊙pc⁻²) * Σ_H2
                double disk_area = M_PI * galaxies[p].DiskScaleRadius * galaxies[p].DiskScaleRadius;
                double h2_surface_density = 0.0;
                
                if(disk_area > 0.0) {
                    h2_surface_density = galaxies[p].H2_gas / disk_area;
                }
                
                // Scale efficiency with surface density (effectively implementing Bigiel's law)
                double local_efficiency = sfr_eff;
                if(h2_surface_density > 0.0) {
                    // Use lower efficiency at low surface densities
                    if(h2_surface_density < 10.0) {
                        local_efficiency *= 0.5 * h2_surface_density / 10.0;
                    }
                    // And higher efficiency at high surface densities (starburst regime)
                    else if(h2_surface_density > 100.0) {
                        local_efficiency *= 1.0 + 0.5 * log10(h2_surface_density/100.0);
                    }
                }
                
                // Star formation rate based on molecular gas and efficiency
                strdot = local_efficiency * galaxies[p].H2_gas / tdyn;
            }
        }
    } else if(run_params->SFprescription == 3) {
        if(galaxies[p].H2_gas > 0.0 && galaxies[p].Vvir > 0.0) {
            // Calculate dynamical time
            reff = 3.0 * galaxies[p].DiskScaleRadius;
            tdyn = reff / galaxies[p].Vvir;
            
            if(tdyn > 0.0) {
                // Mass-dependent efficiency scaling
                // Higher efficiency for more massive galaxies
                double mass_scaling = 1.0;
                
                if(galaxies[p].Mvir > 0.0) {
                    double log_mvir = log10(galaxies[p].Mvir * 1.0e10 / run_params->Hubble_h);
                    
                    // Increase efficiency above 10^10.5 Msun
                    if(log_mvir > 10.5) {
                        // Progressive increase in efficiency
                        mass_scaling = 1.0 + (log_mvir - 10.5) * 0.5;
                        
                        // Cap the scaling to prevent unrealistic values
                        if(mass_scaling > 3.0) mass_scaling = 3.0;
                    }
                }
                
                // Apply mass-dependent efficiency
                strdot = sfr_eff * mass_scaling * galaxies[p].H2_gas / tdyn;
            }
        }
    } else {
        fprintf(stderr, "No star formation prescription selected!\n");
        ABORT(0);
    }

    stars = strdot * dt;
    if(stars < 0.0) {
        stars = 0.0;
    }

    // Determine feedback reheating
    if (run_params->SupernovaRecipeOn == 1) {
        reheated_mass = fb_reheat * stars;
    }

    XASSERT(reheated_mass >= 0.0, -1,
            "Error: Expected reheated gas-mass = %g to be >=0.0\n", reheated_mass);

    // Constrain star formation and feedback based on available gas
    if (run_params->SFprescription == 0) {
        // Original recipe - balance feedback and SF based on total cold gas
        if((stars + reheated_mass) > galaxies[p].ColdGas && (stars + reheated_mass) > 0.0) {
            const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
            stars *= fac;
            reheated_mass *= fac;
        }
    } else {
        // H2-based recipes - ensure we don't use more gas than available
        if(stars > galaxies[p].H2_gas) {
            stars = galaxies[p].H2_gas;
        }
        
        if((stars + reheated_mass) > galaxies[p].ColdGas) {
            const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
            stars *= fac;
            reheated_mass *= fac;
        }
        
        // Remove stars from H2 gas
        if(stars > 0.0) {
            galaxies[p].H2_gas -= stars;
            
            // Recompute gas components after star formation
            update_gas_components(&galaxies[p], run_params);
        }
    }

    // determine ejection
    if(run_params->SupernovaRecipeOn == 1) {
        if(galaxies[centralgal].Vvir > 0.0) {
            ejected_mass =
                (fb_eject * (run_params->EtaSNcode * run_params->EnergySNcode) / 
                (galaxies[centralgal].Vvir * galaxies[centralgal].Vvir) -
                fb_reheat) * stars;
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
        const double FracZleaveDiskVal = run_params->FracZleaveDisk * 
                                     exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
        galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
        galaxies[centralgal].MetalsHotGas += run_params->Yield * FracZleaveDiskVal * stars;
    } else {
        galaxies[centralgal].MetalsHotGas += run_params->Yield * stars;
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
    // Ensure reheated mass doesn't exceed available cold gas
    double adjusted_reheated_mass = reheated_mass;
    double adjusted_ejected_mass = ejected_mass;

    XASSERT(reheated_mass >= 0.0, -1,
            "Error: For galaxy = %d (halonr = %d, centralgal = %d) with MostBoundID = %lld, the reheated mass = %g should be >=0.0",
            p, galaxies[p].HaloNr, centralgal, galaxies[p].MostBoundID, reheated_mass);

    // If reheated mass exceeds available cold gas, scale down
    if(reheated_mass > galaxies[p].ColdGas) {
        double scale_factor = galaxies[p].ColdGas / reheated_mass;
        adjusted_reheated_mass = galaxies[p].ColdGas;
        adjusted_ejected_mass *= scale_factor;
    }

    XASSERT(adjusted_reheated_mass <= galaxies[p].ColdGas, -1,
            "Error: Reheated mass = %g should be <= the coldgas mass of the galaxy = %g",
            adjusted_reheated_mass, galaxies[p].ColdGas);

    if(run_params->SupernovaRecipeOn == 1) {
        galaxies[p].ColdGas -= adjusted_reheated_mass;
        galaxies[p].MetalsColdGas -= metallicity * adjusted_reheated_mass;

        galaxies[centralgal].HotGas += adjusted_reheated_mass;
        galaxies[centralgal].MetalsHotGas += metallicity * adjusted_reheated_mass;

        if (run_params->SFprescription >= 1) {
            update_gas_components(&galaxies[p], run_params);
        }

        if(adjusted_ejected_mass > galaxies[centralgal].HotGas) {
            adjusted_ejected_mass = galaxies[centralgal].HotGas;
        }
        const double metallicityHot = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
        
        // Update the reservoirs
        galaxies[centralgal].HotGas -= adjusted_ejected_mass;
        galaxies[centralgal].MetalsHotGas -= metallicityHot * adjusted_ejected_mass;
        
        // Add to CGM
        galaxies[centralgal].CGMgas += adjusted_ejected_mass;
        galaxies[centralgal].CGMgas_enriched += adjusted_ejected_mass;  // NEW: Add this line (ejected gas is enriched)
        galaxies[centralgal].MetalsCGMgas += metallicityHot * adjusted_ejected_mass;

        galaxies[p].OutflowRate += adjusted_reheated_mass;
    }
}

/**
 * calculate_muratov_mass_loading - Calculate mass loading factor using Muratov et al. (2015) formulation
 * 
 * This implementation follows the actual broken power-law formulation from Muratov et al. (2015),
 * using their Equations 4 and 5, but with modifications to make it work better in SAGE.
 * 
 * @param p: Index of the galaxy in the galaxies array
 * @param z: Current redshift
 * @param galaxies: Array of GALAXY structures
 * @param run_params: Global parameters
 * 
 * @return: Mass loading factor (η)
 */
double calculate_muratov_mass_loading(const int p, const double z, struct GALAXY *galaxies, const struct params *run_params)
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
    
    // Calculate final mass loading factor with a scaling factor to tune down for SAGE
    double eta = NORM * z_term * v_term;
    
    // Cap the maximum mass-loading factor to prevent extreme feedback
    if (eta > 50.0) {
        eta = 50.0;
    }
    
    // Safety check for the result
    if (!isfinite(eta)) {
        return 0.0;  // Return zero if result is NaN or infinity
    }

    return eta;
}

/**
 * Modified version of starformation_and_feedback that uses the Muratov et al. (2015)
 * mass-loading prescription with further adjustments to work in SAGE.
 */
void starformation_and_feedback_with_muratov(const int p, const int centralgal, const double time, const double dt, const int halonr, const int step,
                                           struct GALAXY *galaxies, const struct params *run_params)
{
    double reff, tdyn, strdot, stars, ejected_mass, metallicity;
    double reheated_mass = 0.0; // initialize
    // double cold_gas_before = galaxies[p].ColdGas; // Save for debugging

    // Get current redshift for this galaxy
    double z = run_params->ZZ[galaxies[p].SnapNum];
    
    // Calculate redshift-dependent parameters
    double base_sfr_eff = get_redshift_dependent_parameter(run_params->SfrEfficiency, 
        run_params->SFR_Alpha, z);

    double sfr_eff = base_sfr_eff;

    // NEW: Apply virial velocity enhancement if enabled
    if (run_params->VvirEnhancementOn == 1 && galaxies[p].Vvir > 0.0) {
        if (galaxies[p].Vvir > run_params->VvirThreshold) {
            // Power-law scaling with virial velocity using configurable parameters
            double velocity_ratio = galaxies[p].Vvir / run_params->VvirThreshold;
            double v_enhancement = pow(velocity_ratio, run_params->VvirEnhancementPower);
            
            // Apply maximum enhancement cap
            if (v_enhancement > 20.0) v_enhancement = 20.0;
            
            // Apply the enhancement
            sfr_eff *= v_enhancement;
        }
    }

    // Star formation rate tracking
    galaxies[p].SfrDiskColdGas[step] = galaxies[p].ColdGas;
    galaxies[p].SfrDiskColdGasMetals[step] = galaxies[p].MetalsColdGas;

    // First, update the gas components to ensure H2 and HI are correctly calculated
    if (run_params->SFprescription >= 1) {
        update_gas_components(&galaxies[p], run_params);
    }

    // Initialize variables
    strdot = 0.0;

    // Star formation recipes - same as original function
    if(run_params->SFprescription == 0) {
        // Original SAGE recipe (Kauffmann 1996)
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > cold_crit && tdyn > 0.0) {
            strdot = sfr_eff * (galaxies[p].ColdGas - cold_crit) / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription == 1) {
        // H2-based star formation recipe
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        const double h2_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].H2_gas > h2_crit && tdyn > 0.0) {
            strdot = sfr_eff * (galaxies[p].H2_gas - h2_crit) / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription == 2 || run_params->SFprescription == 3) {
        // Other molecular gas models
        if(galaxies[p].H2_gas > 0.0 && galaxies[p].Vvir > 0.0) {
            reff = 3.0 * galaxies[p].DiskScaleRadius;
            tdyn = reff / galaxies[p].Vvir;
            
            if(tdyn > 0.0) {
                strdot = sfr_eff * galaxies[p].H2_gas / tdyn;
            }
        }
    } else {
        fprintf(stderr, "No star formation prescription selected!\n");
        ABORT(0);
    }

    stars = strdot * dt;
    if(stars < 0.0) {
        stars = 0.0;
    }

    // *** KEY DIFFERENCE: Modified Muratov mass loading ***
    if (run_params->SupernovaRecipeOn == 1) {
        // Use Muratov mass loading with a reduced factor
        reheated_mass = calculate_muratov_mass_loading(p, z, galaxies, run_params) * stars;
        
        // Ensure we don't reheat more than the available cold gas
        if (reheated_mass > galaxies[p].ColdGas) {
            reheated_mass = galaxies[p].ColdGas;
        }
    }

    XASSERT(reheated_mass >= 0.0, -1,
            "Error: Expected reheated gas-mass = %g to be >=0.0\n", reheated_mass);

    // Constrain star formation and feedback based on available gas
    if (run_params->SFprescription == 0) {
        // Original recipe - balance feedback and SF based on total cold gas
        if((stars + reheated_mass) > galaxies[p].ColdGas && (stars + reheated_mass) > 0.0) {
            const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
            stars *= fac;
            reheated_mass *= fac;
        }
    } else {
        // H2-based recipes - ensure we don't use more gas than available
        if(stars > galaxies[p].H2_gas) {
            stars = galaxies[p].H2_gas;
        }
        
        if((stars + reheated_mass) > galaxies[p].ColdGas) {
            const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
            stars *= fac;
            reheated_mass *= fac;
        }
        
        // Remove stars from H2 gas
        if(stars > 0.0) {
            galaxies[p].H2_gas -= stars;
            
            // Recompute gas components after star formation
            update_gas_components(&galaxies[p], run_params);
        }
    }

    // *** SIGNIFICANTLY REDUCE EJECTION TO MAINTAIN GAS SUPPLY ***
    if(run_params->SupernovaRecipeOn == 1) {
        if(galaxies[centralgal].Vvir > 0.0) {
            // Get redshift-dependent ejection efficiency with significant reduction
            double fb_eject = get_redshift_dependent_parameter(run_params->FeedbackEjectionEfficiency, 
                                                             run_params->Ejection_Alpha, z);
            
            // Modified ejection calculation
            if (stars > 0.0) {
                ejected_mass = (fb_eject * (run_params->EtaSNcode * run_params->EnergySNcode) / 
                (galaxies[centralgal].Vvir * galaxies[centralgal].Vvir) -
                reheated_mass) * stars;
                
            } else {
                ejected_mass = 0.0;
            }
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
        const double FracZleaveDiskVal = run_params->FracZleaveDisk * 
                                     exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
        galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
        galaxies[centralgal].MetalsHotGas += run_params->Yield * FracZleaveDiskVal * stars;
    } else {
        galaxies[centralgal].MetalsHotGas += run_params->Yield * stars;
    }
}