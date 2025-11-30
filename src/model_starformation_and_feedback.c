#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_starformation_and_feedback.h"
#include "model_misc.h"
#include "model_disk_instability.h"


void starformation_and_feedback(const int p, const int centralgal, const double time, const double dt, const int halonr, const int step,
                                struct GALAXY *galaxies, const struct params *run_params)
{

    // ========================================================================
    // CHECK FOR FFB REGIME - EARLY EXIT IF FFB
    // ========================================================================
    if(galaxies[p].FFBRegime == 1) {
        // This is a Feedback-Free Burst halo
        // Use specialized FFB star formation (no feedback)
        starformation_ffb(p, centralgal, dt, step, galaxies, run_params);
        return;  // Exit early - FFB path complete
    }

    double reff, tdyn, strdot, stars, ejected_mass, metallicity, total_molecular_gas;

    // Initialise variables
    strdot = 0.0;

    // star formation recipes
    if(run_params->SFprescription == 0) {
        // we take the typical star forming region as 3.0*r_s using the Milky Way as a guide
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;

        // from Kauffmann (1996) eq7 x piR^2, (Vvir in km/s, reff in Mpc/h) in units of 10^10Msun/h
        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > cold_crit && tdyn > 0.0) {
            strdot = run_params->SfrEfficiency * (galaxies[p].ColdGas - cold_crit) / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription == 1) {
        // we take the typical star forming region as 3.0*r_s using the Milky Way as a guide
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;
        // BR06 model
        const float h = run_params->Hubble_h;
        const float rs_pc = galaxies[p].DiskScaleRadius * 1.0e6 / h;
        if (rs_pc <= 0.0) {
            galaxies[p].H2gas = 0.0;
            strdot = 0.0;
            return;
        }
        // float disk_area_pc2 = M_PI * rs_pc * rs_pc; 
        float disk_area_pc2 = M_PI * pow(3.0 * rs_pc, 2); // 3× scale radius captures ~95% of mass 
        float gas_surface_density = (galaxies[p].ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
        float stellar_surface_density = (galaxies[p].StellarMass * 1.0e10 / h) / disk_area_pc2; // M☉/pc²

        total_molecular_gas = calculate_molecular_fraction_BR06(gas_surface_density, stellar_surface_density, 
                                                               rs_pc) * galaxies[p].ColdGas;

        // total_molecular_gas = calculate_molecular_fraction_radial_integration(p, galaxies, run_params);

        galaxies[p].H2gas = total_molecular_gas;

        // const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        // if(galaxies[p].ColdGas > cold_crit) {
            // make stars only from H2
            // float area = M_PI * 9.0 * galaxies[p].DiskScaleRadius * galaxies[p].DiskScaleRadius;
            // strdot = run_params->UnitTime_in_s / SEC_PER_MEGAYEAR * run_params->SfrEfficiency * galaxies[p].H2gas;
        
            // strdot = run_params->SfrEfficiency * galaxies[p].H2gas / tdyn;
        if (galaxies[p].H2gas > 0.0 && tdyn > 0.0) {
             strdot = run_params->SfrEfficiency * galaxies[p].H2gas / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription == 2) {
        // Somerville et al. 2025: Density Modulated Star Formation Efficiency
        // Using Equation 3 for efficiency: epsilon = (Sigma/Sigma_crit)/(1 + Sigma/Sigma_crit)
        
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;
        const float h = run_params->Hubble_h;
        const float rs_pc = galaxies[p].DiskScaleRadius * 1.0e6 / h;
        float disk_area_pc2 = M_PI * pow(3.0 * rs_pc, 2); // pc^2
        float gas_surface_density = (galaxies[p].ColdGas * 1.0e10 / h) / disk_area_pc2; // Msun/pc^2
        
        // Critical surface density from Equation 2
        // Sigma_crit = <p_dot/m_star> / (pi * G)
        // <p_dot/m_star> ~ 30 km/s/Myr, G = 4.302e-3 pc (km/s)^2/Msun
        const double Sigma_crit = 30.0 / (M_PI * 4.302e-3); // ~2176 Msun/pc^2
        
        // Cloud-scale star formation efficiency from Equation 3
        double epsilon_cl = (gas_surface_density / Sigma_crit) / (1.0 + gas_surface_density / Sigma_crit);
        
        // Fraction of gas in dense clouds (f_dense from Equation 8)
        const double f_dense = 0.5; // Paper explores 0.1, 0.5, 1.0
        
        // Star formation rate: SFR ~ epsilon_cl * f_dense * m_gas / tdyn
        // This is the key: efficiency scales with density, but we use tdyn as the timescale
        if(tdyn > 0.0 && gas_surface_density > 0.0) {
            strdot = epsilon_cl * f_dense * galaxies[p].ColdGas / tdyn;
        } else {
            strdot = 0.0;
        }
    } else if(run_params->SFprescription == 3) {
        // Somerville et al. 2025: Density Modulated Star Formation Efficiency with H2
        // Using Equation 3 for efficiency: epsilon = (Sigma/Sigma_crit)/(1 + Sigma/Sigma_crit)
        // But replacing cold gas with H2 gas using Blitz & Rosolowsky 2006
        
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;
        const float h = run_params->Hubble_h;
        const float rs_pc = galaxies[p].DiskScaleRadius * 1.0e6 / h;
        
        if (rs_pc <= 0.0) {
            galaxies[p].H2gas = 0.0;
            strdot = 0.0;
        } else {
            float disk_area_pc2 = M_PI * pow(3.0 * rs_pc, 2); // pc^2
            float gas_surface_density = (galaxies[p].ColdGas * 1.0e10 / h) / disk_area_pc2; // Msun/pc^2
            float stellar_surface_density = (galaxies[p].StellarMass * 1.0e10 / h) / disk_area_pc2; // Msun/pc^2
            
            // Calculate molecular fraction using Blitz & Rosolowsky 2006
            total_molecular_gas = calculate_molecular_fraction_BR06(gas_surface_density, stellar_surface_density, 
                                                                   rs_pc) * galaxies[p].ColdGas;
            
            galaxies[p].H2gas = total_molecular_gas;
            
            // Critical surface density from Equation 2
            // Sigma_crit = <p_dot/m_star> / (pi * G)
            // <p_dot/m_star> ~ 30 km/s/Myr, G = 4.302e-3 pc (km/s)^2/Msun
            const double Sigma_crit = 30.0 / (M_PI * 4.302e-3); // ~2176 Msun/pc^2
            
            // Cloud-scale star formation efficiency from Equation 3
            double epsilon_cl = (gas_surface_density / Sigma_crit) / (1.0 + gas_surface_density / Sigma_crit);
            
            // Fraction of gas in dense clouds (f_dense from Equation 8)
            const double f_dense = 0.5; // Paper explores 0.1, 0.5, 1.0
            
            // Star formation rate using H2 gas instead of total cold gas
            // SFR ~ epsilon_cl * f_dense * H2_gas / tdyn
            if(tdyn > 0.0 && gas_surface_density > 0.0 && galaxies[p].H2gas > 0.0) {
                strdot = epsilon_cl * f_dense * galaxies[p].H2gas / tdyn;
            } else {
                strdot = 0.0;
            }
        }
    } else {
        fprintf(stderr, "No star formation prescription selected!\n");
        ABORT(0);
    }

    stars = strdot * dt;
    if(stars < 0.0) {
        stars = 0.0;
    }

    // Calculate reheated mass - use FIRE model if enabled, otherwise use original feedback
    double reheated_mass = 0.0;
    
    if(run_params->SupernovaRecipeOn == 1) {
        if(run_params->FIREmodeOn == 1) {
            // FIRE: Calculate velocity/redshift scaling from Muratov et al. 2015
            const double z = run_params->ZZ[galaxies[p].SnapNum];
            const double vc = galaxies[p].Vvir;
            const double V_CRIT = 60.0;
            
            // Check for valid inputs to avoid NaN
            if(vc <= 0.0 || z < 0.0) {
                reheated_mass = 0.0;
            } else {
                double z_term = pow(1.0 + z, run_params->RedshiftPowerLawExponent);
                double v_term;
                if (vc < V_CRIT) {
                    v_term = pow(vc / V_CRIT, -3.2);
                } else {
                    v_term = pow(vc / V_CRIT, -1.0);
                }
                double scaling_factor = z_term * v_term;
                
                // Reheating with Muratov scaling: η = 2.9 × (1+z)^α × (V/60)^β
                double eta_reheat = run_params->FeedbackReheatingEpsilon * scaling_factor;
                // Store mass loading for analysis (cast to float)
                galaxies[p].MassLoading = (float)eta_reheat;
                reheated_mass = eta_reheat * stars;
            }
        } else {
            reheated_mass = run_params->FeedbackReheatingEpsilon * stars;
        }
    }

	XASSERT(reheated_mass >= 0.0, -1,
            "Error: Expected reheated gas-mass = %g to be >=0.0\n", reheated_mass);

    // cant use more cold gas than is available! so balance SF and feedback
    if((stars + reheated_mass) > galaxies[p].ColdGas && (stars + reheated_mass) > 0.0) {
        const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
        stars *= fac;
        reheated_mass *= fac;
    }

    // determine ejection
    if(run_params->SupernovaRecipeOn == 1) {
        if(galaxies[centralgal].Vvir > 0.0) {
            if(run_params->FIREmodeOn == 1) {
                // FIRE model: Energy-based ejection following Hirschmann+2016
                // Energy from supernovae (with Muratov scaling)
                const double z = run_params->ZZ[galaxies[p].SnapNum];
                const double vc = galaxies[p].Vvir;
                const double V_CRIT = 60.0;
                
                // Check for valid inputs to avoid NaN
                if(vc <= 0.0 || z < 0.0) {
                    ejected_mass = 0.0;
                } else {
                    double z_term = pow(1.0 + z, run_params->RedshiftPowerLawExponent);
                    double v_term;
                    if (vc < V_CRIT) {
                        v_term = pow(vc / V_CRIT, -3.2);
                    } else {
                        v_term = pow(vc / V_CRIT, -1.0);
                    }
                    double scaling_factor = z_term * v_term;
                    
                    // Total feedback energy: E_FB = ε_eject × scaling × 0.5 × M_* × (η_SN × E_SN)
                    double E_FB = run_params->FeedbackEjectionEfficiency * scaling_factor * 
                                  0.5 * stars * (run_params->EtaSNcode * run_params->EnergySNcode);
                    
                    // Energy needed to lift reheated gas to virial radius: E_lift = 0.5 × M_reheat × V_vir²
                    double E_lift = 0.5 * reheated_mass * vc * vc;
                    
                    // Leftover energy ejects additional gas: E_eject = E_FB - E_lift
                    // Ejected mass: M_eject = E_eject / (0.5 × V_vir²)
                    if(E_FB > E_lift) {
                        ejected_mass = (E_FB - E_lift) / (0.5 * vc * vc);
                    } else {
                        ejected_mass = 0.0;
                    }
                }
            } else {
                // Original non-FIRE calculation
                ejected_mass = (run_params->FeedbackEjectionEfficiency * 
                               (run_params->EtaSNcode * run_params->EnergySNcode) / 
                               (galaxies[p].Vvir * galaxies[p].Vvir) -
                               run_params->FeedbackReheatingEpsilon) * stars;
            }
        } else {
            ejected_mass = 0.0;
        }
        
        if(ejected_mass < 0.0) {
            ejected_mass = 0.0;
        }
    } else {
        ejected_mass = 0.0;
    }


    // update the star formation rate
    galaxies[p].SfrDisk[step] += stars / dt;
    galaxies[p].SfrDiskColdGas[step] = galaxies[p].ColdGas;
    galaxies[p].SfrDiskColdGasMetals[step] = galaxies[p].MetalsColdGas;

    // update for star formation
    metallicity = get_metallicity(galaxies[p].ColdGas, galaxies[p].MetalsColdGas);
    update_from_star_formation(p, stars, metallicity, galaxies, run_params);

    // recompute the metallicity of the cold phase
    metallicity = get_metallicity(galaxies[p].ColdGas, galaxies[p].MetalsColdGas);

    // update from SN feedback
    update_from_feedback(p, centralgal, reheated_mass, ejected_mass, metallicity, galaxies, run_params);

    // check for disk instability
    if(run_params->DiskInstabilityOn) {
        check_disk_instability(p, centralgal, halonr, time, dt, step, galaxies, (struct params *) run_params);
    }

    // formation of new metals - instantaneous recycling approximation - only SNII
    if(galaxies[p].ColdGas > 1.0e-8) {
        const double FracZleaveDiskVal = run_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
        
        // Metals that stay in disk (same for all regimes)
        galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
        
        // Metals that leave disk - regime dependent
        const double metals_leaving_disk = run_params->Yield * FracZleaveDiskVal * stars;
        
        if(run_params->CGMrecipeOn == 1) {
            if(galaxies[centralgal].Regime == 0) {
                // CGM-regime: metals go to CGM
                galaxies[centralgal].MetalsCGMgas += metals_leaving_disk;
            } else {
                // Hot-ICM-regime: metals go to HotGas
                galaxies[centralgal].MetalsHotGas += metals_leaving_disk;
            }
        } else {
            // Original SAGE behavior: metals go to HotGas
            galaxies[centralgal].MetalsHotGas += metals_leaving_disk;
        }
        
    } else {
        // All metals leave disk when ColdGas is very low - regime dependent
        const double all_metals = run_params->Yield * stars;
        
        if(run_params->CGMrecipeOn == 1) {
            if(galaxies[centralgal].Regime == 0) {
                // CGM-regime: metals go to CGM
                galaxies[centralgal].MetalsCGMgas += all_metals;
            } else {
                // Hot-ICM-regime: metals go to HotGas
                galaxies[centralgal].MetalsHotGas += all_metals;
            }
        } else {
            // Original SAGE behavior: metals go to HotGas
            galaxies[centralgal].MetalsHotGas += all_metals;
        }
    }
}



void update_from_star_formation(const int p, const double stars, const double metallicity, struct GALAXY *galaxies, const struct params *run_params)
{
    const double RecycleFraction = run_params->RecycleFraction;
    // update gas and metals from star formation
    galaxies[p].ColdGas -= (1 - RecycleFraction) * stars;
    galaxies[p].MetalsColdGas -= metallicity * (1 - RecycleFraction) * stars;
    galaxies[p].StellarMass += (1 - RecycleFraction) * stars;
    galaxies[p].MetalsStellarMass += metallicity * (1 - RecycleFraction) * stars;
}



void update_from_feedback(const int p, const int centralgal, const double reheated_mass, double ejected_mass, const double metallicity,
                          struct GALAXY *galaxies, const struct params *run_params)
{

    XASSERT(reheated_mass >= 0.0, -1,
            "Error: For galaxy = %d (halonr = %d, centralgal = %d) with MostBoundID = %lld, the reheated mass = %g should be >=0.0",
            p, galaxies[p].HaloNr, centralgal, galaxies[p].MostBoundID, reheated_mass);
    XASSERT(reheated_mass <= galaxies[p].ColdGas, -1,
            "Error: Reheated mass = %g should be <= the coldgas mass of the galaxy = %g",
            reheated_mass, galaxies[p].ColdGas);

    if(run_params->SupernovaRecipeOn == 1) {
        // Remove reheated mass from cold gas (same for all regimes)
        galaxies[p].ColdGas -= reheated_mass;
        galaxies[p].MetalsColdGas -= metallicity * reheated_mass;

        if(run_params->CGMrecipeOn == 1) {
            if(galaxies[centralgal].Regime == 0) {
                // CGM-regime: Cold --> CGM --> Ejected
                
                // Add reheated gas to CGM
                galaxies[centralgal].CGMgas += reheated_mass;
                galaxies[centralgal].MetalsCGMgas += metallicity * reheated_mass;

                // Check if ejection is possible from CGM
                if(ejected_mass > galaxies[centralgal].CGMgas) {
                    ejected_mass = galaxies[centralgal].CGMgas;
                }
                const double metallicityCGM = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);

                // Eject from CGM to EjectedMass
                galaxies[centralgal].CGMgas -= ejected_mass;
                galaxies[centralgal].MetalsCGMgas -= metallicityCGM * ejected_mass;
                galaxies[centralgal].EjectedMass += ejected_mass;
                galaxies[centralgal].MetalsEjectedMass += metallicityCGM * ejected_mass;

            } else {
                // Hot-ICM-regime: Cold --> HotGas --> Ejected
                
                // Add reheated gas to HotGas
                galaxies[centralgal].HotGas += reheated_mass;
                galaxies[centralgal].MetalsHotGas += metallicity * reheated_mass;

                // Check if ejection is possible from HotGas
                if(ejected_mass > galaxies[centralgal].HotGas) {
                    ejected_mass = galaxies[centralgal].HotGas;
                }
                const double metallicityHot = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);

                // Eject from HotGas to EjectedMass
                galaxies[centralgal].HotGas -= ejected_mass;
                galaxies[centralgal].MetalsHotGas -= metallicityHot * ejected_mass;
                galaxies[centralgal].EjectedMass += ejected_mass;
                galaxies[centralgal].MetalsEjectedMass += metallicityHot * ejected_mass;
            }
        } else {
            // Original SAGE behavior: Cold --> HotGas --> Ejected
            
            // Add reheated gas to HotGas
            galaxies[centralgal].HotGas += reheated_mass;
            galaxies[centralgal].MetalsHotGas += metallicity * reheated_mass;

            // Check if ejection is possible from HotGas
            if(ejected_mass > galaxies[centralgal].HotGas) {
                ejected_mass = galaxies[centralgal].HotGas;
            }
            const double metallicityHot = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);

            // Eject from HotGas to EjectedMass
            galaxies[centralgal].HotGas -= ejected_mass;
            galaxies[centralgal].MetalsHotGas -= metallicityHot * ejected_mass;
            galaxies[centralgal].EjectedMass += ejected_mass;
            galaxies[centralgal].MetalsEjectedMass += metallicityHot * ejected_mass;
        }

        galaxies[p].OutflowRate += reheated_mass;
    }
}

void starformation_ffb(const int p, const int centralgal, const double dt, const int step,
                       struct GALAXY *galaxies, const struct params *run_params)
{
    // ========================================================================
    // FEEDBACK-FREE BURST (FFB) STAR FORMATION
    // Implementation of Li et al. 2024 - Equation (4) (modified to be Kauffmann-like)
    // ========================================================================
    
    double reff, tdyn, strdot, stars, metallicity;
    
    // Calculate dynamical time
    reff = 3.0 * galaxies[p].DiskScaleRadius;
    tdyn = (reff > 0.0 && galaxies[p].Vvir > 0.0) ? reff / galaxies[p].Vvir : 0.0;
    
    // Safety checks for NaN in inputs
    if(isnan(galaxies[p].ColdGas) || isinf(galaxies[p].ColdGas) ||
       isnan(galaxies[p].Vvir) || isinf(galaxies[p].Vvir) ||
       isnan(reff) || isinf(reff) || isnan(tdyn) || isinf(tdyn)) {
        stars = 0.0;
    } else if(tdyn > 0.0 && galaxies[p].ColdGas > 0.0) {
        // Equation (4): SFR = ε_FFB × M_gas / t_dyn
        // Use maximum FFB efficiency (typically 0.2, can be up to 1.0)
        const double epsilon_ffb = run_params->FFBMaxEfficiency;
        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        
        // Safety check on cold_crit
        if(isnan(cold_crit) || isinf(cold_crit) || cold_crit < 0.0) {
            stars = 0.0;
        } else if(galaxies[p].ColdGas > cold_crit) {
            // Only form stars if above critical density
            strdot = epsilon_ffb * (galaxies[p].ColdGas - cold_crit) / tdyn;
            
            // Safety check on strdot
            if(isnan(strdot) || isinf(strdot) || strdot < 0.0) {
                stars = 0.0;
            } else {
                stars = strdot * dt;
                
                // Can't form more stars than gas available
                if(stars > galaxies[p].ColdGas) {
                    stars = galaxies[p].ColdGas;
                }
                
                // Final safety check
                if(isnan(stars) || isinf(stars) || stars < 0.0) {
                    stars = 0.0;
                }
            }
        } else {
            // Below critical density - no star formation
            stars = 0.0;
        }
        
        // Debug output (only on first step to avoid spam)
        // if(step == 0) {
        //     const double z = run_params->ZZ[galaxies[p].SnapNum];
        //     printf("FFB SF: z=%.2f, Mvir=%.2e, eps=%.1f%%, M_gas=%.2e, t_dyn=%.3f Gyr, SFR=%.2e Msun/yr\n",
        //            z, galaxies[p].Mvir, epsilon_ffb*100, galaxies[p].ColdGas, 
        //            tdyn * run_params->UnitTime_in_Megayears / 1000.0, strdot);
        // }
    } else {
        stars = 0.0;
    }
    
    // Update star formation rate tracking
    galaxies[p].SfrDisk[step] += stars / dt;
    galaxies[p].SfrDiskColdGas[step] = galaxies[p].ColdGas;
    galaxies[p].SfrDiskColdGasMetals[step] = galaxies[p].MetalsColdGas;
    
    // Update for star formation (convert gas to stars)
    metallicity = get_metallicity(galaxies[p].ColdGas, galaxies[p].MetalsColdGas);
    update_from_star_formation(p, stars, metallicity, galaxies, run_params);
    
    // ========================================================================
    // Stars first form, then feedback acts on them
    // Key physics: star formation completes on free-fall time (~1 Myr)
    // before feedback from these stars can act (~2 Myr)
    // ========================================================================
    
    // Calculate reheated mass - use FIRE model if enabled, otherwise use original feedback
    double reheated_mass = 0.0;
    double ejected_mass = 0.0;
    
    if(run_params->SupernovaRecipeOn == 1) {
        if(run_params->FIREmodeOn == 1) {
            // FIRE: Calculate velocity/redshift scaling from Muratov et al. 2015
            const double z = run_params->ZZ[galaxies[p].SnapNum];
            const double vc = galaxies[p].Vvir;
            const double V_CRIT = 60.0;
            
            // Check for valid inputs to avoid NaN
            if(vc <= 0.0 || z < 0.0) {
                reheated_mass = 0.0;
            } else {
                double z_term = pow(1.0 + z, run_params->RedshiftPowerLawExponent);
                double v_term;
                if (vc < V_CRIT) {
                    v_term = pow(vc / V_CRIT, -3.2);
                } else {
                    v_term = pow(vc / V_CRIT, -1.0);
                }
                double scaling_factor = z_term * v_term;
                
                // Reheating with Muratov scaling: η = 2.9 × (1+z)^α × (V/60)^β
                double eta_reheat = run_params->FeedbackReheatingEpsilon * scaling_factor;
                // Store mass loading for analysis (cast to float)
                galaxies[p].MassLoading = (float)eta_reheat;
                reheated_mass = eta_reheat * stars;
            }
        } else {
            reheated_mass = run_params->FeedbackReheatingEpsilon * stars;
        }
    }

	XASSERT(reheated_mass >= 0.0, -1,
            "Error: Expected reheated gas-mass = %g to be >=0.0\n", reheated_mass);

    // cant use more cold gas than is available! so balance SF and feedback
    if((stars + reheated_mass) > galaxies[p].ColdGas && (stars + reheated_mass) > 0.0) {
        const double fac = galaxies[p].ColdGas / (stars + reheated_mass);
        stars *= fac;
        reheated_mass *= fac;
    }

    // determine ejection
    if(run_params->SupernovaRecipeOn == 1) {
        if(galaxies[centralgal].Vvir > 0.0) {
            if(run_params->FIREmodeOn == 1) {
                // FIRE model: Energy-based ejection following Hirschmann+2016
                // Energy from supernovae (with Muratov scaling)
                const double z = run_params->ZZ[galaxies[p].SnapNum];
                const double vc = galaxies[p].Vvir;
                const double V_CRIT = 60.0;
                
                // Check for valid inputs to avoid NaN
                if(vc <= 0.0 || z < 0.0) {
                    ejected_mass = 0.0;
                } else {
                    double z_term = pow(1.0 + z, run_params->RedshiftPowerLawExponent);
                    double v_term;
                    if (vc < V_CRIT) {
                        v_term = pow(vc / V_CRIT, -3.2);
                    } else {
                        v_term = pow(vc / V_CRIT, -1.0);
                    }
                    double scaling_factor = z_term * v_term;
                    
                    // Total feedback energy: E_FB = ε_eject × scaling × 0.5 × M_* × (η_SN × E_SN)
                    double E_FB = run_params->FeedbackEjectionEfficiency * scaling_factor * 
                                  0.5 * stars * (run_params->EtaSNcode * run_params->EnergySNcode);
                    
                    // Energy needed to lift reheated gas to virial radius: E_lift = 0.5 × M_reheat × V_vir²
                    double E_lift = 0.5 * reheated_mass * vc * vc;
                    
                    // Leftover energy ejects additional gas: E_eject = E_FB - E_lift
                    // Ejected mass: M_eject = E_eject / (0.5 × V_vir²)
                    if(E_FB > E_lift) {
                        ejected_mass = (E_FB - E_lift) / (0.5 * vc * vc);
                    } else {
                        ejected_mass = 0.0;
                    }
                }
            } else {
                // Original non-FIRE calculation
                ejected_mass = (run_params->FeedbackEjectionEfficiency * 
                               (run_params->EtaSNcode * run_params->EnergySNcode) / 
                               (galaxies[p].Vvir * galaxies[p].Vvir) -
                               run_params->FeedbackReheatingEpsilon) * stars;
            }
        } else {
            ejected_mass = 0.0;
        }
        
        if(ejected_mass < 0.0) {
            ejected_mass = 0.0;
        }
    } else {
        ejected_mass = 0.0;
    }

     // update from SN feedback
    update_from_feedback(p, centralgal, reheated_mass, ejected_mass, metallicity, galaxies, run_params);


    // H2 for merger-compatibility, but isn't used for stars
    if(run_params->SFprescription == 1 && galaxies[p].ColdGas > 0.0) {
        const float h = run_params->Hubble_h;
        const float rs_pc = galaxies[p].DiskScaleRadius * 1.0e6 / h;
        
        if(rs_pc > 0.0) {
            float disk_area_pc2 = M_PI * pow(3.0 * rs_pc, 2);
            float gas_surface_density = (galaxies[p].ColdGas * 1.0e10 / h) / disk_area_pc2;
            float stellar_surface_density = (galaxies[p].StellarMass * 1.0e10 / h) / disk_area_pc2;
            
            float f_mol = calculate_molecular_fraction_BR06(gas_surface_density, 
                                                            stellar_surface_density, rs_pc);
            galaxies[p].H2gas = f_mol * galaxies[p].ColdGas;
        } else {
            galaxies[p].H2gas = 0.0;
        }
    }
    
    // ========================================================================
    // METAL PRODUCTION (instantaneous recycling approximation - SNII only)
    // ========================================================================
    
    if(galaxies[p].ColdGas > 1.0e-8) {
        // Metals that stay in disk
        const double FracZleaveDiskVal = run_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);
        galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
        
        // Metals that leave disk - goes to appropriate reservoir based on CGM regime
        const double metals_leaving_disk = run_params->Yield * FracZleaveDiskVal * stars;
        
        if(run_params->CGMrecipeOn == 1) {
            if(galaxies[centralgal].Regime == 0) {
                // CGM-regime: metals go to CGM
                galaxies[centralgal].MetalsCGMgas += metals_leaving_disk;
            } else {
                // Hot-ICM-regime: metals go to HotGas
                galaxies[centralgal].MetalsHotGas += metals_leaving_disk;
            }
        } else {
            // Original SAGE behavior: metals go to HotGas
            galaxies[centralgal].MetalsHotGas += metals_leaving_disk;
        }
        
    } else {
        // All metals leave disk when ColdGas is very low
        const double all_metals = run_params->Yield * stars;
        
        if(run_params->CGMrecipeOn == 1) {
            if(galaxies[centralgal].Regime == 0) {
                galaxies[centralgal].MetalsCGMgas += all_metals;
            } else {
                galaxies[centralgal].MetalsHotGas += all_metals;
            }
        } else {
            galaxies[centralgal].MetalsHotGas += all_metals;
        }
    }
    
    // ========================================================================
    // NO DISK INSTABILITY CHECK
    // Rapid star formation stabilizes the disk
    // ========================================================================
}