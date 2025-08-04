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
    
    return Mstar_SAGE;
}

double calculate_critical_mass_dekel_birnboim_2006(const double z, const struct params *run_params)
{
    // Core physics: compare f*M*(z) vs M_shock to determine regime
    const double f = 3.0;
    
    double Mshock = calculate_mshock(z, run_params);
    double Mstar_z = calculate_press_schechter_mass(z, run_params);  // M*(z) at current redshift
    // double Mstar_z = galaxies[gal].StellarMass;
    
    double Mcrit;
    // const char* regime;
    
    // Equation (43) logic: check if f×M*(z) < M_shock
    if (f * Mstar_z < Mshock) {
        // High redshift regime: f×M*(z) < M_shock, so z > z_crit
        // Use equation (40): M_crit = M_shock² / (f×M*(z))
        Mcrit = (Mshock * Mshock) / (f * Mstar_z);
        // regime = "HIGH-z: Cold streams penetrate";
    } else {
        // Low redshift regime: f×M*(z) >= M_shock, so z <= z_crit  
        // Use equation (43): M_crit = M_shock (no cold streams)
        Mcrit = Mshock;
        // regime = "LOW-z: No cold streams";
    }

    return Mcrit;
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



void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params, const double dt)
{
    double reionization_modifier;

    if(run_params->ReionizationOn) {
        /* reionization_modifier = do_reionization(gal, ZZ[halos[halonr].SnapNum]); */
        reionization_modifier = do_reionization(gal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

    double strippedGas = -1.0 *
        (reionization_modifier * run_params->BaryonFrac * galaxies[gal].Mvir - (galaxies[gal].StellarMass + galaxies[gal].ColdGas + galaxies[gal].HotGas + galaxies[gal].CGMgas + galaxies[gal].BlackHoleMass + galaxies[gal].ICS) ) * dt;
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
    }

    if(infallingGas < 0.0 && galaxies[gal].MetalsHotGas > 0.0) {
        metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
        galaxies[gal].MetalsHotGas += infallingGas*metallicity;
        if(galaxies[gal].MetalsHotGas < 0.0) galaxies[gal].MetalsHotGas = 0.0;

    }

    // CORRECTED: Apply exact Dekel & Birnboim physics for positive infall
    if(infallingGas > 0.0) {
        double Mcrit = calculate_critical_mass_dekel_birnboim_2006(z, run_params);

        // Store diagnostics
        galaxies[gal].CriticalMassDB06 = Mcrit;
        galaxies[gal].MvirToMcritRatio = galaxies[gal].Mvir / Mcrit;


        if (galaxies[gal].Mvir < Mcrit) {
            // "cold streams prevail" - gas can reach galaxy center
            metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
            galaxies[gal].ColdGas += infallingGas;
            galaxies[gal].MetalsColdGas += infallingGas * metallicity;

            galaxies[gal].InflowRegime = 0;
            galaxies[gal].ColdInflowMass += infallingGas;
            galaxies[gal].ColdInflowMetals += infallingGas * metallicity;

        } else {
            // "shutdown of gas supply" - gas goes to hot but stays hot
            metallicity = get_metallicity(galaxies[gal].HotGas, galaxies[gal].MetalsHotGas);
            galaxies[gal].HotGas += infallingGas;
            galaxies[gal].MetalsHotGas += infallingGas * metallicity;

            galaxies[gal].InflowRegime = 1; 
            galaxies[gal].HotInflowMass += infallingGas;
            galaxies[gal].HotInflowMetals += infallingGas * metallicity;

        }
    } else {
        // Negative infall case - use original logic
        galaxies[gal].HotGas += infallingGas;
        if(galaxies[gal].HotGas < 0.0) galaxies[gal].HotGas = galaxies[gal].MetalsHotGas = 0.0;
    }
}