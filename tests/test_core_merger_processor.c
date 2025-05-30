/**
 * Test suite for SAGE Core Merger Processor
 * 
 * Tests the functionality of core_process_merger_queue_agnostically(),
 * which iterates the merger event queue and dispatches events to 
 * configured physics handler functions via module_invoke.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "core/core_merger_processor.h"
#include "core/core_merger_queue.h"
#include "core/core_allvars.h"
#include "core/core_module_system.h"
#include "core/core_module_callback.h"
#include "core/core_logging.h"
#include "core/core_pipeline_system.h"

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

// Mock module IDs for testing
#define MOCK_MERGER_MODULE_ID 100
#define MOCK_DISRUPTION_MODULE_ID 101

// Test fixtures
static struct test_context {
    struct merger_event_queue merger_queue;
    struct pipeline_context pipeline_ctx;
    struct params test_params;
    struct GALAXY test_galaxies[10];
    int mock_merger_calls;
    int mock_disruption_calls;
    struct merger_event last_merger_event;
    struct merger_event last_disruption_event;
} test_ctx;

// Mock merger handler function
static int mock_handle_merger(void *args_ptr, void *context) {
    (void)context;
    
    if (args_ptr != NULL) {
        merger_handler_args_t *args = (merger_handler_args_t *)args_ptr;
        test_ctx.mock_merger_calls++;
        test_ctx.last_merger_event = args->event;
    }
    
    return 0; // Success
}

// Mock disruption handler function
static int mock_handle_disruption(void *args_ptr, void *context) {
    (void)context;
    
    merger_handler_args_t *args = (merger_handler_args_t *)args_ptr;
    test_ctx.mock_disruption_calls++;
    test_ctx.last_disruption_event = args->event;
    
    return 0; // Success
}

// Mock handlers that return errors for testing error propagation
static int mock_handle_merger_with_error(void *args_ptr, void *context) {
    (void)context;
    
    if (args_ptr != NULL) {
        merger_handler_args_t *args = (merger_handler_args_t *)args_ptr;
        test_ctx.mock_merger_calls++;
        test_ctx.last_merger_event = args->event;
    }
    
    return -1; // Simulate error
}

static int mock_handle_disruption_with_error(void *args_ptr, void *context) {
    (void)context;
    
    merger_handler_args_t *args = (merger_handler_args_t *)args_ptr;
    test_ctx.mock_disruption_calls++;
    test_ctx.last_disruption_event = args->event;
    
    return -2; // Simulate error
}

// Dummy initialize functions for validation
static int mock_merger_initialize(struct params *run_params, void **module_data) {
    (void)run_params;
    (void)module_data;
    return MODULE_STATUS_SUCCESS;
}

static int mock_disruption_initialize(struct params *run_params, void **module_data) {
    (void)run_params;
    (void)module_data;
    return MODULE_STATUS_SUCCESS;
}

// Mock module definitions
static struct base_module mock_merger_module = {
    .name = "MockMergerModule",
    .type = MODULE_TYPE_MERGERS,
    .version = "1.0.0",
    .author = "Test Suite",
    .module_id = -1, // Let module_register() assign this dynamically
    .initialize = mock_merger_initialize,
    .cleanup = NULL,
    .configure = NULL,
    .execute_post_phase = NULL,
    .phases = 0
};

static struct base_module mock_disruption_module = {
    .name = "MockDisruptionModule", 
    .type = MODULE_TYPE_MERGERS,
    .version = "1.0.0",
    .author = "Test Suite",
    .module_id = -1, // Let module_register() assign this dynamically
    .initialize = mock_disruption_initialize,
    .cleanup = NULL,
    .configure = NULL,
    .execute_post_phase = NULL,
    .phases = 0
};

// Additional mock modules for error testing
static struct base_module mock_error_merger_module = {
    .name = "MockErrorMergerModule",
    .type = MODULE_TYPE_MERGERS,
    .version = "1.0.0",
    .author = "Test Suite",
    .module_id = -1, // Let module_register() assign this dynamically
    .initialize = mock_merger_initialize,
    .cleanup = NULL,
    .configure = NULL,
    .execute_post_phase = NULL,
    .phases = 0
};

static struct base_module mock_error_disruption_module = {
    .name = "MockErrorDisruptionModule", 
    .type = MODULE_TYPE_MERGERS,
    .version = "1.0.0",
    .author = "Test Suite",
    .module_id = -1, // Let module_register() assign this dynamically
    .initialize = mock_disruption_initialize,
    .cleanup = NULL,
    .configure = NULL,
    .execute_post_phase = NULL,
    .phases = 0
};

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize merger queue
    init_merger_queue(&test_ctx.merger_queue);
    
    // Initialize test galaxies
    for (int i = 0; i < 10; i++) {
        memset(&test_ctx.test_galaxies[i], 0, sizeof(struct GALAXY));
        test_ctx.test_galaxies[i].Type = 0; // Central
    }
    
    // Setup pipeline context
    test_ctx.pipeline_ctx.galaxies = test_ctx.test_galaxies;
    test_ctx.pipeline_ctx.ngal = 10;
    test_ctx.pipeline_ctx.params = &test_ctx.test_params;
    test_ctx.pipeline_ctx.merger_queue = &test_ctx.merger_queue;
    
    // Setup runtime parameters for merger handling
    strncpy(test_ctx.test_params.runtime.MergerHandlerModuleName, "MockMergerModule", MAX_STRING_LEN-1);
    test_ctx.test_params.runtime.MergerHandlerModuleName[MAX_STRING_LEN-1] = '\0'; // Ensure null termination
    strncpy(test_ctx.test_params.runtime.MergerHandlerFunctionName, "HandleMerger", MAX_STRING_LEN-1);
    test_ctx.test_params.runtime.MergerHandlerFunctionName[MAX_STRING_LEN-1] = '\0'; // Ensure null termination
    strncpy(test_ctx.test_params.runtime.DisruptionHandlerModuleName, "MockDisruptionModule", MAX_STRING_LEN-1);
    test_ctx.test_params.runtime.DisruptionHandlerModuleName[MAX_STRING_LEN-1] = '\0'; // Ensure null termination
    strncpy(test_ctx.test_params.runtime.DisruptionHandlerFunctionName, "HandleDisruption", MAX_STRING_LEN-1);
    test_ctx.test_params.runtime.DisruptionHandlerFunctionName[MAX_STRING_LEN-1] = '\0'; // Ensure null termination
    
    // CRITICAL FIX: Disable module discovery to prevent initialization failure
    test_ctx.test_params.runtime.EnableModuleDiscovery = 0;
    
    return 0;
}

static int complete_setup(void) {
    // Initialize just what we need for testing - skip full module system initialization
    
    // Initialize module callback system
    module_callback_system_initialize();
    
    // Register mock modules
    module_register(&mock_merger_module);
    module_register(&mock_disruption_module);
    module_register(&mock_error_merger_module);
    module_register(&mock_error_disruption_module);
    
    // Register normal mock handler functions using the assigned module IDs
    int status1 = module_register_function(
        mock_merger_module.module_id,  // Use assigned ID, not predefined constant
        "HandleMerger",
        (void *)mock_handle_merger,
        FUNCTION_TYPE_INT,
        "int (void*, void*)",
        "Mock merger handler for testing"
    );
    
    int status2 = module_register_function(
        mock_disruption_module.module_id,  // Use assigned ID, not predefined constant
        "HandleDisruption", 
        (void *)mock_handle_disruption,
        FUNCTION_TYPE_INT,
        "int (void*, void*)",
        "Mock disruption handler for testing"
    );
    
    // Register error-generating mock handler functions
    int status3 = module_register_function(
        mock_error_merger_module.module_id,
        "HandleMerger",
        (void *)mock_handle_merger_with_error,
        FUNCTION_TYPE_INT,
        "int (void*, void*)",
        "Mock merger handler that returns errors"
    );
    
    int status4 = module_register_function(
        mock_error_disruption_module.module_id,
        "HandleDisruption", 
        (void *)mock_handle_disruption_with_error,
        FUNCTION_TYPE_INT,
        "int (void*, void*)",
        "Mock disruption handler that returns errors"
    );
    
    if (status1 != MODULE_STATUS_SUCCESS || status2 != MODULE_STATUS_SUCCESS ||
        status3 != MODULE_STATUS_SUCCESS || status4 != MODULE_STATUS_SUCCESS) {
        printf("Failed to register mock handler functions\n");
        return -1;
    }
    
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    module_callback_system_cleanup();
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Empty Queue Processing
 */
static void test_empty_queue_processing(void) {
    printf("\n=== Testing empty queue processing ===\n");
    
    // Ensure queue is empty
    init_merger_queue(&test_ctx.merger_queue);
    TEST_ASSERT(test_ctx.merger_queue.num_events == 0, "Queue should start empty");
    
    // Reset mock call counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process empty queue
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Processing empty queue should succeed");
    TEST_ASSERT(test_ctx.mock_merger_calls == 0, "No merger handlers should be called for empty queue");
    TEST_ASSERT(test_ctx.mock_disruption_calls == 0, "No disruption handlers should be called for empty queue");
}

/**
 * Test: Single Merger Event Processing
 */
static void test_single_merger_event(void) {
    printf("\n=== Testing single merger event processing ===\n");
    
    // Initialize queue and add a merger event
    init_merger_queue(&test_ctx.merger_queue);
    queue_merger_event(&test_ctx.merger_queue, 1, 0, 0.0, 5.0, 0.1, 100, 10, 1);
    
    TEST_ASSERT(test_ctx.merger_queue.num_events == 1, "Queue should have 1 event");
    
    // Reset mock call counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process queue
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Processing single merger should succeed");
    TEST_ASSERT(test_ctx.mock_merger_calls == 1, "Merger handler should be called once");
    TEST_ASSERT(test_ctx.mock_disruption_calls == 0, "Disruption handler should not be called");
    
    // Verify event details were passed correctly
    TEST_ASSERT(test_ctx.last_merger_event.satellite_index == 1, "Satellite index should be correct");
    TEST_ASSERT(test_ctx.last_merger_event.central_index == 0, "Central index should be correct");
    TEST_ASSERT(test_ctx.last_merger_event.merger_time == 0.0, "Merger time should be correct");
    
    // Queue should be cleared after processing
    TEST_ASSERT(test_ctx.merger_queue.num_events == 0, "Queue should be empty after processing");
}

/**
 * Test: Single Disruption Event Processing  
 */
static void test_single_disruption_event(void) {
    printf("\n=== Testing single disruption event processing ===\n");
    
    // Initialize queue and add a disruption event (merger_time > 0.0)
    init_merger_queue(&test_ctx.merger_queue);
    queue_merger_event(&test_ctx.merger_queue, 2, 0, 1.5, 5.0, 0.1, 100, 10, 3);
    
    TEST_ASSERT(test_ctx.merger_queue.num_events == 1, "Queue should have 1 event");
    
    // Reset mock call counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process queue
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Processing single disruption should succeed");
    TEST_ASSERT(test_ctx.mock_merger_calls == 0, "Merger handler should not be called");
    TEST_ASSERT(test_ctx.mock_disruption_calls == 1, "Disruption handler should be called once");
    
    // Verify event details
    TEST_ASSERT(test_ctx.last_disruption_event.satellite_index == 2, "Satellite index should be correct");
    TEST_ASSERT(test_ctx.last_disruption_event.merger_time == 1.5, "Merger time should be correct");
    
    // Queue should be cleared
    TEST_ASSERT(test_ctx.merger_queue.num_events == 0, "Queue should be empty after processing");
}

/**
 * Test: Multiple Mixed Events Processing
 */
static void test_multiple_mixed_events(void) {
    printf("\n=== Testing multiple mixed events processing ===\n");
    
    // Initialize queue and add multiple events
    init_merger_queue(&test_ctx.merger_queue);
    queue_merger_event(&test_ctx.merger_queue, 1, 0, 0.0, 5.0, 0.1, 100, 10, 1); // Merger
    queue_merger_event(&test_ctx.merger_queue, 2, 0, 1.5, 5.0, 0.1, 100, 10, 3); // Disruption  
    queue_merger_event(&test_ctx.merger_queue, 3, 1, 0.0, 5.0, 0.1, 100, 10, 2); // Merger
    
    TEST_ASSERT(test_ctx.merger_queue.num_events == 3, "Queue should have 3 events");
    
    // Reset counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process queue
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Processing multiple events should succeed");
    TEST_ASSERT(test_ctx.mock_merger_calls == 2, "Should call merger handler twice");
    TEST_ASSERT(test_ctx.mock_disruption_calls == 1, "Should call disruption handler once");
    TEST_ASSERT(test_ctx.merger_queue.num_events == 0, "Queue should be empty after processing");
}

/**
 * Test: Invalid Galaxy Indices Handling
 */
static void test_invalid_galaxy_indices(void) {
    printf("\n=== Testing invalid galaxy indices handling ===\n");
    
    // Initialize queue with invalid indices
    init_merger_queue(&test_ctx.merger_queue);
    queue_merger_event(&test_ctx.merger_queue, 99, 0, 0.0, 5.0, 0.1, 100, 10, 1); // Invalid satellite
    queue_merger_event(&test_ctx.merger_queue, 1, 99, 0.0, 5.0, 0.1, 100, 10, 1);  // Invalid central
    
    // Reset counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process queue - should handle invalid indices gracefully
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Should handle invalid indices gracefully");
    TEST_ASSERT(test_ctx.mock_merger_calls == 0, "No handlers should be called for invalid indices");
    TEST_ASSERT(test_ctx.mock_disruption_calls == 0, "No handlers should be called for invalid indices");
}

/**
 * Test: Null Pipeline Context Handling
 */
static void test_null_pipeline_context(void) {
    printf("\n=== Testing null pipeline context handling ===\n");
    
    int result = core_process_merger_queue_agnostically(NULL);
    TEST_ASSERT(result == -1, "Should return error for null pipeline context");
}

/**
 * Test: Configuration Error Handling
 */
static void test_configuration_errors(void) {
    printf("\n=== Testing configuration error handling ===\n");
    
    // Initialize queue with a merger event
    init_merger_queue(&test_ctx.merger_queue);
    queue_merger_event(&test_ctx.merger_queue, 1, 0, 0.0, 5.0, 0.1, 100, 10, 1);
    
    // Test with missing handler module name
    struct params backup_params;
    memcpy(&backup_params, &test_ctx.test_params, sizeof(struct params));
    
    // Clear handler module name
    test_ctx.test_params.runtime.MergerHandlerModuleName[0] = '\0';
    
    // Reset counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process queue - should handle missing configuration gracefully
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Should handle missing module name gracefully");
    TEST_ASSERT(test_ctx.mock_merger_calls == 0, "No handlers should be called with invalid config");
    
    // Restore configuration
    memcpy(&test_ctx.test_params, &backup_params, sizeof(struct params));
}

/**
 * Test: Error Propagation from Physics Handlers
 */
static void test_error_propagation(void) {
    printf("\n=== Testing error propagation from physics handlers ===\n");
    
    // Configure to use error-generating handlers
    struct params backup_params;
    memcpy(&backup_params, &test_ctx.test_params, sizeof(struct params));
    
    strncpy(test_ctx.test_params.runtime.MergerHandlerModuleName, "MockErrorMergerModule", MAX_STRING_LEN-1);
    strncpy(test_ctx.test_params.runtime.DisruptionHandlerModuleName, "MockErrorDisruptionModule", MAX_STRING_LEN-1);
    
    // Initialize queue with mixed events
    init_merger_queue(&test_ctx.merger_queue);
    queue_merger_event(&test_ctx.merger_queue, 1, 0, 0.0, 5.0, 0.1, 100, 10, 1); // Merger
    queue_merger_event(&test_ctx.merger_queue, 2, 0, 1.5, 5.0, 0.1, 100, 10, 3); // Disruption
    
    // Reset counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process queue - should continue despite handler errors
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Should succeed despite handler errors");
    TEST_ASSERT(test_ctx.mock_merger_calls == 1, "Error handler should still be called once");
    TEST_ASSERT(test_ctx.mock_disruption_calls == 1, "Error handler should still be called once");
    TEST_ASSERT(test_ctx.merger_queue.num_events == 0, "Queue should be cleared even with handler errors");
    
    // Restore configuration
    memcpy(&test_ctx.test_params, &backup_params, sizeof(struct params));
}

/**
 * Test: Different Handler Configurations
 */
static void test_different_configurations(void) {
    printf("\n=== Testing different handler configurations ===\n");
    
    struct params backup_params;
    memcpy(&backup_params, &test_ctx.test_params, sizeof(struct params));
    
    // Test configuration 1: Same module for both merger and disruption
    strncpy(test_ctx.test_params.runtime.MergerHandlerModuleName, "MockMergerModule", MAX_STRING_LEN-1);
    strncpy(test_ctx.test_params.runtime.DisruptionHandlerModuleName, "MockMergerModule", MAX_STRING_LEN-1);
    strncpy(test_ctx.test_params.runtime.DisruptionHandlerFunctionName, "HandleMerger", MAX_STRING_LEN-1);
    
    // Initialize queue with both types of events
    init_merger_queue(&test_ctx.merger_queue);
    queue_merger_event(&test_ctx.merger_queue, 1, 0, 0.0, 5.0, 0.1, 100, 10, 1); // Merger
    queue_merger_event(&test_ctx.merger_queue, 2, 0, 1.5, 5.0, 0.1, 100, 10, 3); // Disruption
    
    // Reset counters
    test_ctx.mock_merger_calls = 0;
    test_ctx.mock_disruption_calls = 0;
    
    // Process queue
    int result = core_process_merger_queue_agnostically(&test_ctx.pipeline_ctx);
    
    TEST_ASSERT(result == MODULE_STATUS_SUCCESS, "Should handle unified handler configuration");
    TEST_ASSERT(test_ctx.mock_merger_calls == 2, "Same handler should be called for both event types");
    TEST_ASSERT(test_ctx.mock_disruption_calls == 0, "Disruption handler should not be called");
    
    // Restore configuration
    memcpy(&test_ctx.test_params, &backup_params, sizeof(struct params));
}

int main(void) {
    printf("\n========================================\n");
    printf("Starting tests for test_core_merger_processor\n");
    printf("========================================\n\n");
    
    // Setup test environment
    if (setup_test_context() != 0) {
        printf("Failed to setup test context\n");
        return 1;
    }
    
    if (complete_setup() != 0) {
        printf("Failed to complete test setup\n");
        return 1;
    }
    
    // Run all tests
    test_empty_queue_processing();
    test_single_merger_event();
    test_single_disruption_event();
    test_multiple_mixed_events();
    test_invalid_galaxy_indices();
    test_null_pipeline_context();
    test_configuration_errors();
    test_error_propagation();
    test_different_configurations();
    
    // Cleanup
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_core_merger_processor:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    // Return success if all tests passed
    return (tests_run == tests_passed) ? 0 : 1;
}
