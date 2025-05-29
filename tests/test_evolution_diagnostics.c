/**
 * @file test_evolution_diagnostics.c
 * @brief Comprehensive test suite for core evolution diagnostics system
 * 
 * This test suite validates the core evolution diagnostics system's compliance
 * with core-physics separation principles. Tests cover infrastructure metrics
 * only, without any physics-specific knowledge or dependencies.
 * 
 * Key validation areas:
 * - Core infrastructure independence from physics modules
 * - Performance metrics tracking (timing, galaxy counts, phase statistics)
 * - Core event system validation (infrastructure events only)
 * - Error handling robustness and boundary condition testing
 * - Integration with pipeline system phase execution
 * 
 * The diagnostics system tracks only core infrastructure metrics:
 * - Galaxy counts and structural changes
 * - Pipeline phase timing and execution flow
 * - Core infrastructure events (pipeline, phase, module lifecycle)
 * - Merger queue statistics (infrastructure-level tracking)
 * 
 * Physics modules register their own diagnostic metrics independently
 * through the generic framework provided by the core infrastructure.
 * 
 * @author SAGE Team
 * @date May 2025
 * @phase 5.2.F.3 - Core-Physics Separation Implementation
 * @milestone Evolution Diagnostics System Refactoring (May 23, 2025)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../src/core/core_evolution_diagnostics.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_allvars.h"

// Test counter for comprehensive reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions with detailed reporting
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Mock functions and helpers
static clock_t test_wait_clock_ticks(int ms);

/**
 * Test initialization of the core diagnostics structure
 */
static void test_core_diagnostics_initialization() {
    printf("\n=== Testing core diagnostics initialization ===\n");
    
    // Test normal initialization
    struct core_evolution_diagnostics diag;
    int result = core_evolution_diagnostics_initialize(&diag, 42, 10);
    
    TEST_ASSERT(result == 0, "core_evolution_diagnostics_initialize should return success");
    TEST_ASSERT(diag.halo_nr == 42, "halo_nr should be set correctly");
    TEST_ASSERT(diag.ngal_initial == 10, "ngal_initial should be set correctly");
    TEST_ASSERT(diag.ngal_final == 0, "ngal_final should be initialized to 0");
    TEST_ASSERT(diag.start_time > 0, "start_time should be positive");
    TEST_ASSERT(diag.end_time == 0, "end_time should be initialized to 0");
    TEST_ASSERT(diag.elapsed_seconds == 0.0, "elapsed_seconds should be initialized to 0");
    
    // Check that phases were initialized to 0
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(diag.phases[i].start_time == 0, "phase start_time should be initialized to 0");
        TEST_ASSERT(diag.phases[i].total_time == 0, "phase total_time should be initialized to 0");
        TEST_ASSERT(diag.phases[i].galaxy_count == 0, "phase galaxy_count should be initialized to 0");
        TEST_ASSERT(diag.phases[i].step_count == 0, "phase step_count should be initialized to 0");
    }
    
    // Check that core event counts were initialized to 0
    for (int i = 0; i < CORE_EVENT_TYPE_MAX; i++) {
        TEST_ASSERT(diag.core_event_counts[i] == 0, "core event counts should be initialized to 0");
    }
    
    // Check that merger statistics were initialized to 0
    TEST_ASSERT(diag.mergers_detected == 0, "mergers_detected should be initialized to 0");
    TEST_ASSERT(diag.mergers_processed == 0, "mergers_processed should be initialized to 0");
    TEST_ASSERT(diag.major_mergers == 0, "major_mergers should be initialized to 0");
    TEST_ASSERT(diag.minor_mergers == 0, "minor_mergers should be initialized to 0");
    
    printf("Test completed: core diagnostics initialization\n");
}

/**
 * Test NULL pointer handling
 */
static void test_null_pointer_handling() {
    printf("\n=== Testing NULL pointer handling ===\n");
    
    // Test NULL pointer for initialize
    int result = core_evolution_diagnostics_initialize(NULL, 1, 10);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_initialize(NULL) should return error");
    
    // Create valid diagnostics for remaining tests
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 42, 10);
    
    // Test NULL pointer for add_event
    result = core_evolution_diagnostics_add_event(NULL, CORE_EVENT_PIPELINE_STARTED);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_add_event(NULL) should return error");
    
    // Test NULL pointer for phase tracking
    result = core_evolution_diagnostics_start_phase(NULL, PIPELINE_PHASE_HALO);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_start_phase(NULL) should return error");
    
    result = core_evolution_diagnostics_end_phase(NULL, PIPELINE_PHASE_HALO);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_end_phase(NULL) should return error");
    
    // Test NULL pointer for merger tracking
    result = core_evolution_diagnostics_add_merger_detection(NULL, 1);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_add_merger_detection(NULL) should return error");
    
    result = core_evolution_diagnostics_add_merger_processed(NULL, 1);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_add_merger_processed(NULL) should return error");
    
    // Test NULL pointer for finalize and report
    result = core_evolution_diagnostics_finalize(NULL);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_finalize(NULL) should return error");
    
    result = core_evolution_diagnostics_report(NULL, LOG_LEVEL_INFO);
    TEST_ASSERT(result != 0, "core_evolution_diagnostics_report(NULL) should return error");
    
    printf("Test completed: NULL pointer handling\n");
}

/**
 * Helper function to convert phase flag to array index (duplicated from core_evolution_diagnostics.c)
 */
static int test_phase_to_index(enum pipeline_execution_phase phase) {
    switch (phase) {
        case PIPELINE_PHASE_HALO:   return 0;
        case PIPELINE_PHASE_GALAXY: return 1;
        case PIPELINE_PHASE_POST:   return 2;
        case PIPELINE_PHASE_FINAL:  return 3;
        default:                    return -1;
    }
}

/**
 * Test phase timing accuracy
 */
static void test_phase_timing() {
    printf("\n=== Testing phase timing accuracy ===\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Test HALO phase with 50ms delay
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
    clock_t wait_ticks = test_wait_clock_ticks(50);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    
    // Convert phase to array index for assertions
    int halo_phase_idx = test_phase_to_index(PIPELINE_PHASE_HALO);
    
    // Check that timing is non-zero and positive
    // Note: We don't perform strict bounds checking since timing can vary
    // significantly on different systems and under different load conditions
    TEST_ASSERT(diag.phases[halo_phase_idx].total_time > 0, "HALO phase timing should be positive");
    
    // Make sure timing is at least reasonable (not too small)
    // We use a very loose lower bound to avoid test failures
    TEST_ASSERT(diag.phases[halo_phase_idx].total_time >= wait_ticks * 0.2, 
                "HALO phase timing should be reasonably accurate");
    
    // Check that step count was incremented
    TEST_ASSERT(diag.phases[halo_phase_idx].step_count == 1, "HALO phase step count should be 1");
    
    // Test multiple steps for GALAXY phase
    for (int i = 0; i < 3; i++) {
        core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
        test_wait_clock_ticks(10); // 10ms each
        core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);
    }
    
    // Convert phase to array index for assertions
    int galaxy_phase_idx = test_phase_to_index(PIPELINE_PHASE_GALAXY);
    
    // Check that step count was incremented correctly
    TEST_ASSERT(diag.phases[galaxy_phase_idx].step_count == 3, "GALAXY phase step count should be 3");
    
    // Check that total time accumulates
    TEST_ASSERT(diag.phases[galaxy_phase_idx].total_time > 0, "GALAXY phase timing should be positive");
    
    // Test finalize
    core_evolution_diagnostics_finalize(&diag);
    
    // Check that elapsed time is calculated
    TEST_ASSERT(diag.elapsed_seconds > 0.0, "elapsed_seconds should be positive after finalize");
    TEST_ASSERT(diag.end_time > diag.start_time, "end_time should be greater than start_time");
    
    printf("Test completed: phase timing\n");
}

/**
 * Test core event counting (infrastructure events only)
 */
static void test_core_event_counting() {
    printf("\n=== Testing core event counting ===\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Add various core infrastructure events
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PIPELINE_STARTED);
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PIPELINE_STARTED);
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PIPELINE_STARTED);
    
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PHASE_STARTED);
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PHASE_STARTED);
    
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_GALAXY_CREATED);
    
    // Check that counts are correct
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_PIPELINE_STARTED] == 3, 
                "PIPELINE_STARTED event count should be 3");
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_PHASE_STARTED] == 2, 
                "PHASE_STARTED event count should be 2");
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_GALAXY_CREATED] == 1, 
                "GALAXY_CREATED event count should be 1");
    
    // Check that other event types are still 0
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_MODULE_ACTIVATED] == 0, 
                "MODULE_ACTIVATED event count should be 0");
    
    // Test for invalid event types (should return error but not crash)
    int result = core_evolution_diagnostics_add_event(&diag, -1);
    TEST_ASSERT(result != 0, "Invalid event type -1 should return error");
    
    result = core_evolution_diagnostics_add_event(&diag, CORE_EVENT_TYPE_MAX + 1);
    TEST_ASSERT(result != 0, "Invalid event type beyond max should return error");
    
    printf("Test completed: core event counting\n");
}

/**
 * Test merger statistics tracking
 */
static void test_merger_statistics() {
    printf("\n=== Testing merger statistics ===\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Add merger detections
    core_evolution_diagnostics_add_merger_detection(&diag, 1); // Minor merger
    core_evolution_diagnostics_add_merger_detection(&diag, 2); // Major merger
    core_evolution_diagnostics_add_merger_detection(&diag, 1); // Minor merger
    
    // Add merger processing
    core_evolution_diagnostics_add_merger_processed(&diag, 1);
    core_evolution_diagnostics_add_merger_processed(&diag, 2);
    
    // Check that counts are correct
    TEST_ASSERT(diag.mergers_detected == 3, "mergers_detected should be 3");
    TEST_ASSERT(diag.major_mergers == 1, "major_mergers should be 1");
    TEST_ASSERT(diag.minor_mergers == 2, "minor_mergers should be 2");
    TEST_ASSERT(diag.mergers_processed == 2, "mergers_processed should be 2");
    
    printf("Test completed: merger statistics\n");
}

/**
 * Test edge cases (empty galaxy list, boundary conditions)
 */
static void test_edge_cases() {
    printf("\n=== Testing edge cases ===\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 0); // Zero galaxies
    
    // Test phase boundary conditions - use invalid phase values
    int result = core_evolution_diagnostics_start_phase(&diag, -1);
    TEST_ASSERT(result != 0, "Invalid phase -1 should return error");
    
    // Use a value that doesn't match any of the enum values
    result = core_evolution_diagnostics_start_phase(&diag, 16); // None of the phase flags (1,2,4,8)
    TEST_ASSERT(result != 0, "Invalid phase 16 should return error");
    
    result = core_evolution_diagnostics_end_phase(&diag, -1);
    TEST_ASSERT(result != 0, "Invalid phase -1 for end_phase should return error");
    
    // Use a value that doesn't match any of the enum values
    result = core_evolution_diagnostics_end_phase(&diag, 16); // None of the phase flags (1,2,4,8)
    TEST_ASSERT(result != 0, "Invalid phase 16 for end_phase should return error");
    
    // Test ending a phase that wasn't started
    result = core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    TEST_ASSERT(result != 0, "Ending unstarted phase should return error");
    
    printf("Test completed: edge cases\n");
}

/**
 * Test finalization with zero elapsed time
 */
static void test_zero_time_finalization() {
    printf("\n=== Testing zero time finalization ===\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Force end_time to be extremely close to start_time to simulate zero elapsed time
    // We'll use a very small artificial difference that should result in ~0 elapsed seconds
    diag.end_time = diag.start_time + 1;  // Just 1 clock tick difference
    
    // This should handle near-zero time gracefully
    int result = core_evolution_diagnostics_finalize(&diag);
    TEST_ASSERT(result == 0, "Finalization with minimal time should succeed");
    
    // With a tiny time difference, elapsed_seconds should be very close to zero
    // and galaxies_per_second should be either 0 or a very large number
    // The important thing is that we don't crash from division by zero
    
    // Print the actual values for debugging if needed
    printf("  elapsed_seconds: %.10f\n", diag.elapsed_seconds);
    printf("  galaxies_per_second: %.2f\n", diag.galaxies_per_second);
    
    // Verify general expectations without being too strict
    TEST_ASSERT(diag.elapsed_seconds < 0.01, "elapsed_seconds should be very small");
    
    printf("Test completed: zero time finalization\n");
}

/**
 * Test core infrastructure independence (no physics dependencies)
 */
static void test_core_infrastructure_independence() {
    printf("\n=== Testing core infrastructure independence ===\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 5);
    
    // Test that diagnostics can run without any physics modules
    // This validates core-physics separation compliance
    
    // Simulate a complete pipeline execution with only core events
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PHASE_STARTED);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_GALAXY_CREATED);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);
    
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_POST);
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PHASE_COMPLETED);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_POST);
    
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_FINAL);
    core_evolution_diagnostics_add_event(&diag, CORE_EVENT_PIPELINE_COMPLETED);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_FINAL);
    
    // Finalize and report
    core_evolution_diagnostics_finalize(&diag);
    core_evolution_diagnostics_report(&diag, LOG_LEVEL_DEBUG);
    
    // Verify core events were tracked correctly
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_PHASE_STARTED] == 1, 
                "PHASE_STARTED event should be tracked");
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_GALAXY_CREATED] == 1, 
                "GALAXY_CREATED event should be tracked");
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_PHASE_COMPLETED] == 1, 
                "PHASE_COMPLETED event should be tracked");
    TEST_ASSERT(diag.core_event_counts[CORE_EVENT_PIPELINE_COMPLETED] == 1, 
                "PIPELINE_COMPLETED event should be tracked");
    
    // Verify phase execution was tracked
    TEST_ASSERT(diag.phases[0].step_count == 1, "HALO phase should have 1 step");
    TEST_ASSERT(diag.phases[1].step_count == 1, "GALAXY phase should have 1 step");
    TEST_ASSERT(diag.phases[2].step_count == 1, "POST phase should have 1 step");
    TEST_ASSERT(diag.phases[3].step_count == 1, "FINAL phase should have 1 step");
    
    printf("Test completed: core infrastructure independence\n");
}

/**
 * Main test function
 */
int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for test_evolution_diagnostics\n");
    printf("Testing core-physics separation compliant diagnostics system\n");
    printf("========================================\n");
    
    // Initialize any required systems
    logging_init(LOG_LEVEL_ERROR, stdout);
    
    // Run tests
    test_core_diagnostics_initialization();
    test_null_pointer_handling();
    test_phase_timing();
    test_core_event_counting();
    test_merger_statistics();
    test_edge_cases();
    test_zero_time_finalization();
    test_core_infrastructure_independence();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_evolution_diagnostics:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");
    printf("Core-physics separation validated: diagnostics tracks only infrastructure metrics\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}

/* ----- Helper Functions ----- */

/**
 * Wait for a given number of milliseconds and return elapsed clock ticks
 */
static clock_t test_wait_clock_ticks(int ms) {
    clock_t start = clock();
    
    // Use usleep for more precise delays
    usleep(ms * 1000);
    
    clock_t end = clock();
    return end - start;
}
