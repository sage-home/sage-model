#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "../src/io/io_interface.h"
// NOTE: This test imports the now-removed binary output format files.
// This test should be refactored or removed in the future now that binary output 
// has been deprecated and the files have been archived.
// The test already expects binary output to be deprecated, so it may still work,
// but should be restructured as part of a test suite cleanup.
// Archived file reference: io_binary_output.h (in ignore/202505141845)
#include "../src/io/io_property_serialization.h"
#include "../src/core/core_allvars.h"
#include "../src/core/core_galaxy_extensions.h"

// Mock galaxy extension registry
struct galaxy_extension_registry mock_registry;

// Mock galaxy property data
galaxy_property_t mock_properties[3];

// Mock halos data
struct halo_data mock_halos[10];

// Mock runtime parameters
struct params mock_params;

// Mock extended save info for testing (not the actual save_info structure)
struct extended_save_info {
    struct save_info base;
    struct params *params;
    struct halo_data *halos;
    int nforests;
    int rank;
    int *OutputLists;
    int original_treenr;
    int current_forest;
};

struct extended_save_info mock_save_info;

// Test data
float test_float = 3.14159f;
double test_double = 2.71828;
int32_t test_int32 = 42;

/**
 * @brief Setup mock extension registry for testing
 */
void setup_mock_registry() {
    // Initialize mock registry
    memset(&mock_registry, 0, sizeof(struct galaxy_extension_registry));
    
    // Initialize mock properties
    int prop_idx = 0;
    
    // Float property
    strcpy(mock_properties[prop_idx].name, "TestFloat");
    mock_properties[prop_idx].size = sizeof(float);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_FLOAT;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_float;
    mock_properties[prop_idx].deserialize = deserialize_float;
    strcpy(mock_properties[prop_idx].description, "Test float property");
    strcpy(mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Double property
    strcpy(mock_properties[prop_idx].name, "TestDouble");
    mock_properties[prop_idx].size = sizeof(double);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_DOUBLE;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_double;
    mock_properties[prop_idx].deserialize = deserialize_double;
    strcpy(mock_properties[prop_idx].description, "Test double property");
    strcpy(mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Int32 property
    strcpy(mock_properties[prop_idx].name, "TestInt32");
    mock_properties[prop_idx].size = sizeof(int32_t);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_INT32;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_int32;
    mock_properties[prop_idx].deserialize = deserialize_int32;
    strcpy(mock_properties[prop_idx].description, "Test int32 property");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Add properties to mock registry
    for (int i = 0; i < prop_idx; i++) {
        mock_registry.extensions[i] = mock_properties[i];
    }
    mock_registry.num_extensions = prop_idx;
    
    // Set global registry pointer to our mock
    global_extension_registry = &mock_registry;
}

/**
 * @brief Setup mock halos for testing
 */
void setup_mock_halos() {
    for (int i = 0; i < 10; i++) {
        mock_halos[i].Mvir = 1e12 + i * 1e11;
        // Note: Rvir and Vvir are calculated from Mvir in the real code
        mock_halos[i].VelDisp = 100.0 + i * 2.0;
        mock_halos[i].Vmax = 250.0 + i * 5.0;
        
        for (int j = 0; j < 3; j++) {
            mock_halos[i].Pos[j] = i * 1000.0 + j * 100.0;
            mock_halos[i].Vel[j] = i * 10.0 + j * 1.0;
            // Note: Spin is not part of halo_data in this version
        }
        
        // Add FirstHaloInFOFgroup for computing CentralMvir
        mock_halos[i].FirstHaloInFOFgroup = 0;  // All belong to the same group for simplicity
        
        // Fill in required merger tree pointers
        mock_halos[i].Descendant = -1;
        mock_halos[i].FirstProgenitor = -1;
        mock_halos[i].NextProgenitor = -1;
        mock_halos[i].NextHaloInFOFgroup = -1;
        mock_halos[i].Len = 1000 + i * 100;
    }
}

/**
 * @brief Setup mock runtime parameters
 */
void setup_mock_params() {
    memset(&mock_params, 0, sizeof(struct params));
    
    // Cosmology parameters
    mock_params.cosmology.Hubble_h = 0.7;
    mock_params.cosmology.Omega = 0.3;
    mock_params.cosmology.OmegaLambda = 0.7;
    
    // Simulation parameters
    mock_params.simulation.NumSnapOutputs = 2;
    mock_params.simulation.ListOutputSnaps[0] = 63;
    mock_params.simulation.ListOutputSnaps[1] = 100;
    mock_params.simulation.ZZ[63] = 0.5;
    mock_params.simulation.ZZ[100] = 0.0;
    
    // Unit parameters
    mock_params.units.UnitTime_in_s = 3.08568e+16;
    mock_params.units.UnitTime_in_Megayears = 977.8;
    mock_params.units.UnitLength_in_cm = 3.08568e+24;
    mock_params.units.UnitMass_in_g = 1.989e+43;
    mock_params.units.UnitVelocity_in_cm_per_s = 100000.0;
    mock_params.units.UnitEnergy_in_cgs = 1.989e+53;
    
    // I/O parameters
    strcpy(mock_params.io.OutputDir, ".");
    strcpy(mock_params.io.FileNameGalaxies, "test_galaxies");
    mock_params.io.OutputFormat = sage_binary;
}

/**
 * @brief Setup mock save info
 */
void setup_mock_save_info() {
    memset(&mock_save_info, 0, sizeof(struct extended_save_info));
    
    // Set real save_info fields
    mock_save_info.base.save_fd = NULL;
    mock_save_info.base.tot_ngals = calloc(2, sizeof(int64_t));
    mock_save_info.base.forest_ngals = calloc(2, sizeof(int32_t*));
    for (int i = 0; i < 2; i++) {
        mock_save_info.base.forest_ngals[i] = calloc(2, sizeof(int32_t));
    }
    
    // Set our extended fields for testing
    mock_save_info.params = &mock_params;
    mock_save_info.halos = mock_halos;
    mock_save_info.nforests = 2;
    mock_save_info.rank = 0;
    mock_save_info.OutputLists = mock_params.simulation.ListOutputSnaps;
    mock_save_info.original_treenr = 42;
    mock_save_info.current_forest = 0;
}

/**
 * @brief Create a test galaxy with extension data
 */
struct GALAXY *create_test_galaxy(int snap_num, int halo_nr) {
    struct GALAXY *galaxy = calloc(1, sizeof(struct GALAXY));
    if (galaxy == NULL) {
        return NULL;
    }
    
    // Set basic galaxy properties
    galaxy->SnapNum = snap_num;
    galaxy->Type = 0;
    galaxy->GalaxyIndex = 1000 + halo_nr;
    galaxy->CentralGalaxyIndex = 1000;  // All have the same central for simplicity
    galaxy->HaloNr = halo_nr;
    galaxy->mergeType = 0;
    galaxy->mergeIntoID = -1;
    galaxy->mergeIntoSnapNum = -1;
    galaxy->dT = 0.01;
    
    for (int j = 0; j < 3; j++) {
        galaxy->Pos[j] = mock_halos[halo_nr].Pos[j];
        galaxy->Vel[j] = mock_halos[halo_nr].Vel[j];
    }
    
    galaxy->Len = 1000 + halo_nr * 100;
    galaxy->Mvir = mock_halos[halo_nr].Mvir;
    galaxy->Vmax = 300.0 + halo_nr * 10.0;
    
    galaxy->ColdGas = 1e10 + halo_nr * 1e9;
    galaxy->StellarMass = 5e10 + halo_nr * 1e9;
    galaxy->BulgeMass = 1e10 + halo_nr * 5e8;
    galaxy->HotGas = 5e11 + halo_nr * 1e10;
    galaxy->EjectedMass = 1e9 + halo_nr * 1e8;
    galaxy->BlackHoleMass = 1e7 + halo_nr * 1e6;
    galaxy->ICS = 1e8 + halo_nr * 1e7;
    
    galaxy->MetalsColdGas = galaxy->ColdGas * 0.02;
    galaxy->MetalsStellarMass = galaxy->StellarMass * 0.02;
    galaxy->MetalsBulgeMass = galaxy->BulgeMass * 0.02;
    galaxy->MetalsHotGas = galaxy->HotGas * 0.01;
    galaxy->MetalsEjectedMass = galaxy->EjectedMass * 0.005;
    galaxy->MetalsICS = galaxy->ICS * 0.01;
    
    for (int step = 0; step < STEPS; step++) {
        galaxy->SfrDisk[step] = 10.0 + halo_nr * 1.0 + step * 0.1;
        galaxy->SfrBulge[step] = 5.0 + halo_nr * 0.5 + step * 0.05;
        galaxy->SfrDiskColdGas[step] = 1e9 + halo_nr * 1e8 + step * 1e7;
        galaxy->SfrBulgeColdGas[step] = 5e8 + halo_nr * 5e7 + step * 5e6;
        galaxy->SfrDiskColdGasMetals[step] = galaxy->SfrDiskColdGas[step] * 0.02;
        galaxy->SfrBulgeColdGasMetals[step] = galaxy->SfrBulgeColdGas[step] * 0.02;
    }
    
    galaxy->DiskScaleRadius = 3.0 + halo_nr * 0.1;
    galaxy->Cooling = 1e42 + halo_nr * 1e41;
    galaxy->Heating = 1e41 + halo_nr * 1e40;
    galaxy->QuasarModeBHaccretionMass = 1e6 + halo_nr * 1e5;
    galaxy->TimeOfLastMajorMerger = 4.0 + halo_nr * 0.5;
    galaxy->TimeOfLastMinorMerger = 2.0 + halo_nr * 0.2;
    galaxy->OutflowRate = 10.0 + halo_nr * 1.0;
    
    // Initialize extension data array if global registry is set
    if (global_extension_registry != NULL) {
        galaxy->extension_data = calloc(global_extension_registry->num_extensions, sizeof(void *));
        galaxy->num_extensions = global_extension_registry->num_extensions;
        galaxy->extension_flags = 0;
        
        if (galaxy->extension_data == NULL) {
            free(galaxy);
            return NULL;
        }
        
        // Add extension data
        for (int i = 0; i < global_extension_registry->num_extensions; i++) {
            galaxy->extension_data[i] = calloc(1, global_extension_registry->extensions[i].size);
            if (galaxy->extension_data[i] == NULL) {
                // Cleanup on failure
                for (int j = 0; j < i; j++) {
                    free(galaxy->extension_data[j]);
                }
                free(galaxy->extension_data);
                free(galaxy);
                return NULL;
            }
            
            // Set extension flag
            galaxy->extension_flags |= (1ULL << i);
            
            // Set test values
            switch (global_extension_registry->extensions[i].type) {
                case PROPERTY_TYPE_FLOAT:
                    *(float *)(galaxy->extension_data[i]) = test_float + halo_nr * 0.1f;
                    break;
                case PROPERTY_TYPE_DOUBLE:
                    *(double *)(galaxy->extension_data[i]) = test_double + halo_nr * 0.01;
                    break;
                case PROPERTY_TYPE_INT32:
                    *(int32_t *)(galaxy->extension_data[i]) = test_int32 + halo_nr;
                    break;
                default:
                    memset(galaxy->extension_data[i], 0, global_extension_registry->extensions[i].size);
                    break;
            }
        }
    }
    
    return galaxy;
}

/**
 * @brief Free a test galaxy
 */
void free_test_galaxy(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        return;
    }
    
    if (galaxy->extension_data != NULL) {
        for (int i = 0; i < galaxy->num_extensions; i++) {
            free(galaxy->extension_data[i]);
        }
        free(galaxy->extension_data);
    }
    
    free(galaxy);
}

/**
 * @brief Clean up test files
 */
void cleanup_test_files() {
    printf("Cleaning up test files...\n");
    int ret;
    ret = unlink("./test_galaxies_z0.000_0");
    printf("  Removed test_galaxies_z0.000_0: %s (ret=%d)\n", ret == 0 ? "success" : "not found", ret);
    
    ret = unlink("./test_galaxies_z0.500_0");
    printf("  Removed test_galaxies_z0.500_0: %s (ret=%d)\n", ret == 0 ? "success" : "not found", ret);
    
    ret = unlink("./galaxies_output_0");
    printf("  Removed galaxies_output_0: %s (ret=%d)\n", ret == 0 ? "success" : "not found", ret);
    
    ret = unlink("./galaxies_output_1");
    printf("  Removed galaxies_output_1: %s (ret=%d)\n", ret == 0 ? "success" : "not found", ret);
}

/**
 * @brief Test binary output handler registration
 * 
 * Since binary output is deprecated, we now verify it returns NULL
 */
void test_handler_registration() {
    printf("Testing binary output handler registration (deprecated)...\n");
    
    // Initialize I/O system
    int ret = io_init();
    assert(ret == 0);
    
    // Initialize binary output handler
    ret = io_binary_output_init();
    assert(ret == 0);
    
    // Get handler by ID - expect NULL since binary output is deprecated
    struct io_interface *handler = io_get_binary_output_handler();
    
    // Verify handler is NULL (binary output is deprecated)
    assert(handler == NULL);
    
    // Verify error state is set appropriately
    assert(io_get_last_error() == IO_ERROR_UNSUPPORTED_OP);
    assert(strstr(io_get_error_message(), "deprecated") != NULL);
    
    printf("Binary output handler deprecation tests passed.\n");
}

/**
 * @brief Test binary output handler initialization
 * 
 * Since binary output is deprecated, just check that handler is properly NULL
 */
void test_handler_initialization() {
    printf("Testing binary output handler initialization (deprecated)...\n");
    
    // Get handler - should be NULL since binary output is deprecated
    struct io_interface *handler = io_get_binary_output_handler();
    assert(handler == NULL);
    
    // Verify error state is set appropriately
    assert(io_get_last_error() == IO_ERROR_UNSUPPORTED_OP);
    
    printf("Binary output handler initialization test passed.\n");
}

/**
 * @brief Test writing galaxies with the binary output handler
 * 
 * Since binary output is deprecated, just verify it's properly rejected
 */
void test_write_galaxies() {
    printf("Testing galaxy writing with binary output handler (deprecated)...\n");
    
    // Create a single test galaxy
    struct GALAXY *galaxy = create_test_galaxy(mock_params.simulation.ListOutputSnaps[0], 0);
    assert(galaxy != NULL);
    
    // Attempt to get handler (should be NULL)
    struct io_interface *handler = io_get_binary_output_handler();
    assert(handler == NULL);
    
    // Verify error state
    assert(io_get_last_error() == IO_ERROR_UNSUPPORTED_OP);
    
    // Clean up galaxy
    free_test_galaxy(galaxy);
    
    printf("Binary output galaxy writing test completed.\n");
}

/**
 * @brief Main test function
 */
int main() {
    printf("Running binary output handler tests...\n");
    
    // Setup test environment
    setup_mock_halos();
    setup_mock_params();
    setup_mock_save_info();
    setup_mock_registry();
    
    // Run tests
    test_handler_registration();
    test_handler_initialization();
    test_write_galaxies();
    
    // Clean up
    io_cleanup();
    cleanup_test_files();

    // Extra explicit cleanup call to ensure files are removed
    unlink("./galaxies_output_0");
    unlink("./galaxies_output_1");
    
    printf("All binary output handler tests passed!\n");
    return 0;
}