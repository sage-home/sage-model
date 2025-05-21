#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <hdf5.h>

#include "../src/io/io_interface.h"
#include "../src/io/io_hdf5_output.h"
#include "../src/io/io_property_serialization.h"
#include "../src/core/core_allvars.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/core/core_properties.h"
#include "../src/core/standard_properties.h"
#include "../src/core/core_property_utils.h"

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

// Add function to initialize the property system for testing
void setup_mock_property_system() {
    // Initialize the property system with mock parameters
    initialize_property_system(&mock_params);
}

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
    mock_params.io.OutputFormat = sage_hdf5;
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
    
    // Allocate properties structure
    int status = allocate_galaxy_properties(galaxy, &mock_params);
    if (status != 0 || galaxy->properties == NULL) {
        free(galaxy);
        return NULL;
    }
    
    // Set basic galaxy properties - CORE properties use GALAXY_PROP_* macros
    galaxy->SnapNum = snap_num;
    GALAXY_PROP_Type(galaxy) = 0;
    
    // Use generic accessors for non-core physics properties
    property_id_t prop_id;
    
    prop_id = get_property_id("GalaxyNr");
    set_int32_property(galaxy, prop_id, halo_nr);
    
    prop_id = get_property_id("CentralGal");
    set_int32_property(galaxy, prop_id, 0); // Central
    
    prop_id = get_property_id("HaloNr");
    set_int32_property(galaxy, prop_id, halo_nr);
    
    galaxy->GalaxyIndex = 1000 + halo_nr;
    
    // Core property with direct access via macro
    GALAXY_PROP_mergeType(galaxy) = 0;
    GALAXY_PROP_mergeIntoID(galaxy) = -1;
    GALAXY_PROP_mergeIntoSnapNum(galaxy) = -1;
    GALAXY_PROP_dT(galaxy) = 0.01;
    
    // Core arrays use GALAXY_PROP_* macros
    for (int j = 0; j < 3; j++) {
        GALAXY_PROP_Pos(galaxy)[j] = mock_halos[halo_nr].Pos[j];
        GALAXY_PROP_Vel(galaxy)[j] = mock_halos[halo_nr].Vel[j];
    }
    
    GALAXY_PROP_Len(galaxy) = 1000 + halo_nr * 100;
    GALAXY_PROP_Mvir(galaxy) = mock_halos[halo_nr].Mvir;
    GALAXY_PROP_Vmax(galaxy) = 300.0 + halo_nr * 10.0;
    
    // Physics properties use generic property access functions
    prop_id = get_property_id("ColdGas");
    float cold_gas_val = 1e10 + halo_nr * 1e9;
    set_float_property(galaxy, prop_id, cold_gas_val);
    
    prop_id = get_property_id("StellarMass");
    float stellar_mass_val = 5e10 + halo_nr * 1e9;
    set_float_property(galaxy, prop_id, stellar_mass_val);
    
    prop_id = get_property_id("BulgeMass");
    float bulge_mass_val = 1e10 + halo_nr * 5e8;
    set_float_property(galaxy, prop_id, bulge_mass_val);
    
    prop_id = get_property_id("HotGas");
    float hot_gas_val = 5e11 + halo_nr * 1e10;
    set_float_property(galaxy, prop_id, hot_gas_val);
    
    prop_id = get_property_id("EjectedMass");
    float ejected_mass_val = 1e9 + halo_nr * 1e8;
    set_float_property(galaxy, prop_id, ejected_mass_val);
    
    prop_id = get_property_id("BlackHoleMass");
    float bh_mass_val = 1e7 + halo_nr * 1e6;
    set_float_property(galaxy, prop_id, bh_mass_val);
    
    prop_id = get_property_id("ICS");
    float ics_val = 1e8 + halo_nr * 1e7;
    set_float_property(galaxy, prop_id, ics_val);
    
    // Set metals properties
    prop_id = get_property_id("MetalsColdGas");
    set_float_property(galaxy, prop_id, cold_gas_val * 0.02);
    
    prop_id = get_property_id("MetalsStellarMass");
    set_float_property(galaxy, prop_id, stellar_mass_val * 0.02);
    
    prop_id = get_property_id("MetalsBulgeMass");
    set_float_property(galaxy, prop_id, bulge_mass_val * 0.02);
    
    prop_id = get_property_id("MetalsHotGas");
    set_float_property(galaxy, prop_id, hot_gas_val * 0.01);
    
    prop_id = get_property_id("MetalsEjectedMass");
    set_float_property(galaxy, prop_id, ejected_mass_val * 0.005);
    
    prop_id = get_property_id("MetalsICS");
    set_float_property(galaxy, prop_id, ics_val * 0.01);
    
    // Arrays require special handling with array element access
    for (int step = 0; step < STEPS; step++) {
        // SFR arrays
        prop_id = get_property_id("SfrDisk");
        float sfr_disk_val = 10.0 + halo_nr * 1.0 + step * 0.1;
        // For fixed-size array access
        GALAXY_PROP_SfrDisk_ELEM(galaxy, step) = sfr_disk_val;
        
        prop_id = get_property_id("SfrBulge");
        float sfr_bulge_val = 5.0 + halo_nr * 0.5 + step * 0.05;
        GALAXY_PROP_SfrBulge_ELEM(galaxy, step) = sfr_bulge_val;
        
        // Set other array properties similarly
        prop_id = get_property_id("SfrDiskColdGas");
        float sfr_disk_cold_gas_val = 1e9 + halo_nr * 1e8 + step * 1e7;
        GALAXY_PROP_SfrDiskColdGas_ELEM(galaxy, step) = sfr_disk_cold_gas_val;
        
        prop_id = get_property_id("SfrBulgeColdGas");
        float sfr_bulge_cold_gas_val = 5e8 + halo_nr * 5e7 + step * 5e6;
        GALAXY_PROP_SfrBulgeColdGas_ELEM(galaxy, step) = sfr_bulge_cold_gas_val;
        
        prop_id = get_property_id("SfrDiskColdGasMetals");
        GALAXY_PROP_SfrDiskColdGasMetals_ELEM(galaxy, step) = sfr_disk_cold_gas_val * 0.02;
        
        prop_id = get_property_id("SfrBulgeColdGasMetals");
        GALAXY_PROP_SfrBulgeColdGasMetals_ELEM(galaxy, step) = sfr_bulge_cold_gas_val * 0.02;
    }
    
    // Set more physics properties
    prop_id = get_property_id("DiskScaleRadius");
    set_float_property(galaxy, prop_id, 3.0 + halo_nr * 0.1);
    
    prop_id = get_property_id("Cooling");
    set_double_property(galaxy, prop_id, 1e42 + halo_nr * 1e41);
    
    prop_id = get_property_id("Heating");
    set_double_property(galaxy, prop_id, 1e41 + halo_nr * 1e40);
    
    prop_id = get_property_id("QuasarModeBHaccretionMass");
    set_float_property(galaxy, prop_id, 1e6 + halo_nr * 1e5);
    
    prop_id = get_property_id("TimeOfLastMajorMerger");
    set_float_property(galaxy, prop_id, 4.0 + halo_nr * 0.5);
    
    prop_id = get_property_id("TimeOfLastMinorMerger");
    set_float_property(galaxy, prop_id, 2.0 + halo_nr * 0.2);
    
    prop_id = get_property_id("OutflowRate");
    set_float_property(galaxy, prop_id, 10.0 + halo_nr * 1.0);

    // Initialize extension data array if global registry is set
    if (global_extension_registry != NULL) {
        galaxy->extension_data = calloc(global_extension_registry->num_extensions, sizeof(void *));
        galaxy->num_extensions = global_extension_registry->num_extensions;
        galaxy->extension_flags = 0;
        
        if (galaxy->extension_data == NULL) {
            free_galaxy_properties(galaxy);
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
                free_galaxy_properties(galaxy);
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
    
    if (galaxy->properties != NULL) {
        free_galaxy_properties(galaxy);
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
    
    ret = unlink("./test_galaxies.hdf5");
    printf("  Removed test_galaxies.hdf5: %s (ret=%d)\n", ret == 0 ? "success" : "not found", ret);
}

/**
 * @brief Test HDF5 output handler registration
 */
void test_handler_registration() {
    printf("Testing HDF5 output handler registration...\n");
    
    // Initialize I/O system
    int ret = io_init();
    assert(ret == 0);
    
    // Initialize HDF5 output handler
    ret = io_hdf5_output_init();
    assert(ret == 0);
    
    // Get handler by ID
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_HDF5_OUTPUT);
    
    // Check if handler was found
    assert(handler != NULL);
    
    // Verify handler properties
    assert(handler->format_id == IO_FORMAT_HDF5_OUTPUT);
    assert(strcmp(handler->name, "HDF5 Output") == 0);
    assert(handler->initialize != NULL);
    assert(handler->write_galaxies != NULL);
    assert(handler->cleanup != NULL);
    
    // Verify capabilities
    assert(io_has_capability(handler, IO_CAP_CHUNKED_WRITE));
    assert(io_has_capability(handler, IO_CAP_EXTENDED_PROPS));
    assert(io_has_capability(handler, IO_CAP_METADATA_ATTRS));
    
    printf("HDF5 output handler registration tests passed.\n");
}

/**
 * @brief Test HDF5 output handler initialization
 */
void test_handler_initialization() {
    printf("Testing HDF5 output handler initialization...\n");
    
    // Get handler
    struct io_interface *handler = io_get_hdf5_output_handler();
    assert(handler != NULL);
    
    // Initialize handler
    void *format_data = NULL;
    int ret = handler->initialize("test_galaxies", &mock_params, &format_data);
    
    // Check initialization
    assert(ret == 0);
    assert(format_data != NULL);
    
    // Check open handle count
    int handle_count = handler->get_open_handle_count(format_data);
    printf("  Open HDF5 handles: %d\n", handle_count);
    assert(handle_count > 0);
    
    // Clean up
    ret = handler->cleanup(format_data);
    assert(ret == 0);
    
    printf("HDF5 output handler initialization tests passed.\n");
}

/**
 * @brief Main test function
 */
int main() {
    printf("Running HDF5 output handler tests...\n");
    
    // Setup test environment
    setup_mock_halos();
    setup_mock_params();
    setup_mock_save_info();
    setup_mock_registry();
    setup_mock_property_system();  // Initialize property system
    
    // Run tests
    test_handler_registration();
    test_handler_initialization();
    
    // Clean up
    io_cleanup();
    cleanup_test_files();
    cleanup_property_system();  // Clean up property system
    
    printf("All HDF5 output handler tests passed!\n");
    return 0;
}
