#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../src/core/core_evolution_diagnostics.h"
#include "../src/core/core_event_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_allvars.h"

// Mock functions and helpers
static struct GALAXY *create_test_galaxies(int ngal);
static void free_test_galaxies(struct GALAXY *galaxies);
static clock_t test_wait_clock_ticks(int ms);

/**
 * Test initialization of the diagnostics structure
 */
static void test_diagnostics_initialization() {
    printf("Testing diagnostics initialization...\n");
    
    // Test normal initialization
    struct evolution_diagnostics diag;
    int result = evolution_diagnostics_initialize(&diag, 42, 10);
    
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
    
    // Check that event counts were initialized to 0
    for (int i = 0; i < EVENT_TYPE_MAX; i++) {
        assert(diag.event_counts[i] == 0);
    }
    
    // Check that merger statistics were initialized to 0
    assert(diag.mergers_detected == 0);
    assert(diag.mergers_processed == 0);
    assert(diag.major_mergers == 0);
    assert(diag.minor_mergers == 0);
    
    printf("Test passed: diagnostics initialization\n");
}

/**
 * Test NULL pointer handling
 */
static void test_null_pointer_handling() {
    printf("Testing NULL pointer handling...\n");
    
    // Test NULL pointer for initialize
    int result = evolution_diagnostics_initialize(NULL, 1, 10);
    assert(result != 0); // Should return non-zero error code
    
    // Create valid diagnostics for remaining tests
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, 42, 10);
    
    // Test NULL pointer for add_event
    result = evolution_diagnostics_add_event(NULL, EVENT_TYPE_COOLING);
    assert(result != 0);
    
    // Test NULL pointer for phase tracking
    result = evolution_diagnostics_start_phase(NULL, PIPELINE_PHASE_HALO);
    assert(result != 0);
    
    result = evolution_diagnostics_end_phase(NULL, PIPELINE_PHASE_HALO);
    assert(result != 0);
    
    // Test NULL pointer for merger tracking
    result = evolution_diagnostics_add_merger_detection(NULL, 1);
    assert(result != 0);
    
    result = evolution_diagnostics_add_merger_processed(NULL, 1);
    assert(result != 0);
    
    // Test NULL pointer for property recording
    struct GALAXY *galaxies = create_test_galaxies(5);
    
    result = evolution_diagnostics_record_initial_properties(NULL, galaxies, 5);
    assert(result != 0);
    
    result = evolution_diagnostics_record_initial_properties(&diag, NULL, 5);
    assert(result != 0);
    
    result = evolution_diagnostics_record_final_properties(NULL, galaxies, 5);
    assert(result != 0);
    
    result = evolution_diagnostics_record_final_properties(&diag, NULL, 5);
    assert(result != 0);
    
    // Test NULL pointer for finalize and report
    result = evolution_diagnostics_finalize(NULL);
    assert(result != 0);
    
    result = evolution_diagnostics_report(NULL, LOG_LEVEL_INFO);
    assert(result != 0);
    
    free_test_galaxies(galaxies);
    
    printf("Test passed: NULL pointer handling\n");
}

/**
 * Test phase timing accuracy
 */
static void test_phase_timing() {
    printf("Testing phase timing accuracy...\n");
    
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Test HALO phase with 50ms delay
    evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
    clock_t wait_ticks = test_wait_clock_ticks(50);
    evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    
    // Check that timing is within reasonable bounds
    assert(diag.phases[PIPELINE_PHASE_HALO].total_time > 0);
    assert(diag.phases[PIPELINE_PHASE_HALO].total_time >= wait_ticks * 0.8); // Allow some variance
    assert(diag.phases[PIPELINE_PHASE_HALO].total_time <= wait_ticks * 1.2);
    
    // Check that step count was incremented
    assert(diag.phases[PIPELINE_PHASE_HALO].step_count == 1);
    
    // Test multiple steps for GALAXY phase
    for (int i = 0; i < 3; i++) {
        evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
        test_wait_clock_ticks(10); // 10ms each
        evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);
    }
    
    // Check that step count was incremented correctly
    assert(diag.phases[PIPELINE_PHASE_GALAXY].step_count == 3);
    
    // Check that total time accumulates
    assert(diag.phases[PIPELINE_PHASE_GALAXY].total_time > 0);
    
    // Test finalize
    evolution_diagnostics_finalize(&diag);
    
    // Check that elapsed time is calculated
    assert(diag.elapsed_seconds > 0.0);
    assert(diag.end_time > diag.start_time);
    
    printf("Test passed: phase timing\n");
}

/**
 * Test event counting
 */
static void test_event_counting() {
    printf("Testing event counting...\n");
    
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Add various events
    evolution_diagnostics_add_event(&diag, EVENT_TYPE_COOLING);
    evolution_diagnostics_add_event(&diag, EVENT_TYPE_COOLING);
    evolution_diagnostics_add_event(&diag, EVENT_TYPE_COOLING);
    
    evolution_diagnostics_add_event(&diag, EVENT_TYPE_STAR_FORMATION);
    evolution_diagnostics_add_event(&diag, EVENT_TYPE_STAR_FORMATION);
    
    evolution_diagnostics_add_event(&diag, EVENT_TYPE_FEEDBACK);
    
    // Check that counts are correct
    assert(diag.event_counts[EVENT_TYPE_COOLING] == 3);
    assert(diag.event_counts[EVENT_TYPE_STAR_FORMATION] == 2);
    assert(diag.event_counts[EVENT_TYPE_FEEDBACK] == 1);
    
    // Check that other event types are still 0
    assert(diag.event_counts[EVENT_TYPE_DISK_INSTABILITY] == 0);
    
    // Test for invalid event types (should return error but not crash)
    int result = evolution_diagnostics_add_event(&diag, -1);
    assert(result != 0);
    
    result = evolution_diagnostics_add_event(&diag, EVENT_TYPE_MAX + 1);
    assert(result != 0);
    
    printf("Test passed: event counting\n");
}

/**
 * Test merger statistics tracking
 */
static void test_merger_statistics() {
    printf("Testing merger statistics...\n");
    
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Add merger detections
    evolution_diagnostics_add_merger_detection(&diag, 1); // Minor merger
    evolution_diagnostics_add_merger_detection(&diag, 2); // Major merger
    evolution_diagnostics_add_merger_detection(&diag, 1); // Minor merger
    
    // Add merger processing
    evolution_diagnostics_add_merger_processed(&diag, 1);
    evolution_diagnostics_add_merger_processed(&diag, 2);
    
    // Check that counts are correct
    assert(diag.mergers_detected == 3);
    assert(diag.major_mergers == 1);
    assert(diag.minor_mergers == 2);
    assert(diag.mergers_processed == 2);
    
    printf("Test passed: merger statistics\n");
}

/**
 * Test galaxy property recording
 */
static void test_galaxy_properties() {
    printf("Testing galaxy property recording...\n");
    
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, 1, 5);
    
    // Create test galaxies with known properties
    struct GALAXY *galaxies = create_test_galaxies(5);
    
    // Set some known values
    double expected_stellar_mass = 0.0;
    double expected_cold_gas = 0.0;
    double expected_hot_gas = 0.0;
    double expected_bulge_mass = 0.0;
    
    for (int i = 0; i < 5; i++) {
        galaxies[i].StellarMass = 1.0e10 * (i + 1);
        galaxies[i].ColdGas = 2.0e10 * (i + 1);
        galaxies[i].HotGas = 5.0e11 * (i + 1);
        galaxies[i].BulgeMass = 0.5e10 * (i + 1);
        
        expected_stellar_mass += galaxies[i].StellarMass;
        expected_cold_gas += galaxies[i].ColdGas;
        expected_hot_gas += galaxies[i].HotGas;
        expected_bulge_mass += galaxies[i].BulgeMass;
    }
    
    // Record initial properties
    evolution_diagnostics_record_initial_properties(&diag, galaxies, 5);
    
    // Verify initial properties
    assert(diag.total_stellar_mass_initial == expected_stellar_mass);
    assert(diag.total_cold_gas_initial == expected_cold_gas);
    assert(diag.total_hot_gas_initial == expected_hot_gas);
    assert(diag.total_bulge_mass_initial == expected_bulge_mass);
    
    // Modify galaxy properties for final state
    for (int i = 0; i < 5; i++) {
        galaxies[i].StellarMass *= 1.1; // 10% increase
        galaxies[i].ColdGas *= 0.9;     // 10% decrease
        galaxies[i].HotGas *= 1.05;     // 5% increase
        galaxies[i].BulgeMass *= 1.2;   // 20% increase
    }
    
    // Recalculate expected values
    expected_stellar_mass = 0.0;
    expected_cold_gas = 0.0;
    expected_hot_gas = 0.0;
    expected_bulge_mass = 0.0;
    
    for (int i = 0; i < 5; i++) {
        expected_stellar_mass += galaxies[i].StellarMass;
        expected_cold_gas += galaxies[i].ColdGas;
        expected_hot_gas += galaxies[i].HotGas;
        expected_bulge_mass += galaxies[i].BulgeMass;
    }
    
    // Record final properties
    evolution_diagnostics_record_final_properties(&diag, galaxies, 5);
    
    // Verify final properties
    assert(diag.total_stellar_mass_final == expected_stellar_mass);
    assert(diag.total_cold_gas_final == expected_cold_gas);
    assert(diag.total_hot_gas_final == expected_hot_gas);
    assert(diag.total_bulge_mass_final == expected_bulge_mass);
    assert(diag.ngal_final == 5);
    
    free_test_galaxies(galaxies);
    
    printf("Test passed: galaxy property recording\n");
}

/**
 * Test edge cases (empty galaxy list, boundary conditions)
 */
static void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, 1, 0); // Zero galaxies
    
    // Test empty galaxy list
    struct GALAXY *empty_galaxies = create_test_galaxies(0);
    int result = evolution_diagnostics_record_initial_properties(&diag, empty_galaxies, 0);
    assert(result == 0);
    assert(diag.total_stellar_mass_initial == 0.0);
    
    result = evolution_diagnostics_record_final_properties(&diag, empty_galaxies, 0);
    assert(result == 0);
    assert(diag.ngal_final == 0);
    
    // Test phase boundary conditions
    result = evolution_diagnostics_start_phase(&diag, -1);
    assert(result != 0); // Should fail
    
    result = evolution_diagnostics_start_phase(&diag, 4);
    assert(result != 0); // Should fail
    
    result = evolution_diagnostics_end_phase(&diag, -1);
    assert(result != 0); // Should fail
    
    result = evolution_diagnostics_end_phase(&diag, 4);
    assert(result != 0); // Should fail
    
    // Test ending a phase that wasn't started
    result = evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);
    assert(result != 0); // Should fail because phase wasn't started
    
    free_test_galaxies(empty_galaxies);
    
    printf("Test passed: edge cases\n");
}

/**
 * Test finalization with zero elapsed time
 */
static void test_zero_time_finalization() {
    printf("Testing zero time finalization...\n");
    
    struct evolution_diagnostics diag;
    evolution_diagnostics_initialize(&diag, 1, 10);
    
    // Force end_time to equal start_time (zero elapsed time)
    diag.end_time = diag.start_time;
    
    // This should handle division by zero gracefully
    int result = evolution_diagnostics_finalize(&diag);
    assert(result == 0);
    assert(diag.elapsed_seconds == 0.0);
    assert(diag.galaxies_per_second == 0.0);
    
    printf("Test passed: zero time finalization\n");
}

/**
 * Main test function
 */
int main(int argc, char *argv[]) {
    printf("=== Evolution Diagnostics Test Suite ===\n");
    
    // Initialize any required systems
    logging_init(LOG_LEVEL_ERROR, stdout);
    
    // Run tests
    test_diagnostics_initialization();
    test_null_pointer_handling();
    test_phase_timing();
    test_event_counting();
    test_merger_statistics();
    test_galaxy_properties();
    test_edge_cases();
    test_zero_time_finalization();
    
    printf("All tests passed!\n");
    return 0;
}

/* ----- Helper Functions ----- */

/**
 * Create test galaxies with default values
 */
static struct GALAXY *create_test_galaxies(int ngal) {
    if (ngal <= 0) {
        return calloc(1, sizeof(struct GALAXY)); // Empty but valid pointer
    }
    
    struct GALAXY *galaxies = calloc(ngal, sizeof(struct GALAXY));
    if (galaxies == NULL) {
        fprintf(stderr, "Failed to allocate test galaxies\n");
        exit(1);
    }
    
    // Initialize with default values
    for (int i = 0; i < ngal; i++) {
        galaxies[i].StellarMass = 1.0e10;
        galaxies[i].ColdGas = 2.0e10;
        galaxies[i].HotGas = 5.0e11;
        galaxies[i].BulgeMass = 0.5e10;
        galaxies[i].Type = (i == 0) ? 0 : 1; // First galaxy is central
        galaxies[i].HaloNr = 1;
    }
    
    return galaxies;
}

/**
 * Free test galaxies
 */
static void free_test_galaxies(struct GALAXY *galaxies) {
    free(galaxies);
}

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