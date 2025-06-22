/**
 * @file test_helper.c
 * @brief Implementation of standardized test utilities for SAGE unit tests
 */

#include "test_helper.h"
#include "../src/core/core_mymalloc.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_parameters.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int setup_test_environment(struct TestContext* ctx, int nhalos) {
    memset(ctx, 0, sizeof(struct TestContext));
    
    // Initialize parameter system with defaults first
    if (initialize_parameter_system(&ctx->test_params) != 0) {
        printf("Failed to initialize parameter system\\n");
        return -1;
    }
    
    // **CRITICAL: Initialize core cosmological parameters**
    // Based on Millennium simulation parameters
    ctx->test_params.cosmology.BoxSize = 62.5;           // Mpc/h
    ctx->test_params.cosmology.Omega = 0.25;             // Matter density
    ctx->test_params.cosmology.OmegaLambda = 0.75;       // Dark energy density  
    ctx->test_params.cosmology.Hubble_h = 0.73;          // Hubble parameter
    ctx->test_params.cosmology.PartMass = 0.0860657;     // Particle mass in 10^10 Msun/h
    
    // Unit conversions (critical for physics calculations)
    ctx->test_params.units.UnitLength_in_cm = 3.085678e24;     // 1 Mpc in cm  
    ctx->test_params.units.UnitMass_in_g = 1.989e43;           // 10^10 Msun in g
    ctx->test_params.units.UnitVelocity_in_cm_per_s = 1e5;     // km/s in cm/s
    ctx->test_params.units.UnitTime_in_s = 3.085678e19;        // Mpc/km/s in s
    ctx->test_params.units.UnitTime_in_Megayears = 978.462;    // Time unit in Myr
    
    // Simulation parameters
    ctx->test_params.simulation.NumSnapOutputs = 10;
    ctx->test_params.simulation.SimMaxSnaps = 64;
    ctx->test_params.simulation.LastSnapshotNr = 63;
    ctx->test_params.io.FirstFile = 0;
    ctx->test_params.io.LastFile = 0;
    
    // **CRITICAL: Allocate and initialize Age array (prevents segfaults)**
    ctx->age_array = mymalloc_full(64 * sizeof(double), "test_ages");
    ctx->test_params.simulation.Age = ctx->age_array;
    
    // Initialize realistic age and redshift progression
    for (int i = 0; i < 64; i++) {
        // Age from 0.1 to 13.7 Gyr (age of universe)
        ctx->test_params.simulation.Age[i] = 0.1 + i * 0.21; 
        // Redshift from z=20 to z=0 (ZZ is a fixed array, not pointer)
        ctx->test_params.simulation.ZZ[i] = 20.0 * exp(-i * 0.075);
    }
    
    // Physics model parameters (reasonable defaults)
    ctx->test_params.physics.SfrEfficiency = 0.05;
    ctx->test_params.physics.FeedbackReheatingEpsilon = 3.0;
    ctx->test_params.physics.FeedbackEjectionEfficiency = 0.3;
    ctx->test_params.physics.ReIncorporationFactor = 0.15;
    ctx->test_params.physics.EnergySN = 1.0e51;
    ctx->test_params.physics.EtaSN = 8.0e-3;
    
    // File handling (prevent file access issues in tests)
    strncpy(ctx->test_params.io.FileNameGalaxies, "test_model", MAX_STRING_LEN-1);
    strncpy(ctx->test_params.io.OutputDir, "./test_output/", MAX_STRING_LEN-1);
    strncpy(ctx->test_params.io.TreeName, "test_trees", MAX_STRING_LEN-1);
    ctx->test_params.io.TreeType = lhalo_binary;  // Enum value, not string
    
    // Runtime parameters
    ctx->test_params.runtime.ThisTask = 0;
    ctx->test_params.runtime.NTasks = 1;
    
    // **Allocate halo arrays**
    ctx->nhalo = nhalos;
    ctx->halos = mymalloc_full(nhalos * sizeof(struct halo_data), "test_halos");
    ctx->haloaux = mymalloc_full(nhalos * sizeof(struct halo_aux_data), "test_haloaux");
    
    if (!ctx->halos || !ctx->haloaux) {
        printf("Failed to allocate test halo arrays\\n");
        return -1;
    }
    
    // Initialize halo arrays to zero
    memset(ctx->halos, 0, nhalos * sizeof(struct halo_data));
    memset(ctx->haloaux, 0, nhalos * sizeof(struct halo_aux_data));
    
    // **Create galaxy arrays**
    ctx->galaxies_prev_snap = galaxy_array_new();
    ctx->galaxies_this_snap = galaxy_array_new();
    
    if (!ctx->galaxies_prev_snap || !ctx->galaxies_this_snap) {
        printf("Failed to create galaxy arrays\\n");
        return -1;
    }
    
    ctx->galaxycounter = 1;
    ctx->initialized = 1;
    
    return 0;
}

void teardown_test_environment(struct TestContext* ctx) {
    if (!ctx || !ctx->initialized) return;
    
    // Free allocated arrays in reverse order
    if (ctx->age_array) {
        myfree(ctx->age_array);
        ctx->age_array = NULL;
        ctx->test_params.simulation.Age = NULL;
    }
    // ZZ is a fixed array, no need to free
    if (ctx->halos) {
        myfree(ctx->halos);
        ctx->halos = NULL;
    }
    if (ctx->haloaux) {
        myfree(ctx->haloaux);
        ctx->haloaux = NULL;
    }
    if (ctx->galaxies_prev_snap) {
        galaxy_array_free(&ctx->galaxies_prev_snap);
    }
    if (ctx->galaxies_this_snap) {
        galaxy_array_free(&ctx->galaxies_this_snap);
    }
    
    ctx->initialized = 0;
}

void create_test_halo(struct TestContext* ctx, int halo_idx, int snap_num, float mvir, 
                      int first_prog, int next_prog, int next_in_fof) {
    if (!ctx || !ctx->initialized || halo_idx >= ctx->nhalo) return;
    
    struct halo_data *halo = &ctx->halos[halo_idx];
    
    halo->SnapNum = snap_num;
    halo->Mvir = mvir;
    halo->FirstProgenitor = first_prog;
    halo->NextProgenitor = next_prog;
    halo->NextHaloInFOFgroup = next_in_fof;
    halo->MostBoundID = 1000000 + halo_idx;
    
    // Set realistic positions and velocities
    for (int i = 0; i < 3; i++) {
        halo->Pos[i] = 10.0f + halo_idx * 0.5f;  // Spread out in space
        halo->Vel[i] = 100.0f + halo_idx * 10.0f;
    }
    halo->Len = 100 + halo_idx;
    halo->Vmax = 200.0f + halo_idx;
    halo->VelDisp = 50.0f + halo_idx * 2.0f;
    // Note: Vvir and Rvir are not in halo_data structure
    // These would be calculated by SAGE functions when needed
    
    // Initialize aux data
    ctx->haloaux[halo_idx].FirstGalaxy = -1;
    ctx->haloaux[halo_idx].NGalaxies = 0;
}

int create_test_galaxy(struct TestContext* ctx, int galaxy_type, int halo_nr, float stellar_mass) {
    if (!ctx || !ctx->initialized || halo_nr >= ctx->nhalo) return -1;
    
    struct GALAXY temp_galaxy;
    memset(&temp_galaxy, 0, sizeof(struct GALAXY));
    
    // Initialize properties
    if (allocate_galaxy_properties(&temp_galaxy, &ctx->test_params) != 0) {
        printf("Failed to allocate galaxy properties\\n");
        return -1;
    }
    
    // Set basic properties
    GALAXY_PROP_Type(&temp_galaxy) = galaxy_type;
    GALAXY_PROP_HaloNr(&temp_galaxy) = halo_nr;
    GALAXY_PROP_GalaxyIndex(&temp_galaxy) = ctx->galaxycounter++;
    GALAXY_PROP_SnapNum(&temp_galaxy) = ctx->halos[halo_nr].SnapNum - 1;
    GALAXY_PROP_merged(&temp_galaxy) = 0;
    
    // Set masses and positions from halo
    GALAXY_PROP_Mvir(&temp_galaxy) = ctx->halos[halo_nr].Mvir;
    GALAXY_PROP_StellarMass(&temp_galaxy) = stellar_mass;
    GALAXY_PROP_ColdGas(&temp_galaxy) = stellar_mass * 0.3f;  // Realistic gas fraction
    GALAXY_PROP_HotGas(&temp_galaxy) = stellar_mass * 0.1f;
    
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos(&temp_galaxy)[i] = ctx->halos[halo_nr].Pos[i];
        GALAXY_PROP_Vel(&temp_galaxy)[i] = ctx->halos[halo_nr].Vel[i];
    }
    
    // Add to previous snapshot galaxies
    int galaxy_idx = galaxy_array_append(ctx->galaxies_prev_snap, &temp_galaxy, &ctx->test_params);
    
    // Update halo aux data
    if (ctx->haloaux[halo_nr].FirstGalaxy == -1) {
        ctx->haloaux[halo_nr].FirstGalaxy = galaxy_idx;
    }
    ctx->haloaux[halo_nr].NGalaxies++;
    
    // Clean up temporary galaxy
    free_galaxy_properties(&temp_galaxy);
    return galaxy_idx;
}

void reset_test_galaxies(struct TestContext* ctx) {
    if (!ctx || !ctx->initialized) return;
    
    // Free and recreate galaxy arrays
    galaxy_array_free(&ctx->galaxies_prev_snap);
    galaxy_array_free(&ctx->galaxies_this_snap);
    
    ctx->galaxies_prev_snap = galaxy_array_new();
    ctx->galaxies_this_snap = galaxy_array_new();
    
    // Reset galaxy counter and halo aux data
    ctx->galaxycounter = 1;
    for (int i = 0; i < ctx->nhalo; i++) {
        ctx->haloaux[i].FirstGalaxy = -1;
        ctx->haloaux[i].NGalaxies = 0;
    }
}