#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../../core/core_allvars.h"
#include "../../core/core_cool_func.h"
#include "../../core/core_parameter_views.h"

#include "model_cooling_heating.h"
#include "model_misc.h"


/*
 * Main cooling calculation function
 * 
 * Calculates the amount of gas that cools from the hot halo onto the galaxy disk.
 * Uses parameter views for improved modularity.
 */
double cooling_recipe(const int gal, const double dt, struct GALAXY *galaxies, const struct cooling_params_view *cooling_params)
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
        x /= (cooling_params->UnitDensity_in_cgs * cooling_params->UnitTime_in_s);  // now in internal units
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
        if(cooling_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
            // Create AGN params view for do_AGN_heating
            struct agn_params_view agn_params;
            initialize_agn_params_view(&agn_params, cooling_params->full_params);
            
            coolingGas = do_AGN_heating(coolingGas, gal, dt, x, rcool, galaxies, &agn_params);
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

/*
 * Compatibility wrapper for cooling_recipe
 * 
 * Provides backwards compatibility with the old interface while
 * using the new parameter view-based implementation internally.
 */
double cooling_recipe_compat(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    struct cooling_params_view cooling_params;
    initialize_cooling_params_view(&cooling_params, run_params);
    return cooling_recipe(gal, dt, galaxies, &cooling_params);
}



/*
 * AGN heating calculation
 * 
 * Models the impact of AGN feedback on gas cooling.
 * Uses parameter views for improved modularity.
 */
double do_AGN_heating(double coolingGas, const int centralgal, const double dt, const double x, 
                     const double rcool, struct GALAXY *galaxies, const struct agn_params_view *agn_params)
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
        const struct params *run_params = agn_params->full_params; // For G access
        
        if(agn_params->AGNrecipeOn == 2) {
            // Bondi-Hoyle accretion recipe
            AGNrate = (2.5 * M_PI * run_params->cosmology.G) * (0.375 * 0.6 * x) * 
                    galaxies[centralgal].BlackHoleMass * agn_params->RadioModeEfficiency;
        } else if(agn_params->AGNrecipeOn == 3) {
            // Cold cloud accretion: trigger: rBH > 1.0e-4 Rsonic, and accretion rate = 0.01% cooling rate
            if(galaxies[centralgal].BlackHoleMass > 0.0001 * galaxies[centralgal].Mvir * CUBE(rcool/galaxies[centralgal].Rvir)) {
                AGNrate = 0.0001 * coolingGas / dt;
            } else {
                AGNrate = 0.0;
            }
        } else {
            // empirical (standard) accretion recipe
            if(galaxies[centralgal].Mvir > 0.0) {
                AGNrate = agn_params->RadioModeEfficiency / 
                        (agn_params->UnitMass_in_g / agn_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS) * 
                        (galaxies[centralgal].BlackHoleMass / 0.01) * 
                        CUBE(galaxies[centralgal].Vvir / 200.0) * 
                        ((galaxies[centralgal].HotGas / galaxies[centralgal].Mvir) / 0.1);
            } else {
                AGNrate = agn_params->RadioModeEfficiency / 
                        (agn_params->UnitMass_in_g / agn_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS) * 
                        (galaxies[centralgal].BlackHoleMass / 0.01) * 
                        CUBE(galaxies[centralgal].Vvir / 200.0);
            }
        }

        // Eddington rate
        EDDrate = (1.3e38 * galaxies[centralgal].BlackHoleMass * 1e10 / run_params->cosmology.Hubble_h) / 
                 (agn_params->UnitEnergy_in_cgs / agn_params->UnitTime_in_s) / (0.1 * 9e10);

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

/*
 * Compatibility wrapper for do_AGN_heating
 * 
 * Provides backwards compatibility with the old interface while
 * using the new parameter view-based implementation internally.
 */
double do_AGN_heating_compat(double coolingGas, const int centralgal, const double dt, 
                           const double x, const double rcool, struct GALAXY *galaxies, 
                           const struct params *run_params)
{
    struct agn_params_view agn_params;
    initialize_agn_params_view(&agn_params, run_params);
    return do_AGN_heating(coolingGas, centralgal, dt, x, rcool, galaxies, &agn_params);
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
