#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_mergers.h"
#include "model_misc.h"
#include "model_starformation_and_feedback.h"
#include "model_disk_instability.h"

double estimate_merging_time(const int sat_halo, const int mother_halo, const int ngal, struct halo_data *halos, struct GALAXY *galaxies, const struct params *run_params)
{
    double mergtime;
    const int MinNumPartSatHalo = 10;

    if(sat_halo == mother_halo) {
        fprintf(stderr, "Error: \t\tSnapNum, Type, IDs, sat radius:\t%i\t%i\t%i\t%i\t--- sat/cent have the same ID\n",
               galaxies[ngal].SnapNum, galaxies[ngal].Type, sat_halo, mother_halo);
        return -1.0;
    }

    const double coulomb = log1p(halos[mother_halo].Len / ((double) halos[sat_halo].Len) );//MS: 12/9/2019. As pointed out by codacy -> log1p(x) is better than log(1 + x)

    const double SatelliteMass = get_virial_mass(sat_halo, halos, run_params) + galaxies[ngal].StellarMass + galaxies[ngal].ColdGas;
    const double SatelliteRadius = get_virial_radius(mother_halo, halos, run_params);

    if(SatelliteMass > 0.0 && coulomb > 0.0 && halos[sat_halo].Len >= MinNumPartSatHalo) {
        mergtime = 2.0 *
            1.17 * SatelliteRadius * SatelliteRadius * get_virial_velocity(mother_halo, halos, run_params) / (coulomb * run_params->G * SatelliteMass);
    } else {
        mergtime = -1.0;
    }

    if (mergtime >= 999.0)
    {
        mergtime = 998.0;
        // implementing time ceiling since some objects have merge times longer than universe age when using
        // TNG50 merger trees because of lower simulation particle mass 
    }

    return mergtime;

}

double calculate_merger_remnant_radius(const struct GALAXY *g1, const struct GALAXY *g2, const double mass_ratio)
{
    // 1. Calculate Total Baryonic Mass (Stars + Gas) for both progenitors
    double M1 = g1->StellarMass + g1->ColdGas;
    double M2 = g2->StellarMass + g2->ColdGas;
    double M_tot = M1 + M2;

    if (M_tot <= 0.0) return 0.0;

    // 2. Calculate Half-Mass Radius for both progenitors
    // For Discs: R_half ~ 1.68 * R_scale (Exponential profile)
    // For Bulges: We assume the stored radius is the half-mass radius
    
    // Progenitor 1 (Central)
    double R1_disk_half = 1.68 * g1->DiskScaleRadius;
    double R1_bulge_half = g1->BulgeScaleRadius;
    double R1;

    if (g1->StellarMass + g1->ColdGas > 0) {
        // Mass-weighted average radius of the whole galaxy
        // Note: For pure discs, BulgeMass is 0, so this works naturally
        double M1_disk = g1->ColdGas + (g1->StellarMass - g1->BulgeMass);
        double M1_bulge = g1->BulgeMass;
        R1 = (M1_disk * R1_disk_half + M1_bulge * R1_bulge_half) / M1;
    } else {
        R1 = 0.0;
    }

    // Progenitor 2 (Satellite)
    double R2_disk_half = 1.68 * g2->DiskScaleRadius;
    double R2_bulge_half = g2->BulgeScaleRadius;
    double R2;

    if (g2->StellarMass + g2->ColdGas > 0) {
        double M2_disk = g2->ColdGas + (g2->StellarMass - g2->BulgeMass);
        double M2_bulge = g2->BulgeMass;
        R2 = (M2_disk * R2_disk_half + M2_bulge * R2_bulge_half) / M2;
    } else {
        R2 = 0.0;
    }

    // Safeguard against zero radius (e.g., pure gas cloud with no set radius yet)
    if (R1 <= 0.0) R1 = R2; 
    if (R2 <= 0.0) R2 = R1;
    if (R1 <= 0.0) return 0.0; // Both zero

    // 3. Calculate Energy Terms (ignoring G, as it cancels out)
    // We use "Potential" units: P = M^2 / R
    
    // E_initial (Eq 21): Self-binding energy of progenitors
    double E_init = (M1 * M1) / R1 + (M2 * M2) / R2;

    // E_orbital (Eq 22): Interaction energy at merger
    // Approximated as circular orbit energy at separation R1 + R2
    double E_orb = (M1 * M2) / (R1 + R2);

    // E_rad (Eq 23): Radiative losses due to gas
    // C_rad = 2.75 (from Covington et al. 2011, cited in Tonini 2016)
    double C_rad = 2.75;
    double f_gas = (g1->ColdGas + g2->ColdGas) / M_tot;
    double E_rad = C_rad * E_init * f_gas;

    // 4. Total Final Energy (Eq 20)
    // E_final = E_init + E_orb + E_rad
    double E_final = E_init + E_orb + E_rad;

    // 5. Final Radius (Eq 17 rearranged)
    // R_final = M_tot^2 / E_final
    double R_final = (M_tot * M_tot) / E_final;

    return R_final;
}

void deal_with_galaxy_merger(const int p, const int merger_centralgal, const int centralgal,
                             const double time, const double dt, const int halonr, const int step,
                             struct GALAXY *galaxies, const struct params *run_params)
{
    double mi, ma, mass_ratio;

    // calculate mass ratio of merging galaxies
    if(galaxies[p].StellarMass + galaxies[p].ColdGas <
       galaxies[merger_centralgal].StellarMass + galaxies[merger_centralgal].ColdGas) {
        mi = galaxies[p].StellarMass + galaxies[p].ColdGas;
        ma = galaxies[merger_centralgal].StellarMass + galaxies[merger_centralgal].ColdGas;
    } else {
        mi = galaxies[merger_centralgal].StellarMass + galaxies[merger_centralgal].ColdGas;
        ma = galaxies[p].StellarMass + galaxies[p].ColdGas;
    }

    if(ma > 0) {
        mass_ratio = mi / ma;
    } else {
        mass_ratio = 1.0;
    }

    // 1. Calculate the New Merger Radius via Energy Conservation
    // We calculate this regardless of merger type, but apply it selectively
    double new_merger_radius = calculate_merger_remnant_radius(&galaxies[merger_centralgal], &galaxies[p], mass_ratio);
    
    // Determine Central Morphology (Tonini 2016 Section 5.2)
    // Is the central galaxy Disc-dominated or Bulge-dominated?
    double central_disk_mass = galaxies[merger_centralgal].StellarMass - galaxies[merger_centralgal].BulgeMass;
    int is_disk_dominated = (central_disk_mass > 0.5 * galaxies[merger_centralgal].StellarMass);

    add_galaxies_together(merger_centralgal, p, galaxies, run_params);

    // grow black hole through accretion from cold disk during mergers, a la Kauffmann & Haehnelt (2000)
    if(run_params->AGNrecipeOn) {
        grow_black_hole(merger_centralgal, mass_ratio, galaxies, run_params);
    }

    // starburst recipe similar to Somerville et al. 2001
    collisional_starburst_recipe(mass_ratio, merger_centralgal, centralgal, time, dt, halonr, 0, step, galaxies, run_params);

    if(mass_ratio > 0.1) {
		galaxies[merger_centralgal].TimeOfLastMinorMerger = time;
    }

    if(mass_ratio > run_params->ThreshMajorMerger) {
        // CASE 1: MAJOR MERGER (Section 5.2.3)
        // Destroys disc, creates pure merger-driven bulge
        make_bulge_from_burst(merger_centralgal, galaxies, run_params);
        
        // Apply the Energy Conservation Radius
        galaxies[merger_centralgal].MergerBulgeRadius = new_merger_radius;
        galaxies[merger_centralgal].BulgeScaleRadius = new_merger_radius; // It is now a pure spheroid
        
        galaxies[merger_centralgal].TimeOfLastMajorMerger = time;
        galaxies[p].mergeType = 2; 

    } else {
        // CASE 2: MINOR MERGER
        galaxies[p].mergeType = 1;

        if (is_disk_dominated) {
            // Minor merger on DISC (Section 5.2.1)
            // Satellite absorbed by disc, triggers instability.
            // Radius logic handled inside add_galaxies_together -> update_instability_bulge_radius
            // DO NOT update MergerBulgeRadius here.
        } else {
            // Minor merger on SPHEROID (Section 5.2.3)
            // Satellite adds directly to the Merger Bulge.
            // We use the energy conservation radius calculated above.
            galaxies[merger_centralgal].MergerBulgeRadius = new_merger_radius;
        }
    }

    // if(mass_ratio > run_params->ThreshMajorMerger) {
    //     make_bulge_from_burst(merger_centralgal, galaxies, run_params);
    //     galaxies[merger_centralgal].TimeOfLastMajorMerger = time;
    //     galaxies[p].mergeType = 2;  // mark as major merger
    // } else {
    //     galaxies[p].mergeType = 1;  // mark as minor merger
    // }

}



void grow_black_hole(const int merger_centralgal, const double mass_ratio, struct GALAXY *galaxies, const struct params *run_params)
{
    double BHaccrete, metallicity;

    if(galaxies[merger_centralgal].ColdGas > 0.0) {
        BHaccrete = run_params->BlackHoleGrowthRate * mass_ratio /
            (1.0 + SQR(280.0 / galaxies[merger_centralgal].Vvir)) * galaxies[merger_centralgal].ColdGas;

        // cannot accrete more gas than is available!
        if(BHaccrete > galaxies[merger_centralgal].ColdGas) {
            BHaccrete = galaxies[merger_centralgal].ColdGas;
        }

        metallicity = get_metallicity(galaxies[merger_centralgal].ColdGas, galaxies[merger_centralgal].MetalsColdGas);
        galaxies[merger_centralgal].BlackHoleMass += BHaccrete;
        galaxies[merger_centralgal].ColdGas -= BHaccrete;
        galaxies[merger_centralgal].MetalsColdGas -= metallicity * BHaccrete;

        galaxies[merger_centralgal].QuasarModeBHaccretionMass += BHaccrete;

        quasar_mode_wind(merger_centralgal, BHaccrete, galaxies, run_params);
    }
}



void quasar_mode_wind(const int gal, const double BHaccrete, struct GALAXY *galaxies, const struct params *run_params)
{
    // work out total energy in quasar wind (eta*m*c^2)
    const double quasar_energy = run_params->QuasarModeEfficiency * 0.1 * BHaccrete * (C / run_params->UnitVelocity_in_cm_per_s) * (C / run_params->UnitVelocity_in_cm_per_s);
    const double cold_gas_energy = 0.5 * galaxies[gal].ColdGas * galaxies[gal].Vvir * galaxies[gal].Vvir;

    // compare quasar wind and cold gas energies and eject cold
    if(quasar_energy > cold_gas_energy) {
        galaxies[gal].EjectedMass += galaxies[gal].ColdGas;
        galaxies[gal].MetalsEjectedMass += galaxies[gal].MetalsColdGas;

        galaxies[gal].ColdGas = 0.0;
        galaxies[gal].MetalsColdGas = 0.0;
    }

    // compare quasar wind and cold+hot/CGM gas energies and eject from appropriate reservoir
    if(run_params->CGMrecipeOn == 1) {
        if(galaxies[gal].Regime == 0) {
            // CGM-regime: check and eject from CGM
            const double cgm_gas_energy = 0.5 * galaxies[gal].CGMgas * galaxies[gal].Vvir * galaxies[gal].Vvir;
            
            if(quasar_energy > cold_gas_energy + cgm_gas_energy) {
                galaxies[gal].EjectedMass += galaxies[gal].CGMgas;
                galaxies[gal].MetalsEjectedMass += galaxies[gal].MetalsCGMgas;

                galaxies[gal].CGMgas = 0.0;
                galaxies[gal].MetalsCGMgas = 0.0;
            }
        } else {
            // Hot-ICM-regime: check and eject from HotGas
            const double hot_gas_energy = 0.5 * galaxies[gal].HotGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
            
            if(quasar_energy > cold_gas_energy + hot_gas_energy) {
                galaxies[gal].EjectedMass += galaxies[gal].HotGas;
                galaxies[gal].MetalsEjectedMass += galaxies[gal].MetalsHotGas;

                galaxies[gal].HotGas = 0.0;
                galaxies[gal].MetalsHotGas = 0.0;
            }
        }
    } else {
        // Original SAGE behavior: check and eject from HotGas
        const double hot_gas_energy = 0.5 * galaxies[gal].HotGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
        
        if(quasar_energy > cold_gas_energy + hot_gas_energy) {
            galaxies[gal].EjectedMass += galaxies[gal].HotGas;
            galaxies[gal].MetalsEjectedMass += galaxies[gal].MetalsHotGas;

            galaxies[gal].HotGas = 0.0;
            galaxies[gal].MetalsHotGas = 0.0;
        }
    }
}



void add_galaxies_together(const int t, const int p, struct GALAXY *galaxies, const struct params *run_params)
{
    galaxies[t].ColdGas += galaxies[p].ColdGas;
    galaxies[t].MetalsColdGas += galaxies[p].MetalsColdGas;

    galaxies[t].StellarMass += galaxies[p].StellarMass;
    galaxies[t].MetalsStellarMass += galaxies[p].MetalsStellarMass;

    galaxies[t].HotGas += galaxies[p].HotGas;
    galaxies[t].MetalsHotGas += galaxies[p].MetalsHotGas;

    galaxies[t].EjectedMass += galaxies[p].EjectedMass;
    galaxies[t].MetalsEjectedMass += galaxies[p].MetalsEjectedMass;

    galaxies[t].ICS += galaxies[p].ICS;
    galaxies[t].MetalsICS += galaxies[p].MetalsICS;

    galaxies[t].BlackHoleMass += galaxies[p].BlackHoleMass;

    galaxies[t].CGMgas += galaxies[p].CGMgas;
    galaxies[t].MetalsCGMgas += galaxies[p].MetalsCGMgas;

    // add merger to bulge
    galaxies[t].BulgeMass += galaxies[p].StellarMass;
    galaxies[t].MetalsBulgeMass += galaxies[p].MetalsStellarMass;

    // Track origin based on morphology (Tonini+2016 logic)
    const double disk_mass = galaxies[t].StellarMass - galaxies[t].BulgeMass;
    const double disk_fraction = (galaxies[t].StellarMass > 0.0) ? 
                                 disk_mass / galaxies[t].StellarMass : 0.0;
    
    if(disk_fraction > 0.5) {
        // Disc-dominated: minor merger triggers instability
        const double added_mass = galaxies[p].StellarMass;
        galaxies[t].InstabilityBulgeMass += added_mass;
        
        // UPDATE: Tonini incremental radius evolution (equation 16)
        update_instability_bulge_radius(t, added_mass, galaxies, run_params);
    } else {
        // Spheroid-dominated: grows merger bulge
        galaxies[t].MergerBulgeMass += galaxies[p].StellarMass;
    }

    for(int step = 0; step < STEPS; step++) {
        galaxies[t].SfrBulge[step] += galaxies[p].SfrDisk[step] + galaxies[p].SfrBulge[step];
        galaxies[t].SfrBulgeColdGas[step] += galaxies[p].SfrDiskColdGas[step] + galaxies[p].SfrBulgeColdGas[step];
        galaxies[t].SfrBulgeColdGasMetals[step] += galaxies[p].SfrDiskColdGasMetals[step] + galaxies[p].SfrBulgeColdGasMetals[step];
    }
}



void make_bulge_from_burst(const int p, struct GALAXY *galaxies, const struct params *run_params)
{
    // generate bulge
    galaxies[p].BulgeMass = galaxies[p].StellarMass;
    galaxies[p].MergerBulgeMass = galaxies[p].StellarMass;      // All merger-driven
    galaxies[p].InstabilityBulgeMass = 0.0;                      // Destroyed
    galaxies[p].MetalsBulgeMass = galaxies[p].MetalsStellarMass;

    // galaxies[p].BulgeScaleRadius = get_bulge_radius(p, galaxies, run_params);

    // update the star formation rate
    for(int step = 0; step < STEPS; step++) {
        galaxies[p].SfrBulge[step] += galaxies[p].SfrDisk[step];
        galaxies[p].SfrBulgeColdGas[step] += galaxies[p].SfrDiskColdGas[step];
        galaxies[p].SfrBulgeColdGasMetals[step] += galaxies[p].SfrDiskColdGasMetals[step];
        galaxies[p].SfrDisk[step] = 0.0;
        galaxies[p].SfrDiskColdGas[step] = 0.0;
        galaxies[p].SfrDiskColdGasMetals[step] = 0.0;
    }
}



void collisional_starburst_recipe(const double mass_ratio, const int merger_centralgal, const int centralgal,
                                  const double time, const double dt, const int halonr, const int mode, const int step,
                                  struct GALAXY *galaxies, const struct params *run_params)
{
    double stars, reheated_mass, ejected_mass, fac, metallicity, eburst;

    // This is the major and minor merger starburst recipe of Somerville et al. 2001.
    // The coefficients in eburst are taken from TJ Cox's PhD thesis and should be more accurate then previous.

    // the bursting fraction
    if(mode == 1) {
        eburst = mass_ratio;
    } else {
        eburst = 0.56 * pow(mass_ratio, 0.7);
    }

    double gas_for_starburst;
    if(run_params->SFprescription == 1) {
        // For H2-based prescriptions (BR06, DarkSAGE, GD14), use molecular gas
        gas_for_starburst = galaxies[merger_centralgal].H2gas;
    } else {
        // For traditional prescription, use total cold gas
        gas_for_starburst = galaxies[merger_centralgal].ColdGas;
    }

    stars = eburst * gas_for_starburst;
    if(stars < 0.0) {
        stars = 0.0;
    }

    // this bursting results in SN feedback on the cold/hot gas - use FIRE model if enabled
    if(run_params->SupernovaRecipeOn == 1) {
        if(run_params->FIREmodeOn == 1) {
            // FIRE: Calculate velocity/redshift scaling from Muratov et al. 2015
            const double z = run_params->ZZ[galaxies[merger_centralgal].SnapNum];
            const double vc = galaxies[merger_centralgal].Vvir;
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
                reheated_mass = eta_reheat * stars;
            }
        } else {
            reheated_mass = run_params->FeedbackReheatingEpsilon * stars;
        }
    } else {
        reheated_mass = 0.0;
    }

	XASSERT(reheated_mass >= 0.0, -1,
            "Error: Reheated mass = %g should be >= 0.0",
            reheated_mass);

    // can't use more cold gas than is available! so balance SF and feedback
    if((stars + reheated_mass) > galaxies[merger_centralgal].ColdGas) {
        fac = galaxies[merger_centralgal].ColdGas / (stars + reheated_mass);
        stars *= fac;
        reheated_mass *= fac;
    }

    // determine ejection
    if(run_params->SupernovaRecipeOn == 1) {
        if(galaxies[merger_centralgal].Vvir > 0.0) {
            if(run_params->FIREmodeOn == 1) {
                // FIRE model: Energy-based ejection following Hirschmann+2016
                const double z = run_params->ZZ[galaxies[merger_centralgal].SnapNum];
                const double vc = galaxies[merger_centralgal].Vvir;
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
                ejected_mass =
                    (run_params->FeedbackEjectionEfficiency * (run_params->EtaSNcode * run_params->EnergySNcode) / (galaxies[merger_centralgal].Vvir * galaxies[merger_centralgal].Vvir) -
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

    // starbursts add to the bulge
    galaxies[merger_centralgal].SfrBulge[step] += stars / dt;
    galaxies[merger_centralgal].SfrBulgeColdGas[step] += galaxies[merger_centralgal].ColdGas;
    galaxies[merger_centralgal].SfrBulgeColdGasMetals[step] += galaxies[merger_centralgal].MetalsColdGas;

    metallicity = get_metallicity(galaxies[merger_centralgal].ColdGas, galaxies[merger_centralgal].MetalsColdGas);
    update_from_star_formation(merger_centralgal, stars, metallicity, galaxies, run_params);

    galaxies[merger_centralgal].BulgeMass += (1 - run_params->RecycleFraction) * stars;
    galaxies[merger_centralgal].MetalsBulgeMass += metallicity * (1 - run_params->RecycleFraction) * stars;

    // recompute the metallicity of the cold phase
    metallicity = get_metallicity(galaxies[merger_centralgal].ColdGas, galaxies[merger_centralgal].MetalsColdGas);

    // update from feedback
    update_from_feedback(merger_centralgal, centralgal, reheated_mass, ejected_mass, metallicity, galaxies, run_params);

    // check for disk instability
    if(run_params->DiskInstabilityOn && mode == 0) {
        if(mass_ratio < run_params->ThreshMajorMerger) {
            check_disk_instability(merger_centralgal, centralgal, halonr, time, dt, step, galaxies, (struct params *) run_params);
        }
    }

    // formation of new metals - instantaneous recycling approximation - only SNII
    if(galaxies[merger_centralgal].ColdGas > 1e-8 && mass_ratio < run_params->ThreshMajorMerger) {
        const double FracZleaveDiskVal = run_params->FracZleaveDisk * exp(-1.0 * galaxies[centralgal].Mvir / 30.0);  // Krumholz & Dekel 2011 Eq. 22
        
        // Metals that stay in disk (same for all regimes)
        galaxies[merger_centralgal].MetalsColdGas += run_params->Yield * (1.0 - FracZleaveDiskVal) * stars;
        
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
        // Major merger or low cold gas: all metals leave disk - regime dependent
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



void disrupt_satellite_to_ICS(const int centralgal, const int gal, struct GALAXY *galaxies, const struct params *run_params)
{
    // Transfer satellite's gas to central's hot/CGM reservoir (regime-dependent)
    const double total_gas = galaxies[gal].ColdGas + galaxies[gal].HotGas + galaxies[gal].CGMgas;
    const double total_metals_gas = galaxies[gal].MetalsColdGas + galaxies[gal].MetalsHotGas + galaxies[gal].MetalsCGMgas;
    
    if(run_params->CGMrecipeOn == 1) {
        if(galaxies[centralgal].Regime == 0) {
            // CGM-regime: disrupted gas goes to CGM
            galaxies[centralgal].CGMgas += total_gas;
            galaxies[centralgal].MetalsCGMgas += total_metals_gas;
        } else {
            // Hot-ICM-regime: disrupted gas goes to HotGas
            galaxies[centralgal].HotGas += total_gas;
            galaxies[centralgal].MetalsHotGas += total_metals_gas;
        }
    } else {
        // Original SAGE behavior: disrupted gas goes to HotGas
        galaxies[centralgal].HotGas += total_gas;
        galaxies[centralgal].MetalsHotGas += total_metals_gas;
    }

    // Transfer ejected mass (same for all regimes)
    galaxies[centralgal].EjectedMass += galaxies[gal].EjectedMass;
    galaxies[centralgal].MetalsEjectedMass += galaxies[gal].MetalsEjectedMass;

    // Transfer ICS (same for all regimes)
    galaxies[centralgal].ICS += galaxies[gal].ICS;
    galaxies[centralgal].MetalsICS += galaxies[gal].MetalsICS;

    // Disrupt stellar mass to ICS (same for all regimes)
    galaxies[centralgal].ICS += galaxies[gal].StellarMass;
    galaxies[centralgal].MetalsICS += galaxies[gal].MetalsStellarMass;

    // what should we do with the disrupted satellite BH?
    galaxies[gal].mergeType = 4;  // mark as disruption to the ICS
}
