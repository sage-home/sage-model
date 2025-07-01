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

// Minimum surface density for efficient H2 formation (Msun/pc^2 in internal units)
static const float MIN_SURFACE_DENSITY = 10.0;
// Minimum pressure threshold for H2 formation (dimensionless)
static const float MIN_PRESSURE_NORM = 1e-3;

// Calculate molecular fraction based on mid-plane pressure and environmental factors
float calculate_H2_fraction(const float surface_density, const float metallicity, 
    const float DiskRadius, const struct params *run_params) 
{
    const float disk_area = M_PI * DiskRadius * DiskRadius;
    if (disk_area <= 0.0 || surface_density <= 0.0) {
        return 0.0;
    }

    // Calculate midplane pressure
    const float P_mid = M_PI/2 * run_params->G * surface_density * surface_density;
    const float P_0_internal = 5.93e-12 / run_params->UnitPressure_in_cgs;
    const float P_norm = P_mid / P_0_internal;

    if (P_norm < MIN_PRESSURE_NORM) {
        return 0.0;
    }

    // Adjusted molecular fraction formula
    float f_H2 = 1.0 / (1.0 + pow(P_norm, -0.92 * run_params->H2FractionExponent));

    // Surface density scaling
    if (surface_density < MIN_SURFACE_DENSITY) {
        f_H2 *= surface_density / MIN_SURFACE_DENSITY;
    }

    // Adjusted metallicity dependence
    if (metallicity > 0.0) {
        if (metallicity < 0.1) {
            f_H2 *= pow(metallicity / 0.1, 0.3);  // Shallower scaling at low Z
        } else {
            f_H2 *= pow(metallicity, 0.2);
        }
    }

    // Apply calibrated normalization factor
    f_H2 *= run_params->H2FractionFactor;

    // Ensure fraction stays within bounds
    if (f_H2 < 0.0) f_H2 = 0.0;
    if (f_H2 > 1.0) f_H2 = 1.0;

    return f_H2;
}

// Calculate molecular fraction using Krumholz & Dekel (2012) model
float calculate_H2_fraction_KD12(const float surface_density, const float metallicity, const float clumping_factor) 
{
    if (surface_density <= 0.0) {
        return 0.0;
    }
    
    // Metallicity normalized to solar (assuming solar metallicity = 0.02)
    float Zp = metallicity / 0.02;
    if (Zp < 0.01) Zp = 0.01;  // Set floor to prevent numerical issues
    
    // Apply clumping factor to get the compressed surface density
    float Sigma_comp = clumping_factor * surface_density;
    
    // Calculate dust optical depth parameter
    float tau_c = 0.066 * Sigma_comp * Zp;
    
    // Self-shielding parameter chi (from Krumholz & Dekel 2012, Eq. 2)
    float chi = 0.77 * (1.0 + 3.1 * pow(Zp, 0.365));
    
    // Compute s parameter (from Krumholz, McKee & Tumlinson 2009, Eq. 91)
    float s = log(1.0 + 0.6 * chi) / (0.6 * tau_c);
    
    // Molecular fraction (Krumholz, McKee & Tumlinson 2009, Eq. 93)
    float f_H2;
    if (s < 2.0) {
        f_H2 = 1.0 - 0.75 * s / (1.0 + 0.25 * s);
    } else {
        f_H2 = 0.0;
    }
    
    // Ensure fraction stays within bounds
    if (f_H2 < 0.0) f_H2 = 0.0;
    if (f_H2 > 1.0) f_H2 = 1.0;
    
    return f_H2;
}

/**
 * calculate_midplane_pressure - Calculate midplane pressure based on gas and stellar density
 * 
 * @gas_density: Gas surface density (M_sun/pc^2)
 * @stellar_density: Stellar surface density (M_sun/pc^2)
 * @radius: Galactocentric radius (kpc)
 * @stellar_scale_height: Stellar disk scale height (kpc), or 0 to use default
 * 
 * Returns: Midplane pressure in model units
 */
float calculate_midplane_pressure(float gas_density, float stellar_density, 
                                float radius, float stellar_scale_height)
{
    // Constants
    const float G_CONSTANT = 4.302e-3;  // in pc*M_sun^-1*(km/s)^2
    const float GAS_VELOCITY_DISPERSION = 10.0;  // km/s, typical for neutral ISM
    
    // Calculate stellar scale height if not provided
    if (stellar_scale_height <= 0.0) {
        stellar_scale_height = 0.14 * radius;  // Typical relation from observations
        if (stellar_scale_height < 0.05) {
            stellar_scale_height = 0.05;  // Minimum to avoid division by zero
        }
    }
    
    // Calculate stellar velocity dispersion
    float stellar_velocity_dispersion = 0.0;
    if (stellar_density > 0.0 && stellar_scale_height > 0.0) {
        // From vertical equilibrium, using pi*G approximation
        stellar_velocity_dispersion = sqrt(M_PI * G_CONSTANT * stellar_scale_height * stellar_density);
    }
    
    // Pressure calculation
    float pressure = 0.0;
    
    // P = (pi/2)*G*Sigma_gas*(Sigma_gas + sqrt(gas_disp/stellar_disp)*Sigma_stars)
    if (gas_density > 0.0) {
        float stellar_term = 0.0;
        
        if (stellar_density > 0.0 && stellar_velocity_dispersion > 0.0) {
            stellar_term = sqrt(GAS_VELOCITY_DISPERSION / stellar_velocity_dispersion) * stellar_density;
        }
        
        pressure = (M_PI/2.0) * G_CONSTANT * gas_density * (gas_density + stellar_term);
    }
    
    return pressure;
}

/**
 * gd14_sigma_norm - Calculate GD14 normalization surface density
 * This is the EXACT SHARK implementation from the gd14_sigma_norm function
 */
float gd14_sigma_norm(float d_mw, float u_mw)
{
    // SHARK's exact g parameter calculation: g = sqrt(d_mw² + 0.0289)
    float g = sqrt(d_mw * d_mw + 0.02);
    
    // SHARK's exact sigma_r1 calculation
    float sqrt_term = sqrt(0.01 + u_mw);
    float sigma_r1 = 50.0 / g * sqrt_term / (1.0 + 0.69 * sqrt_term);
    
    // Note: SHARK multiplies by constants::MEGA² to convert to M☉/Mpc², 
    // but since we're working in M☉/pc², we don't need this conversion
    
    return sigma_r1; // Returns in M☉/pc²
}

/**
 * calculate_molecular_fraction_GD14_SHARK_EXACT - EXACT SHARK implementation
 * This follows SHARK's fmol function exactly for the GD14 case
 */
float calculate_molecular_fraction_GD14(float gas_surface_density, float metallicity)
{
    // Early termination for edge cases (as SHARK does)
    if (gas_surface_density <= 0.0) {
        return 0.0;
    }
    
    // SHARK's exact GD14 implementation from the fmol function:
    // else if (parameters.model == StarFormationParameters::GD14){
    //     //Galaxy parameters
    //     double d_mw = zgas;
    //     double u_mw = Sigma_gas / constants::sigma_gas_mw;
    
    // Step 1: Calculate d_mw (metallicity parameter)
    // SHARK uses: d_mw = zgas (metallicity as absolute fraction, not relative to solar)
    float d_mw = metallicity;
    
    // Step 2: Calculate u_mw (surface density parameter)  
    // SHARK uses: constants::sigma_gas_mw = 10 M☉/pc²
    const float sigma_gas_mw = 10.0; // M☉/pc² (SHARK's normalization)
    float u_mw = gas_surface_density / sigma_gas_mw;
    
    // Step 3: Calculate alpha (variable exponent)
    // SHARK's exact formula: alpha = 0.5 + 1/(1 + sqrt(u_mw * d_mw² / 600.0))
    float alpha = 0.5 + 1.0 / (1.0 + sqrt(u_mw * d_mw * d_mw / 600.0));
    
    // Step 4: Calculate normalization surface density
    // SHARK uses: gd14_sigma_norm(d_mw, u_mw)
    float sigma_norm = gd14_sigma_norm(d_mw, u_mw);
    
    // Step 5: Calculate R_mol (molecular-to-atomic ratio)
    // SHARK's exact formula: rmol = (Sigma_gas / sigma_norm)^alpha
    float rmol = pow(gas_surface_density / sigma_norm, alpha);
    
    // Step 6: Convert to molecular fraction
    // SHARK's exact formula: fmol = rmol / (1 + rmol)
    float fmol = rmol / (1.0 + rmol);
    
    // Step 7: Apply bounds (as SHARK does in its fmol function)
    if (fmol > 1.0) {
        fmol = 1.0;
    } else if (fmol < 0.0) {
        fmol = 0.0;
    }
    
    // Debug output every 50,000 galaxies
    if (galaxy_debug_counter % 100000 == 0) {
        printf("DEBUG GD14 SHARK EXACT (galaxy #%ld):\n", galaxy_debug_counter);
        printf("  Input: gas_surf=%.2e M☉/pc², metallicity=%.4f\n", gas_surface_density, metallicity);
        printf("  Step 1 - d_mw: %.4f\n", d_mw);
        printf("  Step 2 - u_mw: %.4f (gas_surf/%.1f)\n", u_mw, sigma_gas_mw);
        printf("  Step 3 - alpha: %.4f (0.5 + 1/(1 + sqrt(%.4f * %.4f² / 600)))\n", alpha, u_mw, d_mw);
        printf("  Step 4 - sigma_norm: %.2f M☉/pc²\n", sigma_norm);
        printf("  Step 5 - rmol: %.4f ((%.2e / %.2f)^%.4f)\n", rmol, gas_surface_density, sigma_norm, alpha);
        printf("  Step 6 - f_H2: %.4f (%.4f / (1 + %.4f))\n", fmol, rmol, rmol);
        printf("  ----------------------------------------\n");
    }
    
    return fmol;
}

// /**
//  * integrate_molecular_gas_radial_SHARK_EXACT - SHARK-style radial integration
//  * This follows SHARK's molecular_hydrogen function structure exactly
//  */
// float integrate_molecular_gas_radial(struct GALAXY *g, const struct params *run_params)
// {
//     if (g->ColdGas <= 0.0) {
//         // if (galaxy_debug_counter % 100000 == 0) {
//         //     printf("DEBUG RADIAL SHARK: No cold gas, returning 0\n");
//         // }
//         return 0.0;
//     }
    
//     // SHARK uses: re = cosmology->comoving_to_physical_size(rgas / constants::RDISK_HALF_SCALE, z)
//     // where constants::RDISK_HALF_SCALE = 1.67
//     float disk_scale_radius = g->DiskScaleRadius; // This is already the scale radius
    
//     if (disk_scale_radius <= 1.0e-6) {
//         // if (galaxy_debug_counter % 100000 == 0) {
//         //     printf("DEBUG RADIAL SHARK: Very small disk radius=%.2e, returning 0\n", disk_scale_radius);
//         // }
//         return 0.0;
//     }
    
//     // SHARK conversion: re (half-mass radius) = rgas / 1.67
//     // Since our disk_scale_radius is the scale length, we need to convert
//     const float h = run_params->Hubble_h;
//     const float re_pc = disk_scale_radius * 1.0e6 / h ; // Half-mass radius in pc
    
//     // Get metallicity (as absolute fraction, not relative to solar - this is key!)
//     float metallicity = 0.0;
//     if (g->ColdGas > 0.0) {
//         // SHARK uses: zgas = galaxy.disk_gas.mass_metals / galaxy.disk_gas.mass
//         // This gives absolute metallicity fraction, not relative to solar
//         metallicity = g->MetalsColdGas / g->ColdGas; // Absolute metallicity fraction
//     }
    
//     // if (galaxy_debug_counter % 100000 == 0) {
//     //     printf("\nDEBUG RADIAL SHARK: Starting integration\n");
//     //     printf("  ColdGas: %.2e, metallicity: %.4f (absolute)\n", g->ColdGas, metallicity);
//     //     printf("  DiskScaleRadius: %.2e Mpc/h, re_pc: %.2e pc\n", disk_scale_radius, re_pc);
//     // }
    
//     // SHARK integration parameters
//     // SHARK uses: double rmax = 5.0*re; (integrates to 5 × half-mass radius)
//     const int N_RADIAL_BINS = 20; // SHARK uses adaptive integration, but we'll use fixed bins
//     const float MAX_RADIUS_FACTOR = 5.0; // SHARK integrates to 5 × half-mass radius
    
//     const float dr = MAX_RADIUS_FACTOR * re_pc / N_RADIAL_BINS;
    
//     // Calculate gas surface density at center (half-mass radius)
//     // SHARK uses: Sigma_gas = cosmology->comoving_to_physical_mass(mcold) / constants::PI2 / (re * re)
//     float gas_surface_density_center = 0.0;
//     if (re_pc > 0.0) {
//         // For exponential disk: Σ_gas(0) = M_gas / (2π × r_e²)
//         float disk_area_pc2 = 2.0 * M_PI * re_pc * re_pc; // Note: 2π for half-mass radius
//         gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
//     }
    
//     // if (galaxy_debug_counter % 100000 == 0) {
//     //     printf("  gas_surface_density_center: %.2e M☉/pc²\n", gas_surface_density_center);
//     // }
    
//     // SHARK-style radial integration
//     // SHARK integrates: PI2 * fmol(Sigma_gas, Sigma_stars, props->zgas, r) * Sigma_gas * r
//     float total_molecular_gas = 0.0;
    
//     for (int i = 0; i < N_RADIAL_BINS; i++) {
//         // Radius in half-mass radii (SHARK uses this scaling)
//         float radius_in_half_mass_radii = (i + 0.5) * MAX_RADIUS_FACTOR / N_RADIAL_BINS;
//         float radius_pc = radius_in_half_mass_radii * re_pc;
        
//         // SHARK uses exponential profile: Sigma_gas = props->sigma_gas0 * exp(-r / props->re)
//         float exp_factor = exp(-radius_in_half_mass_radii);
//         float local_gas_density = gas_surface_density_center * exp_factor; // M☉/pc²
        
//         // Area of this annular ring
//         float ring_area_pc2 = 2.0 * M_PI * radius_pc * dr;
        
//         // Gas mass in this ring (in SAGE units: 10^10 M☉/h)
//         float ring_gas_mass = (local_gas_density * ring_area_pc2) / (1.0e10 / h);
        
//         // Calculate molecular fraction using SHARK-exact GD14
//         float molecular_fraction = 0.0;

//         // SHARK applies sigma_HI_crit threshold HERE during integration
//         const float sigma_HI_crit = 10.0; // M☉/pc² (SHARK's default from parameters)
//         if (local_gas_density >= sigma_HI_crit) {
//             molecular_fraction = calculate_molecular_fraction_GD14(
//                 local_gas_density, metallicity);
//         }
//         // If local_gas_density < sigma_HI_crit, molecular_fraction stays 0.0
        
//         // Add molecular gas from this ring
//         float ring_mol_gas = molecular_fraction * ring_gas_mass;
//         total_molecular_gas += ring_mol_gas;
        
//         // if (i < 3 && galaxy_debug_counter % 100000 == 0) { // Debug first few rings
//         //     printf("  Ring %d: r=%.2f re, local_gas=%.2e M☉/pc², mol_frac=%.4f, ring_mol=%.3e\n", 
//         //            i, radius_in_half_mass_radii, local_gas_density, molecular_fraction, ring_mol_gas);
//         // }
//     }
    
//     // Mass conservation check (SHARK does this)
//     if (total_molecular_gas > g->ColdGas * 0.95) { // Max 95% of cold gas can be molecular
//         // if (galaxy_debug_counter % 100000 == 0) {
//         //     printf("  WARNING: H2 would be %.3f of cold gas, capping at 95%%\n", 
//         //            total_molecular_gas / g->ColdGas);
//         // }
//         total_molecular_gas = g->ColdGas * 0.95;
//     }
    
//     // if (galaxy_debug_counter % 100000 == 0) {
//     //     printf("  Final molecular gas: %.2e (fraction of cold gas: %.3f)\n", 
//     //            total_molecular_gas, total_molecular_gas / g->ColdGas);
//     //     printf("END DEBUG RADIAL SHARK\n\n");
//     // }
    
//     return total_molecular_gas;
// }

// /**
//  * calculate_bulge_molecular_gas_SHARK_EXACT - SHARK-style bulge H2 calculation
//  * SHARK treats bulge and disk separately in get_molecular_gas function
//  */
// float calculate_bulge_molecular_gas(struct GALAXY *g, const struct params *run_params)
// {
//     // Check if we have any bulge
//     if (g->BulgeMass <= 0.0) {
//         // if (galaxy_debug_counter % 100000 == 0) {
//         //     printf("DEBUG BULGE SHARK: No bulge mass, returning 0\n");
//         // }
//         return 0.0;
//     }
    
//     // SHARK estimates bulge gas based on bulge-to-total ratio
//     float bulge_to_total = g->BulgeMass / (g->StellarMass > 0.0 ? g->StellarMass : 1.0);
    
//     // Estimate bulge gas (SHARK doesn't specify exact method, so we use reasonable approach)
//     float bulge_gas = bulge_to_total * 0.3 * g->ColdGas; // 30% of proportional share
    
//     if (bulge_gas <= 0.0) {
//         return 0.0;
//     }
    
//     // Calculate bulge properties
//     const float h = run_params->Hubble_h;
//     float bulge_radius = g->DiskScaleRadius * 0.2; // Typical bulge size
//     float bulge_radius_pc = bulge_radius * 1.0e6 / h;
    
//     // Calculate bulge gas surface density
//     float bulge_gas_surface_density = 0.0;
//     if (bulge_radius_pc > 0.0) {
//         float bulge_area_pc2 = M_PI * bulge_radius_pc * bulge_radius_pc;
//         bulge_gas_surface_density = (bulge_gas * 1.0e10 / h) / bulge_area_pc2; // M☉/pc²
//     }
    
//     // Get bulge metallicity (absolute fraction)
//     float metallicity = 0.0;
//     if (g->BulgeMass > 0.0) {
//         // SHARK uses: zgas = galaxy.bulge_gas.mass_metals / galaxy.bulge_gas.mass
//         metallicity = g->MetalsBulgeMass / g->BulgeMass; // Absolute metallicity fraction
//     } else if (g->ColdGas > 0.0) {
//         metallicity = g->MetalsColdGas / g->ColdGas; // Fallback to cold gas metallicity
//     }
    
//     // if (galaxy_debug_counter % 100000 == 0) {
//     //     printf("DEBUG BULGE SHARK: bulge_gas=%.2e, gas_surf_dens=%.2e, metallicity=%.4f\n",
//     //            bulge_gas, bulge_gas_surface_density, metallicity);
//     // }
    
//     // Use SHARK-exact GD14 calculation
//     float molecular_fraction = calculate_molecular_fraction_GD14(
//         bulge_gas_surface_density, metallicity);
    
//     float bulge_molecular_gas = bulge_gas * molecular_fraction;
    
//     // if (galaxy_debug_counter % 100000 == 0) {
//     //     printf("DEBUG BULGE SHARK: molecular_fraction=%.4f, bulge_H2=%.2e\n", 
//     //            molecular_fraction, bulge_molecular_gas);
//     // }
    
//     return bulge_molecular_gas;
// }

/**
 * update_gas_components - Enhanced gas component update with real disk radius
 * 
 * This function now uses SAGE's real disk radius (calculated via get_disk_radius 
 * in model_misc.c during galaxy initialization) instead of any approximation.
 * The GD14 molecular fraction prescription matches SHARK exactly, but uses
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
        // if (galaxy_debug_counter % 100000 == 0) {
        //     printf("DEBUG MAIN SHARK: Very small DiskScaleRadius=%.2e, setting H2=0\n", 
        //            g->DiskScaleRadius);
        // }
        g->H2_gas = 0.0;
        g->HI_gas = g->ColdGas;
        return;
    }
    
    float total_molecular_gas = 0.0;
    
    if (run_params->SFprescription == 3) {
        // GD14 model with SHARK-exact implementation
        // if (galaxy_debug_counter % 100000 == 0) {
        //     printf("\nDEBUG MAIN SHARK: Starting SHARK-exact GD14 for galaxy #%ld\n", galaxy_debug_counter);
        //     printf("  ColdGas=%.2e, StellarMass=%.2e, DiskScaleRadius=%.2e\n", 
        //            g->ColdGas, g->StellarMass, g->DiskScaleRadius);
        // }
        
        // // Calculate disk molecular gas through SHARK-exact radial integration
        // float disk_molecular_gas = integrate_molecular_gas_radial(g, run_params);
        
        // // Calculate bulge molecular gas using SHARK-exact method
        // float bulge_molecular_gas = 0.0;
        // if (g->BulgeMass > 0.0) {
        //     bulge_molecular_gas = calculate_bulge_molecular_gas(g, run_params);
        // }
        
        // // Total molecular gas - THIS IS THE KEY FIX
        // total_molecular_gas = disk_molecular_gas + bulge_molecular_gas;
        const float h = run_params->Hubble_h;
        const float re_pc = g->DiskScaleRadius * 1.0e6 / h / 1.67; // Half-mass radius in pc
        float disk_area_pc2 = 2.0 * M_PI * re_pc * re_pc; // Note: 2π for half-mass radius
        float gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²

        total_molecular_gas = calculate_molecular_fraction_GD14(
            gas_surface_density_center, 
            g->MetalsColdGas / g->ColdGas) * g->ColdGas; // Use absolute metallicity fraction, convert to mass
    }
    else if (run_params->SFprescription == 2) {
        // KD12 model
        const float disk_area = M_PI * g->DiskScaleRadius * g->DiskScaleRadius;
        if(disk_area <= 0.0) {
            g->H2_gas = 0.0;
            g->HI_gas = g->ColdGas;
            return;
        }
        const float surface_density = g->ColdGas / disk_area;
        float metallicity = 0.0;
        if(g->ColdGas > 0.0) {
            metallicity = g->MetalsColdGas / g->ColdGas; // absolute fraction
        }
        float clumping_factor = run_params->ClumpFactor;
        if (metallicity < 0.01) {
            clumping_factor = run_params->ClumpFactor * pow(0.01, -run_params->ClumpExponent);
        } else if (metallicity < 1.0) {
            clumping_factor = run_params->ClumpFactor * pow(metallicity, -run_params->ClumpExponent);
        }
        float f_H2 = calculate_H2_fraction_KD12(surface_density, metallicity, clumping_factor);
        total_molecular_gas = f_H2 * g->ColdGas;
    }
    else if (run_params->SFprescription == 1) {
        // Pressure-based model
        const float disk_area = M_PI * g->DiskScaleRadius * g->DiskScaleRadius;
        if(disk_area <= 0.0) {
            g->H2_gas = 0.0;
            g->HI_gas = g->ColdGas;
            return;
        }
        const float surface_density = g->ColdGas / disk_area;
        float metallicity = 0.0;
        if(g->ColdGas > 0.0) {
            metallicity = g->MetalsColdGas / g->ColdGas; // absolute fraction
        }
        float f_H2 = calculate_H2_fraction(surface_density, metallicity, g->DiskScaleRadius, run_params);
        total_molecular_gas = f_H2 * g->ColdGas;
    }
    else {
        // For debugging, let's see what happens with other models
        total_molecular_gas = 0.0; // Don't use fallback for debugging
    }
    
    // Update gas components - CRITICAL: Use the actual calculated values
    g->H2_gas = total_molecular_gas * g->ColdGas;
    // g->HI_gas = 0.0; // Initialize HI gas to zero, will be recalculated below

    g->HI_gas = (1.0 - total_molecular_gas) * g->ColdGas;
    
    // Apply bounds checking
    if(g->H2_gas > g->ColdGas) {
        // if (galaxy_debug_counter % 100000 == 0) {
        //     printf("DEBUG MAIN SHARK: H2 > ColdGas (%.2e > %.2e), capping at ColdGas\n", 
        //            g->H2_gas, g->ColdGas);
        // }
        g->H2_gas = g->ColdGas;
        g->HI_gas = 0.0;
    }
    // Note: HI_gas was already calculated correctly above, no need to recalculate

    // Final sanity checks (as SHARK does)
    if(g->H2_gas < 0.0) g->H2_gas = 0.0;
    if(g->HI_gas < 0.0) g->HI_gas = 0.0;
    
    // Mass conservation check with detailed debugging
    if(g->H2_gas + g->HI_gas > g->ColdGas * 1.001) {  
        float total = g->H2_gas + g->HI_gas;
        float scale = g->ColdGas / total;
        // if (galaxy_debug_counter % 100000 == 0) {
        //     printf("DEBUG MAIN SHARK: Mass conservation issue - H2+HI=%.2e > ColdGas=%.2e\n", 
        //            total, g->ColdGas);
        //     printf("  Applying scale factor: %.6f\n", scale);
        // }
        g->H2_gas *= scale;
        g->HI_gas *= scale;
    }
    
    if (galaxy_debug_counter % 100000 == 0) {
        float h2_fraction_cold = (g->ColdGas > 0.0) ? g->H2_gas / g->ColdGas : 0.0;
        float hi_fraction_cold = (g->ColdGas > 0.0) ? g->HI_gas / g->ColdGas : 0.0;
        float h2_fraction_proper = (g->H2_gas + g->HI_gas > 0.0) ? g->H2_gas / (g->H2_gas + g->HI_gas) : 0.0;
        float hi_fraction_proper = (g->H2_gas + g->HI_gas > 0.0) ? g->HI_gas / (g->H2_gas + g->HI_gas) : 0.0;
        printf("  FINAL RESULT: H2=%.2e, HI=%.2e\n", g->H2_gas, g->HI_gas);
        printf("  f_H2 = H2/ColdGas = %.6f, f_HI = HI/ColdGas = %.6f\n", h2_fraction_cold, hi_fraction_cold);
        printf("  f_H2 = H2/(H2+HI) = %.6f, f_HI = HI/(H2+HI) = %.6f\n", h2_fraction_proper, hi_fraction_proper);
        printf("  CHECK: f_H2 + f_HI = %.6f (should be ~1.0)\n", h2_fraction_cold + hi_fraction_cold);
        printf("  CHECK: Does f_H2=%.6f match expected from integration?\n", h2_fraction_cold);
        printf("END DEBUG MAIN SHARK\n");
        printf("========================================\n\n");
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
    printf("=====================================\n\n");    
    }
}   