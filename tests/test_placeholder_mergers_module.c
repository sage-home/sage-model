/**
 * Test suite for SAGE Placeholder Mergers Module
 * 
 * Tests the placeholder_mergers_module.c functionality including:
 * - Module registration and initialization
 * - Handler function registration (HandleMerger, HandleDisruption)
 * - Direct function calls with test data
 * - Proper stub behavior validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "core/core_allvars.h"
#include "core/core_module_system.h"
#include "core/core_module_callback.h"
#include "core/core_logging.h"
#include "core/core_merger_processor.h"
#include "physics/placeholder_mergers_module.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Test assertion macro  
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } \
} while(0)

// Test fixtures
static struct test_context {
    struct pipeline_context pipeline_ctx;
    struct params test_params; 
    struct GALAXY test_galaxies[5];
    void *module_data;
} test_ctx;

// Setup function
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize test galaxies
    for (int i = 0; i < 5; i++) {
        memset(&test_ctx.test_galaxies[i], 0, sizeof(struct GALAXY));
        test_ctx.test_galaxies[i].Type = 0;
    }
    
    // Setup pipeline context
    test_ctx.pipeline_ctx.galaxies = test_ctx.test_galaxies;
    test_ctx.pipeline_ctx.ngal = 5;
    test_ctx.pipeline_ctx.params = &test_ctx.test_params;
    
    return 0;
}

// Teardown function
static void teardown_test_context(void) {
    if (test_ctx.module_data) {
        // Call cleanup if available  
        if (placeholder_mergers_module.cleanup) {
            placeholder_mergers_module.cleanup(test_ctx.module_data);
        }
        test_ctx.module_data = NULL;
    }
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Module Registration and Basic Properties
 */
static void test_module_registration(void) {
    printf("\n=== Testing module registration ===\n");
    
    TEST_ASSERT(placeholder_mergers_module.name != NULL, "Module name should be defined");
    TEST_ASSERT(strcmp(placeholder_mergers_module.name, "PlaceholderMergersModule") == 0, 
                "Module name should be PlaceholderMergersModule");
    TEST_ASSERT(placeholder_mergers_module.type == MODULE_TYPE_MERGERS, 
                "Module type should be MODULE_TYPE_MERGERS");
    TEST_ASSERT(placeholder_mergers_module.initialize != NULL, 
                "Module should have initialize function");
    TEST_ASSERT(placeholder_mergers_module.cleanup != NULL, 
                "Module should have cleanup function");
}

/**
 * Test: Module Initialization
 */
static void test_module_initialization(void) {
    printf("\n=== Testing module initialization ===\n");
    
    // Initialize module callback system first
    module_callback_system_initialize();
    
    // Register the module so function registration works
    module_register(&placeholder_mergers_module);
    
    // Initialize the module
    int result = placeholder_mergers_module.initialize(&test_ctx.test_params, &test_ctx.module_data);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Module initialization should succeed");
    TEST_ASSERT(test_ctx.module_data != NULL, "Module data should be allocated");
    
    // Test that functions were registered
    // We can't directly access the function registry, but we can test via module_invoke
    
    printf("Module initialization completed successfully\n");
}

/**
 * Test: HandleMerger Function via module_invoke
 */
static void test_handle_merger_function(void) {
    printf("\n=== Testing HandleMerger function ===\n");
    
    // Create test merger event
    struct merger_event test_event;
    test_event.satellite_index = 1;
    test_event.central_index = 0;
    test_event.merger_time = 0.0;
    test_event.time = 5.0;
    test_event.dt = 0.1;
    test_event.halo_nr = 100;
    test_event.step = 10;
    test_event.merger_type = 1; // Major merger
    
    // Create handler arguments
    merger_handler_args_t handler_args;
    handler_args.event = test_event;
    handler_args.pipeline_ctx = &test_ctx.pipeline_ctx;
    
    // Call HandleMerger via module_invoke
    int error_code = 0;
    int result = module_invoke(
        -1, // Core caller ID
        MODULE_TYPE_MERGERS,
        "PlaceholderMergersModule",
        "HandleMerger",
        &error_code,
        &handler_args,
        NULL
    );
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "HandleMerger invoke should succeed");
    TEST_ASSERT(error_code == 0, "HandleMerger should not report errors");
}

/**
 * Test: HandleDisruption Function via module_invoke  
 */
static void test_handle_disruption_function(void) {
    printf("\n=== Testing HandleDisruption function ===\n");
    
    // Create test disruption event
    struct merger_event test_event;
    test_event.satellite_index = 2;
    test_event.central_index = 0;
    test_event.merger_time = 1.5; // Positive for disruption
    test_event.time = 5.0;
    test_event.dt = 0.1;
    test_event.halo_nr = 200;
    test_event.step = 10;
    test_event.merger_type = 3; // Disruption
    
    // Create handler arguments
    merger_handler_args_t handler_args;
    handler_args.event = test_event;
    handler_args.pipeline_ctx = &test_ctx.pipeline_ctx;
    
    // Call HandleDisruption via module_invoke
    int error_code = 0;
    int result = module_invoke(
        -1, // Core caller ID
        MODULE_TYPE_MERGERS,
        "PlaceholderMergersModule",
        "HandleDisruption",
        &error_code,
        &handler_args,
        NULL
    );
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "HandleDisruption invoke should succeed");
    TEST_ASSERT(error_code == 0, "HandleDisruption should not report errors");
}

/**
 * Test: Invalid Function Name Handling
 */
static void test_invalid_function_handling(void) {
    printf("\n=== Testing invalid function name handling ===\n");
    
    merger_handler_args_t handler_args;
    memset(&handler_args, 0, sizeof(handler_args));
    handler_args.pipeline_ctx = &test_ctx.pipeline_ctx;
    
    // Try to call non-existent function
    int error_code = 0;
    int result = module_invoke(
        -1,
        MODULE_TYPE_MERGERS,
        "PlaceholderMergersModule",
        "NonExistentFunction",
        &error_code,
        &handler_args,
        NULL
    );
    
    TEST_ASSERT(result != MODULE_STATUS_SUCCESS, "Invalid function call should fail");
}

int main(void) {
    printf("=== SAGE Placeholder Mergers Module Tests ===\n");
    
    // Setup test environment
    if (setup_test_context() != 0) {
        printf("Failed to setup test context\n");
        return 1;
    }
    
    // Run tests
    test_module_registration();
    test_module_initialization();
    test_handle_merger_function();
    test_handle_disruption_function();
    test_invalid_function_handling();
    
    // Report results
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    
    // Cleanup
    teardown_test_context();
    module_callback_system_cleanup();
    
    // Return success if all tests passed
    return (tests_run == tests_passed) ? 0 : 1;
}
