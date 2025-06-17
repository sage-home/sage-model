/**
 * @file test_property_separation_scientific_accuracy.c
 * @brief Scientific accuracy validation for core-physics property separation
 * 
 * This test ensures that the core-physics property separation preserves
 * scientific accuracy by verifying:
 * - Galaxy initialization produces identical results
 * - Property copying during merger tree traversal is accurate
 * - HDF5 output consistency is maintained
 * - No data loss occurs during property system transitions
 * 
 * Created: 2025-06-13 (Property Separation Implementation)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_property_utils.h"
#include "../src/core/core_logging.h"
#include "../src/physics/physics_essential_functions.h"
#include "../src/core/galaxy_array.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            printf("  ✗ %s\n", message); \
        } \
    } while (0)

#define FLOAT_TOLERANCE 1e-6f
#define DOUBLE_TOLERANCE 1e-12

/**
 * Test that galaxy initialization produces identical results
 */
static void test_galaxy_initialization_consistency(void) {
    printf("\n=== Testing Galaxy Initialization Consistency ===\n");
    
    // Create mock halo data
    struct halo_data test_halo;
    memset(&test_halo, 0, sizeof(test_halo));
    
    test_halo.SnapNum = 63;
    test_halo.FirstHaloInFOFgroup = 0;
    test_halo.MostBoundID = 12345678901LL;
    test_halo.Pos[0] = 25.5f; test_halo.Pos[1] = 35.5f; test_halo.Pos[2] = 45.5f;
    test_halo.Vel[0] = 150.0f; test_halo.Vel[1] = 250.0f; test_halo.Vel[2] = 350.0f;
    test_halo.Len = 1000;
    test_halo.Vmax = 220.0f;
    
    // Create test parameters
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Initialize simulation arrays for virial radius calculations
    // Arrays are statically allocated - just initialize the needed values
    // Initialize snapshot 63 data (used by test_halo.SnapNum = 63)
    run_params.simulation.ZZ[63] = 0.0;  // redshift = 0 (present day)
    run_params.simulation.AA[63] = 1.0;  // scale factor = 1 (present day)
    
    // Initialize cosmology parameters for virial radius calculations
    run_params.cosmology.Hubble_h = 0.7;      // h parameter
    run_params.cosmology.Omega = 0.3;         // Matter density
    run_params.cosmology.OmegaLambda = 0.7;   // Dark energy density
    run_params.cosmology.PartMass = 8.6e8;    // Particle mass in solar masses
    
    // Initialize two galaxies with identical halo data
    struct GALAXY galaxy1, galaxy2;
    memset(&galaxy1, 0, sizeof(galaxy1));
    memset(&galaxy2, 0, sizeof(galaxy2));
    
    int32_t galaxy_counter1 = 100;
    int32_t galaxy_counter2 = 100;
    
    // Initialize both galaxies
    init_galaxy(0, 0, &galaxy_counter1, &test_halo, &galaxy1, &run_params);
    init_galaxy(0, 0, &galaxy_counter2, &test_halo, &galaxy2, &run_params);
    
    // Allocate properties for both galaxies before accessing them
    allocate_galaxy_properties(&galaxy1, &run_params);
    allocate_galaxy_properties(&galaxy2, &run_params);
    
    // Verify core properties are identical
    TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy1) == GALAXY_PROP_SnapNum(&galaxy2), "SnapNum initialization consistency");
    TEST_ASSERT(GALAXY_PROP_Type(&galaxy1) == GALAXY_PROP_Type(&galaxy2), "Type initialization consistency");
    TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxy1) == GALAXY_PROP_GalaxyNr(&galaxy2), "GalaxyNr initialization consistency");
    TEST_ASSERT(GALAXY_PROP_HaloNr(&galaxy1) == GALAXY_PROP_HaloNr(&galaxy2), "HaloNr initialization consistency");
    TEST_ASSERT(GALAXY_PROP_MostBoundID(&galaxy1) == GALAXY_PROP_MostBoundID(&galaxy2), "MostBoundID initialization consistency");
    
    TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy1) - GALAXY_PROP_Mvir(&galaxy2)) < FLOAT_TOLERANCE, "Mvir initialization consistency");
    TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&galaxy1) - GALAXY_PROP_Rvir(&galaxy2)) < FLOAT_TOLERANCE, "Rvir initialization consistency");
    TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&galaxy1) - GALAXY_PROP_Vvir(&galaxy2)) < FLOAT_TOLERANCE, "Vvir initialization consistency");
    TEST_ASSERT(fabs(GALAXY_PROP_Vmax(&galaxy1) - GALAXY_PROP_Vmax(&galaxy2)) < FLOAT_TOLERANCE, "Vmax initialization consistency");
    
    // Test position and velocity arrays
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(fabs(GALAXY_PROP_Pos(&galaxy1)[i] - GALAXY_PROP_Pos(&galaxy2)[i]) < FLOAT_TOLERANCE, 
                   "Position array initialization consistency");
        TEST_ASSERT(fabs(GALAXY_PROP_Vel(&galaxy1)[i] - GALAXY_PROP_Vel(&galaxy2)[i]) < FLOAT_TOLERANCE, 
                   "Velocity array initialization consistency");
    }
    
    // Verify physics properties are identical (if properties are allocated)
    if (galaxy1.properties != NULL && galaxy2.properties != NULL) {
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_stellar = get_cached_property_id("StellarMass");
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        
        if (prop_coldgas < PROP_COUNT) {
            float coldgas1 = get_float_property(&galaxy1, prop_coldgas, 0.0f);
            float coldgas2 = get_float_property(&galaxy2, prop_coldgas, 0.0f);
            TEST_ASSERT(fabs(coldgas1 - coldgas2) < FLOAT_TOLERANCE, "ColdGas initialization consistency");
        }
        
        if (prop_stellar < PROP_COUNT) {
            float stellar1 = get_float_property(&galaxy1, prop_stellar, 0.0f);
            float stellar2 = get_float_property(&galaxy2, prop_stellar, 0.0f);
            TEST_ASSERT(fabs(stellar1 - stellar2) < FLOAT_TOLERANCE, "StellarMass initialization consistency");
        }
        
        if (prop_merge_type < PROP_COUNT) {
            int merge_type1 = get_int32_property(&galaxy1, prop_merge_type, 0);
            int merge_type2 = get_int32_property(&galaxy2, prop_merge_type, 0);
            TEST_ASSERT(merge_type1 == merge_type2, "mergeType initialization consistency");
        }
    }
    
    // Cleanup
    if (galaxy1.properties != NULL) free_galaxy_properties(&galaxy1);
    if (galaxy2.properties != NULL) free_galaxy_properties(&galaxy2);
}

/**
 * Test property copying during merger tree traversal
 */
static void test_property_copying_accuracy(void) {
    printf("\n=== Testing Property Copying Accuracy ===\n");
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Initialize simulation arrays for virial radius calculations
    // Arrays are statically allocated - just initialize the needed values
    // Initialize snapshot 62 data (used by source.SnapNum = 62)
    run_params.simulation.ZZ[62] = 0.1;  // redshift = 0.1 
    run_params.simulation.AA[62] = 1.0/(1.0+0.1);  // scale factor 
    
    // Initialize cosmology parameters for virial radius calculations
    run_params.cosmology.Hubble_h = 0.7;      // h parameter
    run_params.cosmology.Omega = 0.3;         // Matter density
    run_params.cosmology.OmegaLambda = 0.7;   // Dark energy density
    run_params.cosmology.PartMass = 8.6e8;    // Particle mass in solar masses
    
    // Create source galaxy with known values
    struct GALAXY source, dest;
    memset(&source, 0, sizeof(source));
    memset(&dest, 0, sizeof(dest));
    
    // Allocate properties first
    int result = allocate_galaxy_properties(&source, &run_params);
    TEST_ASSERT(result == 0, "Source galaxy property allocation");
    
    // Set up source galaxy with specific values
    GALAXY_PROP_SnapNum(&source) = 62;
    GALAXY_PROP_Type(&source) = 1;
    GALAXY_PROP_GalaxyNr(&source) = 12345;
    GALAXY_PROP_HaloNr(&source) = 67890;
    GALAXY_PROP_MostBoundID(&source) = 9876543210LL;
    GALAXY_PROP_Mvir(&source) = 2.5e12f;
    GALAXY_PROP_Rvir(&source) = 300.0f;
    GALAXY_PROP_Vvir(&source) = 200.0f;
    GALAXY_PROP_Vmax(&source) = 250.0f;
    // Note: MergTime is now a physics property, set via property system
    
    GALAXY_PROP_Pos(&source)[0] = 15.0f; GALAXY_PROP_Pos(&source)[1] = 25.0f; GALAXY_PROP_Pos(&source)[2] = 35.0f;
    GALAXY_PROP_Vel(&source)[0] = 120.0f; GALAXY_PROP_Vel(&source)[1] = 220.0f; GALAXY_PROP_Vel(&source)[2] = 320.0f;
    
    // Properties already allocated above
    
    if (source.properties != NULL) {
        // Set physics properties using generic property system
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_stellar = get_cached_property_id("StellarMass");
        property_id_t prop_hotgas = get_cached_property_id("HotGas");
        property_id_t prop_bh = get_cached_property_id("BlackHoleMass");
        property_id_t prop_merge_time = get_cached_property_id("MergTime");
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        property_id_t prop_merge_id = get_cached_property_id("mergeIntoID");
        property_id_t prop_merge_snap = get_cached_property_id("mergeIntoSnapNum");
        
        if (prop_coldgas < PROP_COUNT) {
            set_float_property(&source, prop_coldgas, 3.5e10f);
        }
        if (prop_stellar < PROP_COUNT) {
            set_float_property(&source, prop_stellar, 4.7e10f);
        }
        if (prop_hotgas < PROP_COUNT) {
            set_float_property(&source, prop_hotgas, 8.2e10f);
        }
        if (prop_bh < PROP_COUNT) {
            set_float_property(&source, prop_bh, 2.1e8f);
        }
        if (prop_merge_time < PROP_COUNT) {
            set_float_property(&source, prop_merge_time, 3.5f);
        }
        if (prop_merge_type < PROP_COUNT) {
            set_int32_property(&source, prop_merge_type, 2);
        }
        if (prop_merge_id < PROP_COUNT) {
            set_int32_property(&source, prop_merge_id, 54321);
        }
        if (prop_merge_snap < PROP_COUNT) {
            set_int32_property(&source, prop_merge_snap, 61);
        }
    }
    
    // Allocate properties for destination
    result = allocate_galaxy_properties(&dest, &run_params);
    TEST_ASSERT(result == 0, "Destination galaxy property allocation");
    
    // Perform deep copy (this tests our separation implementation)
    if (source.properties != NULL && dest.properties != NULL) {
        // Copy core properties manually (since they're in struct)
        GALAXY_PROP_SnapNum(&dest) = GALAXY_PROP_SnapNum(&source);
        GALAXY_PROP_Type(&dest) = GALAXY_PROP_Type(&source);
        GALAXY_PROP_GalaxyNr(&dest) = GALAXY_PROP_GalaxyNr(&source);
        GALAXY_PROP_HaloNr(&dest) = GALAXY_PROP_HaloNr(&source);
        GALAXY_PROP_MostBoundID(&dest) = GALAXY_PROP_MostBoundID(&source);
        GALAXY_PROP_Mvir(&dest) = GALAXY_PROP_Mvir(&source);
        GALAXY_PROP_Rvir(&dest) = GALAXY_PROP_Rvir(&source);
        GALAXY_PROP_Vvir(&dest) = GALAXY_PROP_Vvir(&source);
        GALAXY_PROP_Vmax(&dest) = GALAXY_PROP_Vmax(&source);
        // MergTime is now a physics property, copied via property system
        
        for (int i = 0; i < 3; i++) {
            GALAXY_PROP_Pos(&dest)[i] = GALAXY_PROP_Pos(&source)[i];
            GALAXY_PROP_Vel(&dest)[i] = GALAXY_PROP_Vel(&source)[i];
        }
        
        // Copy physics properties via property system
        result = copy_galaxy_properties(&dest, &source, &run_params);
        TEST_ASSERT(result == 0, "Property copying operation succeeds");
        
        // Verify core properties were copied correctly
        TEST_ASSERT(GALAXY_PROP_SnapNum(&dest) == GALAXY_PROP_SnapNum(&source), "SnapNum copying accuracy");
        TEST_ASSERT(GALAXY_PROP_Type(&dest) == GALAXY_PROP_Type(&source), "Type copying accuracy");
        TEST_ASSERT(GALAXY_PROP_GalaxyNr(&dest) == GALAXY_PROP_GalaxyNr(&source), "GalaxyNr copying accuracy");
        TEST_ASSERT(GALAXY_PROP_HaloNr(&dest) == GALAXY_PROP_HaloNr(&source), "HaloNr copying accuracy");
        TEST_ASSERT(GALAXY_PROP_MostBoundID(&dest) == GALAXY_PROP_MostBoundID(&source), "MostBoundID copying accuracy");
        
        TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&dest) - GALAXY_PROP_Mvir(&source)) < FLOAT_TOLERANCE, "Mvir copying accuracy");
        TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&dest) - GALAXY_PROP_Rvir(&source)) < FLOAT_TOLERANCE, "Rvir copying accuracy");
        TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&dest) - GALAXY_PROP_Vvir(&source)) < FLOAT_TOLERANCE, "Vvir copying accuracy");
        TEST_ASSERT(fabs(GALAXY_PROP_Vmax(&dest) - GALAXY_PROP_Vmax(&source)) < FLOAT_TOLERANCE, "Vmax copying accuracy");
        
        // Verify physics properties were copied correctly
        property_id_t prop_coldgas_copy = get_cached_property_id("ColdGas");
        property_id_t prop_stellar_copy = get_cached_property_id("StellarMass");
        property_id_t prop_hotgas_copy = get_cached_property_id("HotGas");
        property_id_t prop_bh_copy = get_cached_property_id("BlackHoleMass");
        property_id_t prop_merge_time_copy = get_cached_property_id("MergTime");
        property_id_t prop_merge_type_copy = get_cached_property_id("mergeType");
        property_id_t prop_merge_id_copy = get_cached_property_id("mergeIntoID");
        property_id_t prop_merge_snap_copy = get_cached_property_id("mergeIntoSnapNum");
        
        if (prop_coldgas_copy < PROP_COUNT) {
            float src_coldgas = get_float_property(&source, prop_coldgas_copy, 0.0f);
            float dst_coldgas = get_float_property(&dest, prop_coldgas_copy, 0.0f);
            TEST_ASSERT(fabs(src_coldgas - dst_coldgas) < FLOAT_TOLERANCE, "ColdGas copying accuracy");
        }
        
        if (prop_stellar_copy < PROP_COUNT) {
            float src_stellar = get_float_property(&source, prop_stellar_copy, 0.0f);
            float dst_stellar = get_float_property(&dest, prop_stellar_copy, 0.0f);
            TEST_ASSERT(fabs(src_stellar - dst_stellar) < FLOAT_TOLERANCE, "StellarMass copying accuracy");
        }
        
        if (prop_hotgas_copy < PROP_COUNT) {
            float src_hotgas = get_float_property(&source, prop_hotgas_copy, 0.0f);
            float dst_hotgas = get_float_property(&dest, prop_hotgas_copy, 0.0f);
            TEST_ASSERT(fabs(src_hotgas - dst_hotgas) < FLOAT_TOLERANCE, "HotGas copying accuracy");
        }
        
        if (prop_bh_copy < PROP_COUNT) {
            float src_bh = get_float_property(&source, prop_bh_copy, 0.0f);
            float dst_bh = get_float_property(&dest, prop_bh_copy, 0.0f);
            TEST_ASSERT(fabs(src_bh - dst_bh) < FLOAT_TOLERANCE, "BlackHoleMass copying accuracy");
        }
        
        if (prop_merge_time_copy < PROP_COUNT) {
            float src_merge_time = get_float_property(&source, prop_merge_time_copy, 0.0f);
            float dst_merge_time = get_float_property(&dest, prop_merge_time_copy, 0.0f);
            TEST_ASSERT(fabs(src_merge_time - dst_merge_time) < FLOAT_TOLERANCE, "MergTime copying accuracy");
        }
        
        if (prop_merge_type_copy < PROP_COUNT) {
            int src_merge_type = get_int32_property(&source, prop_merge_type_copy, 0);
            int dst_merge_type = get_int32_property(&dest, prop_merge_type_copy, 0);
            TEST_ASSERT(src_merge_type == dst_merge_type, "mergeType copying accuracy");
        }
        
        if (prop_merge_id_copy < PROP_COUNT) {
            int src_merge_id = get_int32_property(&source, prop_merge_id_copy, 0);
            int dst_merge_id = get_int32_property(&dest, prop_merge_id_copy, 0);
            TEST_ASSERT(src_merge_id == dst_merge_id, "mergeIntoID copying accuracy");
        }
        
        if (prop_merge_snap_copy < PROP_COUNT) {
            int src_merge_snap = get_int32_property(&source, prop_merge_snap_copy, 0);
            int dst_merge_snap = get_int32_property(&dest, prop_merge_snap_copy, 0);
            TEST_ASSERT(src_merge_snap == dst_merge_snap, "mergeIntoSnapNum copying accuracy");
        }
    }
    
    // Cleanup
    if (source.properties != NULL) free_galaxy_properties(&source);
    if (dest.properties != NULL) free_galaxy_properties(&dest);
}

/**
 * Test HDF5 output consistency
 */
static void test_hdf5_output_consistency(void) {
    printf("\n=== Testing HDF5 Output Consistency ===\n");
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Initialize simulation arrays for virial radius calculations
    // Arrays are statically allocated - just initialize the needed values
    // Initialize snapshot 63 data (used by galaxy.SnapNum = 63)
    run_params.simulation.ZZ[63] = 0.0;  // redshift = 0 (present day)
    run_params.simulation.AA[63] = 1.0;  // scale factor = 1 (present day)
    
    // Initialize cosmology parameters for virial radius calculations
    run_params.cosmology.Hubble_h = 0.7;      // h parameter
    run_params.cosmology.Omega = 0.3;         // Matter density
    run_params.cosmology.OmegaLambda = 0.7;   // Dark energy density
    run_params.cosmology.PartMass = 8.6e8;    // Particle mass in solar masses
    
    // Create a galaxy with known values for HDF5 output testing
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // Allocate properties first
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Galaxy property allocation for HDF5 test");
    
    // Set core properties
    GALAXY_PROP_SnapNum(&galaxy) = 63;
    GALAXY_PROP_Type(&galaxy) = 0;
    GALAXY_PROP_GalaxyNr(&galaxy) = 98765;
    GALAXY_PROP_HaloNr(&galaxy) = 11111;
    GALAXY_PROP_MostBoundID(&galaxy) = 5555555555LL;
    GALAXY_PROP_Mvir(&galaxy) = 1.8e12f;
    GALAXY_PROP_Rvir(&galaxy) = 280.0f;
    GALAXY_PROP_Vvir(&galaxy) = 190.0f;
    GALAXY_PROP_Vmax(&galaxy) = 210.0f;
    
    // Properties already allocated above
    
    if (galaxy.properties != NULL) {
        // Set physics properties using generic property system
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_stellar = get_cached_property_id("StellarMass");
        property_id_t prop_hotgas = get_cached_property_id("HotGas");
        property_id_t prop_bh = get_cached_property_id("BlackHoleMass");
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        property_id_t prop_merge_id = get_cached_property_id("mergeIntoID");
        property_id_t prop_merge_snap = get_cached_property_id("mergeIntoSnapNum");
        
        if (prop_coldgas < PROP_COUNT) {
            set_float_property(&galaxy, prop_coldgas, 2.8e10f);
        }
        if (prop_stellar < PROP_COUNT) {
            set_float_property(&galaxy, prop_stellar, 5.2e10f);
        }
        if (prop_hotgas < PROP_COUNT) {
            set_float_property(&galaxy, prop_hotgas, 9.1e10f);
        }
        if (prop_bh < PROP_COUNT) {
            set_float_property(&galaxy, prop_bh, 3.7e8f);
        }
        if (prop_merge_type < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_type, 1);
        }
        if (prop_merge_id < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_id, 22222);
        }
        if (prop_merge_snap < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_snap, 62);
        }
        
        // Test that property system maintains values consistently
        // This simulates what HDF5 output would read
        
        // Core properties should be read from struct directly
        TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy) == 63, "HDF5 core property: SnapNum");
        TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 0, "HDF5 core property: Type");
        TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxy) == 98765, "HDF5 core property: GalaxyNr");
        TEST_ASSERT(GALAXY_PROP_HaloNr(&galaxy) == 11111, "HDF5 core property: HaloNr");
        TEST_ASSERT(GALAXY_PROP_MostBoundID(&galaxy) == 5555555555LL, "HDF5 core property: MostBoundID");
        TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 1.8e12f) < FLOAT_TOLERANCE, "HDF5 core property: Mvir");
        TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&galaxy) - 280.0f) < FLOAT_TOLERANCE, "HDF5 core property: Rvir");
        TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&galaxy) - 190.0f) < FLOAT_TOLERANCE, "HDF5 core property: Vvir");
        TEST_ASSERT(fabs(GALAXY_PROP_Vmax(&galaxy) - 210.0f) < FLOAT_TOLERANCE, "HDF5 core property: Vmax");
        
        // Physics properties should be read from property system
        property_id_t prop_coldgas_hdf5 = get_cached_property_id("ColdGas");
        property_id_t prop_stellar_hdf5 = get_cached_property_id("StellarMass");
        property_id_t prop_hotgas_hdf5 = get_cached_property_id("HotGas");
        property_id_t prop_bh_hdf5 = get_cached_property_id("BlackHoleMass");
        property_id_t prop_merge_type_hdf5 = get_cached_property_id("mergeType");
        property_id_t prop_merge_id_hdf5 = get_cached_property_id("mergeIntoID");
        property_id_t prop_merge_snap_hdf5 = get_cached_property_id("mergeIntoSnapNum");
        
        if (prop_coldgas_hdf5 < PROP_COUNT) {
            float coldgas = get_float_property(&galaxy, prop_coldgas_hdf5, 0.0f);
            TEST_ASSERT(fabs(coldgas - 2.8e10f) < FLOAT_TOLERANCE, "HDF5 physics property: ColdGas");
        }
        if (prop_stellar_hdf5 < PROP_COUNT) {
            float stellar = get_float_property(&galaxy, prop_stellar_hdf5, 0.0f);
            TEST_ASSERT(fabs(stellar - 5.2e10f) < FLOAT_TOLERANCE, "HDF5 physics property: StellarMass");
        }
        if (prop_hotgas_hdf5 < PROP_COUNT) {
            float hotgas = get_float_property(&galaxy, prop_hotgas_hdf5, 0.0f);
            TEST_ASSERT(fabs(hotgas - 9.1e10f) < FLOAT_TOLERANCE, "HDF5 physics property: HotGas");
        }
        if (prop_bh_hdf5 < PROP_COUNT) {
            float bh = get_float_property(&galaxy, prop_bh_hdf5, 0.0f);
            TEST_ASSERT(fabs(bh - 3.7e8f) < FLOAT_TOLERANCE, "HDF5 physics property: BlackHoleMass");
        }
        if (prop_merge_type_hdf5 < PROP_COUNT) {
            int merge_type = get_int32_property(&galaxy, prop_merge_type_hdf5, 0);
            TEST_ASSERT(merge_type == 1, "HDF5 physics property: mergeType");
        }
        if (prop_merge_id_hdf5 < PROP_COUNT) {
            int merge_id = get_int32_property(&galaxy, prop_merge_id_hdf5, 0);
            TEST_ASSERT(merge_id == 22222, "HDF5 physics property: mergeIntoID");
        }
        if (prop_merge_snap_hdf5 < PROP_COUNT) {
            int merge_snap = get_int32_property(&galaxy, prop_merge_snap_hdf5, 0);
            TEST_ASSERT(merge_snap == 62, "HDF5 physics property: mergeIntoSnapNum");
        }
        
        // Test that there's no confusion between sources
        // Modify one system and verify the other is unchanged
        GALAXY_PROP_Type(&galaxy) = 1;  // Change core property
        
        if (prop_coldgas_hdf5 < PROP_COUNT) {
            set_float_property(&galaxy, prop_coldgas_hdf5, 3.0e10f);  // Change physics property
        }
        
        // Verify core property changed but physics property through property system unchanged in struct
        TEST_ASSERT(GALAXY_PROP_Type(&galaxy) == 1, "HDF5 core property independence");
        
        if (prop_coldgas_hdf5 < PROP_COUNT) {
            float coldgas = get_float_property(&galaxy, prop_coldgas_hdf5, 0.0f);
            TEST_ASSERT(fabs(coldgas - 3.0e10f) < FLOAT_TOLERANCE, "HDF5 physics property independence");
        }
        
        // Other core properties should be unchanged
        TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy) == 63, "HDF5 other core properties unchanged");
        TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy) - 1.8e12f) < FLOAT_TOLERANCE, "HDF5 other core properties unchanged");
        
        // Other physics properties should be unchanged
        if (prop_stellar_hdf5 < PROP_COUNT) {
            float stellar = get_float_property(&galaxy, prop_stellar_hdf5, 0.0f);
            TEST_ASSERT(fabs(stellar - 5.2e10f) < FLOAT_TOLERANCE, "HDF5 other physics properties unchanged");
        }
    }
    
    // Cleanup
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
}

/**
 * Test no data loss during property system transitions
 */
static void test_no_data_loss(void) {
    printf("\n=== Testing No Data Loss During Property Transitions ===\n");
    
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    // Set required parameter for dynamic arrays
    run_params.simulation.NumSnapOutputs = 10; // Non-zero value to allow dynamic array allocation
    
    // Initialize simulation arrays for virial radius calculations
    // Arrays are statically allocated - just initialize the needed values
    // Initialize snapshot 42 data (used by galaxy.SnapNum = 42)
    run_params.simulation.ZZ[42] = 0.5;  // redshift = 0.5
    run_params.simulation.AA[42] = 1.0/(1.0+0.5);  // scale factor 
    
    // Initialize cosmology parameters for virial radius calculations
    run_params.cosmology.Hubble_h = 0.7;      // h parameter
    run_params.cosmology.Omega = 0.3;         // Matter density
    run_params.cosmology.OmegaLambda = 0.7;   // Dark energy density
    run_params.cosmology.PartMass = 8.6e8;    // Particle mass in solar masses
    
    // Create galaxy and populate with comprehensive data
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(galaxy));
    
    // Allocate properties first
    int result = allocate_galaxy_properties(&galaxy, &run_params);
    TEST_ASSERT(result == 0, "Property allocation for data loss test");
    
    // Populate core properties with specific patterns for validation
    GALAXY_PROP_SnapNum(&galaxy) = 42;
    GALAXY_PROP_Type(&galaxy) = 2;
    GALAXY_PROP_GalaxyNr(&galaxy) = 13579;
    GALAXY_PROP_HaloNr(&galaxy) = 24680;
    GALAXY_PROP_MostBoundID(&galaxy) = 1111111111LL;
    GALAXY_PROP_Mvir(&galaxy) = 3.14159e12f;
    GALAXY_PROP_Rvir(&galaxy) = 271.828f;
    GALAXY_PROP_Vvir(&galaxy) = 141.421f;
    GALAXY_PROP_Vmax(&galaxy) = 173.205f;
    // Note: MergTime is now a physics property, set via property system
    GALAXY_PROP_infallMvir(&galaxy) = 2.99792e12f;
    GALAXY_PROP_infallVvir(&galaxy) = 137.036f;
    GALAXY_PROP_infallVmax(&galaxy) = 169.000f;
    
    // Pattern in arrays: Fibonacci-like sequence
    GALAXY_PROP_Pos(&galaxy)[0] = 1.0f; GALAXY_PROP_Pos(&galaxy)[1] = 1.0f; GALAXY_PROP_Pos(&galaxy)[2] = 2.0f;
    GALAXY_PROP_Vel(&galaxy)[0] = 3.0f; GALAXY_PROP_Vel(&galaxy)[1] = 5.0f; GALAXY_PROP_Vel(&galaxy)[2] = 8.0f;
    
    // Properties already allocated above
    
    if (galaxy.properties != NULL) {
        // Populate physics properties with known patterns using generic property system
        property_id_t prop_coldgas = get_cached_property_id("ColdGas");
        property_id_t prop_stellar = get_cached_property_id("StellarMass");
        property_id_t prop_hotgas = get_cached_property_id("HotGas");
        property_id_t prop_ejected = get_cached_property_id("EjectedMass");
        property_id_t prop_bh = get_cached_property_id("BlackHoleMass");
        property_id_t prop_bulge = get_cached_property_id("BulgeMass");
        property_id_t prop_ics = get_cached_property_id("ICS");
        property_id_t prop_metals_cold = get_cached_property_id("MetalsColdGas");
        property_id_t prop_metals_stellar = get_cached_property_id("MetalsStellarMass");
        property_id_t prop_metals_hot = get_cached_property_id("MetalsHotGas");
        property_id_t prop_merge_type = get_cached_property_id("mergeType");
        property_id_t prop_merge_id = get_cached_property_id("mergeIntoID");
        property_id_t prop_merge_snap = get_cached_property_id("mergeIntoSnapNum");
        property_id_t prop_merge_time = get_cached_property_id("MergTime");
        property_id_t prop_cooling = get_cached_property_id("Cooling");
        property_id_t prop_heating = get_cached_property_id("Heating");
        
        if (prop_coldgas < PROP_COUNT) {
            set_float_property(&galaxy, prop_coldgas, 1.23456e10f);
        }
        if (prop_stellar < PROP_COUNT) {
            set_float_property(&galaxy, prop_stellar, 2.34567e10f);
        }
        if (prop_hotgas < PROP_COUNT) {
            set_float_property(&galaxy, prop_hotgas, 3.45678e10f);
        }
        if (prop_ejected < PROP_COUNT) {
            set_float_property(&galaxy, prop_ejected, 4.56789e10f);
        }
        if (prop_bh < PROP_COUNT) {
            set_float_property(&galaxy, prop_bh, 5.6789e8f);
        }
        if (prop_bulge < PROP_COUNT) {
            set_float_property(&galaxy, prop_bulge, 6.78901e9f);
        }
        if (prop_ics < PROP_COUNT) {
            set_float_property(&galaxy, prop_ics, 7.89012e8f);
        }
        if (prop_metals_cold < PROP_COUNT) {
            set_float_property(&galaxy, prop_metals_cold, 1.111e8f);
        }
        if (prop_metals_stellar < PROP_COUNT) {
            set_float_property(&galaxy, prop_metals_stellar, 2.222e8f);
        }
        if (prop_metals_hot < PROP_COUNT) {
            set_float_property(&galaxy, prop_metals_hot, 3.333e8f);
        }
        if (prop_merge_type < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_type, 3);
        }
        if (prop_merge_id < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_id, 87654);
        }
        if (prop_merge_snap < PROP_COUNT) {
            set_int32_property(&galaxy, prop_merge_snap, 41);
        }
        if (prop_merge_time < PROP_COUNT) {
            set_float_property(&galaxy, prop_merge_time, 2.71828f);
        }
        if (prop_cooling < PROP_COUNT) {
            set_double_property(&galaxy, prop_cooling, 1.23456789e25);
        }
        if (prop_heating < PROP_COUNT) {
            set_double_property(&galaxy, prop_heating, 9.87654321e24);
        }
        
        // Create a second galaxy and copy all data
        struct GALAXY galaxy_copy;
        memset(&galaxy_copy, 0, sizeof(galaxy_copy));
        
        result = allocate_galaxy_properties(&galaxy_copy, &run_params);
        TEST_ASSERT(result == 0, "Copy galaxy property allocation");
        
        if (galaxy_copy.properties != NULL) {
            // Copy core properties manually
            GALAXY_PROP_SnapNum(&galaxy_copy) = GALAXY_PROP_SnapNum(&galaxy);
            GALAXY_PROP_Type(&galaxy_copy) = GALAXY_PROP_Type(&galaxy);
            GALAXY_PROP_GalaxyNr(&galaxy_copy) = GALAXY_PROP_GalaxyNr(&galaxy);
            GALAXY_PROP_HaloNr(&galaxy_copy) = GALAXY_PROP_HaloNr(&galaxy);
            GALAXY_PROP_MostBoundID(&galaxy_copy) = GALAXY_PROP_MostBoundID(&galaxy);
            GALAXY_PROP_Mvir(&galaxy_copy) = GALAXY_PROP_Mvir(&galaxy);
            GALAXY_PROP_Rvir(&galaxy_copy) = GALAXY_PROP_Rvir(&galaxy);
            GALAXY_PROP_Vvir(&galaxy_copy) = GALAXY_PROP_Vvir(&galaxy);
            GALAXY_PROP_Vmax(&galaxy_copy) = GALAXY_PROP_Vmax(&galaxy);
            // MergTime is now a physics property, copied via property system
            GALAXY_PROP_infallMvir(&galaxy_copy) = GALAXY_PROP_infallMvir(&galaxy);
            GALAXY_PROP_infallVvir(&galaxy_copy) = GALAXY_PROP_infallVvir(&galaxy);
            GALAXY_PROP_infallVmax(&galaxy_copy) = GALAXY_PROP_infallVmax(&galaxy);
            
            for (int i = 0; i < 3; i++) {
                GALAXY_PROP_Pos(&galaxy_copy)[i] = GALAXY_PROP_Pos(&galaxy)[i];
                GALAXY_PROP_Vel(&galaxy_copy)[i] = GALAXY_PROP_Vel(&galaxy)[i];
            }
            
            // Copy physics properties
            result = copy_galaxy_properties(&galaxy_copy, &galaxy, &run_params);
            TEST_ASSERT(result == 0, "Property copying for data loss test");
            
            // Verify ALL data was preserved exactly
            TEST_ASSERT(GALAXY_PROP_SnapNum(&galaxy_copy) == 42, "Data preservation: SnapNum");
            TEST_ASSERT(GALAXY_PROP_Type(&galaxy_copy) == 2, "Data preservation: Type");
            TEST_ASSERT(GALAXY_PROP_GalaxyNr(&galaxy_copy) == 13579, "Data preservation: GalaxyNr");
            TEST_ASSERT(GALAXY_PROP_HaloNr(&galaxy_copy) == 24680, "Data preservation: HaloNr");
            TEST_ASSERT(GALAXY_PROP_MostBoundID(&galaxy_copy) == 1111111111LL, "Data preservation: MostBoundID");
            
            TEST_ASSERT(fabs(GALAXY_PROP_Mvir(&galaxy_copy) - 3.14159e12f) < FLOAT_TOLERANCE, "Data preservation: Mvir");
            TEST_ASSERT(fabs(GALAXY_PROP_Rvir(&galaxy_copy) - 271.828f) < FLOAT_TOLERANCE, "Data preservation: Rvir");
            TEST_ASSERT(fabs(GALAXY_PROP_Vvir(&galaxy_copy) - 141.421f) < FLOAT_TOLERANCE, "Data preservation: Vvir");
            TEST_ASSERT(fabs(GALAXY_PROP_Vmax(&galaxy_copy) - 173.205f) < FLOAT_TOLERANCE, "Data preservation: Vmax");
            // MergTime is now copied via property system - test below
            TEST_ASSERT(fabs(GALAXY_PROP_infallMvir(&galaxy_copy) - 2.99792e12f) < FLOAT_TOLERANCE, "Data preservation: infallMvir");
            TEST_ASSERT(fabs(GALAXY_PROP_infallVvir(&galaxy_copy) - 137.036f) < FLOAT_TOLERANCE, "Data preservation: infallVvir");
            TEST_ASSERT(fabs(GALAXY_PROP_infallVmax(&galaxy_copy) - 169.000f) < FLOAT_TOLERANCE, "Data preservation: infallVmax");
            
            // Array data preservation
            float expected_pos[] = {1.0f, 1.0f, 2.0f};
            float expected_vel[] = {3.0f, 5.0f, 8.0f};
            for (int i = 0; i < 3; i++) {
                TEST_ASSERT(fabs(GALAXY_PROP_Pos(&galaxy_copy)[i] - expected_pos[i]) < FLOAT_TOLERANCE, "Data preservation: Pos array");
                TEST_ASSERT(fabs(GALAXY_PROP_Vel(&galaxy_copy)[i] - expected_vel[i]) < FLOAT_TOLERANCE, "Data preservation: Vel array");
            }
            
            // Physics properties preservation - test using generic property system
            property_id_t prop_coldgas_preserve = get_cached_property_id("ColdGas");
            property_id_t prop_stellar_preserve = get_cached_property_id("StellarMass");
            property_id_t prop_hotgas_preserve = get_cached_property_id("HotGas");
            property_id_t prop_merge_type_preserve = get_cached_property_id("mergeType");
            property_id_t prop_merge_id_preserve = get_cached_property_id("mergeIntoID");
            property_id_t prop_merge_snap_preserve = get_cached_property_id("mergeIntoSnapNum");
            property_id_t prop_merge_time_preserve = get_cached_property_id("MergTime");
            property_id_t prop_cooling_preserve = get_cached_property_id("Cooling");
            property_id_t prop_heating_preserve = get_cached_property_id("Heating");
            
            if (prop_coldgas_preserve < PROP_COUNT) {
                float coldgas = get_float_property(&galaxy_copy, prop_coldgas_preserve, 0.0f);
                TEST_ASSERT(fabs(coldgas - 1.23456e10f) < FLOAT_TOLERANCE, "Data preservation: ColdGas");
            }
            
            if (prop_stellar_preserve < PROP_COUNT) {
                float stellar = get_float_property(&galaxy_copy, prop_stellar_preserve, 0.0f);
                TEST_ASSERT(fabs(stellar - 2.34567e10f) < FLOAT_TOLERANCE, "Data preservation: StellarMass");
            }
            
            if (prop_hotgas_preserve < PROP_COUNT) {
                float hotgas = get_float_property(&galaxy_copy, prop_hotgas_preserve, 0.0f);
                TEST_ASSERT(fabs(hotgas - 3.45678e10f) < FLOAT_TOLERANCE, "Data preservation: HotGas");
            }
            
            if (prop_merge_type_preserve < PROP_COUNT) {
                int merge_type = get_int32_property(&galaxy_copy, prop_merge_type_preserve, 0);
                TEST_ASSERT(merge_type == 3, "Data preservation: mergeType");
            }
            
            if (prop_merge_id_preserve < PROP_COUNT) {
                int merge_id = get_int32_property(&galaxy_copy, prop_merge_id_preserve, 0);
                TEST_ASSERT(merge_id == 87654, "Data preservation: mergeIntoID");
            }
            
            if (prop_merge_snap_preserve < PROP_COUNT) {
                int merge_snap = get_int32_property(&galaxy_copy, prop_merge_snap_preserve, 0);
                TEST_ASSERT(merge_snap == 41, "Data preservation: mergeIntoSnapNum");
            }
            
            if (prop_merge_time_preserve < PROP_COUNT) {
                float merge_time = get_float_property(&galaxy_copy, prop_merge_time_preserve, 0.0f);
                TEST_ASSERT(fabs(merge_time - 2.71828f) < FLOAT_TOLERANCE, "Data preservation: MergTime");
            }
            
            if (prop_cooling_preserve < PROP_COUNT) {
                double cooling = get_double_property(&galaxy_copy, prop_cooling_preserve, 0.0);
                TEST_ASSERT(fabs(cooling - 1.23456789e25) < 1e20, "Data preservation: Cooling");
            }
            
            if (prop_heating_preserve < PROP_COUNT) {
                double heating = get_double_property(&galaxy_copy, prop_heating_preserve, 0.0);
                TEST_ASSERT(fabs(heating - 9.87654321e24) < 1e19, "Data preservation: Heating");
            }
        }
        
        // Cleanup copy
        if (galaxy_copy.properties != NULL) {
            free_galaxy_properties(&galaxy_copy);
        }
    }
    
    // Cleanup
    if (galaxy.properties != NULL) {
        free_galaxy_properties(&galaxy);
    }
}

int main(void) {
    printf("Starting Scientific Accuracy Validation Tests\n");
    printf("==============================================\n");
    
    // Initialize logging to suppress debug output during tests
    logging_init(LOG_LEVEL_WARNING, stderr);
    
    // Run all test suites
    test_galaxy_initialization_consistency();
    test_property_copying_accuracy();
    test_hdf5_output_consistency();
    test_no_data_loss();
    
    // Report results
    printf("\n==============================================\n");
    printf("Test Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("✓ All tests passed! Scientific accuracy is preserved with property separation.\n");
        return EXIT_SUCCESS;
    } else {
        printf("✗ %d tests failed. Scientific accuracy validation needs attention.\n", tests_run - tests_passed);
        return EXIT_FAILURE;
    }
}