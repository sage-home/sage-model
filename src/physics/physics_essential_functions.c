/**
 * @file physics_essential_functions.c
 * @brief Essential physics functions required for core-physics separation
 *
 * This file provides minimal implementations of physics functions that are required
 * by the core SAGE infrastructure but should remain physics-agnostic. These functions
 * enable the core to operate in "physics-free mode" with empty pipelines while
 * maintaining the essential function signatures that core components depend on.
 *
 * The functions in this file:
 * - Initialize galaxies with core properties only (physics handled by property system)
 * - Provide basic virial calculations using standard formulae
 * - Return sensible defaults for merger timing (no complex physics)
 * - Implement no-op merger handling for physics-free mode
 *
 * This maintains core-physics separation by ensuring the core never directly
 * calls complex physics implementations, while still satisfying compilation
 * and basic runtime requirements.
 *
 * @note These are NOT full physics implementations - they are minimal stubs
 *       that enable core infrastructure to function independently.
 */

#include <math.h>
#include "core_allvars.h"
#include "core_properties.h"
#include "core_logging.h"
#include "physics_essential_functions.h"

void init_galaxy(int p, int halonr, int *galaxycounter, const struct halo_data *halos, 
                 struct GALAXY *galaxies, const struct params *run_params) {
    // Initialize core properties only
    galaxies[p].SnapNum = -1;
    galaxies[p].Type = -1;
    galaxies[p].GalaxyNr = *galaxycounter;
    (*galaxycounter)++;
    galaxies[p].GalaxyIndex = (uint64_t)p;
    galaxies[p].CentralGalaxyIndex = (uint64_t)p;
    galaxies[p].MostBoundID = 0;
    
    // Initialize from halo data
    for (int j = 0; j < 3; j++) {
        galaxies[p].Pos[j] = halos[halonr].Pos[j];
        galaxies[p].Vel[j] = halos[halonr].Vel[j];
    }
    
    galaxies[p].Len = halos[halonr].Len;
    galaxies[p].Mvir = halos[halonr].Mvir;
    galaxies[p].deltaMvir = 0.0;
    galaxies[p].Rvir = get_virial_radius(halonr, halos, run_params);
    galaxies[p].Vvir = get_virial_velocity(halonr, halos, run_params);
    galaxies[p].Vmax = halos[halonr].Vmax;
    
    if (allocate_galaxy_properties(&galaxies[p], run_params) != 0) {
        LOG_ERROR("Failed to allocate galaxy properties for galaxy %d", p);
    }
}

double get_virial_mass(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    (void)run_params;
    return halos[halonr].Mvir;
}

double get_virial_radius(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    const double Mvir = halos[halonr].Mvir;
    const double Hubble_h = run_params->cosmology.Hubble_h;
    const double a = 1.0; // Use a=1 since Redshift field unavailable
    
    const double rho_crit = 2.775e11 * Hubble_h * Hubble_h;
    const double Delta_c = 200.0;
    const double rho_vir = Delta_c * rho_crit;
    
    return pow(3.0 * Mvir / (4.0 * M_PI * rho_vir), 1.0/3.0) * a;
}

double get_virial_velocity(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    const double Mvir = halos[halonr].Mvir;
    const double Rvir = get_virial_radius(halonr, halos, run_params);
    const double G = 43.0071;
    
    return (Rvir > 0.0) ? sqrt(G * Mvir / Rvir) : 0.0;
}

double estimate_merging_time(const int halonr, const int mother_halo, const struct halo_data *halos,
                            const double time, const struct params *run_params) {
    (void)halonr; (void)mother_halo; (void)halos; (void)run_params;
    return time + 1.0; // Arbitrary future time
}

void deal_with_galaxy_merger(int p, int merger_centralgal, int centralgal, double time,
                            int ngal, struct GALAXY *galaxy, struct params *run_params) {
    (void)p; (void)merger_centralgal; (void)centralgal; (void)time;
    (void)ngal; (void)galaxy; (void)run_params;
    // No-op for physics-free mode
}
