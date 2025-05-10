/**
 * @file test_core_physics_separation.c
 * @brief Test core infrastructure independence from physics
 *
 * This test validates that the core infrastructure can run with minimal
 * physics modules and no synchronization between direct fields and properties.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/core/core_event_system.h"
#include "../src/core/physics_pipeline_executor.h"
#include "../src/core/core_properties.h"

/* Test helper functions */
static void setup_minimal_galaxy_data(struct GALAXY *galaxy, struct params *params);
static void setup_minimal_pipeline(void);
static int test_pipeline_execution(void);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    /* Force direct console output for all log messages */
    logging_init(LOG_LEVEL_INFO, stderr);
    fprintf(stderr, "=== Core Physics Separation Test ===\n");
    
    /* The module system may already be initialized by shared library constructor
     * Let's just move forward assuming it's initialized */
    LOG_INFO("Using module system");
    
    /* Let's initialize the event system if not already initialized */
    if (!event_system_is_initialized()) {
        int status = event_system_initialize();
        if (status != 0) {
            LOG_ERROR("Failed to initialize event system");
            return EXIT_FAILURE;
        }
    }
    
    /* Set up minimal pipeline with placeholder module */
    setup_minimal_pipeline();
    
    /* Run pipeline execution test */
    fprintf(stderr, "* Running pipeline execution test\n");
    int result = test_pipeline_execution();
    
    /* Cleanup core systems */
    fprintf(stderr, "* Cleaning up systems\n");
    event_system_cleanup();
    module_system_cleanup();
    
    if (result == 0) {
        LOG_INFO("Core physics separation test PASSED");
        fprintf(stderr, "=== Test completed successfully ===\n");
        return EXIT_SUCCESS;
    } else {
        LOG_ERROR("Core physics separation test FAILED");
        fprintf(stderr, "=== Test FAILED ===\n");
        return EXIT_FAILURE;
    }
}

/**
 * Set up a minimal pipeline with placeholder module
 */
static void setup_minimal_pipeline(void) {
    /* Create global pipeline */
    struct module_pipeline *pipeline = pipeline_create("test_pipeline");
    fprintf(stderr, "  - Created pipeline 'test_pipeline'\n");
    
    pipeline_set_global(pipeline);
    fprintf(stderr, "  - Set as global pipeline\n");
    
    /* Get placeholder module handle (registered via REGISTER_MODULE in placeholder_empty_module.c) */
    int module_idx = module_find_by_name("placeholder_module");
    if (module_idx < 0) {
        fprintf(stderr, "ERROR: Failed to find placeholder module\n");
        LOG_ERROR("Failed to find placeholder module");
        return;
    }
    fprintf(stderr, "  - Found placeholder module at index %d\n", module_idx);
    
    /* Add steps to pipeline */
    int status = pipeline_add_step(pipeline, MODULE_TYPE_MISC, "placeholder_module", "placeholder", true, true);
    if (status != 0) {
        fprintf(stderr, "ERROR: Failed to add step to pipeline: %d\n", status);
    } else {
        fprintf(stderr, "  - Added 'placeholder' step to pipeline\n");
    }
    
    LOG_INFO("Set up minimal pipeline with %d steps", pipeline->num_steps);
    fprintf(stderr, "  - Minimal pipeline has %d steps\n", pipeline->num_steps);
}

/**
 * Initialize a galaxy with minimal fields and properly allocated properties
 */
static void setup_minimal_galaxy_data(struct GALAXY *galaxy, struct params *params) {
    /* Initialize basic fields */
    galaxy->SnapNum = 1;
    galaxy->Type = 0;
    galaxy->GalaxyNr = 1;
    galaxy->CentralGal = 0;
    galaxy->HaloNr = 1;
    galaxy->MostBoundID = 1234;
    galaxy->GalaxyIndex = 10000;
    galaxy->CentralGalaxyIndex = 10000;
    
    galaxy->mergeType = 0;
    galaxy->mergeIntoID = -1;
    galaxy->mergeIntoSnapNum = -1;
    galaxy->dT = 0.1;
    
    galaxy->Pos[0] = galaxy->Pos[1] = galaxy->Pos[2] = 0.0;
    galaxy->Vel[0] = galaxy->Vel[1] = galaxy->Vel[2] = 0.0;
    galaxy->Len = 1000;
    galaxy->Mvir = 1e12;
    galaxy->deltaMvir = 0.0;
    galaxy->CentralMvir = 1e12;
    galaxy->Rvir = 200.0;
    galaxy->Vvir = 200.0;
    galaxy->Vmax = 220.0;
    
    galaxy->MergTime = 999.9;
    galaxy->infallMvir = 0.0;
    galaxy->infallVvir = 0.0;
    galaxy->infallVmax = 0.0;
    
    /* Initialize extension mechanism */
    galaxy->extension_data = NULL;
    galaxy->num_extensions = 0;
    galaxy->extension_flags = 0;
    
    /* Initialize property system properly */
    galaxy->properties = NULL;
    if (allocate_galaxy_properties(galaxy, params) != 0) {
        LOG_ERROR("Failed to allocate galaxy properties");
        return;
    }
    
    /* Initialize core properties with proper values */
    GALAXY_PROP_SnapNum(galaxy) = 1;
    GALAXY_PROP_Type(galaxy) = 0;
    GALAXY_PROP_GalaxyNr(galaxy) = 1;
    GALAXY_PROP_CentralGal(galaxy) = 0;
    GALAXY_PROP_HaloNr(galaxy) = 1;
    GALAXY_PROP_MostBoundID(galaxy) = 1234;
    GALAXY_PROP_GalaxyIndex(galaxy) = 10000;
    GALAXY_PROP_CentralGalaxyIndex(galaxy) = 10000;
    
    GALAXY_PROP_mergeType(galaxy) = 0;
    GALAXY_PROP_mergeIntoID(galaxy) = -1;
    GALAXY_PROP_mergeIntoSnapNum(galaxy) = -1;
    GALAXY_PROP_dT(galaxy) = 0.1;
    
    for (int i = 0; i < 3; i++) {
        GALAXY_PROP_Pos_ELEM(galaxy, i) = 0.0;
        GALAXY_PROP_Vel_ELEM(galaxy, i) = 0.0;
    }
    
    GALAXY_PROP_Len(galaxy) = 1000;
    GALAXY_PROP_Mvir(galaxy) = 1e12;
    GALAXY_PROP_deltaMvir(galaxy) = 0.0;
    GALAXY_PROP_CentralMvir(galaxy) = 1e12;
    GALAXY_PROP_Rvir(galaxy) = 200.0;
    GALAXY_PROP_Vvir(galaxy) = 200.0;
    GALAXY_PROP_Vmax(galaxy) = 220.0;
    
    GALAXY_PROP_MergTime(galaxy) = 999.9;
    GALAXY_PROP_infallMvir(galaxy) = 0.0;
    GALAXY_PROP_infallVvir(galaxy) = 0.0;
    GALAXY_PROP_infallVmax(galaxy) = 0.0;
}

/**
 * Test running the pipeline with minimal physics dependencies
 */
static int test_pipeline_execution(void) {
    /* Create simulation parameters */
    struct params params;
    memset(&params, 0, sizeof(struct params));
    
    /* Set up minimal parameters needed for property allocation */
    params.simulation.nsnapshots = 10;
    params.simulation.SimMaxSnaps = 60;
    
    fprintf(stderr, "  - Initialized parameters (nsnapshots=%d, SimMaxSnaps=%d)\n", 
            params.simulation.nsnapshots, params.simulation.SimMaxSnaps);
    
    /* Initialize times/redshifts arrays */
    for (int i = 0; i < params.simulation.SimMaxSnaps; i++) {
        params.simulation.ZZ[i] = 10.0 / (i + 1) - 1.0;  /* Simple redshift calculation */
        params.simulation.AA[i] = 1.0 / (params.simulation.ZZ[i] + 1.0); /* Scale factor */
    }
    fprintf(stderr, "  - Initialized redshift and scale factor arrays\n");
    
    /* Create a test galaxy */
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(struct GALAXY));
    setup_minimal_galaxy_data(&galaxy, &params);
    fprintf(stderr, "  - Created test galaxy with properties\n");
    
    /* Create pipeline context */
    struct pipeline_context ctx;
    pipeline_context_init(
        &ctx,
        &params,
        &galaxy,
        1,
        0,
        13.8, /* time */
        0.1,  /* dt */
        1,    /* halo_nr */
        0,    /* step */
        NULL  /* user_data */
    );
    fprintf(stderr, "  - Created pipeline context\n");
    
    /* Get global pipeline */
    struct module_pipeline *pipeline = pipeline_get_global();
    if (pipeline == NULL) {
        fprintf(stderr, "ERROR: Global pipeline not found\n");
        LOG_ERROR("Global pipeline not found");
        return -1;
    }
    fprintf(stderr, "  - Retrieved global pipeline with %d steps\n", pipeline->num_steps);
    
    /* Test HALO phase */
    fprintf(stderr, "  - Testing HALO phase...\n");
    ctx.execution_phase = PIPELINE_PHASE_HALO;
    int status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_HALO);
    if (status != 0) {
        fprintf(stderr, "ERROR: HALO phase execution failed: %d\n", status);
        LOG_ERROR("HALO phase execution failed: %d", status);
        free_galaxy_properties(&galaxy);
        return -1;
    }
    fprintf(stderr, "    HALO phase completed successfully\n");
    
    /* Test GALAXY phase */
    fprintf(stderr, "  - Testing GALAXY phase...\n");
    ctx.execution_phase = PIPELINE_PHASE_GALAXY;
    ctx.current_galaxy = 0;
    status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_GALAXY);
    if (status != 0) {
        fprintf(stderr, "ERROR: GALAXY phase execution failed: %d\n", status);
        LOG_ERROR("GALAXY phase execution failed: %d", status);
        free_galaxy_properties(&galaxy);
        return -1;
    }
    fprintf(stderr, "    GALAXY phase completed successfully\n");
    
    /* Test POST phase */
    fprintf(stderr, "  - Testing POST phase...\n");
    ctx.execution_phase = PIPELINE_PHASE_POST;
    status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_POST);
    if (status != 0) {
        fprintf(stderr, "ERROR: POST phase execution failed: %d\n", status);
        LOG_ERROR("POST phase execution failed: %d", status);
        free_galaxy_properties(&galaxy);
        return -1;
    }
    fprintf(stderr, "    POST phase completed successfully\n");
    
    /* Test FINAL phase */
    fprintf(stderr, "  - Testing FINAL phase...\n");
    ctx.execution_phase = PIPELINE_PHASE_FINAL;
    status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_FINAL);
    if (status != 0) {
        fprintf(stderr, "ERROR: FINAL phase execution failed: %d\n", status);
        LOG_ERROR("FINAL phase execution failed: %d", status);
        free_galaxy_properties(&galaxy);
        return -1;
    }
    fprintf(stderr, "    FINAL phase completed successfully\n");
    
    fprintf(stderr, "  - All pipeline phases executed successfully\n");
    
    /* Clean up property resources */
    free_galaxy_properties(&galaxy);
    fprintf(stderr, "  - Cleaned up galaxy properties\n");
    
    return 0;
}
