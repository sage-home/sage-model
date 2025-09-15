#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_starformation_and_feedback.h"
#include "model_misc.h"
#include "model_disk_instability.h"

static long galaxy_debug_counter = 0;

void starformation_and_feedback(const int p, const int centralgal, const double time, const double dt, const int halonr, const int step,
                                struct GALAXY *galaxies, const struct params *run_params)
{
    double reff, tdyn, strdot, stars, ejected_mass, metallicity, reheated_mass, total_molecular_gas;

    // Initialise variables
    strdot = 0.0;
    reheated_mass = 0.0;
    ejected_mass = 0.0;
    total_molecular_gas = 0.0;

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
        // float disk_area_pc2 = M_PI * rs_pc * rs_pc; 
        float disk_area_pc2 = M_PI * pow(3.0 * rs_pc, 2); // 3× scale radius captures ~95% of mass 
        float gas_surface_density = (galaxies[p].ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
        float stellar_surface_density = (galaxies[p].StellarMass * 1.0e10 / h) / disk_area_pc2; // M☉/pc²

        total_molecular_gas = calculate_molecular_fraction_BR06(gas_surface_density, stellar_surface_density, 
                                                               rs_pc) * galaxies[p].ColdGas;
        float actual_f_mol = calculate_molecular_fraction_BR06(gas_surface_density, stellar_surface_density, rs_pc);

        galaxies[p].H2gas = total_molecular_gas;
        galaxy_debug_counter++;

         if (galaxy_debug_counter % 750000 == 0) {
            // Calculate additional quantities for debugging
            float pressure = calculate_midplane_pressure_BR06(gas_surface_density, stellar_surface_density, rs_pc);
            float h_star = calculate_stellar_scale_height_BR06(rs_pc);
            printf("DEBUG BR06: rs_pc=%.2e, h_star_pc=%.2e, pressure=%.2e K cm^-3, f_mol=%.4f\n",
                rs_pc, h_star, pressure, actual_f_mol);  // NOW PRINTS THE ACTUAL FRACTION
            printf("DEBUG BR06: gas_sigma=%.2e, star_sigma=%.2e M_sun/pc^2\n",
                gas_surface_density, stellar_surface_density);
            printf("DEBUG BR06: ColdGas=%.2e, StellarMass=%.2e M_sun\n",
                galaxies[p].ColdGas, galaxies[p].StellarMass);
            printf("DEBUG BR06: H2_gas=%.4e, HI_gas=%.4e\n", galaxies[p].H2gas, galaxies[p].ColdGas - galaxies[p].H2gas);
    }

        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > cold_crit) {
            strdot = 0.7 * run_params->SfrEfficiency * galaxies[p].H2gas / tdyn;
        } else {
            strdot = 0.0;
        }

    } else if(run_params->SFprescription == 2) {
        // DarkSAGE pressure-based H2 model
        reff = 3.0 * galaxies[p].DiskScaleRadius;
        tdyn = reff / galaxies[p].Vvir;
        
        const float h = run_params->Hubble_h;
        const float rs_pc = galaxies[p].DiskScaleRadius * 1.0e6 / h;
        // float disk_area_pc2 = M_PI * rs_pc * rs_pc;
        float disk_area_pc2 = M_PI * pow(3.0 * rs_pc, 2);
        float gas_surface_density = (galaxies[p].ColdGas * 1.0e10 / h) / disk_area_pc2; // M☉/pc²
        float stellar_surface_density = (galaxies[p].StellarMass * 1.0e10 / h) / disk_area_pc2; // M☉/pc²

        // Estimate velocity dispersions (you might want to track these in your galaxy structure)
        float gas_velocity_dispersion = 8.0;  // km/s, typical for cold ISM
        float stellar_velocity_dispersion = fmax(30.0, 0.5 * galaxies[p].Vvir);  // km/s, scale with Vvir

        // Assume aligned discs for simplicity (you could track disc misalignment)
        float disk_alignment_angle = 0.0;  // degrees

        float actual_f_mol = calculate_molecular_fraction_darksage_pressure(gas_surface_density, 
                                                                            stellar_surface_density,
                                                                            gas_velocity_dispersion,
                                                                            stellar_velocity_dispersion,
                                                                            disk_alignment_angle);
        
        total_molecular_gas = actual_f_mol * galaxies[p].ColdGas;
        galaxies[p].H2gas = total_molecular_gas;
        galaxy_debug_counter++;

        if (galaxy_debug_counter % 750000 == 0) {
            printf("DEBUG DarkSAGE: rs_pc=%.2e, gas_vdisp=%.1f, star_vdisp=%.1f, f_mol=%.4f\n",
                rs_pc, gas_velocity_dispersion, stellar_velocity_dispersion, actual_f_mol);
            printf("DEBUG DarkSAGE: gas_sigma=%.2e, star_sigma=%.2e M_sun/pc^2\n",
                gas_surface_density, stellar_surface_density);
            printf("DEBUG DarkSAGE: ColdGas=%.2e, StellarMass=%.2e M_sun\n",
                galaxies[p].ColdGas, galaxies[p].StellarMass);
            printf("DEBUG DarkSAGE: H2_gas=%.4e, HI_gas=%.4e\n", galaxies[p].H2gas, galaxies[p].ColdGas - galaxies[p].H2gas);
        }

        const double cold_crit = 0.19 * galaxies[p].Vvir * reff;
        if(galaxies[p].ColdGas > cold_crit) {
            strdot = 4 * run_params->SfrEfficiency * galaxies[p].H2gas / tdyn;
        } else {
            strdot = 0.0;
        }

    } else {
        fprintf(stderr, "No star formation prescription selected!\n");
        ABORT(0);
    }

    stars = strdot * dt;
    if(stars < 0.0) {
        stars = 0.0;
    }

    double z = run_params->ZZ[galaxies[p].SnapNum];
    double vmax = galaxies[p].Vmax;

    // determine reheated mass
    if(run_params->SupernovaRecipeOn == 1) {
        if(run_params->FIREMassLoading == 1) {
            // Use Muratov mass loading calculation
            reheated_mass = calculate_muratov_mass_loading(p, galaxies, z) * stars;
            } else {
                // Use traditional feedback parameter
                reheated_mass = run_params->FeedbackReheatingEpsilon * stars;
            }
        } 
    // double reheated_mass = (run_params->SupernovaRecipeOn == 1) ? run_params->FeedbackReheatingEpsilon * stars: 0.0;

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
            if (run_params->FIREejection == 1) {
                // Implement FIRE ejection calculation
                double alpha = (vmax < 60.0) ? -3.2 : -1.0;
                double fire_scaling = pow(1.0 + z, 1.3) * pow(vmax / 60.0, alpha);
                
                double E_FB = run_params->FeedbackEjectionEfficiency * fire_scaling * 
                            0.5 * stars * run_params->EtaSNcode * run_params->EnergySNcode;
                
                double energy_used_reheating = 0.5 * reheated_mass * galaxies[centralgal].Vvir * galaxies[centralgal].Vvir;
                double available_energy = E_FB - energy_used_reheating;
                
                if(available_energy > 0.0) {
                    ejected_mass = available_energy / (0.5 * galaxies[centralgal].Vvir * galaxies[centralgal].Vvir);
                } else {
                    ejected_mass = 0.0;
                }
            } else {
                if (run_params->FIREMassLoading == 1) {
                    // Use Muratov mass loading calculation
                    double alpha = (vmax < 60.0) ? -3.2 : -1.0;
                    double fire_scaling = pow(1.0 + z, 1.3) * pow(vmax / 60.0, alpha);
                    ejected_mass = ((run_params->FeedbackEjectionEfficiency * (run_params->EtaSNcode * run_params->EnergySNcode) / (galaxies[centralgal].Vvir * galaxies[centralgal].Vvir) -
                    calculate_muratov_mass_loading(centralgal, galaxies, z)) * stars) * fire_scaling;
                } else {
                    // Use traditional feedback parameter
                    ejected_mass =
                    (run_params->FeedbackEjectionEfficiency * (run_params->EtaSNcode * run_params->EnergySNcode) / (galaxies[centralgal].Vvir * galaxies[centralgal].Vvir) -
                     run_params->FeedbackReheatingEpsilon) * stars;
                }
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

    if (run_params->CGMrecipeOn > 0) {
        if (galaxies[p].Regime == 0) {
            if(galaxies[p].ColdGas > 1.0e-8) {
            const double FracZleaveDiskVal = run_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
            galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
            galaxies[centralgal].MetalsCGMgas += run_params->Yield * FracZleaveDiskVal * stars;
        } else {
            galaxies[centralgal].MetalsCGMgas += run_params->Yield * stars;
            }
        } else {
            if(galaxies[p].ColdGas > 1.0e-8) {
            const double FracZleaveDiskVal = run_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
            galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
            galaxies[centralgal].MetalsHotGas += run_params->Yield * FracZleaveDiskVal * stars;
        } else {
            galaxies[centralgal].MetalsHotGas += run_params->Yield * stars;
            }
        }
    } else {
        if(galaxies[p].ColdGas > 1.0e-8) {
            const double FracZleaveDiskVal = run_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
            galaxies[p].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
            galaxies[centralgal].MetalsHotGas += run_params->Yield * FracZleaveDiskVal * stars;
            // galaxies[centralgal].MetalsCGMgas += run_params->Yield * FracZleaveDiskVal * stars;
        } else {
            galaxies[centralgal].MetalsHotGas += run_params->Yield * stars;
            // galaxies[centralgal].MetalsCGMgas += run_params->Yield * stars;
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

    XASSERT(reheated_mass >= 0.0, -1,
            "Error: For galaxy = %d (halonr = %d, centralgal = %d) with MostBoundID = %lld, the reheated mass = %g should be >=0.0",
            p, galaxies[p].HaloNr, centralgal, galaxies[p].MostBoundID, reheated_mass);
    XASSERT(reheated_mass <= galaxies[p].ColdGas, -1,
            "Error: Reheated mass = %g should be <= the coldgas mass of the galaxy = %g",
            reheated_mass, galaxies[p].ColdGas);

    if(run_params->SupernovaRecipeOn == 1) {
        // Remove gas from cold reservoir (always from galaxy p)
        galaxies[p].ColdGas -= reheated_mass;
        galaxies[p].MetalsColdGas -= metallicity * reheated_mass;

        if(run_params->CGMrecipeOn == 1) {
            // FIXED: Use CENTRAL galaxy's regime for routing (reheated gas goes to central)
            // But use STAR-FORMING galaxy's regime for ejection constraints
        
            if(galaxies[centralgal].Regime == 0) {
                // CENTRAL is CGM REGIME: Reheated gas goes to central's CGM
                galaxies[centralgal].CGMgas += reheated_mass;
                galaxies[centralgal].MetalsCGMgas += metallicity * reheated_mass;
                
                // Ejection from central's CGM reservoir
                if(ejected_mass > galaxies[centralgal].CGMgas) {
                    ejected_mass = galaxies[centralgal].CGMgas;
                    }
                if(ejected_mass > 0.0) {
                    const double metallicityCGM = get_metallicity(galaxies[centralgal].CGMgas, galaxies[centralgal].MetalsCGMgas);
                    galaxies[centralgal].CGMgas -= ejected_mass;
                    galaxies[centralgal].MetalsCGMgas -= metallicityCGM * ejected_mass;
                    }
                
            } else {
                // CENTRAL is HOT REGIME: Reheated gas goes to central's HotGas  
                galaxies[centralgal].HotGas += reheated_mass;
                galaxies[centralgal].MetalsHotGas += metallicity * reheated_mass;

                // Ejection from central's HotGas reservoir
                if(ejected_mass > galaxies[centralgal].HotGas) {
                    ejected_mass = galaxies[centralgal].HotGas;
                    }
                if(ejected_mass > 0.0) {
                    const double metallicityHot = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
                    galaxies[centralgal].HotGas -= ejected_mass;
                    galaxies[centralgal].MetalsHotGas -= metallicityHot * ejected_mass;
                }
            }
        } else {
            // Original behavior: reheated gas goes to HotGas
            galaxies[centralgal].HotGas += reheated_mass;
            galaxies[centralgal].MetalsHotGas += metallicity * reheated_mass;

            // Ejection logic: remove from HotGas, add to CGM (original SAGE behavior)
            if(ejected_mass > galaxies[centralgal].HotGas) {
                ejected_mass = galaxies[centralgal].HotGas;
                }
            if(ejected_mass > 0.0) {
                const double metallicityHot = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
                galaxies[centralgal].HotGas -= ejected_mass;
                galaxies[centralgal].MetalsHotGas -= metallicityHot * ejected_mass;
                galaxies[centralgal].CGMgas += ejected_mass;
                galaxies[centralgal].MetalsCGMgas += metallicityHot * ejected_mass;
            }
        }

        // Track outflow rate for star-forming galaxy
        galaxies[p].OutflowRate += reheated_mass;
    }
}