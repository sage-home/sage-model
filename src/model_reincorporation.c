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
    /* If mass-dependent delayed reincorporation is enabled, log some diagnostic information if in VERBOSE mode */
    #ifdef VERBOSE
    static int counter = 0;
    static int total_modified_galaxies = 0;
    static double total_reincorporated_mass = 0.0;
    static double min_reincorporation_mass = INFINITY;
    static double max_reincorporation_mass = -INFINITY;
    
    if(run_params->MassReincorporationOn && galaxies[centralgal].Mvir < run_params->CriticalReincMass) {
        counter++;
        total_modified_galaxies++;
    
        // Calculate mass-dependent factor
        double mass_dependent_factor = galaxies[centralgal].Mvir / run_params->CriticalReincMass;
        mass_dependent_factor = pow(mass_dependent_factor, run_params->ReincorporationMassExp);
        
        // Ensure factor is in sensible range
        if (mass_dependent_factor < run_params->MinReincorporationFactor)
            mass_dependent_factor = run_params->MinReincorporationFactor;
        if (mass_dependent_factor > 1.0)
            mass_dependent_factor = 1.0;
    
        // Track min and max masses modified
        if (galaxies[centralgal].Mvir < min_reincorporation_mass) 
            min_reincorporation_mass = galaxies[centralgal].Mvir;
        if (galaxies[centralgal].Mvir > max_reincorporation_mass) 
            max_reincorporation_mass = galaxies[centralgal].Mvir;
    
        // Periodically print detailed diagnostics
        if(counter % 100000 == 0) { 
            fprintf(stdout, "\n--- Mass-Dependent Reincorporation Diagnostics ---\n");
            fprintf(stdout, "Total Modified Galaxies: %d\n", total_modified_galaxies);
            fprintf(stdout, "Current Galaxy: HaloNr=%d\n", galaxies[centralgal].HaloNr);
            fprintf(stdout, "Halo Mass: %g (log10: %.2f)\n", 
                    galaxies[centralgal].Mvir, 
                    log10(galaxies[centralgal].Mvir));
            fprintf(stdout, "Critical Mass: %g (log10: %.2f)\n", 
                    run_params->CriticalReincMass, 
                    log10(run_params->CriticalReincMass));
            fprintf(stdout, "Mass Dependent Factor: %g\n", mass_dependent_factor);
            fprintf(stdout, "Reincorporation Mass Exponent: %g\n", run_params->ReincorporationMassExp);
            fprintf(stdout, "Min Reincorporation Factor: %g\n", run_params->MinReincorporationFactor);
            fprintf(stdout, "Ejected Mass Before: %g\n", galaxies[centralgal].EjectedMass);
            
            fprintf(stdout, "\nMass Range of Modified Galaxies:\n");
            fprintf(stdout, "Min Mass: %g (log10: %.2f)\n", 
                    min_reincorporation_mass, 
                    log10(min_reincorporation_mass));
            fprintf(stdout, "Max Mass: %g (log10: %.2f)\n", 
                    max_reincorporation_mass, 
                    log10(max_reincorporation_mass));
            
            fprintf(stdout, "\n--- End of Diagnostic Block ---\n");
        }
        
    }
    #endif
    
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
                
                #ifdef VERBOSE
                if(run_params->MassReincorporationOn && galaxies[centralgal].Mvir < run_params->CriticalReincMass) {
                    total_modified_galaxies++;
                    
                    // Track min and max masses modified
                    if (galaxies[centralgal].Mvir < min_reincorporation_mass) 
                        min_reincorporation_mass = galaxies[centralgal].Mvir;
                    if (galaxies[centralgal].Mvir > max_reincorporation_mass) 
                        max_reincorporation_mass = galaxies[centralgal].Mvir;
                }
                #endif
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
            
        #ifdef VERBOSE
        // Only print the mass-dependent diagnostics once per simulation
        // (previously was printing once per million galaxies)
        if(run_params->MassReincorporationOn == 1 && 
           counter == 1000000 && 
           total_modified_galaxies > 0) { 
            fprintf(stdout, "Mass-dep Reinc Summary: modified=%d mass_range=[%.2g-%.2g]\n",
                    total_modified_galaxies,
                    min_reincorporation_mass,
                    max_reincorporation_mass);
        }
        #endif
    }
}
