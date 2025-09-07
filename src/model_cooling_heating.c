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
    double coolingGas = 0.0;
    
    // Check basic requirements for cooling
    if(galaxies[gal].Vvir <= 0.0) {
        return 0.0;
    }

    if(galaxies[gal].Regime == 0) {
        // CGM REGIME: rcool > Rvir (cold accretion regime)
        // Cool from CGM gas reservoir
        coolingGas = cooling_recipe_cgm(gal, dt, galaxies, run_params);
    } else {
        // HOT REGIME: rcool < Rvir (hot halo cooling regime)  
        // Cool from HotGas reservoir
        coolingGas = cooling_recipe_hot(gal, dt, galaxies, run_params);
    }

    XASSERT(coolingGas >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);
    return coolingGas;
}


double calculate_cgm_cool_fraction(const int gal, struct GALAXY *galaxies)
{
    // Calculate fraction of CGM gas that can cool efficiently (T < 10^4 K)
    // Based on observational constraints from Tumlinson+2017, Werk+2014
    
    if(galaxies[gal].Vvir <= 0.0) return 0.0;
    
    const double T_vir = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir; // Virial temperature in K
    const double T_cool = 1.0e4; // Cooling floor temperature in K
    
    double f_cool;
    if(T_vir <= T_cool) {
        // Low-mass halos: most gas can cool
        f_cool = 0.9;
    } else {
        // Temperature-dependent cooling fraction
        // Power-law model based on observational constraints
        f_cool = pow(T_cool / T_vir, 0.7);
        
        // Apply observational limits
        if(galaxies[gal].Mvir < 1.0e12) {
            // Low-mass halos: higher cool fraction (50-80%)
            f_cool = fmax(f_cool, 0.5);
            f_cool = fmin(f_cool, 0.8);
        } else {
            // High-mass halos: lower cool fraction (10-30%)
            f_cool = fmax(f_cool, 0.1);
            f_cool = fmin(f_cool, 0.3);
        }
    }
    
    return f_cool;
}

void cgm_inflow_model(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    // CGM inflow model: called for all systems to handle gas flow from CGM reservoir
    // Only acts when system has CGM gas accumulated (from rcool >= Rvir conditions)
    
    if(galaxies[gal].CGMgas <= 0.0 || galaxies[gal].Vvir <= 0.0) {
        return; // No CGM gas or invalid galaxy
    }
    
    // This function handles additional CGM->disk flows beyond the main cooling recipe
    // For now, it's a placeholder that ensures mass conservation
    // Future enhancement: could add environmental effects, ram pressure, etc.
    
    // The main CGM cooling is handled by cooling_recipe_cgm() which uses
    // temperature-dependent cooling fractions. This function could handle
    // additional physics like:
    // - Environmental stripping
    // - Ram pressure effects  
    // - Satellite-specific CGM physics
    // - Time-dependent CGM evolution
    
    return; // Currently no additional inflow beyond cooling_recipe_cgm()
}

double cooling_recipe_cgm(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    double coolingGas = 0.0;

    // In CGM regime, only cool the fraction of gas that is <10^4 K
    if(galaxies[gal].CGMgas > 0.0 && galaxies[gal].Vvir > 0.0) {
        
        // Calculate what fraction of CGM gas can cool efficiently
        const double f_cool = calculate_cgm_cool_fraction(gal, galaxies);
        const double coolable_cgm_mass = f_cool * galaxies[gal].CGMgas;
        
        if(coolable_cgm_mass > 0.0) {
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

            // In CGM regime, only use CGM gas for density calculation
            const double rho0 = galaxies[gal].CGMgas / (4 * M_PI * galaxies[gal].Rvir);
            const double rcool = sqrt(rho0 / rho_rcool);

            // Cool only the coolable fraction on freefall timescale
            coolingGas = coolable_cgm_mass / tcool * dt;

            if(coolingGas > coolable_cgm_mass) {
                coolingGas = coolable_cgm_mass;
            } else {
                if(coolingGas < 0.0) coolingGas = 0.0;
            }

            // Apply AGN heating if enabled (using original AGN function)
            if(run_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
                coolingGas = do_AGN_heating_cgm(coolingGas, gal, dt, x, rcool, galaxies, run_params);
            }

            if (coolingGas > 0.0) {
                galaxies[gal].Cooling += 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
            }
        }
    }

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

    // now calculate the new heating rate - for CGM regime we need to check for gas availability
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
            // empirical (standard) accretion recipe - modified for CGM regime
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

        // cannot accrete more mass than is available in CGM!
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

        // accreted mass onto black hole from CGM
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



void cool_gas_onto_galaxy_regime_aware(const int centralgal, const double coolingGas, struct GALAXY *galaxies, const struct params *run_params)
{
    
    // add the fraction 1/STEPS of the total cooling gas to the cold disk
    if(coolingGas > 0.0) {
        if(galaxies[centralgal].Regime == 0) {
            // CGM regime - cool from CGM reservoir
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
            // Hot regime - cool from HotGas reservoir
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