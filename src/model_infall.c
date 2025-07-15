#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_infall.h"
#include "model_misc.h"

// Add a debug counter to limit output
static long debug_counter = 0;

// Equation (18): A parameter - ADD DEBUG
double calculate_A_parameter(const double z, const struct params *run_params)
{
    const double a = 1.0 / (1.0 + z);  // Expansion factor
    const double Delta_200 = 200.0;    // Virial density factor (approximately)
    const double Omega_m_03 = run_params->Omega / 0.3;
    const double h_07 = run_params->Hubble_h / 0.7;
    
    // A ≡ (Δ₂₀₀ Ω_m,0.3 h²₀.₇)^(-1/3) a
    double A = pow(Delta_200 * Omega_m_03 * h_07 * h_07, -1.0/3.0) * a;
    
    // DEBUG: Print every 10000th calculation
    // if (debug_counter % 500000 == 0) {
    //     printf("DEBUG A-param: z=%.3f, a=%.3f, Ω_m=%.3f, h=%.3f → A=%.6f\n", 
    //            z, a, run_params->Omega, run_params->Hubble_h, A);
    // }
    
    return A;
}

// Equation (30): F factor  
double calculate_F_factor(void)
{
    // These are the crude estimates from Section 3.6 of the paper
    const double f_r = 0.1;        // r/R_v for inner halo
    const double f_u = 2.5;        // u(r)/u(R_v) 
    const double f_rho = 100;    // ρ(r)/ρ(R_v)
    const double f_b_005 = 1.0;    // f_b / 0.05
    const double f_rho_017 = 1.0;  // (ρ/ρ̄) / 0.17
    const double u_tilde_s = 0.0;  // shock velocity (at rest)
    
    // F ≡ f_r f_u^(-1) / (f_ρ f_b,0.05 f_ρ,0.17 (1-3ũs)^(-1))
    double F = (f_r / f_u) / (f_rho * f_b_005 * f_rho_017 * (1.0 - 3.0 * u_tilde_s));
    
    return F;
}

double calculate_cosmic_metallicity(const double z)
{
    // Dekel & Birnboim (2006) Section 3.4: Z(z) = Z_0 * 10^(-s*z)
    const double Z0 = 0.03;     // Solar units - their fiducial value
    const double s = 0.17;      // Enrichment rate
    
    double Z_cosmic = Z0 * pow(10.0, -s * z);
    
    return Z_cosmic;
}

double calculate_mshock(const double z, const struct params *run_params)
{
    // Equation (34): M₁₁ ≃ 25.9 A^(3/8) (Z^0.7/0.03 F)^(3/4) f_u^(-3) (1 + ũs)^(-3)
    
    const double A = calculate_A_parameter(z, run_params);
    const double Z_cosmic = calculate_cosmic_metallicity(z);
    const double Z_ref = 0.03;
    const double F = calculate_F_factor();
    const double f_u = 2.5;         // From Section 3.5
    const double u_tilde_s = 0.0;   // Shock at rest
    
    // Break down the calculation for debugging
    double A_term = pow(A, 3.0/8.0);
    double Z_term = pow(Z_cosmic/Z_ref, 0.7);
    double ZF_term = pow(Z_term * F, 3.0/4.0);
    double fu_term = pow(f_u, -3.0);
    double us_term = pow(1.0 + u_tilde_s, -3.0);
    
    // Calculate M₁₁ (in units of 10^11 M_sun)
    double M11 = 25.9 * A_term * ZF_term * fu_term * us_term;
    
    // Convert to M_sun
    double Mshock_Msun = M11 * 1.0e11;
    
    // Convert to SAGE units: 10^10 M_sun/h
    double Mshock_SAGE = Mshock_Msun / (1.0e10 / run_params->Hubble_h);
    
    // DEBUG: Print detailed breakdown every 10000th call
    // if (debug_counter % 500000 == 0) {
    //     printf("DEBUG Mshock: z=%.3f\n", z);
    //     printf("  A=%.6f → A^(3/8)=%.6f\n", A, A_term);
    //     printf("  Z_cosmic=%.6f, Z_ref=%.3f → (Z/Z_ref)^0.7=%.6f\n", Z_cosmic, Z_ref, Z_term);
    //     printf("  F=%.6f → (Z*F)^(3/4)=%.6f\n", F, ZF_term);
    //     printf("  f_u=%.1f → f_u^(-3)=%.6f\n", f_u, fu_term);
    //     printf("  M11=%.3f → M_shock=%.3e M_sun → %.3e SAGE units\n", M11, Mshock_Msun, Mshock_SAGE);
    // }
    
    return Mshock_SAGE;
}

// NEW: Implementation of Equation (40) - Cold streams in hot halos
double calculate_mstream_max(const double z, const struct params *run_params)
{
    const double f = 3.0;  // Cosmic web filament factor
    
    double Mshock = calculate_mshock(z, run_params);
    double Mstar_z = calculate_press_schechter_mass(z, run_params);
    
    // Equation (40): M_stream = M_shock² / (f × M*(z)) when f×M*(z) < M_shock
    if (f * Mstar_z < Mshock) {
        return (Mshock * Mshock) / (f * Mstar_z);
    } else {
        return Mshock;  // No cold streams above M_shock when f×M*(z) >= M_shock
    }
}

double calculate_press_schechter_mass(const double z, const struct params *run_params)
{
    // Their approximation: log M* ≃ 13.1 - 1.3*z (for z ≤ 2)
    double log_Mstar_Msun = 13.1 - 1.3 * z;
    double Mstar_Msun = pow(10.0, log_Mstar_Msun);
    
    // Convert to SAGE units: 10^10 M_sun/h  
    double Mstar_SAGE = Mstar_Msun / (1.0e10 / run_params->Hubble_h);

    // DEBUG: Print every 10000th calculation
    // if (debug_counter % 500000 == 0) {
    //     printf("DEBUG M*: z=%.3f → log_M*=%.3f → M*=%.3e M_sun → %.3e SAGE units\n", 
    //            z, log_Mstar_Msun, Mstar_Msun, Mstar_SAGE);
    // }
    
    return Mstar_SAGE;
}

double calculate_critical_mass_dekel_birnboim_2006(const double z, const struct params *run_params, struct GALAXY *galaxies, const int gal)
{
    // Dekel & Birnboim (2006) critical mass calculation
    // Based on their equations (40) and (43)
    // Option 1
    // Use the paper's fundamental stability criterion: tcool = tcomp
    // Equation (29): T6 * Λ^(-1) = 1.04 A^(-3/2) F
    // But solve this properly, not with crude estimates
    
    // From the paper's Figure 2 results, use their empirical fit
    // At z=0: Mcrit ≃ 6×10^11 M☉ for Z0=0.1 (their middle curve)
    
    // const double Mcrit_z0_Msun = 6.0e11;  // From their Figure 2
    
    // // The paper shows Mcrit is roughly constant with redshift
    // // Their Figure 2 shows very weak redshift dependence
    // const double redshift_factor = 1.0;  // Approximately constant per Figure 2
    
    // const double Mcrit_Msun = Mcrit_z0_Msun * redshift_factor;
    
    // // Convert to SAGE units (10^10 M_sun/h)
    // const double Mcrit_SAGE = Mcrit_Msun / (1.0e10 / run_params->Hubble_h);
    
    // // Debug output matching the paper's approach
    // if (debug_counter % 200000 == 0) {
    //     printf("DEBUG DB06 PAPER: z=%.3f, M_crit=%.3e M_sun (%.3e SAGE units)\n", 
    //            z, Mcrit_Msun, Mcrit_SAGE);
    //     printf("  Based on Figure 2 from Dekel & Birnboim 2006\n");
    // }
    
    // return Mcrit_SAGE;

    // Option 2
    // Core physics: compare f*M*(z) vs M_shock to determine regime
    const double f = 3.0;
    
    double Mshock = calculate_mshock(z, run_params);
    double Mstar_z = calculate_press_schechter_mass(z, run_params);  // M*(z) at current redshift
    // double Mstar_z = galaxies[gal].StellarMass;
    
    double Mcrit;
    const char* regime;
    
    // Equation (43) logic: check if f×M*(z) < M_shock
    if (f * Mstar_z < Mshock) {
        // High redshift regime: f×M*(z) < M_shock, so z > z_crit
        // Use equation (40): M_crit = M_shock² / (f×M*(z))
        Mcrit = (Mshock * Mshock) / (f * Mstar_z);
        regime = "HIGH-z: Cold streams penetrate";
    } else {
        // Low redshift regime: f×M*(z) >= M_shock, so z <= z_crit  
        // Use equation (43): M_crit = M_shock (no cold streams)
        Mcrit = Mshock;
        regime = "LOW-z: No cold streams";
    }

    // // DEBUG: Print every 10000th calculation
    // if (debug_counter % 200000 == 0) {
    //     printf("DEBUG Mcrit: z=%.3f\n", z);
    //     printf("  M_shock=%.3e, M*=%.3e, f*M*=%.3e\n", Mshock, Mstar_z, f * Mstar_z);
    //     printf("  f*M* < M_shock? %s\n", (f * Mstar_z < Mshock) ? "YES" : "NO");
    //     printf("  REGIME: %s\n", regime);
    //     printf("  M_crit=%.3e SAGE units\n", Mcrit);
    // }

    // // DEBUG: Print intermediate values
    // if (debug_counter % 200000 == 0) {
    //     printf("=== DEKEL & BIRNBOIM DEBUG ===\n");
    //     printf("z=%.3f\n", z);
    //     printf("M_shock = %.3e SAGE units\n", Mshock);
    //     printf("M*(z) = %.3e SAGE units\n", Mstar_z);
    //     printf("f * M*(z) = %.3e SAGE units\n", f * Mstar_z);
    //     printf("f * M*(z) < M_shock? %s\n", (f * Mstar_z < Mshock) ? "YES" : "NO");
    //     // Also print the F factor components
    //     const double A = calculate_A_parameter(z, run_params);
    //     const double F = calculate_F_factor();
    //     printf("A parameter = %.6f\n", A);
    //     printf("F factor = %.6f\n", F);
    // }

    return Mcrit;

    // Option 3
    // const double f = 3.0;  // Cosmic web filament factor (from paper)
    
    // // Calculate M*(z) using the paper's prescription
    // double Mstar_z = calculate_press_schechter_mass(z, run_params);
    
    // // CALIBRATED M_shock: Use the paper's scaling but calibrate the normalization
    // // From Figure 2, at z=0: M_crit ≈ 6×10^11 M_sun
    // // From equation (34): M_shock ∝ A^(3/8)
    // const double A = calculate_A_parameter(z, run_params);
    // const double A_z0 = calculate_A_parameter(0.0, run_params);
    
    // // Use Figure 2 as calibration point
    // const double Mcrit_z0_Msun = 6.0e11;  // From Figure 2
    // const double Mcrit_z0_SAGE = Mcrit_z0_Msun / (1.0e10 / run_params->Hubble_h);
    
    // // Apply the paper's redshift scaling

    // Base value from Figure 2
    // const double Mcrit_z0_Msun = 6.0e11;
    
    // // Simple redshift evolution from A parameter only
    // const double A_z = calculate_A_parameter(z, run_params);
    // const double A_z0 = calculate_A_parameter(0.0, run_params);
    
    // // M_crit ∝ A^(3/8) from D&B06 equation (34)
    // const double redshift_factor = pow(A_z / A_z0, 3.0/8.0);
    
    // const double Mcrit_Msun = Mcrit_z0_Msun * redshift_factor;
    
    // return Mcrit_Msun / (1.0e10 / run_params->Hubble_h);

    // Option 4: Smooth transition between cold and hot regimes
    // This is a more advanced approach that smoothly transitions between the two regimes
    // based on the ratio of M_shock to f * M*(z).
    // This avoids sharp transitions and provides a more gradual change in behavior.
//     const double f = 3.0;
//     double Mshock = calculate_mshock(z, run_params);
//     double Mstar_z = calculate_press_schechter_mass(z, run_params);
    
//     // Instead of sharp if/else, use smooth transition
//     double ratio = (f * Mstar_z) / Mshock;
//     double smoothing_width = 0.5;  // Controls transition sharpness
    
//     // transition_factor: -1 → +1 as ratio goes from small → large
//     double transition_factor = tanh((log(ratio) - log(1.0)) / smoothing_width);
    
//     // Smooth interpolation between the two regimes
//     double Mcrit_cold = (Mshock * Mshock) / (f * Mstar_z);  // Equation (40)
//     double Mcrit_hot = Mshock;                               // Equation (43)
    
//     // Interpolate: when ratio << 1 → Mcrit_cold, when ratio >> 1 → Mcrit_hot
//     double Mcrit = Mcrit_hot + 0.5 * (1.0 - transition_factor) * (Mcrit_cold - Mcrit_hot);

//     if (debug_counter % 100000 == 0) {
//     printf("SMOOTH D&B: z=%.3f, M_crit=%.3e, ratio=%.3f, transition_factor=%.3f\n", 
//            z, Mcrit, ratio, transition_factor);
// }
    
//     return Mcrit;

    //Option 5

    // const double f = 3.0;
    // double Mshock = calculate_mshock(z, run_params);
    // double Mstar_z = calculate_press_schechter_mass(z, run_params);
    
    // // Calculate the ratio that determines the regime
    // double ratio = (f * Mstar_z) / Mshock;
    // double smoothing_width = 0.3;  // Controls transition sharpness
    // double transition_factor = tanh((log(ratio) - log(1.0)) / smoothing_width);
    
    // // Calculate critical masses for both regimes
    // double Mcrit_cold = (Mshock * Mshock) / (f * Mstar_z);  // Equation (40)
    // double Mcrit_max = 10.0 * Mshock;  // Maximum allowed critical mass (to prevent runaway growth)
    // if (Mcrit_cold > Mcrit_max) {
    //     Mcrit_cold = Mcrit_max;
    // }
    // double Mcrit_hot = Mshock;                               // Equation (43)
    
    // // Smooth interpolation between regimes
    // double Mcrit = Mcrit_hot + 0.5 * (1.0 - transition_factor) * (Mcrit_cold - Mcrit_hot);
    
    // // DEBUG: Print detailed breakdown every 100,000th call
    // if (debug_counter % 100000 == 0) {
    //     printf("=== DEKEL & BIRNBOIM HYBRID DEBUG ===\n");
    //     printf("z=%.3f\n", z);
    //     printf("M_shock = %.3e SAGE units\n", Mshock);
    //     printf("M*(z) = %.3e SAGE units\n", Mstar_z);
    //     printf("f * M*(z) = %.3e SAGE units (f=%.1f)\n", f * Mstar_z, f);
    //     printf("Ratio (f*M*/M_shock) = %.3f\n", ratio);
    //     printf("Log(ratio) = %.3f\n", log(ratio));
    //     printf("Transition factor = %.3f (smoothing_width=%.2f)\n", transition_factor, smoothing_width);
    //     printf("M_crit_COLD (Eq.40) = %.3e SAGE units\n", Mcrit_cold);
    //     printf("M_crit_HOT (Eq.43)  = %.3e SAGE units\n", Mcrit_hot);
    //     printf("M_crit_FINAL = %.3e SAGE units\n", Mcrit);
        
    //     // Regime interpretation
    //     if (transition_factor < -0.5) {
    //         printf("REGIME: COLD STREAMS DOMINANT (high-z, f*M* << M_shock)\n");
    //     } else if (transition_factor > 0.5) {
    //         printf("REGIME: SHOCK HEATED DOMINANT (low-z, f*M* >> M_shock)\n");
    //     } else {
    //         printf("REGIME: TRANSITION ZONE (intermediate regime)\n");
    //     }
        
    //     // Show the weighting
    //     double cold_weight = 0.5 * (1.0 - transition_factor);
    //     double hot_weight = 1.0 - cold_weight;
    //     printf("Weights: %.1f%% HOT + %.1f%% COLD\n", hot_weight*100, cold_weight*100);
    //     printf("================================\n\n");
    // }
    
    // // More frequent summary output every 50,000th call
    // if (debug_counter % 50000 == 0 && debug_counter % 100000 != 0) {
    //     const char* regime_str;
    //     if (transition_factor < -0.5) {
    //         regime_str = "COLD-DOMINATED";
    //     } else if (transition_factor > 0.5) {
    //         regime_str = "HOT-DOMINATED";
    //     } else {
    //         regime_str = "TRANSITION";
    //     }
        
    //     printf("D&B SUMMARY: z=%.2f, ratio=%.2f, M_crit=%.2e, regime=%s\n", 
    //            z, ratio, Mcrit, regime_str);
    // }

    // // if (z > 6.0) {
    // //     // At very high z, use a constant M_crit based on Figure 7
    // //     const double Mcrit_highz_Msun = 1.0e12;  // From Figure 7
    // //     Mcrit = Mcrit_highz_Msun / (1.0e10 / run_params->Hubble_h);
        
    // //     if (debug_counter % 50000 == 0) {
    // //         printf("HIGH-Z OVERRIDE: z=%.2f, using constant M_crit=%.2e\n", z, Mcrit);
    // //     }
    // // }
    // // After your current calculation but before the return
    // double Mcrit_final = Mcrit;  // Save the theoretical result

    // // Apply smooth transition near critical redshift (where ratio ≈ 1)
    // if (z > 3.0) {  // Apply smoothing in the problematic redshift range
    //     const double Mcrit_empirical = 1.0e12 / (1.0e10 / run_params->Hubble_h);  // ~10^12 M_sun
        
    //     // Smooth transition based on how extreme the theoretical value is
    //     double excess_factor = Mcrit / Mshock;  // How many times larger than M_shock
        
    //     if (excess_factor > 100.0) {  // If M_crit >> M_shock, apply smoothing
    //         double smoothing_strength = tanh((z - 3.0) / 1.0);  // Stronger smoothing at higher z
    //         double max_allowed = Mshock * (100.0 + 900.0 * smoothing_strength);  // Allow 100x to 1000x M_shock
            
    //         if (Mcrit > max_allowed) {
    //             // Smooth blend between theoretical and capped value
    //             double blend_factor = 1.0 / (1.0 + (excess_factor / 1000.0));
    //             Mcrit = blend_factor * Mcrit + (1.0 - blend_factor) * max_allowed;
    //         }
    //     }
        
    //     // Additional high-z empirical transition
    //     if (z > 7.0) {
    //         double z_weight = tanh((z - 6.0) / 1.0);  // Smooth transition starting at z=6
    //         Mcrit = (1.0 - z_weight) * Mcrit + z_weight * Mcrit_empirical;
    //     }
        
    //     if (debug_counter % 50000 == 0 && Mcrit != Mcrit_final) {
    //         printf("SMOOTHING APPLIED: z=%.2f, theoretical=%.2e, final=%.2e (excess=%.1fx)\n", 
    //             z, Mcrit_final, Mcrit, excess_factor);
    //     }
    // }
    
    // // Very frequent basic output for key transitions
    // if (debug_counter % 10000 == 0) {
    //     // Check if we're near the critical redshift where f*M*(z) ≈ M_shock
    //     if (fabs(log(ratio)) < 0.5) {  // Within factor of ~1.6 of equality
    //         printf("*** NEAR CRITICAL REDSHIFT: z=%.3f, f*M*/M_shock=%.3f ***\n", z, ratio);
    //     }
        
    //     // Check for extreme values that might indicate problems
    //     if (Mcrit > 10.0 * Mshock) {
    //         printf("WARNING: M_crit (%.2e) >> M_shock (%.2e) at z=%.2f\n", Mcrit, Mshock, z);
    //     }
    //     if (Mcrit < 0.1 * Mshock) {
    //         printf("WARNING: M_crit (%.2e) << M_shock (%.2e) at z=%.2f\n", Mcrit, Mshock, z);
    //     }
    // }
    
    // return Mcrit;
}

double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params)
{
    double tot_stellarMass, tot_BHMass, tot_coldMass, tot_hotMass, tot_ejected, tot_ICS;
    double tot_ejectedMetals, tot_ICSMetals;
    double infallingMass, reionization_modifier;

    // need to add up all the baryonic mass asociated with the full halo
    tot_stellarMass = tot_coldMass = tot_hotMass = tot_ejected = tot_BHMass = tot_ejectedMetals = tot_ICS = tot_ICSMetals = 0.0;

	// loop over all galaxies in the FoF-halo
    for(int i = 0; i < ngal; i++) {
        tot_stellarMass += galaxies[i].StellarMass;
        tot_BHMass += galaxies[i].BlackHoleMass;
        tot_coldMass += galaxies[i].ColdGas;
        tot_hotMass += galaxies[i].HotGas;
        tot_ejected += galaxies[i].CGMgas;
        tot_ejectedMetals += galaxies[i].MetalsCGMgas;
        tot_ICS += galaxies[i].ICS;
        tot_ICSMetals += galaxies[i].MetalsICS;


        if(i != centralgal) {
            // satellite ejected gas goes to central ejected reservior
            galaxies[i].CGMgas = galaxies[i].MetalsCGMgas = 0.0;

            // satellite ICS goes to central ICS
            galaxies[i].ICS = galaxies[i].MetalsICS = 0.0;
        }
    }

    // include reionization if necessary
    if(run_params->ReionizationOn) {
        reionization_modifier = do_reionization(centralgal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

    infallingMass =
        reionization_modifier * run_params->BaryonFrac * galaxies[centralgal].Mvir - (tot_stellarMass + tot_coldMass + tot_hotMass + tot_ejected + tot_BHMass + tot_ICS);
    /* reionization_modifier * run_params->BaryonFrac * galaxies[centralgal].deltaMvir - newSatBaryons; */

    // the central galaxy keeps all the ejected mass
    galaxies[centralgal].CGMgas = tot_ejected;
    galaxies[centralgal].MetalsCGMgas = tot_ejectedMetals;

    if(galaxies[centralgal].MetalsCGMgas > galaxies[centralgal].CGMgas) {
        galaxies[centralgal].MetalsCGMgas = galaxies[centralgal].CGMgas;
    }

    if(galaxies[centralgal].CGMgas < 0.0) {
        galaxies[centralgal].CGMgas = galaxies[centralgal].MetalsCGMgas = 0.0;
    }

    if(galaxies[centralgal].MetalsCGMgas < 0.0) {
        galaxies[centralgal].MetalsCGMgas = 0.0;
    }

    // the central galaxy keeps all the ICS (mostly for numerical convenience)
    galaxies[centralgal].ICS = tot_ICS;
    galaxies[centralgal].MetalsICS = tot_ICSMetals;

    if(galaxies[centralgal].MetalsICS > galaxies[centralgal].ICS) {
        galaxies[centralgal].MetalsICS = galaxies[centralgal].ICS;
    }

    if(galaxies[centralgal].ICS < 0.0) {
        galaxies[centralgal].ICS = galaxies[centralgal].MetalsICS = 0.0;
    }

    if(galaxies[centralgal].MetalsICS < 0.0) {
        galaxies[centralgal].MetalsICS = 0.0;
    }

    return infallingMass;
}



void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params)
{
    double reionization_modifier;

    if(run_params->ReionizationOn) {
        /* reionization_modifier = do_reionization(gal, ZZ[halos[halonr].SnapNum]); */
        reionization_modifier = do_reionization(gal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

    double strippedGas = -1.0 *
        (reionization_modifier * run_params->BaryonFrac * galaxies[gal].Mvir - (galaxies[gal].StellarMass + galaxies[gal].ColdGas + galaxies[gal].HotGas + galaxies[gal].CGMgas + galaxies[gal].BlackHoleMass + galaxies[gal].ICS) ) / STEPS;
    // ( reionization_modifier * run_params->BaryonFrac * galaxies[gal].deltaMvir ) / STEPS;

    if(strippedGas > 0.0) {
        const double metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        double strippedGasMetals = strippedGas * metallicity;

        if(strippedGas > galaxies[gal].HotGas) strippedGas = galaxies[gal].HotGas;
        if(strippedGasMetals > galaxies[gal].MetalsHotGas) strippedGasMetals = galaxies[gal].MetalsHotGas;

        galaxies[gal].HotGas -= strippedGas;
        galaxies[gal].MetalsHotGas -= strippedGasMetals;

        galaxies[centralgal].HotGas += strippedGas;
        galaxies[centralgal].MetalsHotGas += strippedGas * metallicity;
    }

}



double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params)
{
    double f_of_a;

    // we employ the reionization recipie described in Gnedin (2000), however use the fitting
    // formulas given by Kravtsov et al (2004) Appendix B

    // here are two parameters that Kravtsov et al keep fixed, alpha gives the best fit to the Gnedin data
    const double alpha = 6.0;
    const double Tvir = 1e4;

    // calculate the filtering mass
    const double a = 1.0 / (1.0 + Zcurr);
    const double a_on_a0 = a / run_params->a0;
    const double a_on_ar = a / run_params->ar;

    if(a <= run_params->a0) {
        f_of_a = 3.0 * a / ((2.0 + alpha) * (5.0 + 2.0 * alpha)) * pow(a_on_a0, alpha);
    } else if((a > run_params->a0) && (a < run_params->ar)) {
        f_of_a =
            (3.0 / a) * run_params->a0 * run_params->a0 * (1.0 / (2.0 + alpha) - 2.0 * pow(a_on_a0, -0.5) / (5.0 + 2.0 * alpha)) +
            a * a / 10.0 - (run_params->a0 * run_params->a0 / 10.0) * (5.0 - 4.0 * pow(a_on_a0, -0.5));
    } else {
        f_of_a =
            (3.0 / a) * (run_params->a0 * run_params->a0 * (1.0 / (2.0 + alpha) - 2.0 * pow(a_on_a0, -0.5) / (5.0 + 2.0 * alpha)) +
                         (run_params->ar * run_params->ar / 10.0) * (5.0 - 4.0 * pow(a_on_ar, -0.5)) - (run_params->a0 * run_params->a0 / 10.0) * (5.0 -
                                                                                                   4.0 / sqrt(a_on_a0)
                                                                                                   ) +
                         a * run_params->ar / 3.0 - (run_params->ar * run_params->ar / 3.0) * (3.0 - 2.0 / sqrt(a_on_ar)));
    }

    // this is in units of 10^10Msun/h, note mu=0.59 and mu^-1.5 = 2.21
    const double Mjeans = 25.0 / sqrt(run_params->Omega) * 2.21;
    const double Mfiltering = Mjeans * pow(f_of_a, 1.5);

    // calculate the characteristic mass coresponding to a halo temperature of 10^4K
    const double Vchar = sqrt(Tvir / 36.0);
    const double omegaZ = run_params->Omega * (CUBE(1.0 + Zcurr) / (run_params->Omega * CUBE(1.0 + Zcurr) + run_params->OmegaLambda));
    const double xZ = omegaZ - 1.0;
    const double deltacritZ = 18.0 * M_PI * M_PI + 82.0 * xZ - 39.0 * xZ * xZ;
    const double HubbleZ = run_params->Hubble * sqrt(run_params->Omega * CUBE(1.0 + Zcurr) + run_params->OmegaLambda);

    const double Mchar = Vchar * Vchar * Vchar / (run_params->G * HubbleZ * sqrt(0.5 * deltacritZ));

    // we use the maximum of Mfiltering and Mchar
    const double mass_to_use = dmax(Mfiltering, Mchar);
    const double modifier = 1.0 / CUBE(1.0 + 0.26 * (mass_to_use / galaxies[gal].Mvir));

    return modifier;

}



// void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies)
// {
//     float metallicity;

//     // if the halo has lost mass, subtract baryons from the ejected mass first, then the hot gas
//     if(infallingGas < 0.0 && galaxies[gal].CGMgas > 0.0) {
//         metallicity = get_metallicity(galaxies[gal].CGMgas, galaxies[gal].MetalsCGMgas);
//         galaxies[gal].MetalsCGMgas += infallingGas*metallicity;
//         if(galaxies[gal].MetalsCGMgas < 0.0) galaxies[gal].MetalsCGMgas = 0.0;

//         galaxies[gal].CGMgas += infallingGas;
//         if(galaxies[gal].CGMgas < 0.0) {
//             infallingGas = galaxies[gal].CGMgas;
//             galaxies[gal].CGMgas = galaxies[gal].MetalsCGMgas = 0.0;
//         } else {
//             infallingGas = 0.0;
//         }
//     }

//     // if the halo has lost mass, subtract hot metals mass next, then the hot gas
//     if(infallingGas < 0.0 && galaxies[gal].MetalsHotGas > 0.0) {
//         metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
//         galaxies[gal].MetalsHotGas += infallingGas*metallicity;
//         if(galaxies[gal].MetalsHotGas < 0.0) galaxies[gal].MetalsHotGas = 0.0;
//     }

//     // add (subtract) the ambient (enriched) infalling gas to the central galaxy hot component
//     galaxies[gal].HotGas += infallingGas;
//     if(galaxies[gal].HotGas < 0.0) galaxies[gal].HotGas = galaxies[gal].MetalsHotGas = 0.0;

// }

void add_infall_to_hot(const int gal, double infallingGas, const double z, struct GALAXY *galaxies, const struct params *run_params)
{
    float metallicity;
    
    // Increment debug counter for each galaxy processed
    debug_counter++;

    // Handle negative infall (mass loss) - keep existing logic
    if(infallingGas < 0.0 && galaxies[gal].CGMgas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].CGMgas, galaxies[gal].MetalsCGMgas);
        galaxies[gal].MetalsCGMgas += infallingGas*metallicity;
        if(galaxies[gal].MetalsCGMgas < 0.0) galaxies[gal].MetalsCGMgas = 0.0;

        galaxies[gal].CGMgas += infallingGas;
        if(galaxies[gal].CGMgas < 0.0) {
            infallingGas = galaxies[gal].CGMgas;
            galaxies[gal].CGMgas = galaxies[gal].MetalsCGMgas = 0.0;
        } else {
            infallingGas = 0.0;
        }
        // DEBUG for negative infall
        // if (debug_counter % 500000 == 0) {
        //     printf("DEBUG Infall: NEGATIVE infall=%.3e from CGM (metallicity=%.6f)\n", 
        //            infallingGas, metallicity);
        // }
    }

    if(infallingGas < 0.0 && galaxies[gal].MetalsHotGas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        galaxies[gal].MetalsHotGas += infallingGas*metallicity;
        if(galaxies[gal].MetalsHotGas < 0.0) galaxies[gal].MetalsHotGas = 0.0;

        // DEBUG for negative infall from hot gas
        // if (debug_counter % 500000 == 0) {
        //     printf("DEBUG Infall: NEGATIVE infall=%.3e from HotGas (metallicity=%.6f)\n", 
        //            infallingGas, metallicity);
        // }
    }

    // CORRECTED: Apply exact Dekel & Birnboim physics for positive infall
    if(infallingGas > 0.0) {
        double Mcrit = calculate_critical_mass_dekel_birnboim_2006(z, run_params, galaxies, gal);
        // double Z_cosmic = calculate_cosmic_metallicity(z);  // Use cosmic metallicity evolution
        // const double igm_metallicity = 0.02;  // IGM metallicity (~2% solar)
        // float metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);

        // Store diagnostics
        galaxies[gal].CriticalMassDB06 = Mcrit;
        galaxies[gal].MvirToMcritRatio = galaxies[gal].Mvir / Mcrit;


        if (galaxies[gal].Mvir < Mcrit) {
            // "cold streams prevail" - gas can reach galaxy center
            // Use existing hot gas metallicity (or small default if no hot gas exists)
            // float metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
            metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
            galaxies[gal].ColdGas += infallingGas;
            galaxies[gal].MetalsColdGas += infallingGas * metallicity;

            galaxies[gal].InflowRegime = 0;
            galaxies[gal].ColdInflowMass += infallingGas;
            galaxies[gal].ColdInflowMetals += infallingGas * metallicity;
            // DEBUG: Cold stream case
            // if (debug_counter % 90000 == 0) {  // More frequent for positive infall
            //     printf("DEBUG Infall: COLD STREAM z=%.3f\n", z);
            //     printf("  M_vir=%.3e < M_crit=%.3e → COLD STREAM\n", galaxies[gal].Mvir, Mcrit);
            //     printf("  Infall=%.3e → ColdGas, metallicity=%.6f\n", infallingGas, metallicity);
            //     printf("  New ColdGas=%.3e, MetalsColdGas=%.3e\n", 
            //            galaxies[gal].ColdGas, galaxies[gal].MetalsColdGas);
            // }
        } else {
            // "shutdown of gas supply" - gas goes to hot but stays hot
            metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
            galaxies[gal].HotGas += infallingGas;
            galaxies[gal].MetalsHotGas += infallingGas * metallicity;

            galaxies[gal].InflowRegime = 1; 
            galaxies[gal].HotInflowMass += infallingGas;
            galaxies[gal].HotInflowMetals += infallingGas * metallicity;
            // DEBUG: Shock heated case
            // if (debug_counter % 10000 == 0) {  // More frequent for positive infall
            //     printf("DEBUG Infall: SHOCK HEATED z=%.3f\n", z);
            //     printf("  M_vir=%.3e >= M_crit=%.3e → SHOCK HEATED\n", galaxies[gal].Mvir, Mcrit);
            //     printf("  Infall=%.3e → HotGas, metallicity=%.6f\n", infallingGas, metallicity);
            //     printf("  New HotGas=%.3e, MetalsHotGas=%.3e\n", 
            //            galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
            // }   
        }
    } else {
        // Negative infall case - use original logic
        galaxies[gal].HotGas += infallingGas;
        if(galaxies[gal].HotGas < 0.0) galaxies[gal].HotGas = galaxies[gal].MetalsHotGas = 0.0;
    }
}