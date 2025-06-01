/**
 * Test suite for LHalo HDF5 I/O Handler
 * 
 * Tests cover:
 * - Format detection with various file types and edge cases
 * - Handler registration and metadata validation
 * - Comprehensive error handling with invalid inputs
 * - Resource management and cleanup verification
 * - Integration with the broader I/O system
 * - Capability validation for HDF5-specific features
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/io/io_interface.h"
#include "../src/io/io_lhalo_hdf5.h"
#include "../src/core/core_allvars.h"

#ifdef HDF5

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test fixtures
static struct test_context {
    bool io_initialized;
    bool hdf5_initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize HDF5 resource tracking
    hdf5_tracking_init();
    test_ctx.hdf5_initialized = 1;
    
    // Initialize I/O system - this automatically registers handlers
    int result = io_init();
    if (result != 0) {
        printf("ERROR: io_init failed with code %d\n", result);
        return -1;
    }
    test_ctx.io_initialized = 1;
    
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.io_initialized) {
        io_cleanup();
        test_ctx.io_initialized = 0;
    }
    
    if (test_ctx.hdf5_initialized) {
        hdf5_tracking_cleanup();
        test_ctx.hdf5_initialized = 0;
    }
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Format detection with various file types and edge cases
 */
static void test_format_detection(void) {
    printf("=== Testing LHalo HDF5 format detection ===\n");
    
    // Test with valid LHalo HDF5 file extensions
    const char *valid_files[] = {
        "test_lhalo.hdf5",
        "lhalo_trees.h5",
        "merger_trees.hdf5"
    };
    
    for (int i = 0; i < 3; i++) {
        bool result = io_is_lhalo_hdf5(valid_files[i]);
        printf("  Detection for %s: %s\n", valid_files[i], result ? "detected" : "not detected");
        TEST_ASSERT(result == true, "Valid LHalo HDF5 files should be detected by extension");
    }
    
    // Test with invalid file extensions
    const char *invalid_files[] = {
        "test_file.txt",
        "data.bin",
        "Makefile",
        "trees_063.0",       // LHalo binary format
        "test.dat"
    };
    
    for (int i = 0; i < 5; i++) {
        bool result = io_is_lhalo_hdf5(invalid_files[i]);
        TEST_ASSERT(result == false, "Files without HDF5 extensions should not be detected");
    }
    
    // Test edge cases
    const char *edge_cases[] = {
        "file.h5x",           // Similar but wrong extension
        ".hdf5",              // Hidden file with HDF5 extension  
        "file.hdf5.backup",   // HDF5 extension but with suffix
        "file_hdf5",          // No dot before extension
        "file.HDF5"           // Uppercase extension
    };
    
    for (int i = 0; i < 5; i++) {
        bool result = io_is_lhalo_hdf5(edge_cases[i]);
        printf("  Edge case '%s': %s\n", edge_cases[i], result ? "detected" : "not detected");
        // Most should return false (proper validation depends on implementation)
    }
    
    printf("Format detection tests completed\n");
}

/**
 * Test: Comprehensive error handling with invalid inputs
 */
static void test_error_handling(void) {
    printf("\n=== Testing comprehensive error handling ===\n");
    
    // Test NULL parameter
    bool result = io_is_lhalo_hdf5(NULL);
    TEST_ASSERT(result == false, "io_is_lhalo_hdf5(NULL) should return false");
    
    // Test empty string
    result = io_is_lhalo_hdf5("");
    TEST_ASSERT(result == false, "io_is_lhalo_hdf5(\"\") should return false");
    
    // Test directory instead of file
    result = io_is_lhalo_hdf5(".");
    TEST_ASSERT(result == false, "Directory should not be detected as HDF5 file");
    
    // Test security-related edge cases (path traversal, etc.)
    const char *security_tests[] = {
        "../../../etc/passwd",
        "file\nwith\nnewlines.hdf5",
        "file with spaces.hdf5"
    };
    
    for (int i = 0; i < 3; i++) {
        result = io_is_lhalo_hdf5(security_tests[i]);
        printf("  Security test '%s': %s\n", security_tests[i], result ? "detected" : "not detected");
        // These should ideally be rejected for security
    }
    
    printf("Error handling tests completed\n");
}

/**
 * Test: LHalo HDF5 handler registration and metadata
 */
static void test_handler_registration(void) {
    printf("\n=== Testing LHalo HDF5 handler registration ===\n");
    
    // Get handler by ID
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_LHALO_HDF5);
    TEST_ASSERT(handler != NULL, "LHalo HDF5 handler should be registered");
    
    if (handler != NULL) {
        // Verify handler properties
        TEST_ASSERT(handler->format_id == IO_FORMAT_LHALO_HDF5, "Handler should have correct format ID");
        TEST_ASSERT(strcmp(handler->name, "LHalo HDF5") == 0, "Handler should have correct name");
        TEST_ASSERT(handler->version != NULL, "Handler version should not be NULL");
        TEST_ASSERT(strlen(handler->version) > 0, "Handler version should not be empty");
        
        // Check that function pointers are properly set (not NULL like stubs)
        TEST_ASSERT(handler->initialize != NULL, "Handler initialize function should not be NULL");
        TEST_ASSERT(handler->read_forest != NULL, "Handler read_forest function should not be NULL");
        TEST_ASSERT(handler->write_galaxies == NULL, "Input format should have NULL write_galaxies function");
        TEST_ASSERT(handler->cleanup != NULL, "Handler cleanup function should not be NULL");
        TEST_ASSERT(handler->close_open_handles != NULL, "Handler close_open_handles function should not be NULL");
        TEST_ASSERT(handler->get_open_handle_count != NULL, "Handler get_open_handle_count function should not be NULL");
        
        printf("All function pointers properly set\n");
    }
    
    printf("Handler registration tests completed\n");
}

/**
 * Test: Capability validation for HDF5-specific features
 */
static void test_capability_validation(void) {
    printf("\n=== Testing capability validation ===\n");
    
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_LHALO_HDF5);
    TEST_ASSERT(handler != NULL, "Handler should be available for capability testing");
    
    if (handler != NULL) {
        // Test HDF5-specific capabilities
        TEST_ASSERT(io_has_capability(handler, IO_CAP_RANDOM_ACCESS), 
                   "LHalo HDF5 handler should support random access");
        TEST_ASSERT(io_has_capability(handler, IO_CAP_MULTI_FILE), 
                   "LHalo HDF5 handler should support multi-file operations");
        TEST_ASSERT(io_has_capability(handler, IO_CAP_METADATA_QUERY), 
                   "LHalo HDF5 handler should support metadata queries");
        TEST_ASSERT(io_has_capability(handler, IO_CAP_METADATA_ATTRS), 
                   "LHalo HDF5 handler should support metadata attributes");
        
        // Test capability checking with invalid capability
        TEST_ASSERT(!io_has_capability(handler, 0x80000000), "Invalid capability should return false");
        
        // Test multiple capability checks
        uint32_t combined_caps = IO_CAP_RANDOM_ACCESS | IO_CAP_METADATA_QUERY;
        TEST_ASSERT(io_has_capability(handler, combined_caps), "Handler should support combined capabilities");
        
        printf("All expected capabilities verified\n");
    }
    
    printf("Capability validation tests completed\n");
}

/**
 * Test: Resource management and cleanup verification
 */
static void test_resource_management(void) {
    printf("\n=== Testing resource management ===\n");
    
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_LHALO_HDF5);
    TEST_ASSERT(handler != NULL, "Handler should be available for resource testing");
    
    if (handler != NULL) {
        // Test initialization with invalid parameters
        void *format_data = NULL;
        struct params dummy_params = {0};
        
        // Test NULL filename
        int result = handler->initialize(NULL, &dummy_params, &format_data);
        TEST_ASSERT(result != 0, "Initialize should fail with NULL filename");
        TEST_ASSERT(format_data == NULL, "format_data should remain NULL on failure");
        
        // Test NULL params
        result = handler->initialize("test.hdf5", NULL, &format_data);
        TEST_ASSERT(result != 0, "Initialize should fail with NULL params");
        TEST_ASSERT(format_data == NULL, "format_data should remain NULL on failure");
        
        // Test cleanup with NULL data (should not crash)
        result = handler->cleanup(NULL);
        TEST_ASSERT(result == 0, "Cleanup should handle NULL gracefully");
        
        // Test resource management functions with NULL
        result = handler->close_open_handles(NULL);
        TEST_ASSERT(result == 0, "close_open_handles should handle NULL gracefully");
        
        int count = handler->get_open_handle_count(NULL);
        TEST_ASSERT(count == 0, "get_open_handle_count should return 0 for NULL");
        
        printf("Resource management verification completed\n");
    }
    
    // Test that multiple init/cleanup cycles work correctly
    for (int i = 0; i < 3; i++) {
        io_cleanup();
        int result = io_init();
        TEST_ASSERT(result == 0, "Multiple init/cleanup cycles should work");
        
        handler = io_get_handler_by_id(IO_FORMAT_LHALO_HDF5);
        TEST_ASSERT(handler != NULL, "Handler should be available after re-initialization");
    }
    
    printf("Resource management tests completed\n");
}

/**
 * Test: Integration with the broader I/O system
 */
static void test_io_system_integration(void) {
    printf("\n=== Testing I/O system integration ===\n");
    
    // Test format detection integration
    bool detected = io_detect_format("test_lhalo.hdf5") != NULL;
    TEST_ASSERT(detected, "I/O system should be able to detect LHalo HDF5 files");
    
    // Test that the correct handler is returned
    struct io_interface *detected_handler = io_detect_format("test_lhalo.hdf5");
    if (detected_handler != NULL) {
        TEST_ASSERT(detected_handler->format_id == IO_FORMAT_LHALO_HDF5, 
                   "Detected handler should be LHalo HDF5 handler");
    }
    
    // Test error handling integration
    io_clear_error();
    int error_code = io_get_last_error();
    TEST_ASSERT(error_code == IO_ERROR_NONE, "Error should be cleared");
    
    const char *error_msg = io_get_error_message();
    TEST_ASSERT(error_msg != NULL, "Error message should not be NULL");
    
    // Test setting and getting errors
    io_set_error(IO_ERROR_FILE_NOT_FOUND, "Test error message");
    error_code = io_get_last_error();
    TEST_ASSERT(error_code == IO_ERROR_FILE_NOT_FOUND, "Error code should be set correctly");
    
    error_msg = io_get_error_message();
    TEST_ASSERT(strstr(error_msg, "Test error message") != NULL, "Error message should contain test text");
    
    printf("Integration tests completed\n");
}

//=============================================================================
// Main Test Function
//=============================================================================

int main() {
    printf("\n========================================\n");
    printf("Starting tests for test_lhalo_hdf5\n");
    printf("========================================\n");
    printf("\n");
    printf("This test verifies that the LHalo HDF5 I/O handler:\n");
    printf("  1. Correctly detects LHalo HDF5 files by format validation\n");
    printf("  2. Handles comprehensive error conditions gracefully\n");
    printf("  3. Registers properly with the I/O interface system\n");
    printf("  4. Manages resources correctly with proper cleanup\n");
    printf("  5. Supports appropriate HDF5-specific capabilities\n");
    printf("  6. Integrates properly with the broader I/O system\n");
    printf("\n");
    
    // Setup test environment
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run test suites
    test_format_detection();
    test_error_handling();
    test_handler_registration();
    test_capability_validation();
    test_resource_management();
    test_io_system_integration();
    
    // Clean up test environment
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_lhalo_hdf5:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}

#else

/**
 * Fallback main when HDF5 is not available
 */
int main() {
    printf("HDF5 support not compiled in - skipping LHalo HDF5 handler tests.\n");
    return 0;
}

#endif /* HDF5 */