#include <math.h>
#include "core_allvars.h"
#include "model_h2_formation.h"
#include "model_misc.h"

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
 * get_mass_dependent_radiation_field - Better radiation field calculation
 * 
 * This improved version:
 * - Accounts for galaxy type (stronger in centrals with AGN)
 * - Has more realistic mass scaling
 * - Includes stellar mass and SFR feedback effects
 */
float get_mass_dependent_radiation_field(struct GALAXY *g, const struct params *run_params)
{
    // Physical Motivation: More nuanced radiation field calculation
    // Incorporates multiple physical processes affecting radiation

    // Base radiation field with more complex initial scaling
    float radiation_field = run_params->RadiationFieldNorm * 
                            (1.0 + 0.2 * (g->Type == 0));  // Subtle central galaxy boost

    // Mass-dependent scaling with non-linear components
    if (g->Mvir > 0.0) {
        float log_mvir = log10(g->Mvir * 1.0e10 / run_params->Hubble_h);
        
        // More sophisticated mass scaling
        // Inspired by halo assembly bias and radiation field physics
        if (log_mvir > 11.0) {
            // Logarithmic scaling with saturation
            radiation_field *= 1.0 + 
                               log(1.0 + exp(log_mvir - 12.0)) * 0.5;
        }
    }

    // Enhanced Black Hole Growth Effects
    if (g->BlackHoleMass > 0.0) {
        float log_bh = log10(g->BlackHoleMass);
        
        // More dramatic AGN radiation scaling
        // Motivated by AGN feedback mechanisms
        if (log_bh > 6.0) {
            radiation_field *= 1.0 + 
                               pow(log_bh - 6.0, 1.5) * 0.7;  // Non-linear scaling
        }

        // Quasar mode accretion with more complex scaling
        if (g->QuasarModeBHaccretionMass > 0.0) {
            float accretion_ratio = g->QuasarModeBHaccretionMass / 
                                    (g->BlackHoleMass + 1e-10);
            
            // Log-normal like scaling to capture burst dynamics
            radiation_field *= 1.0 + 
                               15.0 * accretion_ratio * 
                               exp(-pow(log(accretion_ratio + 1), 2));
        }
    }

    // Star Formation History Integration
    float recent_sfr = 0.0, specific_sfr = 0.0;
    for (int step = 0; step < STEPS; step++) {
        recent_sfr += g->SfrDisk[step] + g->SfrBulge[step];
    }
    recent_sfr /= STEPS;
    
    if (g->StellarMass > 0.0) {
        specific_sfr = recent_sfr / g->StellarMass;
        
        // More complex specific SFR scaling
        // Captures stochastic star formation variations
        radiation_field *= 1.0 + 
                           log(1.0 + specific_sfr * 1e12) * 
                           tanh(specific_sfr * 1e11);
    }

    // Environmental Density Dependence
    // Captures large-scale structure effects on radiation field
    radiation_field *= pow(1.0 + g->Mvir / 1e12, 0.15);

    // Limit extreme values
    radiation_field = fmax(0.1, fmin(radiation_field, 1000.0));

    return radiation_field;
}

/**
 * calculate_molecular_fraction_GD14 - Improved molecular fraction calculation
 * 
 * Removes the artificial 80% minimum at high densities and uses a more
 * physically motivated density dependence
 */
float calculate_molecular_fraction_GD14(float gas_density, float metallicity, 
                                     float radiation_field, const struct params *run_params)
{
    // Early termination for zero or negative values
    if (gas_density <= 0.0) {
        return 0.0;
    }
    
    // Early termination for extremely low metallicity environments
    // (H2 formation requires dust, which requires metals)
    if (metallicity < 1.0e-4) {
        // Very low molecular fraction in extremely metal-poor gas
        return 0.01 * gas_density / 100.0;  // Scale with density but keep very low
    }
    
    // Early termination for very strong radiation fields 
    // (which dissociate H2 efficiently)
    if (radiation_field > 1000.0) {
        return 0.0;  // No H2 survives extremely strong radiation
    }

    // --- IMPROVEMENT 2: REMOVE ARTIFICIAL MINIMUM AT HIGH DENSITIES ---
    
    // Critical surface density (above which gas becomes mostly molecular)
    const float SIGMA_CRIT = 10.0; // M⊙/pc²
    
    // Metallicity factor (using configurable exponent from run_params)
    float metallicity_exponent = run_params->MetallicityExponent;
    float z_factor = pow(metallicity, metallicity_exponent); 
    
    // Smoother transition for molecular fraction with density
    // More physically motivated model without artificial 80% minimum
    float f_mol = 0.0;
    if (gas_density > 0.0) {
        // Sigmoid function for a smoother transition from atomic to molecular
        float density_ratio = gas_density / (SIGMA_CRIT / z_factor);
        
        // More physically motivated formula for H2 fraction
        // Based on improved approximation of Gnedin & Draine results
        f_mol = 1.0 / (1.0 + pow(density_ratio, -1.8));
        
        // Apply radiation field (higher radiation = less molecular)
        // More physically motivated radiation scaling
        float radiation_suppression = 1.0 / (1.0 + 0.7 * sqrt(radiation_field));
        f_mol *= radiation_suppression;
        
        // At very high densities, molecular fraction approaches 1 naturally
        // without an artificial minimum
        if (gas_density > 100.0) {
            // Natural asymptotic approach to high molecular fraction
            float high_density_factor = 0.95 * (1.0 - exp(-(gas_density - 100.0) / 30.0));
            f_mol = f_mol + high_density_factor * (1.0 - f_mol);
        }
    }
    
    // Ensure bounds
    if (f_mol < 0.0) f_mol = 0.0;
    if (f_mol > 1.0) f_mol = 1.0;
    
#ifdef VERBOSE
static int counter_gd = 0;
if (counter_gd % 50000000 == 0) {
    printf("GD14 H2 Calculation:\n");
    printf("  Gas density: %g M☉/pc², Metallicity: %g Z☉\n", 
            gas_density, metallicity);
    printf("  Z factor: %g, SIGMA_CRIT: %g\n", z_factor, SIGMA_CRIT);
    printf("  Radiation field: %g, Final f_mol: %g\n", 
            radiation_field, f_mol);
}
counter_gd++;
#endif
    
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
    if (g->ColdGas <= 0.0) {
        return 0.0;
    }
    
    // Skip calculation for unrealistic disk radius values
    if (g->DiskScaleRadius <= 0.0) {
        return 0.0;
    }
    
    // For very small galaxies, use a simplified approximation
    if (g->ColdGas < 1.0e-5 && g->StellarMass < 1.0e-5) {
        return 0.3 * g->ColdGas;  // Simple fixed fraction
    }
    
    // Early termination for very large stellar mass to gas ratios
    // (these will have extremely low molecular fractions)
    if (g->StellarMass > 0.0 && g->ColdGas/g->StellarMass < 1.0e-4) {
        return 0.05 * g->ColdGas;  // Very low H2 fraction
    }

    // Don't calculate if there's no cold gas
    if (g->ColdGas <= 0.0 || g->DiskScaleRadius <= 0.0) {
        return 0.0;
    }
    
    // IMPORTANT: Convert Mpc/h to pc for surface density calculations
    // Typical values: h ≈ 0.7, so 1 Mpc/h ≈ 1.43 Mpc ≈ 1.43 × 10^6 pc
    const float h = run_params->Hubble_h;
    const float disk_radius_pc = g->DiskScaleRadius * 1.0e6 / h;  // Convert Mpc/h to pc
    
    // Number of radial bins for integration
    const int N_RADIAL_BINS = run_params->IntegrationBins > 0 ? run_params->IntegrationBins : 30;
    
    // Integrate from 0 to 5 scale radii (covers >99% of exponential disk)
    const float MAX_RADIUS_FACTOR = 5.0;
    const float dr = MAX_RADIUS_FACTOR * g->DiskScaleRadius / N_RADIAL_BINS;
    const float dr_pc = dr * 1.0e6 / h;  // Convert to pc
    
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
            // Mass in 10^10 M_sun/h, area in (Mpc/h)^2
            // Convert to M_sun/pc^2
            float disk_area_pc2 = M_PI * disk_radius_pc * disk_radius_pc;
            stellar_surface_density_center = (disk_stellar_mass * 1.0e10 / h) / disk_area_pc2;
        }
    }
    
    // Calculate gas surface density at center (exponential disk)
    float gas_surface_density_center = 0.0;
    if (g->DiskScaleRadius > 0.0) {
        // Mass in 10^10 M_sun/h, area in (Mpc/h)^2
        // Convert to M_sun/pc^2
        float disk_area_pc2 = M_PI * disk_radius_pc * disk_radius_pc;
        gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2;
    }
    
    // Setup for integration
    float total_molecular_gas = 0.0;
    float total_cold_gas = 0.0;
    
    // Perform integration over radius
    for (int i = 0; i < N_RADIAL_BINS; i++) {
        // Radius at the middle of this bin in Mpc/h (original units)
        float radius = (i + 0.5) * dr;
        float radius_pc = radius * 1.0e6 / h;  // Convert to pc
        
        // Calculate local surface densities (exponential profile)
        float exp_factor = exp(-radius / g->DiskScaleRadius);
        float local_gas_density = gas_surface_density_center * exp_factor;  // M_sun/pc^2
        float local_stellar_density = stellar_surface_density_center * exp_factor;  // M_sun/pc^2
        
        // Calculate area of this annular ring in pc^2
        float ring_area_pc2 = 2.0 * M_PI * radius_pc * dr_pc;
        
        // Gas mass in this ring in 10^10 M_sun/h (SAGE internal units)
        float ring_gas_mass = (local_gas_density * ring_area_pc2) / (1.0e10 / h);
        total_cold_gas += ring_gas_mass;
        
        // Use mass-dependent radiation field
        float base_radiation_field = get_mass_dependent_radiation_field(g, run_params);
        
        // Scale radiation field with local stellar density
        float radiation_field = base_radiation_field;
        if (local_stellar_density > 0.0) {
            radiation_field *= pow(local_stellar_density / stellar_surface_density_center, 0.3);
        }
        
        // Now the surface density is correctly in M_sun/pc^2 for molecular fraction calculation
        float molecular_fraction = calculate_molecular_fraction_GD14(
            local_gas_density, metallicity, radiation_field, run_params);
        
        // Molecular gas in this ring in 10^10 M_sun/h
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
    
    // IMPORTANT: Convert units for surface density calculation
    const float h = run_params->Hubble_h;
    const float bulge_radius_pc = bulge_radius * 1.0e6 / h;  // Convert Mpc/h to pc
    
    // Calculate bulge gas surface density (assuming spherical distribution)
    float bulge_gas_surface_density = 0.0;
    if (bulge_radius > 0.0) {
        float bulge_area_pc2 = M_PI * bulge_radius_pc * bulge_radius_pc;
        bulge_gas_surface_density = (bulge_gas * 1.0e10 / h) / bulge_area_pc2;  // M_sun/pc^2
    }
    
    // Get metallicity - now using bulge metallicity from MetalsBulgeMass
    float metallicity = 0.0;
    if (g->BulgeMass > 0.0) {
        // Use actual bulge metallicity instead of cold gas metallicity
        metallicity = g->MetalsBulgeMass / g->BulgeMass / 0.02;  // Normalized to solar
    } else if (g->ColdGas > 0.0) {
        // Fallback to cold gas metallicity if no bulge stars (unlikely given earlier check)
        metallicity = g->MetalsColdGas / g->ColdGas / 0.02;
    }
    
    // GD14 model - but with enhanced radiation field due to dense stellar population
    float radiation_field = run_params->RadiationFieldNorm * 2.0;  // Enhanced in bulge
    
    float molecular_fraction = calculate_molecular_fraction_GD14(
        bulge_gas_surface_density, metallicity, radiation_field, run_params);
    
    // Bulges typically have high molecular fractions due to high density
    // Set minimum molecular fraction for bulges to 0.5
    if (molecular_fraction < 0.5) {
        molecular_fraction = 0.5;
    }

    if (g->StellarMass > 1e10) {
        // Reduce molecular fraction in massive galaxy bulges
        molecular_fraction *= 0.7;  // 30% reduction
    }
    
    return bulge_gas * molecular_fraction;
}

/**
 * apply_environmental_effects - Enhanced environmental effects
 * 
 * This improved version:
 * - Begins environmental effects at lower halo masses
 * - Has more gradual transition with halo mass
 * - Accounts for orbit/position within halo
 */
void apply_environmental_effects(struct GALAXY *g, const struct params *run_params) 
{
    // Skip if environmental effects are disabled
    if (run_params->EnvironmentalEffectsOn == 0) return;
    
    // Skip if no H2 gas
    if (g->H2_gas <= 0.0) return;
    
    // --- IMPROVEMENT 4: REFINED ENVIRONMENTAL EFFECTS ---
    
    // 1. Apply to all galaxies, but stronger in satellites
    float type_factor = 1.0;
    if (g->Type == 0) {  // Central galaxy
        type_factor = 0.3;  // Environmental effects are 70% weaker in centrals
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
    env_strength *= run_params->EnvEffectStrength;
    
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
        
        // Update gas components
        g->H2_gas -= h2_affected;
        g->HI_gas += h2_to_hi;
        g->ColdGas -= h2_removed;  // Also update total cold gas
        
        // Ensure non-negative values
        if (g->H2_gas < 0.0) g->H2_gas = 0.0;
        if (g->HI_gas < 0.0) g->HI_gas = 0.0;
        if (g->ColdGas < 0.0) g->ColdGas = 0.0;
    }
}

/**
 * update_gas_components - Enhanced gas component update
 * 
 * This integrates all the improvements into the main function
 */
void update_gas_components(struct GALAXY *g, const struct params *run_params)
{
    // Early termination - if no cold gas or extremely small amount
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
    
    // Early termination - if extremely metal-poor and low mass
    // (H2 formation is inefficient in these conditions)
    if(g->MetalsColdGas < 1.0e-8 && g->ColdGas < 1.0e-6) {
        g->H2_gas = 0.1 * g->ColdGas;  // Assign small fixed fraction
        g->HI_gas = g->ColdGas - g->H2_gas;
        return;
    }

    float total_molecular_gas = 0.0;
    
    if (run_params->SFprescription == 3) {
        // GD14 model with radial integration
        
        // Calculate disk molecular gas through radial integration
        // But using improved models for radiation field and molecular fraction
        float disk_molecular_gas = 0.0;
        float bulge_molecular_gas = 0.0;
        
        // Get metallicity
        float metallicity = 0.0;
        if (g->ColdGas > 0.0) {
            metallicity = g->MetalsColdGas / g->ColdGas / 0.02;  // Normalized to solar
        }
        
        // Set up for radial calculation
        const float h = run_params->Hubble_h;
        const float disk_radius_pc = g->DiskScaleRadius * 1.0e6 / h;  // Convert Mpc/h to pc
        
        // Number of radial bins for integration
        const int N_RADIAL_BINS = run_params->IntegrationBins > 0 ? run_params->IntegrationBins : 30;
        
        // Max radius and step size
        const float MAX_RADIUS_FACTOR = 5.0;
        const float dr = MAX_RADIUS_FACTOR * g->DiskScaleRadius / N_RADIAL_BINS;
        const float dr_pc = dr * 1.0e6 / h;  // Convert to pc
        
        // Calculate surface densities
        float gas_surface_density_center = 0.0;
        float stellar_surface_density_center = 0.0;
        
        if (g->DiskScaleRadius > 0.0) {
            float disk_area_pc2 = M_PI * disk_radius_pc * disk_radius_pc;
            gas_surface_density_center = (g->ColdGas * 1.0e10 / h) / disk_area_pc2;
            
            if (g->StellarMass > g->BulgeMass) {
                float disk_stellar_mass = g->StellarMass - g->BulgeMass;
                stellar_surface_density_center = (disk_stellar_mass * 1.0e10 / h) / disk_area_pc2;
            }
        }
        
        // Get base radiation field with improvements
        float base_radiation_field = get_mass_dependent_radiation_field(g, run_params);
        
        // Perform integration over radius
        float total_cold_gas = 0.0;
        
        for (int i = 0; i < N_RADIAL_BINS; i++) {
            // Radius at the middle of this bin in Mpc/h (original units)
            float radius = (i + 0.5) * dr;
            float radius_pc = radius * 1.0e6 / h;  // Convert to pc
            
            // Calculate local surface densities (exponential profile)
            float exp_factor = exp(-radius / g->DiskScaleRadius);
            float local_gas_density = gas_surface_density_center * exp_factor;  // M_sun/pc^2
            float local_stellar_density = stellar_surface_density_center * exp_factor;  // M_sun/pc^2
            
            // Calculate area of this annular ring in pc^2
            float ring_area_pc2 = 2.0 * M_PI * radius_pc * dr_pc;
            
            // Gas mass in this ring in 10^10 M_sun/h (SAGE internal units)
            float ring_gas_mass = (local_gas_density * ring_area_pc2) / (1.0e10 / h);
            total_cold_gas += ring_gas_mass;
            
            // Scale radiation field with local stellar density
            float radiation_field = base_radiation_field;
            if (local_stellar_density > 0.0) {
                radiation_field *= pow(local_stellar_density / stellar_surface_density_center, 0.3);
            }
            
            // Use molecular fraction calculation with improvements
            float molecular_fraction = calculate_molecular_fraction_GD14(
                local_gas_density, metallicity, radiation_field, run_params);
            
            // Molecular gas in this ring in 10^10 M_sun/h
            float ring_mol_gas = molecular_fraction * ring_gas_mass;
            disk_molecular_gas += ring_mol_gas;
        }
        
        // Calculate bulge molecular gas
        if (g->BulgeMass > 0.0) {
            // Similar approach for bulge gas
            float bulge_to_total = g->BulgeMass / (g->StellarMass > 0.0 ? g->StellarMass : 1.0);
            float bulge_gas_fraction = bulge_to_total * 0.5;
            float bulge_gas = bulge_gas_fraction * g->ColdGas;
            
            float bulge_radius = g->DiskScaleRadius * 0.2;
            float bulge_radius_pc = bulge_radius * 1.0e6 / h;
            
            if (bulge_radius > 0.0) {
                float bulge_area_pc2 = M_PI * bulge_radius_pc * bulge_radius_pc;
                float bulge_gas_surface_density = (bulge_gas * 1.0e10 / h) / bulge_area_pc2;
                
                // Get bulge metallicity
                float bulge_metallicity = 0.0;
                if (g->BulgeMass > 0.0) {
                    bulge_metallicity = g->MetalsBulgeMass / g->BulgeMass / 0.02;
                } else if (g->ColdGas > 0.0) {
                    bulge_metallicity = g->MetalsColdGas / g->ColdGas / 0.02;
                }
                
                // Enhanced radiation field in bulge (denser stellar population)
                float bulge_radiation_field = base_radiation_field * 2.0;
                
                // Use molecular fraction calculation with improvements
                float bulge_molecular_fraction = calculate_molecular_fraction_GD14(
                    bulge_gas_surface_density, bulge_metallicity, bulge_radiation_field, run_params);
                
                bulge_molecular_gas = bulge_gas * bulge_molecular_fraction;
            }
        }
        
        // Total molecular gas
        total_molecular_gas = disk_molecular_gas + bulge_molecular_gas;
        
        // Apply environmental effects with improvements
        if (run_params->EnvironmentalEffectsOn != 0) {
            apply_environmental_effects(g, run_params);
        }
    }
    else if (run_params->SFprescription == 2) {
        // Existing KD12 model - kept largely unchanged
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
        
        float f_H2 = calculate_H2_fraction_KD12(surface_density, metallicity, clumping_factor);
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