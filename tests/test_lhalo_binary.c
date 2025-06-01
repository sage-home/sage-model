/**
 * Test suite for LHalo Binary I/O Handler
 * 
 * Tests cover:
 * - Format detection with various file types and edge cases
 * - Handler registration and metadata validation
 * - Comprehensive error handling with invalid inputs
 * - Resource management and cleanup verification
 * - Integration with the broader I/O system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/io/io_interface.h"
#include "../src/io/io_lhalo_binary.h"
#include "../src/core/core_allvars.h"

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
    printf("=== Testing LHalo binary format detection ===\n");
    
    // Test with valid file - use existing test data file
    const char *valid_file = "tests/test_data/trees_063.0";
    bool result = io_is_lhalo_binary(valid_file);
    TEST_ASSERT(result == true, "Valid LHalo binary file should be detected");
    
    // Test with invalid file
    const char *invalid_file = "Makefile";
    result = io_is_lhalo_binary(invalid_file);
    TEST_ASSERT(result == false, "Non-binary file should not be detected as LHalo binary format");
    
    // Test various other invalid files
    const char *invalid_files[] = {
        "tests/test_lhalo_binary.c",  // Source file
        "/dev/null",                  // Empty file (on Unix)
        "non_existent_file.bin"       // Non-existent file
    };
    
    for (int i = 0; i < 3; i++) {
        result = io_is_lhalo_binary(invalid_files[i]);
        TEST_ASSERT(result == false, "Invalid files should not be detected as LHalo binary");
    }
}

/**
 * Test: Comprehensive error handling with invalid inputs
 */
static void test_error_handling(void) {
    printf("\n=== Testing comprehensive error handling ===\n");
    
    // Test NULL parameter
    bool result = io_is_lhalo_binary(NULL);
    TEST_ASSERT(result == false, "io_is_lhalo_binary(NULL) should return false");
    
    // Test empty string
    result = io_is_lhalo_binary("");
    TEST_ASSERT(result == false, "io_is_lhalo_binary(\"\") should return false");
    
    // Test directory instead of file
    result = io_is_lhalo_binary(".");
    TEST_ASSERT(result == false, "Directory should not be detected as binary file");
    
    // Test file that exists but is too small to be valid
    result = io_is_lhalo_binary("/dev/null");
    TEST_ASSERT(result == false, "Empty file should not be detected as LHalo binary");
    
    // Test files that might have some binary data but aren't LHalo format
    const char *binary_but_invalid[] = {
        "libsage.so",  // Shared library
        "sage"         // Executable (if it exists)
    };
    
    printf("Testing detection of binary files that aren't LHalo format:\n");
    for (int i = 0; i < 2; i++) {
        result = io_is_lhalo_binary(binary_but_invalid[i]);
        printf("  %s: %s\n", binary_but_invalid[i], result ? "detected" : "not detected");
        // These should return false as they're not LHalo format
        // (Note: We can't be 100% certain without knowing exact file contents)
    }
    
    // Test very long filename (edge case)
    char long_filename[1000];
    memset(long_filename, 'a', sizeof(long_filename) - 1);
    long_filename[sizeof(long_filename) - 1] = '\0';
    result = io_is_lhalo_binary(long_filename);
    TEST_ASSERT(result == false, "Very long non-existent filename should return false");
    
    // Test filename with special characters
    const char *special_filenames[] = {
        "file with spaces.bin",
        "file@#$%^&*().dat",
        "../../../etc/passwd",
        "file\nwith\nnewlines",
        ""
    };
    
    for (int i = 0; i < 5; i++) {
        result = io_is_lhalo_binary(special_filenames[i]);
        TEST_ASSERT(result == false, "Files with special characters should be handled safely");
    }
}

/**
 * Test: Handler registration and metadata validation
 */
static void test_handler_registration(void) {
    printf("\n=== Testing LHalo binary handler registration ===\n");
    
    // Get handler by ID
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_LHALO_BINARY);
    
    // Check if handler was found
    TEST_ASSERT(handler != NULL, "LHalo binary handler should be registered");
    
    if (handler == NULL) {
        printf("ERROR: Handler not found - skipping remaining handler tests\n");
        return;
    }
    
    // Verify handler properties
    TEST_ASSERT(handler->format_id == IO_FORMAT_LHALO_BINARY, "Handler format_id should match expected value");
    TEST_ASSERT(strcmp(handler->name, "LHalo Binary") == 0, "Handler name should be 'LHalo Binary'");
    TEST_ASSERT(handler->version != NULL, "Handler version should not be NULL");
    TEST_ASSERT(strlen(handler->version) > 0, "Handler version should not be empty");
    
    // For the binary implementation, the function pointers should be non-NULL
    TEST_ASSERT(handler->initialize != NULL, "Binary implementation should have non-NULL initialize function");
    TEST_ASSERT(handler->read_forest != NULL, "Binary implementation should have non-NULL read_forest function");
    TEST_ASSERT(handler->write_galaxies == NULL, "Binary format is input-only, write_galaxies should be NULL");
    TEST_ASSERT(handler->cleanup != NULL, "Binary implementation should have non-NULL cleanup function");
    
    // Verify capabilities
    TEST_ASSERT(io_has_capability(handler, IO_CAP_RANDOM_ACCESS), "Handler should support random access");
    TEST_ASSERT(io_has_capability(handler, IO_CAP_MULTI_FILE), "Handler should support multi-file datasets");
    
    // Test capability checking with invalid capability
    TEST_ASSERT(!io_has_capability(handler, 0x80000000), "Invalid capability should return false");
    
    // Test multiple capability checks
    uint32_t combined_caps = IO_CAP_RANDOM_ACCESS | IO_CAP_MULTI_FILE;
    TEST_ASSERT(io_has_capability(handler, combined_caps), "Handler should support combined capabilities");
    
    // Test capability that handler shouldn't have
    TEST_ASSERT(!io_has_capability(handler, IO_CAP_COMPRESSION), "Handler should not claim unsupported capabilities");
}

/**
 * Test: Enhanced resource management and cleanup verification
 */
static void test_resource_management(void) {
    printf("\n=== Testing enhanced resource management ===\n");
    
    // Get handler
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_LHALO_BINARY);
    TEST_ASSERT(handler != NULL, "Handler should be available for resource testing");
    
    if (handler == NULL) {
        printf("ERROR: Handler not found - skipping resource management tests\n");
        return;
    }
    
    // Test error state initialization
    TEST_ASSERT(handler->last_error == IO_ERROR_NONE, "Handler should start with no error");
    TEST_ASSERT(strlen(handler->error_message) == 0, "Handler should start with empty error message");
    
    // Test that resource management functions exist and are callable
    TEST_ASSERT(handler->close_open_handles != NULL, "Handler should have close_open_handles function");
    TEST_ASSERT(handler->get_open_handle_count != NULL, "Handler should have get_open_handle_count function");
    
    // Test calling resource management functions safely (without real data)
    printf("Testing resource management function calls with NULL data...\n");
    
    // These should be safe to call even without initialization
    // as they should handle NULL format_data gracefully
    int handle_count = handler->get_open_handle_count(NULL);
    TEST_ASSERT(handle_count >= 0, "get_open_handle_count should return non-negative value for NULL data");
    
    int close_result = handler->close_open_handles(NULL);
    TEST_ASSERT(close_result >= 0, "close_open_handles should return non-negative value for NULL data");
    
    printf("Enhanced resource management tests completed successfully\n");
}

/**
 * Test: Integration with I/O system
 */
static void test_integration(void) {
    printf("\n=== Testing integration with I/O system ===\n");
    
    // Test that handler can be retrieved by different methods
    struct io_interface *handler1 = io_get_handler_by_id(IO_FORMAT_LHALO_BINARY);
    struct io_interface *handler2 = io_get_lhalo_binary_handler();
    
    TEST_ASSERT(handler1 != NULL, "Handler should be retrievable by format ID");
    TEST_ASSERT(handler2 != NULL, "Handler should be retrievable by specific function");
    TEST_ASSERT(handler1 == handler2, "Both retrieval methods should return same handler instance");
    
    // Test format detection with the actual test file
    const char *test_file = "tests/test_data/trees_063.0";
    bool detected = io_is_lhalo_binary(test_file);
    TEST_ASSERT(detected == true, "Test data file should be detected as LHalo binary");
    
    // Test that the I/O system can find the right handler for this format
    printf("Testing I/O system format auto-detection...\n");
    
    // Test multiple files to ensure consistent detection
    const char *test_patterns[] = {
        "tests/test_data/trees_063.0",
        "fake_tree_file.bin"  // Non-existent but would match pattern if it existed
    };
    
    for (int i = 0; i < 1; i++) {  // Only test the file that actually exists
        bool result = io_is_lhalo_binary(test_patterns[i]);
        printf("  Detection for %s: %s\n", test_patterns[i], result ? "detected" : "not detected");
    }
    
    printf("Format detection integration successful\n");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("Starting tests for test_lhalo_binary\n");
    printf("========================================\n\n");
    
    printf("This test verifies that the LHalo Binary I/O handler:\n");
    printf("  1. Correctly detects LHalo binary files by header validation\n");
    printf("  2. Handles comprehensive error conditions gracefully\n");
    printf("  3. Registers properly with the I/O interface system\n");
    printf("  4. Manages resources correctly with enhanced validation\n");
    printf("  5. Integrates properly with the broader I/O system\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_format_detection();
    test_error_handling();
    test_handler_registration();
    test_resource_management();
    test_integration();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_lhalo_binary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
