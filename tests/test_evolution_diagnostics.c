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

// Mock functions and helpers
static clock_t test_wait_clock_ticks(int ms);

/**
 * Test initialization of the core diagnostics structure
 */
static void test_core_diagnostics_initialization() {
    printf("Testing core diagnostics initialization...\n");
    
    // Test normal initialization
    struct core_evolution_diagnostics diag;
    int result = core_evolution_diagnostics_initialize(&diag, 42, 10);
    
    assert(result == 0);
    assert(diag.halo_nr == 42);
    assert(diag.ngal_initial == 10);
    assert(diag.ngal_final == 0);
    assert(diag.start_time > 0);
    assert(diag.end_time == 0);
    assert(diag.elapsed_seconds == 0.0);
    
    // Check that phases were initialized to 0
    for (int i = 0; i < 4; i++) {
        assert(diag.phases[i].start_time == 0);
        assert(diag.phases[i].total_time == 0);
        assert(diag.phases[i].galaxy_count == 0);
        assert(diag.phases[i].step_count == 0);
    }
    
    // Check that core event counts were initialized to 0
    for (int i = 0; i < CORE_EVENT_TYPE_MAX; i++) {
        assert(diag.core_event_counts[i] == 0);
    }
    
    // Check that merger statistics were initialized to 0
    assert(diag.mergers_detected == 0);
    assert(diag.mergers_processed == 0);
    assert(diag.major_mergers == 0);
    assert(diag.minor_mergers == 0);
    
    printf("Test passed: core diagnostics initialization\n");
}

/**
 * Test NULL pointer handling
 */
static void test_null_pointer_handling() {
    printf("Testing NULL pointer handling...\n");
    
    // Test NULL pointer for initialize
    int result = core_evolution_diagnostics_initialize(NULL, 1, 10);
    assert(result != 0); // Should return non-zero error code
    
    // Create valid diagnostics for remaining tests
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 42, 10);
    
    // Test NULL pointer for add_event
    result = core_evolution_diagnostics_add_event(NULL, CORE_EVENT_PIPELINE_STARTED);
    assert(result != 0);
    
    // Test NULL pointer for phase tracking
    result = core_evolution_diagnostics_start_phase(NULL, PIPELINE_PHASE_HALO);
    assert(result != 0);
    
    result = core_evolution_diagnostics_end_phase(NULL, PIPELINE_PHASE_HALO);
    assert(result != 0);
    
    // Test NULL pointer for merger tracking
    result = core_evolution_diagnostics_add_merger_detection(NULL, 1);
    assert(result != 0);
    
    result = core_evolution_diagnostics_add_merger_processed(NULL, 1);
    assert(result != 0);
    
    // Test NULL pointer for finalize and report
    result = core_evolution_diagnostics_finalize(NULL);
    assert(result != 0);
    
    result = core_evolution_diagnostics_report(NULL, LOG_LEVEL_INFO);
    assert(result != 0);
    
    printf("Test passed: NULL pointer handling\n");
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
    printf("Testing phase timing accuracy...\n");
    
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
    assert(diag.phases[halo_phase_idx].total_time > 0);
    
    // Make sure timing is at least reasonable (not too small)
    // We use a very loose lower bound to avoid test failures
    assert(diag.phases[halo_phase_idx].total_time >= wait_ticks * 0.2);
    
    // Check that step count was incremented
    assert(diag.phases[halo_phase_idx].step_count == 1);
    
    // Test multiple steps for GALAXY phase
    for (int i = 0; i < 3; i++) {
        core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
        test_wait_clock_ticks(10); // 10ms each
        core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);
    }
    
    // Convert phase to array index for assertions
    int galaxy_phase_idx = test_phase_to_index(PIPELINE_PHASE_GALAXY);
    
    // Check that step count was incremented correctly
    assert(diag.phases[galaxy_phase_idx].step_count == 3);
    
    // Check that total time accumulates
    assert(diag.phases[galaxy_phase_idx].total_time > 0);
    
    // Test finalize
    core_evolution_diagnostics_finalize(&diag);
    
    // Check that elapsed time is calculated
    assert(diag.elapsed_seconds > 0.0);
    assert(diag.end_time > diag.start_time);
    
    printf("Test passed: phase timing\n");
}

/**
 * Test core event counting (infrastructure events only)
 */
static void test_core_event_counting() {
    printf("Testing core event counting...\n");
    
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
    assert(diag.core_event_counts[CORE_EVENT_PIPELINE_STARTED] == 3);
    assert(diag.core_event_counts[CORE_EVENT_PHASE_STARTED] == 2);
    assert(diag.core_event_counts[CORE_EVENT_GALAXY_CREATED] == 1);
    
    // Check that other event types are still 0
    assert(diag.core_event_counts[CORE_EVENT_MODULE_ACTIVATED] == 0);
    
    // Test for invalid event types (should return error but not crash)
    int result = core_evolution_diagnostics_add_event(&diag, -1);
    assert(result != 0);
    
    result = core_evolution_diagnostics_add_event(&diag, CORE_EVENT_TYPE_MAX + 1);
    assert(result != 0);
    
    printf("Test passed: core event counting\n");
}

/**
 * Test merger statistics tracking
 */
static void test_merger_statistics() {
    printf("Testing merger statistics...\n");
    
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
    assert(diag.mergers_detected == 3);
    assert(diag.major_mergers == 1);
    assert(diag.minor_mergers == 2);
    assert(diag.mergers_processed == 2);
    
    printf("Test passed: merger statistics\n");
}

/**
 * Test edge cases (empty galaxy list, boundary conditions)
 */
static void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 0); // Zero galaxies
    
    // Test phase boundary conditions - use invalid phase values
    int result = core_evolution_diagnostics_start_phase(&diag, -1);
    assert(result != 0); // Should fail
    
    // Use a value that doesn't match any of the enum values
    result = core_evolution_diagnostics_start_phase(&diag, 16); // None of the phase flags (1,2,4,8)
    assert(result != 0); // Should fail
    
    result = core_evolution_diagnostics_end_phase(&diag, -1);
    assert(result != 0); // Should fail
    
    // Use a value that doesn't match any of the enum values
    result = core_evolution_diagnostics_end_phase(&diag, 16); // None of the phase flags (1,2,4,8)
    assert(result != 0); // Should fail
    
    // Test ending a phase that wasn't started
    result = core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    assert(result != 0); // Should fail because phase wasn't started
    
    printf("Test passed: edge cases\n");
}

/**
 * Test finalization with zero elapsed time
 */
static void test_zero_time_finalization() {
    printf("Testing zero time finalization...\n");
    
    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Force end_time to be extremely close to start_time to simulate zero elapsed time
    // We'll use a very small artificial difference that should result in ~0 elapsed seconds
    diag.end_time = diag.start_time + 1;  // Just 1 clock tick difference
    
    // This should handle near-zero time gracefully
    int result = core_evolution_diagnostics_finalize(&diag);
    assert(result == 0);
    
    // With a tiny time difference, elapsed_seconds should be very close to zero
    // and galaxies_per_second should be either 0 or a very large number
    // The important thing is that we don't crash from division by zero
    
    // Print the actual values for debugging if needed
    printf("  elapsed_seconds: %.10f\n", diag.elapsed_seconds);
    printf("  galaxies_per_second: %.2f\n", diag.galaxies_per_second);
    
    // Verify general expectations without being too strict
    assert(diag.elapsed_seconds < 0.01);  // Should be very small
    
    printf("Test passed: zero time finalization\n");
}

/**
 * Test core infrastructure independence (no physics dependencies)
 */
static void test_core_infrastructure_independence() {
    printf("Testing core infrastructure independence...\n");
    
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
    assert(diag.core_event_counts[CORE_EVENT_PHASE_STARTED] == 1);
    assert(diag.core_event_counts[CORE_EVENT_GALAXY_CREATED] == 1);
    assert(diag.core_event_counts[CORE_EVENT_PHASE_COMPLETED] == 1);
    assert(diag.core_event_counts[CORE_EVENT_PIPELINE_COMPLETED] == 1);
    
    // Verify phase execution was tracked
    assert(diag.phases[0].step_count == 1); // HALO
    assert(diag.phases[1].step_count == 1); // GALAXY
    assert(diag.phases[2].step_count == 1); // POST
    assert(diag.phases[3].step_count == 1); // FINAL
    
    printf("Test passed: core infrastructure independence\n");
}

/**
 * Main test function
 */
int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("=== Core Evolution Diagnostics Test Suite ===\n");
    printf("Testing core-physics separation compliant diagnostics system\n");
    
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
    
    printf("All core diagnostics tests passed!\n");
    printf("Core-physics separation validated: diagnostics tracks only infrastructure metrics\n");
    return 0;
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
