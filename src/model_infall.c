#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_infall.h"
#include "model_misc.h"
#include "model_h2_formation.h"


double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, struct params *run_params)
{
    double tot_stellarMass, tot_BHMass, tot_coldMass, tot_hotMass, tot_ejected, tot_ICS;
    double tot_ejectedMetals, tot_ICSMetals;
    double infallingMass, reionization_modifier;

    // need to add up all the baryonic mass asociated with the full halo
    tot_stellarMass = tot_coldMass = tot_hotMass = tot_ejected = tot_BHMass = tot_ejectedMetals = tot_ICS = tot_ICSMetals = 0.0;

	// loop over all galaxies in the FoF-halo
    for(int i = 0; i < ngal; i++) {
        tot_stellarMass += galaxies[i].StellarMass;
        tot_BHMass += galaxies[i].BlackHoleMass;
        tot_coldMass += galaxies[i].ColdGas;
        tot_hotMass += galaxies[i].HotGas;
        tot_ejected += galaxies[i].CGMgas;
        tot_ejectedMetals += galaxies[i].MetalsCGMgas;
        tot_ICS += galaxies[i].ICS;
        tot_ICSMetals += galaxies[i].MetalsICS;


        if(i != centralgal) {
            // satellite ejected gas goes to central ejected reservior
            galaxies[i].CGMgas = galaxies[i].MetalsCGMgas = 0.0;

            // satellite ICS goes to central ICS
            galaxies[i].ICS = galaxies[i].MetalsICS = 0.0;
        }
    }

    // include reionization if necessary
    if(run_params->ReionizationOn) {
        reionization_modifier = do_reionization_enhanced(centralgal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

   
    // Calculate the total available baryon mass
    double total_baryon_mass = reionization_modifier * run_params->BaryonFrac * galaxies[centralgal].Mvir;
    
    // Calculate how much of that should go to the CGM (but don't update CGM yet)
    double cgm_fraction = 0.0;
    if (run_params->CGMBuildOn) {
        cgm_fraction = calculate_cgm(centralgal, Zcurr, galaxies, run_params);
        
        // NEW: Reduce CGM fraction for high-mass galaxies
        if (galaxies[centralgal].Mvir > 10.0) {  // In 10^10 Msun/h units
            double log_mvir = log10(galaxies[centralgal].Mvir * 1.0e10 / run_params->Hubble_h);
            
            if (log_mvir > 11.0) {
                // Reduce CGM fraction by up to 50% for massive galaxies
                double reduction = 0.9 * fmin(1.0, (log_mvir - 11.0) / 1.5);
                cgm_fraction *= (1.0 - reduction);
            }
        }
    }
    
    // Calculate infall by subtracting existing baryons and CGM fraction from total baryon budget
    double total_existing_baryons = tot_stellarMass + tot_coldMass + tot_hotMass + tot_ejected + tot_BHMass + tot_ICS;
    double cgm_mass = cgm_fraction * total_baryon_mass;
    
    // This is the actual amount available for infall after preventative effects
    infallingMass = total_baryon_mass - cgm_mass - total_existing_baryons;
    
    // Update the CGM reservoir with the calculated mass
    galaxies[centralgal].CGMgas = cgm_mass;

    // the central galaxy keeps all the ejected mass
    galaxies[centralgal].CGMgas = tot_ejected;
    galaxies[centralgal].MetalsCGMgas = tot_ejectedMetals;

    if(galaxies[centralgal].MetalsCGMgas > galaxies[centralgal].CGMgas) {
        galaxies[centralgal].MetalsCGMgas = galaxies[centralgal].CGMgas;
    }

    if(galaxies[centralgal].CGMgas < 0.0) {
        galaxies[centralgal].CGMgas = galaxies[centralgal].MetalsCGMgas = 0.0;
    }

    if(galaxies[centralgal].MetalsCGMgas < 0.0) {
        galaxies[centralgal].MetalsCGMgas = 0.0;
    }

    // the central galaxy keeps all the ICS (mostly for numerical convenience)
    galaxies[centralgal].ICS = tot_ICS;
    galaxies[centralgal].MetalsICS = tot_ICSMetals;

    if(galaxies[centralgal].MetalsICS > galaxies[centralgal].ICS) {
        galaxies[centralgal].MetalsICS = galaxies[centralgal].ICS;
    }

    if(galaxies[centralgal].ICS < 0.0) {
        galaxies[centralgal].ICS = galaxies[centralgal].MetalsICS = 0.0;
    }

    if(galaxies[centralgal].MetalsICS < 0.0) {
        galaxies[centralgal].MetalsICS = 0.0;
    }

    return infallingMass;
}



void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, struct params *run_params)
{
    double reionization_modifier;

    if(run_params->ReionizationOn) {
        // Use the enhanced reionization model
        reionization_modifier = do_reionization_enhanced(gal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

    double strippedGas = -1.0 *
        (reionization_modifier * run_params->BaryonFrac * galaxies[gal].Mvir - (galaxies[gal].StellarMass + galaxies[gal].ColdGas + galaxies[gal].HotGas + galaxies[gal].CGMgas + galaxies[gal].BlackHoleMass + galaxies[gal].ICS) ) / STEPS;
    // ( reionization_modifier * run_params->BaryonFrac * galaxies[gal].deltaMvir ) / STEPS;

    if(strippedGas > 0.0) {
        const double metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        double strippedGasMetals = strippedGas * metallicity;

        if(strippedGas > galaxies[gal].HotGas) strippedGas = galaxies[gal].HotGas;
        if(strippedGasMetals > galaxies[gal].MetalsHotGas) strippedGasMetals = galaxies[gal].MetalsHotGas;

        galaxies[gal].HotGas -= strippedGas;
        galaxies[gal].MetalsHotGas -= strippedGasMetals;

        galaxies[centralgal].HotGas += strippedGas;
        galaxies[centralgal].MetalsHotGas += strippedGas * metallicity;
    }

    // After stripping, update the H₂/HI components
    if (run_params->SFprescription >= 1) {
        if (galaxies[gal].ColdGas > 0) {
            update_gas_components(&galaxies[gal], run_params);
        }
        if (galaxies[centralgal].ColdGas > 0) {
            update_gas_components(&galaxies[centralgal], run_params);
        }
    }

}



double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, struct params *run_params)
{
    double f_of_a;

    // we employ the reionization recipie described in Gnedin (2000), however use the fitting
    // formulas given by Kravtsov et al (2004) Appendix B

    // here are two parameters that Kravtsov et al keep fixed, alpha gives the best fit to the Gnedin data
    const double alpha = 6.0;
    const double Tvir = 1e4;

    // calculate the filtering mass
    const double a = 1.0 / (1.0 + Zcurr);
    const double a_on_a0 = a / run_params->a0;
    const double a_on_ar = a / run_params->ar;

    if(a <= run_params->a0) {
        f_of_a = 3.0 * a / ((2.0 + alpha) * (5.0 + 2.0 * alpha)) * pow(a_on_a0, alpha);
    } else if((a > run_params->a0) && (a < run_params->ar)) {
        f_of_a =
            (3.0 / a) * run_params->a0 * run_params->a0 * (1.0 / (2.0 + alpha) - 2.0 * pow(a_on_a0, -0.5) / (5.0 + 2.0 * alpha)) +
            a * a / 10.0 - (run_params->a0 * run_params->a0 / 10.0) * (5.0 - 4.0 * pow(a_on_a0, -0.5));
    } else {
        f_of_a =
            (3.0 / a) * (run_params->a0 * run_params->a0 * (1.0 / (2.0 + alpha) - 2.0 * pow(a_on_a0, -0.5) / (5.0 + 2.0 * alpha)) +
                         (run_params->ar * run_params->ar / 10.0) * (5.0 - 4.0 * pow(a_on_ar, -0.5)) - (run_params->a0 * run_params->a0 / 10.0) * (5.0 -
                                                                                                   4.0 / sqrt(a_on_a0)
                                                                                                   ) +
                         a * run_params->ar / 3.0 - (run_params->ar * run_params->ar / 3.0) * (3.0 - 2.0 / sqrt(a_on_ar)));
    }

    // this is in units of 10^10Msun/h, note mu=0.59 and mu^-1.5 = 2.21
    const double Mjeans = 25.0 / sqrt(run_params->Omega) * 2.21;
    const double Mfiltering = Mjeans * pow(f_of_a, 1.5);

    // calculate the characteristic mass coresponding to a halo temperature of 10^4K
    const double Vchar = sqrt(Tvir / 36.0);
    const double omegaZ = run_params->Omega * (CUBE(1.0 + Zcurr) / (run_params->Omega * CUBE(1.0 + Zcurr) + run_params->OmegaLambda));
    const double xZ = omegaZ - 1.0;
    const double deltacritZ = 18.0 * M_PI * M_PI + 82.0 * xZ - 39.0 * xZ * xZ;
    const double HubbleZ = run_params->Hubble * sqrt(run_params->Omega * CUBE(1.0 + Zcurr) + run_params->OmegaLambda);

    const double Mchar = Vchar * Vchar * Vchar / (run_params->G * HubbleZ * sqrt(0.5 * deltacritZ));

    // we use the maximum of Mfiltering and Mchar
    const double mass_to_use = dmax(Mfiltering, Mchar);
    const double modifier = 1.0 / CUBE(1.0 + 0.26 * (mass_to_use / galaxies[gal].Mvir));

    return modifier;

}



void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies)
{
    float metallicity;

    // if the halo has lost mass, subtract baryons from the ejected mass first, then the hot gas
    if(infallingGas < 0.0 && galaxies[gal].CGMgas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].CGMgas, galaxies[gal].MetalsCGMgas);
        galaxies[gal].MetalsCGMgas += infallingGas*metallicity;
        if(galaxies[gal].MetalsCGMgas < 0.0) galaxies[gal].MetalsCGMgas = 0.0;

        galaxies[gal].CGMgas += infallingGas;
        if(galaxies[gal].CGMgas < 0.0) {
            infallingGas = galaxies[gal].CGMgas;
            galaxies[gal].CGMgas = galaxies[gal].MetalsCGMgas = 0.0;
        } else {
            infallingGas = 0.0;
        }
    }

    // if the halo has lost mass, subtract hot metals mass next, then the hot gas
    if(infallingGas < 0.0 && galaxies[gal].MetalsHotGas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        galaxies[gal].MetalsHotGas += infallingGas*metallicity;
        if(galaxies[gal].MetalsHotGas < 0.0) galaxies[gal].MetalsHotGas = 0.0;
    }

    // add (subtract) the ambient (enriched) infalling gas to the central galaxy hot component
    galaxies[gal].HotGas += infallingGas;
    if(galaxies[gal].HotGas < 0.0) galaxies[gal].HotGas = galaxies[gal].MetalsHotGas = 0.0;

}

/**
 * calculate_preventative_feedback - Calculate the reduction in baryonic accretion due to preventative feedback
 * 
 * This function implements the preventative feedback model where gas infall is reduced
 * for low-mass halos, especially at high redshift. The suppression is stronger for
 * halos with virial velocities below a characteristic scale.
 * 
 * @param gal: Index of the galaxy
 * @param z: Current redshift
 * @param galaxies: Array of galaxy structures
 * @param run_params: Simulation parameters
 * 
 * @return: Suppression factor between 0 and 1 (1 = no suppression)
 */
double calculate_cgm(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    // Skip calculation if preventative feedback is disabled
    if (run_params->CGMBuildOn == 0) {
        return 1.0;
    }
    
    // Get halo virial velocity in km/s
    double vvir = galaxies[gal].Vvir;
    if (vvir <= 0.0) return 1.0;
    
    // Calculate z-factor and critical velocity
    double z_factor = (z <= 1.0) ? 
        pow(1.0 + z, run_params->CGMBuildZdep) :
        pow(1.0 + 1.0, run_params->CGMBuildZdep) * pow((1.0 + z)/(1.0 + 1.0), 2.0);
    
    double v_crit = run_params->CGMBuildVcrit * z_factor;
    
    // Calculate minimum floor
    double min_floor = 0.3 / (1.0 + 0.5 * (z - 1.0));
    if (min_floor < 0.05) min_floor = 0.05;
    if (z <= 1.0) min_floor = 0.3;
    
    // Calculate suppression factor
    double f_suppress = min_floor + (1.0 - min_floor) / 
                       (1.0 + pow(v_crit/vvir, run_params->CGMBuildAlpha));
    
    // Apply additional suppression for intermediate-mass halos at high-z
    // if (z > 1.0 && vvir > 50.0 && vvir < 150.0) {
    //     double peak_suppress = 0.3 * (z - 1.0) / 2.0;
    //     if (peak_suppress > 0.5) peak_suppress = 0.5;
        
    //     double v_relative = (vvir - 100.0) / 50.0;
    //     double extra_suppress = peak_suppress * exp(-v_relative * v_relative);
        
    //     f_suppress *= (1.0 - extra_suppress);
    // }
    
    // Ensure bounds
    if (f_suppress < 0.05) f_suppress = 0.05;
    if (f_suppress > 1.0) f_suppress = 1.0;
    
    double cgm_fraction = 1.0 - f_suppress;
    
    return cgm_fraction;
}

/**
 * update_cgm_reservoir - Calculate and update the CGM gas content
 * 
 * This function calculates the CGM gas content based on the total baryonic budget
 * and current galaxy gas reservoirs.
 * 
 * @param centralgal: Index of the central galaxy
 * @param z: Current redshift
 * @param galaxies: Array of galaxy structures
 * @param run_params: Simulation parameters
 */
void update_cgm_reservoir(const int centralgal, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    // Calculate the total baryon mass expected for this halo
    double total_baryon_budget = run_params->BaryonFrac * galaxies[centralgal].Mvir;
    
    // Calculate the gas fraction that should be in the CGM
    double cgm_fraction = calculate_cgm(centralgal, z, galaxies, run_params);
    
    // Calculate the total mass in existing baryonic components (excluding ejected mass)
    double stellar_mass = galaxies[centralgal].StellarMass;
    double cold_gas = galaxies[centralgal].ColdGas;
    double hot_gas = galaxies[centralgal].HotGas;
    double bh_mass = galaxies[centralgal].BlackHoleMass;
    double ics_mass = galaxies[centralgal].ICS;
    
    double total_non_cgm_mass = stellar_mass + cold_gas + hot_gas + bh_mass + ics_mass;
    
    // Calculate how much gas should be in the CGM
    double target_cgm_mass = cgm_fraction * total_baryon_budget;
    
    // Update the CGM mass, ensuring we don't exceed the available baryons
    double new_cgm_mass = target_cgm_mass;
    if (new_cgm_mass + total_non_cgm_mass > total_baryon_budget) {
        new_cgm_mass = total_baryon_budget - total_non_cgm_mass;
    }
    if (new_cgm_mass < 0.0) new_cgm_mass = 0.0;
    
    galaxies[centralgal].CGMgas = new_cgm_mass;
 }

double do_reionization_enhanced(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    // Skip if reionization is turned off
    if (!run_params->ReionizationOn) {
        return 1.0;  // No suppression
    }
    
    double modifier = 1.0;
    
    // Choose model based on parameter
    switch(run_params->ReionizationModel) {
        case 0:  // Legacy model (Gnedin 2000, Kravtsov 2004)
            modifier = do_reionization(gal, z, galaxies, (struct params *)run_params);
            break;
            
        case 1:  // Enhanced Gnedin model
            modifier = enhanced_gnedin_model(gal, z, galaxies, run_params);
            break;
            
        case 2:  // Sobacchi & Mesinger model
            modifier = sobacchi_mesinger_model(gal, z, galaxies, run_params);
            break;
            
        case 3:  // Patchy reionization
            modifier = patchy_reionization_model(gal, z, galaxies, run_params);
            break;
            
        default:
            // Default to legacy model
            modifier = do_reionization(gal, z, galaxies, (struct params *)run_params);
    }
    
    // Apply early UV background effects if enabled
    if (run_params->UVBackgroundStrength > 0.0) {
        modifier = apply_early_uv_background(gal, z, modifier, galaxies, run_params);
    }
    
    // Ensure safety bounds
    if (modifier < 0.01) modifier = 0.01; // Never completely suppress
    if (modifier > 1.0) modifier = 1.0;   // Never enhance
    
    return modifier;
}

double calculate_filtering_mass(const double z, const struct params *run_params)
{
    // Implementation based on Naoz & Barkana (2007) and Hoeft et al. (2006)
    // Cited in Okamoto et al. (2008) and used in L-GALAXIES
    
    // Base filtering mass at z=zr (reionization)
    const double Mfilt_zr = run_params->FilteringMassNorm;  // [10^10 Msun/h]
    
    // Filtering mass evolution depends on redshift regime
    if (z >= run_params->Reionization_zr) {
        // Pre-reionization: minimal suppression
        return 0.1 * Mfilt_zr;
    } 
    else if (z >= run_params->Reionization_z0) {
        // During reionization transition
        double z_frac = (z - run_params->Reionization_z0) / 
                        (run_params->Reionization_zr - run_params->Reionization_z0);
        // Interpolate between values
        return Mfilt_zr * pow(10.0, -3.0 * z_frac);
    }
    else {
        // Post-reionization: power-law evolution
        double alpha = run_params->PostReionSlope;
        return Mfilt_zr * pow((1.0 + z)/(1.0 + run_params->Reionization_z0), alpha);
    }
}

double calculate_local_reionization_redshift(const int gal, struct GALAXY *galaxies, const struct params *run_params)
{
    // Based on Aubert et al. (2018) and Ocvirk et al. (2020) showing that
    // denser regions typically reionize earlier
    
    // Base reionization redshift from parameter
    double base_zreion = run_params->Reionization_zr;
    
    // Maximum variance allowed
    const double max_dz = 2.0 * run_params->LocalReionVariance;
    
    // Skip if no local variance is requested
    if (run_params->LocalReionVariance <= 0.0) {
        return base_zreion;
    }
    
    // Calculate environmental density proxy based on halo properties
    double density_factor = 0.0;
    
    if (galaxies[gal].Mvir > 0.0) {
        // For central galaxies, use direct proxies
        if (galaxies[gal].Type == 0) {
            // Normalize to rough average halo mass at that redshift
            // Follows Trac et al. (2008) scaling relation
            const double avg_mass = 1.0 * pow(1.0 + base_zreion, -2.5); // [10^10 Msun/h]
            
            // Calculate overdensity (capped to avoid extreme values)
            density_factor = log10(galaxies[gal].Mvir / avg_mass);
            if (density_factor > 1.0) density_factor = 1.0;
            if (density_factor < -1.0) density_factor = -1.0;
        }
        // For satellites, use host halo properties
        else if (galaxies[gal].Type > 0 && galaxies[gal].CentralGal >= 0) {
            // Use central galaxy's halo as proxy for local density
            int central_idx = galaxies[gal].CentralGal;
            const double avg_mass = 1.0 * pow(1.0 + base_zreion, -2.5); // [10^10 Msun/h]
            density_factor = log10(galaxies[central_idx].Mvir / avg_mass);
            if (density_factor > 1.0) density_factor = 1.0;
            if (density_factor < -1.0) density_factor = -1.0;
        }
    }
    
    // Modulate reionization redshift by density factor
    // Denser regions reionize earlier
    return base_zreion + max_dz * density_factor;
}

double enhanced_gnedin_model(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    // Enhanced version of the Gnedin (2000) model
    // Uses improved filtering mass calculation
    
    // Calculate the filtering mass at this redshift
    const double Mfilt = calculate_filtering_mass(z, run_params);
    
    // Convert to actual mass in model units
    const double filtering_mass = Mfilt; // Already in 10^10 Msun/h
    
    // Apply formula from Gnedin (2000) with α = 2 rather than original α = 1
    // This steeper slope matches recent simulation results better
    // See Okamoto et al. (2008) and Sobacchi & Mesinger (2013)
    const double modifier = 1.0 / pow(1.0 + pow(2.0 * filtering_mass / galaxies[gal].Mvir, 2.0), 1.5);
    
    return modifier;
}

double sobacchi_mesinger_model(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    // Implementation of Sobacchi & Mesinger (2013) reionization model
    // Based on radiative-hydrodynamic simulations
    
    // Parameter values from S&M13 paper
    const double Mc0 = run_params->FilteringMassNorm; // Base critical mass
    const double a_M = 2.0/3.0;  // Mass power-law slope
    const double zion = run_params->Reionization_zr; // Reionization redshift
    const double gamma_UV = 2.0;  // UVB spectral index (default = 2.0)
    
    // Calculate the critical mass as function of redshift & reionization redshift
    // Following equation 5 from Sobacchi & Mesinger (2013)
    double Mc = 0.0;
    
    if (z <= zion) {
        // After reionization
        double t_over_trec = pow(1.0 + z, -5.0/2.0) - pow(1.0 + zion, -5.0/2.0);
        t_over_trec *= pow(10.0/3.0, 1.5) * 100.0 * pow(0.3, -0.5);  // Convert to t/t_rec
        
        if (t_over_trec < 0.01) t_over_trec = 0.01;  // Avoid numerical issues
        
        // Calculate critical mass using the S&M13 formula
        const double J21 = 1.0;  // Normalized UVB intensity at z (can be improved)
        Mc = Mc0 * pow(J21, a_M) * pow(1.0 + z, 0.5 * a_M * (gamma_UV - 1.0)) *
             pow(1.0 - exp(-t_over_trec / 2.0), -a_M);
    } else {
        // Before reionization - minimal suppression
        Mc = 0.1 * Mc0;
    }
    
    // Calculate suppression factor
    // Using their equation 6
    double x = galaxies[gal].Mvir / Mc;
    double modifier = pow(1.0 + pow(3.0*x, -0.7), -2.5);
    
    // Bound the result
    if (modifier > 0.99) modifier = 0.99;
    if (modifier < 0.01) modifier = 0.01;
    
    return modifier;
}

double patchy_reionization_model(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    // Implementation of patchy reionization based on Choudhury et al. (2009) 
    // and recent large-scale simulations like CROC (Gnedin 2014)
    
    // Calculate local reionization redshift for this galaxy
    const double z_reion_local = calculate_local_reionization_redshift(gal, galaxies, run_params);
    
    // Width of reionization transition in redshift units
    const double dz = run_params->PatchyReionWidth;
    
    // Calculate suppression based on position in reionization history
    double modifier = 1.0;
    
    if (z <= z_reion_local - dz) {
        // Fully reionized region - apply the Sobacchi & Mesinger model
        modifier = sobacchi_mesinger_model(gal, z, galaxies, run_params);
    }
    else if (z >= z_reion_local + dz) {
        // Not yet reionized - minimal suppression
        modifier = 0.95;
    }
    else {
        // During transition - smooth interpolation
        double phase = (z_reion_local + dz - z) / (2.0 * dz);
        
        // Mix between full and minimal suppression
        double post_reion_modifier = sobacchi_mesinger_model(gal, z_reion_local - dz, galaxies, run_params);
        modifier = 0.95 - phase * (0.95 - post_reion_modifier);
    }
    
    return modifier;
}

double apply_early_uv_background(const int gal, const double z, double reion_modifier, 
                                struct GALAXY *galaxies, const struct params *run_params)
{
    // Implements early UV background effects before full reionization
    // Based on Finkelstein et al. (2019) and Yung et al. (2020)
    
    // Only apply at high redshift, before reionization
    if (z < run_params->Reionization_zr) {
        return reion_modifier;  // Use standard model after reionization
    }
    
    // Calculate early UV strength increasing toward reionization
    const double base_strength = run_params->UVBackgroundStrength;
    
    // Scale with redshift - stronger closer to reionization
    // Following Yung et al. (2020) model for pre-reionization feedback
    double uvb_strength = base_strength * 
                          (1.0 - (z - run_params->Reionization_zr) / 
                                (15.0 - run_params->Reionization_zr));
    
    // Bound the result
    if (uvb_strength < 0.0) uvb_strength = 0.0;
    if (uvb_strength > base_strength) uvb_strength = base_strength;
    
    // Small halos are more affected by early UV background
    // Sensitivity depends on circular velocity following Finkelstein et al. (2019)
    double mass_factor = 1.0;
    if (galaxies[gal].Vvir > 0.0) {
        if (galaxies[gal].Vvir < 30.0) {
            // Strong suppression for low-mass halos
            mass_factor = galaxies[gal].Vvir / 30.0;
            
            // Apply more suppression for very small halos
            // Based on Finlator et al. (2011) simulation results
            if (galaxies[gal].Vvir < 20.0) {
                mass_factor *= 0.8;
            }
        }
    }
    
    // Calculate combined modifier
    // Early UV background can only make suppression stronger
    double early_uvb_modifier = 1.0 - uvb_strength * (1.0 - mass_factor);
    
    return reion_modifier * early_uvb_modifier;
}