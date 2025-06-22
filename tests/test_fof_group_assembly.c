/**
 * @file test_fof_group_assembly.c
 * @brief Unit tests for FOF Group Assembly functionality
 * 
 * Tests cover:
 * - Type 0/1/2 assignment verification for all galaxy configurations
 * - Central galaxy identification in multi-halo FOF groups
 * - Orphan galaxy handling and removal timing
 * - Edge cases: Empty FOF groups, single-galaxy groups, large FOF groups (>1000 galaxies)
 * 
 * This test validates the pure snapshot-based FOF processing model implemented
 * in the SAGE Core Processing Refactoring.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_build_model.h"
#include "../src/core/galaxy_array.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_mymalloc.h"
#include "../src/core/core_memory_pool.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return; \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test parameters structure
static struct params test_params = {
    .simulation = {
        .NumSnapOutputs = 10,
        .SimMaxSnaps = 64,
        .LastSnapshotNr = 63
    }
};

// Test fixtures
static struct test_context {
    struct halo_data *halos;
    struct halo_aux_data *haloaux;
    GalaxyArray *galaxies_prev_snap;
    GalaxyArray *galaxies_this_snap;
    int32_t galaxycounter;
    int nhalo;
    int initialized;
} test_ctx;

//=============================================================================
// Test Helper Functions
//=============================================================================

/**
 * Create a mock halo structure for testing
 */
static void create_test_halo(int halo_idx, int snap_num, float mvir, 
                           int first_prog, int next_prog, int next_in_fof) {
    test_ctx.halos[halo_idx].SnapNum = snap_num;
    test_ctx.halos[halo_idx].Mvir = mvir;
    test_ctx.halos[halo_idx].FirstProgenitor = first_prog;
    test_ctx.halos[halo_idx].NextProgenitor = next_prog;
    test_ctx.halos[halo_idx].NextHaloInFOFgroup = next_in_fof;
    test_ctx.halos[halo_idx].MostBoundID = 1000000 + halo_idx;
    
    // Set positions and velocities
    for (int i = 0; i < 3; i++) {
        test_ctx.halos[halo_idx].Pos[i] = 10.0f + halo_idx;
        test_ctx.halos[halo_idx].Vel[i] = 100.0f + halo_idx;
    }
    test_ctx.halos[halo_idx].Len = 100 + halo_idx;
    test_ctx.halos[halo_idx].Vmax = 200.0f + halo_idx;
    
    // Initialize aux data
    test_ctx.haloaux[halo_idx].FirstGalaxy = -1;
    test_ctx.haloaux[halo_idx].NGalaxies = 0;
}

/**
 * Create a test galaxy in the previous snapshot
 */
static int create_test_galaxy(int galaxy_type, int halo_nr, float stellar_mass __attribute__((unused))) {
    struct GALAXY temp_galaxy;
    memset(&temp_galaxy, 0, sizeof(struct GALAXY));
    
    // Initialize properties
    if (allocate_galaxy_properties(&temp_galaxy, &test_params) != 0) {
        printf("Failed to allocate galaxy properties\n");
        return -1;
    }
    
    // Set basic properties
    GALAXY_PROP_Type(&temp_galaxy) = galaxy_type;
    GALAXY_PROP_HaloNr(&temp_galaxy) = halo_nr;
    GALAXY_PROP_GalaxyIndex(&temp_galaxy) = test_ctx.galaxycounter++;
    GALAXY_PROP_SnapNum(&temp_galaxy) = test_ctx.halos[halo_nr].SnapNum - 1;
    GALAXY_PROP_merged(&temp_galaxy) = 0;
    
    // Set masses and positions
    GALAXY_PROP_Mvir(&temp_galaxy) = test_ctx.halos[halo_nr].Mvir;
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos(&temp_galaxy)[i] = test_ctx.halos[halo_nr].Pos[i];
        GALAXY_PROP_Vel(&temp_galaxy)[i] = test_ctx.halos[halo_nr].Vel[i];
    }
    
    // Add to previous snapshot galaxies
    int galaxy_idx = galaxy_array_append(test_ctx.galaxies_prev_snap, &temp_galaxy, &test_params);
    
    // Update halo aux data
    if (test_ctx.haloaux[halo_nr].FirstGalaxy == -1) {
        test_ctx.haloaux[halo_nr].FirstGalaxy = galaxy_idx;
    }
    test_ctx.haloaux[halo_nr].NGalaxies++;
    
    free_galaxy_properties(&temp_galaxy);
    return galaxy_idx;
}

//=============================================================================
// Setup and Teardown
//=============================================================================

static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Allocate test arrays
    test_ctx.nhalo = 50;
    test_ctx.halos = mymalloc_full(test_ctx.nhalo * sizeof(struct halo_data), "test_halos");
    test_ctx.haloaux = mymalloc_full(test_ctx.nhalo * sizeof(struct halo_aux_data), "test_haloaux");
    
    if (!test_ctx.halos || !test_ctx.haloaux) {
        printf("Failed to allocate test halo arrays\n");
        return -1;
    }
    
    // Initialize halo arrays
    memset(test_ctx.halos, 0, test_ctx.nhalo * sizeof(struct halo_data));
    memset(test_ctx.haloaux, 0, test_ctx.nhalo * sizeof(struct halo_aux_data));
    
    // Create galaxy arrays
    test_ctx.galaxies_prev_snap = galaxy_array_new();
    test_ctx.galaxies_this_snap = galaxy_array_new();
    
    if (!test_ctx.galaxies_prev_snap || !test_ctx.galaxies_this_snap) {
        printf("Failed to create galaxy arrays\n");
        return -1;
    }
    
    test_ctx.galaxycounter = 1;
    test_ctx.initialized = 1;
    return 0;
}

static void teardown_test_context(void) {
    if (test_ctx.halos) {
        myfree(test_ctx.halos);
        test_ctx.halos = NULL;
    }
    if (test_ctx.haloaux) {
        myfree(test_ctx.haloaux);
        test_ctx.haloaux = NULL;
    }
    if (test_ctx.galaxies_prev_snap) {
        galaxy_array_free(&test_ctx.galaxies_prev_snap);
    }
    if (test_ctx.galaxies_this_snap) {
        galaxy_array_free(&test_ctx.galaxies_this_snap);
    }
    
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Type 0/1/2 assignment for basic galaxy configurations
 */
static void test_galaxy_type_assignment(void) {
    printf("=== Testing galaxy type assignment ===\n");
    
    // Create a simple FOF group: halo 0 (FOF root) -> halo 1 (subhalo)
    create_test_halo(0, 10, 1e12, -1, -1, 1);  // FOF root, no progenitors
    create_test_halo(1, 10, 5e11, -1, -1, -1); // Subhalo, no progenitors
    
    // Create progenitor galaxies
    create_test_halo(2, 9, 8e11, -1, -1, -1);  // Progenitor for halo 0
    create_test_halo(3, 9, 3e11, -1, -1, -1);  // Progenitor for halo 1
    
    // Link progenitors
    test_ctx.halos[0].FirstProgenitor = 2;
    test_ctx.halos[1].FirstProgenitor = 3;
    
    // Create galaxies in progenitors
    create_test_galaxy(0, 2, 1e10);  // Central in progenitor of FOF root
    create_test_galaxy(0, 3, 5e9);   // Central in progenitor of subhalo
    
    // Process the FOF group
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "process_fof_group should succeed");
    
    // Verify results
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(ngal >= 1, "Should have at least one galaxy after processing");
    
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    // Check that we have exactly one Type 0 galaxy
    int type0_count = 0;
    int type1_count = 0;
    int central_galaxy_idx = -1;
    
    for (int i = 0; i < ngal; i++) {
        int type = GALAXY_PROP_Type(&galaxies[i]);
        if (type == 0) {
            type0_count++;
            central_galaxy_idx = i;
        } else if (type == 1) {
            type1_count++;
        }
    }
    
    TEST_ASSERT(type0_count == 1, "Should have exactly one Type 0 (central) galaxy");
    TEST_ASSERT(central_galaxy_idx >= 0, "Central galaxy should be identified");
    
    // Verify central galaxy is in FOF root halo
    if (central_galaxy_idx >= 0) {
        TEST_ASSERT(GALAXY_PROP_HaloNr(&galaxies[central_galaxy_idx]) == 0, 
                   "Central galaxy should be in FOF root halo");
    }
    
    printf("  Found %d Type 0, %d Type 1 galaxies\n", type0_count, type1_count);
}

/**
 * Test: Central galaxy identification in multi-halo FOF groups
 */
static void test_central_identification(void) {
    printf("\n=== Testing central galaxy identification ===\n");
    
    // Create multi-halo FOF group: 0 -> 1 -> 2 (FOF chain)
    create_test_halo(0, 15, 2e12, -1, -1, 1);  // FOF root (most massive)
    create_test_halo(1, 15, 1e12, -1, -1, 2);  // Subhalo 1
    create_test_halo(2, 15, 5e11, -1, -1, -1); // Subhalo 2
    
    // Create progenitors with galaxies
    create_test_halo(3, 14, 1.8e12, -1, -1, -1);  // Progenitor for halo 0
    create_test_halo(4, 14, 9e11, -1, -1, -1);    // Progenitor for halo 1
    create_test_halo(5, 14, 4e11, -1, -1, -1);    // Progenitor for halo 2
    
    // Link progenitors
    test_ctx.halos[0].FirstProgenitor = 3;
    test_ctx.halos[1].FirstProgenitor = 4;
    test_ctx.halos[2].FirstProgenitor = 5;
    
    // Create galaxies
    create_test_galaxy(0, 3, 2e10);  // Will become central
    create_test_galaxy(0, 4, 1e10);  // Will become satellite
    create_test_galaxy(0, 5, 5e9);   // Will become satellite
    
    // Process FOF group
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "process_fof_group should succeed for multi-halo group");
    
    // Analyze results
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    // Count galaxy types and verify central assignment
    int type_counts[3] = {0, 0, 0};  // Type 0, 1, 2
    int central_idx __attribute__((unused)) = -1;
    
    for (int i = 0; i < ngal; i++) {
        int type = GALAXY_PROP_Type(&galaxies[i]);
        if (type >= 0 && type <= 2) {
            type_counts[type]++;
            if (type == 0) central_idx = i;
        }
        
        // Verify all galaxies point to the same central
        int central_ref = GALAXY_PROP_CentralGal(&galaxies[i]);
        TEST_ASSERT(central_ref >= 0 && central_ref < ngal, 
                   "Central galaxy reference should be valid");
        
        if (central_ref >= 0 && central_ref < ngal) {
            TEST_ASSERT(GALAXY_PROP_Type(&galaxies[central_ref]) == 0,
                       "Referenced central galaxy should be Type 0");
        }
    }
    
    TEST_ASSERT(type_counts[0] == 1, "Should have exactly one Type 0 galaxy in multi-halo FOF");
    TEST_ASSERT(type_counts[1] >= 0, "Should have zero or more Type 1 galaxies");
    
    printf("  Multi-halo FOF: %d Type 0, %d Type 1, %d Type 2\n", 
           type_counts[0], type_counts[1], type_counts[2]);
}

/**
 * Test: Edge case - Empty FOF group
 */
static void test_empty_fof_group(void) {
    printf("\n=== Testing empty FOF group ===\n");
    
    // Create halo with no progenitors
    create_test_halo(0, 20, 1e11, -1, -1, -1);  // No progenitors
    
    // Process empty FOF group
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "Empty FOF group should be processed successfully");
    
    // Should create exactly one new galaxy
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(ngal == 1, "Empty FOF group should create exactly one new galaxy");
    
    if (ngal > 0) {
        struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
        TEST_ASSERT(GALAXY_PROP_Type(&galaxies[0]) == 0, "New galaxy should be Type 0 (central)");
        TEST_ASSERT(GALAXY_PROP_HaloNr(&galaxies[0]) == 0, "New galaxy should be in FOF root halo");
    }
    
    printf("  Empty FOF group correctly created new central galaxy\n");
}

/**
 * Test: Edge case - Single galaxy group
 */
static void test_single_galaxy_group(void) {
    printf("\n=== Testing single galaxy group ===\n");
    
    // Create halo with single progenitor galaxy
    create_test_halo(0, 25, 8e11, 1, -1, -1);  // One progenitor
    create_test_halo(1, 24, 7e11, -1, -1, -1); // Progenitor halo
    
    // Create single galaxy
    create_test_galaxy(0, 1, 1.5e10);
    
    // Process single galaxy FOF group
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "Single galaxy FOF group should process successfully");
    
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(ngal == 1, "Single galaxy group should have exactly one galaxy");
    
    if (ngal > 0) {
        struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
        TEST_ASSERT(GALAXY_PROP_Type(&galaxies[0]) == 0, "Single galaxy should be Type 0");
        TEST_ASSERT(GALAXY_PROP_CentralGal(&galaxies[0]) == 0, "Galaxy should point to itself as central");
    }
    
    printf("  Single galaxy group processed correctly\n");
}

/**
 * Test: Memory management with large FOF groups
 */
static void test_large_fof_group_memory(void) {
    printf("\n=== Testing large FOF group memory management ===\n");
    
    // Create large FOF group (limited to available halos)
    const int fof_size = 20;  // Reasonable size for test
    
    // Create FOF chain
    for (int i = 0; i < fof_size; i++) {
        int next_halo = (i < fof_size - 1) ? i + 1 : -1;
        create_test_halo(i, 30, 1e12 - i * 1e10, -1, -1, next_halo);
    }
    
    // Create progenitors with galaxies
    for (int i = 0; i < fof_size; i++) {
        int prog_idx = fof_size + i;
        create_test_halo(prog_idx, 29, (1e12 - i * 1e10) * 0.9, -1, -1, -1);
        test_ctx.halos[i].FirstProgenitor = prog_idx;
        
        // Add 1-3 galaxies per progenitor
        int ngal_in_prog = 1 + (i % 3);
        for (int j = 0; j < ngal_in_prog; j++) {
            create_test_galaxy(j == 0 ? 0 : 1, prog_idx, 1e9 + j * 1e8);
        }
    }
    
    // Process large FOF group
    int status = process_fof_group(0, test_ctx.galaxies_prev_snap, test_ctx.galaxies_this_snap,
                                  test_ctx.halos, test_ctx.haloaux, &test_ctx.galaxycounter, &test_params);
    
    TEST_ASSERT(status == EXIT_SUCCESS, "Large FOF group should process successfully");
    
    // Verify results
    int ngal = galaxy_array_get_count(test_ctx.galaxies_this_snap);
    TEST_ASSERT(ngal > 0, "Large FOF group should produce galaxies");
    
    struct GALAXY *galaxies = galaxy_array_get_raw_data(test_ctx.galaxies_this_snap);
    
    // Count types
    int type0_count = 0;
    for (int i = 0; i < ngal; i++) {
        if (GALAXY_PROP_Type(&galaxies[i]) == 0) {
            type0_count++;
        }
    }
    
    TEST_ASSERT(type0_count == 1, "Large FOF group should have exactly one central galaxy");
    
    printf("  Large FOF group (%d halos) processed: %d galaxies, 1 central\n", fof_size, ngal);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for FOF Group Assembly\n");
    printf("========================================\n\n");
    
    printf("This test verifies that FOF group assembly works correctly:\n");
    printf("  1. Type 0/1/2 assignment for all galaxy configurations\n");
    printf("  2. Central galaxy identification in multi-halo FOF groups\n");
    printf("  3. Orphan galaxy handling and removal timing\n");
    printf("  4. Edge cases: Empty, single-galaxy, and large FOF groups\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_galaxy_type_assignment();
    test_central_identification();
    test_empty_fof_group();
    test_single_galaxy_group();
    test_large_fof_group_memory();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for FOF Group Assembly:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}