#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../src/io/io_buffer_manager.h"

/* Test callback functions */
static int test_write_callback(int fd, const void* buffer, size_t size, off_t offset, void* context) {
    return pwrite(fd, buffer, size, offset);
}

static ssize_t test_read_callback(int fd, void* buffer, size_t size, off_t offset, void* context) {
    return pread(fd, buffer, size, offset);
}

/* Helper to create a test file with specified content */
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

/* Basic buffer creation and destruction test */
static int test_buffer_create_destroy() {
    printf("Running test_buffer_create_destroy...\n");
    
    // Create a test configuration
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    
    // Create a dummy file
    const char* test_filename = "test_buffer.dat";
    if (create_test_file(test_filename, "test data") != 0) {
        return -1;
    }
    
    // Open the file
    int fd = open(test_filename, O_RDWR);
    if (fd < 0) {
        perror("Failed to open test file");
        return -1;
    }
    
    // Create a buffer
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to create buffer\n");
        close(fd);
        return -1;
    }
    
    // Verify buffer size
    size_t capacity = buffer_get_capacity(buffer);
    printf("  Buffer capacity: %zu bytes\n", capacity);
    assert(capacity >= 1024 * 1024); // At least 1 MB
    
    // Clean up
    int result = buffer_destroy(buffer);
    close(fd);
    unlink(test_filename);
    
    printf("  test_buffer_create_destroy: %s\n", (result == 0) ? "PASSED" : "FAILED");
    return result;
}

/* Test writing data through the buffer */
static int test_buffer_write() {
    printf("Running test_buffer_write...\n");
    
    // Create a test configuration with a small buffer to force flushing
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    
    // Create a temporary file
    const char* test_filename = "test_buffer_write.dat";
    int fd = open(test_filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to create test file");
        return -1;
    }
    
    // Create a buffer
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to create buffer\n");
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    // Create some test data
    const int DATA_SIZE = 2 * 1024 * 1024; // 2 MB (should be larger than buffer)
    char* data = (char*)malloc(DATA_SIZE);
    if (data == NULL) {
        fprintf(stderr, "Failed to allocate test data\n");
        buffer_destroy(buffer);
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    // Fill with a pattern
    for (int i = 0; i < DATA_SIZE; i++) {
        data[i] = 'A' + (i % 26);
    }
    
    // Write data in chunks
    const int CHUNK_SIZE = 100 * 1024; // 100 KB
    int total_written = 0;
    int chunks = DATA_SIZE / CHUNK_SIZE;
    
    for (int i = 0; i < chunks; i++) {
        int result = buffer_write(buffer, data + (i * CHUNK_SIZE), CHUNK_SIZE);
        if (result != 0) {
            fprintf(stderr, "Failed to write data chunk %d: error %d\n", i, result);
            free(data);
            buffer_destroy(buffer);
            close(fd);
            unlink(test_filename);
            return -1;
        }
        total_written += CHUNK_SIZE;
    }
    
    // Flush and destroy buffer
    buffer_destroy(buffer);
    
    // Verify the file content
    char* verification_data = (char*)malloc(DATA_SIZE);
    if (verification_data == NULL) {
        fprintf(stderr, "Failed to allocate verification data\n");
        free(data);
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    lseek(fd, 0, SEEK_SET);
    ssize_t bytes_read = read(fd, verification_data, DATA_SIZE);
    
    if (bytes_read != total_written) {
        fprintf(stderr, "Read %zd bytes but expected %d bytes\n", bytes_read, total_written);
        free(data);
        free(verification_data);
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    // Compare the data
    int mismatch = 0;
    for (int i = 0; i < total_written; i++) {
        if (data[i] != verification_data[i]) {
            mismatch = 1;
            fprintf(stderr, "Data mismatch at position %d: expected %c, got %c\n", 
                   i, data[i], verification_data[i]);
            break;
        }
    }
    
    // Clean up
    free(data);
    free(verification_data);
    close(fd);
    unlink(test_filename);
    
    printf("  test_buffer_write: %s\n", (mismatch == 0) ? "PASSED" : "FAILED");
    return mismatch;
}

/* Test buffer resizing */
static int test_buffer_resize() {
    printf("Running test_buffer_resize...\n");
    
    // Create a test configuration
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    config.auto_resize = true;
    config.resize_threshold_percent = 70;
    
    // Create a temporary file
    const char* test_filename = "test_buffer_resize.dat";
    int fd = open(test_filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to create test file");
        return -1;
    }
    
    // Create a buffer
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to create buffer\n");
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    // Get initial capacity
    size_t initial_capacity = buffer_get_capacity(buffer);
    printf("  Initial buffer capacity: %zu bytes\n", initial_capacity);
    
    // Write data until we trigger resize
    const int CHUNK_SIZE = 100 * 1024; // 100 KB
    char* chunk = (char*)malloc(CHUNK_SIZE);
    if (chunk == NULL) {
        fprintf(stderr, "Failed to allocate chunk memory\n");
        buffer_destroy(buffer);
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    // Fill with a pattern
    for (int i = 0; i < CHUNK_SIZE; i++) {
        chunk[i] = 'A' + (i % 26);
    }
    
    // Write chunks until buffer resizes
    int chunks_written = 0;
    size_t current_capacity = initial_capacity;
    
    for (int i = 0; i < 20; i++) { // Max 20 chunks (should be enough to trigger resize)
        int result = buffer_write(buffer, chunk, CHUNK_SIZE);
        if (result != 0) {
            fprintf(stderr, "Failed to write data chunk %d: error %d\n", i, result);
            free(chunk);
            buffer_destroy(buffer);
            close(fd);
            unlink(test_filename);
            return -1;
        }
        
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
    
    // Verify that resize actually happened
    int resize_happened = (current_capacity > initial_capacity);
    
    // Clean up
    free(chunk);
    buffer_destroy(buffer);
    close(fd);
    unlink(test_filename);
    
    printf("  test_buffer_resize: %s\n", resize_happened ? "PASSED" : "FAILED");
    return resize_happened ? 0 : -1;
}

/* Test read functionality */
static int test_buffer_read() {
    printf("Running test_buffer_read...\n");
    
    // Create a test file with known content
    const char* test_filename = "test_buffer_read.dat";
    const char* test_content = "This is test content for buffer reading!";
    if (create_test_file(test_filename, test_content) != 0) {
        return -1;
    }
    
    // Open the file
    int fd = open(test_filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open test file");
        unlink(test_filename);
        return -1;
    }
    
    // Create a buffer configuration
    struct io_buffer_config config = buffer_config_default(1, 1, 4);
    
    // Create a buffer (need to provide a write callback even for read)
    struct io_buffer* buffer = buffer_create(&config, fd, 0, test_write_callback, NULL);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to create buffer\n");
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    // Read the data
    char read_buffer[100];
    ssize_t bytes_read = buffer_read(buffer, test_read_callback, NULL, read_buffer, 
                                   strlen(test_content));
    
    if (bytes_read != (ssize_t)strlen(test_content)) {
        fprintf(stderr, "Read %zd bytes but expected %zu bytes\n", 
               bytes_read, strlen(test_content));
        buffer_destroy(buffer);
        close(fd);
        unlink(test_filename);
        return -1;
    }
    
    // Null terminate for comparison
    read_buffer[bytes_read] = '\0';
    
    // Verify the data
    int match = (strcmp(read_buffer, test_content) == 0);
    
    // Clean up
    buffer_destroy(buffer);
    close(fd);
    unlink(test_filename);
    
    printf("  test_buffer_read: %s\n", match ? "PASSED" : "FAILED");
    return match ? 0 : -1;
}

int main() {
    int failures = 0;
    
    // Run the tests
    if (test_buffer_create_destroy() != 0) failures++;
    if (test_buffer_write() != 0) failures++;
    if (test_buffer_resize() != 0) failures++;
    if (test_buffer_read() != 0) failures++;
    
    // Print summary
    printf("\nTest Summary: %s (%d %s)\n", 
           (failures == 0) ? "ALL TESTS PASSED" : "SOME TESTS FAILED",
           (failures == 0) ? 4 : failures,
           (failures == 0) ? "tests passed" : "tests failed");
    
    return (failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}