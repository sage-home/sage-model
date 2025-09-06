#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_infall.h"
#include "model_misc.h"
#include "core_cool_func.h"


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
        reionization_modifier = do_reionization(centralgal, Zcurr, galaxies, run_params);
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
        reionization_modifier = do_reionization(gal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

    double strippedGas = -1.0 *
        (reionization_modifier * run_params->BaryonFrac * galaxies[gal].Mvir - (galaxies[gal].StellarMass + galaxies[gal].ColdGas + galaxies[gal].HotGas + galaxies[gal].CGMgas + galaxies[gal].BlackHoleMass + galaxies[gal].ICS) ) / STEPS;
    // ( reionization_modifier * run_params->BaryonFrac * galaxies[gal].deltaMvir ) / STEPS;

    if(strippedGas > 0.0) {
        if(run_params->CGMrecipeOn > 0) {
            // Regime-aware stripping: strip from correct reservoir based on satellite's regime
            double satellite_rcool_to_rvir = calculate_rcool_to_rvir_ratio(gal, galaxies, run_params);
            double central_rcool_to_rvir = calculate_rcool_to_rvir_ratio(centralgal, galaxies, run_params);
            
            if(satellite_rcool_to_rvir > 1.0) {
                // CGM regime satellite: strip from CGMgas
                const double metallicity = get_metallicity(galaxies[gal].CGMgas, galaxies[gal].MetalsCGMgas);
                double strippedGasMetals = strippedGas * metallicity;

                if(strippedGas > galaxies[gal].CGMgas) strippedGas = galaxies[gal].CGMgas;
                if(strippedGasMetals > galaxies[gal].MetalsCGMgas) strippedGasMetals = galaxies[gal].MetalsCGMgas;

                galaxies[gal].CGMgas -= strippedGas;
                galaxies[gal].MetalsCGMgas -= strippedGasMetals;
                
                // Add to central galaxy's correct reservoir
                if(central_rcool_to_rvir > 1.0) {
                    // Central is CGM regime
                    galaxies[centralgal].CGMgas += strippedGas;
                    galaxies[centralgal].MetalsCGMgas += strippedGas * metallicity;
                } else {
                    // Central is HOT regime
                    galaxies[centralgal].HotGas += strippedGas;
                    galaxies[centralgal].MetalsHotGas += strippedGas * metallicity;
                }
            } else {
                // HOT regime satellite: strip from HotGas  
                const double metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
                double strippedGasMetals = strippedGas * metallicity;

                if(strippedGas > galaxies[gal].HotGas) strippedGas = galaxies[gal].HotGas;
                if(strippedGasMetals > galaxies[gal].MetalsHotGas) strippedGasMetals = galaxies[gal].MetalsHotGas;

                galaxies[gal].HotGas -= strippedGas;
                galaxies[gal].MetalsHotGas -= strippedGasMetals;
                
                // Add to central galaxy's correct reservoir
                if(central_rcool_to_rvir > 1.0) {
                    // Central is CGM regime
                    galaxies[centralgal].CGMgas += strippedGas;
                    galaxies[centralgal].MetalsCGMgas += strippedGas * metallicity;
                } else {
                    // Central is HOT regime
                    galaxies[centralgal].HotGas += strippedGas;
                    galaxies[centralgal].MetalsHotGas += strippedGas * metallicity;
                }
            }
        } else {
            // Original behavior when CGMrecipeOn = 0
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



void add_infall_to_cgm(const int gal, double infallingGas, struct GALAXY *galaxies)
{
    float metallicity;

    // In the low-mass regime (CGM regime), ALL gas goes to/from CGM, NO HotGas
    // For mass loss (negative infall), remove from CGM only
    if(infallingGas < 0.0 && galaxies[gal].CGMgas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].CGMgas, galaxies[gal].MetalsCGMgas);
        galaxies[gal].MetalsCGMgas += infallingGas*metallicity;
        if(galaxies[gal].MetalsCGMgas < 0.0) galaxies[gal].MetalsCGMgas = 0.0;

        galaxies[gal].CGMgas += infallingGas;
        if(galaxies[gal].CGMgas < 0.0) {
            galaxies[gal].CGMgas = galaxies[gal].MetalsCGMgas = 0.0;
        }
    } else {
        // For positive infall, add to CGM
        galaxies[gal].CGMgas += infallingGas;
        if(galaxies[gal].CGMgas < 0.0) galaxies[gal].CGMgas = galaxies[gal].MetalsCGMgas = 0.0;
    }

}



void add_infall_to_hot_pure(const int gal, double infallingGas, struct GALAXY *galaxies)
{
    float metallicity;

    // In the high-mass regime (hot halo regime), ALL gas goes to/from HotGas, NO CGM
    // For mass loss (negative infall), remove from HotGas only
    if(infallingGas < 0.0 && galaxies[gal].HotGas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        galaxies[gal].MetalsHotGas += infallingGas*metallicity;
        if(galaxies[gal].MetalsHotGas < 0.0) galaxies[gal].MetalsHotGas = 0.0;

        galaxies[gal].HotGas += infallingGas;
        if(galaxies[gal].HotGas < 0.0) {
            galaxies[gal].HotGas = galaxies[gal].MetalsHotGas = 0.0;
        }
    } else {
        // For positive infall, add to HotGas
        galaxies[gal].HotGas += infallingGas;
        if(galaxies[gal].HotGas < 0.0) galaxies[gal].HotGas = galaxies[gal].MetalsHotGas = 0.0;
    }

}



double calculate_rcool_to_rvir_ratio(const int gal, struct GALAXY *galaxies, const struct params *run_params)
{
    // Check basic requirements for physics calculation
    if(galaxies[gal].Vvir <= 0.0) {
        return 2.0;  // Return > 1.0 to indicate CGM regime for very small halos
    }

    const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
    const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;         // in Kelvin

    // Use metallicity from either HotGas or CGM gas, whichever exists
    double logZ = -10.0;
    if(galaxies[gal].HotGas > 0.0 && galaxies[gal].MetalsHotGas > 0.0) {
        logZ = log10(galaxies[gal].MetalsHotGas / galaxies[gal].HotGas);
    } else if(galaxies[gal].CGMgas > 0.0 && galaxies[gal].MetalsCGMgas > 0.0) {
        logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
    }

    double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
    double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
    x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);         // now in internal units
    const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

    // For density calculation, use total gas (HotGas + CGMgas) to represent potential hot halo
    double total_gas = galaxies[gal].HotGas + galaxies[gal].CGMgas;
    if(total_gas <= 0.0) {
        return 2.0;  // Return > 1.0 to indicate CGM regime if no gas at all
    }

    // an isothermal density profile for the hot gas is assumed here
    const double rho0 = total_gas / (4 * M_PI * galaxies[gal].Rvir);
    const double rcool = sqrt(rho0 / rho_rcool);

    return rcool / galaxies[gal].Rvir;
}



void handle_regime_transition(const int gal, struct GALAXY *galaxies, const struct params *run_params)
{
    // Only handle transitions when CGM recipe is enabled
    if(run_params->CGMrecipeOn != 1) {
        return;
    }
    
    const double rcool_to_rvir = calculate_rcool_to_rvir_ratio(gal, galaxies, run_params);
    
    if(rcool_to_rvir > 1.0) {
        // CGM regime: transfer all HotGas to CGM
        if(galaxies[gal].HotGas > 1e-10) {
            galaxies[gal].CGMgas += galaxies[gal].HotGas;
            galaxies[gal].MetalsCGMgas += galaxies[gal].MetalsHotGas;
            galaxies[gal].HotGas = 0.0;
            galaxies[gal].MetalsHotGas = 0.0;
        }
    } else {
        // HOT regime: transfer all CGMgas to HotGas
        if(galaxies[gal].CGMgas > 1e-10) {
            galaxies[gal].HotGas += galaxies[gal].CGMgas;
            galaxies[gal].MetalsHotGas += galaxies[gal].MetalsCGMgas;
            galaxies[gal].CGMgas = 0.0;
            galaxies[gal].MetalsCGMgas = 0.0;
        }
    }
}
