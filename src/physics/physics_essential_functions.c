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
#include "core_logging.h"
#include "physics_essential_functions.h"

#ifndef LOG_DEBUG
#define LOG_DEBUG(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
#endif

void init_galaxy(int p, int halonr, int32_t *galaxycounter, const struct halo_data *halos, 
                 struct GALAXY *galaxies, const struct params *run_params) {
    // Initialize core properties only

    if (halonr != halos[halonr].FirstHaloInFOFgroup) {
        LOG_ERROR("Halo validation failed: halonr=%d should equal FirstHaloInFOFgroup=%d", 
                halonr, halos[halonr].FirstHaloInFOFgroup);
    }

    // CRITICAL: Initialize properties pointer to NULL to prevent corruption
    galaxies[p].properties = NULL;
    
    galaxies[p].Type = 0;  // New galaxies start as a central galaxy
    
    galaxies[p].GalaxyNr = *galaxycounter;
    (*galaxycounter)++;
    
    galaxies[p].HaloNr = halonr;
    galaxies[p].MostBoundID = halos[halonr].MostBoundID;
    galaxies[p].SnapNum = halos[halonr].SnapNum - 1;
    
    // Initialize from halo data
    for (int j = 0; j < 3; j++) {
        galaxies[p].Pos[j] = halos[halonr].Pos[j];
        galaxies[p].Vel[j] = halos[halonr].Vel[j];
    }
    
    galaxies[p].Len = halos[halonr].Len;
    galaxies[p].Vmax = halos[halonr].Vmax;
    galaxies[p].Rvir = get_virial_radius(halonr, halos, run_params);
    galaxies[p].Mvir = get_virial_mass(halonr, halos, run_params);
    galaxies[p].Vvir = get_virial_velocity(halonr, halos, run_params);

    galaxies[p].deltaMvir = 0.0;
    
    // infall properties
    galaxies[p].infallMvir = -1.0;
    galaxies[p].infallVvir = -1.0;
    galaxies[p].infallVmax = -1.0;

    // Extension system initialization for compatibility
    galaxies[p].extension_data = NULL;
    galaxies[p].num_extensions = 0;
    galaxies[p].extension_flags = 0;
    
    // Initialize merger properties with defaults
    galaxies[p].mergeType = 0;
    galaxies[p].mergeIntoID = -1;
    galaxies[p].mergeIntoSnapNum = -1;
    galaxies[p].dT = -1.0;
    galaxies[p].MergTime = 0.0;

    // CRITICAL FIX: Allocate properties BEFORE any property system operations
    if (allocate_galaxy_properties(&galaxies[p], run_params) != 0) {
        LOG_ERROR("Failed to allocate galaxy properties for galaxy %d", p);
        return; // Early return on allocation failure to prevent further errors
    }
    
    // Set core galaxy properties using property system (critical for output phase)
    // Properties are now safely allocated above
    if (galaxies[p].properties != NULL) {
        // Update properties to match direct field values for consistency
        GALAXY_PROP_Type(&galaxies[p]) = galaxies[p].Type;
        GALAXY_PROP_SnapNum(&galaxies[p]) = galaxies[p].SnapNum;
        GALAXY_PROP_HaloNr(&galaxies[p]) = galaxies[p].HaloNr;
        GALAXY_PROP_MostBoundID(&galaxies[p]) = galaxies[p].MostBoundID;
        GALAXY_PROP_Len(&galaxies[p]) = galaxies[p].Len;
        GALAXY_PROP_Mvir(&galaxies[p]) = galaxies[p].Mvir;
        GALAXY_PROP_deltaMvir(&galaxies[p]) = galaxies[p].deltaMvir;
        GALAXY_PROP_Rvir(&galaxies[p]) = galaxies[p].Rvir;
        GALAXY_PROP_Vvir(&galaxies[p]) = galaxies[p].Vvir;
        GALAXY_PROP_Vmax(&galaxies[p]) = galaxies[p].Vmax;
        GALAXY_PROP_VelDisp(&galaxies[p]) = 0.0; // Default value
        GALAXY_PROP_MergTime(&galaxies[p]) = galaxies[p].MergTime;
        GALAXY_PROP_infallMvir(&galaxies[p]) = galaxies[p].infallMvir;
        GALAXY_PROP_infallVvir(&galaxies[p]) = galaxies[p].infallVvir;
        GALAXY_PROP_infallVmax(&galaxies[p]) = galaxies[p].infallVmax;
        
        // Set position and velocity arrays
        for (int j = 0; j < 3; j++) {
            GALAXY_PROP_Pos_ELEM(&galaxies[p], j) = galaxies[p].Pos[j];
            GALAXY_PROP_Vel_ELEM(&galaxies[p], j) = galaxies[p].Vel[j];
            GALAXY_PROP_Spin_ELEM(&galaxies[p], j) = 0.0; // Default value
        }
        
        /* Reduce noise - only log galaxy initialization for first 5 galaxies */
        static int init_count = 0;
        init_count++;
        if (init_count <= 5) {
            if (init_count == 5) {
                LOG_DEBUG("Successfully initialized galaxy %d with allocated properties (init #%d - further messages suppressed)", p, init_count);
            } else {
                LOG_DEBUG("Successfully initialized galaxy %d with allocated properties (init #%d)", p, init_count);
            }
        }
    } else {
        LOG_ERROR("CRITICAL ERROR: Galaxy %d properties allocation failed - this will cause output errors", p);
    }
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
