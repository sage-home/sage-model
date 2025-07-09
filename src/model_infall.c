#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_infall.h"
#include "model_misc.h"


double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params)
{
    double tot_stellarMass, tot_BHMass, tot_coldMass, tot_hotMass, tot_cgm, tot_ICS;
    double tot_cgmMetals, tot_ICSMetals;
    double infallingMass, reionization_modifier;

    // need to add up all the baryonic mass asociated with the full halo
    tot_stellarMass = tot_coldMass = tot_hotMass = tot_cgm = tot_BHMass = tot_cgmMetals = tot_ICS = tot_ICSMetals = 0.0;

	// loop over all galaxies in the FoF-halo
    for(int i = 0; i < ngal; i++) {
        tot_stellarMass += galaxies[i].StellarMass;
        tot_BHMass += galaxies[i].BlackHoleMass;
        tot_coldMass += galaxies[i].ColdGas;
        tot_hotMass += galaxies[i].HotGas;
        tot_cgm += galaxies[i].CGMgas;
        tot_cgmMetals += galaxies[i].MetalsCGMgas;
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

    infallingMass =
        reionization_modifier * run_params->BaryonFrac * galaxies[centralgal].Mvir - (tot_stellarMass + tot_coldMass + tot_hotMass + tot_cgm + tot_BHMass + tot_ICS);
    /* reionization_modifier * run_params->BaryonFrac * galaxies[centralgal].deltaMvir - newSatBaryons; */

    // the central galaxy keeps all the ejected mass
    galaxies[centralgal].CGMgas = tot_cgm;
    galaxies[centralgal].MetalsCGMgas = tot_cgmMetals;

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



void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params)
{
    double reionization_modifier;

    if(run_params->ReionizationOn) {
        /* reionization_modifier = do_reionization(gal, ZZ[halos[halonr].SnapNum]); */
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

}



double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params)
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

double get_cgm_infall_fraction(const int gal, struct GALAXY *galaxies, const struct params *run_params)
{
    const double z = run_params->ZZ[galaxies[gal].SnapNum];
    double base_fraction = run_params->CGMInfallFraction;
    double original_fraction = base_fraction;
    
    // Cold streams: MORE gas goes through CGM for small halos at high-z
    if (z > 0.0 && galaxies[gal].Mvir > 0.0) {
        double log_mass = log10(galaxies[gal].Mvir);
    
    if (log_mass < 2.5) {
        // Exponential boost with redshift
        double redshift_factor = exp(0.2 * (z - 2.0));  // Grows exponentially with z
        double mass_factor = 1.0 + 1.0 * (2.5 - log_mass);  // Very strong mass dependence
        
        double boost_factor = redshift_factor * mass_factor;
        base_fraction *= boost_factor;
    }
}
    
    // Keep reasonable bounds
    if (base_fraction > 0.95) base_fraction = 0.95;  // Always leave some direct pathway
    if (base_fraction < 0.0) base_fraction = 0.0;
    
    // Debug output
    static int debug_counter = 0;
    debug_counter++;
    if (debug_counter % 10000 == 0) {
        printf("DEBUG CGM Infall: z=%.2f, M=%.2e, original=%.3f, new=%.3f", 
               z, galaxies[gal].Mvir, original_fraction, base_fraction);
        
        if (z > 0.0 && galaxies[gal].Mvir > 0.0) {
            double log_mass = log10(galaxies[gal].Mvir);
            if (log_mass < 2.5) {
                printf(" [COLD STREAM BOOST]");
            } else {
                printf(" [HIGH-Z, MASSIVE HALO]");
            }
        } else {
            printf(" [LOW-Z OR NO MASS]");
        }
        printf("\n");
    }
    
    return base_fraction;
}

void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies, 
                       const struct params *run_params)
{
    float metallicity;

    // Handle negative infall (halo mass loss) - same as before
    if(infallingGas < 0.0 && galaxies[gal].CGMgas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].CGMgas, galaxies[gal].MetalsCGMgas);
        galaxies[gal].MetalsCGMgas += infallingGas*metallicity;
        if(galaxies[gal].MetalsCGMgas < 0.0) galaxies[gal].MetalsCGMgas = 0.0;

        galaxies[gal].CGMgas += infallingGas;
        // NEW: Also adjust CGM components
        if(galaxies[gal].CGMgas < 0.0) {
            infallingGas = galaxies[gal].CGMgas;
            galaxies[gal].CGMgas = galaxies[gal].MetalsCGMgas = 0.0;
            galaxies[gal].CGMgas_pristine = galaxies[gal].CGMgas_enriched = 0.0;  // NEW
        } else {
            infallingGas = 0.0;
        }
    }

    if(infallingGas < 0.0 && galaxies[gal].MetalsHotGas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        galaxies[gal].MetalsHotGas += infallingGas*metallicity;
        if(galaxies[gal].MetalsHotGas < 0.0) galaxies[gal].MetalsHotGas = 0.0;

        galaxies[gal].HotGas += infallingGas;
        if(galaxies[gal].HotGas < 0.0) {
            galaxies[gal].HotGas = galaxies[gal].MetalsHotGas = 0.0;
        }
    } else if(infallingGas > 0.0) {
        // NEW: Enhanced positive infall routing
        // Debug: Print before modification
        // if(gal == 0) {
        //     printf("DEBUG: Before infall - CGM total: %.6f, pristine: %.6f, enriched: %.6f\n",
        //            galaxies[gal].CGMgas, galaxies[gal].CGMgas_pristine, galaxies[gal].CGMgas_enriched);
        // }
        
        // Split infall between direct-to-hot and CGM pathways
        double cgm_pathway = infallingGas * get_cgm_infall_fraction(gal, galaxies, run_params);
        double direct_pathway = infallingGas * (1.0 - get_cgm_infall_fraction(gal, galaxies, run_params));

        // Direct pathway (original behavior)
        if(direct_pathway > 0.0) {
            galaxies[gal].HotGas += direct_pathway;
            galaxies[gal].MetalsHotGas += direct_pathway * 0.0;  // Primordial gas has no metals

            // NEW: Track direct infall to hot
            galaxies[gal].InfallRate_to_Hot += direct_pathway;
        }
        
        // CGM pathway (new behavior) 
        if(cgm_pathway > 0.0) {
            double pristine_gas = cgm_pathway * run_params->CGMPristineFraction;
            double enriched_gas = cgm_pathway * (1.0 - run_params->CGMPristineFraction);
            
            galaxies[gal].CGMgas += cgm_pathway;
            galaxies[gal].CGMgas_pristine += pristine_gas;
            galaxies[gal].CGMgas_enriched += enriched_gas;

            // Safety check: ensure components don't exceed total
            double component_sum = galaxies[gal].CGMgas_pristine + galaxies[gal].CGMgas_enriched;
            if (component_sum > galaxies[gal].CGMgas) {
                // Rescale components proportionally to match total
                double scale_factor = galaxies[gal].CGMgas / component_sum;
                galaxies[gal].CGMgas_pristine *= scale_factor;
                galaxies[gal].CGMgas_enriched *= scale_factor;
            }
            
            // Only enriched gas has metals (pristine has zero by definition)
            // Following existing pattern: 0.3 * 0.02 = 0.006 (30% of solar metallicity)
            double enriched_metallicity = 0.3 * 0.02;
            galaxies[gal].MetalsCGMgas += enriched_gas * enriched_metallicity;

            // NEW: Track infall to CGM
            galaxies[gal].InfallRate_to_CGM += cgm_pathway;
            
        }
        // Debug: Check consistency after modification
        // if(gal == 0) {
        //     double component_sum = galaxies[gal].CGMgas_pristine + galaxies[gal].CGMgas_enriched;
        //     printf("DEBUG: After infall - CGM total: %.6f, components sum: %.6f, difference: %.6f\n",
        //            galaxies[gal].CGMgas, component_sum, fabs(galaxies[gal].CGMgas - component_sum));
        // }
    }
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