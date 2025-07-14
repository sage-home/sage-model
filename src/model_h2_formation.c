#include <math.h>
#include "core_allvars.h"
#include "model_h2_formation.h"
#include "model_misc.h"

static long galaxy_debug_counter = 0;

void init_gas_components(struct GALAXY *g)
{
    g->H2_gas = 0.0;
    g->HI_gas = 0.0;
}

double gd14_sigma_norm(float d_mw, float u_mw)
{
    // g parameter calculation: g = sqrt(d_mw² + 0.0289)
    float g = sqrt(d_mw * d_mw + 0.02);
    
    // sigma_r1 calculation
    float sqrt_term = sqrt(0.01 + u_mw);
    float sigma_r1 = 50.0 / g * sqrt_term / (1.0 + 0.69 * sqrt_term);
    
    return sigma_r1;
}


double calculate_molecular_fraction_GD14(float gas_surface_density, float metallicity)
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
    const float sigma_gas_mw = 5.0; // M☉/pc² (SHARK's normalization)
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

    // CRITICAL: Prevent R_mol=0 which causes f_mol=0
    rmol = fmax(rmol, 0.001);  // Minimum R_mol = 0.001
    
    // Step 6: Convert to molecular fraction
    // fmol = rmol / (1 + rmol)
    float fmol = rmol / (1.0 + rmol);

    // extern long galaxy_debug_counter;
    if (galaxy_debug_counter % 500000 == 0) {
        printf("DEBUG GD14: Σgas=%.1f M☉/pc², Z=%.4f, d_mw=%.4f, u_mw=%.2f, α=%.3f, Σnorm=%.1f, R_mol=%.3f, f_mol=%.3f\n",
               gas_surface_density, metallicity, d_mw, u_mw, alpha, sigma_norm, rmol, fmol);
    }
    
    // Step 7: Apply bounds
    if (fmol > 1.0) {
        fmol = 1.0;
    } else if (fmol < 0.0) {
        fmol = 0.0;
    }
    
    return fmol;
}

double calculate_midplane_pressure_BR06(float sigma_gas, float sigma_stars, float radius_pc)
{
    // Early termination for edge cases
    if (sigma_gas <= 0.0 || radius_pc <= 0.0) {
        return 0.0;
    }
    
    // BR06 hardcoded parameters (from paper)
    const float v_g = 8.0;          // km/s, gas velocity dispersion (BR06 uses 8 km/s)
    
    // Stellar disk scale height from Kregel et al. (2002): h* = 0.14 * R
    const float h_star_pc = 0.14 * radius_pc; // pc
    
    // BR06 Equation (5) EXACTLY as written in paper:
    // P_ext/k = 272 cm⁻³ K × (Σ_gas/M_⊙ pc⁻²) × (Σ_*/M_⊙ pc⁻²)^0.5 × (v_g/km s⁻¹) × (h_*/pc)^-0.5
    float pressure = 272.0 * sigma_gas * sqrt(sigma_stars) * v_g / sqrt(h_star_pc);
    
    return pressure; // K cm⁻³
}

double calculate_molecular_fraction_BR06(float gas_surface_density, float stellar_surface_density,
                                       float radius_pc)
{
    // Calculate midplane pressure using corrected formula
    float pressure = calculate_midplane_pressure_BR06(gas_surface_density, stellar_surface_density, 
                                                     radius_pc);
    
    if (pressure <= 0.0) {
        return 0.0;
    }
    
    // BR06 parameters from Table 1 in paper
    const float P0 = 1.7e4;    // Reference pressure, K cm⁻³
    const float alpha = 0.8;   // Power law index (they call it α, you called it beta)
    
    // Apply pressure threshold - below this, no molecular gas forms
    const float P_threshold = 1000.0; // K cm⁻³ (reasonable physical threshold)
    if (pressure < P_threshold) {
        return 0.0;
    }
    
    // BR06 Equation (11): R_mol = (P/P₀)^α
    float pressure_ratio = pressure / P0;
    float R_mol = pow(pressure_ratio, alpha);
    
    // Convert to molecular fraction: f_mol = R_mol / (1 + R_mol)
    float f_mol = R_mol / (1.0 + R_mol);
    
    // Apply physical bounds
    if (f_mol > 0.95) f_mol = 0.95; // Max 95% molecular
    if (f_mol < 0.0) f_mol = 0.0;   // No negative fractions
    
    return f_mol;
}

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
        if (galaxy_debug_counter % 100 == 0) {
            printf("DEBUG MAIN: Very small DiskScaleRadius=%.2e, setting H2=0\n", 
                   g->DiskScaleRadius);
        }
        g->H2_gas = 0.0;
        g->HI_gas = g->ColdGas;
        return;
    }
    
    float total_molecular_gas = 0.0;
    
    if (run_params->SFprescription == 1) {
        
        const float h = run_params->Hubble_h;
        const float re_pc = g->DiskScaleRadius * 1.0e6 / h; // Half-mass radius in pc
        float disk_area_pc2 = 2.0 * M_PI * re_pc * re_pc; // Note: 2π for half-mass radius
        float gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²

        total_molecular_gas = calculate_molecular_fraction_GD14(
            gas_surface_density_center, 
            g->MetalsColdGas / g->ColdGas); // Use absolute metallicity fraction

        // Mass conservation check
        if (total_molecular_gas > 0.95) {
            total_molecular_gas = 0.95;
        }
    }
    else if (run_params->SFprescription == 2) {
        // NEW: BR06 pressure-based model with hardcoded parameters
        const float h = run_params->Hubble_h;
        const float re_pc = g->DiskScaleRadius * 1.0e6 / h; // Convert Mpc/h to pc
        
        // Calculate surface densities at the disk scale radius
        // Use half-mass radius area (factor of 2π, not 4π)
        float disk_area_pc2 = 2.0 * M_PI * re_pc * re_pc;
        
        // Convert SAGE internal units to M☉/pc²
        float gas_surface_density = (g->ColdGas * 1.0e10 / h) / disk_area_pc2;      // M☉/pc²
        float stellar_surface_density = (g->StellarMass * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
        
        total_molecular_gas = calculate_molecular_fraction_BR06(
            gas_surface_density, stellar_surface_density, re_pc);
            
        // Debug output for BR06 model
        if (galaxy_debug_counter % 500000 == 0) {
            printf("DEBUG BR06: Σgas=%.1f M☉/pc², Σstar=%.1f M☉/pc², R=%.1f pc, f_mol=%.3f\n",
                   gas_surface_density, stellar_surface_density, re_pc, total_molecular_gas);
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

    // Final sanity checks
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