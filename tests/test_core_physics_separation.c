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
#include "../src/physics/placeholder_empty_module.h"

/* Test helper functions */
static void setup_minimal_galaxy_data(struct GALAXY *galaxy);
static void setup_minimal_pipeline(void);
static int test_pipeline_execution(void);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    /* Initialize logging */
    set_logging_level(LOG_LEVEL_INFO);
    LOG_INFO("Testing core physics separation...");
    
    /* Initialize core systems */
    if (initialize_module_system() != 0) {
        LOG_ERROR("Failed to initialize module system");
        return EXIT_FAILURE;
    }
    
    if (initialize_event_system() != 0) {
        LOG_ERROR("Failed to initialize event system");
        module_system_cleanup();
        return EXIT_FAILURE;
    }
    
    /* Set up minimal pipeline with placeholder module */
    setup_minimal_pipeline();
    
    /* Run pipeline execution test */
    int result = test_pipeline_execution();
    
    /* Cleanup core systems */
    event_system_cleanup();
    module_system_cleanup();
    
    if (result == 0) {
        LOG_INFO("Core physics separation test PASSED");
        return EXIT_SUCCESS;
    } else {
        LOG_ERROR("Core physics separation test FAILED");
        return EXIT_FAILURE;
    }
}

/**
 * Set up a minimal pipeline with placeholder module
 */
static void setup_minimal_pipeline(void) {
    /* Create global pipeline */
    struct module_pipeline *pipeline = pipeline_create("test_pipeline");
    pipeline_set_global(pipeline);
    
    /* Get placeholder module handle (registered via REGISTER_MODULE in placeholder_empty_module.c) */
    int module_idx = module_get_index_by_name("placeholder_module");
    if (module_idx < 0) {
        LOG_ERROR("Failed to find placeholder module");
        return;
    }
    
    /* Add steps to pipeline */
    pipeline_add_step(pipeline, MODULE_TYPE_MISC, "placeholder_module", "placeholder", true, true);
    
    LOG_INFO("Set up minimal pipeline with %d steps", pipeline->num_steps);
}

/**
 * Initialize a galaxy with minimal fields required by core (no physics fields)
 */
static void setup_minimal_galaxy_data(struct GALAXY *galaxy) {
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
    
    /* Initialize property system */
    galaxy->properties = NULL;
}

/**
 * Test running the pipeline with minimal physics dependencies
 */
static int test_pipeline_execution(void) {
    /* Create simulation parameters */
    struct params params;
    memset(&params, 0, sizeof(struct params));
    
    /* Create a test galaxy */
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(struct GALAXY));
    setup_minimal_galaxy_data(&galaxy);
    
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
    
    /* Get global pipeline */
    struct module_pipeline *pipeline = pipeline_get_global();
    if (pipeline == NULL) {
        LOG_ERROR("Global pipeline not found");
        return -1;
    }
    
    /* Test HALO phase */
    LOG_INFO("Testing HALO phase...");
    ctx.execution_phase = PIPELINE_PHASE_HALO;
    int status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_HALO);
    if (status != 0) {
        LOG_ERROR("HALO phase execution failed: %d", status);
        return -1;
    }
    
    /* Test GALAXY phase */
    LOG_INFO("Testing GALAXY phase...");
    ctx.execution_phase = PIPELINE_PHASE_GALAXY;
    ctx.current_galaxy = 0;
    status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_GALAXY);
    if (status != 0) {
        LOG_ERROR("GALAXY phase execution failed: %d", status);
        return -1;
    }
    
    /* Test POST phase */
    LOG_INFO("Testing POST phase...");
    ctx.execution_phase = PIPELINE_PHASE_POST;
    status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_POST);
    if (status != 0) {
        LOG_ERROR("POST phase execution failed: %d", status);
        return -1;
    }
    
    /* Test FINAL phase */
    LOG_INFO("Testing FINAL phase...");
    ctx.execution_phase = PIPELINE_PHASE_FINAL;
    status = pipeline_execute_phase(pipeline, &ctx, PIPELINE_PHASE_FINAL);
    if (status != 0) {
        LOG_ERROR("FINAL phase execution failed: %d", status);
        return -1;
    }
    
    LOG_INFO("All pipeline phases executed successfully");
    return 0;
}
