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
    galaxies[p].SnapNum = halos[halonr].SnapNum - 1;
    
    // Set galaxy type: central (0) if this is the first halo in FOF group, satellite (1) otherwise
    if (halonr == halos[halonr].FirstHaloInFOFgroup) {
        galaxies[p].Type = 0;  // Central galaxy
    } else {
        galaxies[p].Type = 1;  // Satellite galaxy
    }

    galaxies[p].GalaxyNr = *galaxycounter;
    (*galaxycounter)++;

    galaxies[p].HaloNr = halonr;
    galaxies[p].MostBoundID = halos[halonr].MostBoundID;

    galaxies[p].GalaxyIndex = (uint64_t)p;  // TODO: not core, and calculate at output
    galaxies[p].CentralGalaxyIndex = (uint64_t)p;  // TODO: not core, and calculate at output
    
    // Initialize from halo data
    for (int j = 0; j < 3; j++) {
        galaxies[p].Pos[j] = halos[halonr].Pos[j];
        galaxies[p].Vel[j] = halos[halonr].Vel[j];
    }
    
    galaxies[p].Len = halos[halonr].Len;
    galaxies[p].Vmax = halos[halonr].Vmax;
    galaxies[p].Rvir = get_virial_radius(halonr, halos, run_params);
    galaxies[p].Mvir = get_virial_mass(halonr, halos, run_params);;
    galaxies[p].Vvir = get_virial_velocity(halonr, halos, run_params);

    galaxies[p].deltaMvir = 0.0;
    
    // infall properties
    galaxies[p].infallMvir = -1.0;
    galaxies[p].infallVvir = -1.0;
    galaxies[p].infallVmax = -1.0;

    if (allocate_galaxy_properties(&galaxies[p], run_params) != 0) {
        LOG_ERROR("Failed to allocate galaxy properties for galaxy %d", p);
    }
}

double get_virial_mass(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    // TODO: update with legacy code equivalent
    (void)run_params;
    return halos[halonr].Mvir;
}

double get_virial_radius(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    // TODO: update with legacy code equivalent
    const double Mvir = halos[halonr].Mvir;
    const double Hubble_h = run_params->cosmology.Hubble_h;
    const double a = 1.0; // Use a=1 since Redshift field unavailable
    
    const double rho_crit = 2.775e11 * Hubble_h * Hubble_h;
    const double Delta_c = 200.0;
    const double rho_vir = Delta_c * rho_crit;
    
    return pow(3.0 * Mvir / (4.0 * M_PI * rho_vir), 1.0/3.0) * a;
}

double get_virial_velocity(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    // TODO: update with legacy code equivalent
    const double Mvir = halos[halonr].Mvir;
    const double Rvir = get_virial_radius(halonr, halos, run_params);
    const double G = 43.0071;
    
    return (Rvir > 0.0) ? sqrt(G * Mvir / Rvir) : 0.0;
}

double estimate_merging_time(const int halonr, const int mother_halo, const struct halo_data *halos,
                            const double time, const struct params *run_params) {
    // TODO: update with legacy code equivalent
    (void)halonr; (void)mother_halo; (void)halos; (void)run_params;
    return time + 1.0; // Arbitrary future time
}

void deal_with_galaxy_merger(int p, int merger_centralgal, int centralgal, double time,
                            int ngal, struct GALAXY *galaxy, struct params *run_params) {
    // TODO: update with legacy code equivalent
    (void)p; (void)merger_centralgal; (void)centralgal; (void)time;
    (void)ngal; (void)galaxy; (void)run_params;
}
