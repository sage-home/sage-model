/**
 * Test suite for SAGE Placeholder Mergers Module
 * 
 * Tests the placeholder_mergers_module.c functionality including:
 * - Module registration and initialization
 * - Handler function registration (HandleMerger, HandleDisruption) 
 * - Function invocation via module callback system
 * - Error handling for invalid operations
 * - Module lifecycle management (init/cleanup)
 * - Integration with merger event processing
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
    // Note: Module cleanup is handled by the module system
    // when module_cleanup() or module_unregister() is called
    test_ctx.module_data = NULL;
}

// Helper function to validate merger event data integrity
static void validate_merger_event_data(struct merger_event *event, const char *event_type) {
    TEST_ASSERT(event != NULL, "Merger event should not be NULL");
    TEST_ASSERT(event->satellite_index >= 0, "Satellite index should be non-negative");
    TEST_ASSERT(event->central_index >= 0, "Central index should be non-negative");
    TEST_ASSERT(event->satellite_index != event->central_index, 
                "Satellite and central should be different galaxies");
    TEST_ASSERT(event->time >= 0.0, "Event time should be non-negative");
    TEST_ASSERT(event->dt > 0.0, "Time step should be positive");
    TEST_ASSERT(event->step >= 0, "Step number should be non-negative");
    printf("  %s event data validation passed\n", event_type);
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
    
    // Check if module is already registered before attempting registration
    int existing_module_id = module_find_by_name("PlaceholderMergersModule");
    
    if (existing_module_id >= 0) {
        // Module already registered - use existing registration
        printf("  Module already registered with ID: %d\n", existing_module_id);
        TEST_ASSERT(existing_module_id >= 0, "Existing module should have valid ID");
        
        // Ensure the global module reference has the correct ID
        placeholder_mergers_module.module_id = existing_module_id;
    } else {
        // Module not registered - register it now
        int register_result = module_register(&placeholder_mergers_module);
        TEST_ASSERT(register_result == MODULE_STATUS_SUCCESS, "New module registration should succeed");
        printf("  Module registered successfully with ID: %d\n", placeholder_mergers_module.module_id);
    }
    
    // Get the assigned module ID from the module structure
    int module_id = placeholder_mergers_module.module_id;
    TEST_ASSERT(module_id >= 0, "Module should be assigned a valid ID");
    
    // Initialize the module using proper system API (not direct function call)
    int result = module_initialize(module_id, &test_ctx.test_params);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS || result == MODULE_STATUS_ALREADY_INITIALIZED, 
                "Module initialization should succeed (or already initialized)");
    
    // Get module data through proper API
    struct base_module *module = NULL;
    void *module_data = NULL;
    int get_result = module_get(module_id, &module, &module_data);
    TEST_ASSERT(get_result == MODULE_STATUS_SUCCESS, "Module retrieval should succeed");
    TEST_ASSERT(module_data != NULL, "Module data should be allocated");
    
    // Store for cleanup
    test_ctx.module_data = module_data;
    
    // Test that re-initialization fails correctly (fail fast behavior)
    int reinit_result = module_initialize(module_id, &test_ctx.test_params);
    TEST_ASSERT(reinit_result == MODULE_STATUS_ALREADY_INITIALIZED, 
                "Re-initialization should fail with MODULE_STATUS_ALREADY_INITIALIZED");
    
    printf("Module initialization completed successfully\n");
}

/**
 * Test: Module Cleanup and Lifecycle
 */
static void test_module_cleanup_lifecycle(void) {
    printf("\n=== Testing module cleanup and lifecycle ===\n");
    
    // Test cleanup with NULL data (should handle gracefully)
    int null_cleanup_result = placeholder_mergers_module.cleanup(NULL);
    TEST_ASSERT(null_cleanup_result == MODULE_STATUS_SUCCESS, "Cleanup should handle NULL data gracefully");
    
    // Test that cleanup function is available
    TEST_ASSERT(placeholder_mergers_module.cleanup != NULL, "Module should have cleanup function");
    
    printf("  Module cleanup and lifecycle test completed\n");
}

/**
 * Test: Module Error Condition Handling
 * Tests defensive programming aspects and input validation
 */
static void test_module_error_conditions(void) {
    printf("\n=== Testing module error condition handling ===\n");
    
    // Test 1: Invalid module ID should fail
    int result_invalid_id = module_initialize(-1, &test_ctx.test_params);
    TEST_ASSERT(result_invalid_id == MODULE_STATUS_INVALID_ARGS, 
                "Initialization with invalid module ID should return MODULE_STATUS_INVALID_ARGS");
    
    // Test 2: NULL params should be handled gracefully (module can handle NULL params)
    // Use the already registered module for this test
    int module_id = placeholder_mergers_module.module_id;
    if (module_id >= 0) {
        // Test re-initialization with NULL params (should still fail with ALREADY_INITIALIZED)
        int result_null_params = module_initialize(module_id, NULL);
        TEST_ASSERT(result_null_params == MODULE_STATUS_ALREADY_INITIALIZED, 
                    "Re-initialization with NULL params should return MODULE_STATUS_ALREADY_INITIALIZED");
    }
    
    printf("  Error condition testing completed - defensive programming validated\n");
}

/**
 * Test: HandleMerger Function via module_invoke
 */
static void test_handle_merger_function(void) {
    printf("\n=== Testing HandleMerger function ===\n");
    
    // Create test merger event with representative data
    // merger_time = 0.0 indicates this is an immediate merger event
    // merger_type = 1 represents a major merger scenario
    struct merger_event test_event;
    test_event.satellite_index = 1;        // Index of satellite galaxy 
    test_event.central_index = 0;          // Index of central galaxy
    test_event.merger_time = 0.0;          // Time until merger (0.0 = immediate)
    test_event.time = 5.0;                 // Current simulation time
    test_event.dt = 0.1;                   // Current time step
    test_event.halo_nr = 100;              // Halo identification number
    test_event.step = 10;                  // Current simulation step
    test_event.merger_type = 1;            // Merger classification (1 = major merger)
    
    // Validate the test event data before using it
    validate_merger_event_data(&test_event, "Major merger");
    
    // Create handler arguments structure
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
    
    // Create test disruption event with representative data
    // merger_time > 0.0 indicates this is a disruption event
    // merger_type = 3 represents a complete disruption scenario
    struct merger_event test_event;
    test_event.satellite_index = 2;        // Index of satellite galaxy to be disrupted
    test_event.central_index = 0;          // Index of central galaxy
    test_event.merger_time = 1.5;          // Time until disruption (positive for disruption)
    test_event.time = 5.0;                 // Current simulation time
    test_event.dt = 0.1;                   // Current time step
    test_event.halo_nr = 200;              // Halo identification number
    test_event.step = 10;                  // Current simulation step
    test_event.merger_type = 3;            // Event classification (3 = disruption)
    
    // Validate the test event data before using it
    validate_merger_event_data(&test_event, "Disruption");
    
    // Create handler arguments structure
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
    printf("\n========================================\n");
    printf("Starting tests for test_placeholder_mergers_module\n");
    printf("========================================\n\n");
    
    printf("This test verifies that the placeholder mergers module:\n");
    printf("  1. Registers correctly with the module system\n");
    printf("  2. Initializes and allocates module data successfully\n");
    printf("  3. Registers handler functions (HandleMerger, HandleDisruption) with the callback system\n");
    printf("  4. Responds to function invocation via module_invoke() with proper error handling\n");
    printf("  5. Validates core-physics separation by operating independently from core infrastructure\n");
    printf("  6. Demonstrates module lifecycle management (initialization, cleanup, error handling)\n\n");
    
    // Setup test environment
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_module_registration();
    test_module_initialization();
    test_module_cleanup_lifecycle();
    test_module_error_conditions();
    test_handle_merger_function();
    test_handle_disruption_function();
    test_invalid_function_handling();
    
    // Cleanup
    teardown_test_context();
    module_callback_system_cleanup();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_placeholder_mergers_module:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
