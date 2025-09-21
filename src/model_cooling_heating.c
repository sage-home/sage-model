#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"
#include "core_cool_func.h"

#include "model_cooling_heating.h"
#include "model_misc.h"
#include "model_infall.h"


double cooling_recipe(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    // Check if CGM recipe is enabled for backwards compatibility
    if(run_params->CGMrecipeOn > 0) {
        return cooling_recipe_regime_aware(gal, dt, galaxies, run_params);
    } else {
        return cooling_recipe_hot(gal, dt, galaxies, run_params);
    }
}


double cooling_recipe_hot(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    double coolingGas;

    if(galaxies[gal].HotGas > 0.0 && galaxies[gal].Vvir > 0.0) {
        const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
        const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;         // in Kelvin

        double logZ = -10.0;
        if(galaxies[gal].MetalsHotGas > 0) {
            logZ = log10(galaxies[gal].MetalsHotGas / galaxies[gal].HotGas);
        }

        double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
        double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
        x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);         // now in internal units
        const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

        // an isothermal density profile for the hot gas is assumed here
        const double rho0 = galaxies[gal].HotGas / (4 * M_PI * galaxies[gal].Rvir);
        const double rcool = sqrt(rho0 / rho_rcool);

        // Store rcool/Rvir ratio for later reference
        galaxies[gal].RcoolToRvir = rcool / galaxies[gal].Rvir;

        coolingGas = 0.0;
        if(rcool > galaxies[gal].Rvir) {
            // "cold accretion" regime
            coolingGas = galaxies[gal].HotGas / (galaxies[gal].Rvir / galaxies[gal].Vvir) * dt;
        } else {
            // "hot halo cooling" regime
            coolingGas = (galaxies[gal].HotGas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;
        }

        // coolingGas = (galaxies[gal].HotGas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;

        if(coolingGas > galaxies[gal].HotGas) {
            coolingGas = galaxies[gal].HotGas;
        } else {
			if(coolingGas < 0.0) coolingGas = 0.0;
        }

		// at this point we have calculated the maximal cooling rate
		// if AGNrecipeOn we now reduce it in line with past heating before proceeding

		if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
			coolingGas = do_AGN_heating(coolingGas, gal, dt, x, rcool, galaxies, run_params);
        }

		if (coolingGas > 0.0) {
			galaxies[gal].Cooling += 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
        }
	} else {
		coolingGas = 0.0;
    }

	XASSERT(coolingGas >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);
    return coolingGas;
}


double cooling_recipe_regime_aware(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    double total_cooling = 0.0;
    
    if(galaxies[gal].Regime == 0) {
        // CGM REGIME: CGM physics dominates
        
        // Primary: Precipitation cooling from CGMgas
        if(galaxies[gal].CGMgas > 0.0) {
            double cgm_cooling = cooling_recipe_cgm(gal, dt, galaxies, run_params);
            total_cooling += cgm_cooling;
        }
        
        // Secondary: Traditional cooling from HotGas  
        if(galaxies[gal].HotGas > 0.0) {
            double hot_cooling = cooling_recipe_hot(gal, dt, galaxies, run_params);
            total_cooling += hot_cooling;
        }
        
    } else {
        // HOT REGIME: Traditional physics dominates
        
        // Primary: Traditional cooling from HotGas
        if(galaxies[gal].HotGas > 0.0) {
            double hot_cooling = cooling_recipe_hot(gal, dt, galaxies, run_params);
            total_cooling += hot_cooling;
        }
        
        // Secondary: Precipitation cooling from CGMgas (gradually depletes)
        if(galaxies[gal].CGMgas > 0.0) {
            double cgm_cooling = cooling_recipe_cgm(gal, dt, galaxies, run_params);
            total_cooling += cgm_cooling;
        }
    }

    XASSERT(total_cooling >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", total_cooling);
    return total_cooling;
}

// double cooling_recipe_cgm(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
// {
//     double coolingGas = 0.0;

//     // In CGM regime, only cool the fraction of gas that is <10^4 K
//     if(galaxies[gal].CGMgas > 0.0 && galaxies[gal].Vvir > 0.0) {
        
//         // Calculate what fraction of CGM gas can cool efficiently
//         // const double f_cool = calculate_cgm_cool_fraction(gal, galaxies);
//         const double coolable_cgm_mass = galaxies[gal].CGMgas;
        
//         if(coolable_cgm_mass > 0.0) {
//             const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
//             const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;         // in Kelvin

//             double logZ = -10.0;
//             if(galaxies[gal].MetalsCGMgas > 0) {
//                 logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
//             }

//             double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
//             double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
//             x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);         // now in internal units
//             const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

//             // In CGM regime, only use CGM gas for density calculation
//             const double rho0 = galaxies[gal].CGMgas / (4 * M_PI * galaxies[gal].Rvir);
//             const double rcool = sqrt(rho0 / rho_rcool);

//             // Store rcool/Rvir ratio for later reference
//             galaxies[gal].RcoolToRvir = rcool / galaxies[gal].Rvir;

//             // Add a temperature-dependent efficiency factor for gradual cooling
//             // double cool_efficiency = 1.0;
//             // const double T_threshold = 2.5e5;

//             // if(temp <= T_threshold) {
//             //     cool_efficiency = 0.5;  // Reduced cooling rate in CGM regime
//             // } else {
//             //     cool_efficiency = 0.05;  // Even lower for transition regime
//             // }

//             // // Apply efficiency to final cooling rate
//             // if(rcool > galaxies[gal].Rvir) {
//             //     coolingGas = cool_efficiency * galaxies[gal].CGMgas / tcool * dt;
//             // } else {
//             //     coolingGas = cool_efficiency * (galaxies[gal].CGMgas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;
//             // }

//             // if(coolingGas > coolable_cgm_mass) {
//             //     coolingGas = coolable_cgm_mass;
//             // } else {
//             //     if(coolingGas < 0.0) coolingGas = 0.0;
//             // }
//             coolingGas = 0.0;
//             if(rcool > galaxies[gal].Rvir) {
//                 // "cold accretion" regime
//                 coolingGas = galaxies[gal].CGMgas / (galaxies[gal].Rvir / galaxies[gal].Vvir) * dt;
//             } else {
//                 // "hot halo cooling" regime
//                 coolingGas = (galaxies[gal].CGMgas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;
//             }

//             // coolingGas = (galaxies[gal].HotGas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;

//             if(coolingGas > galaxies[gal].CGMgas) {
//                 coolingGas = galaxies[gal].CGMgas;
//             } else {
//                 if(coolingGas < 0.0) coolingGas = 0.0;
//             }

//             // Apply AGN heating if enabled (using original AGN function)
//             if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
//                 coolingGas = do_AGN_heating_cgm(coolingGas, gal, dt, x, rcool, galaxies, run_params);
//             }

//             if (coolingGas > 0.0) {
//                 galaxies[gal].Cooling += 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
//             }
//         }
//     }

//     return coolingGas;
// }
// double cooling_recipe_cgm(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
// {
//     static long precipitation_debug_counter = 0;
//     precipitation_debug_counter++;
    
//     double coolingGas = 0.0;

//     // Check basic requirements
//     if(galaxies[gal].CGMgas <= 0.0 || galaxies[gal].Vvir <= 0.0 || galaxies[gal].Rvir <= 0.0) {
//         if(precipitation_debug_counter % 50000 == 0) {
//             printf("DEBUG PRECIP [%ld]: Early exit - CGMgas=%.2e, Vvir=%.2f, Rvir=%.2e\n",
//                    precipitation_debug_counter, galaxies[gal].CGMgas, galaxies[gal].Vvir, galaxies[gal].Rvir);
//         }
//         return 0.0;
//     }

//     // Step 1: Calculate tcool/tff ratio for precipitation criterion
//     const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir; // Virial temperature in Kelvin

//     double logZ = -10.0;
//     if(galaxies[gal].MetalsCGMgas > 0) {
//         logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
//     }

//     double lambda = get_metaldependent_cooling_rate(log10(temp), logZ); // erg cm^3 s^-1
    
//     // CORRECTED: Proper unit conversion for cooling time calculation
//     // Convert CGMgas from code units (10^10 Msun/h) to CGS (grams)
//     const double CGMgas_cgs = galaxies[gal].CGMgas * 1e10 * SOLAR_MASS / run_params->Hubble_h; // g
    
//     // Convert Rvir from code units (Mpc/h) to CGS (cm)  
//     const double Rvir_cgs = galaxies[gal].Rvir * CM_PER_MPC / run_params->Hubble_h; // cm
    
//     // Calculate mass density in CGS
//     const double volume_cgs = (4.0 * M_PI / 3.0) * Rvir_cgs * Rvir_cgs * Rvir_cgs; // cm^3
//     const double mass_density_cgs = CGMgas_cgs / volume_cgs; // g cm^-3
    
//     // Convert to number density (particles per cm^3)
//     // For fully ionized gas: mean molecular weight μ ≈ 0.59 (accounts for electrons)
//     const double mu = 0.59;
//     const double mean_particle_mass = mu * PROTONMASS; // g
//     const double number_density = mass_density_cgs / mean_particle_mass; // cm^-3
    
//     // Cooling time in CGS units (seconds)
//     const double tcool_cgs = (1.5 * BOLTZMANN * temp) / (number_density * lambda); // s
    
//     // Convert cooling time to code units
//     const double tcool = tcool_cgs / run_params->UnitTime_in_s; // code time units
    
//     // Free-fall time calculation (in code units)
//     const double g_accel = run_params->G * galaxies[gal].Mvir / (galaxies[gal].Rvir * galaxies[gal].Rvir);
//     const double tff = sqrt(2.0 * galaxies[gal].Rvir / g_accel);
    
//     const double tcool_over_tff = tcool / tff;
    
//     // Store the ratio for diagnostics
//     galaxies[gal].RcoolToRvir = tcool_over_tff / 10.0; // Repurpose this field for tcool/tff
    
//     // Step 2: Precipitation threshold criterion
//     const double precipitation_threshold = 10.0; // From literature
//     const double transition_width = 5.0; // Smooth transition over factor of ~2
    
//     double precipitation_fraction = 0.0;
//     if(tcool_over_tff < precipitation_threshold) {
//         precipitation_fraction = 1.0;
//     } else if(tcool_over_tff < precipitation_threshold + transition_width) {
//         const double x = (tcool_over_tff - precipitation_threshold) / transition_width;
//         precipitation_fraction = 0.5 * (1.0 - tanh(x));
//     }
    
//     // Debug output every 50,000 galaxies
//     if(precipitation_debug_counter % 50000 == 0) {
//         printf("\n=== PRECIPITATION COOLING DEBUG [Galaxy #%ld] ===\n", precipitation_debug_counter);
//         printf("BASIC PROPERTIES:\n");
//         printf("  CGMgas:      %.3e Msun (10^10 Msun/h units)\n", galaxies[gal].CGMgas);
//         printf("  CGMgas_cgs:  %.3e g\n", CGMgas_cgs);
//         printf("  Mvir:        %.3e Msun\n", galaxies[gal].Mvir);
//         printf("  Vvir:        %.2f km/s\n", galaxies[gal].Vvir);
//         printf("  Rvir:        %.3e Mpc/h\n", galaxies[gal].Rvir);
//         printf("  Rvir_cgs:    %.3e cm\n", Rvir_cgs);
//         printf("  T_vir:       %.2e K\n", temp);
//         printf("  Metallicity: %.4f (log10(Z/Z_sun) = %.2f)\n", 
//                (galaxies[gal].MetalsCGMgas > 0) ? galaxies[gal].MetalsCGMgas/galaxies[gal].CGMgas : 0.0, logZ);
        
//         printf("\nCOOLING PHYSICS:\n");
//         printf("  Lambda:      %.3e erg cm^3 s^-1\n", lambda);
//         printf("  Volume:      %.3e cm^3\n", volume_cgs);
//         printf("  Mass dens:   %.3e g cm^-3\n", mass_density_cgs);
//         printf("  Mean mass:   %.3e g (μ=%.2f)\n", mean_particle_mass, mu);
//         printf("  n_gas:       %.3e cm^-3\n", number_density);
//         printf("  t_cool_cgs:  %.3e s (%.2f Myr)\n", tcool_cgs, tcool_cgs / (1e6 * SEC_PER_YEAR));
//         printf("  t_cool_code: %.3e (%.2f Myr)\n", tcool, tcool * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR));
//         printf("  UnitTime:    %.3e s\n", run_params->UnitTime_in_s);
//         printf("  g_accel:     %.3e (code units)\n", g_accel);
//         printf("  t_ff:        %.3e (%.2f Myr)\n", tff, tff * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR));
//         printf("  t_cool/t_ff: %.2f\n", tcool_over_tff);
        
//         printf("\nPRECIPITATION CRITERION:\n");
//         printf("  Threshold:   %.1f\n", precipitation_threshold);
//         printf("  Trans width: %.1f\n", transition_width);
//         printf("  Upper limit: %.1f\n", precipitation_threshold + transition_width);
//         printf("  Precip frac: %.4f", precipitation_fraction);
//         if(tcool_over_tff < precipitation_threshold) {
//             printf(" [FULL PRECIPITATION]");
//         } else if(tcool_over_tff < precipitation_threshold + transition_width) {
//             printf(" [TRANSITION REGIME]");
//         } else {
//             printf(" [NO PRECIPITATION]");
//         }
//         printf("\n");
//     }
    
//     // Step 3: Calculate precipitation-driven cooling rate
//     double precip_rate = 0.0;
//     double raw_cooling_gas = 0.0;
    
//     if(precipitation_fraction > 0.0) {
//         // Gas precipitates on dynamical time when thermally unstable
//         precip_rate = precipitation_fraction * galaxies[gal].CGMgas / tff;
//         raw_cooling_gas = precip_rate * dt;
//         coolingGas = raw_cooling_gas;
        
//         // Ensure physical limits
//         if(coolingGas > galaxies[gal].CGMgas) {
//             coolingGas = galaxies[gal].CGMgas;
//         }
//         if(coolingGas < 0.0) {
//             coolingGas = 0.0;
//         }
        
//         if(precipitation_debug_counter % 50000 == 0) {
//             printf("\nPRECIPITATION COOLING:\n");
//             printf("  dt:              %.3e (%.2f Myr)\n", dt, dt * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR));
//             printf("  Precip rate:     %.3e Msun/time_unit\n", precip_rate);
//             printf("  Raw cooling:     %.3e Msun\n", raw_cooling_gas);
//             printf("  Limited cooling: %.3e Msun", coolingGas);
//             if(raw_cooling_gas > galaxies[gal].CGMgas) {
//                 printf(" [LIMITED BY AVAILABLE GAS]");
//             }
//             printf("\n");
//             printf("  Cooling/CGM:     %.4f (fraction of CGM cooled)\n", 
//                    galaxies[gal].CGMgas > 0 ? coolingGas/galaxies[gal].CGMgas : 0.0);
//         }
//     }
    
//     // Step 4: Self-regulation mechanism
//     double feedback_heating = 0.0;
//     double coolingGas_before_regulation = coolingGas;
    
//     if(coolingGas > 0.0) {
//         if(run_params->SupernovaRecipeOn > 0) {
//             // FIXED: Much more conservative self-regulation
            
//             // Option A: Simple efficiency-based approach (recommended)
//             // const double precipitation_sf_efficiency = 0.0001; // Reduced from 0.1 to 0.01
//             // const double triggered_sf = precipitation_sf_efficiency * coolingGas;
            
//             // // Use standard feedback efficiency from main SAGE model
//             // double z = run_params->ZZ[galaxies[gal].SnapNum];
//             // const double heating_efficiency = calculate_muratov_mass_loading(gal, galaxies, z); // Typically ~0.5-2.0
//             // feedback_heating = heating_efficiency * triggered_sf;
            
//             // Option B: Energy-based approach (if you prefer physics-based)

//             const double precipitation_sf_efficiency = galaxies[gal].SfrDisk[0] + galaxies[gal].SfrBulge[0] > 0 ? 0.1 : 0.01; // Higher efficiency if already forming stars
//             const double triggered_sf = precipitation_sf_efficiency * coolingGas;
            
//             // Energy injection per unit stellar mass (in code units)
//             const double energy_per_unit_mass = run_params->EtaSNcode * run_params->EnergySNcode;
            
//             // Convert to mass equivalent using virial velocity squared
//             // Add safety factor to prevent over-suppression
//             const double safety_factor = 0.1; // Prevent complete suppression
//             feedback_heating = safety_factor * triggered_sf * energy_per_unit_mass / (galaxies[gal].Vvir * galaxies[gal].Vvir);

//         }
        
//         // Apply heating to reduce cooling (self-regulation)
//         if(feedback_heating > coolingGas) {
//             coolingGas = 0.0; // Heating completely suppresses cooling
//         } else {
//             coolingGas -= feedback_heating;
//         }
        
//         if(precipitation_debug_counter % 50000 == 0) {
//             printf("\nSELF-REGULATION:\n");
//             printf("  SN recipe on:    %s\n", run_params->SupernovaRecipeOn > 0 ? "YES" : "NO");
//             if(run_params->SupernovaRecipeOn > 0) {
//                 const double precipitation_sf_efficiency = 0.1; // Updated value
//                 const double triggered_sf = precipitation_sf_efficiency * coolingGas_before_regulation;
//                 const double heating_efficiency = 1.0; // Consistent with above
                
//                 printf("  SF efficiency:   %.3f (reduced from 0.1)\n", precipitation_sf_efficiency);
//                 printf("  Triggered SF:    %.3e Msun\n", triggered_sf);
//                 printf("  Heat efficiency: %.3f (fixed)\n", heating_efficiency);
//                 printf("  Feedback heat:   %.3e Msun\n", feedback_heating);
//                 printf("  Before reg:      %.3e Msun\n", coolingGas_before_regulation);
//                 printf("  After reg:       %.3e Msun", coolingGas);
//                 if(feedback_heating > coolingGas_before_regulation) {
//                     printf(" [COOLING FULLY SUPPRESSED]");
//                 } else if(feedback_heating > 0.0) {
//                     printf(" [PARTIALLY REGULATED]");
//                 } else {
//                     printf(" [NO REGULATION]");
//                 }
//                 printf("\n");
//                 printf("  Regulation eff:  %.3f (fraction suppressed)\n", 
//                        (coolingGas_before_regulation > 0) ? feedback_heating/coolingGas_before_regulation : 0.0);
//                 printf("  Heat/Cool ratio: %.3f\n", 
//                        (coolingGas_before_regulation > 0) ? feedback_heating/coolingGas_before_regulation : 0.0);
//             }
//         }
//     }
    
//     // Step 5: Apply AGN heating if enabled  
//     double coolingGas_before_agn = coolingGas;
    
//     if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
//         // For precipitation model, AGN heating is triggered by precipitating gas
        
//         // Calculate x parameter with proper units (same as in original cooling calculation)
//         const double x = PROTONMASS * BOLTZMANN * temp / lambda; // in g cm^3 s^-1
//         const double x_code = x / (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s); // convert to code units
        
//         // Calculate rcool for AGN heating (using tcool/tff ratio)
//         const double rcool = galaxies[gal].Rvir * sqrt(tcool_over_tff / 10.0);
        
//         coolingGas = do_AGN_heating_cgm(coolingGas, gal, dt, x_code, rcool, galaxies, run_params);
//     }
    
//     // Track cooling energy
//     if (coolingGas > 0.0) {
//         galaxies[gal].Cooling += 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
//     }

//     if(precipitation_debug_counter % 50000 == 0) {
//         printf("\nFINAL RESULTS:\n");
//         printf("  AGN heating:     %s\n", "DISABLED");
//         if(run_params->AGNrecipeOn > 0) {
//             printf("  Before AGN:      %.3e Msun\n", coolingGas_before_agn);
//             printf("  AGN suppression: %.3e Msun (%.3f efficiency)\n", 
//                    coolingGas_before_agn - coolingGas, 
//                    coolingGas_before_agn > 0 ? (coolingGas_before_agn - coolingGas)/coolingGas_before_agn : 0.0);
//         }
//         printf("  Final cooling:   %.3e Msun\n", coolingGas);
//         printf("  Cooling energy:  %.3e (code units)\n", 
//                coolingGas > 0.0 ? 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir : 0.0);
//         printf("  Total efficiency: %.4f (final/raw)\n", 
//                (raw_cooling_gas > 0) ? coolingGas/raw_cooling_gas : 0.0);
        
//         // Physical reasonableness checks
//         printf("\nSANITY CHECKS:\n");
//         if(coolingGas > galaxies[gal].CGMgas) {
//             printf("  WARNING: Cooling > CGM gas!\n");
//         }
//         if(tcool_over_tff < 1.0) {
//             printf("  NOTE: Very short cooling time (tcool < tff)\n");
//         }
//         if(tcool_over_tff > 100.0) {
//             printf("  NOTE: Very long cooling time (tcool >> tff)\n");
//         }
//         if(precipitation_fraction > 0.0 && coolingGas == 0.0) {
//             printf("  NOTE: Precipitation predicted but cooling suppressed\n");
//         }
//         if(precipitation_fraction == 0.0 && coolingGas > 0.0) {
//             printf("  WARNING: Cooling without precipitation!\n");
//         }
        
//         printf("============================================\n\n");
//     }

//     XASSERT(coolingGas >= 0.0, -1, "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);
//     return coolingGas;
// }

// double cooling_recipe_cgm(const int gal, const double dt, struct GALAXY *galaxies, 
//                           const struct params *run_params)
// {
//     static long cgm_inflow_debug_counter = 0;
//     cgm_inflow_debug_counter++;
    
//     // Early exit if no CGM gas available
//     if(galaxies[gal].CGMgas <= 1e-20) {
//         return 0.0;
//     }
    
//     // Calculate recent SFR (current timestep total)
//     double recent_sfr_disk = 0.0;
//     double recent_sfr_bulge = 0.0;
//     for(int step = 0; step < STEPS; step++) {
//         recent_sfr_disk += galaxies[gal].SfrDisk[step];
//         recent_sfr_bulge += galaxies[gal].SfrBulge[step];
//     }
//     recent_sfr_disk /= STEPS;
//     recent_sfr_bulge /= STEPS;
//     double total_recent_sfr = recent_sfr_disk + recent_sfr_bulge;
    
//     // SCENARIO C: DIRECT TIME-BASED DEPLETION
    
//     // Step 1: Check if this galaxy should have any inflow at all
//     double sfr_threshold = 1e-6; // Very low threshold
//     if(total_recent_sfr > sfr_threshold) {
//         // High SFR: Completely block inflow (preventive feedback)
//         if(cgm_inflow_debug_counter % 50000 == 0) {
//             printf("\n=== CGM INFLOW BLOCKED [Galaxy #%ld] ===\n", cgm_inflow_debug_counter);
//             printf("  SFR=%.3e > threshold=%.3e -> NO INFLOW\n", total_recent_sfr, sfr_threshold);
//             printf("  CGMgas=%.3e will deplete via other channels\n", galaxies[gal].CGMgas);
//             printf("============================================\n\n");
//         }
//         return 0.0; // Completely block inflow during active star formation
//     }
    
//     // Step 2: For low SFR galaxies, apply DIRECT TIME-BASED depletion
//     // Target: CGM depletes in 50-200 Myr (reasonable for Scenario C)
//     double target_depletion_time_myr = 500.0; // 500 Myr target depletion time
    
//     // Convert to code units
//     double target_depletion_time_code = target_depletion_time_myr * 1e6 * SEC_PER_YEAR / run_params->UnitTime_in_s;
    
//     // Calculate current timestep in Myr for diagnostics
//     double dt_myr = dt * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
    
//     // Calculate base inflow rate: CGM_mass / depletion_time
//     double base_inflow_rate = galaxies[gal].CGMgas / target_depletion_time_code; // mass per code time
    
//     // Apply minimal suppression for low SFR
//     double CGM_SFR0 = 1e-6; // Match the threshold
//     double sfr_ratio = total_recent_sfr / CGM_SFR0;
//     double suppression_factor = 1.0 / (1.0 + 10.0 * sfr_ratio); // Strong suppression
    
//     // Minimum allowed fraction
//     double minimum_inflow_fraction = 0.01; // 1% minimum
//     suppression_factor = fmax(suppression_factor, minimum_inflow_fraction);
    
//     // Step 3: Final inflow calculation
//     double inflow_rate = base_inflow_rate * suppression_factor;
//     double actual_inflow = inflow_rate * dt; // mass per timestep
    
//     // Ensure we don't exceed available CGM
//     actual_inflow = fmin(actual_inflow, galaxies[gal].CGMgas);
    
//     // Step 4: Safety check - complete depletion for tiny amounts
//     if(galaxies[gal].CGMgas < 1e-10) {
//         actual_inflow = galaxies[gal].CGMgas; // Complete depletion for tiny amounts
//     }
    
//     // Debug output every 50,000 galaxies
//     if(cgm_inflow_debug_counter % 50000 == 0) {
//         printf("\n=== DIRECT TIME-BASED CGM DEPLETION [Galaxy #%ld] ===\n", cgm_inflow_debug_counter);
//         printf("SIMPLE TIME-BASED APPROACH:\n");
//         printf("  CGMgas:                  %.3e Msun\n", galaxies[gal].CGMgas);
//         printf("  Recent SFR:              %.3e Msun/time_unit\n", total_recent_sfr);
//         printf("  SFR threshold:           %.3e (above this = NO inflow)\n", sfr_threshold);
//         printf("  Target depletion time:   %.1f Myr\n", target_depletion_time_myr);
//         printf("  Target time (code units): %.3e\n", target_depletion_time_code);
//         printf("  Current dt:              %.3e (code units)\n", dt);
//         printf("  dt in Myr:               %.2f Myr\n", dt_myr);
//         printf("  Base inflow rate:        %.3e Msun/time_unit\n", base_inflow_rate);
//         printf("  Suppression factor:      %.4f\n", suppression_factor);
//         printf("  Final inflow:            %.3e Msun\n", actual_inflow);
//         printf("  CGM fraction consumed:   %.4f (fraction of total CGM)\n",
//                galaxies[gal].CGMgas > 0 ? actual_inflow/galaxies[gal].CGMgas : 0.0);
        
//         // CORRECTED: Proper depletion time calculation
//         if(actual_inflow > 0.0) {
//             double actual_depletion_time_code = galaxies[gal].CGMgas / inflow_rate;
//             double actual_depletion_time_myr = actual_depletion_time_code * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
//             double timesteps_to_depletion = actual_depletion_time_code / dt;
            
//             printf("  Actual depletion time:   %.2f Myr (direct calculation)\n", actual_depletion_time_myr);
//             printf("  Timesteps to deplete:    %.1f timesteps\n", timesteps_to_depletion);
//         } else {
//             printf("  Actual depletion time:   INFINITE (no consumption)\n");
//         }
        
//         if(total_recent_sfr > sfr_threshold) {
//             printf("  STATUS: INFLOW BLOCKED - SFR too high\n");
//         } else if(actual_inflow >= 0.9 * galaxies[gal].CGMgas) {
//             printf("  STATUS: COMPLETE DEPLETION - CGM emptying this timestep\n");
//         } else if(actual_inflow > 0.0) {
//             double actual_depletion_time_myr = (galaxies[gal].CGMgas / inflow_rate) * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
//             if(actual_depletion_time_myr < 500.0) {
//                 printf("  STATUS: RAPID DEPLETION - CGM draining in %.1f Myr\n", actual_depletion_time_myr);
//             } else {
//                 printf("  STATUS: SLOW DEPLETION - CGM draining in %.1f Myr\n", actual_depletion_time_myr);
//             }
//         } else {
//             printf("  STATUS: NO DEPLETION - No consumption\n");
//         }
//         printf("============================================\n\n");
//     }

//     // if(galaxies[gal].CGMgas > 0.0 && galaxies[gal].Vvir > 0.0) {
//     //     const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
//     //     const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;         // in Kelvin

//     //     double logZ = -10.0;
//     //     if(galaxies[gal].MetalsCGMgas > 0) {
//     //         logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
//     //     }

//     //     double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
//     //     double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
//     //     x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);         // now in internal units
//     //     const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

//     //     // an isothermal density profile for the hot gas is assumed here
//     //     const double rho0 = galaxies[gal].CGMgas / (4 * M_PI * galaxies[gal].Rvir);
//     //     const double rcool = sqrt(rho0 / rho_rcool);

//     //     if(run_params->AGNrecipeOn > 0 && actual_inflow > 0.0) {
//     //             actual_inflow = do_AGN_heating_cgm(actual_inflow, gal, dt, x, rcool, galaxies, run_params);
//     //         }
//     // }
    
//     XASSERT(actual_inflow >= 0.0, -1, "Error: CGM inflow = %g should be >= 0.0", actual_inflow);
//     XASSERT(actual_inflow <= galaxies[gal].CGMgas + 1e-12, -1, 
//             "Error: CGM inflow = %g exceeds available CGM gas = %g", 
//             actual_inflow, galaxies[gal].CGMgas);
    
//     return actual_inflow;
// }

// double calculate_adaptive_sfr_norm(const int gal, struct GALAXY *galaxies, const struct params *run_params) 
// {
//     double current_sfr = 0.0;
//     for(int step = 0; step < STEPS; step++) {
//         current_sfr += galaxies[gal].SfrDisk[step] + galaxies[gal].SfrBulge[step];
//     }
//     current_sfr /= STEPS;
    
//     // Much gentler halo scaling - use log scaling
//     double log_mvir = log10(fmax(galaxies[gal].Mvir, 1.0));
//     double halo_based_sfr = pow(10.0, 0.5 * (log_mvir - 1.0)); // Scales as M^0.5, normalized at 10 Msun
    
//     double blended_sfr = 0.3 * halo_based_sfr + 0.7 * current_sfr; // Prefer current SFR
    
//     return fmax(blended_sfr, 0.1); // Much smaller minimum
// }


double cooling_recipe_cgm(const int gal, const double dt, struct GALAXY *galaxies, 
                          const struct params *run_params)
{
    static long cgm_method3_debug_counter = 0;
    cgm_method3_debug_counter++;
    
    // Early exit if no CGM gas available
    if(galaxies[gal].CGMgas <= 1e-20) {
        return 0.0;
    }
    
    // Calculate recent SFR (current timestep total)
    double recent_sfr_disk = 0.0;
    double recent_sfr_bulge = 0.0;
    for(int step = 0; step < STEPS; step++) {
        recent_sfr_disk += galaxies[gal].SfrDisk[step];
        recent_sfr_bulge += galaxies[gal].SfrBulge[step];
    }
    recent_sfr_disk /= STEPS;
    recent_sfr_bulge /= STEPS;
    double total_recent_sfr = recent_sfr_disk + recent_sfr_bulge;
    
    // COUPLED FEEDBACK: Calculate recent outflow rate
    // Current outflow rate = mass_loading_factor * current_SFR
    double current_mass_loading = galaxies[gal].MassLoading; // Already calculated in starformation
    double current_outflow_rate = current_mass_loading * total_recent_sfr;
    
    // Estimate recent outflow rate (proxy for full history)
    // In a full implementation, you'd track outflow history like SFR history
    double recent_outflow_rate = current_outflow_rate; // Simplified for now
    
    // METHOD 3 COUPLED PARAMETERS
    double M_0 = 100.0;   // Characteristic inflow mass per unit time
    double SFR_0 = 0.1;    // SFR normalization for preventive feedback
    double OUTFLOW_0 = 0.5; // Outflow normalization for back-pressure (NEW PARAMETER)
    
    // COUPLED SUPPRESSION CALCULATION
    // 1. Original Method 3: SFR-based suppression
    double sfr_suppression_factor = SFR_0 / (SFR_0 + total_recent_sfr);

    // If this galaxy is in a dense environment (proxy: high Vvir)
    if(galaxies[gal].Vvir > 80.0) {
        sfr_suppression_factor *= 0.01;  // 90% additional suppression
    }
    
    // 2. NEW: Outflow back-pressure suppression
    // High outflow creates "pressure" that further suppresses inflow
    double outflow_suppression_factor = OUTFLOW_0 / (OUTFLOW_0 + recent_outflow_rate);
    
    // 3. Combined suppression (multiplicative coupling)
    double combined_suppression_factor = sfr_suppression_factor * outflow_suppression_factor;
    
    // Final inflow calculation
    double inflow_rate = M_0 * combined_suppression_factor;
    double actual_inflow = inflow_rate * dt;
    
    // Ensure we don't exceed available CGM
    double limited_inflow = fmin(actual_inflow, galaxies[gal].CGMgas);
    
    // Safety check for tiny amounts
    if(galaxies[gal].CGMgas < 1e-10) {
        limited_inflow = galaxies[gal].CGMgas;
    }
    
    // Calculate current timestep in Myr for diagnostics
    double dt_myr = dt * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
    
    // Enhanced diagnostics showing coupling
    if(cgm_method3_debug_counter % 50000 == 0) {
        printf("\n=== METHOD 3: COUPLED OUTFLOW-INFLOW FEEDBACK [Galaxy #%ld] ===\n", cgm_method3_debug_counter);
        printf("BASIC PROPERTIES:\n");
        printf("  CGMgas:              %.3e Msun\n", galaxies[gal].CGMgas);
        printf("  Recent SFR:          %.3e Msun/time_unit\n", total_recent_sfr);
        printf("  Mass loading factor: %.3f\n", current_mass_loading);
        printf("  Current outflow:     %.3e Msun/time_unit\n", current_outflow_rate);
        printf("  Current dt:          %.3e time_units (%.2f Myr)\n", dt, dt_myr);
        
        printf("\nCOUPLED FEEDBACK PARAMETERS:\n");
        printf("  M_0 (baseline):      %.3e Msun/time_unit\n", M_0);
        printf("  SFR_0 (SFR norm):    %.3e Msun/time_unit\n", SFR_0);
        printf("  OUTFLOW_0 (new):     %.3e Msun/time_unit\n", OUTFLOW_0);
        
        printf("\nDUAL SUPPRESSION CALCULATION:\n");
        printf("  SFR suppression:     %.6f (SFR_0 / (SFR_0 + SFR))\n", sfr_suppression_factor);
        printf("  Outflow suppression: %.6f (OUTFLOW_0 / (OUTFLOW_0 + OUTFLOW))\n", outflow_suppression_factor);
        printf("  Combined suppression: %.6f (multiplicative coupling)\n", combined_suppression_factor);
        printf("  Final inflow rate:   %.3e Msun/time_unit\n", inflow_rate);
        printf("  Raw inflow:          %.3e Msun\n", actual_inflow);
        printf("  Limited inflow:      %.3e Msun\n", limited_inflow);
        
        printf("\nCOUPLED FEEDBACK INTERPRETATION:\n");
        if(total_recent_sfr < 0.1 * SFR_0 && recent_outflow_rate < 0.1 * OUTFLOW_0) {
            printf("  REGIME: QUIESCENT - Minimal SFR & outflow, strong inflow\n");
            printf("  STATUS: CGM draining efficiently, galaxy refueling\n");
        } else if(total_recent_sfr > 10.0 * SFR_0 && recent_outflow_rate > 10.0 * OUTFLOW_0) {
            printf("  REGIME: STARBURST - High SFR & outflow, maximum suppression\n");
            printf("  STATUS: CGM pressurized, inflow nearly strangled\n");
        } else if(recent_outflow_rate > 5.0 * OUTFLOW_0) {
            printf("  REGIME: OUTFLOW DOMINATED - Back-pressure effect\n");
            printf("  STATUS: High outflow suppressing inflow beyond SFR effect\n");
        } else {
            printf("  REGIME: TRANSITIONAL - Mixed SFR/outflow effects\n");
            printf("  STATUS: Partial suppression from both mechanisms\n");
        }
        
        printf("\nSUPPRESSION BREAKDOWN:\n");
        printf("  SFR contribution:    %.3f (1 - SFR_suppression)\n", 1.0 - sfr_suppression_factor);
        printf("  Outflow contribution: %.3f (1 - outflow_suppression)\n", 1.0 - outflow_suppression_factor);
        printf("  Total suppression:   %.3f (1 - combined)\n", 1.0 - combined_suppression_factor);
        printf("  CGM fraction consumed: %.6f\n", 
               galaxies[gal].CGMgas > 0 ? limited_inflow/galaxies[gal].CGMgas : 0.0);
        
        // Compare to uncoupled Method 3
        double uncoupled_inflow = M_0 * sfr_suppression_factor * dt;
        uncoupled_inflow = fmin(uncoupled_inflow, galaxies[gal].CGMgas);
        printf("\nCOUPLING EFFECT:\n");
        printf("  Uncoupled Method 3:  %.3e Msun (SFR suppression only)\n", uncoupled_inflow);
        printf("  Coupled Method 3:    %.3e Msun (SFR + outflow suppression)\n", limited_inflow);
        printf("  Coupling ratio:      %.3f (coupled/uncoupled)\n", 
               uncoupled_inflow > 0 ? limited_inflow/uncoupled_inflow : 0.0);
        
        // Timescale analysis
        if(limited_inflow > 0.0) {
            double depletion_time_code = galaxies[gal].CGMgas / inflow_rate;
            double depletion_time_myr = depletion_time_code * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
            printf("\nTIMESCALE ANALYSIS:\n");
            printf("  CGM depletion time:  %.2f Myr\n", depletion_time_myr);
            
            if(depletion_time_myr < 50.0) {
                printf("  STATUS: Rapid depletion - galaxy will exhaust CGM quickly\n");
            } else if(depletion_time_myr > 5000.0) {
                printf("  STATUS: Quasi-static - CGM effectively frozen\n");
            } else {
                printf("  STATUS: Moderate depletion - sustained recycling possible\n");
            }
        } else {
            printf("\nTIMESCALE ANALYSIS:\n");
            printf("  CGM depletion time:  INFINITE (complete strangulation)\n");
        }
        
        printf("=========================================================\n\n");
    }
    
    // Final safety checks
    XASSERT(limited_inflow >= 0.0, -1, "Error: CGM inflow = %g should be >= 0.0", limited_inflow);
    XASSERT(limited_inflow <= galaxies[gal].CGMgas + 1e-12, -1, 
            "Error: CGM inflow = %g exceeds available CGM gas = %g", 
            limited_inflow, galaxies[gal].CGMgas);
    
    return limited_inflow;
}


double do_AGN_heating(double coolingGas, const int centralgal, const double dt, const double x, const double rcool, struct GALAXY *galaxies, const struct params *run_params)
{
    double AGNrate, EDDrate, AGNaccreted, AGNcoeff, AGNheating, metallicity;

	// first update the cooling rate based on the past AGN heating
	if(galaxies[centralgal].r_heat < rcool) {
		coolingGas = (1.0 - galaxies[centralgal].r_heat / rcool) * coolingGas;
    } else {
		coolingGas = 0.0;
    }

	XASSERT(coolingGas >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);

	// now calculate the new heating rate
    if(galaxies[centralgal].HotGas > 0.0) {
        if(run_params->AGNrecipeOn == 2) {
            // Bondi-Hoyle accretion recipe
            AGNrate = (2.5 * M_PI * run_params->G) * (0.375 * 0.6 * x) * galaxies[centralgal].BlackHoleMass * run_params->RadioModeEfficiency;
        } else if(run_params->AGNrecipeOn == 3) {
            // Cold cloud accretion: trigger: rBH > 1.0e-4 Rsonic, and accretion rate = 0.01% cooling rate
            if(galaxies[centralgal].BlackHoleMass > 0.0001 * galaxies[centralgal].Mvir * CUBE(rcool/galaxies[centralgal].Rvir)) {
                AGNrate = 0.0001 * coolingGas / dt;
            } else {
                AGNrate = 0.0;
            }
        } else {
            // empirical (standard) accretion recipe
            if(galaxies[centralgal].Mvir > 0.0) {
                AGNrate = run_params->RadioModeEfficiency / (run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS)
                    * (galaxies[centralgal].BlackHoleMass / 0.01) * CUBE(galaxies[centralgal].Vvir / 200.0)
                    * ((galaxies[centralgal].HotGas / galaxies[centralgal].Mvir) / 0.1);
            } else {
                AGNrate = run_params->RadioModeEfficiency / (run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS)
                    * (galaxies[centralgal].BlackHoleMass / 0.01) * CUBE(galaxies[centralgal].Vvir / 200.0);
            }
        }

        // Eddington rate
        EDDrate = (1.3e38 * galaxies[centralgal].BlackHoleMass * 1e10 / run_params->Hubble_h) / (run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s) / (0.1 * 9e10);

        // accretion onto BH is always limited by the Eddington rate
        if(AGNrate > EDDrate) {
            AGNrate = EDDrate;
        }

        // accreted mass onto black hole
        AGNaccreted = AGNrate * dt;

        // cannot accrete more mass than is available!
        if(AGNaccreted > galaxies[centralgal].HotGas) {
            AGNaccreted = galaxies[centralgal].HotGas;
        }

        // coefficient to heat the cooling gas back to the virial temperature of the halo
        // 1.34e5 = sqrt(2*eta*c^2), eta=0.1 (standard efficiency) and c in km/s
        AGNcoeff = (1.34e5 / galaxies[centralgal].Vvir) * (1.34e5 / galaxies[centralgal].Vvir);

        // cooling mass that can be suppresed from AGN heating
        AGNheating = AGNcoeff * AGNaccreted;

        /// the above is the maximal heating rate. we now limit it to the current cooling rate
        if(AGNheating > coolingGas) {
            AGNaccreted = coolingGas / AGNcoeff;
            AGNheating = coolingGas;
        }

        // accreted mass onto black hole
        metallicity = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
        galaxies[centralgal].BlackHoleMass += AGNaccreted;
        galaxies[centralgal].HotGas -= AGNaccreted;
        galaxies[centralgal].MetalsHotGas -= metallicity * AGNaccreted;

        // update the heating radius as needed
        if(galaxies[centralgal].r_heat < rcool && coolingGas > 0.0) {
            double r_heat_new = (AGNheating / coolingGas) * rcool;
            if(r_heat_new > galaxies[centralgal].r_heat) {
                galaxies[centralgal].r_heat = r_heat_new;
            }
        }

        if (AGNheating > 0.0) {
            galaxies[centralgal].Heating += 0.5 * AGNheating * galaxies[centralgal].Vvir * galaxies[centralgal].Vvir;
        }
    }
    return coolingGas;
}

// double do_AGN_heating_cgm(double coolingGas, const int centralgal, const double dt, const double x, const double rcool, 
//                          struct GALAXY *galaxies, const struct params *run_params)
// {
//     double AGNrate, EDDrate, AGNaccreted, AGNcoeff, AGNheating, metallicity;

// 	// first update the cooling rate based on the past AGN heating
// 	if(galaxies[centralgal].r_heat < rcool) {
// 		coolingGas = (1.0 - galaxies[centralgal].r_heat / rcool) * coolingGas;
//     } else {
// 		coolingGas = 0.0;
//     }

// 	XASSERT(coolingGas >= 0.0, -1,
//             "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);

// 	// now calculate the new heating rate
//     if(galaxies[centralgal].CGMgas > 0.0) {
//         if(run_params->AGNrecipeOn == 2) {
//             // Bondi-Hoyle accretion recipe
//             AGNrate = (2.5 * M_PI * run_params->G) * (0.375 * 0.6 * x) * galaxies[centralgal].BlackHoleMass * run_params->RadioModeEfficiency;
//         } else if(run_params->AGNrecipeOn == 3) {
//             // Cold cloud accretion: trigger: rBH > 1.0e-4 Rsonic, and accretion rate = 0.01% cooling rate
//             if(galaxies[centralgal].BlackHoleMass > 0.0001 * galaxies[centralgal].Mvir * CUBE(rcool/galaxies[centralgal].Rvir)) {
//                 AGNrate = 0.0001 * coolingGas / dt;
//             } else {
//                 AGNrate = 0.0;
//             }
//         } else {
//             // empirical (standard) accretion recipe
//             if(galaxies[centralgal].Mvir > 0.0) {
//                 AGNrate = run_params->RadioModeEfficiency / (run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS)
//                     * (galaxies[centralgal].BlackHoleMass / 0.01) * CUBE(galaxies[centralgal].Vvir / 200.0)
//                     * ((galaxies[centralgal].CGMgas / galaxies[centralgal].Mvir) / 0.1);
//             } else {
//                 AGNrate = run_params->RadioModeEfficiency / (run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS)
//                     * (galaxies[centralgal].BlackHoleMass / 0.01) * CUBE(galaxies[centralgal].Vvir / 200.0);
//             }
//         }

//         // Eddington rate
//         EDDrate = (1.3e38 * galaxies[centralgal].BlackHoleMass * 1e10 / run_params->Hubble_h) / (run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s) / (0.1 * 9e10);

//         // accretion onto BH is always limited by the Eddington rate
//         if(AGNrate > EDDrate) {
//             AGNrate = EDDrate;
//         }

//         // accreted mass onto black hole
//         AGNaccreted = AGNrate * dt;

//         // cannot accrete more mass than is available!
//         if(AGNaccreted > galaxies[centralgal].CGMgas) {
//             AGNaccreted = galaxies[centralgal].CGMgas;
//         }

//         // coefficient to heat the cooling gas back to the virial temperature of the halo
//         // 1.34e5 = sqrt(2*eta*c^2), eta=0.1 (standard efficiency) and c in km/s
//         AGNcoeff = (1.34e5 / galaxies[centralgal].Vvir) * (1.34e5 / galaxies[centralgal].Vvir);

//         // cooling mass that can be suppresed from AGN heating
//         AGNheating = AGNcoeff * AGNaccreted;

//         /// the above is the maximal heating rate. we now limit it to the current cooling rate
//         if(AGNheating > coolingGas) {
//             AGNaccreted = coolingGas / AGNcoeff;
//             AGNheating = coolingGas;
//         }

//         // accreted mass onto black hole
//         metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
//         galaxies[centralgal].BlackHoleMass += AGNaccreted;
//         galaxies[centralgal].CGMgas -= AGNaccreted;
//         galaxies[centralgal].MetalsCGMgas -= metallicity * AGNaccreted;

//         // update the heating radius as needed
//         if(galaxies[centralgal].r_heat < rcool && coolingGas > 0.0) {
//             double r_heat_new = (AGNheating / coolingGas) * rcool;
//             if(r_heat_new > galaxies[centralgal].r_heat) {
//                 galaxies[centralgal].r_heat = r_heat_new;
//             }
//         }

//         if (AGNheating > 0.0) {
//             galaxies[centralgal].Heating += 0.5 * AGNheating * galaxies[centralgal].Vvir * galaxies[centralgal].Vvir;
//         }
//     }
//     return coolingGas;
// }



void cool_gas_onto_galaxy_regime_aware(const int centralgal, const double coolingGas, struct GALAXY *galaxies)
{
    
    if(coolingGas <= 0.0) return;
    
    double total_available = galaxies[centralgal].CGMgas + galaxies[centralgal].HotGas;
    if(total_available <= 0.0) return;
    
    // Calculate what fraction of cooling comes from each reservoir
    double cgm_fraction = galaxies[centralgal].CGMgas / total_available;
    double hot_fraction = galaxies[centralgal].HotGas / total_available;
    
    double cgm_cooling = coolingGas * cgm_fraction;
    double hot_cooling = coolingGas * hot_fraction;
    
    // Apply CGM cooling
    if(cgm_cooling > 0.0 && cgm_cooling <= galaxies[centralgal].CGMgas) {
        const double metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
        galaxies[centralgal].ColdGas += cgm_cooling;
        galaxies[centralgal].MetalsColdGas += metallicity * cgm_cooling;
        galaxies[centralgal].CGMgas -= cgm_cooling;
        galaxies[centralgal].MetalsCGMgas -= metallicity * cgm_cooling;
    }
    
    // Apply HotGas cooling
    if(hot_cooling > 0.0 && hot_cooling <= galaxies[centralgal].HotGas) {
        const double metallicity = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
        galaxies[centralgal].ColdGas += hot_cooling;
        galaxies[centralgal].MetalsColdGas += metallicity * hot_cooling;
        galaxies[centralgal].HotGas -= hot_cooling;
        galaxies[centralgal].MetalsHotGas -= metallicity * hot_cooling;
    }
}

void cool_gas_onto_galaxy(const int centralgal, const double coolingGas, struct GALAXY *galaxies)
{
    // add the fraction 1/STEPS of the total cooling gas to the cold disk
    if(coolingGas > 0.0) {
        if(coolingGas < galaxies[centralgal].HotGas) {
            const double metallicity = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
            galaxies[centralgal].ColdGas += coolingGas;
            galaxies[centralgal].MetalsColdGas += metallicity * coolingGas;
            galaxies[centralgal].HotGas -= coolingGas;
            galaxies[centralgal].MetalsHotGas -= metallicity * coolingGas;
        } else {
            galaxies[centralgal].ColdGas += galaxies[centralgal].HotGas;
            galaxies[centralgal].MetalsColdGas += galaxies[centralgal].MetalsHotGas;
            galaxies[centralgal].HotGas = 0.0;
            galaxies[centralgal].MetalsHotGas = 0.0;
        }
    }
}