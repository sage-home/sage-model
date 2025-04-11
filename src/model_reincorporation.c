#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_reincorporation.h"
#include "model_misc.h"

void reincorporate_gas(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    /* If mass-dependent delayed reincorporation is enabled, log some diagnostic information if in VERBOSE mode */
#ifdef VERBOSE
    static int counter = 0;
    if(run_params->MassReincorporationOn && galaxies[centralgal].Mvir < run_params->CriticalReincMass) {
        counter++;
        if(counter % 50000 == 0) { // Only print once every 50,000 galaxies
            fprintf(stdout, "Delayed reincorporation active for galaxy: HaloNr=%d, Mvir=%g (critical mass=%g)\n",
                    galaxies[centralgal].HaloNr, galaxies[centralgal].Mvir, run_params->CriticalReincMass);
        }
    }
#endif
    
    // SN velocity is 630km/s, and the condition for reincorporation is that the
    // halo has an escape velocity greater than this, i.e. V_SN/sqrt(2) = 445.48km/s
    const double Vcrit = 445.48 * run_params->ReIncorporationFactor;

    if(galaxies[centralgal].Vvir > Vcrit) {
        // Traditional reincorporation calculation
        double base_reincorporation_rate = 
            (galaxies[centralgal].Vvir / Vcrit - 1.0) * 
            galaxies[centralgal].EjectedMass / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir);

        // Apply mass-dependent scaling for low-mass halos
        // This follows Henriques et al. (2015) approach where reincorporation is proportional to 1/Mhalo
        double mass_dependent_factor = 1.0;
        
        if (run_params->MassReincorporationOn == 1) {
            // Only apply to halos below the critical mass (set to 10^10.5 M_sun by default)
            if (galaxies[centralgal].Mvir < run_params->CriticalReincMass) {
                // Mass-dependent reincorporation time: larger halos reincorporate faster
                // Scale inversely with halo mass relative to the critical mass
                mass_dependent_factor = galaxies[centralgal].Mvir / run_params->CriticalReincMass;
                
                // Apply exponent to control strength of mass dependence
                mass_dependent_factor = pow(mass_dependent_factor, run_params->ReincorporationMassExp);
                
                // Ensure factor is in sensible range
                if (mass_dependent_factor < run_params->MinReincorporationFactor)
                    mass_dependent_factor = run_params->MinReincorporationFactor;
                if (mass_dependent_factor > 1.0)
                    mass_dependent_factor = 1.0;
            }
        }
        
        // Calculate final reincorporation amount
        double reincorporated = base_reincorporation_rate * mass_dependent_factor * dt;

        if(reincorporated > galaxies[centralgal].EjectedMass)
            reincorporated = galaxies[centralgal].EjectedMass;

        const double metallicity = get_metallicity(galaxies[centralgal].EjectedMass, galaxies[centralgal].MetalsEjectedMass);
        galaxies[centralgal].EjectedMass -= reincorporated;
        galaxies[centralgal].MetalsEjectedMass -= metallicity * reincorporated;
        galaxies[centralgal].HotGas += reincorporated;
        galaxies[centralgal].MetalsHotGas += metallicity * reincorporated;
    }
}
