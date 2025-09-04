#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_reincorporation.h"
#include "model_misc.h"
#include "model_infall.h"

// Debug counter for regime tracking
static long reincorporation_debug_counter = 0;

void reincorporate_gas(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{

    // SN velocity is 630km/s, and the condition for reincorporation is that the
    // halo has an escape velocity greater than this, i.e. V_SN/sqrt(2) = 445.48km/s
    const double Vcrit = 445.48 * run_params->ReIncorporationFactor;

    if(galaxies[centralgal].Vvir > Vcrit) {
        double reincorporated =
            ( galaxies[centralgal].Vvir / Vcrit - 1.0 ) *
            galaxies[centralgal].CGMgas / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir) * dt;

        if(reincorporated > galaxies[centralgal].CGMgas)
            reincorporated = galaxies[centralgal].CGMgas;

        const double metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
        galaxies[centralgal].CGMgas -= reincorporated;
        galaxies[centralgal].MetalsCGMgas -= metallicity * reincorporated;
        galaxies[centralgal].HotGas += reincorporated;
        galaxies[centralgal].MetalsHotGas += metallicity * reincorporated;
        galaxies[centralgal].ReincorporatedGas = reincorporated;
    }
}

void reincorporate_gas_regime(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    // Increment debug counter
    reincorporation_debug_counter++;

    // If CGM toggle is off, use original behavior
    if(run_params->CgmOn == 0) {
        reincorporate_gas(centralgal, dt, galaxies, run_params);
        return;
    }

    // In regime-aware system with no ejection, reincorporation is disabled
    // There is no ejected gas to reincorporate since all feedback stays in appropriate reservoirs
    // Debug output every 50,000 galaxies to confirm reincorporation is disabled
    if(reincorporation_debug_counter % 50000 == 0) {
        double halo_mass_1e13 = galaxies[centralgal].Mvir / 1000.0;
        const char* regime = (halo_mass_1e13 < run_params->CgmMassThreshold) ? "CGM" : "HOT";
        printf("DEBUG REINCORP [gal %ld]: Mvir=%.2e (%.2e x10^13), regime=%s, reinc=0.00e+00 (DISABLED)\n",
               reincorporation_debug_counter, galaxies[centralgal].Mvir, halo_mass_1e13, regime);
    }

    // Set reincorporated gas to zero for accounting
    galaxies[centralgal].ReincorporatedGas = 0.0;

    // No actual reincorporation occurs - all gas cycles remain within appropriate reservoirs
    return;
}