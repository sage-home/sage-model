#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"
#include "core_cool_func.h"

#include "model_cooling_heating.h"
#include "model_misc.h"

// Debug counter for regime tracking
static long cooling_debug_counter = 0;


double cooling_recipe(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
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

        if(coolingGas > galaxies[gal].HotGas) {
            coolingGas = galaxies[gal].HotGas;
        } else {
			if(coolingGas < 0.0) coolingGas = 0.0;
        }

		// at this point we have calculated the maximal cooling rate
		// if AGNrecipeOn we now reduce it in line with past heating before proceeding

		if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
			coolingGas = do_AGN_heating_regime(coolingGas, gal, dt, x, rcool, galaxies, run_params);
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

double cooling_recipe_regime(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    // Increment debug counter
    cooling_debug_counter++;
    
    // If CGM toggle is off, use original behavior
    if(run_params->CgmOn == 0) {
        return cooling_recipe(gal, dt, galaxies, run_params);
    }

    // Convert halo mass to units of 10^13 Msun for comparison with threshold
    double halo_mass_1e13 = galaxies[gal].Mvir / 1000.0;

    double coolingGas;

    if(halo_mass_1e13 < run_params->CgmMassThreshold) {
        // Low-mass regime: cool from CGM only
        if(galaxies[gal].CGMgas > 0.0 && galaxies[gal].Vvir > 0.0) {
            const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
            const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;

            double logZ = -10.0;
            if(galaxies[gal].MetalsCGMgas > 0) {
                logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
            }

            double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
            double x = PROTONMASS * BOLTZMANN * temp / lambda;
            x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);
            const double rho_rcool = x / tcool * 0.885;

            const double rho0 = galaxies[gal].CGMgas / (4 * M_PI * galaxies[gal].Rvir);
            const double rcool = sqrt(rho0 / rho_rcool);

            coolingGas = 0.0;
            if(rcool > galaxies[gal].Rvir) {
                coolingGas = galaxies[gal].CGMgas / (galaxies[gal].Rvir / galaxies[gal].Vvir) * dt;
            } else {
                coolingGas = (galaxies[gal].CGMgas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;
            }

            if(coolingGas > galaxies[gal].CGMgas) {
                coolingGas = galaxies[gal].CGMgas;
            } else {
                if(coolingGas < 0.0) coolingGas = 0.0;
            }

            // AGN heating would need to be adapted for CGM, for now skip it in low-mass regime
            // if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
            //     coolingGas = do_AGN_heating(coolingGas, gal, dt, x, rcool, galaxies, run_params);
            // }

            if (coolingGas > 0.0) {
                galaxies[gal].Cooling += 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
            }
        } else {
            coolingGas = 0.0;
        }
    } else {
        // High-mass regime: use original hot gas cooling
        coolingGas = cooling_recipe(gal, dt, galaxies, run_params);
    }

    // Debug output every 50,000 galaxies
    if(cooling_debug_counter % 50000 == 0) {
        const char* regime = (halo_mass_1e13 < run_params->CgmMassThreshold) ? "CGM" : "HOT";
        printf("DEBUG COOLING [gal %ld]: Mvir=%.2e (%.2e x10^13), regime=%s, cooling=%.2e, gas=%.2e\n",
               cooling_debug_counter, galaxies[gal].Mvir, halo_mass_1e13, regime, coolingGas,
               (halo_mass_1e13 < run_params->CgmMassThreshold) ? galaxies[gal].CGMgas : galaxies[gal].HotGas);
    }

    XASSERT(coolingGas >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);
    return coolingGas;
}

void cool_gas_onto_galaxy_regime(const int centralgal, const double coolingGas, struct GALAXY *galaxies, const struct params *run_params)
{
    // If CGM toggle is off, use original behavior
    if(run_params->CgmOn == 0) {
        cool_gas_onto_galaxy(centralgal, coolingGas, galaxies);
        return;
    }

    // Convert halo mass to units of 10^13 Msun for comparison with threshold
    double halo_mass_1e13 = galaxies[centralgal].Mvir / 1000.0;

    if(coolingGas > 0.0) {
        if(halo_mass_1e13 < run_params->CgmMassThreshold) {
            // Low-mass regime: cool from CGM to ColdGas
            if(coolingGas < galaxies[centralgal].CGMgas) {
                const double metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
                galaxies[centralgal].ColdGas += coolingGas;
                galaxies[centralgal].MetalsColdGas += metallicity * coolingGas;
                galaxies[centralgal].CGMgas -= coolingGas;
                galaxies[centralgal].MetalsCGMgas -= metallicity * coolingGas;
            } else {
                galaxies[centralgal].ColdGas += galaxies[centralgal].CGMgas;
                galaxies[centralgal].MetalsColdGas += galaxies[centralgal].MetalsCGMgas;
                galaxies[centralgal].CGMgas = 0.0;
                galaxies[centralgal].MetalsCGMgas = 0.0;
            }
        } else {
            // High-mass regime: use original hot gas cooling
            cool_gas_onto_galaxy(centralgal, coolingGas, galaxies);
        }
    }
}

double do_AGN_heating_regime(double coolingGas, const int centralgal, const double dt, const double x, const double rcool, struct GALAXY *galaxies, const struct params *run_params)
{
    static long debug_count = 0;
    static long cgm_regime_count = 0;
    static long hot_regime_count = 0;
    
    debug_count++;
    
    if (run_params->CgmOn) {
        double halo_mass_1e13 = galaxies[centralgal].Mvir / 1000.0;
        
        if (halo_mass_1e13 < run_params->CgmMassThreshold) {
            // CGM regime: AGN heating operates on CGM gas
            coolingGas = do_AGN_heating_cgm(coolingGas, centralgal, dt, x, rcool, galaxies, run_params);
            cgm_regime_count++;
            
            // Ensure no HotGas exists in CGM regime
            galaxies[centralgal].HotGas = 0.0;
            galaxies[centralgal].MetalsHotGas = 0.0;
        } else {
            // HotGas regime: AGN heating operates on HotGas (original behavior)
            coolingGas = do_AGN_heating(coolingGas, centralgal, dt, x, rcool, galaxies, run_params);
            hot_regime_count++;
            
            // Ensure no CGM exists in HotGas regime
            galaxies[centralgal].CGMgas = 0.0;
            galaxies[centralgal].MetalsCGMgas = 0.0;
        }
    } else {
        // Original behavior when CgmOn=0
        coolingGas = do_AGN_heating(coolingGas, centralgal, dt, x, rcool, galaxies, run_params);
        hot_regime_count++;
    }
    
    if (debug_count % 50000 == 0) {
        printf("AGN heating: Processed %ld galaxies - CGM regime: %ld, HotGas regime: %ld\n", 
               debug_count, cgm_regime_count, hot_regime_count);
    }
    
    return coolingGas;
}

double do_AGN_heating_cgm(double coolingGas, const int centralgal, const double dt, const double x, const double rcool, struct GALAXY *galaxies, const struct params *run_params)
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

	// now calculate the new heating rate - operating on CGM gas instead of HotGas
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
            // empirical (standard) accretion recipe - using CGMgas instead of HotGas
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

        // cannot accrete more mass than is available from CGM!
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

        // accreted mass onto black hole - from CGM gas and metals
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
