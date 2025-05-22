#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/io/io_validation.h"
#include "../src/io/io_interface.h"
#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h" 
#include "../src/core/core_property_utils.h"

// Mock I/O handlers for testing format validation
struct io_interface mock_handler = {
    .name = "Mock Handler",
    .version = "1.0",
    .format_id = 999,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_MULTI_FILE,
    .initialize = NULL,
    .read_forest = NULL,
    .write_galaxies = NULL,
    .cleanup = NULL,
    .close_open_handles = NULL,
    .get_open_handle_count = NULL
};

struct io_interface hdf5_handler = {
    .name = "HDF5 Format Handler",
    .version = "1.0",
    .format_id = 7, // IO_FORMAT_HDF5_OUTPUT,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_EXTENDED_PROPS | IO_CAP_METADATA_ATTRS,
    .initialize = NULL,
    .read_forest = NULL,
    .write_galaxies = NULL,
    .cleanup = NULL,
    .close_open_handles = NULL,
    .get_open_handle_count = NULL
};

// Test context initialization and configuration
int test_context_init() {
    struct validation_context ctx;
    int status;
    
    printf("Testing context initialization...\n");
    
    // Initialize with default strictness
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    assert(ctx.strictness == VALIDATION_STRICTNESS_NORMAL);
    assert(ctx.num_results == 0);
    assert(ctx.error_count == 0);
    assert(ctx.warning_count == 0);
    
    // Clean up
    validation_cleanup(&ctx);
    
    // Initialize with strict mode
    status = validation_init(&ctx, VALIDATION_STRICTNESS_STRICT);
    assert(status == 0);
    assert(ctx.strictness == VALIDATION_STRICTNESS_STRICT);
    
    // Configure
    validation_configure(&ctx, VALIDATION_STRICTNESS_RELAXED, 20, 1);
    assert(ctx.strictness == VALIDATION_STRICTNESS_RELAXED);
    assert(ctx.max_results == 20);
    assert(ctx.abort_on_first_error == true);
    
    // Reset
    validation_reset(&ctx);
    assert(ctx.num_results == 0);
    assert(ctx.error_count == 0);
    assert(ctx.warning_count == 0);
    assert(ctx.strictness == VALIDATION_STRICTNESS_RELAXED);  // Configuration preserved
    
    printf("Context initialization tests passed\n");
    return 0;
}

// Test result collection and reporting
int test_result_collection() {
    struct validation_context ctx;
    int status;
    
    printf("Testing result collection...\n");
    
    // Initialize
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    
    // Add various results
    validation_add_result(&ctx, VALIDATION_ERROR_NULL_POINTER, VALIDATION_SEVERITY_ERROR,
                        VALIDATION_CHECK_CONSISTENCY, "TestComponent", 
                        __FILE__, __LINE__, "Test error message");
    
    validation_add_result(&ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_SEVERITY_WARNING,
                        VALIDATION_CHECK_GALAXY_DATA, "TestComponent", 
                        __FILE__, __LINE__, "Test warning message");
    
    validation_add_result(&ctx, VALIDATION_SUCCESS, VALIDATION_SEVERITY_INFO,
                        VALIDATION_CHECK_GALAXY_DATA, "TestComponent", 
                        __FILE__, __LINE__, "Test info message");
    
    // Check counts
    assert(ctx.num_results == 3);
    assert(ctx.error_count == 1);
    assert(ctx.warning_count == 1);
    assert(validation_get_result_count(&ctx) == 3);
    assert(validation_get_error_count(&ctx) == 1);
    assert(validation_get_warning_count(&ctx) == 1);
    assert(validation_has_errors(&ctx) == true);
    assert(validation_has_warnings(&ctx) == true);
    assert(validation_passed(&ctx) == false);
    
    // Report results
    status = validation_report(&ctx);
    assert(status == 1);  // 1 error
    
    // Reset
    validation_reset(&ctx);
    assert(ctx.num_results == 0);
    assert(ctx.error_count == 0);
    assert(ctx.warning_count == 0);
    
    printf("Result collection tests passed\n");
    return 0;
}

// Test strictness levels
int test_strictness_levels() {
    struct validation_context ctx;
    int status;
    
    printf("Testing strictness levels...\n");
    
    // Test relaxed mode (warnings ignored)
    status = validation_init(&ctx, VALIDATION_STRICTNESS_RELAXED);
    assert(status == 0);
    
    validation_add_result(&ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_SEVERITY_WARNING,
                        VALIDATION_CHECK_GALAXY_DATA, "TestComponent", 
                        __FILE__, __LINE__, "Warning in relaxed mode");
    
    assert(ctx.num_results == 0);  // Warning ignored in relaxed mode
    assert(ctx.warning_count == 0);
    
    // Add an error (should still be recorded)
    validation_add_result(&ctx, VALIDATION_ERROR_NULL_POINTER, VALIDATION_SEVERITY_ERROR,
                        VALIDATION_CHECK_CONSISTENCY, "TestComponent", 
                        __FILE__, __LINE__, "Error in relaxed mode");
    
    assert(ctx.num_results == 1);
    assert(ctx.error_count == 1);
    
    validation_reset(&ctx);
    
    // Test strict mode (warnings become errors)
    validation_configure(&ctx, VALIDATION_STRICTNESS_STRICT, -1, -1);
    
    validation_add_result(&ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_SEVERITY_WARNING,
                        VALIDATION_CHECK_GALAXY_DATA, "TestComponent", 
                        __FILE__, __LINE__, "Warning in strict mode");
    
    assert(ctx.num_results == 1);
    assert(ctx.error_count == 1);  // Warning became error
    assert(ctx.warning_count == 0);
    
    validation_reset(&ctx);
    
    // Test normal mode
    validation_configure(&ctx, VALIDATION_STRICTNESS_NORMAL, -1, -1);
    
    validation_add_result(&ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_SEVERITY_WARNING,
                        VALIDATION_CHECK_GALAXY_DATA, "TestComponent", 
                        __FILE__, __LINE__, "Warning in normal mode");
    
    assert(ctx.num_results == 1);
    assert(ctx.warning_count == 1);
    assert(ctx.error_count == 0);
    
    printf("Strictness level tests passed\n");
    return 0;
}

// Test basic validation utilities
int test_validation_utilities() {
    struct validation_context ctx;
    int status;
    
    printf("Testing validation utilities...\n");
    
    // Initialize
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    
    // Test NULL pointer validation
    void *test_ptr = NULL;
    status = validation_check_not_null(&ctx, test_ptr, "TestComponent", 
                                     __FILE__, __LINE__, "Test pointer is NULL");
    assert(status != 0);  // Should return non-zero for NULL pointer
    assert(ctx.error_count == 1);
    
    test_ptr = &ctx;  // Valid pointer
    status = validation_check_not_null(&ctx, test_ptr, "TestComponent", 
                                     __FILE__, __LINE__, "Test pointer is valid");
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 1);  // No new errors
    
    validation_reset(&ctx);
    
    // Test finite validation
    double test_value = NAN;
    status = validation_check_finite(&ctx, test_value, "TestComponent", 
                                   __FILE__, __LINE__, "Test value is NaN");
    assert(status != 0);  // Should return non-zero for NaN
    assert(ctx.error_count == 1);
    
    test_value = INFINITY;
    status = validation_check_finite(&ctx, test_value, "TestComponent", 
                                   __FILE__, __LINE__, "Test value is Infinity");
    assert(status != 0);  // Should return non-zero for Infinity
    assert(ctx.error_count == 2);
    
    test_value = 3.14;  // Valid value
    status = validation_check_finite(&ctx, test_value, "TestComponent", 
                                   __FILE__, __LINE__, "Test value is finite");
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 2);  // No new errors
    
    validation_reset(&ctx);
    
    // Test bounds validation
    int64_t test_index = -1;
    status = validation_check_bounds(&ctx, test_index, 0, 10, "TestComponent", 
                                   __FILE__, __LINE__, "Test index is negative");
    assert(status != 0);  // Should return non-zero for out-of-bounds
    assert(ctx.error_count == 1);
    
    test_index = 15;
    status = validation_check_bounds(&ctx, test_index, 0, 10, "TestComponent", 
                                   __FILE__, __LINE__, "Test index is too large");
    assert(status != 0);  // Should return non-zero for out-of-bounds
    assert(ctx.error_count == 2);
    
    test_index = 5;  // Valid index
    status = validation_check_bounds(&ctx, test_index, 0, 10, "TestComponent", 
                                   __FILE__, __LINE__, "Test index is valid");
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 2);  // No new errors
    
    validation_reset(&ctx);
    
    // Test capability validation
    status = validation_check_capability(&ctx, &mock_handler, IO_CAP_RANDOM_ACCESS, 
                                       "TestComponent", __FILE__, __LINE__, 
                                       "Format should support random access");
    assert(status == 0);  // Should pass (handler has this capability)
    
    status = validation_check_capability(&ctx, &mock_handler, IO_CAP_COMPRESSION, 
                                       "TestComponent", __FILE__, __LINE__, 
                                       "Format should support compression");
    assert(status != 0);  // Should return non-zero for missing capability
    assert(ctx.error_count == 1);
    
    validation_reset(&ctx);
    
    printf("Validation utilities tests passed\n");
    return 0;
}

// Test condition validation with different severities
int test_condition_validation() {
    struct validation_context ctx;
    int status;
    
    printf("Testing condition validation...\n");
    
    // Initialize
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    
    // Test with failed condition (warning)
    status = validation_check_condition(&ctx, false, VALIDATION_SEVERITY_WARNING,
                                      VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                      VALIDATION_CHECK_CONSISTENCY,
                                      "TestComponent", __FILE__, __LINE__,
                                      "Test warning condition");
    assert(status == 0);  // Warnings should return 0
    assert(ctx.warning_count == 1);
    assert(ctx.error_count == 0);
    
    // Test with failed condition (error)
    status = validation_check_condition(&ctx, false, VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_ERROR_DATA_INCONSISTENT,
                                      VALIDATION_CHECK_GALAXY_DATA,
                                      "TestComponent", __FILE__, __LINE__,
                                      "Test error condition");
    assert(status != 0);  // Errors should return non-zero
    assert(ctx.warning_count == 1);
    assert(ctx.error_count == 1);
    
    // Configure to abort on first error
    validation_reset(&ctx);
    validation_configure(&ctx, -1, -1, 1);  // abort_on_first_error = true
    
    // Test with failed condition (error)
    status = validation_check_condition(&ctx, false, VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_ERROR_DATA_INCONSISTENT,
                                      VALIDATION_CHECK_GALAXY_DATA,
                                      "TestComponent", __FILE__, __LINE__,
                                      "Test error condition with abort");
    assert(status != 0);  // Should abort
    assert(ctx.error_count == 1);
    
    validation_reset(&ctx);
    
    // Test with successful condition
    status = validation_check_condition(&ctx, true, VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_ERROR_DATA_INCONSISTENT,
                                      VALIDATION_CHECK_GALAXY_DATA,
                                      "TestComponent", __FILE__, __LINE__,
                                      "Test successful condition");
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 0);
    
    printf("Condition validation tests passed\n");
    return 0;
}

// Function to initialize galaxy properties
void initialize_test_galaxies(struct GALAXY *galaxies, int ngals) {
    for (int i = 0; i < ngals; i++) {
        // Initialize properties structure
        allocate_galaxy_properties(&galaxies[i], NULL);
    }
}

// Function to free galaxy properties
void free_test_galaxies(struct GALAXY *galaxies, int ngals) {
    for (int i = 0; i < ngals; i++) {
        if (galaxies[i].properties != NULL) {
            free_galaxy_properties(&galaxies[i]);
        }
    }
}

// Test galaxy validation
int test_galaxy_validation() {
    struct validation_context ctx;
    int status;
    property_id_t prop_id;
    
    printf("Testing galaxy validation...\n");
    
    // Initialize
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    
    // Create a valid galaxy
    struct GALAXY galaxies[4];
    memset(galaxies, 0, sizeof(galaxies));
    
    // Initialize properties for all galaxies
    initialize_test_galaxies(galaxies, 4);
    
    // Set up a valid galaxy (index 0)
    GALAXY_PROP_Type(&galaxies[0]) = 0;  // Central - core property
    
    // Set physics properties using generic accessor functions
    prop_id = get_cached_property_id("StellarMass");
    set_float_property(&galaxies[0], prop_id, 1e10);
    
    prop_id = get_cached_property_id("BulgeMass");
    set_float_property(&galaxies[0], prop_id, 5e9);
    
    prop_id = get_cached_property_id("ColdGas");
    set_float_property(&galaxies[0], prop_id, 2e9);
    
    prop_id = get_cached_property_id("HotGas");
    set_float_property(&galaxies[0], prop_id, 8e9);
    
    prop_id = get_cached_property_id("EjectedMass");
    set_float_property(&galaxies[0], prop_id, 1e9);
    
    prop_id = get_cached_property_id("BlackHoleMass");
    set_float_property(&galaxies[0], prop_id, 1e7);
    
    prop_id = get_cached_property_id("MetalsStellarMass");
    set_float_property(&galaxies[0], prop_id, 1e8);
    
    prop_id = get_cached_property_id("MetalsBulgeMass");
    set_float_property(&galaxies[0], prop_id, 5e7);
    
    prop_id = get_cached_property_id("MetalsColdGas");
    set_float_property(&galaxies[0], prop_id, 1e7);
    
    prop_id = get_cached_property_id("MetalsHotGas");
    set_float_property(&galaxies[0], prop_id, 4e7);
    
    // Core properties use GALAXY_PROP_* macros
    GALAXY_PROP_mergeIntoID(&galaxies[0]) = -1;
    GALAXY_PROP_CentralGal(&galaxies[0]) = 0;
    GALAXY_PROP_GalaxyNr(&galaxies[0]) = 0;
    GALAXY_PROP_HaloNr(&galaxies[0]) = 100;
    GALAXY_PROP_mergeType(&galaxies[0]) = 0;
    
    // Set position and velocity (core properties)
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos(&galaxies[0])[i] = i * 100.0;
        GALAXY_PROP_Vel(&galaxies[0])[i] = i * 200.0;
    }
    
    // Set up a galaxy with invalid data values (index 1)
    GALAXY_PROP_Type(&galaxies[1]) = 1;  // Satellite
    
    // Set physics properties with invalid values
    prop_id = get_cached_property_id("StellarMass");
    set_float_property(&galaxies[1], prop_id, NAN);  // NaN value
    
    prop_id = get_cached_property_id("BulgeMass");
    set_float_property(&galaxies[1], prop_id, 1e8);
    
    prop_id = get_cached_property_id("ColdGas");
    set_float_property(&galaxies[1], prop_id, 5e8);
    
    prop_id = get_cached_property_id("HotGas");
    set_float_property(&galaxies[1], prop_id, 2e9);
    
    // Invalid position (core property)
    GALAXY_PROP_Pos(&galaxies[1])[0] = INFINITY;  // Invalid position
    GALAXY_PROP_mergeIntoID(&galaxies[1]) = -1;
    GALAXY_PROP_CentralGal(&galaxies[1]) = 0;
    GALAXY_PROP_GalaxyNr(&galaxies[1]) = 1;
    
    // Set up a galaxy with invalid references (index 2)
    GALAXY_PROP_Type(&galaxies[2]) = 5;  // Invalid type (should be 0-2)
    
    prop_id = get_cached_property_id("StellarMass");
    set_float_property(&galaxies[2], prop_id, 1e9);
    
    prop_id = get_cached_property_id("BulgeMass");
    set_float_property(&galaxies[2], prop_id, 5e8);
    
    prop_id = get_cached_property_id("ColdGas");
    set_float_property(&galaxies[2], prop_id, 1e9);
    
    prop_id = get_cached_property_id("HotGas");
    set_float_property(&galaxies[2], prop_id, 3e9);
    
    GALAXY_PROP_mergeIntoID(&galaxies[2]) = 10;  // Invalid reference
    GALAXY_PROP_CentralGal(&galaxies[2]) = 5;    // Invalid reference (out of bounds)
    GALAXY_PROP_GalaxyNr(&galaxies[2]) = 2;
    
    // Set up a galaxy with inconsistent data (index 3)
    GALAXY_PROP_Type(&galaxies[3]) = 2;  // Orphan
    
    // Set inconsistent mass values
    prop_id = get_cached_property_id("StellarMass");
    set_float_property(&galaxies[3], prop_id, 1e9);
    
    prop_id = get_cached_property_id("BulgeMass");
    set_float_property(&galaxies[3], prop_id, 2e9);   // BulgeMass > StellarMass (inconsistent)
    
    prop_id = get_cached_property_id("ColdGas");
    set_float_property(&galaxies[3], prop_id, 1e9);
    
    prop_id = get_cached_property_id("HotGas");
    set_float_property(&galaxies[3], prop_id, 3e9);
    
    prop_id = get_cached_property_id("MetalsStellarMass");
    set_float_property(&galaxies[3], prop_id, 2e9);  // Metals > Mass (inconsistent)
    
    GALAXY_PROP_mergeIntoID(&galaxies[3]) = -1;
    GALAXY_PROP_CentralGal(&galaxies[3]) = 0;
    GALAXY_PROP_GalaxyNr(&galaxies[3]) = 3;
    
    // Validate just galaxy data (should catch the NaN and Infinity)
    status = validation_check_galaxies(&ctx, galaxies, 4, "TestGalaxies",
                                     VALIDATION_CHECK_GALAXY_DATA);
    assert(status != 0);  // Should return non-zero if errors found
    assert(ctx.error_count > 0);
    int data_errors = ctx.error_count;
    printf("  Found %d errors in galaxy data validation\n", data_errors);
    validation_reset(&ctx);
    
    // Validate just galaxy references (should catch the invalid references)
    status = validation_check_galaxies(&ctx, galaxies, 4, "TestGalaxies",
                                     VALIDATION_CHECK_GALAXY_REFS);
    assert(status != 0);  // Should return non-zero if errors found
    assert(ctx.error_count > 0);
    int ref_errors = ctx.error_count;
    printf("  Found %d errors in galaxy reference validation\n", ref_errors);
    validation_reset(&ctx);
    
    // Validate with consistency checks (should catch multiple issues)
    status = validation_check_galaxies(&ctx, galaxies, 4, "TestGalaxies",
                                     VALIDATION_CHECK_CONSISTENCY);
    assert(status != 0);  // Should return non-zero if errors found
    assert(ctx.error_count > 0);
    int consistency_errors = ctx.error_count;
    printf("  Found %d errors in galaxy consistency validation\n", consistency_errors);
    assert(consistency_errors >= data_errors + ref_errors);  // Should catch at least all the previous errors
    validation_reset(&ctx);
    
    // Fix the galaxies
    prop_id = get_cached_property_id("StellarMass");
    set_float_property(&galaxies[1], prop_id, 1e8); // Fix NaN
    
    GALAXY_PROP_Pos(&galaxies[1])[0] = 100.0; // Fix infinity
    GALAXY_PROP_Type(&galaxies[2]) = 1; // Fix invalid type
    GALAXY_PROP_mergeIntoID(&galaxies[2]) = -1; // Fix invalid reference
    GALAXY_PROP_CentralGal(&galaxies[2]) = 0; // Fix invalid reference
    
    prop_id = get_cached_property_id("BulgeMass");
    set_float_property(&galaxies[3], prop_id, 5e8); // Fix BulgeMass > StellarMass
    
    prop_id = get_cached_property_id("MetalsStellarMass");
    set_float_property(&galaxies[3], prop_id, 1e8); // Fix Metals > Mass
    
    // Validate again with all checks
    status = validation_check_galaxies(&ctx, galaxies, 4, "TestGalaxies",
                                     VALIDATION_CHECK_CONSISTENCY);
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 0);
    printf("  All errors fixed, validation passes\n");
    
    // Free galaxy properties
    free_test_galaxies(galaxies, 4);
    
    printf("Galaxy validation tests passed\n");
    return 0;
}

// Test assertion status checks
int test_assertion_status() {
    struct validation_context ctx;
    int status;
    
    printf("Testing assertion status checks...\n");
    
    // Initialize
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    
    // Test condition validation with error severity - should return non-zero on failure
    status = validation_check_condition(&ctx, true, VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                      VALIDATION_CHECK_CONSISTENCY,
                                      "TestComponent", __FILE__, __LINE__,
                                      "This condition should pass");
    assert(status == 0);  // Should pass
    
    status = validation_check_condition(&ctx, false, VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                      VALIDATION_CHECK_CONSISTENCY,
                                      "TestComponent", __FILE__, __LINE__,
                                      "This condition should fail");
    assert(status != 0);  // Should fail
    
    printf("Assertion status checks passed\n");
    return 0;
}

// Test format validation - HDF5 only version (binary format removed from codebase)
int test_format_validation() {
    struct validation_context ctx;
    int status;
    
    printf("Testing format validation (HDF5 only)...\n");
    
    // Initialize
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    
    // Test format capabilities validation
    enum io_capabilities required_caps[] = {
        IO_CAP_RANDOM_ACCESS,
        IO_CAP_MULTI_FILE
    };
    
    // Test with all capabilities present
    status = validation_check_format_capabilities(&ctx, &mock_handler, 
                                               required_caps, 2, 
                                               "TestComponent", __FILE__, __LINE__,
                                               "test_operation");
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 0);
    
    // Test with missing capability
    validation_reset(&ctx);
    enum io_capabilities missing_caps[] = {
        IO_CAP_RANDOM_ACCESS,
        IO_CAP_COMPRESSION  // Mock handler doesn't have this
    };
    
    // This should add an error for the missing capability
    status = validation_check_format_capabilities(&ctx, &mock_handler, 
                                               missing_caps, 2, 
                                               "TestComponent", __FILE__, __LINE__,
                                               "test_operation");
    if (status == 0) {
        printf("WARNING: Expected validation_check_format_capabilities to return non-zero status\n");
    }
    // Check that the missing capability resulted in an error
    assert(ctx.error_count > 0);
    
    validation_reset(&ctx);
    
    // Test HDF5 format compatibility
    status = validation_check_hdf5_compatibility(&ctx, &hdf5_handler, 
                                             "TestComponent", __FILE__, __LINE__);
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 0);
    
    // Test with non-HDF5 format (using mock handler which isn't HDF5)
    validation_reset(&ctx);
    status = validation_check_hdf5_compatibility(&ctx, &mock_handler, 
                                             "TestComponent", __FILE__, __LINE__);
    // Should detect that mock format is not HDF5
    if (status == 0) {
        printf("WARNING: Expected validation_check_hdf5_compatibility to return non-zero status\n");
    }
    assert(ctx.error_count > 0);  // Should produce an error
    
    validation_reset(&ctx);
    
    // Test convenience macros
    status = VALIDATE_FORMAT_CAPABILITIES(&ctx, &mock_handler, required_caps, 2, 
                                       "TestComponent", "test_operation");
    assert(status == 0);  // Should pass
    
    validation_reset(&ctx);
    
    status = VALIDATE_HDF5_COMPATIBILITY(&ctx, &hdf5_handler, "TestComponent");
    assert(status == 0);  // Should pass
    
    printf("Format validation tests passed\n");
    return 0;
}

// Initialize mock parameters for the property system
struct params mock_params;

// Function to initialize mock parameters
void setup_mock_params() {
    memset(&mock_params, 0, sizeof(struct params));
    
    // Set basic parameters that exist in current struct
    mock_params.simulation.NumSnapOutputs = 10;
}

// Function to initialize property system for testing
void my_initialize_property_system() {
    // Initialize mock parameters
    setup_mock_params();
    
    // Initialize the real property system with mock parameters
    initialize_property_system(&mock_params);
}

// Function to clean up property system
void my_cleanup_property_system() {
    // Call the real cleanup function
    cleanup_property_system();
}

int main() {
    int status = 0;
    
    printf("Running I/O validation tests...\n");
    
    // Initialize property system
    my_initialize_property_system();
    
    status |= test_context_init();
    status |= test_result_collection();
    status |= test_strictness_levels();
    status |= test_validation_utilities();
    status |= test_condition_validation();
    status |= test_galaxy_validation();
    status |= test_assertion_status();
    status |= test_format_validation();
    
    // Clean up property system
    my_cleanup_property_system();
    
    if (status == 0) {
        printf("All I/O validation tests passed!\n");
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Some tests failed\n");
        return EXIT_FAILURE;
    }
}
