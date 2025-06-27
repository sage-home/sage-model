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
    
    const double Vcrit = 445.48 * run_params->inflowFactor;

    if(galaxies[centralgal].Vvir > Vcrit) {
        double base_inflow_rate = 
            (galaxies[centralgal].Vvir / Vcrit - 1.0) * 
            galaxies[centralgal].CGMgas / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir);

        double inflowed = base_inflow_rate * dt;

        if(inflowed > galaxies[centralgal].CGMgas)
            inflowed = galaxies[centralgal].CGMgas;

        if(inflowed > 0.0) {
            const double metallicity = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
            
            // NEW: Calculate component fractions before removing gas
            double pristine_fraction = 0.0;
            double enriched_fraction = 0.0;
            
            if(galaxies[centralgal].CGMgas > 0.0) {
                pristine_fraction = galaxies[centralgal].CGMgas_pristine / galaxies[centralgal].CGMgas;
                enriched_fraction = galaxies[centralgal].CGMgas_enriched / galaxies[centralgal].CGMgas;
            }
            
            double pristine_removed = inflowed * pristine_fraction;
            double enriched_removed = inflowed * enriched_fraction;
            
            // Remove from CGM (existing code)
            galaxies[centralgal].CGMgas -= inflowed;
            galaxies[centralgal].MetalsCGMgas -= metallicity * inflowed;
            
            // NEW: Also remove from components
            galaxies[centralgal].CGMgas_pristine -= pristine_removed;
            galaxies[centralgal].CGMgas_enriched -= enriched_removed;
            
            // Add to hot (existing code)
            galaxies[centralgal].HotGas += inflowed;
            galaxies[centralgal].MetalsHotGas += metallicity * inflowed;
            
            // Safety checks
            if(galaxies[centralgal].CGMgas_pristine < 0.0) galaxies[centralgal].CGMgas_pristine = 0.0;
            if(galaxies[centralgal].CGMgas_enriched < 0.0) galaxies[centralgal].CGMgas_enriched = 0.0;
        }
    }
}

void transfer_cgm_to_hot(const int centralgal, const double dt, struct GALAXY *galaxies, 
                        const struct params *run_params)
{
    if(galaxies[centralgal].CGMgas <= 0.0) return;
    
    // Calculate transfer rate following the same pattern as inflow_gas
    const double Vcrit = 445.48 * run_params->CGMTransferEfficiency;

    // Debug output for central galaxy
    // if(centralgal == 0) {
    //     printf("DEBUG Transfer: Vvir=%.1f, Vcrit=%.1f, CGMgas=%.6f, Rvir=%.6f\n",
    //            galaxies[centralgal].Vvir, Vcrit, galaxies[centralgal].CGMgas, galaxies[centralgal].Rvir);
    // }
    
    if(galaxies[centralgal].Vvir > Vcrit) {
        // Base transfer rate similar to reincorporation calculation
        double base_transfer_rate = 
            (galaxies[centralgal].Vvir / Vcrit - 1.0) * 
            galaxies[centralgal].CGMgas / (galaxies[centralgal].Rvir / galaxies[centralgal].Vvir);
            
        double transfer_amount = base_transfer_rate * dt;
        
        if(transfer_amount > galaxies[centralgal].CGMgas) {
            transfer_amount = galaxies[centralgal].CGMgas;
        }

        // if(centralgal == 0 && transfer_amount > 0.0) {
        //     printf("DEBUG Transfer: base_rate=%.6f, transfer_amount=%.6f\n", 
        //            base_transfer_rate, transfer_amount);
        // }
        
        if(transfer_amount > 0.0) {
            // Calculate fractions to transfer
            double pristine_fraction = 0.0;
            double enriched_fraction = 0.0;
            
            if(galaxies[centralgal].CGMgas > 0.0) {
                pristine_fraction = galaxies[centralgal].CGMgas_pristine / galaxies[centralgal].CGMgas;
                enriched_fraction = galaxies[centralgal].CGMgas_enriched / galaxies[centralgal].CGMgas;
            }
            
            double pristine_transfer = transfer_amount * pristine_fraction;
            double enriched_transfer = transfer_amount * enriched_fraction;
            
            // Calculate metallicity of transferred gas
            const double metallicity = get_metallicity(galaxies[centralgal].CGMgas, 
                                                      galaxies[centralgal].MetalsCGMgas);
            
            // Transfer from CGM to hot
            galaxies[centralgal].CGMgas -= transfer_amount;
            galaxies[centralgal].CGMgas_pristine -= pristine_transfer;
            galaxies[centralgal].CGMgas_enriched -= enriched_transfer;
            galaxies[centralgal].MetalsCGMgas -= metallicity * transfer_amount;
            
            galaxies[centralgal].HotGas += transfer_amount;
            galaxies[centralgal].MetalsHotGas += metallicity * transfer_amount;

            // NEW: Track CGM to hot transfer
            galaxies[centralgal].TransferRate_CGM_to_Hot += transfer_amount;
            
            // Safety checks
            if(galaxies[centralgal].CGMgas_pristine < 0.0) galaxies[centralgal].CGMgas_pristine = 0.0;
            if(galaxies[centralgal].CGMgas_enriched < 0.0) galaxies[centralgal].CGMgas_enriched = 0.0;
        }
    }
}