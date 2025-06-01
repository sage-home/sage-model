/**
 * Test suite for Memory Mapping I/O Infrastructure
 * 
 * Tests cover:
 * - Cross-platform memory mapping availability
 * - File mapping and data access functionality  
 * - Error handling and resource management
 * - Edge cases and invalid inputs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#include "../src/io/io_memory_map.h"

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

// Test constants
#define TEST_FILE "test_mmap_file.dat"
#define TEST_LARGE_FILE "test_large_mmap_file.dat"
#define TEST_ZERO_FILE "test_zero_mmap_file.dat"
#define TEST_SIZE 1024
#define TEST_LARGE_SIZE 4096
#define TEST_PATTERN 0xAB

// Test fixtures
static struct test_context {
    bool files_created;
    int test_fd;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    test_ctx.test_fd = -1;
    test_ctx.files_created = false;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    // Clean up any remaining test files
    unlink(TEST_FILE);
    unlink(TEST_LARGE_FILE);
    unlink(TEST_ZERO_FILE);
    
    // Close any open file descriptors
    if (test_ctx.test_fd >= 0) {
        close(test_ctx.test_fd);
        test_ctx.test_fd = -1;
    }
    
    test_ctx.files_created = false;
}

/**
 * Create a test file with the specified pattern
 * Returns 0 on success, -1 on failure
 */
static int create_test_file(const char* filename, size_t size, char pattern) {
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        printf("Failed to create test file %s\n", filename);
        return -1;
    }
    
    if (size > 0) {
        char* buffer = malloc(size);
        if (buffer == NULL) {
            fclose(fp);
            printf("Failed to allocate buffer for test file\n");
            return -1;
        }
        
        memset(buffer, pattern, size);
        
        if (fwrite(buffer, 1, size, fp) != size) {
            free(buffer);
            fclose(fp);
            printf("Failed to write test file data\n");
            return -1;
        }
        
        free(buffer);
    }
    
    fclose(fp);
    return 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Memory mapping availability
 */
static void test_mmap_availability(void) {
    printf("=== Testing memory mapping availability ===\n");
    
    bool available = mmap_is_available();
    printf("Memory mapping availability: %s\n", available ? "Yes" : "No");
    
    TEST_ASSERT(available == true, "Memory mapping should be available on modern systems");
}

/**
 * Test: Default options creation
 */
static void test_default_options(void) {
    printf("\n=== Testing default options creation ===\n");
    
    struct mmap_options options = mmap_default_options();
    
    TEST_ASSERT(options.mode == MMAP_READ_ONLY, "Default mode should be MMAP_READ_ONLY");
    TEST_ASSERT(options.mapping_size == 0, "Default mapping_size should be 0 (map entire file)");
    TEST_ASSERT(options.offset == 0, "Default offset should be 0");
}

/**
 * Test: Basic memory mapping functionality
 */
static void test_basic_mapping(void) {
    printf("\n=== Testing basic memory mapping functionality ===\n");
    
    // Create test file
    int result = create_test_file(TEST_FILE, TEST_SIZE, TEST_PATTERN);
    TEST_ASSERT(result == 0, "Test file creation should succeed");
    
    if (result != 0) {
        return; // Skip rest of test if file creation failed
    }
    
    // Create mapping options
    struct mmap_options options = mmap_default_options();
    
    // Map the file
    struct mmap_region* region = mmap_file(TEST_FILE, -1, &options);
    TEST_ASSERT(region != NULL, "File mapping should succeed");
    
    if (region == NULL) {
        printf("Mapping error: %s\n", mmap_get_error());
        unlink(TEST_FILE);
        return;
    }
    
    // Get pointer to mapped memory
    void* data = mmap_get_pointer(region);
    TEST_ASSERT(data != NULL, "Mapped memory pointer should be valid");
    
    // Get mapped size
    size_t size = mmap_get_size(region);
    TEST_ASSERT(size == TEST_SIZE, "Mapped size should match file size");
    
    // Verify file content
    if (data != NULL) {
        unsigned char* buffer = (unsigned char*)data;
        bool data_valid = true;
        for (size_t i = 0; i < TEST_SIZE; i++) {
            if (buffer[i] != TEST_PATTERN) {
                data_valid = false;
                break;
            }
        }
        TEST_ASSERT(data_valid, "Mapped data should match file content");
    }
    
    // Unmap the file
    int unmap_result = mmap_unmap(region);
    TEST_ASSERT(unmap_result == 0, "File unmapping should succeed");
    
    // Clean up
    unlink(TEST_FILE);
}

/**
 * Test: Mapping with file descriptor
 */
static void test_fd_mapping(void) {
    printf("\n=== Testing file descriptor mapping ===\n");
    
    // Create test file
    int result = create_test_file(TEST_FILE, TEST_SIZE, TEST_PATTERN);
    TEST_ASSERT(result == 0, "Test file creation should succeed");
    
    if (result != 0) {
        return;
    }
    
    // Open the file
    int fd = open(TEST_FILE, O_RDONLY);
    TEST_ASSERT(fd >= 0, "File opening should succeed");
    
    if (fd < 0) {
        unlink(TEST_FILE);
        return;
    }
    
    // Create mapping options
    struct mmap_options options = mmap_default_options();
    
    // Map the file using the file descriptor
    struct mmap_region* region = mmap_file(NULL, fd, &options);
    TEST_ASSERT(region != NULL, "File descriptor mapping should succeed");
    
    if (region == NULL) {
        printf("FD mapping error: %s\n", mmap_get_error());
        close(fd);
        unlink(TEST_FILE);
        return;
    }
    
    // Get pointer to mapped memory
    void* data = mmap_get_pointer(region);
    TEST_ASSERT(data != NULL, "Mapped memory pointer should be valid");
    
    // Verify file content
    if (data != NULL) {
        unsigned char* buffer = (unsigned char*)data;
        bool data_valid = true;
        for (size_t i = 0; i < TEST_SIZE; i++) {
            if (buffer[i] != TEST_PATTERN) {
                data_valid = false;
                break;
            }
        }
        TEST_ASSERT(data_valid, "Mapped data should match file content");
    }
    
    // Unmap the file
    int unmap_result = mmap_unmap(region);
    TEST_ASSERT(unmap_result == 0, "File unmapping should succeed");
    
    // Close the file descriptor
    close(fd);
    
    // Clean up
    unlink(TEST_FILE);
}

/**
 * Test: Error handling for invalid inputs
 */
static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    struct mmap_options options = mmap_default_options();
    
    // Test 1: Try to map non-existent file
    struct mmap_region* region = mmap_file("nonexistent_file.dat", -1, &options);
    TEST_ASSERT(region == NULL, "Mapping non-existent file should fail");
    
    const char* error = mmap_get_error();
    TEST_ASSERT(error != NULL && strlen(error) > 0, "Error message should be available for failed mapping");
    
    printf("Expected error for non-existent file: %s\n", error ? error : "No error message");
    
    // Test 2: Try to map with NULL options
    region = mmap_file(NULL, -1, NULL);
    TEST_ASSERT(region == NULL, "Mapping with NULL options should fail");
    
    // Test 3: Try to get pointer from NULL region
    void* data = mmap_get_pointer(NULL);
    TEST_ASSERT(data == NULL, "Getting pointer from NULL region should return NULL");
    
    // Test 4: Try to get size from NULL region
    size_t size = mmap_get_size(NULL);
    TEST_ASSERT(size == 0, "Getting size from NULL region should return 0");
    
    // Test 5: Try to unmap NULL region
    int result = mmap_unmap(NULL);
    TEST_ASSERT(result != 0, "Unmapping NULL region should fail");
}

/**
 * Test: Partial mapping with offset
 */
static void test_partial_mapping(void) {
    printf("\n=== Testing partial mapping with offset ===\n");
    
    // Create larger test file
    int result = create_test_file(TEST_LARGE_FILE, TEST_LARGE_SIZE, 0);
    TEST_ASSERT(result == 0, "Large test file creation should succeed");
    
    if (result != 0) {
        return;
    }
    
    // Write pattern at specific offset
    FILE* fp = fopen(TEST_LARGE_FILE, "r+b");
    TEST_ASSERT(fp != NULL, "Test file should be reopenable for writing");
    
    if (fp == NULL) {
        unlink(TEST_LARGE_FILE);
        return;
    }
    
    // Use page-aligned offset (start of file)
    const off_t test_offset = 0;
    const size_t test_size = 512;
    char* test_data = malloc(test_size);
    TEST_ASSERT(test_data != NULL, "Test data allocation should succeed");
    
    if (test_data == NULL) {
        fclose(fp);
        unlink(TEST_LARGE_FILE);
        return;
    }
    
    memset(test_data, TEST_PATTERN, test_size);
    
    fseek(fp, test_offset, SEEK_SET);
    size_t written = fwrite(test_data, 1, test_size, fp);
    fclose(fp);
    free(test_data);
    
    TEST_ASSERT(written == test_size, "Test data should be written completely");
    
    // Create mapping options with offset
    struct mmap_options options = mmap_default_options();
    options.offset = test_offset;
    options.mapping_size = test_size;
    
    // Map the file
    struct mmap_region* region = mmap_file(TEST_LARGE_FILE, -1, &options);
    TEST_ASSERT(region != NULL, "Partial file mapping should succeed");
    
    if (region == NULL) {
        printf("Partial mapping error: %s\n", mmap_get_error());
        unlink(TEST_LARGE_FILE);
        return;
    }
    
    // Get pointer to mapped memory
    void* data = mmap_get_pointer(region);
    TEST_ASSERT(data != NULL, "Mapped memory pointer should be valid");
    
    // Verify mapped data
    if (data != NULL) {
        unsigned char* buffer = (unsigned char*)data;
        bool data_valid = true;
        for (size_t i = 0; i < test_size; i++) {
            if (buffer[i] != TEST_PATTERN) {
                data_valid = false;
                break;
            }
        }
        TEST_ASSERT(data_valid, "Partially mapped data should match expected pattern");
    }
    
    // Unmap the file
    int unmap_result = mmap_unmap(region);
    TEST_ASSERT(unmap_result == 0, "Partial file unmapping should succeed");
    
    // Clean up
    unlink(TEST_LARGE_FILE);
}

/**
 * Test: Invalid file descriptor handling
 */
static void test_invalid_fd(void) {
    printf("\n=== Testing invalid file descriptor handling ===\n");
    
    struct mmap_options options = mmap_default_options();
    
    // Test with invalid file descriptor
    struct mmap_region* region = mmap_file(NULL, -999, &options);
    TEST_ASSERT(region == NULL, "Mapping with invalid fd should fail");
    
    const char* error = mmap_get_error();
    TEST_ASSERT(error != NULL && strlen(error) > 0, "Error message should be available for invalid fd");
    
    printf("Expected error for invalid fd: %s\n", error ? error : "No error message");
}

/**
 * Test: Zero-size file handling
 */
static void test_zero_size_file(void) {
    printf("\n=== Testing zero-size file handling ===\n");
    
    // Create zero-size file
    int result = create_test_file(TEST_ZERO_FILE, 0, 0);
    TEST_ASSERT(result == 0, "Zero-size file creation should succeed");
    
    if (result != 0) {
        return;
    }
    
    struct mmap_options options = mmap_default_options();
    
    // Try to map zero-size file
    struct mmap_region* region = mmap_file(TEST_ZERO_FILE, -1, &options);
    TEST_ASSERT(region == NULL, "Mapping zero-size file should fail gracefully");
    
    const char* error = mmap_get_error();
    TEST_ASSERT(error != NULL && strlen(error) > 0, "Error message should be available for zero-size file");
    
    printf("Expected error for zero-size file: %s\n", error ? error : "No error message");
    
    // Clean up
    unlink(TEST_ZERO_FILE);
}

/**
 * Test: Large mapping request handling
 */
static void test_large_mapping_request(void) {
    printf("\n=== Testing large mapping request handling ===\n");
    
    // Create small test file
    int result = create_test_file(TEST_FILE, TEST_SIZE, TEST_PATTERN);
    TEST_ASSERT(result == 0, "Test file creation should succeed");
    
    if (result != 0) {
        return;
    }
    
    struct mmap_options options = mmap_default_options();
    // Request mapping size much larger than file
    options.mapping_size = SIZE_MAX / 2; // Extremely large mapping size
    
    // Try to map with unreasonable size
    struct mmap_region* region = mmap_file(TEST_FILE, -1, &options);
    
    // This should either fail or map only the available file size
    if (region != NULL) {
        size_t mapped_size = mmap_get_size(region);
        TEST_ASSERT(mapped_size <= TEST_SIZE, "Mapped size should not exceed file size");
        
        int unmap_result = mmap_unmap(region);
        TEST_ASSERT(unmap_result == 0, "Unmapping should succeed even for adjusted size");
    } else {
        // It's also acceptable for this to fail
        const char* error = mmap_get_error();
        printf("Large mapping request failed as expected: %s\n", error ? error : "No error message");
        TEST_ASSERT(true, "Large mapping request handled appropriately");
    }
    
    // Clean up
    unlink(TEST_FILE);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    
    printf("\n========================================\n");
    printf("Starting tests for test_io_memory_map\n");
    printf("========================================\n\n");
    
    printf("This test verifies memory mapping I/O infrastructure:\n");
    printf("  1. Cross-platform memory mapping availability\n");
    printf("  2. File mapping and data access functionality\n");
    printf("  3. Error handling and resource management\n");
    printf("  4. Edge cases and invalid input handling\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_mmap_availability();
    test_default_options();
    test_basic_mapping();
    test_fd_mapping();
    test_error_handling();
    test_partial_mapping();
    test_invalid_fd();
    test_zero_size_file();
    test_large_mapping_request();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_io_memory_map:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
