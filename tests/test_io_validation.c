#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/io/io_validation.h"
#include "../src/io/io_interface.h"
#include "../src/core/core_allvars.h"

// Mock I/O handler for testing capability validation
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

// Test galaxy validation
int test_galaxy_validation() {
    struct validation_context ctx;
    int status;
    
    printf("Testing galaxy validation...\n");
    
    // Initialize
    status = validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    assert(status == 0);
    
    // Create a valid galaxy
    struct GALAXY galaxies[3];
    memset(galaxies, 0, sizeof(galaxies));
    
    // Set up a valid galaxy (index 0)
    galaxies[0].StellarMass = 1e10;
    galaxies[0].BulgeMass = 5e9;
    galaxies[0].ColdGas = 2e9;
    galaxies[0].HotGas = 8e9;
    galaxies[0].mergeIntoID = -1;
    
    // Set up an invalid galaxy (index 1) - NaN value
    galaxies[1].StellarMass = NAN;
    galaxies[1].BulgeMass = 1e8;
    galaxies[1].ColdGas = 5e8;
    galaxies[1].HotGas = 2e9;
    galaxies[1].mergeIntoID = -1;
    
    // Set up an invalid galaxy (index 2) - Negative mass
    galaxies[2].StellarMass = 1e9;
    galaxies[2].BulgeMass = -5e8;  // Negative mass
    galaxies[2].ColdGas = 1e9;
    galaxies[2].HotGas = 3e9;
    galaxies[2].mergeIntoID = 10;  // Invalid reference
    
    // Validate just galaxy data (should catch the NaN)
    status = validation_check_galaxies(&ctx, galaxies, 3, "TestGalaxies",
                                     VALIDATION_CHECK_GALAXY_DATA);
    assert(status != 0);  // Should return non-zero if errors found
    assert(ctx.error_count > 0);
    
    validation_reset(&ctx);
    
    // Validate just galaxy references (should catch the invalid mergeIntoID)
    status = validation_check_galaxies(&ctx, galaxies, 3, "TestGalaxies",
                                     VALIDATION_CHECK_GALAXY_REFS);
    assert(status != 0);  // Should return non-zero if errors found
    assert(ctx.error_count > 0);
    
    validation_reset(&ctx);
    
    // Validate with consistency checks (should catch multiple issues)
    status = validation_check_galaxies(&ctx, galaxies, 3, "TestGalaxies",
                                     VALIDATION_CHECK_CONSISTENCY);
    assert(status != 0);  // Should return non-zero if errors found
    assert(ctx.error_count > 0);
    
    validation_reset(&ctx);
    
    // Fix the galaxies
    galaxies[1].StellarMass = 1e8;
    galaxies[2].BulgeMass = 5e8;
    galaxies[2].mergeIntoID = -1;
    
    // Validate again
    status = validation_check_galaxies(&ctx, galaxies, 3, "TestGalaxies",
                                     VALIDATION_CHECK_CONSISTENCY);
    assert(status == 0);  // Should pass
    assert(ctx.error_count == 0);
    
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

int main() {
    int status = 0;
    
    printf("Running I/O validation tests...\n");
    
    status |= test_context_init();
    status |= test_result_collection();
    status |= test_strictness_levels();
    status |= test_validation_utilities();
    status |= test_condition_validation();
    status |= test_galaxy_validation();
    status |= test_assertion_status();
    
    if (status == 0) {
        printf("All I/O validation tests passed!\n");
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Some tests failed\n");
        return EXIT_FAILURE;
    }
}
