#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"
#include "core_cool_func.h"

#include "model_cooling_heating.h"
#include "model_misc.h"


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


double cooling_recipe_cgm(const int gal, const double dt, struct GALAXY *galaxies, 
                         const struct params *run_params)
{
    static long precipitation_debug_counter = 0;
    precipitation_debug_counter++;
    
    double coolingGas = 0.0;

    // ========================================================================
    // EARLY EXIT CONDITIONS
    // ========================================================================
    if(galaxies[gal].CGMgas <= 0.0 || galaxies[gal].Vvir <= 0.0 || galaxies[gal].Rvir <= 0.0) {
        if(precipitation_debug_counter % 50000 == 0) {
            printf("DEBUG PRECIP [%ld]: Early exit - CGMgas=%.2e, Vvir=%.2f, Rvir=%.2e\n",
                   precipitation_debug_counter, galaxies[gal].CGMgas, 
                   galaxies[gal].Vvir, galaxies[gal].Rvir);
        }
        return 0.0;
    }

    // ========================================================================
    // STEP 1: CALCULATE COOLING TIME (CGS UNITS)
    // ========================================================================
    
    // Virial temperature
    const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir; // Kelvin
    
    // Metallicity
    double logZ = -10.0;
    if(galaxies[gal].MetalsCGMgas > 0) {
        logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
    }
    
    // Cooling function (erg cm^3 s^-1)
    double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
    
    // Convert CGM mass and radius to CGS
    const double CGMgas_cgs = galaxies[gal].CGMgas * 1e10 * SOLAR_MASS / run_params->Hubble_h; // g
    const double Rvir_cgs = galaxies[gal].Rvir * CM_PER_MPC / run_params->Hubble_h; // cm
    
    // Volume and mass density
    const double volume_cgs = (4.0 * M_PI / 3.0) * Rvir_cgs * Rvir_cgs * Rvir_cgs; // cm^3
    const double mass_density_cgs = CGMgas_cgs / volume_cgs; // g cm^-3
    
    // Number density (fully ionized gas: μ ≈ 0.59)
    const double mu = 0.59;
    const double mean_particle_mass = mu * PROTONMASS; // g
    const double number_density = mass_density_cgs / mean_particle_mass; // cm^-3
    
    // Cooling time: tcool = (3/2) * k * T / (n * Λ)
    const double tcool_cgs = (1.5 * BOLTZMANN * temp) / (number_density * lambda); // s
    const double tcool = tcool_cgs / run_params->UnitTime_in_s; // code units

    // ========================================================================
    // STEP 2: CALCULATE FREE-FALL TIME
    // ========================================================================
    
    // Gravitational acceleration at Rvir
    const double g_accel = run_params->G * galaxies[gal].Mvir / (galaxies[gal].Rvir * galaxies[gal].Rvir);
    
    // Free-fall time: tff = sqrt(2*R/g)
    const double tff = sqrt(2.0 * galaxies[gal].Rvir / g_accel); // code units
    
    // Critical ratio for precipitation
    const double tcool_over_tff = tcool / tff;

    // ========================================================================
    // STEP 3: PRECIPITATION CRITERION
    // ========================================================================
    
    const double precipitation_threshold = 10.0;  // McCourt et al. 2012
    const double transition_width = 2.0;  // Smooth transition over factor ~2
    
    double precipitation_fraction = 0.0;
    
    if(tcool_over_tff < precipitation_threshold) {
        // Don't always precipitate at free-fall rate!
        // Scale by how much tcool < tff
        double instability_factor = precipitation_threshold / tcool_over_tff;
        instability_factor = fmin(instability_factor, 3.0);  // Cap at 3x
        precipitation_fraction = tanh(instability_factor / 2.0);  // Smooth scaling

        } else if(tcool_over_tff < precipitation_threshold + transition_width) {
            // Transition regime - smooth with hyperbolic tangent
            const double x = (tcool_over_tff - precipitation_threshold) / transition_width;
            precipitation_fraction = 0.5 * (1.0 - tanh(x));
    }
    // else: tcool_over_tff >= 15, precipitation_fraction = 0.0 (thermally stable)

    // ========================================================================
    // STEP 4: CALCULATE PRECIPITATION RATE
    // ========================================================================
    
    if(precipitation_fraction > 0.0) {
        // Gas precipitates on the free-fall timescale when thermally unstable
        // This is the key physical insight: dM/dt = f_precip * M_CGM / t_ff
        const double precip_rate = precipitation_fraction * galaxies[gal].CGMgas / tff;
        coolingGas = precip_rate * dt;
        
        // Physical limits
        if(coolingGas > galaxies[gal].CGMgas) {
            coolingGas = galaxies[gal].CGMgas;
        }
        if(coolingGas < 0.0) {
            coolingGas = 0.0;
        }
    }

    // if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
    //         // Calculate x parameter (same as hot halo case)
    //         double x = PROTONMASS * BOLTZMANN * temp / lambda;
    //         x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);
            
    //         // For CGM, use free-fall time as characteristic radius scale
    //         // (analogous to cooling radius in hot halo case)
    //         const double rcool_cgm = sqrt(galaxies[gal].CGMgas / (4 * M_PI * galaxies[gal].Rvir));
            
    //         // Call the existing AGN heating function (works for CGM too!)
    //         coolingGas = do_AGN_heating_cgm(coolingGas, gal, dt, x, rcool_cgm, 
    //                                        galaxies, run_params);
    //     }

    // ========================================================================
    // STEP 5: TRACK COOLING ENERGY
    // ========================================================================
    
    // Energy associated with cooling (for feedback balance tracking)
    if(coolingGas > 0.0) {
        // Specific energy ~ 0.5 * Vvir^2 (thermal + kinetic)
        galaxies[gal].Cooling += 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
    }

    // ========================================================================
    // DIAGNOSTIC OUTPUT (every 50,000 galaxies)
    // ========================================================================
    
    // if(precipitation_debug_counter % 50000 == 0) {
    //     printf("\n=== PRECIPITATION COOLING DEBUG [Galaxy #%ld] ===\n", precipitation_debug_counter);
        
    //     printf("BASIC PROPERTIES:\n");
    //     printf("  CGMgas:      %.3e (10^10 Msun/h)\n", galaxies[gal].CGMgas);
    //     printf("  Mvir:        %.3e (10^10 Msun/h)\n", galaxies[gal].Mvir);
    //     printf("  Vvir:        %.2f km/s\n", galaxies[gal].Vvir);
    //     printf("  Rvir:        %.3e Mpc/h\n", galaxies[gal].Rvir);
    //     printf("  T_vir:       %.2e K\n", temp);
    //     printf("  Metallicity: log10(Z/Zsun) = %.2f\n", logZ);
        
    //     printf("\nCOOLING PHYSICS:\n");
    //     printf("  Lambda:      %.3e erg cm^3 s^-1\n", lambda);
    //     printf("  n_gas:       %.3e cm^-3\n", number_density);
    //     printf("  t_cool:      %.2f Myr\n", tcool * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR));
    //     printf("  t_ff:        %.2f Myr\n", tff * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR));
    //     printf("  t_cool/t_ff: %.2f", tcool_over_tff);
        
    //     if(tcool_over_tff < precipitation_threshold) {
    //         printf(" [THERMALLY UNSTABLE - FULL PRECIPITATION]\n");
    //     } else if(tcool_over_tff < precipitation_threshold + transition_width) {
    //         printf(" [TRANSITION REGIME - PARTIAL PRECIPITATION]\n");
    //     } else {
    //         printf(" [THERMALLY STABLE - NO PRECIPITATION]\n");
    //     }
        
    //     printf("\nPRECIPITATION RESULTS:\n");
    //     printf("  Precip frac: %.4f\n", precipitation_fraction);
    //     printf("  Cooling:     %.3e Msun (this timestep)\n", coolingGas);
    //     printf("  Fraction:    %.4f (of total CGM)\n", 
    //            galaxies[gal].CGMgas > 0 ? coolingGas/galaxies[gal].CGMgas : 0.0);
        
    //     // Depletion timescale
    //     if(coolingGas > 0.0) {
    //         const double depletion_time = galaxies[gal].CGMgas * tff / (precipitation_fraction * galaxies[gal].CGMgas);
    //         const double depletion_time_myr = depletion_time * run_params->UnitTime_in_s / (1e6 * SEC_PER_YEAR);
    //         printf("  Depletion t: %.2f Myr\n", depletion_time_myr);
    //     }
        
    //     printf("============================================\n\n");
    // }

    // Sanity check
    XASSERT(coolingGas >= 0.0, -1, "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);
    XASSERT(coolingGas <= galaxies[gal].CGMgas + 1e-12, -1,
            "Error: Cooling gas = %g exceeds CGM gas = %g", coolingGas, galaxies[gal].CGMgas);
    
    return coolingGas;
}

double cooling_recipe_cgm_sage(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    double coolingGas;

    if(galaxies[gal].CGMgas > 0.0 && galaxies[gal].Vvir > 0.0) {
        const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
        const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;         // in Kelvin

        double logZ = -10.0;
        if(galaxies[gal].MetalsCGMgas > 0) {
            logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
        }

        double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
        double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
        x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);         // now in internal units
        const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

        // an isothermal density profile for the hot gas is assumed here
        const double rho0 = galaxies[gal].CGMgas / (4 * M_PI * galaxies[gal].Rvir);
        const double rcool = sqrt(rho0 / rho_rcool);

        coolingGas = 0.0;
        if(rcool > galaxies[gal].Rvir) {
            // "cold accretion" regime
            coolingGas = galaxies[gal].CGMgas / (galaxies[gal].Rvir / galaxies[gal].Vvir) * dt;
        } else {
            // "hot halo cooling" regime
            coolingGas = (galaxies[gal].CGMgas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;
        }

        // coolingGas = (galaxies[gal].CGMgas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;
        // coolingGas = galaxies[gal].CGMgas / (galaxies[gal].Rvir / galaxies[gal].Vvir) * dt;

        if(coolingGas > galaxies[gal].CGMgas) {
            coolingGas = galaxies[gal].CGMgas;
        } else {
			if(coolingGas < 0.0) coolingGas = 0.0;
        }

		// at this point we have calculated the maximal cooling rate
		// if AGNrecipeOn we now reduce it in line with past heating before proceeding

		if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
			coolingGas = do_AGN_heating_cgm(coolingGas, gal, dt, x, rcool, galaxies, run_params);
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
    double cgm_cooling = 0.0;
    double hot_cooling = 0.0;
    
    if(galaxies[gal].Regime == 0) {
        // CGM REGIME: CGM physics dominates
        
        // Primary: Precipitation cooling from CGMgas
        if(galaxies[gal].CGMgas > 0.0) {
            if (run_params->CGMrecipeSAGEOn == 1) {
                cgm_cooling = cooling_recipe_cgm_sage(gal, dt, galaxies, run_params);
            } else {
                cgm_cooling = cooling_recipe_cgm(gal, dt, galaxies, run_params);
            }
        }
        
        // Secondary: Traditional cooling from HotGas  
        if(galaxies[gal].HotGas > 0.0) {
            hot_cooling = cooling_recipe_hot(gal, dt, galaxies, run_params);
        }
        
    } else {
        // HOT REGIME: Traditional physics dominates
        
        // Primary: Traditional cooling from HotGas
        if(galaxies[gal].HotGas > 0.0) {
            hot_cooling = cooling_recipe_hot(gal, dt, galaxies, run_params);
        }
        
        // Secondary: Precipitation cooling from CGMgas (gradually depletes)
        if(galaxies[gal].CGMgas > 0.0) {
            if (run_params->CGMrecipeSAGEOn == 1) {
                cgm_cooling = cooling_recipe_cgm_sage(gal, dt, galaxies, run_params);
            } else {
                cgm_cooling = cooling_recipe_cgm(gal, dt, galaxies, run_params);
            }
        }
    }

    // Now apply the cooling directly to preserve the physics-based split
    // Apply CGM cooling
    if(cgm_cooling > 0.0) {
        const double metallicity = get_metallicity(galaxies[gal].CGMgas, galaxies[gal].MetalsCGMgas);
        galaxies[gal].ColdGas += cgm_cooling;
        galaxies[gal].MetalsColdGas += metallicity * cgm_cooling;
        galaxies[gal].CGMgas -= cgm_cooling;
        galaxies[gal].MetalsCGMgas -= metallicity * cgm_cooling;
    }
    
    // Apply HotGas cooling
    if(hot_cooling > 0.0) {
        const double metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        galaxies[gal].ColdGas += hot_cooling;
        galaxies[gal].MetalsColdGas += metallicity * hot_cooling;
        galaxies[gal].HotGas -= hot_cooling;
        galaxies[gal].MetalsHotGas -= metallicity * hot_cooling;
    }

    double total_cooling = cgm_cooling + hot_cooling;
    XASSERT(total_cooling >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", total_cooling);
    return total_cooling;
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

double do_AGN_heating_cgm(double coolingGas, const int centralgal, const double dt, const double x, const double rcool, 
                         struct GALAXY *galaxies, const struct params *run_params)
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
    if(galaxies[centralgal].CGMgas > 0.0) {
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
                    * ((galaxies[centralgal].CGMgas / galaxies[centralgal].Mvir) / 0.1);
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
        if(AGNaccreted > galaxies[centralgal].CGMgas) {
            AGNaccreted = galaxies[centralgal].CGMgas;
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
        metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
        galaxies[centralgal].BlackHoleMass += AGNaccreted;
        galaxies[centralgal].CGMgas -= AGNaccreted;
        galaxies[centralgal].MetalsCGMgas -= metallicity * AGNaccreted;

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



// void cool_gas_onto_galaxy_regime_aware(const int centralgal, const double coolingGas, struct GALAXY *galaxies)
// {
//     if(coolingGas <= 0.0) return;
    
//     double total_available = galaxies[centralgal].CGMgas + galaxies[centralgal].HotGas;
//     if(total_available <= 0.0) return;
    
//     // Cap cooling to what's actually available
//     double actual_cooling = (coolingGas > total_available) ? total_available : coolingGas;
    
//     // Calculate what fraction comes from each reservoir
//     double cgm_fraction = galaxies[centralgal].CGMgas / total_available;
//     double hot_fraction = galaxies[centralgal].HotGas / total_available;
    
//     double cgm_cooling = actual_cooling * cgm_fraction;
//     double hot_cooling = actual_cooling * hot_fraction;
    
//     // Apply CGM cooling
//     if(cgm_cooling > 0.0) {
//         const double metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
//         galaxies[centralgal].ColdGas += cgm_cooling;
//         galaxies[centralgal].MetalsColdGas += metallicity * cgm_cooling;
//         galaxies[centralgal].CGMgas -= cgm_cooling;
//         galaxies[centralgal].MetalsCGMgas -= metallicity * cgm_cooling;
//     }
    
//     // Apply HotGas cooling
//     if(hot_cooling > 0.0) {
//         const double metallicity = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
//         galaxies[centralgal].ColdGas += hot_cooling;
//         galaxies[centralgal].MetalsColdGas += metallicity * hot_cooling;
//         galaxies[centralgal].HotGas -= hot_cooling;
//         galaxies[centralgal].MetalsHotGas -= metallicity * hot_cooling;
//     }
// }

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
