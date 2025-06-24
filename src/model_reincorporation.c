#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_reincorporation.h"
#include "model_misc.h"
#include "model_lowmass_suppression.h"

void reincorporate_gas(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    
    // Get current redshift for this galaxy
    double z = run_params->ZZ[galaxies[centralgal].SnapNum];
    
    // SN velocity is 630km/s, and the condition for reincorporation is that the
    // halo has an escape velocity greater than this, i.e. V_SN/sqrt(2) = 445.48km/s
    const double Vcrit = 445.48 * run_params->ReIncorporationFactor;

    if(galaxies[centralgal].Vvir > Vcrit) {
        // Traditional reincorporation calculation
        double base_reincorporation_rate = 
            (galaxies[centralgal].Vvir / Vcrit - 1.0) * 
            galaxies[centralgal].CGMgas / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir);

        // Apply scaling factors
        double total_scaling = 1.0;
        
        // Mass-dependent scaling (if enabled)
        if (run_params->MassReincorporationOn == 1) {
            // Only apply to halos below the critical mass
            if (galaxies[centralgal].Mvir < run_params->CriticalReincMass) {
                // Calculate mass-dependent factor
                double mass_dependent_factor = galaxies[centralgal].Mvir / run_params->CriticalReincMass;
                mass_dependent_factor = pow(mass_dependent_factor, run_params->ReincorporationMassExp);
                
                // Ensure factor is in sensible range
                if (mass_dependent_factor < run_params->MinReincorporationFactor)
                    mass_dependent_factor = run_params->MinReincorporationFactor;
                if (mass_dependent_factor > 1.0)
                    mass_dependent_factor = 1.0;
                
                // Apply to total scaling
                total_scaling *= mass_dependent_factor;
        
            }
        }
        
        // Redshift-dependent scaling (if enabled)
        if (run_params->RedshiftReincorporationOn == 1) {
            // Calculate redshift-dependent factor: slower reincorporation at high redshift
            double redshift_factor = pow(1.0 + z, -run_params->ReincorporationRedshiftExp);
            
            // Apply to total scaling
            total_scaling *= redshift_factor;
        }
        
        // Calculate final reincorporation amount
        double reincorporated = base_reincorporation_rate * total_scaling * dt;

        if(reincorporated > galaxies[centralgal].EjectedMass)
            reincorporated = galaxies[centralgal].EjectedMass;

        const double metallicity = get_metallicity(galaxies[centralgal].EjectedMass, galaxies[centralgal].MetalsEjectedMass);
        galaxies[centralgal].CGMgas -= reincorporated;
        galaxies[centralgal].MetalsCGMgas -= metallicity * reincorporated;
        galaxies[centralgal].HotGas += reincorporated;
        galaxies[centralgal].MetalsHotGas += metallicity * reincorporated;

        // Apply targeted suppression
        if (run_params->LowMassHighzSuppressionOn == 1) {
            double z = run_params->ZZ[galaxies[centralgal].SnapNum];
            double suppression = calculate_lowmass_suppression(centralgal, z, galaxies, run_params);
            reincorporated *= suppression;
        }
    }
}
