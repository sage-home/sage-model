#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/io/io_interface.h"
#ifdef HDF5
#include "../src/io/io_hdf5_utils.h"
#endif

// Simple mock handler for testing
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

// Test handler registration and lookup
int test_handler_registry() {
    int status;
    
    // Initialize the I/O system
    status = io_init();
    if (status != 0) {
        fprintf(stderr, "Failed to initialize I/O system\n");
        return 1;
    }
    
    // Register our mock handler
    status = io_register_handler(&mock_handler);
    if (status != 0) {
        fprintf(stderr, "Failed to register mock handler: %s\n", io_get_error_message());
        return 1;
    }
    
    // Look up the handler by ID
    struct io_interface *handler = io_get_handler_by_id(999);
    if (handler == NULL) {
        fprintf(stderr, "Failed to look up handler: %s\n", io_get_error_message());
        return 1;
    }
    
    // Verify handler properties
    if (handler->format_id != 999 || 
        strcmp(handler->name, "Mock Handler") != 0 ||
        strcmp(handler->version, "1.0") != 0) {
        fprintf(stderr, "Handler properties don't match\n");
        return 1;
    }
    
    // Test capability checking
    if (!io_has_capability(handler, IO_CAP_RANDOM_ACCESS)) {
        fprintf(stderr, "Expected RANDOM_ACCESS capability not found\n");
        return 1;
    }
    
    if (io_has_capability(handler, IO_CAP_COMPRESSION)) {
        fprintf(stderr, "Unexpected COMPRESSION capability found\n");
        return 1;
    }
    
    // Clean up
    io_cleanup();
    
    printf("Handler registry test passed\n");
    return 0;
}

// Test error handling
int test_error_handling() {
    // Set an error
    io_set_error(IO_ERROR_FILE_NOT_FOUND, "Test error message");
    
    // Check error code
    if (io_get_last_error() != IO_ERROR_FILE_NOT_FOUND) {
        fprintf(stderr, "Error code doesn't match\n");
        return 1;
    }
    
    // Check error message
    if (strcmp(io_get_error_message(), "Test error message") != 0) {
        fprintf(stderr, "Error message doesn't match\n");
        return 1;
    }
    
    // Clear error
    io_clear_error();
    
    // Verify error is cleared
    if (io_get_last_error() != IO_ERROR_NONE || 
        strlen(io_get_error_message()) != 0) {
        fprintf(stderr, "Error not cleared\n");
        return 1;
    }
    
    printf("Error handling test passed\n");
    return 0;
}

// Test format ID mapping
int test_format_mapping() {
    // Test tree type mapping
    int format_id = io_map_tree_type_to_format_id(lhalo_binary);
    if (format_id != IO_FORMAT_LHALO_BINARY) {
        fprintf(stderr, "Tree type mapping failed\n");
        return 1;
    }
    
    // Test output format mapping - binary format is now unsupported
    format_id = io_map_output_format_to_format_id(sage_hdf5);
    if (format_id != IO_FORMAT_HDF5_OUTPUT) {
        fprintf(stderr, "Output format mapping failed\n");
        return 1;
    }
    
    printf("Format mapping test passed\n");
    return 0;
}

#ifdef HDF5
// Test HDF5 handle tracking
int test_hdf5_tracking() {
    int status;
    
    // Initialize tracking
    status = hdf5_tracking_init();
    if (status != 0) {
        fprintf(stderr, "Failed to initialize HDF5 tracking\n");
        return 1;
    }
    
    // Check initial state
    if (hdf5_get_open_handle_count() != 0) {
        fprintf(stderr, "Expected 0 handles initially\n");
        return 1;
    }
    
    // Mock handle IDs (not real HDF5 handles)
    hid_t file_id = 100;
    hid_t group_id = 200;
    
    // Track handles
    status = hdf5_track_handle(file_id, HDF5_HANDLE_FILE, "test_file.c", 123);
    if (status != 0) {
        fprintf(stderr, "Failed to track file handle\n");
        return 1;
    }
    
    status = hdf5_track_handle(group_id, HDF5_HANDLE_GROUP, "test_file.c", 456);
    if (status != 0) {
        fprintf(stderr, "Failed to track group handle\n");
        return 1;
    }
    
    // Check handle count
    if (hdf5_get_open_handle_count() != 2) {
        fprintf(stderr, "Expected 2 handles tracked\n");
        return 1;
    }
    
    // Untrack one handle
    status = hdf5_untrack_handle(file_id);
    if (status != 0) {
        fprintf(stderr, "Failed to untrack file handle\n");
        return 1;
    }
    
    // Check handle count again
    if (hdf5_get_open_handle_count() != 1) {
        fprintf(stderr, "Expected 1 handle after untracking\n");
        return 1;
    }
    
    // Print handles for debugging
    hdf5_print_open_handles();
    
    // Enable testing mode to skip actual handle closing
    hdf5_set_testing_mode(1);
    
    // Clean up
    status = hdf5_tracking_cleanup();
    if (status != 0) {
        fprintf(stderr, "Failed to clean up HDF5 tracking\n");
        return 1;
    }
    
    // Disable testing mode
    hdf5_set_testing_mode(0);
    
    printf("HDF5 tracking test passed\n");
    return 0;
}
#endif

int main() {
    int status = 0;
    
    printf("Running I/O interface tests...\n");
    
    status |= test_handler_registry();
    status |= test_error_handling();
    status |= test_format_mapping();
    
#ifdef HDF5
    status |= test_hdf5_tracking();
#endif
    
    if (status == 0) {
        printf("All I/O interface tests passed!\n");
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Some tests failed\n");
        return EXIT_FAILURE;
    }
}
