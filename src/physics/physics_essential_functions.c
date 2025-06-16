/**
 * @file physics_essential_functions.c
 * @brief Essential functions required for core-physics separation
 *
 * This file provides implementations of functions that are required by the core SAGE 
 * infrastructure but should remain physics-agnostic. These functions enable the core 
 * to operate in "physics-free mode" with empty pipelines while maintaining the 
 * essential function signatures that core components depend on.
 *
 * The functions in this file:
 * - Initialize galaxies with core properties only (physics handled by property system)
 * - Provide halo/tree property calculations (virial mass/radius/velocity from merger trees)
 * - Return sensible defaults for merger timing (no complex galaxy formation physics)
 * - Implement no-op merger handling for physics-free mode
 *
 * IMPORTANT DISTINCTION:
 * - Halo/tree property calculations (virial properties) are CORE functionality - these 
 *   are fundamental properties from input merger trees, not galaxy formation physics
 * - Galaxy formation physics (cooling, star formation, feedback) belongs in physics modules
 * - This separation allows core infrastructure to process merger trees independently 
 *   of galaxy formation models
 *
 * @note Merger functions are minimal stubs - full merger physics belongs in physics modules
 */

#include <math.h>
#include "core_allvars.h"
#include "core_properties.h"
#include "core_property_utils.h"
#include "core_logging.h"
#include "physics_essential_functions.h"

void init_galaxy(int p, int halonr, int32_t *galaxycounter, const struct halo_data *halos, 
                 struct GALAXY *galaxies, const struct params *run_params) {
    
    if (halonr != halos[halonr].FirstHaloInFOFgroup) {
        LOG_ERROR("Halo validation failed: halonr=%d should equal FirstHaloInFOFgroup=%d", 
                halonr, halos[halonr].FirstHaloInFOFgroup);
    }

    // Zero-initialize the galaxy struct first
    memset(&galaxies[p], 0, sizeof(struct GALAXY));
    
    // Allocate the properties structure
    if (allocate_galaxy_properties(&galaxies[p], run_params) != 0) {
        LOG_ERROR("Failed to allocate galaxy properties for galaxy %d", p);
        return;
    }
    
    // Initialize ALL properties with their default values from properties.yaml
    initialize_all_properties(&galaxies[p]);
    
    // Now set core properties directly from halo data (single source of truth)
    GALAXY_PROP_SnapNum(&galaxies[p]) = halos[halonr].SnapNum - 1;
    GALAXY_PROP_Type(&galaxies[p]) = 0;  // Central galaxy initially
    GALAXY_PROP_GalaxyNr(&galaxies[p]) = *galaxycounter;
    (*galaxycounter)++;
    GALAXY_PROP_HaloNr(&galaxies[p]) = halonr;
    GALAXY_PROP_MostBoundID(&galaxies[p]) = halos[halonr].MostBoundID;
    
    // Copy position, velocity, and spin arrays directly from halo data
    for (int j = 0; j < 3; j++) {
        GALAXY_PROP_Pos(&galaxies[p])[j] = halos[halonr].Pos[j];
        GALAXY_PROP_Vel(&galaxies[p])[j] = halos[halonr].Vel[j];
        GALAXY_PROP_Spin(&galaxies[p])[j] = halos[halonr].Spin[j];
    }
    
    // Set scalar halo properties directly from halo data
    GALAXY_PROP_Len(&galaxies[p]) = halos[halonr].Len;
    GALAXY_PROP_Vmax(&galaxies[p]) = halos[halonr].Vmax;
    GALAXY_PROP_VelDisp(&galaxies[p]) = halos[halonr].VelDisp;
    GALAXY_PROP_Rvir(&galaxies[p]) = get_virial_radius(halonr, halos, run_params);
    GALAXY_PROP_Mvir(&galaxies[p]) = get_virial_mass(halonr, halos, run_params);
    GALAXY_PROP_Vvir(&galaxies[p]) = get_virial_velocity(halonr, halos, run_params);
    
    // Extension system initialization (not part of property system)
    galaxies[p].extension_data = NULL;
    galaxies[p].num_extensions = 0;
    galaxies[p].extension_flags = 0;
}

double get_virial_mass(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    if (halonr == halos[halonr].FirstHaloInFOFgroup && halos[halonr].Mvir >= 0.0)
        return halos[halonr].Mvir; /* take spherical overdensity mass estimate */
    else
        return halos[halonr].Len * run_params->cosmology.PartMass;
}

double get_virial_velocity(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    double Rvir;
    
    Rvir = get_virial_radius(halonr, halos, run_params);
    
    if (Rvir > 0.0)
        return sqrt(GRAVITY * get_virial_mass(halonr, halos, run_params) / Rvir);
    else
        return 0.0;
}

double get_virial_radius(const int halonr, const struct halo_data *halos, const struct params *run_params) {
    // return halos[halonr].Rvir;  // Used for Bolshoi
    
    double zplus1, hubble_of_z_sq, rhocrit, fac;
    
    zplus1 = 1 + run_params->simulation.ZZ[halos[halonr].SnapNum];
    hubble_of_z_sq = 
        run_params->cosmology.Hubble_h * run_params->cosmology.Hubble_h *
        (run_params->cosmology.Omega * zplus1 * zplus1 * zplus1 +
         (1 - run_params->cosmology.Omega - run_params->cosmology.OmegaLambda) * zplus1 * zplus1 +
         run_params->cosmology.OmegaLambda);
    
    rhocrit = 3 * hubble_of_z_sq / (8 * M_PI * GRAVITY);
    fac = 1 / (200 * 4 * M_PI / 3.0 * rhocrit);
    
    return cbrt(get_virial_mass(halonr, halos, run_params) * fac);
}

double estimate_merging_time(const int halonr, const int mother_halo, const struct halo_data *halos,
                            const double time, const struct params *run_params) {
    // Physics-free mode: return immediate merge time (no physics calculation)
    (void)halonr; (void)mother_halo; (void)halos; (void)run_params;
    return time + 1.0; // Arbitrary future time
}

void deal_with_galaxy_merger(int p, int merger_centralgal, int centralgal, double time,
                            int ngal, struct GALAXY *galaxy, struct params *run_params) {
    // Physics-free mode: no-op merger handling (physics will be handled by merger modules)
    (void)p; (void)merger_centralgal; (void)centralgal; (void)time;
    (void)ngal; (void)galaxy; (void)run_params;
}
