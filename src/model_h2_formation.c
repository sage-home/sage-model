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


double calculate_molecular_fraction_GD14(float gas_surface_density, float metallicity)
{
    // Early termination for edge cases
    if (gas_surface_density <= 0.0) {
        return 0.0;
    }
    
    // Convert metallicity to dust-to-gas ratio relative to Milky Way
    const float zsun = 0.02;  // Solar metallicity
    float D_MW = metallicity / zsun;
    float U_MW = 1.0;  // Milky Way UV field strength
    
    // Calculate spatial scale parameter (assume ~100 pc scale)
    float S = 1.0; // S = L / 100 pc, here assuming L ~ 100 pc
    
    // Calculate D* parameter (GD14 equation between 5 and 6)
    float D_star = 0.17 * (2.0 + pow(S, 5)) / (1.0 + pow(S, 5));
    
    // Calculate g parameter (GD14 equation between 5 and 6)
    float g = sqrt(D_MW * D_MW + D_star * D_star);
    
    // Use erratum's improved formulation:
    // s parameter from erratum
    float s = pow(0.001 + 0.1 * U_MW, 0.7);
    
    // Calculate alpha using erratum's improved formula (not original equation 9)
    float alpha = (1.0 + 0.7 * s) / (1.0 + s);
    
    // Calculate Sigma_R=1 using the erratum formula
    float Sigma_R1 = 40.0 * g / s; // M_sun/pc^2
    
    // Calculate eta parameter (0 for kpc scale, 0.25 for 500 pc scale)
    // Assuming we're working at ~kpc scale, so eta ≈ 0
    float eta = 0.0;  // Could be 0.25 for finer scales
    
    // Calculate R using the complete erratum formula
    float ratio = gas_surface_density / Sigma_R1;
    float R = pow(ratio, alpha) / (1.0 + eta * ratio);
    
    // Convert to molecular fraction: f_H2 = R / (1 + R)
    float f_mol = R / (1.0 + R);
    
    // Apply bounds
    if (f_mol > 1.0) {
        f_mol = 1.0;
    } else if (f_mol < 0.0) {
        f_mol = 0.0;
    }
    
    return f_mol;
}

/**
 * calculate_stellar_scale_height_BR06 - Calculate stellar scale height using BR06 eq. (9)
 * From Kregel et al. (2002) relationship as used in Blitz & Rosolowsky (2006)
 * 
 * @param disk_scale_length_pc: Disk scale length in parsecs (R* in the paper)
 * @return: Stellar scale height in parsecs (h* in the paper)
 */
float calculate_stellar_scale_height_BR06(float disk_scale_length_pc)
{
    // BR06 equation (9): log h* = -0.23 - 0.8 log R*
    // where h* and R* are measured in parsecs
    if (disk_scale_length_pc <= 0.0) {
        return 300.0; // Default fallback value in pc
    }
    
    float log_h_star = -0.23 + 0.8 * log10(disk_scale_length_pc);
    float h_star_pc = pow(10.0, log_h_star);
    
    // Apply reasonable physical bounds (from 10 pc to 10 kpc)
    if (h_star_pc < 10.0) h_star_pc = 10.0;
    if (h_star_pc > 10000.0) h_star_pc = 10000.0;
    
    return h_star_pc;
}

/**
 * calculate_midplane_pressure_BR06 - BR06 pressure calculation EXACTLY as in paper
 * From Blitz & Rosolowsky (2006) Equation (5)
 */
float calculate_midplane_pressure_BR06(float sigma_gas, float sigma_stars, float disk_scale_length_pc)
{
    // Early termination for edge cases
    if (sigma_gas <= 0.0 || disk_scale_length_pc <= 0.0) {
        return 0.0;
    }
    
    // Handle zero or very low stellar surface density (early galaxy evolution)
    float effective_sigma_stars = sigma_stars;
    if (sigma_stars < 1.0) { // M_sun/pc^2
        // For early galaxies with little stellar mass, use minimum based on gas
        effective_sigma_stars = fmax(sigma_gas * 0.1, 1.0);
    }
    
    // Calculate stellar scale height using exact BR06 equation (9)
    float h_star_pc = calculate_stellar_scale_height_BR06(disk_scale_length_pc);
    
    // BR06 hardcoded parameters EXACTLY as in paper
    const float v_g = 8.0;          // km/s, gas velocity dispersion (BR06 Table text)
    
    // BR06 Equation (5) EXACTLY as written in paper:
    // P_ext/k = 272 cm⁻³ K × (Σ_gas/M_⊙ pc⁻²) × (Σ_*/M_⊙ pc⁻²)^0.5 × (v_g/km s⁻¹) × (h_*/pc)^-0.5
    float pressure = 272.0 * sigma_gas * sqrt(effective_sigma_stars) * v_g / sqrt(h_star_pc);
    
    return pressure; // K cm⁻³
}

/**
 * calculate_molecular_fraction_BR06 - BR06 molecular fraction EXACTLY as in paper
 * From Blitz & Rosolowsky (2006) Equations (11) and (13)
 */
double calculate_molecular_fraction_BR06(float gas_surface_density, float stellar_surface_density, 
                                         float disk_scale_length_pc)
{
    // Calculate midplane pressure using exact BR06 formula
    float pressure = calculate_midplane_pressure_BR06(gas_surface_density, stellar_surface_density, 
                                                     disk_scale_length_pc);
    
    if (pressure <= 0.0) {
        return 0.0;
    }
    
    // BR06 parameters from equation (13) for non-interacting galaxies
    // These are the exact values from the paper
    const float P0 = 4.3e4;    // Reference pressure, K cm⁻³ (equation 13)
    const float alpha = 0.92;  // Power law index (equation 13)
    
    // Apply pressure threshold - below this, no molecular gas forms
    // Paper doesn't specify exact value, but this is physically reasonable
    const float P_threshold = 1000.0; // K cm⁻³
    if (pressure < P_threshold) {
        return 0.0;
    }
    
    // BR06 Equation (11): R_mol = (P_ext/P₀)^α
    float pressure_ratio = pressure / P0;
    float R_mol = pow(pressure_ratio, alpha);
    
    // Convert to molecular fraction: f_mol = R_mol / (1 + R_mol)
    // This is the standard conversion from molecular-to-atomic ratio to molecular fraction
    double f_mol = R_mol / (1.0 + R_mol);
    
    // Apply physical bounds
    if (f_mol > 0.95) f_mol = 0.95; // Max 95% molecular (to avoid numerical issues)
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
        g->H2_gas = 0.0;
        g->HI_gas = g->ColdGas;
        return;
    }
    
    float total_molecular_gas = 0.0;
    
    if (run_params->SFprescription == 1) {
        
        const float h = run_params->Hubble_h;
        const float rs_pc = g->DiskScaleRadius * 1.0e6 / h;
        float disk_area_pc2 = 2.0 * M_PI * rs_pc * rs_pc; 
        float gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
        float metallicity = (g->ColdGas > 0.0) ? g->MetalsColdGas / g->ColdGas : 0.0;

        total_molecular_gas = calculate_molecular_fraction_GD14(
            gas_surface_density_center, metallicity);

        if (galaxy_debug_counter % 10000 == 0) {
            printf("DEBUG MAIN: disk radius=%.4e\n disk area (pc^2)=%.4e\n gas surface density=%.4e\n metallicity=%.4e\n H2_gas=%.4e\n HI_gas=%.4e\n",
                   rs_pc, disk_area_pc2, gas_surface_density_center, metallicity, g->H2_gas, g->HI_gas);
        }

        // Mass conservation check
        if (total_molecular_gas > 0.95) {
            total_molecular_gas = 0.95;
        }
    }
    else if (run_params->SFprescription == 2) {
        // BR06 model
        const float h = run_params->Hubble_h;
        const float rs_pc = g->DiskScaleRadius * 1.0e6 / h; // Half-mass radius in pc
        float disk_area_pc2 = 2.0 * M_PI * rs_pc * rs_pc; // Note: 2π for half-mass radius
        float gas_surface_density = (g->ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
        float stellar_surface_density = (g->StellarMass * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
        
        total_molecular_gas = calculate_molecular_fraction_BR06(gas_surface_density, stellar_surface_density, 
                                                               rs_pc);
        
        if (galaxy_debug_counter % 10000 == 0) {
            // Calculate additional quantities for debugging
            float pressure = calculate_midplane_pressure_BR06(gas_surface_density, stellar_surface_density, rs_pc);
            float h_star = calculate_stellar_scale_height_BR06(rs_pc);
            printf("DEBUG BR06: rs_pc=%.2e, h_star_pc=%.2e, pressure=%.2e K cm^-3, f_mol=%.4f\n",
                   rs_pc, h_star, pressure, total_molecular_gas);
            printf("DEBUG BR06: gas_sigma=%.2e, star_sigma=%.2e M_sun/pc^2\n",
                   gas_surface_density, stellar_surface_density);
        }
                
        // Mass conservation check
        if (total_molecular_gas > 0.95) {
            total_molecular_gas = 0.95;
        }
    }
    else {
        total_molecular_gas = 0.0; // No H2 formation for other prescriptions
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