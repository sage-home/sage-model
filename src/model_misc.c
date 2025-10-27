#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_misc.h"

void init_galaxy(const int p, const int halonr, int *galaxycounter, const struct halo_data *halos,
                 struct GALAXY *galaxies, const struct params *run_params)
{

	XASSERT(halonr == halos[halonr].FirstHaloInFOFgroup, -1,
            "Error: halonr = %d should be equal to the FirsthaloInFOFgroup = %d\n",
            halonr, halos[halonr].FirstHaloInFOFgroup);

    galaxies[p].Type = 0;
    galaxies[p].Regime = -1; // undefined at initialization

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
    galaxies[p].StellarMass = 0.0;
    galaxies[p].BulgeMass = 0.0;
    galaxies[p].HotGas = 0.0;
    galaxies[p].EjectedMass = 0.0;
    galaxies[p].BlackHoleMass = 0.0;
    galaxies[p].ICS = 0.0;
    galaxies[p].CGMgas = 0.0;
    galaxies[p].H2gas = 0.0;

    galaxies[p].MetalsColdGas = 0.0;
    galaxies[p].MetalsStellarMass = 0.0;
    galaxies[p].MetalsBulgeMass = 0.0;
    galaxies[p].MetalsHotGas = 0.0;
    galaxies[p].MetalsEjectedMass = 0.0;
    galaxies[p].MetalsICS = 0.0;
    galaxies[p].MetalsCGMgas = 0.0;

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
    galaxies[p].RcoolToRvir = 0.0;
    galaxies[p].MassLoading = 0.0;

	// infall properties
    galaxies[p].infallMvir = -1.0;
    galaxies[p].infallVvir = -1.0;
    galaxies[p].infallVmax = -1.0;
    galaxies[p].TimeOfInfall = -1.0;

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


// void determine_and_store_regime(const int ngal, struct GALAXY *galaxies, 
//                                 const struct params *run_params)
// {
    
//     // CGM recipe is on - classify based on virial temperature
//     for(int p = 0; p < ngal; p++) {
//         if(galaxies[p].mergeType > 0) continue;
        
//         // Calculate virial temperature: Tvir = (mu * mp / 2k) * Vvir^2
//         // Simplifies to: Tvir ≈ 35.9 * Vvir^2 (Vvir in km/s, Tvir in K)
//         const double z = run_params->ZZ[galaxies[p].SnapNum];
//         const double Tvir = 35.9 * galaxies[p].Vvir * galaxies[p].Vvir;
        
//         // Threshold with redshift evolution
//         const double Tvir_threshold_z0 = 8.0e5;  // 800,000 K at z=0
//         const double z_scaling = pow(1.0 + z, 0.35);
//         const double Tvir_threshold = Tvir_threshold_z0 * z_scaling;
        
//         // Regime 0: CGM regime (low-mass halos, Tvir < threshold)
//         // Regime 1: Hot-ICM regime (high-mass halos, Tvir >= threshold)
//         galaxies[p].Regime = (Tvir < Tvir_threshold) ? 0 : 1;
//     }
// }

void determine_and_store_regime(const int ngal, struct GALAXY *galaxies, 
                                const struct params *run_params)
{
    for(int p = 0; p < ngal; p++) {
        if(galaxies[p].mergeType > 0) continue;
        
        // Convert Mvir to physical units (Msun)
        // Mvir is stored in units of 10^10 Msun/h
        const double Mvir_physical = galaxies[p].Mvir * 1.0e10 / run_params->Hubble_h;
        
        // Shock mass threshold
        const double Mshock = 6.0e11;  // Msun
        
        // Calculate (Mvir/Mshock)^(4/3)
        const double mass_ratio = Mvir_physical / Mshock;
        const double regime_criterion = pow(mass_ratio, 4.0/3.0);

        galaxies[p].Regime = (regime_criterion >= 1.0) ? 1 : 0;
        
        // Optional: store the smooth value for use in cooling calculations
        // This would require adding a new field like galaxies[p].RegimeSmooth = regime_smooth;
    }
}

float calculate_muratov_mass_loading(const int gal, struct GALAXY *galaxies, const double z, const struct params *run_params)
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
    const double Z_EXP = run_params->RedshiftPowerLawExponent;    // Redshift power-law exponent
    const double LOW_V_EXP = -3.2;  // Low velocity power-law exponent
    const double HIGH_V_EXP = -1.0; // High velocity power-law exponent

    // Calculate redshift term: (1+z)^Z_EXP
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
    
    double eta = run_params->FeedbackReheatingEpsilon * NORM * z_term * v_term;

    // Store mass loading for analysis (cast to float)
    galaxies[gal].MassLoading = (float)eta;

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
    
    // BR06 Equation (11): R_mol = (P_ext/P₀)^α
    float pressure_ratio = pressure / P0;
    float R_mol = pow(pressure_ratio, alpha);
    
    // Convert to molecular fraction: f_mol = R_mol / (1 + R_mol)
    // This is the standard conversion from molecular-to-atomic ratio to molecular fraction
    double f_mol = R_mol / (1.0 + R_mol);
    
    // Apply physical bounds
    // Apply aggressive capping for massive galaxies
    // Cap at 40% for most galaxies, with smooth transition
    // if (f_mol > 0.4) {
    //     f_mol = 0.4 + 0.2 * tanh((f_mol - 0.4) / 0.15);
    // }
    
    return f_mol;
}