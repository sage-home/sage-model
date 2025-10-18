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
    // SN velocity is 630km/s, and the condition for reincorporation is that the
    // halo has an escape velocity greater than this, i.e. V_SN/sqrt(2) = 445.48km/s
    const double Vcrit = 445.48 * run_params->ReIncorporationFactor;

    if(galaxies[centralgal].Vvir > Vcrit) {
        double reincorporated =
            ( galaxies[centralgal].Vvir / Vcrit - 1.0 ) *
            galaxies[centralgal].EjectedMass / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir) * dt;

        if(reincorporated > galaxies[centralgal].EjectedMass)
            reincorporated = galaxies[centralgal].EjectedMass;

        const double metallicity = get_metallicity(galaxies[centralgal].EjectedMass, galaxies[centralgal].MetalsEjectedMass);
        
        // Remove from ejected reservoir (same for all regimes)
        galaxies[centralgal].EjectedMass -= reincorporated;
        galaxies[centralgal].MetalsEjectedMass -= metallicity * reincorporated;
        
        // Add to appropriate hot reservoir (regime-dependent)
        if(run_params->CGMrecipeOn == 1) {
            if(galaxies[centralgal].Regime == 0) {
                // CGM-regime: reincorporate to CGM
                galaxies[centralgal].CGMgas += reincorporated;
                galaxies[centralgal].MetalsCGMgas += metallicity * reincorporated;
            } else {
                // Hot-ICM-regime: reincorporate to HotGas
                galaxies[centralgal].HotGas += reincorporated;
                galaxies[centralgal].MetalsHotGas += metallicity * reincorporated;
            }
        } else {
            // Original SAGE behavior: reincorporate to HotGas
            galaxies[centralgal].HotGas += reincorporated;
            galaxies[centralgal].MetalsHotGas += metallicity * reincorporated;
        }
    }
}