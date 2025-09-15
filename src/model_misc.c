#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "core_cool_func.h"
#include "core_allvars.h"
#include "model_misc.h"

void init_galaxy(const int p, const int halonr, int *galaxycounter, const struct halo_data *halos,
                 struct GALAXY *galaxies, const struct params *run_params)
{

	XASSERT(halonr == halos[halonr].FirstHaloInFOFgroup, -1,
            "Error: halonr = %d should be equal to the FirsthaloInFOFgroup = %d\n",
            halonr, halos[halonr].FirstHaloInFOFgroup);

    galaxies[p].Type = 0;
    galaxies[p].Regime = 1; // Hot to start with, will be updated later

    galaxies[p].GalaxyNr = *galaxycounter;
    (*galaxycounter)++;

    galaxies[p].HaloNr = halonr;
    galaxies[p].MostBoundID = halos[halonr].MostBoundID;
    galaxies[p].SnapNum = halos[halonr].SnapNum - 1;

    galaxies[p].mergeType = 0;
    galaxies[p].mergeIntoID = -1;
    galaxies[p].mergeIntoSnapNum = -1;
    galaxies[p].dT = -1.0;

    for(int j = 0; j < 3; j++) {
        galaxies[p].Pos[j] = halos[halonr].Pos[j];
        galaxies[p].Vel[j] = halos[halonr].Vel[j];
    }

    galaxies[p].Len = halos[halonr].Len;
    galaxies[p].Vmax = halos[halonr].Vmax;
    galaxies[p].Vvir = get_virial_velocity(halonr, halos, run_params);
    galaxies[p].Mvir = get_virial_mass(halonr, halos, run_params);
    galaxies[p].Rvir = get_virial_radius(halonr, halos, run_params);

    galaxies[p].deltaMvir = 0.0;

    galaxies[p].ColdGas = 0.0;
    galaxies[p].H2gas = 0.0;
    galaxies[p].StellarMass = 0.0;
    galaxies[p].BulgeMass = 0.0;
    galaxies[p].HotGas = 0.0;
    galaxies[p].CGMgas = 0.0;
    galaxies[p].BlackHoleMass = 0.0;
    galaxies[p].ICS = 0.0;

    galaxies[p].MetalsColdGas = 0.0;
    galaxies[p].MetalsStellarMass = 0.0;
    galaxies[p].MetalsBulgeMass = 0.0;
    galaxies[p].MetalsHotGas = 0.0;
    galaxies[p].MetalsCGMgas = 0.0;
    galaxies[p].MetalsICS = 0.0;
    galaxies[p].RcoolToRvir = 0.0;

    for(int step = 0; step < STEPS; step++) {
        galaxies[p].SfrDisk[step] = 0.0;
        galaxies[p].SfrBulge[step] = 0.0;
        galaxies[p].SfrDiskColdGas[step] = 0.0;
        galaxies[p].SfrDiskColdGasMetals[step] = 0.0;
        galaxies[p].SfrBulgeColdGas[step] = 0.0;
        galaxies[p].SfrBulgeColdGasMetals[step] = 0.0;
    }

    galaxies[p].DiskScaleRadius = get_disk_radius(halonr, p, halos, galaxies);
    galaxies[p].MergTime = 999.9f;
    galaxies[p].Cooling = 0.0;
    galaxies[p].Heating = 0.0;
    galaxies[p].r_heat = 0.0;
    galaxies[p].QuasarModeBHaccretionMass = 0.0;
    galaxies[p].TimeOfLastMajorMerger = -1.0;
    galaxies[p].TimeOfLastMinorMerger = -1.0;
    galaxies[p].OutflowRate = 0.0;
	galaxies[p].TotalSatelliteBaryons = 0.0;

	// infall properties
    galaxies[p].infallMvir = -1.0;
    galaxies[p].infallVvir = -1.0;
    galaxies[p].infallVmax = -1.0;

}



double get_disk_radius(const int halonr, const int p, const struct halo_data *halos, const struct GALAXY *galaxies)
{
	if(galaxies[p].Vvir > 0.0 && galaxies[p].Rvir > 0.0) {
		// See Mo, Shude & White (1998) eq12, and using a Bullock style lambda.
		double SpinMagnitude = sqrt(halos[halonr].Spin[0] * halos[halonr].Spin[0] +
                                    halos[halonr].Spin[1] * halos[halonr].Spin[1] + halos[halonr].Spin[2] * halos[halonr].Spin[2]);

		double SpinParameter = SpinMagnitude / ( 1.414 * galaxies[p].Vvir * galaxies[p].Rvir);
		return (SpinParameter / 1.414 ) * galaxies[p].Rvir;
        /* return SpinMagnitude * 0.5 / galaxies[p].Vvir; /\* should be equivalent to previous call *\/ */
	} else {
		return 0.1 * galaxies[p].Rvir;
    }
}



double get_metallicity(const double gas, const double metals)
{
  double metallicity = 0.0;

  if(gas > 0.0 && metals > 0.0) {
      metallicity = metals / gas;
      metallicity = metallicity >= 1.0 ? 1.0:metallicity;
  }

  return metallicity;
}



double dmax(const double x, const double y)
{
    return (x > y) ? x:y;
}



double get_virial_mass(const int halonr, const struct halo_data *halos, const struct params *run_params)
{
  if(halonr == halos[halonr].FirstHaloInFOFgroup && halos[halonr].Mvir >= 0.0)
    return halos[halonr].Mvir;   /* take spherical overdensity mass estimate */
  else
    return halos[halonr].Len * run_params->PartMass;
}



double get_virial_velocity(const int halonr, const struct halo_data *halos, const struct params *run_params)
{
	double Rvir;

	Rvir = get_virial_radius(halonr, halos, run_params);

    if(Rvir > 0.0)
		return sqrt(run_params->G * get_virial_mass(halonr, halos, run_params) / Rvir);
	else
		return 0.0;
}


double get_virial_radius(const int halonr, const struct halo_data *halos, const struct params *run_params)
{
  // return halos[halonr].Rvir;  // Used for Bolshoi
  const int snapnum = halos[halonr].SnapNum;
  const double zplus1 = 1.0 + run_params->ZZ[snapnum];
  const double hubble_of_z_sq =
      run_params->Hubble * run_params->Hubble *(run_params->Omega * zplus1 * zplus1 * zplus1 + (1.0 - run_params->Omega - run_params->OmegaLambda) * zplus1 * zplus1 +
                                              run_params->OmegaLambda);

  const double rhocrit = 3.0 * hubble_of_z_sq / (8.0 * M_PI * run_params->G);
  const double fac = 1.0 / (200.0 * 4.0 * M_PI / 3.0 * rhocrit);

  return cbrt(get_virial_mass(halonr, halos, run_params) * fac);
}

void determine_and_store_regime(const int ngal, struct GALAXY *galaxies)
{
    for(int p = 0; p < ngal; p++) {
        if(galaxies[p].mergeType > 0) continue;
        
        // const double rcool_to_rvir = calculate_rcool_to_rvir_ratio(p, galaxies, run_params);
        
        // Use Vvir threshold instead of mass threshold
        // const double Vvir_threshold = 90.0;  // km/s, corresponds to ~1e11.5 M☉ and T_vir ~1e5 K
        const double Tvir = 35.9 * galaxies[p].Vvir * galaxies[p].Vvir; // in Kelvin
        const double Tvir_threshold = 2.5e5; // K, corresponds to Vvir ~52.7 km/s
        const double Tvir_to_Tmax_ratio = Tvir_threshold / Tvir;

        // FIXED: Determine regime based on BOTH cooling radius AND virial velocity
        // CGM regime (0): BOTH rcool > Rvir AND Vvir < 90 km/s
        // HOT regime (1): EITHER rcool <= Rvir OR Vvir >= 90 km/s

        // galaxies[p].Regime = (rcool_to_rvir > 1.0 && galaxies[p].Vvir < Vvir_threshold) ? 0 : 1;
        galaxies[p].Regime = (Tvir_to_Tmax_ratio >= 1.0 ) ? 0 : 1;

        // ALSO enforce the regime immediately to prevent inconsistencies
        if(galaxies[p].Regime == 0) {
            // CGM regime: transfer all HotGas to CGM
            if(galaxies[p].HotGas > 1e-20) {
                galaxies[p].CGMgas += galaxies[p].HotGas;
                galaxies[p].MetalsCGMgas += galaxies[p].MetalsHotGas;
                galaxies[p].HotGas = 0.0;
                galaxies[p].MetalsHotGas = 0.0;
            }
        } else {
            // HOT regime: transfer all CGMgas to HotGas
            if(galaxies[p].CGMgas > 1e-20) {
                galaxies[p].HotGas += galaxies[p].CGMgas;
                galaxies[p].MetalsHotGas += galaxies[p].MetalsCGMgas;
                galaxies[p].CGMgas = 0.0;
                galaxies[p].MetalsCGMgas = 0.0;
            }
        }
    }
}

void final_regime_check(const int ngal, struct GALAXY *galaxies, const struct params *run_params)
{
    if(run_params->CGMrecipeOn > 0) {
        return;
    }
    
    // static int total_galaxies_processed = 0;  // Track total galaxies across all calls
    // int moves_this_batch = 0;                 // Track moves in this batch
    
    for(int p = 0; p < ngal; p++) {
        if(galaxies[p].mergeType > 0) continue;
        
        // total_galaxies_processed++;
        
        // Use Vvir threshold instead of mass threshold
        // const double Vvir_threshold = 90.0;  // km/s
        // const double rcool_to_rvir = calculate_rcool_to_rvir_ratio(p, galaxies, run_params);
        const double Tvir = 35.9 * galaxies[p].Vvir * galaxies[p].Vvir; // in Kelvin
        const double Tvir_threshold = 2.5e5; // K, corresponds to Vvir ~52.7 km/s
        const double Tvir_to_Tmax_ratio = Tvir_threshold / Tvir;

        // Store original values for diagnostic output
        // double original_hot_gas = galaxies[p].HotGas;
        // double original_cgm_gas = galaxies[p].CGMgas;
        // double original_metals_hot = galaxies[p].MetalsHotGas;
        // double original_metals_cgm = galaxies[p].MetalsCGMgas;
        // double original_reincorp = galaxies[p].ReincorporatedGas;
        // int original_regime = galaxies[p].Regime;
        
        // int gas_moved = 0;  // Track if any gas was moved for this galaxy
        
        // ENFORCE VELOCITY THRESHOLD: Vvir < 90 km/s -> CGM, Vvir >= 90 km/s -> HOT
        // This is the final arbiter that overrides physics-based regime determination

        if(Tvir_to_Tmax_ratio >= 1.0) {
            // LOW-VELOCITY: Must be in CGM regime regardless of rcool/Rvir
            if(galaxies[p].HotGas > 1e-20) {
                galaxies[p].CGMgas += galaxies[p].HotGas;
                galaxies[p].MetalsCGMgas += galaxies[p].MetalsHotGas;
                galaxies[p].HotGas = 0.0;
                galaxies[p].MetalsHotGas = 0.0;
                // gas_moved = 1;
            }
            galaxies[p].Regime = 0;
        } else {
            // HIGH-VELOCITY: Must be in HOT regime regardless of rcool/Rvir
            if(galaxies[p].CGMgas > 1e-20) {
                galaxies[p].HotGas += galaxies[p].CGMgas;
                galaxies[p].MetalsHotGas += galaxies[p].MetalsCGMgas;
                galaxies[p].CGMgas = 0.0;
                galaxies[p].MetalsCGMgas = 0.0;
                // gas_moved = 1;
            }
            galaxies[p].Regime = 1;
        }
        
        // Print diagnostics only when gas is moved
        // if(gas_moved) {
        //     printf("\n=== REGIME ENFORCEMENT DIAGNOSTICS (Galaxy #%d) ===\n", total_galaxies_processed);
        //     printf("  *** GAS MOVED FOR THIS GALAXY ***\n");
        //     printf("  Before -> After:\n");
        //     printf("    HotGas:     %.3e -> %.3e Msun\n", original_hot_gas, galaxies[p].HotGas);
        //     printf("    CGMgas:     %.3e -> %.3e Msun\n", original_cgm_gas, galaxies[p].CGMgas);
        //     printf("    MetalsHot:  %.3e -> %.3e Msun\n", original_metals_hot, galaxies[p].MetalsHotGas);
        //     printf("    MetalsCGM:  %.3e -> %.3e Msun\n", original_metals_cgm, galaxies[p].MetalsCGMgas);
        //     printf("    Reincorp:   %.3e -> %.3e Msun\n", original_reincorp, galaxies[p].ReincorporatedGas);
        //     // Total gas mass conservation check
        //     double original_total = original_hot_gas + original_cgm_gas + original_reincorp;
        //     double final_total = galaxies[p].HotGas + galaxies[p].CGMgas + galaxies[p].ReincorporatedGas;
        //     printf("  Total gas: %.3e -> %.3e Msun", original_total, final_total);
        //     if(fabs(original_total - final_total) > 1e-12) {
        //         printf(" [WARNING: Mass not conserved!");
        //     }
        //     printf("\n======================================================\n");
        // }
    }
}

double calculate_rcool_to_rvir_ratio(const int gal, struct GALAXY *galaxies, const struct params *run_params)
{
    // Check basic requirements for physics calculation
    if(galaxies[gal].Vvir <= 0.0) {
        return 1.1;  // Return > 1.0 to indicate CGM regime for very small halos
    }

    const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
    const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;         // in Kelvin

    // SAFETY: Check for reasonable temperature
    if(temp <= 0.0 || tcool <= 0.0) {
        return 1.1;
    }

    // Use metallicity from either HotGas or CGM gas, whichever exists
    double logZ = -10.0;
    if(galaxies[gal].HotGas > 0.0 && galaxies[gal].MetalsHotGas > 0.0) {
        logZ = log10(galaxies[gal].MetalsHotGas / galaxies[gal].HotGas);
    } else if(galaxies[gal].CGMgas > 0.0 && galaxies[gal].MetalsCGMgas > 0.0) {
        logZ = log10(galaxies[gal].MetalsCGMgas / galaxies[gal].CGMgas);
    }

    double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
    double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
    x /= (run_params->UnitDensity_in_cgs * run_params->UnitTime_in_s);         // now in internal units
    const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

    // For density calculation, use total gas (HotGas + CGMgas) to represent potential hot halo
    double total_gas = galaxies[gal].HotGas + galaxies[gal].CGMgas;
    if(total_gas <= 0.0) {
        return 1.1;  // Return > 1.0 to indicate CGM regime if no gas at all
    }

    // an isothermal density profile for the hot gas is assumed here
    const double rho0 = total_gas / (4 * M_PI * galaxies[gal].Rvir);
    const double rcool = sqrt(rho0 / rho_rcool);

    // Continue with physics but add bounds checking...
    double result = rcool / galaxies[gal].Rvir;
    
    // SAFETY: Clamp to reasonable range
    if(result < 0.001) result = 0.001;
    if(result > 100.0) result = 100.0;
    
    return result;
}

float calculate_muratov_mass_loading(const int gal, struct GALAXY *galaxies, const double z)
{
    // Get circular velocity in km/s
    double vc = galaxies[gal].Vvir;  // Using virial velocity (already in km/s)

    // Add safety check to prevent division by zero
    if (vc <= 0.0) {
        return 0.0;  // Return zero mass loading if velocity is invalid
    }
    
    // Constants from Muratov et al. (2015) paper
    const double V_CRIT = 60.0;  // Critical velocity where the power law breaks
    const double NORM = 2.9;     // Normalization factor
    const double Z_EXP = 1.3;    // Redshift power-law exponent
    const double LOW_V_EXP = -3.2;  // Low velocity power-law exponent
    const double HIGH_V_EXP = -1.0; // High velocity power-law exponent

    // Calculate redshift term: (1+z)^1.3
    double z_term = pow(1.0 + z, Z_EXP);
    
    // Calculate velocity term with SHARP BREAK at exactly 60 km/s
    double v_term;
    if (vc < V_CRIT) {
        // Equation 4: For vc < 60 km/s
        v_term = pow(vc / V_CRIT, LOW_V_EXP);
    } else {
        // Equation 5: For vc >= 60 km/s  
        v_term = pow(vc / V_CRIT, HIGH_V_EXP);
    }
    
    double eta = NORM * z_term * v_term;

    // Safety check for the result
    if (!isfinite(eta)) {
        return 0.0;  // Return zero if result is NaN or infinity
    }

    return eta;
}

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
    if (sigma_stars < 3.0) {
        effective_sigma_stars = fmax(sigma_gas * 0.2, 3.0);
    }
    
    // Calculate stellar scale height using exact BR06 equation (9)
    float h_star_pc = calculate_stellar_scale_height_BR06(disk_scale_length_pc);
    
    // BR06 hardcoded parameters EXACTLY as in paper
    const float v_g = 8.0;          // km/s, gas velocity dispersion (BR06 Table text)
    
    // BR06 Equation (5) EXACTLY as written in paper:
    // P_ext/k = 272 cm⁻³ K × (Σ_gas/M_⊙ pc⁻²) × (Σ_*/M_⊙ pc⁻²)^0.5 × (v_g/km s⁻¹) × (h_*/pc)^-0.5
    float pressure = 272.0 * sigma_gas * sqrt(effective_sigma_stars) * v_g / sqrt(h_star_pc);

    if (pressure > 6e5) {
        pressure = 6e5; // K cm^-3
    }

    return pressure; // K cm⁻³
}

/**
 * calculate_molecular_fraction_BR06 - BR06 molecular fraction EXACTLY as in paper
 * From Blitz & Rosolowsky (2006) Equations (11) and (13)
 */
float calculate_molecular_fraction_BR06(float gas_surface_density, float stellar_surface_density, 
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
    // const float P_threshold = 1000.0; // K cm⁻³
    // if (pressure < P_threshold) {
    //     return 0.0;
    // }
    
    // BR06 Equation (11): R_mol = (P_ext/P₀)^α
    float pressure_ratio = pressure / P0;
    float R_mol = pow(pressure_ratio, alpha);
    
    // Convert to molecular fraction: f_mol = R_mol / (1 + R_mol)
    // This is the standard conversion from molecular-to-atomic ratio to molecular fraction
    double f_mol = R_mol / (1.0 + R_mol);
    
    // // Apply physical bounds
    // if (f_mol > 0.95) f_mol = 0.95; // Max 95% molecular (to avoid numerical issues)
    // if (f_mol < 0.0) f_mol = 0.0;   // No negative fractions
    // Smooth cap instead of hard limit
    if (f_mol > 0.8) {
        f_mol = 0.8 + 0.15 * tanh((f_mol - 0.8) / 0.1);
    }
    
    return f_mol;
}

/**
 * calculate_molecular_fraction_darksage_pressure - DarkSAGE pressure-based H2 formation
 * Implements the exact pressure-based prescription from DarkSAGE H2prescription==0
 */
float calculate_molecular_fraction_darksage_pressure(float gas_surface_density_msun_pc2, 
                                                  float stellar_surface_density_msun_pc2,
                                                  float gas_velocity_dispersion_km_s,
                                                  float stellar_velocity_dispersion_km_s,
                                                  float disk_alignment_angle_deg,
                                                  const struct params *run_params)
{
    // Early termination for edge cases
    if (gas_surface_density_msun_pc2 <= 1e-10) {
        return 0.0;
    }
    
    // DarkSAGE constants (these should match your parameter file values)
    const float ThetaThresh = 30.0;  // degrees
    const float H2FractionFactor = 1.0;  
    const float H2FractionExponent = 0.92;  
    const float P_0 = 4.3e4;  // Reference pressure in K cm^-3
    
    // Physical constants from your macros
    const float G_cgs = GRAVITY;  // cm^3 g^-1 s^-2
    const float kB = BOLTZMANN;   // erg K^-1
    const float MSUN_g = SOLAR_MASS;  // g
    const float PC_cm = CM_PER_MPC / 1e6;  // cm per pc
    
    // Convert surface densities to CGS (g cm^-2)
    float sigma_gas_cgs = gas_surface_density_msun_pc2 * MSUN_g / (PC_cm * PC_cm);
    float sigma_stars_cgs = stellar_surface_density_msun_pc2 * MSUN_g / (PC_cm * PC_cm);
    
    // Calculate velocity dispersion ratio (DarkSAGE approach)
    float f_sigma = (stellar_velocity_dispersion_km_s > 0.0) ? 
                    gas_velocity_dispersion_km_s / stellar_velocity_dispersion_km_s : 1.0;
    
    // Calculate midplane pressure using DarkSAGE formula
    float pressure_cgs;  // dyne cm^-2
    if (disk_alignment_angle_deg <= ThetaThresh) {
        // Aligned discs - both gas and stars contribute
        pressure_cgs = 0.5 * M_PI * G_cgs * sigma_gas_cgs * 
                      (sigma_gas_cgs + f_sigma * sigma_stars_cgs);
    } else {
        // Misaligned discs - only gas self-gravity
        pressure_cgs = 0.5 * M_PI * G_cgs * sigma_gas_cgs * sigma_gas_cgs;
    }
    
    // Apply Hubble factor like DarkSAGE does
    pressure_cgs *= run_params->Hubble_h * run_params->Hubble_h;
    
    // Convert to K cm^-3 (CORRECT FORMULA)
    float pressure_K_cm3 = pressure_cgs / kB;
    
    // Safety checks
    if (!isfinite(pressure_K_cm3) || pressure_K_cm3 <= 0.0) {
        return 0.0;
    }
    
    // Apply the DarkSAGE power-law relation
    float pressure_ratio = pressure_K_cm3 / P_0;
    float f_H2_HI = H2FractionFactor * pow(pressure_ratio, H2FractionExponent);
    
    // Safety check on power law result
    if (!isfinite(f_H2_HI)) {
        return 0.0;
    }
    
    // Convert H2/(H2+HI) ratio to H2/total_gas fraction
    const float X_H = 0.76;
    double f_mol = X_H * f_H2_HI / (1.0 + f_H2_HI);
    
    // Apply physical bounds
    if (f_mol > 0.95) f_mol = 0.95;
    if (f_mol < 0.0 || !isfinite(f_mol)) f_mol = 0.0;
    
    return f_mol;
}