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
    // double z = run_params->ZZ[galaxies[centralgal].SnapNum];
    
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

double get_cgm_transfer_efficiency(const int gal, struct GALAXY *galaxies, const struct params *run_params)
{
    const double z = run_params->ZZ[galaxies[gal].SnapNum];
    double base_efficiency = run_params->CGMTransferEfficiency;
    double original_efficiency = base_efficiency;
    
    // Cold streams: REDUCE transfer efficiency for small halos at high-z
    if (z > 0.0 && galaxies[gal].Mvir > 0.0) {
        double log_mass = log10(galaxies[gal].Mvir);
        
        if (log_mass < 2.5) {  // Below ~3×10^12 Msun/h
            // Small halos: gas stays in CGM longer (cold streams)
            double reduction_factor = exp(-0.5 * (2.5 - log_mass));  // Gentle reduction
            base_efficiency *= reduction_factor;
        }
    }
    
    if (base_efficiency < 0.01) base_efficiency = 0.01;  // Reasonable minimum
    
    // Debug output
    static int debug_counter = 0;
    debug_counter++;
    if (debug_counter % 10000 == 0) {
        printf("DEBUG CGM Transfer: z=%.2f, M=%.2e, original=%.3f, new=%.3f", 
               z, galaxies[gal].Mvir, original_efficiency, base_efficiency);
        
        if (z > 0.0 && galaxies[gal].Mvir > 0.0) {
            double log_mass = log10(galaxies[gal].Mvir);
            if (log_mass < 2.5) {
                printf(" [COLD STREAM REDUCTION]");
            } else {
                printf(" [HIGH-Z, MASSIVE HALO]");
            }
        } else {
            printf(" [LOW-Z OR NO MASS]");
        }
        printf("\n");
    }
    
    return base_efficiency;
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
    double base_transfer_rate = get_cgm_transfer_efficiency(centralgal, galaxies, run_params) * 
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

void mix_cgm_components(const int centralgal, const double dt, struct GALAXY *galaxies, 
                       const struct params *run_params)
{
    if(galaxies[centralgal].CGMgas <= 0.0) return;
    if(galaxies[centralgal].CGMgas_pristine <= 0.0) return;  // No pristine gas to mix
    
    // Calculate proper SAGE dynamical timescale using disk radius
    const double reff = 3.0 * galaxies[centralgal].DiskScaleRadius;
    const double dynamical_time = reff / galaxies[centralgal].Vvir;
    
    // Convert mixing timescale parameter to units of dynamical times
    // CGMMixingTimescale is in Gyr, convert to code units and normalize by t_dyn
    const double mixing_time_code_units = run_params->CGMMixingTimescale / run_params->UnitTime_in_Megayears * 1000.0; // Convert Gyr to code time units
    const double mixing_rate_per_dyn_time = dynamical_time / mixing_time_code_units;
    
    // Calculate how much pristine gas gets mixed this timestep
    double mixing_rate = mixing_rate_per_dyn_time * galaxies[centralgal].CGMgas_pristine / dynamical_time;
    double mixed_amount = mixing_rate * dt;
    
    // Don't mix more than available pristine gas
    if(mixed_amount > galaxies[centralgal].CGMgas_pristine) {
        mixed_amount = galaxies[centralgal].CGMgas_pristine;
    }
    
    if(mixed_amount > 0.0) {
        // Calculate average metallicity after mixing
        // Pristine gas has zero metallicity, enriched gas has some metallicity
        // const double current_enriched_metallicity = get_metallicity(galaxies[centralgal].CGMgas_enriched, 
        //                                                            galaxies[centralgal].MetalsCGMgas);
        
        // Move pristine gas to enriched component
        galaxies[centralgal].CGMgas_pristine -= mixed_amount;
        galaxies[centralgal].CGMgas_enriched += mixed_amount;
        
        // The mixed gas gets the average metallicity (pristine=0, so it's just diluted enriched metallicity)
        // No change to total metals since pristine gas has zero metals
        
        // Safety checks
        if(galaxies[centralgal].CGMgas_pristine < 0.0) galaxies[centralgal].CGMgas_pristine = 0.0;
    }
}