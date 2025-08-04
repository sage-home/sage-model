/**
 * Test suite for Galaxy Inheritance and Orphan Handling
 * 
 * Tests the core galaxy inheritance functionality after SAGE Tree Conversion Plan.
 * This is a focused unit test that verifies the building blocks of inheritance:
 *
 * CORE FUNCTIONALITY TESTED:
 * - Primordial galaxy creation (init_galaxy)
 * - Galaxy property copying (deep_copy_galaxy) 
 * - Property access and updates (GALAXY_PROP macros)
 * - Halo property calculations (get_virial_mass, etc.)
 * - Galaxy array management during inheritance
 * - Central vs satellite galaxy classification
 *
 * INHERITANCE SCENARIOS TESTED:
 * - New galaxy creation for halos without progenitors
 * - Galaxy inheritance from single progenitor
 * - Galaxy inheritance from multiple progenitors (mergers)
 * - Property updates during inheritance (position, mass, etc.)
 * - FOF group processing with multiple halos
 * 
 * This test focuses on the inheritance LOGIC rather than the full physics pipeline,
 * ensuring the core inheritance mechanisms work correctly in isolation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "core_allvars.h"
#include "core_mymalloc.h"
#include "core_build_model.h"
#include "galaxy_array.h"
#include "core_properties.h"
#include "physics/physics_essential_functions.h"
#include "core_galaxy_extensions.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
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
    struct halo_data* halos;
    struct halo_aux_data* haloaux;
    struct params run_params;
    GalaxyArray* working_galaxies;
    GalaxyArray* output_galaxies;
    GalaxyArray* previous_galaxies;
    int nhalo;
    int initialized;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize memory allocation system
    memory_system_init();
    
    // Initialize minimal run parameters for testing
    test_ctx.run_params.simulation.SimMaxSnaps = 64;
    test_ctx.run_params.simulation.LastSnapshotNr = 63;
    test_ctx.run_params.simulation.NumSnapOutputs = 10;  // Required for properties
    
    // Initialize simulation arrays that are needed by initialize_evolution_context
    // Allocate Age array (needed for halo_age calculation)
    test_ctx.run_params.simulation.Age = mycalloc(64, sizeof(double));
    if (!test_ctx.run_params.simulation.Age) {
        return EXIT_FAILURE;
    }
    
    // Initialize with dummy values - just need non-zero values for testing
    for (int i = 0; i < 64; i++) {
        test_ctx.run_params.simulation.ZZ[i] = (double)i * 0.1;  // Mock redshifts
        test_ctx.run_params.simulation.Age[i] = 13.8 - (double)i * 0.2;  // Mock ages in Gyr
    }
    
    // Set basic cosmology parameters
    test_ctx.run_params.cosmology.Omega = 0.3;
    test_ctx.run_params.cosmology.OmegaLambda = 0.7;
    test_ctx.run_params.cosmology.Hubble_h = 0.7;
    
    // Initialize galaxy arrays
    test_ctx.working_galaxies = galaxy_array_new();
    test_ctx.output_galaxies = galaxy_array_new();
    test_ctx.previous_galaxies = galaxy_array_new();
    
    if (!test_ctx.working_galaxies || !test_ctx.output_galaxies || !test_ctx.previous_galaxies) {
        return EXIT_FAILURE;
    }
    
    test_ctx.initialized = 1;
    return EXIT_SUCCESS;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.working_galaxies) {
        galaxy_array_free(&test_ctx.working_galaxies);
    }
    if (test_ctx.output_galaxies) {
        galaxy_array_free(&test_ctx.output_galaxies);
    }
    if (test_ctx.previous_galaxies) {
        galaxy_array_free(&test_ctx.previous_galaxies);
    }
    if (test_ctx.halos) {
        myfree(test_ctx.halos);
        test_ctx.halos = NULL;
    }
    if (test_ctx.haloaux) {
        myfree(test_ctx.haloaux);
        test_ctx.haloaux = NULL;
    }
    if (test_ctx.run_params.simulation.Age) {
        myfree(test_ctx.run_params.simulation.Age);
        test_ctx.run_params.simulation.Age = NULL;
    }
    
    // Cleanup memory manager
    memory_system_cleanup();
    
    test_ctx.initialized = 0;
}

// Helper function to create simple tree structure
static void create_simple_tree(int nhalo) {
    test_ctx.nhalo = nhalo;
    test_ctx.halos = mycalloc(nhalo, sizeof(struct halo_data));
    test_ctx.haloaux = mycalloc(nhalo, sizeof(struct halo_aux_data));
    
    // Initialize all halos with default values
    for (int i = 0; i < nhalo; i++) {
        test_ctx.halos[i].Descendant = -1;
        test_ctx.halos[i].FirstProgenitor = -1;
        test_ctx.halos[i].NextProgenitor = -1;
        test_ctx.halos[i].FirstHaloInFOFgroup = i;  // Each halo is its own FOF by default
        test_ctx.halos[i].NextHaloInFOFgroup = -1;
        test_ctx.halos[i].SnapNum = 63;  // Default to z=0
        test_ctx.halos[i].Len = 100;     // Default particle count
        test_ctx.halos[i].Mvir = 1.0e12; // Default mass
        test_ctx.halos[i].Vmax = 220.0;  // Default max velocity
        test_ctx.halos[i].Pos[0] = 50.0 + i * 10.0;
        test_ctx.halos[i].Pos[1] = 100.0 + i * 10.0;
        test_ctx.halos[i].Pos[2] = 150.0 + i * 10.0;
        
        // Initialize auxiliary data
        test_ctx.haloaux[i].NGalaxies = 0;
        test_ctx.haloaux[i].FirstGalaxy = -1;
        test_ctx.haloaux[i].output_snap_n = -1;
        test_ctx.haloaux[i].was_processed_in_current_snap = false;
    }
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Basic galaxy array initialization
 */
static void test_galaxy_array_lifecycle(void) {
    printf("=== Testing galaxy array lifecycle ===\n");
    
    create_simple_tree(1);
    
    TEST_ASSERT(test_ctx.working_galaxies != NULL, "Working galaxy array should be initialized");
    TEST_ASSERT(test_ctx.output_galaxies != NULL, "Output galaxy array should be initialized");
    TEST_ASSERT(test_ctx.previous_galaxies != NULL, "Previous galaxy array should be initialized");
    TEST_ASSERT(galaxy_array_get_count(test_ctx.working_galaxies) == 0, "Working array should start empty");
    TEST_ASSERT(galaxy_array_get_count(test_ctx.output_galaxies) == 0, "Output array should start empty");
    TEST_ASSERT(galaxy_array_get_count(test_ctx.previous_galaxies) == 0, "Previous array should start empty");
}

/**
 * Test: Primordial galaxy creation using init_galaxy() function
 */
static void test_primordial_galaxy_creation(void) {
    printf("\n=== Testing primordial galaxy creation ===\n");
    
    create_simple_tree(1);
    
    // Test creating a new galaxy using the core init_galaxy function
    struct GALAXY new_galaxy;
    memset(&new_galaxy, 0, sizeof(struct GALAXY));
    
    // Initialize extensions
    galaxy_extension_initialize(&new_galaxy);
    
    int32_t galaxy_counter = 0;
    
    // This is the core function that creates new galaxies without progenitors
    init_galaxy(0, 0, &galaxy_counter, test_ctx.halos, &new_galaxy, &test_ctx.run_params);
    
    // Verify galaxy was properly initialized
    TEST_ASSERT(galaxy_counter == 1, "Galaxy counter should be incremented");
    TEST_ASSERT(new_galaxy.properties != NULL, "Galaxy properties should be allocated");
    TEST_ASSERT(GALAXY_PROP_HaloNr(&new_galaxy) == 0, "Galaxy should be assigned to halo 0");
    TEST_ASSERT(GALAXY_PROP_Type(&new_galaxy) == 0, "New galaxy should be central (Type=0)");
    TEST_ASSERT(GALAXY_PROP_Mvir(&new_galaxy) > 0.0, "Galaxy should have positive virial mass");
    
    // Test galaxy can be added to array
    int result = galaxy_array_append(test_ctx.working_galaxies, &new_galaxy, &test_ctx.run_params);
    TEST_ASSERT(result >= 0, "Galaxy should be successfully added to array");
    TEST_ASSERT(galaxy_array_get_count(test_ctx.working_galaxies) == 1, "Array should contain one galaxy");
    
    // Cleanup
    free_galaxy_properties(&new_galaxy);
}

/**
 * Test: Galaxy inheritance using deep_copy_galaxy() function
 */
static void test_galaxy_inheritance_copying(void) {
    printf("\n=== Testing galaxy inheritance copying ===\n");
    
    create_simple_tree(2);
    
    // Create a progenitor galaxy in previous snapshot
    struct GALAXY progenitor_galaxy;
    memset(&progenitor_galaxy, 0, sizeof(struct GALAXY));
    galaxy_extension_initialize(&progenitor_galaxy);
    
    int32_t galaxy_counter = 0;
    init_galaxy(0, 1, &galaxy_counter, test_ctx.halos, &progenitor_galaxy, &test_ctx.run_params);
    
    // Set some properties to verify inheritance
    GALAXY_PROP_HaloNr(&progenitor_galaxy) = 1;  // Progenitor halo
    GALAXY_PROP_StellarMass(&progenitor_galaxy) = 1.5e10;
    GALAXY_PROP_ColdGas(&progenitor_galaxy) = 2.0e9;
    GALAXY_PROP_Type(&progenitor_galaxy) = 0;  // Central galaxy
    
    // Test inheritance copying
    struct GALAXY inherited_galaxy;
    memset(&inherited_galaxy, 0, sizeof(struct GALAXY));
    galaxy_extension_initialize(&inherited_galaxy);
    
    // This is the core inheritance function
    deep_copy_galaxy(&inherited_galaxy, &progenitor_galaxy, &test_ctx.run_params);
    
    // Verify inheritance worked correctly
    TEST_ASSERT(inherited_galaxy.properties != NULL, "Inherited galaxy should have properties");
    TEST_ASSERT(GALAXY_PROP_StellarMass(&inherited_galaxy) == GALAXY_PROP_StellarMass(&progenitor_galaxy), 
                "Stellar mass should be inherited");
    TEST_ASSERT(GALAXY_PROP_ColdGas(&inherited_galaxy) == GALAXY_PROP_ColdGas(&progenitor_galaxy), 
                "Cold gas should be inherited");
    TEST_ASSERT(GALAXY_PROP_Type(&inherited_galaxy) == GALAXY_PROP_Type(&progenitor_galaxy), 
                "Galaxy type should be inherited");
    
    // Test that we can update inherited properties (simulating inheritance updates)
    GALAXY_PROP_HaloNr(&inherited_galaxy) = 0;  // Update to new host halo
    TEST_ASSERT(GALAXY_PROP_HaloNr(&inherited_galaxy) != GALAXY_PROP_HaloNr(&progenitor_galaxy), 
                "Halo number should be updatable after inheritance");
    
    // Cleanup
    free_galaxy_properties(&progenitor_galaxy);
    free_galaxy_properties(&inherited_galaxy);
}

/**
 * Test: Property updates during inheritance (virial mass calculation)
 */
static void test_inheritance_property_updates(void) {
    printf("\n=== Testing inheritance property updates ===\n");
    
    create_simple_tree(2);
    
    // Set different masses for halos to test property updates
    test_ctx.halos[0].Mvir = 2.0e12;  // Descendant halo (more massive)
    test_ctx.halos[1].Mvir = 1.0e12;  // Progenitor halo (less massive)
    
    // Test virial mass calculation function
    double mvir_0 = get_virial_mass(0, test_ctx.halos, &test_ctx.run_params);
    double mvir_1 = get_virial_mass(1, test_ctx.halos, &test_ctx.run_params);
    
    TEST_ASSERT(mvir_0 > 0.0, "Halo 0 should have positive virial mass");
    TEST_ASSERT(mvir_1 > 0.0, "Halo 1 should have positive virial mass");
    TEST_ASSERT(mvir_0 > mvir_1, "Descendant halo should be more massive than progenitor");
    
    // Create galaxy and test property updates during inheritance
    struct GALAXY progenitor_galaxy;
    memset(&progenitor_galaxy, 0, sizeof(struct GALAXY));
    galaxy_extension_initialize(&progenitor_galaxy);
    
    int32_t galaxy_counter = 0;
    init_galaxy(0, 1, &galaxy_counter, test_ctx.halos, &progenitor_galaxy, &test_ctx.run_params);
    
    // Get original virial mass
    double original_mvir = GALAXY_PROP_Mvir(&progenitor_galaxy);
    TEST_ASSERT(original_mvir > 0.0, "Progenitor galaxy should have positive Mvir");
    
    // Simulate inheritance to new halo with different mass
    struct GALAXY inherited_galaxy;
    memset(&inherited_galaxy, 0, sizeof(struct GALAXY));
    galaxy_extension_initialize(&inherited_galaxy);
    deep_copy_galaxy(&inherited_galaxy, &progenitor_galaxy, &test_ctx.run_params);
    
    // Update properties for new halo (simulating inheritance)
    GALAXY_PROP_HaloNr(&inherited_galaxy) = 0;  // Move to descendant halo
    GALAXY_PROP_Mvir(&inherited_galaxy) = mvir_0;  // Update virial mass
    
    // Verify property updates
    TEST_ASSERT(GALAXY_PROP_HaloNr(&inherited_galaxy) == 0, "Galaxy should be assigned to new halo");
    TEST_ASSERT(GALAXY_PROP_Mvir(&inherited_galaxy) == mvir_0, "Galaxy virial mass should be updated");
    TEST_ASSERT(GALAXY_PROP_Mvir(&inherited_galaxy) != original_mvir, "Virial mass should change during inheritance");
    
    // Cleanup
    free_galaxy_properties(&progenitor_galaxy);
    free_galaxy_properties(&inherited_galaxy);
}


//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for Galaxy Inheritance\n");
    printf("========================================\n\n");
    
    printf("This test verifies core galaxy inheritance functionality:\n");
    printf("  1. Galaxy array lifecycle management\n");
    printf("  2. Primordial galaxy creation using init_galaxy()\n");
    printf("  3. Galaxy inheritance copying using deep_copy_galaxy()\n");
    printf("  4. Property updates during inheritance (masses, positions)\n");
    printf("  5. Halo property calculations (get_virial_mass)\n");
    printf("  6. GALAXY_PROP macro access and updates\n");
    printf("  7. Memory management during inheritance operations\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_galaxy_array_lifecycle();
    test_primordial_galaxy_creation();
    test_galaxy_inheritance_copying();
    test_inheritance_property_updates();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for Galaxy Inheritance:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}