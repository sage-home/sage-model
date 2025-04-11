#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_infall.h"
#include "model_misc.h"
#include "model_h2_formation.h"


double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params)
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
        tot_ejected += galaxies[i].EjectedMass;
        tot_ejectedMetals += galaxies[i].MetalsEjectedMass;
        tot_ICS += galaxies[i].ICS;
        tot_ICSMetals += galaxies[i].MetalsICS;


        if(i != centralgal) {
            // satellite ejected gas goes to central ejected reservior
            galaxies[i].EjectedMass = galaxies[i].MetalsEjectedMass = 0.0;

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

   // Apply preventative feedback - this is the key addition
   double preventative_factor = calculate_preventative_feedback(centralgal, Zcurr, galaxies, run_params);
    
   // Modify infalling mass with both reionization and preventative feedback
   infallingMass = preventative_factor * reionization_modifier * run_params->BaryonFrac * 
                  galaxies[centralgal].Mvir - 
                  (tot_stellarMass + tot_coldMass + tot_hotMass + tot_ejected + tot_BHMass + tot_ICS);
    /* reionization_modifier * run_params->BaryonFrac * galaxies[centralgal].deltaMvir - newSatBaryons; */

    // the central galaxy keeps all the ejected mass
    galaxies[centralgal].EjectedMass = tot_ejected;
    galaxies[centralgal].MetalsEjectedMass = tot_ejectedMetals;

    if(galaxies[centralgal].MetalsEjectedMass > galaxies[centralgal].EjectedMass) {
        galaxies[centralgal].MetalsEjectedMass = galaxies[centralgal].EjectedMass;
    }

    if(galaxies[centralgal].EjectedMass < 0.0) {
        galaxies[centralgal].EjectedMass = galaxies[centralgal].MetalsEjectedMass = 0.0;
    }

    if(galaxies[centralgal].MetalsEjectedMass < 0.0) {
        galaxies[centralgal].MetalsEjectedMass = 0.0;
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
        (reionization_modifier * run_params->BaryonFrac * galaxies[gal].Mvir - (galaxies[gal].StellarMass + galaxies[gal].ColdGas + galaxies[gal].HotGas + galaxies[gal].EjectedMass + galaxies[gal].BlackHoleMass + galaxies[gal].ICS) ) / STEPS;
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
    if(infallingGas < 0.0 && galaxies[gal].EjectedMass > 0.0) {
        metallicity = get_metallicity(galaxies[gal].EjectedMass, galaxies[gal].MetalsEjectedMass);
        galaxies[gal].MetalsEjectedMass += infallingGas*metallicity;
        if(galaxies[gal].MetalsEjectedMass < 0.0) galaxies[gal].MetalsEjectedMass = 0.0;

        galaxies[gal].EjectedMass += infallingGas;
        if(galaxies[gal].EjectedMass < 0.0) {
            infallingGas = galaxies[gal].EjectedMass;
            galaxies[gal].EjectedMass = galaxies[gal].MetalsEjectedMass = 0.0;
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
double calculate_preventative_feedback(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    // Skip calculation if preventative feedback is disabled
    if (run_params->PreventativeFeedbackOn == 0) {
        return 1.0;
    }
    
    // Get halo virial velocity in km/s
    double vvir = galaxies[gal].Vvir;
    
    // Safety check for valid velocity
    if (vvir <= 0.0) {
        return 1.0;
    }
    
    // Much stronger redshift dependence - scales with (1+z)^2 for z>1
    double z_factor;
    if (z <= 1.0) {
        // Mild evolution at low redshift
        z_factor = pow(1.0 + z, run_params->PreventativeFeedbackZdep);
    } else {
        // Strong evolution at high redshift
        z_factor = pow(1.0 + 1.0, run_params->PreventativeFeedbackZdep) * 
                   pow((1.0 + z)/(1.0 + 1.0), 2.0);
    }
    
    // Calculate the critical velocity scale with enhanced redshift dependence
    double v_crit = run_params->PreventativeFeedbackVcrit * z_factor;
    
    // Apply mass-dependent suppression with a redshift-dependent floor
    // Minimum floor decreases with redshift - allowing stronger suppression at high-z
    double min_floor = 0.3 / (1.0 + 0.5 * (z - 1.0));
    if (min_floor < 0.05) min_floor = 0.05; // Don't go below 5%
    if (z <= 1.0) min_floor = 0.3; // Fix floor at low redshift
    
    // Calculate suppression factor with redshift-dependent minimum floor
    double f_suppress = min_floor + (1.0 - min_floor) / 
                       (1.0 + pow(v_crit/vvir, run_params->PreventativeFeedbackAlpha));
    
    // Apply additional suppression for intermediate-mass halos at high redshift
    // This targets halos in the 10^10-10^11 M☉ range that contribute to the excess
    if (z > 1.0 && vvir > 50.0 && vvir < 150.0) {
        double peak_suppress = 0.3 * (z - 1.0) / 2.0; // Up to 30% additional suppression at z=3
        if (peak_suppress > 0.5) peak_suppress = 0.5;
        
        // Gaussian-like suppression centered at 100 km/s
        double v_relative = (vvir - 100.0) / 50.0;
        double extra_suppress = peak_suppress * exp(-v_relative * v_relative);
        
        f_suppress *= (1.0 - extra_suppress);
    }
    
    // Ensure the suppression factor is between 0 and 1
    if (f_suppress < 0.05) f_suppress = 0.05; // Absolute minimum 5%
    if (f_suppress > 1.0) f_suppress = 1.0;
    
    return f_suppress;
}