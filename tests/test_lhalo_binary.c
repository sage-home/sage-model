#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/io/io_interface.h"
#include "../src/io/io_lhalo_binary.h"
#include "../src/core/core_allvars.h"

/**
 * Test for LHalo binary format detection
 */
void test_format_detection() {
    printf("Testing LHalo binary format detection...\n");
    
    // Test with valid file
    const char *valid_file = "tests/test_data/trees_063.0";
    bool result = io_is_lhalo_binary(valid_file);
    assert(result == true);
    
    // Test with invalid file
    const char *invalid_file = "Makefile";
    result = io_is_lhalo_binary(invalid_file);
    assert(result == false);
    
    printf("LHalo binary format detection tests passed.\n");
}

/**
 * Test for LHalo binary handler registration
 */
void test_handler_registration() {
    printf("Testing LHalo binary handler registration...\n");
    
    // Initialize I/O system - this automatically registers handlers
    io_init();
    
    // Get handler by ID
    struct io_interface *handler = io_get_handler_by_id(IO_FORMAT_LHALO_BINARY);
    
    // Check if handler was found
    assert(handler != NULL);
    
    // Verify handler properties
    assert(handler->format_id == IO_FORMAT_LHALO_BINARY);
    assert(strcmp(handler->name, "LHalo Binary") == 0);
    assert(handler->initialize != NULL);
    assert(handler->read_forest != NULL);
    assert(handler->cleanup != NULL);
    
    // Verify capabilities
    assert(io_has_capability(handler, IO_CAP_RANDOM_ACCESS));
    assert(io_has_capability(handler, IO_CAP_MULTI_FILE));
    
    printf("LHalo binary handler registration tests passed.\n");
}

/**
 * Main test function
 */
int main() {
    printf("Running LHalo binary handler tests...\n");
    
    test_format_detection();
    test_handler_registration();
    
    // Clean up
    io_cleanup();
    
    printf("All LHalo binary handler tests passed!\n");
    return 0;
}
