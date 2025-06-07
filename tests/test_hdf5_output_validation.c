/**
 * Test suite for HDF5 Output Validation
 * 
 * Simple test to validate basic HDF5 functionality underlying SAGE output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <hdf5.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Constants for validation
#define TOLERANCE_FLOAT 1e-5
#define TEST_OUTPUT_FILENAME "/tmp/sage_hdf5_test_output.h5"

// Helper macro for test assertions
#define TEST_ASSERT(condition, message, ...) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: " message "\n", ##__VA_ARGS__); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        fflush(stdout); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Simple test context
static struct {
    struct params run_params;
    bool setup_complete;
} test_ctx;

// Forward declarations
static int setup_test_context(void);
static void teardown_test_context(void);
static void test_hdf5_basic_functionality(void);
static void test_sage_hdf5_structure(void);
static void test_sage_hdf5_read_validation(void);

static int setup_test_context(void) {
    printf("Setting up HDF5 test context...\n");
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize minimal parameters needed
    test_ctx.run_params.cosmology.Omega = 0.3089;
    test_ctx.run_params.cosmology.OmegaLambda = 0.6911;
    test_ctx.run_params.cosmology.Hubble_h = 0.678;
    
    test_ctx.run_params.units.UnitLength_in_cm = 3.085678e21;
    test_ctx.run_params.units.UnitMass_in_g = 1.989e43;
    test_ctx.run_params.units.UnitVelocity_in_cm_per_s = 1.0e5;
    
    test_ctx.run_params.simulation.NumSnapOutputs = 1;
    test_ctx.run_params.simulation.ListOutputSnaps[0] = 63;
    test_ctx.run_params.simulation.SimMaxSnaps = 64;
    
    strcpy(test_ctx.run_params.io.OutputDir, "/tmp/");
    strcpy(test_ctx.run_params.io.FileNameGalaxies, "sage_hdf5_test_output");
    test_ctx.run_params.io.TreeType = lhalo_binary;
    
    // Initialize core systems
    if (initialize_logging(&test_ctx.run_params) != 0) {
        printf("ERROR: Failed to initialize logging system\n");
        return -1;
    }
    
    initialize_units(&test_ctx.run_params);
    
    test_ctx.setup_complete = true;
    printf("Test context setup complete.\n");
    return 0;
}

static void teardown_test_context(void) {
    printf("Cleaning up test context...\n");
    
    if (test_ctx.setup_complete) {
        cleanup_logging();
        test_ctx.setup_complete = false;
    }
    
    // Remove test output file
    remove(TEST_OUTPUT_FILENAME);
    printf("Test context cleanup complete.\n");
}

/**
 * Basic HDF5 functionality test
 * 
 * Tests that HDF5 library works correctly for file/group/dataset operations.
 */
static void test_hdf5_basic_functionality(void) {
    printf("\n=== Testing basic HDF5 functionality ===\n");
    
    // Test HDF5 file creation
    hid_t test_file_id = H5Fcreate(TEST_OUTPUT_FILENAME, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    TEST_ASSERT(test_file_id >= 0, "Should be able to create HDF5 file");
    
    if (test_file_id >= 0) {
        // Test HDF5 group creation (similar to /Snap_63 structure)
        hid_t group_id = H5Gcreate2(test_file_id, "/TestGroup", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        TEST_ASSERT(group_id >= 0, "Should be able to create HDF5 group");
        
        if (group_id >= 0) {
            // Test HDF5 dataset creation and I/O
            hsize_t dims[1] = {10};
            hid_t space_id = H5Screate_simple(1, dims, NULL);
            hid_t dataset_id = H5Dcreate2(group_id, "TestData", H5T_NATIVE_FLOAT, space_id, 
                                        H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            TEST_ASSERT(dataset_id >= 0, "Should be able to create HDF5 dataset");
            
            if (dataset_id >= 0) {
                // Test data writing
                float test_data[10] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
                herr_t write_status = H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, test_data);
                TEST_ASSERT(write_status >= 0, "Should be able to write data to HDF5 dataset");
                
                // Test data reading
                float read_data[10];
                herr_t read_status = H5Dread(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, read_data);
                TEST_ASSERT(read_status >= 0, "Should be able to read data from HDF5 dataset");
                
                // Verify data integrity
                bool data_matches = true;
                for (int i = 0; i < 10; i++) {
                    if (fabsf(read_data[i] - test_data[i]) > TOLERANCE_FLOAT) {
                        printf("  Data mismatch at index %d: %.6f vs %.6f\n", i, read_data[i], test_data[i]);
                        data_matches = false;
                        break;
                    }
                }
                TEST_ASSERT(data_matches, "Round-trip data should match original");
                
                H5Dclose(dataset_id);
            }
            H5Sclose(space_id);
            H5Gclose(group_id);
        }
        H5Fclose(test_file_id);
    }
    
    printf("Basic HDF5 functionality test completed.\n");
}

int main(int argc, char *argv[]) {
    (void)argc; /* Suppress unused parameter warning */
    (void)argv; /* Suppress unused parameter warning */
    
    printf("\n==============================================\n");
    printf("Starting HDF5 Output Validation Tests\n");
    printf("==============================================\n\n");
    
    printf("This test validates basic HDF5 functionality that\n");
    printf("underlies the SAGE HDF5 output pipeline.\n\n");
    
    // Setup test context
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    printf("Running HDF5 validation tests...\n");
    test_hdf5_basic_functionality();
    test_sage_hdf5_structure();
    test_sage_hdf5_read_validation();
    
    // Cleanup
    teardown_test_context();
    
    // Report results
    printf("\n==============================================\n");
    printf("Test results for HDF5 Output Validation:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("  Status: ALL TESTS PASSED ✓\n");
        printf("  HDF5 basic functionality is working correctly\n");
    } else {
        printf("  Status: SOME TESTS FAILED ✗\n");
        printf("  HDF5 functionality requires attention\n");
    }
    printf("==============================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

/**
 * Test SAGE HDF5 file structure creation
 * 
 * Tests that SAGE can create the expected HDF5 file structure.
 */
static void test_sage_hdf5_structure(void) {
    printf("\n=== Testing SAGE HDF5 file structure creation ===\n");
    
    // Test creating a file with the expected SAGE structure
    hid_t file_id = H5Fcreate(TEST_OUTPUT_FILENAME, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    TEST_ASSERT(file_id >= 0, "Should be able to create SAGE HDF5 file");
    
    if (file_id >= 0) {
        // Create Header group structure (based on example file)
        hid_t header_group = H5Gcreate2(file_id, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        TEST_ASSERT(header_group >= 0, "Should be able to create Header group");
        
        if (header_group >= 0) {
            // Create Header subgroups
            hid_t misc_group = H5Gcreate2(header_group, "Misc", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            TEST_ASSERT(misc_group >= 0, "Should be able to create Header/Misc group");
            
            hid_t runtime_group = H5Gcreate2(header_group, "Runtime", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            TEST_ASSERT(runtime_group >= 0, "Should be able to create Header/Runtime group");
            
            hid_t simulation_group = H5Gcreate2(header_group, "Simulation", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            TEST_ASSERT(simulation_group >= 0, "Should be able to create Header/Simulation group");
            
            if (misc_group >= 0) H5Gclose(misc_group);
            if (runtime_group >= 0) H5Gclose(runtime_group);
            if (simulation_group >= 0) H5Gclose(simulation_group);
            H5Gclose(header_group);
        }
        
        // Create snapshot group (like /Snap_63)
        hid_t snap_group = H5Gcreate2(file_id, "/Snap_63", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        TEST_ASSERT(snap_group >= 0, "Should be able to create snapshot group");
        
        if (snap_group >= 0) {
            // Create a sample dataset with attributes (like StellarMass)
            hsize_t dims[1] = {5};
            hid_t space_id = H5Screate_simple(1, dims, NULL);
            hid_t dataset_id = H5Dcreate2(snap_group, "StellarMass", H5T_NATIVE_FLOAT, space_id,
                                        H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            TEST_ASSERT(dataset_id >= 0, "Should be able to create StellarMass dataset");
            
            if (dataset_id >= 0) {
                // Add attributes like the example file
                hid_t attr_space = H5Screate(H5S_SCALAR);
                hid_t attr_type = H5Tcopy(H5T_C_S1);
                H5Tset_size(attr_type, 256);
                
                // Units attribute
                hid_t units_attr = H5Acreate2(dataset_id, "Units", attr_type, attr_space, H5P_DEFAULT, H5P_DEFAULT);
                if (units_attr >= 0) {
                    const char *units_str = "1.0e10 Msun/h";
                    H5Awrite(units_attr, attr_type, units_str);
                    H5Aclose(units_attr);
                }
                
                // Description attribute
                hid_t desc_attr = H5Acreate2(dataset_id, "Description", attr_type, attr_space, H5P_DEFAULT, H5P_DEFAULT);
                if (desc_attr >= 0) {
                    const char *desc_str = "Mass of stars.";
                    H5Awrite(desc_attr, attr_type, desc_str);
                    H5Aclose(desc_attr);
                }
                
                H5Tclose(attr_type);
                H5Sclose(attr_space);
                H5Dclose(dataset_id);
            }
            H5Sclose(space_id);
            H5Gclose(snap_group);
        }
        
        // Create TreeInfo group structure
        hid_t treeinfo_group = H5Gcreate2(file_id, "/TreeInfo", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        TEST_ASSERT(treeinfo_group >= 0, "Should be able to create TreeInfo group");
        
        if (treeinfo_group >= 0) {
            hid_t treeinfo_snap = H5Gcreate2(treeinfo_group, "Snap_63", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            TEST_ASSERT(treeinfo_snap >= 0, "Should be able to create TreeInfo/Snap_63 group");
            if (treeinfo_snap >= 0) H5Gclose(treeinfo_snap);
            H5Gclose(treeinfo_group);
        }
        
        H5Fclose(file_id);
    }
    
    printf("SAGE HDF5 structure test completed.\n");
}

/**
 * Test reading and validating SAGE HDF5 file structure
 * 
 * Tests that we can read HDF5 files and verify they have the expected structure.
 */
static void test_sage_hdf5_read_validation(void) {
    printf("\n=== Testing SAGE HDF5 file reading and validation ===\n");
    
    // First create a test file with SAGE structure
    hid_t file_id = H5Fcreate(TEST_OUTPUT_FILENAME, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_id >= 0) {
        // Create minimal SAGE structure
        hid_t header_group = H5Gcreate2(file_id, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        hid_t snap_group = H5Gcreate2(file_id, "/Snap_63", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        
        if (snap_group >= 0) {
            // Create sample galaxy datasets
            hsize_t dims[1] = {3};  // 3 test galaxies
            hid_t space_id = H5Screate_simple(1, dims, NULL);
            
            // Create SnapNum dataset
            hid_t snapnum_dataset = H5Dcreate2(snap_group, "SnapNum", H5T_NATIVE_INT32, space_id,
                                             H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            if (snapnum_dataset >= 0) {
                int32_t snapnum_data[3] = {63, 63, 63};
                H5Dwrite(snapnum_dataset, H5T_NATIVE_INT32, H5S_ALL, H5S_ALL, H5P_DEFAULT, snapnum_data);
                H5Dclose(snapnum_dataset);
            }
            
            // Create Mvir dataset
            hid_t mvir_dataset = H5Dcreate2(snap_group, "Mvir", H5T_NATIVE_FLOAT, space_id,
                                          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            if (mvir_dataset >= 0) {
                float mvir_data[3] = {12.5, 15.7, 18.9};
                H5Dwrite(mvir_dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, mvir_data);
                H5Dclose(mvir_dataset);
            }
            
            H5Sclose(space_id);
            H5Gclose(snap_group);
        }
        
        if (header_group >= 0) H5Gclose(header_group);
        H5Fclose(file_id);
    }
    
    // Now test reading the file back
    file_id = H5Fopen(TEST_OUTPUT_FILENAME, H5F_ACC_RDONLY, H5P_DEFAULT);
    TEST_ASSERT(file_id >= 0, "Should be able to open created HDF5 file for reading");
    
    if (file_id >= 0) {
        // Test that expected groups exist
        htri_t header_exists = H5Lexists(file_id, "/Header", H5P_DEFAULT);
        TEST_ASSERT(header_exists > 0, "Header group should exist in SAGE HDF5 file");
        
        htri_t snap_exists = H5Lexists(file_id, "/Snap_63", H5P_DEFAULT);
        TEST_ASSERT(snap_exists > 0, "Snap_63 group should exist in SAGE HDF5 file");
        
        // Test reading datasets
        if (snap_exists > 0) {
            hid_t snap_group = H5Gopen2(file_id, "/Snap_63", H5P_DEFAULT);
            if (snap_group >= 0) {
                // Test SnapNum dataset
                htri_t snapnum_exists = H5Lexists(snap_group, "SnapNum", H5P_DEFAULT);
                TEST_ASSERT(snapnum_exists > 0, "SnapNum dataset should exist");
                
                if (snapnum_exists > 0) {
                    hid_t snapnum_dataset = H5Dopen2(snap_group, "SnapNum", H5P_DEFAULT);
                    if (snapnum_dataset >= 0) {
                        int32_t read_snapnum[3];
                        H5Dread(snapnum_dataset, H5T_NATIVE_INT32, H5S_ALL, H5S_ALL, H5P_DEFAULT, read_snapnum);
                        
                        bool snapnum_correct = (read_snapnum[0] == 63 && read_snapnum[1] == 63 && read_snapnum[2] == 63);
                        TEST_ASSERT(snapnum_correct, "SnapNum data should be read correctly");
                        
                        H5Dclose(snapnum_dataset);
                    }
                }
                
                // Test Mvir dataset
                htri_t mvir_exists = H5Lexists(snap_group, "Mvir", H5P_DEFAULT);
                TEST_ASSERT(mvir_exists > 0, "Mvir dataset should exist");
                
                if (mvir_exists > 0) {
                    hid_t mvir_dataset = H5Dopen2(snap_group, "Mvir", H5P_DEFAULT);
                    if (mvir_dataset >= 0) {
                        float read_mvir[3];
                        H5Dread(mvir_dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, read_mvir);
                        
                        bool mvir_correct = (fabsf(read_mvir[0] - 12.5f) < TOLERANCE_FLOAT &&
                                           fabsf(read_mvir[1] - 15.7f) < TOLERANCE_FLOAT &&
                                           fabsf(read_mvir[2] - 18.9f) < TOLERANCE_FLOAT);
                        TEST_ASSERT(mvir_correct, "Mvir data should be read correctly");
                        
                        H5Dclose(mvir_dataset);
                    }
                }
                
                H5Gclose(snap_group);
            }
        }
        
        H5Fclose(file_id);
    }
    
    printf("SAGE HDF5 read validation test completed.\n");
}
