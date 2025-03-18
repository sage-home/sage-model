#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../core/core_allvars.h"
#include "../core/core_parameter_views.h"

#include "../physics/model_reincorporation.h"
#include "../physics/model_misc.h"

/*
 * Main gas reincorporation function
 * 
 * Reincorporates gas from the ejected reservoir back into the hot gas component.
 * Uses parameter views for improved modularity.
 */
void reincorporate_gas(const int centralgal, const double dt, struct GALAXY *galaxies, 
                      const struct reincorporation_params_view *reincorp_params)
{
    // SN velocity is 630km/s, and the condition for reincorporation is that the
    // halo has an escape velocity greater than this, i.e. V_SN/sqrt(2) = 445.48km/s
    const double Vcrit = 445.48 * reincorp_params->ReIncorporationFactor;

    if(galaxies[centralgal].Vvir > Vcrit) {
        double reincorporated =
            ( galaxies[centralgal].Vvir / Vcrit - 1.0 ) *
            galaxies[centralgal].EjectedMass / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir) * dt;

        if(reincorporated > galaxies[centralgal].EjectedMass)
            reincorporated = galaxies[centralgal].EjectedMass;

        const double metallicity = get_metallicity(galaxies[centralgal].EjectedMass, galaxies[centralgal].MetalsEjectedMass);
        galaxies[centralgal].EjectedMass -= reincorporated;
        galaxies[centralgal].MetalsEjectedMass -= metallicity * reincorporated;
        galaxies[centralgal].HotGas += reincorporated;
        galaxies[centralgal].MetalsHotGas += metallicity * reincorporated;
    }
}

/*
 * Compatibility wrapper for reincorporate_gas
 * 
 * Provides backwards compatibility with the old interface while
 * using the new parameter view-based implementation internally.
 */
void reincorporate_gas_compat(const int centralgal, const double dt, struct GALAXY *galaxies, 
                             const struct params *run_params)
{
    struct reincorporation_params_view reincorp_params;
    initialize_reincorporation_params_view(&reincorp_params, run_params);
    reincorporate_gas(centralgal, dt, galaxies, &reincorp_params);
}
