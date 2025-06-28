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
    
    // Calculate proper SAGE dynamical timescale using disk radius
    // reff = 3.0 * DiskScaleRadius = (3.0/√2) × λ × Rvir  
    const double reff = 3.0 * galaxies[centralgal].DiskScaleRadius;
    const double dynamical_time = reff / galaxies[centralgal].Vvir;
    
    // Transfer rate: fraction of CGM per dynamical time
    double base_transfer_rate = run_params->CGMTransferEfficiency * 
                               galaxies[centralgal].CGMgas / dynamical_time;
            
    double transfer_amount = base_transfer_rate * dt;
    
    if(transfer_amount > galaxies[centralgal].CGMgas) {
        transfer_amount = galaxies[centralgal].CGMgas;
    }
    
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

        // Track CGM to hot transfer
        galaxies[centralgal].TransferRate_CGM_to_Hot += transfer_amount;
        
        // Safety checks
        if(galaxies[centralgal].CGMgas_pristine < 0.0) galaxies[centralgal].CGMgas_pristine = 0.0;
        if(galaxies[centralgal].CGMgas_enriched < 0.0) galaxies[centralgal].CGMgas_enriched = 0.0;
    }
}