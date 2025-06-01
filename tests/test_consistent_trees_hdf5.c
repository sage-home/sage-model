/**
 * Test suite for ConsistentTrees HDF5 I/O Handler
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
#include "../src/io/io_consistent_trees_hdf5.h"
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
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
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
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Format detection with various file types and edge cases
 */
static void test_format_detection(void) {
    printf("=== Testing ConsistentTrees HDF5 format detection ===\n");
    printf("NOTE: Currently using extension-based detection (stub implementation)\n");
    
    // Test with HDF5 extensions - should detect as ConsistentTrees HDF5 (stub behavior)
    const char *hdf5_files[] = {
        "test_ctrees.hdf5",
        "consistent_trees.h5",
        "tree_data.hdf5"
    };
    
    for (int i = 0; i < 3; i++) {
        bool result = io_is_consistent_trees_hdf5(hdf5_files[i]);
        printf("  Detection for %s: %s\n", hdf5_files[i], result ? "detected" : "not detected");
        TEST_ASSERT(result == true, "HDF5 extensions should be detected by stub implementation");
    }
    
    // Test with non-HDF5 extensions - should not be detected
    const char *non_hdf5_files[] = {
        "Makefile",                          // No extension
        "tests/test_consistent_trees_hdf5.c", // .c extension
        "test.txt",                          // .txt extension
        "data.bin"                           // .bin extension
    };
    
    for (int i = 0; i < 4; i++) {
        bool result = io_is_consistent_trees_hdf5(non_hdf5_files[i]);
        TEST_ASSERT(result == false, "Non-HDF5 extensions should not be detected");
    }
    
    // Test files without extensions
    const char *no_ext_files[] = {
        "/dev/null",
        "test_file",
        "."
    };
    
    for (int i = 0; i < 3; i++) {
        bool result = io_is_consistent_trees_hdf5(no_ext_files[i]);
        TEST_ASSERT(result == false, "Files without HDF5 extensions should not be detected");
    }
    
    printf("Format detection tests completed\n");
}

/**
 * Test: Comprehensive error handling with invalid inputs
 */
static void test_error_handling(void) {
    printf("\n=== Testing comprehensive error handling ===\n");
    
    // Test NULL parameter
    bool result = io_is_consistent_trees_hdf5(NULL);
    TEST_ASSERT(result == false, "io_is_consistent_trees_hdf5(NULL) should return false");
    
    // Test empty string
    result = io_is_consistent_trees_hdf5("");
    TEST_ASSERT(result == false, "io_is_consistent_trees_hdf5(\"\") should return false");
    
    // Test directory instead of file
    result = io_is_consistent_trees_hdf5(".");
    TEST_ASSERT(result == false, "Directory should not be detected as HDF5 file");
    
    // Test very long filename (edge case)
    char long_filename[1000];
    memset(long_filename, 'a', sizeof(long_filename) - 1);
    long_filename[sizeof(long_filename) - 1] = '\0';
    result = io_is_consistent_trees_hdf5(long_filename);
    TEST_ASSERT(result == false, "Very long non-existent filename should return false");
    
    // Test filename with special characters
    const char *special_filenames[] = {
        "file with spaces.hdf5",
        "file@#$%^&*().h5",
        "../../../etc/passwd",
        "file\nwith\nnewlines.hdf5"
    };
    
    for (int i = 0; i < 4; i++) {
        result = io_is_consistent_trees_hdf5(special_filenames[i]);
        TEST_ASSERT(result == false, "Files with special characters should be handled safely");
    }
    
    printf("Error handling tests completed\n");
}

/**
 * Test: Handler registration and metadata validation
 */
static void test_handler_registration(void) {
    printf("\n=== Testing ConsistentTrees HDF5 handler registration ===\n");
    
    // Get handler by ID
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_CONSISTENT_TREES_HDF5);
    
    // Check if handler was found
    TEST_ASSERT(handler != NULL, "ConsistentTrees HDF5 handler should be registered");
    
    if (handler == NULL) {
        printf("ERROR: Handler not found - skipping remaining handler tests\n");
        return;
    }
    
    // Verify handler properties
    TEST_ASSERT(handler->format_id == IO_FORMAT_CONSISTENT_TREES_HDF5, "Handler format_id should match expected value");
    TEST_ASSERT(strcmp(handler->name, "ConsistentTrees HDF5") == 0, "Handler name should be 'ConsistentTrees HDF5'");
    TEST_ASSERT(handler->version != NULL, "Handler version should not be NULL");
    TEST_ASSERT(strlen(handler->version) > 0, "Handler version should not be empty");
    
    // For HDF5 implementations, function pointers should be set appropriately
    TEST_ASSERT(handler->initialize != NULL, "HDF5 implementation should have non-NULL initialize function");
    TEST_ASSERT(handler->read_forest != NULL, "HDF5 implementation should have non-NULL read_forest function");
    TEST_ASSERT(handler->write_galaxies == NULL, "Input format should have NULL write_galaxies function");
    TEST_ASSERT(handler->cleanup != NULL, "HDF5 implementation should have non-NULL cleanup function");
    
    printf("Handler registration tests completed\n");
}

/**
 * Test: Capability validation for HDF5-specific features
 */
static void test_capability_validation(void) {
    printf("\n=== Testing capability validation ===\n");
    
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_CONSISTENT_TREES_HDF5);
    if (handler == NULL) {
        printf("ERROR: Handler not found - skipping capability tests\n");
        return;
    }
    
    // Verify HDF5-specific capabilities
    TEST_ASSERT(io_has_capability(handler, IO_CAP_RANDOM_ACCESS), "Handler should support random access");
    TEST_ASSERT(io_has_capability(handler, IO_CAP_MULTI_FILE), "Handler should support multi-file datasets");
    TEST_ASSERT(io_has_capability(handler, IO_CAP_METADATA_QUERY), "Handler should support metadata queries");
    TEST_ASSERT(io_has_capability(handler, IO_CAP_METADATA_ATTRS), "Handler should support metadata attributes");
    
    // Test capability checking with invalid capability
    TEST_ASSERT(!io_has_capability(handler, 0x80000000), "Invalid capability should return false");
    
    // Test multiple capability checks
    uint32_t combined_caps = IO_CAP_RANDOM_ACCESS | IO_CAP_METADATA_QUERY;
    TEST_ASSERT(io_has_capability(handler, combined_caps), "Handler should support combined capabilities");
    
    printf("Capability validation tests completed\n");
}

/**
 * Test: Resource management and cleanup verification
 */
static void test_resource_management(void) {
    printf("\n=== Testing resource management ===\n");
    
    // Test that multiple init/cleanup cycles work correctly
    for (int i = 0; i < 3; i++) {
        io_cleanup();
        int result = io_init();
        TEST_ASSERT(result == 0, "Multiple init cycles should succeed");
        
        struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_CONSISTENT_TREES_HDF5);
        TEST_ASSERT(handler != NULL, "Handler should be available after re-initialization");
    }
    
    printf("Resource management tests completed\n");
}

/**
 * Test: Integration with the broader I/O system
 */
static void test_integration(void) {
    printf("\n=== Testing I/O system integration ===\n");
    
    // Test that the handler can be found through the interface
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_CONSISTENT_TREES_HDF5);
    TEST_ASSERT(handler != NULL, "Handler should be accessible through I/O interface");
    
    // Test handler enumeration
    int handler_count = 0;
    for (int format_id = 0; format_id < 10; format_id++) {
        struct io_interface *h = io_get_handler_by_id(format_id);
        if (h != NULL) {
            handler_count++;
            if (h->format_id == IO_FORMAT_CONSISTENT_TREES_HDF5) {
                TEST_ASSERT(strcmp(h->name, "ConsistentTrees HDF5") == 0, "Handler should be properly enumerated");
            }
        }
    }
    
    TEST_ASSERT(handler_count > 0, "At least one handler should be registered");
    printf("Found %d registered handlers\n", handler_count);
    
    printf("Integration tests completed\n");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("Starting tests for test_consistent_trees_hdf5\n");
    printf("========================================\n\n");
    
    printf("This test verifies that the ConsistentTrees HDF5 I/O handler:\n");
    printf("  1. Correctly detects ConsistentTrees HDF5 files by format validation\n");
    printf("  2. Handles comprehensive error conditions gracefully\n");
    printf("  3. Registers properly with the I/O interface system\n");
    printf("  4. Manages resources correctly with proper cleanup\n");
    printf("  5. Supports appropriate HDF5-specific capabilities\n");
    printf("  6. Integrates properly with the broader I/O system\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_format_detection();
    test_error_handling();
    test_handler_registration();
    test_capability_validation();
    test_resource_management();
    test_integration();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_consistent_trees_hdf5:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}

#else

/**
 * Fallback main when HDF5 is not available
 */
int main(void) {
    printf("HDF5 support not compiled in - skipping ConsistentTrees HDF5 handler tests.\n");
    return 0;
}

#endif /* HDF5 */
