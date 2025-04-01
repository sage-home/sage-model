#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/io/io_interface.h"
#include "../src/io/io_lhalo_hdf5.h"
#include "../src/core/core_allvars.h"

#ifdef HDF5

/**
 * Test for LHalo HDF5 format detection
 */
void test_format_detection() {
    printf("Testing LHalo HDF5 format detection...\n");
    
    // Test a file with HDF5 extension - will be detected by our basic stub
    const char *valid_file = "tests/test_lhalo_hdf5.c.hdf5";
    
    // If the test file exists, run the detection test
    FILE *test_file = fopen(valid_file, "r");
    if (test_file != NULL) {
        fclose(test_file);
        
        bool result = io_is_lhalo_hdf5(valid_file);
        assert(result == true);
        
        printf("LHalo HDF5 format detection test with valid file passed.\n");
    } else {
        printf("Skipping valid file test - test file not found.\n");
    }
    
    // Test with invalid file
    const char *invalid_file = "Makefile";
    bool result = io_is_lhalo_hdf5(invalid_file);
    assert(result == false);
    
    printf("LHalo HDF5 format detection tests passed.\n");
}

/**
 * Test for LHalo HDF5 handler registration
 */
void test_handler_registration() {
    printf("Testing LHalo HDF5 handler registration...\n");
    
    // Initialize I/O system - this automatically registers handlers
    io_init();
    
    // Get handler by ID
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_LHALO_HDF5);
    
    // Check if handler was found
    assert(handler != NULL);
    
    // Verify handler properties
    assert(handler->format_id == IO_FORMAT_LHALO_HDF5);
    assert(strcmp(handler->name, "LHalo HDF5") == 0);
    
    // For the stub implementation, the function pointers are NULL
    // When fully implemented, they should be non-NULL
    printf("NOTE: Using stub implementation with NULL function pointers\n");
    
    // Verify capabilities
    assert(io_has_capability(handler, IO_CAP_RANDOM_ACCESS));
    assert(io_has_capability(handler, IO_CAP_MULTI_FILE));
    assert(io_has_capability(handler, IO_CAP_METADATA_QUERY));
    assert(io_has_capability(handler, IO_CAP_METADATA_ATTRS));
    
    printf("Handler capability verification passed\n");
    
    printf("LHalo HDF5 handler registration tests passed.\n");
}

/**
 * Main test function
 */
int main() {
    printf("Running LHalo HDF5 handler tests...\n");
    
    // Initialize HDF5 resource tracking
    hdf5_tracking_init();
    
    test_format_detection();
    test_handler_registration();
    
    // Clean up
    hdf5_tracking_cleanup();
    io_cleanup();
    
    printf("All LHalo HDF5 handler tests passed!\n");
    return 0;
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