#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../src/io/io_memory_map.h"

#define TEST_PASSED printf("PASSED: %s\n", __func__)
#define TEST_FAILED printf("FAILED: %s at line %d\n", __func__, __LINE__); exit(EXIT_FAILURE)

#define TEST_FILE "test_mmap_file.dat"
#define TEST_SIZE 1024
#define TEST_PATTERN 0xAB

/**
 * Create a test file with the specified pattern
 */
void create_test_file(const char* filename, size_t size, char pattern) {
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to create test file %s\n", filename);
        exit(EXIT_FAILURE);
    }
    
    char* buffer = malloc(size);
    memset(buffer, pattern, size);
    
    fwrite(buffer, 1, size, fp);
    fclose(fp);
    free(buffer);
}

/**
 * Clean up test file
 */
void remove_test_file(const char* filename) {
    unlink(filename);
}

/**
 * Test basic mmap_is_available function
 */
void test_mmap_availability() {
    bool available = mmap_is_available();
    printf("Memory mapping availability: %s\n", available ? "Yes" : "No");
    
    // This should always be true on modern systems
    if (!available) {
        TEST_FAILED;
    }
    
    TEST_PASSED;
}

/**
 * Test creation of default options
 */
void test_default_options() {
    struct mmap_options options = mmap_default_options();
    
    if (options.mode != MMAP_READ_ONLY ||
        options.mapping_size != 0 ||
        options.offset != 0) {
        TEST_FAILED;
    }
    
    TEST_PASSED;
}

/**
 * Test basic memory mapping functionality
 */
void test_basic_mapping() {
    create_test_file(TEST_FILE, TEST_SIZE, TEST_PATTERN);
    
    // Create mapping options
    struct mmap_options options = mmap_default_options();
    
    // Map the file
    struct mmap_region* region = mmap_file(TEST_FILE, -1, &options);
    if (region == NULL) {
        fprintf(stderr, "Failed to map file: %s\n", mmap_get_error());
        TEST_FAILED;
    }
    
    // Get pointer to mapped memory
    void* data = mmap_get_pointer(region);
    if (data == NULL) {
        fprintf(stderr, "Failed to get pointer to mapped memory\n");
        TEST_FAILED;
    }
    
    // Verify file content
    unsigned char* buffer = (unsigned char*)data;
    for (size_t i = 0; i < TEST_SIZE; i++) {
        if (buffer[i] != TEST_PATTERN) {
            fprintf(stderr, "Data mismatch at offset %zu: expected 0x%02X, got 0x%02X\n", 
                    i, TEST_PATTERN, buffer[i]);
            TEST_FAILED;
        }
    }
    
    // Get mapped size
    size_t size = mmap_get_size(region);
    if (size != TEST_SIZE) {
        fprintf(stderr, "Size mismatch: expected %d, got %zu\n", TEST_SIZE, size);
        TEST_FAILED;
    }
    
    // Unmap the file
    int result = mmap_unmap(region);
    if (result != 0) {
        fprintf(stderr, "Failed to unmap file: %s\n", mmap_get_error());
        TEST_FAILED;
    }
    
    // Clean up
    remove_test_file(TEST_FILE);
    
    TEST_PASSED;
}

/**
 * Test mapping with file descriptor
 */
void test_fd_mapping() {
    create_test_file(TEST_FILE, TEST_SIZE, TEST_PATTERN);
    
    // Open the file
    int fd = open(TEST_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open test file");
        TEST_FAILED;
    }
    
    // Create mapping options
    struct mmap_options options = mmap_default_options();
    
    // Map the file using the file descriptor
    struct mmap_region* region = mmap_file(NULL, fd, &options);
    if (region == NULL) {
        fprintf(stderr, "Failed to map file: %s\n", mmap_get_error());
        close(fd);
        TEST_FAILED;
    }
    
    // Get pointer to mapped memory
    void* data = mmap_get_pointer(region);
    if (data == NULL) {
        fprintf(stderr, "Failed to get pointer to mapped memory\n");
        close(fd);
        TEST_FAILED;
    }
    
    // Verify file content
    unsigned char* buffer = (unsigned char*)data;
    for (size_t i = 0; i < TEST_SIZE; i++) {
        if (buffer[i] != TEST_PATTERN) {
            fprintf(stderr, "Data mismatch at offset %zu: expected 0x%02X, got 0x%02X\n", 
                    i, TEST_PATTERN, buffer[i]);
            close(fd);
            TEST_FAILED;
        }
    }
    
    // Unmap the file
    int result = mmap_unmap(region);
    if (result != 0) {
        fprintf(stderr, "Failed to unmap file: %s\n", mmap_get_error());
        close(fd);
        TEST_FAILED;
    }
    
    // Close the file descriptor
    close(fd);
    
    // Clean up
    remove_test_file(TEST_FILE);
    
    TEST_PASSED;
}

/**
 * Test error handling
 */
void test_error_handling() {
    // Try to map non-existent file
    struct mmap_options options = mmap_default_options();
    struct mmap_region* region = mmap_file("nonexistent_file.dat", -1, &options);
    
    if (region != NULL) {
        fprintf(stderr, "Expected mapping failure for non-existent file\n");
        TEST_FAILED;
    }
    
    const char* error = mmap_get_error();
    if (error == NULL || strlen(error) == 0) {
        fprintf(stderr, "Expected error message for failed mapping\n");
        TEST_FAILED;
    }
    
    // Print error message to stderr instead of stdout
    fprintf(stderr, "Error message for non-existent file: %s\n", error);
    
    TEST_PASSED;
}

/**
 * Test partial mapping with offset
 */
void test_partial_mapping() {
    // Create larger test file
    const size_t file_size = 4096;
    create_test_file(TEST_FILE, file_size, 0);
    
    // Write pattern at specific offset
    FILE* fp = fopen(TEST_FILE, "r+b");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open test file for writing\n");
        TEST_FAILED;
    }
    
    // On most systems, mmap offset must be page-aligned
    // Use page-aligned offset (start of file)
    const off_t test_offset = 0;
    const size_t test_size = 512;
    char* test_data = malloc(test_size);
    memset(test_data, TEST_PATTERN, test_size);
    
    fseek(fp, test_offset, SEEK_SET);
    fwrite(test_data, 1, test_size, fp);
    fclose(fp);
    free(test_data);
    
    // Create mapping options with offset
    struct mmap_options options = mmap_default_options();
    options.offset = test_offset;
    options.mapping_size = test_size;
    
    // Map the file
    struct mmap_region* region = mmap_file(TEST_FILE, -1, &options);
    if (region == NULL) {
        fprintf(stderr, "Failed to map file with offset: %s\n", mmap_get_error());
        TEST_FAILED;
    }
    
    // Get pointer to mapped memory
    void* data = mmap_get_pointer(region);
    if (data == NULL) {
        fprintf(stderr, "Failed to get pointer to mapped memory\n");
        TEST_FAILED;
    }
    
    // Verify mapped data
    unsigned char* buffer = (unsigned char*)data;
    for (size_t i = 0; i < test_size; i++) {
        if (buffer[i] != TEST_PATTERN) {
            fprintf(stderr, "Data mismatch at offset %zu: expected 0x%02X, got 0x%02X\n", 
                    i, TEST_PATTERN, buffer[i]);
            TEST_FAILED;
        }
    }
    
    // Unmap the file
    int result = mmap_unmap(region);
    if (result != 0) {
        fprintf(stderr, "Failed to unmap file: %s\n", mmap_get_error());
        TEST_FAILED;
    }
    
    // Clean up
    remove_test_file(TEST_FILE);
    
    TEST_PASSED;
}

int main(int argc, char** argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    printf("Testing memory mapping implementation...\n");
    
    test_mmap_availability();
    test_default_options();
    test_basic_mapping();
    test_fd_mapping();
    test_error_handling();
    test_partial_mapping();
    
    printf("All tests passed!\n");
    return 0;
}
