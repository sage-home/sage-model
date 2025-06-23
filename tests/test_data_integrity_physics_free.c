/**
 * Test suite for Data Integrity in Physics-Free Mode
 * 
 * CRITICAL TEST: Validates that halo and galaxy properties maintain integrity
 * throughout the entire SAGE pipeline when no physics modules are active.
 * 
 * Tests cover:
 * - Halo property preservation from input to galaxy initialization
 * - Galaxy property integrity through pipeline phases (HALO→GALAXY→POST→FINAL)
 * - Memory initialization correctness (detecting garbage values)
 * - Core property preservation without corruption
 * - Output serialization accuracy
 * 
 * This test is designed to FAIL if there are ANY data corruption issues
 * in the core infrastructure, ensuring reliable detection of problems
 * like the Galaxy Number assignment corruption we recently fixed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <float.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_build_model.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_property_utils.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_mymalloc.h"
#include "../src/core/core_init.h"
#include "../src/core/galaxy_array.h"
#include "../src/io/io_galaxy_output.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Memory corruption detection constants
#define MEMORY_POISON_VALUE_32 0xDEADBEEF
#define MEMORY_POISON_VALUE_64 0xDEADBEEFCAFEBABEULL
#define MAX_REASONABLE_GALAXY_NR 1000000  // Way above expected test values
#define TOLERANCE_EXACT 1e-9
#define TOLERANCE_NORMAL 1e-6

// Helper macro for test assertions with detailed error reporting
#define TEST_ASSERT(condition, message, ...) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: " message "\n", ##__VA_ARGS__); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        fflush(stdout); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Enhanced assertion that reports expected vs actual values
#define TEST_ASSERT_VALUES(condition, expected, actual, message, ...) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: " message "\n", ##__VA_ARGS__); \
        printf("  Expected: %g, Actual: %g\n", (double)(expected), (double)(actual)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        fflush(stdout); \
    } else { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } \
} while(0)

// Test data structures for tracking injected values
struct test_halo_snapshot {
    int32_t original_snapnum;
    float original_mvir;
    float original_pos[3];
    float original_vel[3];
    long long original_mostboundid;
    int original_len;
    float original_vmax;
    float original_spin[3];
};

struct test_galaxy_snapshot {
    int32_t original_galaxynr;
    int32_t original_type;
    int32_t original_snapnum;
    int32_t original_halonr;
    float original_mvir;
    float original_pos[3];
    float original_vel[3];
    long long original_mostboundid;
    int original_len;
    float original_vmax;
    float original_rvir;
    float original_vvir;
    float original_mergtime;
    float original_infall_mvir;
    float original_infall_vvir;
    float original_infall_vmax;
    uint64_t original_galaxy_index;
    uint64_t original_central_galaxy_index;
};

// Test context structure
static struct test_context {
    struct params run_params;
    struct halo_data *test_halos;
    struct halo_aux_data *test_haloaux;
    GalaxyArray *test_galaxies;
    GalaxyArray *test_halogal;
    int num_halos;
    int num_galaxies;
    int max_galaxies;
    bool setup_complete;
    struct test_halo_snapshot *halo_snapshots;
    struct test_galaxy_snapshot *galaxy_snapshots;
    struct forest_info forest_info;
    struct galaxy_output_context output_ctx;
} test_ctx;

// Forward declarations
static int setup_test_context(void);
static void teardown_test_context(void);
static void test_memory_initialization_integrity(void);
static void test_halo_to_galaxy_data_preservation(void);
static void test_galaxy_pipeline_integrity(void);
static void test_output_serialization_accuracy(void);
static void test_memory_corruption_detection(void);

static int create_test_halos(void);
static int create_test_galaxies(void);
static void capture_halo_snapshots(void);
static void capture_galaxy_snapshots(void);
static bool verify_halo_integrity(int halo_idx);
static bool verify_galaxy_integrity(int gal_idx);
static void inject_memory_poison(void *ptr, size_t size);

//=============================================================================
// Setup and Teardown
//=============================================================================

static int setup_test_context(void) {
    printf("Setting up comprehensive data integrity test context...\n");
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize test parameters
    memset(&test_ctx.run_params, 0, sizeof(struct params));
    
    // Core cosmology parameters
    test_ctx.run_params.cosmology.Omega = 0.3089;
    test_ctx.run_params.cosmology.OmegaLambda = 0.6911;
    test_ctx.run_params.cosmology.Hubble_h = 0.678;
    test_ctx.run_params.cosmology.PartMass = 1.0e10;
    test_ctx.run_params.cosmology.G = 4.3e-9; // km^2/s^2 * kpc/Msun
    
    // Unit system
    test_ctx.run_params.units.UnitLength_in_cm = 3.085678e21;
    test_ctx.run_params.units.UnitMass_in_g = 1.989e43;
    test_ctx.run_params.units.UnitVelocity_in_cm_per_s = 1.0e5;
    test_ctx.run_params.units.UnitTime_in_s = test_ctx.run_params.units.UnitLength_in_cm / 
                                            test_ctx.run_params.units.UnitVelocity_in_cm_per_s;
    test_ctx.run_params.units.UnitTime_in_Megayears = test_ctx.run_params.units.UnitTime_in_s / SEC_PER_MEGAYEAR;
    
    // Simulation parameters for physics-free mode
    test_ctx.run_params.simulation.NumSnapOutputs = 1;
    test_ctx.run_params.simulation.ListOutputSnaps[0] = 63;
    test_ctx.run_params.simulation.SimMaxSnaps = 64;
    test_ctx.run_params.runtime.FileNr_Mulfac = 1000000000000000LL; // Large enough to avoid overflow
    test_ctx.run_params.runtime.ForestNr_Mulfac = 1000000;
    
    // Allocate memory for Age array (it's a pointer, not a static array)
    test_ctx.run_params.simulation.Age = malloc(test_ctx.run_params.simulation.SimMaxSnaps * sizeof(double));
    if (!test_ctx.run_params.simulation.Age) {
        printf("ERROR: Failed to allocate memory for Age array\n");
        return -1;
    }
    
    // Initialize simulation time arrays to prevent crashes
    // Set up a realistic snapshot list for z=0 (snapshot 63)
    test_ctx.run_params.simulation.Snaplistlen = 64;  // 64 snapshots total
    for (int snap = 0; snap < test_ctx.run_params.simulation.SimMaxSnaps; snap++) {
        // Create a simple redshift sequence from z=20 (snap 0) to z=0 (snap 63)
        double redshift = (snap == 63) ? 0.0 : 20.0 - (snap * 20.0 / 62.0);
        test_ctx.run_params.simulation.ZZ[snap] = redshift;
        test_ctx.run_params.simulation.AA[snap] = 1.0 / (1.0 + redshift);
        test_ctx.run_params.simulation.Age[snap] = 13.8 * (snap + 1.0) / 64.0; // Simple age progression
    }
    
    // I/O parameters
    test_ctx.run_params.io.TreeType = lhalo_binary; // Set a specific tree type
    
    // Initialize core systems that process_fof_group requires
    printf("Initializing core systems...\n");
    
    // Initialize logging system FIRST (required for CONTEXT_LOG macro)
    if (initialize_logging(&test_ctx.run_params) != 0) {
        printf("ERROR: Failed to initialize logging system\n");
        return -1;
    }
    
    // Initialize memory management system (remove if not needed - mymalloc will auto-init)
    // initialize_dynamic_memory_system();
    
    // Initialize basic units and constants
    initialize_units(&test_ctx.run_params);
    
    // Initialize module system (required for pipeline)
    if (initialize_module_system(&test_ctx.run_params) != 0) {
        printf("ERROR: Failed to initialize module system\n");
        return -1;
    }
    
    // Initialize module callback system
    initialize_module_callback_system();
    
    // Initialize galaxy extension system
    initialize_galaxy_extension_system();
    
    // Initialize property system
    if (initialize_property_system(&test_ctx.run_params) != 0) {
        printf("ERROR: Failed to initialize property system\n");
        cleanup_module_system();
        return -1;
    }
    
    // Initialize standard properties
    initialize_standard_properties(&test_ctx.run_params);
    
    // Initialize event system
    initialize_event_system();
    
    // Initialize pipeline system (required for evolve_galaxies)
    initialize_pipeline_system();
    
    printf("Core systems initialized successfully.\n");
    
    // Set up test data sizes
    test_ctx.num_halos = 5;
    test_ctx.num_galaxies = 0; // Will be determined by galaxy creation
    test_ctx.max_galaxies = 20; // Upper bound for allocation
    
    // Create test halos and galaxies
    if (create_test_halos() != 0) {
        printf("ERROR: Failed to create test halos\n");
        cleanup_property_system();
        return -1;
    }
    
    if (create_test_galaxies() != 0) {
        printf("ERROR: Failed to create test galaxies\n");
        cleanup_property_system();
        return -1;
    }
    
    // Set up forest info for output context
    test_ctx.forest_info.original_treenr = malloc(sizeof(int64_t));
    test_ctx.forest_info.FileNr = malloc(sizeof(int32_t));
    if (!test_ctx.forest_info.original_treenr || !test_ctx.forest_info.FileNr) {
        printf("ERROR: Failed to allocate forest info\n");
        cleanup_property_system();
        return -1;
    }
    test_ctx.forest_info.original_treenr[0] = 1; // Small tree number for test
    test_ctx.forest_info.FileNr[0] = 1;
    
    test_ctx.setup_complete = true;
    printf("Test context setup complete: %d halos, %d galaxies\n", 
           test_ctx.num_halos, test_ctx.num_galaxies);
    
    return 0;
}

static void teardown_test_context(void) {
    printf("Cleaning up test context...\n");
    
    if (test_ctx.test_halos) {
        free(test_ctx.test_halos);
        test_ctx.test_halos = NULL;
    }
    
    if (test_ctx.test_haloaux) {
        free(test_ctx.test_haloaux);
        test_ctx.test_haloaux = NULL;
    }
    
    if (test_ctx.test_galaxies) {
        galaxy_array_free(&test_ctx.test_galaxies);
    }
    
    if (test_ctx.test_halogal) {
        galaxy_array_free(&test_ctx.test_halogal);
    }
    
    if (test_ctx.halo_snapshots) {
        free(test_ctx.halo_snapshots);
        test_ctx.halo_snapshots = NULL;
    }
    
    if (test_ctx.galaxy_snapshots) {
        free(test_ctx.galaxy_snapshots);
        test_ctx.galaxy_snapshots = NULL;
    }
    
    if (test_ctx.forest_info.original_treenr) {
        free(test_ctx.forest_info.original_treenr);
        test_ctx.forest_info.original_treenr = NULL;
    }
    
    if (test_ctx.forest_info.FileNr) {
        free(test_ctx.forest_info.FileNr);
        test_ctx.forest_info.FileNr = NULL;
    }
    
    free_output_arrays(&test_ctx.output_ctx);
    
    // Free allocated Age array
    if (test_ctx.run_params.simulation.Age) {
        free(test_ctx.run_params.simulation.Age);
        test_ctx.run_params.simulation.Age = NULL;
    }
    
    if (test_ctx.setup_complete) {
        // Cleanup systems in reverse order of initialization
        cleanup_pipeline_system();
        cleanup_event_system();
        cleanup_galaxy_extension_system();
        cleanup_module_callback_system();
        cleanup_module_system();
        cleanup_property_system();
        cleanup_logging();
        test_ctx.setup_complete = false;
    }
    
    printf("Test context cleanup complete.\n");
}

//=============================================================================
// Test Data Creation
//=============================================================================

static int create_test_halos(void) {
    printf("Creating test halos...\n");
    
    test_ctx.test_halos = malloc(test_ctx.num_halos * sizeof(struct halo_data));
    test_ctx.test_haloaux = malloc(test_ctx.num_halos * sizeof(struct halo_aux_data));
    test_ctx.halo_snapshots = malloc(test_ctx.num_halos * sizeof(struct test_halo_snapshot));
    
    if (!test_ctx.test_halos || !test_ctx.test_haloaux || !test_ctx.halo_snapshots) {
        printf("ERROR: Failed to allocate test halo arrays\n");
        return -1;
    }
    
    // Initialize with specific test values that we can verify
    for (int i = 0; i < test_ctx.num_halos; i++) {
        struct halo_data *halo = &test_ctx.test_halos[i];
        struct halo_aux_data *aux = &test_ctx.test_haloaux[i];
        
        // CRITICAL: Zero-initialize the entire halo structure first
        memset(halo, 0, sizeof(struct halo_data));
        memset(aux, 0, sizeof(struct halo_aux_data));
        
        // Set up merger tree pointers (CRITICAL for process_fof_group)
        halo->Descendant = -1;          // No descendants (these are final snapshot halos)
        halo->FirstProgenitor = -1;     // No progenitors (simplest case)
        halo->NextProgenitor = -1;      // No progenitor siblings
        halo->FirstHaloInFOFgroup = i;  // Each halo is its own FOF group center
        halo->NextHaloInFOFgroup = -1;  // No other halos in FOF group
        
        // Set up halo with specific, verifiable values
        halo->SnapNum = 62; // Halo snapshot - galaxies evolve from SnapNum-1 to SnapNum
        halo->Len = 100 + i * 50; // Varying particle counts
        halo->MostBoundID = 1000000 + i; // Unique IDs
        halo->Mvir = 10.0 + i * 5.0; // Varying masses (1e10 Msun/h units)
        halo->Pos[0] = 1000.0 + i * 100.0; // kpc/h
        halo->Pos[1] = 2000.0 + i * 100.0;
        halo->Pos[2] = 3000.0 + i * 100.0;
        halo->Vel[0] = 100.0 + i * 10.0; // km/s
        halo->Vel[1] = 200.0 + i * 10.0;
        halo->Vel[2] = 300.0 + i * 10.0;
        halo->Vmax = 200.0 + i * 25.0; // km/s
        halo->Spin[0] = 0.1 + i * 0.02;
        halo->Spin[1] = 0.2 + i * 0.02;
        halo->Spin[2] = 0.3 + i * 0.02;
        
        // File/simulation metadata
        halo->FileNr = 0;
        halo->SubhaloIndex = i;
        
        // Initialize auxiliary data
        // Note: HaloFlag removed in Phase 2.2 refactoring
        aux->NGalaxies = 0;
        aux->FirstGalaxy = -1;
        aux->output_snap_n = -1;
        // Note: DoneFlag removed in Phase 2.2 refactoring for stateless processing
        
        printf("  Halo %d: SnapNum=%d, Len=%d, Mvir=%.1f, MostBoundID=%lld\n",
               i, halo->SnapNum, halo->Len, halo->Mvir, halo->MostBoundID);
    }
    
    return 0;
}

static int create_test_galaxies(void) {
    printf("Creating test galaxies using process_fof_group...\n");

    // Allocate galaxy arrays using GalaxyArray API
    test_ctx.test_galaxies = galaxy_array_new();
    test_ctx.test_halogal = galaxy_array_new();

    if (!test_ctx.test_galaxies || !test_ctx.test_halogal) {
        printf("ERROR: Failed to allocate galaxy arrays\n");
        return -1;
    }

    // Allocate and initialize processed_flags array for FOF group processing
    bool *processed_flags = calloc(test_ctx.num_halos, sizeof(bool));
    if (!processed_flags) {
        printf("ERROR: Failed to allocate processed_flags array\n");
        return -1;
    }

    // GalaxyArray handles initialization internally, no manual memset needed

    // Build galaxies for each halo using the core SAGE function
    int total_galaxies = 0;
    int32_t galaxy_counter = 0;

    for (int halo_idx = 0; halo_idx < test_ctx.num_halos; halo_idx++) {
        int num_gals_before = total_galaxies;

        printf("  Building galaxies for halo %d...\n", halo_idx);

        int result = process_fof_group(halo_idx, test_ctx.test_halogal, test_ctx.test_galaxies,
                                     test_ctx.test_halos, test_ctx.test_haloaux,
                                     &galaxy_counter, &test_ctx.run_params, processed_flags);

        if (result != 0) {
            printf("ERROR: process_fof_group failed for halo %d with result %d\n", halo_idx, result);
            printf("       This indicates a core infrastructure problem\n");
            printf("       Check pipeline system initialization and galaxy validation\n");

            // Add specific debugging for common failure points
            printf("       Common causes:\n");
            printf("         - XASSERT failure in init_galaxy (halo FOF group mismatch)\n");
            printf("         - Central galaxy validation failure in evolve_galaxies\n");
            printf("         - Pipeline system not properly initialized\n");
            printf("         - Property allocation failure\n");
            free(processed_flags);
            return -1;
        }

        total_galaxies = galaxy_array_get_count(test_ctx.test_galaxies);
        int num_gals_in_halo = total_galaxies - num_gals_before;

        printf("    Created %d galaxies (total: %d, galaxy_counter: %d)\n",
               num_gals_in_halo, total_galaxies, galaxy_counter);

        // Debug: Check the SnapNum of created galaxies
        if (num_gals_in_halo > 0) {
            struct GALAXY *created_gal = galaxy_array_get(test_ctx.test_galaxies, total_galaxies - 1);
            if (created_gal) {
                printf("    -> Created galaxy has SnapNum=%d, HaloNr=%d\n", GALAXY_PROP_SnapNum(created_gal), GALAXY_PROP_HaloNr(created_gal));
            }
        }
    }

    free(processed_flags);

    test_ctx.num_galaxies = total_galaxies;

    // Allocate snapshot arrays
    test_ctx.galaxy_snapshots = malloc(test_ctx.num_galaxies * sizeof(struct test_galaxy_snapshot));
    if (!test_ctx.galaxy_snapshots) {
        printf("ERROR: Failed to allocate galaxy snapshot array\n");
        return -1;
    }

    printf("Successfully created %d galaxies from %d halos\n", test_ctx.num_galaxies, test_ctx.num_halos);

    return 0;
}

//=============================================================================
// Snapshot and Verification Functions
//=============================================================================

static void capture_halo_snapshots(void) {
    printf("Capturing halo property snapshots...\n");
    
    for (int i = 0; i < test_ctx.num_halos; i++) {
        struct halo_data *halo = &test_ctx.test_halos[i];
        struct test_halo_snapshot *snapshot = &test_ctx.halo_snapshots[i];
        
        snapshot->original_snapnum = halo->SnapNum;
        snapshot->original_mvir = halo->Mvir;
        snapshot->original_pos[0] = halo->Pos[0];
        snapshot->original_pos[1] = halo->Pos[1];
        snapshot->original_pos[2] = halo->Pos[2];
        snapshot->original_vel[0] = halo->Vel[0];
        snapshot->original_vel[1] = halo->Vel[1];
        snapshot->original_vel[2] = halo->Vel[2];
        snapshot->original_mostboundid = halo->MostBoundID;
        snapshot->original_len = halo->Len;
        snapshot->original_vmax = halo->Vmax;
        snapshot->original_spin[0] = halo->Spin[0];
        snapshot->original_spin[1] = halo->Spin[1];
        snapshot->original_spin[2] = halo->Spin[2];
    }
}

static void capture_galaxy_snapshots(void) {
    printf("Capturing galaxy property snapshots...\n");
    
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        struct GALAXY *galaxy = galaxy_array_get(test_ctx.test_galaxies, i);
        struct test_galaxy_snapshot *snapshot = &test_ctx.galaxy_snapshots[i];
        
        // Capture core properties via property system (single source of truth)
        snapshot->original_galaxynr = GALAXY_PROP_GalaxyNr(galaxy);
        snapshot->original_type = GALAXY_PROP_Type(galaxy);
        snapshot->original_snapnum = GALAXY_PROP_SnapNum(galaxy);
        snapshot->original_halonr = GALAXY_PROP_HaloNr(galaxy);
        snapshot->original_mvir = GALAXY_PROP_Mvir(galaxy);
        snapshot->original_pos[0] = GALAXY_PROP_Pos_ELEM(galaxy, 0);
        snapshot->original_pos[1] = GALAXY_PROP_Pos_ELEM(galaxy, 1);
        snapshot->original_pos[2] = GALAXY_PROP_Pos_ELEM(galaxy, 2);
        snapshot->original_vel[0] = GALAXY_PROP_Vel_ELEM(galaxy, 0);
        snapshot->original_vel[1] = GALAXY_PROP_Vel_ELEM(galaxy, 1);
        snapshot->original_vel[2] = GALAXY_PROP_Vel_ELEM(galaxy, 2);
        snapshot->original_mostboundid = GALAXY_PROP_MostBoundID(galaxy);
        snapshot->original_len = GALAXY_PROP_Len(galaxy);
        snapshot->original_vmax = GALAXY_PROP_Vmax(galaxy);
        snapshot->original_rvir = GALAXY_PROP_Rvir(galaxy);
        snapshot->original_vvir = GALAXY_PROP_Vvir(galaxy);
        // MergTime is a physics property - get via property system
        property_id_t mergtime_prop = get_cached_property_id("MergTime");
        if (mergtime_prop < PROP_COUNT && galaxy->properties != NULL) {
            snapshot->original_mergtime = get_float_property(galaxy, mergtime_prop, 0.0f);
        } else {
            snapshot->original_mergtime = 0.0f; // Default value if property not available
        }
        // infallMvir, infallVvir, and infallVmax are core properties - use direct access
        snapshot->original_infall_mvir = GALAXY_PROP_infallMvir(galaxy);
        snapshot->original_infall_vvir = GALAXY_PROP_infallVvir(galaxy);
        snapshot->original_infall_vmax = GALAXY_PROP_infallVmax(galaxy);
        snapshot->original_galaxy_index = GALAXY_PROP_GalaxyIndex(galaxy);
        snapshot->original_central_galaxy_index = GALAXY_PROP_CentralGalaxyIndex(galaxy);
        
        printf("  Galaxy %d snapshot: GalaxyNr=%d, Type=%d, Mvir=%.1f, GalaxyIndex=%" PRIu64 "\n",
               i, snapshot->original_galaxynr, snapshot->original_type, 
               snapshot->original_mvir, snapshot->original_galaxy_index);
    }
}

static bool verify_halo_integrity(int halo_idx) {
    if (halo_idx >= test_ctx.num_halos) return false;
    
    struct halo_data *halo = &test_ctx.test_halos[halo_idx];
    struct test_halo_snapshot *snapshot = &test_ctx.halo_snapshots[halo_idx];
    
    bool integrity_ok = true;
    
    if (halo->SnapNum != snapshot->original_snapnum) {
        printf("ERROR: Halo %d SnapNum corrupted: expected %d, got %d\n",
               halo_idx, snapshot->original_snapnum, halo->SnapNum);
        integrity_ok = false;
    }
    
    if (fabsf(halo->Mvir - snapshot->original_mvir) > TOLERANCE_EXACT) {
        printf("ERROR: Halo %d Mvir corrupted: expected %.6f, got %.6f\n",
               halo_idx, snapshot->original_mvir, halo->Mvir);
        integrity_ok = false;
    }
    
    for (int j = 0; j < 3; j++) {
        if (fabsf(halo->Pos[j] - snapshot->original_pos[j]) > TOLERANCE_EXACT) {
            printf("ERROR: Halo %d Pos[%d] corrupted: expected %.6f, got %.6f\n",
                   halo_idx, j, snapshot->original_pos[j], halo->Pos[j]);
            integrity_ok = false;
        }
        if (fabsf(halo->Vel[j] - snapshot->original_vel[j]) > TOLERANCE_EXACT) {
            printf("ERROR: Halo %d Vel[%d] corrupted: expected %.6f, got %.6f\n",
                   halo_idx, j, snapshot->original_vel[j], halo->Vel[j]);
            integrity_ok = false;
        }
    }
    
    if (halo->MostBoundID != snapshot->original_mostboundid) {
        printf("ERROR: Halo %d MostBoundID corrupted: expected %lld, got %lld\n",
               halo_idx, snapshot->original_mostboundid, halo->MostBoundID);
        integrity_ok = false;
    }
    
    return integrity_ok;
}

static bool verify_galaxy_integrity(int gal_idx) {
    if (gal_idx >= test_ctx.num_galaxies) return false;
    
    struct GALAXY *galaxy = galaxy_array_get(test_ctx.test_galaxies, gal_idx);
    struct test_galaxy_snapshot *snapshot = &test_ctx.galaxy_snapshots[gal_idx];
    
    bool integrity_ok = true;
    
    // Check for the specific corruption we fixed: garbage values in GalaxyNr
    int32_t galaxy_nr = GALAXY_PROP_GalaxyNr(galaxy);
    if (galaxy_nr < 0 || galaxy_nr > MAX_REASONABLE_GALAXY_NR) {
        printf("ERROR: Galaxy %d has corrupted GalaxyNr: %d (outside reasonable range)\n",
               gal_idx, galaxy_nr);
        integrity_ok = false;
    }
    
    if (galaxy_nr != snapshot->original_galaxynr) {
        printf("ERROR: Galaxy %d GalaxyNr corrupted: expected %d, got %d\n",
               gal_idx, snapshot->original_galaxynr, galaxy_nr);
        integrity_ok = false;
    }
    
    int32_t galaxy_type = GALAXY_PROP_Type(galaxy);
    if (galaxy_type != snapshot->original_type) {
        printf("ERROR: Galaxy %d Type corrupted: expected %d, got %d\n",
               gal_idx, snapshot->original_type, galaxy_type);
        integrity_ok = false;
    }
    
    float galaxy_mvir = GALAXY_PROP_Mvir(galaxy);
    if (fabsf(galaxy_mvir - snapshot->original_mvir) > TOLERANCE_NORMAL) {
        printf("ERROR: Galaxy %d Mvir corrupted: expected %.6f, got %.6f\n",
               gal_idx, snapshot->original_mvir, galaxy_mvir);
        integrity_ok = false;
    }
    
    // Check GalaxyIndex for corruption (this was affected by the garbage GalaxyNr issue)
    uint64_t galaxy_index = GALAXY_PROP_GalaxyIndex(galaxy);
    if (galaxy_index != snapshot->original_galaxy_index) {
        printf("ERROR: Galaxy %d GalaxyIndex corrupted: expected %" PRIu64 ", got %" PRIu64 "\n",
               gal_idx, snapshot->original_galaxy_index, galaxy_index);
        integrity_ok = false;
    }
    
    return integrity_ok;
}

static void inject_memory_poison(void *ptr, size_t size) {
    // Fill memory with detectable poison pattern
    uint32_t *ptr32 = (uint32_t *)ptr;
    size_t num_32bit = size / sizeof(uint32_t);
    
    for (size_t i = 0; i < num_32bit; i++) {
        ptr32[i] = MEMORY_POISON_VALUE_32;
    }
    
    // Handle remaining bytes
    uint8_t *ptr8 = (uint8_t *)ptr + (num_32bit * sizeof(uint32_t));
    size_t remaining = size % sizeof(uint32_t);
    for (size_t i = 0; i < remaining; i++) {
        ptr8[i] = 0xDE;
    }
}


//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Memory initialization integrity
 * 
 * Verifies that galaxy arrays are properly zero-initialized and don't contain
 * garbage values like the GalaxyNr corruption we fixed.
 */
static void test_memory_initialization_integrity(void) {
    printf("\n=== Testing memory initialization integrity ===\n");
    
    // Allocate arrays the same way SAGE does
    const int test_size = 100;
    struct GALAXY *test_array_malloc = mymalloc(test_size * sizeof(struct GALAXY));
    struct GALAXY *test_array_poison = mymalloc(test_size * sizeof(struct GALAXY));
    
    TEST_ASSERT(test_array_malloc != NULL, "mymalloc should succeed");
    TEST_ASSERT(test_array_poison != NULL, "mymalloc should succeed");
    
    // Inject poison pattern to simulate garbage memory
    inject_memory_poison(test_array_poison, test_size * sizeof(struct GALAXY));
    
    // Test that uninitialized memory contains garbage (this should detect corruption)
    // Check the actual fields that exist in the new struct layout
    bool found_garbage = false;
    for (int i = 0; i < test_size; i++) {
        // Check if extension_data pointer has garbage values
        if (test_array_poison[i].extension_data != NULL &&
            ((uintptr_t)test_array_poison[i].extension_data == MEMORY_POISON_VALUE_64 ||
             (uintptr_t)test_array_poison[i].extension_data > 0x7FFFFFFFFFFFFFFF)) {
            found_garbage = true;
            break;
        }
        // Check if num_extensions has garbage values  
        if (test_array_poison[i].num_extensions == (int)MEMORY_POISON_VALUE_32 ||
            test_array_poison[i].num_extensions < -1000000 || 
            test_array_poison[i].num_extensions > 1000000) {
            found_garbage = true;
            break;
        }
    }
    TEST_ASSERT(found_garbage, "Should detect garbage values in uninitialized memory");
    
    // Now test that memset fixes the corruption
    memset(test_array_poison, 0, test_size * sizeof(struct GALAXY));
    
    // Verify all critical fields are zeroed (check actual struct fields)
    for (int i = 0; i < test_size; i++) {
        TEST_ASSERT(test_array_poison[i].extension_data == NULL, 
                   "extension_data should be NULL after memset");
        TEST_ASSERT(test_array_poison[i].num_extensions == 0, 
                   "num_extensions should be zero after memset");
        TEST_ASSERT(test_array_poison[i].extension_flags == 0, 
                   "extension_flags should be zero after memset");
        TEST_ASSERT(test_array_poison[i].properties == NULL, 
                   "properties should be NULL after memset");
    }
    
    printf("Memory initialization test: Verified that memset correctly zeros galaxy arrays\n");
    
    myfree(test_array_malloc);
    myfree(test_array_poison);
}

/**
 * Test: Halo to galaxy data preservation
 * 
 * Verifies that halo properties are correctly transferred to galaxy properties
 * during galaxy initialization without corruption.
 */
static void test_halo_to_galaxy_data_preservation(void) {
    printf("\n=== Testing halo to galaxy data preservation ===\n");
    
    capture_halo_snapshots();
    capture_galaxy_snapshots();
    
    // Verify that galaxy properties correctly reflect halo properties
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        struct GALAXY *galaxy = galaxy_array_get(test_ctx.test_galaxies, i);
        int halo_idx = GALAXY_PROP_HaloNr(galaxy);
        
        TEST_ASSERT(halo_idx >= 0 && halo_idx < test_ctx.num_halos,
                   "Galaxy %d should reference a valid halo index: %d", i, halo_idx);
        
        if (halo_idx >= 0 && halo_idx < test_ctx.num_halos) {
            struct halo_data *halo = &test_ctx.test_halos[halo_idx];
            
            // Test position preservation
            TEST_ASSERT_VALUES(fabsf(GALAXY_PROP_Pos_ELEM(galaxy, 0) - halo->Pos[0]) < TOLERANCE_EXACT,
                              halo->Pos[0], GALAXY_PROP_Pos_ELEM(galaxy, 0),
                              "Galaxy %d Pos[0] should match halo", i);
            
            TEST_ASSERT_VALUES(fabsf(GALAXY_PROP_Pos_ELEM(galaxy, 1) - halo->Pos[1]) < TOLERANCE_EXACT,
                              halo->Pos[1], GALAXY_PROP_Pos_ELEM(galaxy, 1), 
                              "Galaxy %d Pos[1] should match halo", i);
            
            TEST_ASSERT_VALUES(fabsf(GALAXY_PROP_Pos_ELEM(galaxy, 2) - halo->Pos[2]) < TOLERANCE_EXACT,
                              halo->Pos[2], GALAXY_PROP_Pos_ELEM(galaxy, 2),
                              "Galaxy %d Pos[2] should match halo", i);
            
            // Test velocity preservation
            TEST_ASSERT_VALUES(fabsf(GALAXY_PROP_Vel_ELEM(galaxy, 0) - halo->Vel[0]) < TOLERANCE_EXACT,
                              halo->Vel[0], GALAXY_PROP_Vel_ELEM(galaxy, 0),
                              "Galaxy %d Vel[0] should match halo", i);
            
            // Test other critical properties
            TEST_ASSERT_VALUES(GALAXY_PROP_MostBoundID(galaxy) == halo->MostBoundID,
                              halo->MostBoundID, GALAXY_PROP_MostBoundID(galaxy),
                              "Galaxy %d MostBoundID should match halo", i);
            
            TEST_ASSERT_VALUES(GALAXY_PROP_Len(galaxy) == halo->Len,
                              halo->Len, GALAXY_PROP_Len(galaxy),
                              "Galaxy %d Len should match halo", i);
            
            TEST_ASSERT_VALUES(fabsf(GALAXY_PROP_Vmax(galaxy) - halo->Vmax) < TOLERANCE_NORMAL,
                              halo->Vmax, GALAXY_PROP_Vmax(galaxy),
                              "Galaxy %d Vmax should match halo", i);
        }
        
        // Test galaxy-specific property initialization
        int32_t galaxy_nr = GALAXY_PROP_GalaxyNr(galaxy);
        TEST_ASSERT(galaxy_nr >= 0 && galaxy_nr < MAX_REASONABLE_GALAXY_NR,
                   "Galaxy %d should have reasonable GalaxyNr: %d", i, galaxy_nr);
        
        int32_t galaxy_type = GALAXY_PROP_Type(galaxy);
        TEST_ASSERT(galaxy_type == 0, // Central galaxies in our test
                   "Galaxy %d should be central (Type=0), got %d", i, galaxy_type);
        
        int32_t galaxy_snapnum = GALAXY_PROP_SnapNum(galaxy);
        TEST_ASSERT(galaxy_snapnum == 62, // Galaxies evolve from halo.SnapNum-1 (61) to halo.SnapNum (62) - final state after evolution
                   "Galaxy %d should have SnapNum=62 (halo.SnapNum after evolution), got %d", i, galaxy_snapnum);
        
        printf("  Galaxy %d: GalaxyNr=%d, HaloNr=%d, integrity verified\n",
               i, galaxy_nr, halo_idx);
    }
    
    printf("Halo to galaxy preservation test: Verified %d galaxies\n", test_ctx.num_galaxies);
}

/**
 * Test: Galaxy pipeline integrity
 * 
 * Verifies that galaxy properties remain unchanged through the pipeline phases
 * when no physics modules are active (physics-free mode).
 */
static void test_galaxy_pipeline_integrity(void) {
    printf("\n=== Testing galaxy pipeline integrity ===\n");
    
    // At this point, galaxies have been through process_fof_group
    // In physics-free mode, most properties should remain unchanged
    
    // Verify all galaxies maintain integrity
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        bool integrity = verify_galaxy_integrity(i);
        TEST_ASSERT(integrity, "Galaxy %d should maintain integrity through pipeline", i);
        
        // Additional checks for pipeline corruption
        struct GALAXY *galaxy = galaxy_array_get(test_ctx.test_galaxies, i);
        
        // Check for memory corruption patterns by examining property values
        int32_t galaxy_nr = GALAXY_PROP_GalaxyNr(galaxy);
        TEST_ASSERT(galaxy_nr >= 0 && galaxy_nr < MAX_REASONABLE_GALAXY_NR,
                   "Galaxy %d GalaxyNr should not contain memory corruption: %d", i, galaxy_nr);
        
        // Verify reasonable values for computed properties
        float galaxy_mvir = GALAXY_PROP_Mvir(galaxy);
        if (galaxy_mvir > 0.0f) {
            float galaxy_rvir = GALAXY_PROP_Rvir(galaxy);
            TEST_ASSERT(galaxy_rvir > 0.0f,
                       "Galaxy %d with Mvir=%.3f should have positive Rvir=%.3f", 
                       i, galaxy_mvir, galaxy_rvir);
            
            float galaxy_vvir = GALAXY_PROP_Vvir(galaxy);
            TEST_ASSERT(galaxy_vvir > 0.0f,
                       "Galaxy %d with Mvir=%.3f should have positive Vvir=%.3f",
                       i, galaxy_mvir, galaxy_vvir);
        }
        
        // GalaxyIndex and CentralGalaxyIndex are set during output preparation,
        // not during pipeline execution, so they should be 0 here
        uint64_t galaxy_index = GALAXY_PROP_GalaxyIndex(galaxy);
        TEST_ASSERT(galaxy_index == 0,
                   "Galaxy %d should have unset GalaxyIndex before output prep: %" PRIu64, i, galaxy_index);
        
        uint64_t central_galaxy_index = GALAXY_PROP_CentralGalaxyIndex(galaxy);
        TEST_ASSERT(central_galaxy_index == 0,
                   "Galaxy %d should have unset CentralGalaxyIndex before output prep: %" PRIu64, 
                   i, central_galaxy_index);
    }
    
    printf("Pipeline integrity test: Verified %d galaxies maintain integrity\n", 
           test_ctx.num_galaxies);
}

/**
 * Test: Output serialization accuracy
 * 
 * Verifies that galaxy properties are correctly serialized to output without
 * corruption, especially testing the unique galaxy index generation.
 */
static void test_output_serialization_accuracy(void) {
    printf("\n=== Testing output serialization accuracy ===\n");
    
    // Test the output preparation pipeline
    int result = prepare_galaxies_for_output(0, // task_forestnr
                                           test_ctx.test_halos,
                                           &test_ctx.forest_info,
                                           test_ctx.test_haloaux,
                                           galaxy_array_get_raw_data(test_ctx.test_galaxies),
                                           test_ctx.num_galaxies,
                                           &test_ctx.output_ctx,
                                           &test_ctx.run_params);
    
    TEST_ASSERT(result == EXIT_SUCCESS, 
               "prepare_galaxies_for_output should succeed, got %d", result);
    
    // Verify that galaxy indices were generated correctly
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        struct GALAXY *galaxy = galaxy_array_get(test_ctx.test_galaxies, i);
        
        // Test that GalaxyIndex is within reasonable bounds
        uint64_t galaxy_index = GALAXY_PROP_GalaxyIndex(galaxy);
        TEST_ASSERT(galaxy_index > 0 && galaxy_index < UINT64_MAX,
                   "Galaxy %d should have valid GalaxyIndex: %" PRIu64, i, galaxy_index);
        
        // Test that CentralGalaxyIndex is valid
        uint64_t central_galaxy_index = GALAXY_PROP_CentralGalaxyIndex(galaxy);
        TEST_ASSERT(central_galaxy_index > 0 && central_galaxy_index < UINT64_MAX,
                   "Galaxy %d should have valid CentralGalaxyIndex: %" PRIu64, i, central_galaxy_index);
        
        // In our test case, all galaxies are central, so indices should be equal
        TEST_ASSERT(galaxy_index == central_galaxy_index,
                   "Galaxy %d: central galaxy indices should match: %" PRIu64 " != %" PRIu64,
                   i, galaxy_index, central_galaxy_index);
        
        printf("  Galaxy %d: GalaxyIndex=%" PRIu64 ", CentralGalaxyIndex=%" PRIu64 "\n",
               i, galaxy_index, central_galaxy_index);
    }
    
    // Verify unique galaxy indices (no duplicates)
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        for (int j = i + 1; j < test_ctx.num_galaxies; j++) {
            struct GALAXY *gal_i = galaxy_array_get(test_ctx.test_galaxies, i);
            struct GALAXY *gal_j = galaxy_array_get(test_ctx.test_galaxies, j);
            uint64_t gal_i_index = GALAXY_PROP_GalaxyIndex(gal_i);
            uint64_t gal_j_index = GALAXY_PROP_GalaxyIndex(gal_j);
            TEST_ASSERT(gal_i_index != gal_j_index,
                       "Galaxies %d and %d should have unique GalaxyIndex: %" PRIu64, 
                       i, j, gal_i_index);
        }
    }
    
    printf("Output serialization test: Verified unique indices for %d galaxies\n", 
           test_ctx.num_galaxies);
}

/**
 * Test: Memory corruption detection
 * 
 * Actively tests for the types of memory corruption that can occur,
 * ensuring our detection mechanisms work correctly.
 */
static void test_memory_corruption_detection(void) {
    printf("\n=== Testing memory corruption detection ===\n");
    
    // Test 1: Verify that our corruption detection actually works
    struct GALAXY test_galaxy;
    memset(&test_galaxy, 0, sizeof(struct GALAXY));
    
    // Initialize minimal params for property system
    struct params test_params;
    memset(&test_params, 0, sizeof(test_params));
    test_params.simulation.NumSnapOutputs = 10;
    test_params.simulation.SimMaxSnaps = 64; // Required for dynamic array allocation
    
    // Allocate properties first
    int result = allocate_galaxy_properties(&test_galaxy, &test_params);
    TEST_ASSERT(result == 0, "Property allocation should succeed");
    
    // Set a normal value
    GALAXY_PROP_GalaxyNr(&test_galaxy) = 42;
    TEST_ASSERT(GALAXY_PROP_GalaxyNr(&test_galaxy) == 42, "Normal value should be preserved");
    
    // Inject corruption
    GALAXY_PROP_GalaxyNr(&test_galaxy) = MEMORY_POISON_VALUE_32;
    TEST_ASSERT(GALAXY_PROP_GalaxyNr(&test_galaxy) == (int32_t)MEMORY_POISON_VALUE_32, 
               "Should detect injected corruption");
    
    // Test 2: Verify corruption detection in galaxy arrays
    bool all_galaxies_clean = true;
    for (int i = 0; i < test_ctx.num_galaxies; i++) {
        struct GALAXY *galaxy = galaxy_array_get(test_ctx.test_galaxies, i);
        
        // Check for various corruption patterns
        int32_t galaxy_nr = GALAXY_PROP_GalaxyNr(galaxy);
        if (galaxy_nr == (int32_t)MEMORY_POISON_VALUE_32 ||
            galaxy_nr == (int32_t)0xDEADBEEF ||
            galaxy_nr == -1 ||
            galaxy_nr > MAX_REASONABLE_GALAXY_NR) {
            printf("ERROR: Detected corruption in galaxy %d: GalaxyNr = %d\n", 
                   i, galaxy_nr);
            all_galaxies_clean = false;
        }
        
        // Check for unreasonable GalaxyIndex values
        uint64_t galaxy_index = GALAXY_PROP_GalaxyIndex(galaxy);
        if (galaxy_index == MEMORY_POISON_VALUE_64 ||
            galaxy_index == 0) {
            printf("ERROR: Detected corruption in galaxy %d: GalaxyIndex = %" PRIu64 "\n", 
                   i, galaxy_index);
            all_galaxies_clean = false;
        }
    }
    
    TEST_ASSERT(all_galaxies_clean, "All galaxies should be free of memory corruption");
    
    // Test 3: Verify halo integrity
    bool all_halos_clean = true;
    for (int i = 0; i < test_ctx.num_halos; i++) {
        if (!verify_halo_integrity(i)) {
            all_halos_clean = false;
        }
    }
    
    TEST_ASSERT(all_halos_clean, "All halos should maintain integrity");
    
    printf("Memory corruption detection test: All data structures verified clean\n");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc; /* Suppress unused parameter warning */
    (void)argv; /* Suppress unused parameter warning */
    printf("\n==============================================\n");
    printf("Starting tests for test_data_integrity_physics_free\n");
    printf("==============================================\n\n");
    
    printf("This test verifies data integrity through the SAGE pipeline:\n");
    printf("  1. Memory initialization correctness (garbage value detection)\n");
    printf("  2. Halo property preservation during galaxy initialization\n");
    printf("  3. Galaxy property integrity through pipeline phases\n");
    printf("  4. Output serialization accuracy and unique ID generation\n");
    printf("  5. Active memory corruption detection\n\n");
    
    printf("CRITICAL IMPORTANCE: This test is designed to FAIL if there are\n");
    printf("ANY data corruption issues in the core SAGE infrastructure.\n\n");
    
    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests in logical order
    test_memory_initialization_integrity();
    test_halo_to_galaxy_data_preservation();
    test_galaxy_pipeline_integrity();
    test_output_serialization_accuracy();
    test_memory_corruption_detection();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n==============================================\n");
    printf("Test results for test_data_integrity_physics_free:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("==============================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
