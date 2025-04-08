#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/* Don't redefine TESTING_STANDALONE if already defined by compiler */
#ifndef TESTING_STANDALONE
#define TESTING_STANDALONE
#endif

#include "../src/io/io_memory_map.h"

/**
 * Test program for memory mapping integration 
 */

int main(int argc, char **argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning
    printf("Testing memory mapping integration\n");
    
    // Check if memory mapping is available on this platform
    if (!mmap_is_available()) {
        printf("Memory mapping is not available on this platform\n");
        return 1;
    }
    
    printf("Memory mapping is available on this platform\n");
    
    // Create a test file
    const char *filename = "test_mmap_integration.dat";
    const size_t file_size = 4096;
    char test_pattern = 0x42;
    
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to create test file\n");
        return 1;
    }
    
    char *buffer = malloc(file_size);
    memset(buffer, test_pattern, file_size);
    fwrite(buffer, 1, file_size, fp);
    fclose(fp);
    free(buffer);
    
    // Create a mapping to test integration
    struct mmap_options options = mmap_default_options();
    options.mode = MMAP_READ_ONLY;
    
    struct mmap_region *region = mmap_file(filename, -1, &options);
    if (region == NULL) {
        fprintf(stderr, "Failed to map file: %s\n", mmap_get_error());
        unlink(filename);
        return 1;
    }
    
    void *data = mmap_get_pointer(region);
    if (data == NULL) {
        fprintf(stderr, "Failed to get pointer to mapped memory\n");
        mmap_unmap(region);
        unlink(filename);
        return 1;
    }
    
    size_t size = mmap_get_size(region);
    if (size != file_size) {
        fprintf(stderr, "Size mismatch: expected %zu, got %zu\n", file_size, size);
        mmap_unmap(region);
        unlink(filename);
        return 1;
    }
    
    // Verify the content
    unsigned char *mapped_data = (unsigned char *)data;
    for (size_t i = 0; i < size; i++) {
        if (mapped_data[i] != test_pattern) {
            fprintf(stderr, "Data mismatch at offset %zu\n", i);
            mmap_unmap(region);
            unlink(filename);
            return 1;
        }
    }
    
    // Test successful
    mmap_unmap(region);
    unlink(filename);
    
    printf("Integration test successful\n");
    return 0;
}