#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_inflow.h"
#include "model_misc.h"

void inflow_gas(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    
    // Get current redshift for this galaxy
    double z = run_params->ZZ[galaxies[centralgal].SnapNum];
    
    // SN velocity is 630km/s, and the condition for reincorporation is that the
    // halo has an escape velocity greater than this, i.e. V_SN/sqrt(2) = 445.48km/s
    const double Vcrit = 445.48 * run_params->inflowFactor;

    if(galaxies[centralgal].Vvir > Vcrit) {
        // Traditional inflow calculation
        double base_inflow_rate = 
            (galaxies[centralgal].Vvir / Vcrit - 1.0) * 
            galaxies[centralgal].CGMgas / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir);

        // Apply scaling factors
        double total_scaling = 1.0;
        // Calculate final inflow amount
        double inflowed = base_inflow_rate * total_scaling * dt;

        if(inflowed > galaxies[centralgal].CGMgas)
            inflowed = galaxies[centralgal].CGMgas;

        const double metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
        galaxies[centralgal].CGMgas -= inflowed;
        galaxies[centralgal].MetalsCGMgas -= metallicity * inflowed;
        galaxies[centralgal].HotGas += inflowed;
        galaxies[centralgal].MetalsHotGas += metallicity * inflowed;
}
}