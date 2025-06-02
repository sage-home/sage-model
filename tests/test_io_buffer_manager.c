/**
 * Test suite for I/O Buffer Manager
 * 
 * Tests cover:
 * - Basic buffer lifecycle management
 * - Buffered write operations with auto-flush
 * - Dynamic buffer resizing functionality
 * - Read operations through callbacks
 * - Error handling and edge cases
 * - Performance characteristics
 * - Resource management and cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "../src/io/io_buffer_manager.h"

// Test constants
#define TEST_CHUNK_SIZE (100 * 1024)  // 100 KB
#define TEST_LARGE_SIZE (10 * 1024 * 1024)  // 10 MB
#define TEST_STRESS_SIZE (50 * 1024 * 1024)  // 50 MB
#define TEST_FILENAME_PREFIX "test_buffer"
#define MAX_FILENAME_LEN 256

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test fixtures
static struct test_context {
    char temp_files[10][MAX_FILENAME_LEN];
    int num_temp_files;
    int initialized;
} test_ctx;

//=============================================================================
// Test Helper Functions
//=============================================================================

/**
 * Setup function - called before tests
 */
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    test_ctx.initialized = 1;
    return 0;
}

/**
 * Teardown function - called after tests
 */
static void teardown_test_context(void) {
    // Clean up any remaining temp files
    for (int i = 0; i < test_ctx.num_temp_files; i++) {
        unlink(test_ctx.temp_files[i]);
    }
    test_ctx.num_temp_files = 0;
    test_ctx.initialized = 0;
}

/**
 * Generate unique temporary filename
 */
static void generate_temp_filename(char* filename, const char* suffix) {
    static int counter = 0;
    snprintf(filename, MAX_FILENAME_LEN, "%s_%d_%s.dat", 
             TEST_FILENAME_PREFIX, counter++, suffix);
    
    // Track for cleanup
    if (test_ctx.num_temp_files < 10) {
        strncpy(test_ctx.temp_files[test_ctx.num_temp_files], filename, MAX_FILENAME_LEN - 1);
        test_ctx.num_temp_files++;
    }
}

/**
 * Test callback functions
 */
static int test_write_callback(int fd, const void* buffer, size_t size, off_t offset, void* context __attribute__((unused))) {
    return pwrite(fd, buffer, size, offset);
}

static ssize_t test_read_callback(int fd, void* buffer, size_t size, off_t offset, void* context __attribute__((unused))) {
    return pread(fd, buffer, size, offset);
}

/**
 * Helper to create a test file with specified content
 */
static int create_test_file(const char* filename, const char* content) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("Failed to create test file");
        return -1;
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    if (written != len) {
        perror("Failed to write test content");
        return -1;
    }
    
    return 0;
}

/**
 * Helper to create test data with a specific pattern
 */
static char* create_test_data(size_t size, char pattern_base) {
    char* data = (char*)malloc(size);
    if (data == NULL) {
        return NULL;
    }
    
    for (size_t i = 0; i < size; i++) {
        data[i] = pattern_base + (i % 26);
    }
    
    return data;
}

/**
 * Helper to verify data integrity
 */
static int verify_data_integrity(const char* data1, const char* data2, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (data1[i] != data2[i]) {
            printf("  Data mismatch at position %zu: expected %c, got %c\n", 
                   i, data1[i], data2[i]);
            return 0;
        }
    }
    return 1;
}

//=============================================================================
// Basic Functionality Tests
//=============================================================================

/**
 * Test: Buffer creation and destruction
 */
static int test_buffer_create_destroy(void) {
    printf("=== Testing buffer creation and destruction ===\n");
    
    // Create a test configuration
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "create_destroy");
    
    // Create a dummy file
    int create_result = create_test_file(filename, "test data");
    TEST_ASSERT(create_result == 0, "Should create test file successfully");
    
    // Open the file
    int fd = open(filename, O_RDWR);
    TEST_ASSERT(fd >= 0, "Should open test file successfully");
    
    // Create a buffer
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Verify buffer properties
    size_t capacity = buffer_get_capacity(buffer);
    TEST_ASSERT(capacity >= 1024 * 1024, "Buffer capacity should be at least 1 MB");
    printf("  Buffer capacity: %zu bytes (%.2f MB)\n", capacity, capacity / (1024.0 * 1024.0));
    
    size_t used = buffer_get_used(buffer);
    TEST_ASSERT(used == 0, "New buffer should have zero used bytes");
    
    // Clean up
    int destroy_result = buffer_destroy(buffer);
    TEST_ASSERT(destroy_result == 0, "Should destroy buffer successfully");
    
    close(fd);
    unlink(filename);
    
    return 0;
}

/**
 * Test: Basic write operations
 */
static int test_buffer_write_basic(void) {
    printf("\n=== Testing basic write operations ===\n");
    
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "write_basic");
    
    // Create and open file
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should create test file successfully");
    
    // Create buffer
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Create test data
    const char* test_data = "Hello, World! This is a test of basic write functionality.";
    size_t data_len = strlen(test_data);
    
    // Write data
    int write_result = buffer_write(buffer, test_data, data_len);
    TEST_ASSERT(write_result == 0, "Should write data successfully");
    
    // Verify buffer state
    size_t used = buffer_get_used(buffer);
    TEST_ASSERT(used == data_len, "Buffer should contain written data");
    
    // Flush and destroy
    int destroy_result = buffer_destroy(buffer);
    TEST_ASSERT(destroy_result == 0, "Should destroy buffer successfully");
    
    // Verify file content
    char* verification_data = (char*)malloc(data_len + 1);
    TEST_ASSERT(verification_data != NULL, "Should allocate verification buffer");
    
    lseek(fd, 0, SEEK_SET);
    ssize_t bytes_read = read(fd, verification_data, data_len);
    TEST_ASSERT(bytes_read == (ssize_t)data_len, "Should read expected amount of data");
    
    verification_data[data_len] = '\0';
    TEST_ASSERT(strcmp(test_data, verification_data) == 0, "File content should match written data");
    
    free(verification_data);
    close(fd);
    unlink(filename);
    
    return 0;
}

/**
 * Test: Large write operations with automatic flushing
 */
static int test_buffer_write_large(void) {
    printf("\n=== Testing large write operations ===\n");
    
    // Create configuration with small buffer to force flushing
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "write_large");
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should create test file successfully");
    
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Create large test data (larger than buffer to force flushing)
    const int DATA_SIZE = 2 * 1024 * 1024; // 2 MB
    char* data = create_test_data(DATA_SIZE, 'A');
    TEST_ASSERT(data != NULL, "Should allocate test data successfully");
    
    // Write data in chunks
    int total_written = 0;
    int chunks = DATA_SIZE / TEST_CHUNK_SIZE;
    
    for (int i = 0; i < chunks; i++) {
        int result = buffer_write(buffer, data + (i * TEST_CHUNK_SIZE), TEST_CHUNK_SIZE);
        TEST_ASSERT(result == 0, "Should write data chunk successfully");
        total_written += TEST_CHUNK_SIZE;
    }
    
    printf("  Successfully wrote %d bytes in %d chunks\n", total_written, chunks);
    
    // Destroy buffer (flushes remaining data)
    buffer_destroy(buffer);
    
    // Verify file content
    char* verification_data = (char*)malloc(DATA_SIZE);
    TEST_ASSERT(verification_data != NULL, "Should allocate verification buffer");
    
    lseek(fd, 0, SEEK_SET);
    ssize_t bytes_read = read(fd, verification_data, DATA_SIZE);
    TEST_ASSERT(bytes_read == total_written, "Should read expected amount of data");
    
    int integrity_check = verify_data_integrity(data, verification_data, total_written);
    TEST_ASSERT(integrity_check == 1, "Data integrity should be maintained");
    
    free(data);
    free(verification_data);
    close(fd);
    unlink(filename);
    
    return 0;
}

/**
 * Test: Dynamic buffer resizing
 */
static int test_buffer_resize(void) {
    printf("\n=== Testing dynamic buffer resizing ===\n");
    
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    config.auto_resize = true;
    config.resize_threshold_percent = 70;
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "resize");
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should create test file successfully");
    
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Get initial capacity
    size_t initial_capacity = buffer_get_capacity(buffer);
    printf("  Initial buffer capacity: %zu bytes (%.2f MB)\n", 
           initial_capacity, initial_capacity / (1024.0 * 1024.0));
    
    // Create test data
    char* chunk = create_test_data(TEST_CHUNK_SIZE, 'A');
    TEST_ASSERT(chunk != NULL, "Should allocate test chunk successfully");
    
    // Write chunks until buffer resizes
    int chunks_written = 0;
    size_t current_capacity = initial_capacity;
    
    for (int i = 0; i < 20; i++) { // Max 20 chunks
        int result = buffer_write(buffer, chunk, TEST_CHUNK_SIZE);
        TEST_ASSERT(result == 0, "Should write data chunk successfully");
        
        chunks_written++;
        
        // Check if buffer resized
        size_t new_capacity = buffer_get_capacity(buffer);
        if (new_capacity > current_capacity) {
            printf("  Buffer resized from %zu to %zu bytes after %d chunks\n", 
                  current_capacity, new_capacity, chunks_written);
            current_capacity = new_capacity;
            break;
        }
    }
    
    TEST_ASSERT(current_capacity > initial_capacity, "Buffer should have resized");
    
    free(chunk);
    buffer_destroy(buffer);
    close(fd);
    unlink(filename);
    
    return 0;
}

/**
 * Test: Read functionality
 */
static int test_buffer_read(void) {
    printf("\n=== Testing read functionality ===\n");
    
    const char* test_content = "This is test content for buffer reading! It includes multiple sentences.";
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "read");
    
    // Create test file
    int create_result = create_test_file(filename, test_content);
    TEST_ASSERT(create_result == 0, "Should create test file successfully");
    
    int fd = open(filename, O_RDONLY);
    TEST_ASSERT(fd >= 0, "Should open test file successfully");
    
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Read the data
    size_t content_len = strlen(test_content);
    char* read_buffer = (char*)malloc(content_len + 1);
    TEST_ASSERT(read_buffer != NULL, "Should allocate read buffer successfully");
    
    ssize_t bytes_read = buffer_read(buffer, test_read_callback, NULL, read_buffer, content_len);
    TEST_ASSERT(bytes_read == (ssize_t)content_len, "Should read expected amount of data");
    
    // Verify data
    read_buffer[bytes_read] = '\0';
    TEST_ASSERT(strcmp(read_buffer, test_content) == 0, "Read data should match file content");
    
    free(read_buffer);
    buffer_destroy(buffer);
    close(fd);
    unlink(filename);
    
    return 0;
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * Test: Error handling with invalid parameters
 */
static int test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    // Test NULL configuration
    struct io_buffer* result = buffer_create(NULL, 0, 0, test_write_callback, NULL);
    TEST_ASSERT(result == NULL, "buffer_create with NULL config should fail");
    
    // Test NULL write callback
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    result = buffer_create(&config, 0, 0, NULL, NULL);
    TEST_ASSERT(result == NULL, "buffer_create with NULL callback should fail");
    
    // Test invalid file descriptor
    result = buffer_create(&config, -1, 0, test_write_callback, NULL);
    TEST_ASSERT(result == NULL, "buffer_create with invalid fd should fail");
    
    // Test operations on NULL buffer
    int write_result = buffer_write(NULL, "test", 4);
    TEST_ASSERT(write_result != 0, "buffer_write with NULL buffer should fail");
    
    int flush_result = buffer_flush(NULL);
    TEST_ASSERT(flush_result != 0, "buffer_flush with NULL buffer should fail");
    
    size_t capacity = buffer_get_capacity(NULL);
    TEST_ASSERT(capacity == 0, "buffer_get_capacity with NULL buffer should return 0");
    
    size_t used = buffer_get_used(NULL);
    TEST_ASSERT(used == 0, "buffer_get_used with NULL buffer should return 0");
    
    // Test destroying NULL buffer (should not crash)
    int destroy_result = buffer_destroy(NULL);
    TEST_ASSERT(destroy_result == 0, "buffer_destroy with NULL buffer should succeed");
    
    printf("  All error conditions handled correctly\n");
    
    return 0;
}

/**
 * Test: Edge cases and boundary conditions
 */
static int test_edge_cases(void) {
    printf("\n=== Testing edge cases ===\n");
    
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "edge_cases");
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should create test file successfully");
    
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Test zero-length write
    int result = buffer_write(buffer, "test", 0);
    TEST_ASSERT(result == 0, "Zero-length write should succeed");
    
    size_t used = buffer_get_used(buffer);
    TEST_ASSERT(used == 0, "Buffer should remain empty after zero-length write");
    
    // Test write larger than buffer capacity
    size_t capacity = buffer_get_capacity(buffer);
    size_t large_size = capacity * 2;
    char* large_data = create_test_data(large_size, 'X');
    TEST_ASSERT(large_data != NULL, "Should allocate large test data successfully");
    
    result = buffer_write(buffer, large_data, large_size);
    TEST_ASSERT(result == 0, "Large write should succeed (bypass buffer)");
    
    // Buffer should still be empty after direct write
    used = buffer_get_used(buffer);
    TEST_ASSERT(used == 0, "Buffer should be empty after direct large write");
    
    free(large_data);
    buffer_destroy(buffer);
    close(fd);
    unlink(filename);
    
    return 0;
}

//=============================================================================
// Performance and Stress Tests
//=============================================================================

/**
 * Test: Performance characteristics
 */
static int test_performance(void) {
    printf("\n=== Testing performance characteristics ===\n");
    
    struct io_buffer_config config = buffer_config_default(4, 1, 16); // Larger buffer
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "performance");
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should create test file successfully");
    
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Measure write performance
    const int PERF_DATA_SIZE = TEST_LARGE_SIZE; // 10 MB
    char* perf_data = create_test_data(PERF_DATA_SIZE, 'P');
    TEST_ASSERT(perf_data != NULL, "Should allocate performance test data successfully");
    
    clock_t start_time = clock();
    
    // Write in medium-sized chunks
    const int PERF_CHUNK_SIZE = 64 * 1024; // 64 KB chunks
    int chunks = PERF_DATA_SIZE / PERF_CHUNK_SIZE;
    
    for (int i = 0; i < chunks; i++) {
        int result = buffer_write(buffer, perf_data + (i * PERF_CHUNK_SIZE), PERF_CHUNK_SIZE);
        TEST_ASSERT(result == 0, "Performance test write should succeed");
    }
    
    buffer_destroy(buffer); // Flush remaining data
    
    clock_t end_time = clock();
    double elapsed = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    double throughput = (PERF_DATA_SIZE / (1024.0 * 1024.0)) / elapsed;
    
    printf("  Wrote %.2f MB in %.3f seconds (%.2f MB/s)\n", 
           PERF_DATA_SIZE / (1024.0 * 1024.0), elapsed, throughput);
    
    TEST_ASSERT(elapsed > 0, "Performance test should take measurable time");
    TEST_ASSERT(throughput > 0, "Throughput should be positive");
    
    free(perf_data);
    close(fd);
    unlink(filename);
    
    return 0;
}

/**
 * Test: Stress testing with large datasets
 */
static int test_stress(void) {
    printf("\n=== Testing stress conditions ===\n");
    
    struct io_buffer_config config = buffer_config_default(2, 1, 8);
    config.auto_resize = true;
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "stress");
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should create test file successfully");
    
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Stress test with many small writes
    const int STRESS_ITERATIONS = 10000;
    const int SMALL_WRITE_SIZE = 1024; // 1 KB
    char* small_data = create_test_data(SMALL_WRITE_SIZE, 'S');
    TEST_ASSERT(small_data != NULL, "Should allocate stress test data successfully");
    
    size_t initial_capacity = buffer_get_capacity(buffer);
    int resize_count = 0;
    size_t current_capacity = initial_capacity;
    
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        int result = buffer_write(buffer, small_data, SMALL_WRITE_SIZE);
        TEST_ASSERT(result == 0, "Stress test write should succeed");
        
        // Track resizes
        size_t new_capacity = buffer_get_capacity(buffer);
        if (new_capacity > current_capacity) {
            resize_count++;
            current_capacity = new_capacity;
        }
        
        // Progress indicator
        if (i % 1000 == 0) {
            printf("  Completed %d/%d iterations\n", i, STRESS_ITERATIONS);
        }
    }
    
    printf("  Completed %d stress iterations with %d buffer resizes\n", 
           STRESS_ITERATIONS, resize_count);
    printf("  Final buffer capacity: %zu bytes (%.2f MB)\n", 
           current_capacity, current_capacity / (1024.0 * 1024.0));
    
    TEST_ASSERT(resize_count > 0, "Buffer should have resized during stress test");
    
    free(small_data);
    buffer_destroy(buffer);
    close(fd);
    unlink(filename);
    
    return 0;
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * Test: Integration with SAGE I/O patterns
 */
static int test_integration(void) {
    printf("\n=== Testing integration patterns ===\n");
    
    // Simulate SAGE-like I/O patterns: large sequential writes with periodic flushes
    struct io_buffer_config config = buffer_config_default(8, 2, 32);
    config.auto_resize = true;
    config.resize_threshold_percent = 85;
    
    char filename[MAX_FILENAME_LEN];
    generate_temp_filename(filename, "integration");
    
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should create test file successfully");
    
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    TEST_ASSERT(buffer != NULL, "Should create buffer successfully");
    
    // Simulate writing multiple galaxy snapshots
    const int NUM_SNAPSHOTS = 5;
    const int GALAXIES_PER_SNAPSHOT = 1000;
    const int GALAXY_RECORD_SIZE = 512; // Simulated galaxy data size
    
    char* galaxy_data = create_test_data(GALAXY_RECORD_SIZE, 'G');
    TEST_ASSERT(galaxy_data != NULL, "Should allocate galaxy data successfully");
    
    for (int snapshot = 0; snapshot < NUM_SNAPSHOTS; snapshot++) {
        printf("  Writing snapshot %d/%d (%d galaxies)\n", 
               snapshot + 1, NUM_SNAPSHOTS, GALAXIES_PER_SNAPSHOT);
        
        for (int galaxy = 0; galaxy < GALAXIES_PER_SNAPSHOT; galaxy++) {
            int result = buffer_write(buffer, galaxy_data, GALAXY_RECORD_SIZE);
            TEST_ASSERT(result == 0, "Galaxy write should succeed");
        }
        
        // Periodic flush (simulate snapshot completion)
        int flush_result = buffer_flush(buffer);
        TEST_ASSERT(flush_result == 0, "Snapshot flush should succeed");
    }
    
    size_t total_written = NUM_SNAPSHOTS * GALAXIES_PER_SNAPSHOT * GALAXY_RECORD_SIZE;
    printf("  Total data written: %zu bytes (%.2f MB)\n", 
           total_written, total_written / (1024.0 * 1024.0));
    
    free(galaxy_data);
    buffer_destroy(buffer);
    close(fd);
    
    // Verify file size
    struct stat file_stat;
    int stat_result = stat(filename, &file_stat);
    TEST_ASSERT(stat_result == 0, "Should get file statistics successfully");
    TEST_ASSERT((size_t)file_stat.st_size == total_written, "File size should match written data");
    
    unlink(filename);
    
    return 0;
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for test_io_buffer_manager\n");
    printf("========================================\n\n");
    
    printf("This test verifies buffer management functionality:\n");
    printf("  1. Buffer creation, configuration, and destruction\n");
    printf("  2. Buffered write operations with automatic flushing\n");
    printf("  3. Dynamic buffer resizing based on usage patterns\n");
    printf("  4. Read operations through callback mechanisms\n");
    printf("  5. Error handling and boundary condition management\n");
    printf("  6. Performance characteristics under various loads\n");
    printf("  7. Integration with SAGE-like I/O workflow patterns\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run basic functionality tests
    if (test_buffer_create_destroy() != 0) goto cleanup;
    if (test_buffer_write_basic() != 0) goto cleanup;
    if (test_buffer_write_large() != 0) goto cleanup;
    if (test_buffer_resize() != 0) goto cleanup;
    if (test_buffer_read() != 0) goto cleanup;
    
    // Run error handling tests
    if (test_error_handling() != 0) goto cleanup;
    if (test_edge_cases() != 0) goto cleanup;
    
    // Run performance tests
    if (test_performance() != 0) goto cleanup;
    if (test_stress() != 0) goto cleanup;
    
    // Run integration tests
    if (test_integration() != 0) goto cleanup;
    
cleanup:
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_io_buffer_manager:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
