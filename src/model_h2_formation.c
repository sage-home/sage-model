#include <math.h>
#include "core_allvars.h"
#include "model_h2_formation.h"
#include "model_misc.h"

// Static counter for debug output - only print every 1000000th galaxy
static long galaxy_debug_counter = 0;

/**
 * init_gas_components - Initialize gas components in a galaxy
 * 
 * Sets the initial values for H2 and HI gas in a new galaxy
 */
void init_gas_components(struct GALAXY *g)
{
    // Initialize molecular and atomic hydrogen to zero
    g->H2_gas = 0.0;
    g->HI_gas = 0.0;
}

/**
 * gd14_sigma_norm - Calculate GD14 normalization surface density
 * This is the EXACT SHARK implementation from the gd14_sigma_norm function
 */
float gd14_sigma_norm(float d_mw, float u_mw)
{
    // g parameter calculation: g = sqrt(d_mw² + 0.0289)
    float g = sqrt(d_mw * d_mw + 0.02);
    
    // sigma_r1 calculation
    float sqrt_term = sqrt(0.01 + u_mw);
    float sigma_r1 = 50.0 / g * sqrt_term / (1.0 + 0.69 * sqrt_term);
    
    return sigma_r1; // Returns in M☉/pc²
}


float calculate_molecular_fraction_GD14(float gas_surface_density, float metallicity)
{
    // Early termination for edge cases
    if (gas_surface_density <= 0.0) {
        return 0.0;
    }
    
    // Step 1: Calculate d_mw (metallicity parameter)
    // d_mw = zgas (metallicity as absolute fraction, not relative to solar)
    float d_mw = metallicity;
    
    // Step 2: Calculate u_mw (surface density parameter)  
    // constants::sigma_gas_mw = 10 M☉/pc²
    const float sigma_gas_mw = 10.0; // M☉/pc² (SHARK's normalization)
    float u_mw = gas_surface_density / sigma_gas_mw;
    
    // Step 3: Calculate alpha (variable exponent)
    // alpha = 0.5 + 1/(1 + sqrt(u_mw * d_mw² / 600.0))
    float alpha = 0.5 + 1.0 / (1.0 + sqrt(u_mw * d_mw * d_mw / 600.0));
    
    // Step 4: Calculate normalization surface density
    // gd14_sigma_norm(d_mw, u_mw)
    float sigma_norm = gd14_sigma_norm(d_mw, u_mw);
    
    // Step 5: Calculate R_mol (molecular-to-atomic ratio)
    // rmol = (Sigma_gas / sigma_norm)^alpha
    float rmol = pow(gas_surface_density / sigma_norm, alpha);
    
    // Step 6: Convert to molecular fraction
    // fmol = rmol / (1 + rmol)
    float fmol = rmol / (1.0 + rmol);
    
    // Step 7: Apply bounds
    if (fmol > 1.0) {
        fmol = 1.0;
    } else if (fmol < 0.0) {
        fmol = 0.0;
    }
    
    return fmol;
}

float integrate_molecular_gas_radial(struct GALAXY *g, const struct params *run_params)
{
    if (g->ColdGas <= 0.0) {
        if (galaxy_debug_counter % 100 == 0) {
            printf("DEBUG RADIAL: No cold gas, returning 0\n");
        }
        return 0.0;
    }
    
    // re = cosmology->comoving_to_physical_size(rgas / constants::RDISK_HALF_SCALE, z)
    // where constants::RDISK_HALF_SCALE = 1.67
    float disk_scale_radius = g->DiskScaleRadius;
    
    if (disk_scale_radius <= 1.0e-6) {
        if (galaxy_debug_counter % 100 == 0) {
            printf("DEBUG RADIAL: Very small disk radius=%.2e, returning 0\n", disk_scale_radius);
        }
        return 0.0;
    }
    
    // Since our disk_scale_radius is the scale length, we need to convert
    const float h = run_params->Hubble_h;
    const float re_pc = disk_scale_radius * 1.0e6 / h / 1.67;

    // Get metallicity (as absolute fraction, not relative to solar - this is key!)
    float metallicity = 0.0;
    if (g->ColdGas > 0.0) {
        // zgas = galaxy.disk_gas.mass_metals / galaxy.disk_gas.mass
        // This gives absolute metallicity fraction, not relative to solar
        metallicity = g->MetalsColdGas / g->ColdGas; // Absolute metallicity fraction
    }
    
    // rmax = 5.0*re; (integrates to 5 × half-mass radius)
    const int N_RADIAL_BINS = 20;
    const float MAX_RADIUS_FACTOR = 5.0; 
    const float dr = MAX_RADIUS_FACTOR * re_pc / N_RADIAL_BINS;
    
    // Calculate gas surface density at center (half-mass radius)
    // Sigma_gas = cosmology->comoving_to_physical_mass(mcold) / constants::PI2 / (re * re)
    float gas_surface_density_center = 0.0;
    if (re_pc > 0.0) {
        // For exponential disk: Σ_gas(0) = M_gas / (2π × r_e²)
        float disk_area_pc2 = 2.0 * M_PI * re_pc * re_pc; // Note: 2π for half-mass radius
        gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
    }

    // Integration = PI2 * fmol(Sigma_gas, Sigma_stars, props->zgas, r) * Sigma_gas * r
    float total_molecular_gas = 0.0;
    
    for (int i = 0; i < N_RADIAL_BINS; i++) {
    
        float radius_in_half_mass_radii = (i + 0.5) * MAX_RADIUS_FACTOR / N_RADIAL_BINS;
        float radius_pc = radius_in_half_mass_radii * re_pc;
        
        // Exponential profile: Sigma_gas = props->sigma_gas0 * exp(-r / props->re)
        float exp_factor = exp(-radius_in_half_mass_radii);
        float local_gas_density = gas_surface_density_center * exp_factor; // M☉/pc²
        
        // Area of this annular ring
        float ring_area_pc2 = 2.0 * M_PI * radius_pc * dr;
        
        // Gas mass in this ring (in SAGE units: 10^10 M☉/h)
        float ring_gas_mass = (local_gas_density * ring_area_pc2) / (1.0e10 / h);
        
        // Calculate molecular fraction
        float molecular_fraction = 0.0;

        // sigma_HI_crit threshold HERE during integration
        const float sigma_HI_crit = 10.0; // M☉/pc² (SHARK's default from parameters)
        if (local_gas_density >= sigma_HI_crit) {
            molecular_fraction = calculate_molecular_fraction_GD14(
                local_gas_density, metallicity);
        }
        
        // Add molecular gas from this ring
        float ring_mol_gas = molecular_fraction * ring_gas_mass;
        total_molecular_gas += ring_mol_gas;
        
    }
    
    // Mass conservation check
    if (total_molecular_gas > g->ColdGas * 0.95) { // Max 95% of cold gas can be molecular
        total_molecular_gas = g->ColdGas * 0.95;
    }

    // Comparing fraction to fraction
    if (total_molecular_gas > 0.95) {
        total_molecular_gas = 0.95;
    }
    
    return total_molecular_gas;
}


float calculate_bulge_molecular_gas(struct GALAXY *g, const struct params *run_params)
{
    // Check if we have any bulge
    if (g->BulgeMass <= 0.0) {
        return 0.0;
    }

    // Estimate bulge gas based on bulge-to-total ratio
    float bulge_to_total = g->BulgeMass / (g->StellarMass > 0.0 ? g->StellarMass : 1.0);
    
    // Estimate bulge gas
    float bulge_gas = bulge_to_total * 0.5 * g->ColdGas; // 50% of proportional share

    if (bulge_gas <= 0.0) {
        return 0.0;
    }
    
    // Calculate bulge properties
    const float h = run_params->Hubble_h;
    float bulge_radius = g->DiskScaleRadius * 0.2; // Typical bulge size
    float bulge_radius_pc = bulge_radius * 1.0e6 / h / 1.67; // Convert to physical size in pc
    
    // Calculate bulge gas surface density
    float bulge_gas_surface_density = 0.0;
    if (bulge_radius_pc > 0.0) {
        float bulge_area_pc2 = M_PI * bulge_radius_pc * bulge_radius_pc;
        bulge_gas_surface_density = (bulge_gas * 1.0e10 / h) / bulge_area_pc2; // M☉/pc²
    }
    
    // Get bulge metallicity (absolute fraction)
    float metallicity = 0.0;
    if (g->BulgeMass > 0.0) {
        // zgas = galaxy.bulge_gas.mass_metals / galaxy.bulge_gas.mass
        metallicity = g->MetalsBulgeMass / g->BulgeMass; // Absolute metallicity fraction
    } else if (g->ColdGas > 0.0) {
        metallicity = g->MetalsColdGas / g->ColdGas; // Fallback to cold gas metallicity
    }
    
    float molecular_fraction = calculate_molecular_fraction_GD14(
        bulge_gas_surface_density, metallicity);
    
    float bulge_molecular_gas = bulge_gas * molecular_fraction;
    
    return bulge_molecular_gas;
}

/**
 * apply_environmental_effects - Enhanced environmental effects with CGM transfer
 * 
 * This improved version:
 * - Begins environmental effects at lower halo masses
 * - Has more gradual transition with halo mass
 * - Accounts for orbit/position within halo
 * - Transfers stripped mass to central galaxy's CGM (NEW)
 */
void apply_environmental_effects(struct GALAXY *g, struct GALAXY *galaxies, 
                                 int central_gal_index, 
                                 const struct params *run_params) 
{
    // Skip if no H2 gas
    if (g->H2_gas <= 0.0) return;
    
    // 1. Apply to all galaxies, but stronger in satellites
    float type_factor = 1.0;
    if (g->Type == 0) {  // Central galaxy
        type_factor = 0.0;  // Environmental effects are 100% weaker in centrals
    } else if (g->Type == 1) {  // Satellite with subhalo
        type_factor = 1.0;  // Full effect
    } else if (g->Type == 2) {  // Orphan satellite
        type_factor = 1.2;  // 20% stronger effect for orphan satellites (no protection from subhalo)
    }
    
    // 2. Start environmental effects at lower mass and make transition more gradual
    float env_strength = 0.0;
    
    // Get central halo mass - use different approaches based on galaxy type
    float central_mvir = 0.0;
    if (g->Type == 0) {
        central_mvir = g->Mvir;  // For centrals, use own virial mass
    } else {
        // For satellites, use stored CentralMvir
        central_mvir = g->CentralMvir;
        if (central_mvir <= 0.0) return;  // Safety check
    }
    
    // Convert to solar masses
    float central_mass = central_mvir * 1.0e10 / run_params->Hubble_h;
    float log_mass = log10(central_mass > 0.0 ? central_mass : 1.0);
    
    // Begin effect at lower mass (10^12 M⊙) with more gradual increase
    // Previous version started at 10^13 M⊙
    if (log_mass > 12.0) {
        // More gradual scaling with mass
        env_strength = 0.2 + 0.3 * (log_mass - 12.0);  // 20% base effect + 30% per dex
        
        // Cap maximum effect
        if (env_strength > 0.9) env_strength = 0.9;
    } 
    // Even galaxies in groups (10^11-10^12 M⊙) experience mild environmental effects
    else if (log_mass > 11.0) {
        env_strength = 0.05 + 0.15 * (log_mass - 11.0);  // 5% base effect + 15% scaling
    }
    
    // 3. Scale with user parameter
    // env_strength *= 25.0;
    // 4. Apply type-dependent scaling
    env_strength *= type_factor;
    
    // 5. Account for orbit/position effects (time since infall)
    if (g->Type > 0) {  // Only for satellites
        // Use merger time as proxy for orbital phase
        if (g->MergTime > 0.0 && g->infallVvir > 0.0) {
            // Recently accreted satellites experience less environmental effects
            float orbit_phase = 1.0 - g->MergTime / 3.0;  // Normalized to ~1 after 3 Gyr
            if (orbit_phase < 0.0) orbit_phase = 0.0;
            if (orbit_phase > 1.0) orbit_phase = 1.0;
            
            env_strength *= orbit_phase;  // Reduce effect for recent infall
        }
    }
    
    // Apply the effect - remove H2 gas
    if (env_strength > 0.0) {
        float h2_affected = g->H2_gas * env_strength;
        
        // 30% is completely removed, 70% converted to HI
        float h2_removed = h2_affected * 0.3;
        float h2_to_hi = h2_affected * 0.7;
        
        // Calculate metallicity before modifying gas masses
        float metallicity = (g->ColdGas > 0.0) ? g->MetalsColdGas / g->ColdGas : 0.0;
        
        // Update gas components in current galaxy
        g->H2_gas -= h2_affected;
        g->HI_gas += h2_to_hi;
        g->ColdGas -= h2_removed;  // Only the removed part reduces total cold gas
        
        // Remove metals proportionally for the stripped gas
        g->MetalsColdGas -= h2_removed * metallicity;
        
        // Transfer stripped mass to central galaxy's CGM
        if (g->Type > 0 && central_gal_index >= 0) {  // Only for satellites with valid central
            galaxies[central_gal_index].CGMgas += h2_removed;
            galaxies[central_gal_index].MetalsCGMgas += h2_removed * metallicity;
        }
        
        // Ensure non-negative values
        if (g->H2_gas < 0.0) g->H2_gas = 0.0;
        if (g->HI_gas < 0.0) g->HI_gas = 0.0;
        if (g->ColdGas < 0.0) g->ColdGas = 0.0;
        if (g->MetalsColdGas < 0.0) g->MetalsColdGas = 0.0;
    }
}

/**
 * update_gas_components - Enhanced gas component update with real disk radius
 * 
 * This function now uses SAGE's real disk radius (calculated via get_disk_radius 
 * in model_misc.c during galaxy initialization) instead of any approximation.
 * The GD14 molecular fraction prescription but uses
 * the proper disk radius as calculated by SAGE's physics.
 */
void update_gas_components(struct GALAXY *g, const struct params *run_params)
{
    // Increment the galaxy counter for debug purposes
    galaxy_debug_counter++;
    
    // Early termination - if no cold gas
    if(g->ColdGas <= 0.0) {
        g->H2_gas = 0.0;
        g->HI_gas = 0.0;
        return;
    }
    
    // Early termination - if disk radius is effectively zero
    if(g->DiskScaleRadius <= 1.0e-6) {
        if (galaxy_debug_counter % 10000 == 0) {
            printf("DEBUG MAIN: Very small DiskScaleRadius=%.2e, setting H2=0\n", 
                   g->DiskScaleRadius);
        }
        g->H2_gas = 0.0;
        g->HI_gas = g->ColdGas;
        return;
    }
    
    float total_molecular_gas = 0.0;
    
    if (run_params->SFprescription == 1) {
        // GD14 model with SHARK-exact implementation
        // if (galaxy_debug_counter % 10000 == 0) {
        //     printf("\nDEBUG MAIN SHARK: Starting SHARK-exact GD14 for galaxy #%ld\n", galaxy_debug_counter);
        //     printf("  ColdGas=%.2e, StellarMass=%.2e, DiskScaleRadius=%.2e\n", 
        //            g->ColdGas, g->StellarMass, g->DiskScaleRadius);
        // }
        
        // Calculate disk molecular gas through SHARK-exact radial integration
        // float disk_molecular_gas = integrate_molecular_gas_radial(g, run_params);
        
        // // Calculate bulge molecular gas using SHARK-exact method
        // float bulge_molecular_gas = 0.0;
        // if (g->BulgeMass > 0.0) {
        //     bulge_molecular_gas = calculate_bulge_molecular_gas(g, run_params);
        // }
        
        // // Total molecular gas - THIS IS THE KEY FIX
        // total_molecular_gas = disk_molecular_gas + bulge_molecular_gas;
        const float h = run_params->Hubble_h;
        const float re_pc = g->DiskScaleRadius * 1.0e6 / h; // Half-mass radius in pc
        float disk_area_pc2 = 2.0 * M_PI * re_pc * re_pc; // Note: 2π for half-mass radius
        float gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²

        total_molecular_gas = calculate_molecular_fraction_GD14(
            gas_surface_density_center, 
            g->MetalsColdGas / g->ColdGas); // Use absolute metallicity fraction

        // Mass conservation check
        // Comparing fraction to fraction
        if (total_molecular_gas > 0.95) {
            total_molecular_gas = 0.95;
        }
    }
    else {
        total_molecular_gas = 0.0; // Don't use fallback for debugging
    }
    
    // Update gas components - CRITICAL: Use the actual calculated values
    g->H2_gas = total_molecular_gas * g->ColdGas;
    g->HI_gas = (1.0 - total_molecular_gas) * g->ColdGas;
    
    // Apply bounds checking
    if(g->H2_gas > g->ColdGas) {
        g->H2_gas = g->ColdGas;
        g->HI_gas = 0.0;
    }

    // Final sanity checks (as SHARK does)
    if(g->H2_gas < 0.0) g->H2_gas = 0.0;
    if(g->HI_gas < 0.0) g->HI_gas = 0.0;
    
    // Mass conservation check with detailed debugging
    if(g->H2_gas + g->HI_gas > g->ColdGas * 1.001) {  
        float total = g->H2_gas + g->HI_gas;
        float scale = g->ColdGas / total;
        if (galaxy_debug_counter % 100 == 0) {
            printf("DEBUG MAIN: Mass conservation issue - H2+HI=%.2e > ColdGas=%.2e\n", 
                   total, g->ColdGas);
            printf("  Applying scale factor: %.6f\n", scale);
            printf("  Metallicity debug: MetalsColdGas=%.4e, ColdGas=%.4e, metallicity=%.4e\n", 
                   g->MetalsColdGas, g->ColdGas, (g->ColdGas > 0.0 ? g->MetalsColdGas / g->ColdGas : 0.0));
        }
        g->H2_gas *= scale;
        g->HI_gas *= scale;
    }
}

void diagnose_cgm_h2_interaction(struct GALAXY *g, const struct params *run_params)
{
    // Only diagnose every 9,000,000 galaxies to avoid spam
    galaxy_debug_counter++;
    
    if (g->ColdGas <= 0.0) return;
    
    if (galaxy_debug_counter % 900000 == 0) {
        printf("========================================\n");
        printf("DEBUG CGM-H2 DIAGNOSTIC for galaxy #%ld\n", galaxy_debug_counter);
        
        // Basic galaxy properties
        printf("Galaxy Properties:\n");
    printf("  ColdGas: %.2e, StellarMass: %.2e, BulgeMass: %.2e\n", 
           g->ColdGas, g->StellarMass, g->BulgeMass);
    float h2_frac_cold = g->H2_gas / g->ColdGas;
    float h2_frac_proper = (g->H2_gas + g->HI_gas > 0.0) ? g->H2_gas / (g->H2_gas + g->HI_gas) : 0.0;
    printf("  H2_gas: %.2e, HI_gas: %.2e\n", g->H2_gas, g->HI_gas);
    printf("  f_H2 = H2/ColdGas = %.4f\n", h2_frac_cold);
    printf("  f_H2 = H2/(H2+HI) = %.4f\n", h2_frac_proper);
    
    // Metallicity assessment
    float metallicity = g->MetalsColdGas / g->ColdGas;
    float metallicity_solar = metallicity / 0.02; // Assuming Z_sun = 0.02
    printf("  Metallicity: %.4f (%.1f%% solar)\n", metallicity, metallicity_solar * 100);
    
    // CGM properties
    printf("\nCGM Properties:\n");
    printf("  CGMgas: %.2e, HotGas: %.2e\n", g->CGMgas, g->HotGas);
    printf("  CGM/ColdGas ratio: %.2f\n", g->CGMgas / g->ColdGas);
    printf("  CGM/HotGas ratio: %.2f\n", g->CGMgas / (g->HotGas > 0 ? g->HotGas : 1e-10));
    
    // CGM metallicity
    float cgm_metallicity = 0.0;
    if (g->CGMgas > 0.0) {
        cgm_metallicity = g->MetalsCGMgas / g->CGMgas;
        printf("  CGM metallicity: %.4f (%.1f%% solar)\n", 
               cgm_metallicity, cgm_metallicity / 0.02 * 100);
    }
    
    // Gas cycle efficiency assessment
    printf("\nGas Cycle Assessment:\n");
    
    // Check if galaxy is gas-rich or gas-poor
    float gas_fraction = g->ColdGas / (g->ColdGas + g->StellarMass);
    printf("  Gas fraction: %.3f ", gas_fraction);
    if (gas_fraction > 0.5) {
        printf("(Gas-rich - good for sustained SF)\n");
    } else if (gas_fraction > 0.1) {
        printf("(Moderate gas - balanced evolution)\n");
    } else {
        printf("(Gas-poor - may need more infall)\n");
    }
    
    // Assess CGM reservoir size
    float cgm_ratio = g->CGMgas / g->ColdGas;
    printf("  CGM reservoir: ");
    if (cgm_ratio > 10.0) {
        printf("VERY LARGE (%.1fx cold gas - may be too slow transfer)\n", cgm_ratio);
    } else if (cgm_ratio > 3.0) {
        printf("LARGE (%.1fx cold gas - good reservoir)\n", cgm_ratio);
    } else if (cgm_ratio > 1.0) {
        printf("MODERATE (%.1fx cold gas - balanced)\n", cgm_ratio);
    } else {
        printf("SMALL (%.1fx cold gas - may need slower transfer)\n", cgm_ratio);
    }
    
    // H2 formation assessment
    printf("\nH2 Formation Assessment:\n");
    float expected_h2_low = 0.001; // 0.1% for very low metallicity
    float expected_h2_high = 0.1;  // 10% for moderate metallicity
    
    float actual_h2_frac = g->H2_gas / g->ColdGas;
    float actual_h2_frac_proper = (g->H2_gas + g->HI_gas > 0.0) ? g->H2_gas / (g->H2_gas + g->HI_gas) : 0.0;
    printf("  f_H2 = H2/ColdGas = %.4f ", actual_h2_frac);
    printf("  f_H2 = H2/(H2+HI) = %.4f ", actual_h2_frac_proper);
    
    if (actual_h2_frac < expected_h2_low) {
        printf("(Very low - typical for Z < 0.3 Z_sun)\n");
    } else if (actual_h2_frac < expected_h2_high) {
        printf("(Low-moderate - typical for Z ~ 0.3-1.0 Z_sun)\n");
    } else {
        printf("(High - typical for Z > 1.0 Z_sun)\n");
    }
    
    // Metallicity-H2 consistency check
    printf("  Metallicity-H2 consistency: ");
    if (metallicity_solar < 0.3 && actual_h2_frac < 0.01) {
        printf("✓ CONSISTENT (Low Z → Low f_H2)\n");
    } else if (metallicity_solar > 0.7 && actual_h2_frac > 0.05) {
        printf("✓ CONSISTENT (High Z → High f_H2)\n");
    } else if (metallicity_solar < 0.3 && actual_h2_frac > 0.1) {
        printf("⚠ INCONSISTENT (Low Z but High f_H2 - check parameters)\n");
    } else if (metallicity_solar > 1.0 && actual_h2_frac < 0.01) {
        printf("⚠ INCONSISTENT (High Z but Low f_H2 - check parameters)\n");
    } else {
        printf("~ BORDERLINE (In transition regime)\n");
    }
    
    // Parameter effectiveness assessment
    printf("\nCGM Parameter Effectiveness:\n");
    printf("  Current settings (your values):\n");
    printf("    CGMInfallFraction: %.2f\n", run_params->CGMInfallFraction);
    printf("    CGMTransferEfficiency: %.3f\n", run_params->CGMTransferEfficiency);
    printf("    CGMPristineFraction: %.2f\n", run_params->CGMPristineFraction);
    printf("    CGMMixingTimescale: %.1f Gyr\n", run_params->CGMMixingTimescale);
    
    // Recommendations based on observations
    printf("\n  Recommendations:\n");
    if (cgm_ratio > 20.0) {
        printf("    - Consider increasing CGMTransferEfficiency (current: %.3f → suggest: %.3f)\n", 
               run_params->CGMTransferEfficiency, run_params->CGMTransferEfficiency * 1.5);
    }
    if (metallicity_solar > 1.5 && actual_h2_frac < 0.05) {
        printf("    - Metal-rich galaxy with low f_H2 - check if CGM is too metal-poor\n");
    }
    if (gas_fraction < 0.05 && cgm_ratio < 1.0) {
        printf("    - Gas-starved galaxy - consider decreasing CGMTransferEfficiency\n");
    }
    if (actual_h2_frac > 0.3) {
        printf("    - Very high f_H2 - may need more pristine infall or faster mixing\n");
    }
    
    printf("=====================================\n\n");
    } // End of conditional check for galaxy_debug_counter % 9000000 == 0
}