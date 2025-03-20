#include <math.h>
#include "core_allvars.h"
#include "model_h2_formation.h"
#include "model_misc.h"

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
    const float P_mid = M_PI * run_params->G * surface_density * surface_density;
    const float P_0_internal = 5.93e-12 / run_params->UnitPressure_in_cgs;
    const float P_norm = P_mid / P_0_internal;

#ifdef VERBOSE
    static int counter = 0;
    if (counter % 500000 == 0) {
        printf("Disk area: %g, Surface density: %g, Metallicity: %g\n", disk_area, surface_density, metallicity);
        printf("Internal pressure: %g, P_norm: %g\n", P_mid, P_norm);
    }
    counter++;
#endif

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

#ifdef VERBOSE
    if (counter % 500000 == 0) {
        printf("Final f_H2: %g\n", f_H2);
    }
#endif

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
 * calculate_molecular_fraction_GD14 - Calculate molecular fraction using Gnedin & Draine (2014) model
 * 
 * @gas_density: Gas surface density (M_sun/pc^2)
 * @metallicity: Gas metallicity normalized to solar
 * @radiation_field: UV radiation field strength normalized to Milky Way
 * 
 * Returns: Molecular gas fraction (0-1)
 */
float calculate_molecular_fraction_GD14(float gas_density, float metallicity, float radiation_field)
{
    // Default values for other parameters
    float stellar_density = 0.0;  // Assume no stellar contribution to pressure
    float radius = 1.0;           // Default radius 1 kpc if not specified
    
    // Calculate midplane pressure
    float pressure = calculate_midplane_pressure(gas_density, stellar_density, radius, 0.0);
    
    // Ensure minimum metallicity to avoid numerical issues
    if (metallicity < 0.01) metallicity = 0.01;
    
    // Gas surface density relative to Milky Way typical value (~10 M_sun/pc^2)
    float u_mw = gas_density / 10.0;
    
    // Metallicity squared factor
    float d_mw2 = metallicity * metallicity;
    
    // Calculate alpha parameter per GD14 model
    float alpha = 0.5 + 1.0 / (1.0 + sqrt(u_mw * d_mw2 / 600.0));
    
    // Calculate normalization factor that depends on metallicity and radiation field
    float sigma_norm = 20.0 * pow(metallicity, -0.7) * radiation_field;
    
    // Calculate r_mol directly from pressure - alternative approach
    float pressure_term = 0.0;
    if (pressure > 0.0) {
        // P_0 from BR06 in appropriate units
        const float P_0 = 4.3e4;  // in K cm^-3
        pressure_term = pow(pressure / P_0, 0.92);
    }
    
    // Choose maximum of pressure-based and GD14 model predictions
    float r_mol_pressure = pressure_term;
    
    // GD14 model prediction
    float r_mol_gd14 = 0.0;
    if (sigma_norm > 0.0) {
        r_mol_gd14 = pow(gas_density / sigma_norm, alpha);
    }
    
    // Use the higher prediction - physical justification: pressure becomes dominant in high-density regions
    float r_mol = (r_mol_pressure > r_mol_gd14) ? r_mol_pressure : r_mol_gd14;
    
    // Convert ratio to fraction: f_H2 = r_mol / (1 + r_mol)
    float f_mol = r_mol / (1.0 + r_mol);
    
    // Sanity checks
    if (f_mol < 0.0) f_mol = 0.0;
    if (f_mol > 1.0) f_mol = 1.0;
    
    return f_mol;
}

/**
 * integrate_molecular_gas_radial - Calculates molecular gas mass by integrating over disk radius
 * 
 * @g: Galaxy structure containing disk properties
 * @run_params: Global parameters
 * 
 * Returns: Total molecular gas mass
 */
float integrate_molecular_gas_radial(struct GALAXY *g, const struct params *run_params)
{
    // Don't calculate if there's no cold gas
    if (g->ColdGas <= 0.0 || g->DiskScaleRadius <= 0.0) {
        return 0.0;
    }
    
    // Number of radial bins for integration
    const int N_RADIAL_BINS = run_params->IntegrationBins > 0 ? run_params->IntegrationBins : 30;
    
    // Integrate from 0 to 5 scale radii (covers >99% of exponential disk)
    const float MAX_RADIUS_FACTOR = 5.0;
    const float dr = MAX_RADIUS_FACTOR * g->DiskScaleRadius / N_RADIAL_BINS;
    
    // Get metallicity
    float metallicity = 0.0;
    if (g->ColdGas > 0.0) {
        metallicity = g->MetalsColdGas / g->ColdGas / 0.02;  // Normalized to solar
    }
    
    // Calculate stellar surface density at center
    float stellar_surface_density_center = 0.0;
    if (g->StellarMass > g->BulgeMass) {
        // Only consider disk stars, not bulge stars
        float disk_stellar_mass = g->StellarMass - g->BulgeMass;
        if (g->DiskScaleRadius > 0.0) {
            stellar_surface_density_center = disk_stellar_mass / 
                                         (2.0 * M_PI * g->DiskScaleRadius * g->DiskScaleRadius);
        }
    }
    
    // Calculate gas surface density at center (exponential disk)
    float gas_surface_density_center = 0.0;
    if (g->DiskScaleRadius > 0.0) {
        gas_surface_density_center = g->ColdGas / 
                                 (2.0 * M_PI * g->DiskScaleRadius * g->DiskScaleRadius);
    }
    
    // Setup for integration
    float total_molecular_gas = 0.0;
    float total_cold_gas = 0.0;
    
    // Perform integration over radius
    for (int i = 0; i < N_RADIAL_BINS; i++) {
        // Radius at the middle of this bin
        float radius = (i + 0.5) * dr;
        
        // Calculate local surface densities (exponential profile)
        float exp_factor = exp(-radius / g->DiskScaleRadius);
        float local_gas_density = gas_surface_density_center * exp_factor;
        float local_stellar_density = stellar_surface_density_center * exp_factor;
        
        // Calculate area of this annular ring
        float ring_area = 2.0 * M_PI * radius * dr;
        
        // Gas mass in this ring
        float ring_gas_mass = local_gas_density * ring_area;
        total_cold_gas += ring_gas_mass;
        
        // Calculate molecular fraction
        float radiation_field = run_params->RadiationFieldNorm;
        
        // Scale radiation field with stellar density
        if (local_stellar_density > 0.0) {
            radiation_field *= pow(local_stellar_density / stellar_surface_density_center, 0.3);
        }
        
        float molecular_fraction = calculate_molecular_fraction_GD14(
            local_gas_density, metallicity, radiation_field);
        
        // Molecular gas in this ring
        float ring_mol_gas = molecular_fraction * ring_gas_mass;
        total_molecular_gas += ring_mol_gas;
    }
    
    // Ensure we don't exceed total cold gas (can happen due to numerical integration)
    if (total_molecular_gas > g->ColdGas) {
        total_molecular_gas = g->ColdGas;
    }
    
    return total_molecular_gas;
}

/**
 * calculate_bulge_molecular_gas - Estimate molecular gas in the galaxy bulge
 * 
 * @g: Galaxy structure containing bulge properties
 * @run_params: Global parameters
 * 
 * Returns: Molecular gas mass in bulge
 */
float calculate_bulge_molecular_gas(struct GALAXY *g, const struct params *run_params)
{
    // Check if we have any bulge
    if (g->BulgeMass <= 0.0) {
        return 0.0;
    }
    
    // Estimate bulge gas as fraction of cold gas based on B/T ratio
    float bulge_to_total = g->BulgeMass / (g->StellarMass > 0.0 ? g->StellarMass : 1.0);
    
    // Estimate bulge gas fraction - bulges typically have less gas than disks
    float bulge_gas_fraction = bulge_to_total * 0.5;  // typically less gas-rich
    float bulge_gas = bulge_gas_fraction * g->ColdGas;
    
    // Get bulge radius - if not available, estimate from disk radius
    float bulge_radius = g->DiskScaleRadius * 0.2;  // typical bulge is ~1/5 of disk
    
    // Calculate bulge gas surface density (assuming spherical distribution)
    float bulge_gas_surface_density = 0.0;
    if (bulge_radius > 0.0) {
        bulge_gas_surface_density = bulge_gas / (M_PI * bulge_radius * bulge_radius);
    }
    
    // Get metallicity
    float metallicity = 0.0;
    if (g->ColdGas > 0.0) {
        // Assume same metallicity for disk and bulge gas
        metallicity = g->MetalsColdGas / g->ColdGas / 0.02;
    }
    
    // Bulges have high surface densities, high metallicities, and high pressure
    // This generally leads to high molecular fractions
    
    // GD14 model - but with enhanced radiation field due to dense stellar population
    float radiation_field = run_params->RadiationFieldNorm * 2.0;  // Enhanced in bulge
    
    float molecular_fraction = calculate_molecular_fraction_GD14(
        bulge_gas_surface_density, metallicity, radiation_field);
    
    // Bulges typically have high molecular fractions due to high density
    // Set minimum molecular fraction for bulges to 0.5
    if (molecular_fraction < 0.5) {
        molecular_fraction = 0.5;
    }
    
    return bulge_gas * molecular_fraction;
}

void update_gas_components(struct GALAXY *g, const struct params *run_params)
{
    // Don't update if no cold gas
    if(g->ColdGas <= 0.0) {
        g->H2_gas = 0.0;
        g->HI_gas = 0.0;
        return;
    }

    float total_molecular_gas = 0.0;
    
    if (run_params->SFprescription == 3) {
        // GD14 model with radial integration
        
        // Calculate disk molecular gas through radial integration
        float disk_molecular_gas = integrate_molecular_gas_radial(g, run_params);
        
        // Calculate bulge molecular gas
        float bulge_molecular_gas = calculate_bulge_molecular_gas(g, run_params);
        
        // Total molecular gas
        total_molecular_gas = disk_molecular_gas + bulge_molecular_gas;
    }
    else if (run_params->SFprescription == 2) {
        // Existing KD12 model - kept unchanged
        // Calculate surface density using disk scale radius
        const float disk_area = M_PI * g->DiskScaleRadius * g->DiskScaleRadius;
        if(disk_area <= 0.0) {
            g->H2_gas = 0.0;
            g->HI_gas = g->ColdGas;
            return;
        }
        
        const float surface_density = g->ColdGas / disk_area;
        float metallicity = 0.0;
        if(g->ColdGas > 0.0) {
            metallicity = g->MetalsColdGas / g->ColdGas / 0.02;
        }
        
        float clumping_factor = run_params->ClumpFactor;
        if (metallicity * 0.02 < 0.01) {
            clumping_factor = run_params->ClumpFactor * 
                              pow(0.01, -run_params->ClumpExponent);
        } else if (metallicity * 0.02 < 1.0) {
            clumping_factor = run_params->ClumpFactor * 
                              pow(metallicity * 0.02, -run_params->ClumpExponent);
        }
        
        float f_H2 = calculate_H2_fraction_KD12(surface_density, g->MetalsColdGas/g->ColdGas, clumping_factor);
        total_molecular_gas = f_H2 * g->ColdGas;
    }
    else if (run_params->SFprescription == 1) {
        // Original pressure-based model - kept unchanged
        const float disk_area = M_PI * g->DiskScaleRadius * g->DiskScaleRadius;
        if(disk_area <= 0.0) {
            g->H2_gas = 0.0;
            g->HI_gas = g->ColdGas;
            return;
        }
        
        const float surface_density = g->ColdGas / disk_area;
        float metallicity = 0.0;
        if(g->ColdGas > 0.0) {
            metallicity = g->MetalsColdGas / g->ColdGas / 0.02;
        }
        
        float f_H2 = calculate_H2_fraction(surface_density, metallicity, 
                                         g->DiskScaleRadius, run_params);
        total_molecular_gas = f_H2 * g->ColdGas;
    }
    else {
        // Default for model 0 or anything else - simple prescription
        total_molecular_gas = 0.3 * g->ColdGas;  // Fixed 30% molecular fraction
    }
    
    // Update gas components
    g->H2_gas = total_molecular_gas;
    if(g->H2_gas > g->ColdGas) {
        g->H2_gas = g->ColdGas;
        g->HI_gas = 0.0;
    } else {
        g->HI_gas = g->ColdGas - g->H2_gas;
    }

    // Final sanity checks
    if(g->H2_gas < 0.0) g->H2_gas = 0.0;
    if(g->HI_gas < 0.0) g->HI_gas = 0.0;
    
    // Check mass conservation with small numerical tolerance
    if(g->H2_gas + g->HI_gas > g->ColdGas * 1.001) {  
        float total = g->H2_gas + g->HI_gas;
        float scale = g->ColdGas / total;
        g->H2_gas *= scale;
        g->HI_gas *= scale;
    }
}

void init_gas_components(struct GALAXY *g)
{
    g->H2_gas = 0.0;
    g->HI_gas = 0.0;
}