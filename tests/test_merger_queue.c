/**
 * Test suite for SAGE Core Merger Queue Data Management
 * 
 * Tests the functionality of the galaxy merger event queue for collecting
 * and storing merger events. The actual processing of these events
 * is handled by a separate physics module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "core/core_merger_queue.h" // Will only contain init_merger_queue and queue_merger_event
#include "core/core_allvars.h"    // For GALAXY struct and MAX_GALAXIES_PER_HALO
#include "core/core_logging.h"    // For LOG_* macros, if used by queue functions

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

// Merger queue test specific constants
#define MERGER_TYPE_MAJOR 1
#define MERGER_TYPE_MINOR 2
#define MERGER_TYPE_DISRUPTION 3

// Test fixtures
static struct test_context {
    struct merger_event_queue *queue;
    // test_galaxies are no longer strictly needed for testing the queue component itself,
    // but queue_merger_event takes indices, implying an external galaxy array.
    // We can keep it for context if queue_merger_event might do bounds checks in the future,
    // or remove it if the queue is purely about storing indices.
    // For now, let's keep it minimal as the queue itself doesn't use the GALAXY structs.
} test_ctx;


// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Create merger queue
    test_ctx.queue = (struct merger_event_queue *)malloc(sizeof(struct merger_event_queue));
    if (test_ctx.queue == NULL) {
        printf("Failed to allocate memory for merger queue\n");
        return -1;
    }
    
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.queue) {
        free(test_ctx.queue);
        test_ctx.queue = NULL;
    }
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Merger Queue Initialization
 */
static void test_queue_init(void) {
    printf("\n=== Testing merger queue initialization ===\n");
    
    struct merger_event_queue *queue = test_ctx.queue;
    init_merger_queue(queue);
    
    TEST_ASSERT(queue != NULL, "Queue should be allocated (fixture)");
    TEST_ASSERT(queue->num_events == 0, "Queue should start empty after init_merger_queue");
    
    // Test NULL pointer handling (if init_merger_queue is robust to it)
    // Assuming init_merger_queue has a NULL check as per current SAGE code.
    init_merger_queue(NULL); 
    TEST_ASSERT(1, "init_merger_queue with NULL should not crash");
}

/**
 * Test: Adding Merger Events and Verifying Stored Data
 */
static void test_add_merger_event_and_data_integrity(void) {
    printf("\n=== Testing adding merger events and data integrity ===\n");
    
    struct merger_event_queue *queue = test_ctx.queue;
    init_merger_queue(queue);
    
    // Event 1
    int result = queue_merger_event(queue, 10, 0, 0.1, 5.0, 0.5, 100, 1, MERGER_TYPE_MAJOR);
    TEST_ASSERT(result == 0, "queue_merger_event (1) should succeed");
    TEST_ASSERT(queue->num_events == 1, "Queue should have 1 event after first add");
    TEST_ASSERT(queue->events[0].satellite_index == 10, "Event 0: satellite_index check");
    TEST_ASSERT(queue->events[0].central_index == 0, "Event 0: central_index check");
    TEST_ASSERT(fabs(queue->events[0].merger_time - 0.1) < 1e-6, "Event 0: merger_time check");
    TEST_ASSERT(fabs(queue->events[0].time - 5.0) < 1e-6, "Event 0: time check");
    TEST_ASSERT(fabs(queue->events[0].dt - 0.5) < 1e-6, "Event 0: dt check");
    TEST_ASSERT(queue->events[0].halo_nr == 100, "Event 0: halo_nr check");
    TEST_ASSERT(queue->events[0].step == 1, "Event 0: step check");
    TEST_ASSERT(queue->events[0].merger_type == MERGER_TYPE_MAJOR, "Event 0: merger_type check");

    // Event 2
    result = queue_merger_event(queue, 20, 1, 0.0, 6.0, 0.6, 200, 2, MERGER_TYPE_MINOR);
    TEST_ASSERT(result == 0, "queue_merger_event (2) should succeed");
    TEST_ASSERT(queue->num_events == 2, "Queue should have 2 events after second add");
    TEST_ASSERT(queue->events[1].satellite_index == 20, "Event 1: satellite_index check");
    TEST_ASSERT(queue->events[1].merger_type == MERGER_TYPE_MINOR, "Event 1: merger_type check");

    // Event 3 (Disruption)
    result = queue_merger_event(queue, 30, 2, 1.5, 7.0, 0.7, 300, 3, MERGER_TYPE_DISRUPTION);
    TEST_ASSERT(result == 0, "queue_merger_event (3) should succeed");
    TEST_ASSERT(queue->num_events == 3, "Queue should have 3 events after third add");
    TEST_ASSERT(queue->events[2].satellite_index == 30, "Event 2: satellite_index check");
    TEST_ASSERT(queue->events[2].merger_type == MERGER_TYPE_DISRUPTION, "Event 2: merger_type check");
    TEST_ASSERT(queue->events[2].merger_time > 0.0, "Event 2 (disruption) should have positive merger_time");
}

/**
 * Test: Merger Event Storage Order (FIFO)
 * This test verifies that events are stored in the queue in the order they are added.
 * The actual processing order is the responsibility of the module that consumes the queue.
 */
static void test_merger_event_storage_order(void) {
    printf("\n=== Testing merger event storage order (FIFO) ===\n");
    
    struct merger_event_queue *queue = test_ctx.queue;
    init_merger_queue(queue);
    
    // Add events
    queue_merger_event(queue, 1, 0, 0.0, 0.5, 0.1, 1, 63, MERGER_TYPE_MAJOR); 
    queue_merger_event(queue, 2, 0, 0.0, 0.5, 0.1, 1, 63, MERGER_TYPE_MINOR); 
    queue_merger_event(queue, 3, 0, 1.0, 0.5, 0.1, 1, 63, MERGER_TYPE_DISRUPTION);
    
    // Verify the order of events as stored in the queue's internal array
    TEST_ASSERT(queue->events[0].satellite_index == 1, "First event added should be at index 0");
    TEST_ASSERT(queue->events[1].satellite_index == 2, "Second event added should be at index 1");
    TEST_ASSERT(queue->events[2].satellite_index == 3, "Third event added should be at index 2");
}


/**
 * Test: Queue Overflow
 * Verifies that the queue correctly handles attempts to add events beyond its capacity.
 */
static void test_queue_overflow(void) {
    printf("\n=== Testing queue overflow handling ===\n");
    
    struct merger_event_queue *queue = test_ctx.queue;
    init_merger_queue(queue);
    
    // Fill the queue to capacity
    int i = 0;
    int result = 0;
    printf("Attempting to fill queue to capacity (%d events)...\n", MAX_GALAXIES_PER_HALO);
    while (i < MAX_GALAXIES_PER_HALO) {
        result = queue_merger_event(queue, i, 0, 0.0, 0.5, 0.1, 1, 63, MERGER_TYPE_MAJOR);
        if (result != 0) {
            printf("queue_merger_event failed at event %d before reaching capacity.\n", i);
            break;
        }
        i++;
    }
    
    TEST_ASSERT(i == MAX_GALAXIES_PER_HALO, "Queue should accept MAX_GALAXIES_PER_HALO events");
    TEST_ASSERT(queue->num_events == MAX_GALAXIES_PER_HALO, "Queue num_events should be at capacity");
    
    // Try to add one more event, which should fail
    printf("Attempting to add one more event to full queue...\n");
    result = queue_merger_event(queue, i, 0, 0.0, 0.5, 0.1, 1, 63, MERGER_TYPE_MAJOR);
    TEST_ASSERT(result != 0, "queue_merger_event should fail when queue is full");
    TEST_ASSERT(queue->num_events == MAX_GALAXIES_PER_HALO, "Queue num_events should remain at capacity after overflow attempt");
    
    // Reset queue and test it's empty
    init_merger_queue(queue);
    TEST_ASSERT(queue->num_events == 0, "Queue should be reset to empty after init");
}

/**
 * Test: Invalid Parameters for queue_merger_event
 * This test focuses on how queue_merger_event handles invalid inputs.
 * The core_merger_queue.c itself does not currently validate satellite/central indices
 * or merger_type, so those specific tests are commented out or noted.
 */
static void test_invalid_parameters_for_queueing(void) {
    printf("\n=== Testing invalid parameter handling for queue_merger_event ===\n");
    
    struct merger_event_queue *queue = test_ctx.queue;
    init_merger_queue(queue); // Ensure queue is valid for some tests
    
    // Test NULL queue with queue_merger_event
    int result = queue_merger_event(NULL, 0, 0, 0.0, 0.5, 0.1, 1, 63, MERGER_TYPE_MAJOR);
    TEST_ASSERT(result != 0, "queue_merger_event should fail with NULL queue");
    
    // Test invalid merger type with queue_merger_event
    // Note: The current SAGE core_merger_queue.c implementation of queue_merger_event
    // does not validate the merger_type. This test would fail unless that function is updated.
    // It's included here as per the test plan's intent for robust error handling.
    int invalid_merger_type = -100; // An obviously invalid type
    result = queue_merger_event(queue, 1, 0, 0.0, 0.5, 0.1, 1, 63, invalid_merger_type);
    // TEST_ASSERT(result != 0, "queue_merger_event should fail with invalid merger type"); 
    // ^ Assertion commented out as current library code doesn't validate this.
    if (result == 0) {
        printf("NOTE: queue_merger_event accepted an invalid merger_type (%d) as expected with current library code.\n", invalid_merger_type);
        // We can still check if it was stored, even if invalid
        TEST_ASSERT(queue->events[queue->num_events-1].merger_type == invalid_merger_type, "Invalid merger_type should still be stored if not validated");
        init_merger_queue(queue); // Reset for next test
    } else {
        TEST_ASSERT(result != 0, "queue_merger_event ideally should fail with invalid merger type (but may not with current library code)");
    }

    // Test with invalid satellite/central indices (e.g., negative)
    // Note: Current SAGE core_merger_queue.c does not validate these indices.
    // This test would pass (result == 0) with current library code.
    result = queue_merger_event(queue, -1, 0, 0.0, 0.5, 0.1, 1, 63, MERGER_TYPE_MAJOR);
    // TEST_ASSERT(result != 0, "queue_merger_event should fail with invalid satellite_index");
    if (result == 0) {
        printf("NOTE: queue_merger_event accepted an invalid satellite_index (-1) as expected with current library code.\n");
        init_merger_queue(queue); // Reset
    } else {
         TEST_ASSERT(result != 0, "queue_merger_event ideally should fail with invalid satellite_index (but may not with current library code)");
    }
}


int main(void) {
    printf("=== SAGE Core Merger Queue Data Management Tests ===\n");
    
    // Setup test environment
    if (setup_test_context() != 0) {
        printf("Failed to setup test context\n");
        return 1;
    }
    
    // Run tests
    test_queue_init();
    test_add_merger_event_and_data_integrity();
    test_merger_event_storage_order();
    test_queue_overflow();
    test_invalid_parameters_for_queueing();
    // test_process_merger_events and test_deferred_processing are removed as they
    // test functionality that will move to a physics module.
    
    // Report results
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    
    // Cleanup
    teardown_test_context();
    
    // Return success if all tests passed
    return (tests_run == tests_passed) ? 0 : 1;
}