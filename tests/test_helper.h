/**
 * @file test_helper.h
 * @brief Standardized test utilities for SAGE unit tests
 * 
 * This header provides a robust, shared test fixture that properly initializes
 * the SAGE environment for unit testing. It ensures all core parameters,
 * arrays, and data structures are in a valid state before test execution.
 */

#ifndef TEST_HELPER_H
#define TEST_HELPER_H

#include "../src/core/core_allvars.h"
#include "../src/core/galaxy_array.h"

/**
 * @brief Standardized test context structure
 * 
 * Contains all necessary SAGE data structures and parameters for unit testing,
 * properly initialized to prevent segfaults and validation failures.
 */
struct TestContext {
    struct halo_data *halos;
    struct halo_aux_data *haloaux;
    GalaxyArray *galaxies_prev_snap;
    GalaxyArray *galaxies_this_snap;
    int32_t galaxycounter;
    int nhalo;
    struct params test_params;  // Embed params directly
    double *age_array;          // Age array for proper cleanup
    int initialized;
};

/**
 * @brief Initialize a complete SAGE test environment
 * 
 * Sets up all necessary data structures and parameters for testing SAGE
 * functions. Initializes cosmological parameters, snapshot arrays, and
 * halo/galaxy arrays.
 * 
 * @param ctx Pointer to TestContext structure to initialize
 * @param nhalos Number of halos to allocate space for
 * @return 0 on success, -1 on failure
 */
int setup_test_environment(struct TestContext* ctx, int nhalos);

/**
 * @brief Clean up test environment and free all allocated memory
 * 
 * @param ctx Pointer to TestContext structure to clean up
 */
void teardown_test_environment(struct TestContext* ctx);

/**
 * @brief Create a properly initialized test halo
 * 
 * @param ctx Test context containing halo arrays
 * @param halo_idx Index in halo array to initialize
 * @param snap_num Snapshot number for this halo
 * @param mvir Virial mass in solar masses
 * @param first_prog Index of first progenitor (-1 if none)
 * @param next_prog Index of next progenitor (-1 if none)
 * @param next_in_fof Index of next halo in FOF group (-1 if last)
 */
void create_test_halo(struct TestContext* ctx, int halo_idx, int snap_num, float mvir, 
                      int first_prog, int next_prog, int next_in_fof);

/**
 * @brief Create a properly initialized test galaxy
 * 
 * @param ctx Test context containing galaxy arrays
 * @param galaxy_type Galaxy type (0=central, 1=satellite, 2=orphan)
 * @param halo_nr Halo number this galaxy belongs to
 * @param stellar_mass Stellar mass in solar masses
 * @return Galaxy index in array, or -1 on failure
 */
int create_test_galaxy(struct TestContext* ctx, int galaxy_type, int halo_nr, float stellar_mass);

/**
 * @brief Reset galaxy arrays for a new test while preserving other setup
 * 
 * @param ctx Test context to reset
 */
void reset_test_galaxies(struct TestContext* ctx);

#endif // TEST_HELPER_H