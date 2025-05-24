#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_build_model.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_evolution_diagnostics.h"
#include "../src/core/physics_pipeline_executor.h"
#include "../src/core/core_read_parameter_file.h"

/**
 * @file test_empty_pipeline.c
 * @brief Test the physics-agnostic core with empty pipeline
 * 
 * This test validates that the SAGE core infrastructure can run 
 * with no physics components at all, using just placeholder modules
 * in a completely empty pipeline. It executes all pipeline phases
 * with no physics operations to validate the core-physics separation.
 */

/* Function prototypes */
void verify_module_loading(void);
void verify_pipeline_execution(struct params *run_params);
void verify_basic_galaxy_properties(struct GALAXY *galaxies, int ngal);

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <parameter_file>\n", argv[0]);
        return 1;
    }

    char *parameter_file = argv[1];
    
    /* Initialize logging */
    logging_init(LOG_LEVEL_INFO, stdout);
    LOG_INFO("=== Empty Pipeline Validation Test ===");
    LOG_INFO("Using parameter file: %s", parameter_file);
    
    /* Load parameters */
    LOG_INFO("Loading parameters...");
    struct params run_params;
    if (read_parameter_file(parameter_file, &run_params) != 0) {
        LOG_ERROR("Failed to read parameter file");
        return 1;
    }
    
    /* Initialize core components */
    LOG_INFO("Initializing core infrastructure...");
    init(&run_params);
    
    /* Verify module loading */
    LOG_INFO("Verifying module loading...");
    verify_module_loading();
    
    /* Verify pipeline execution */
    LOG_INFO("Verifying pipeline execution...");
    verify_pipeline_execution(&run_params);
    
    LOG_INFO("=== Empty Pipeline Validation Test PASSED ===");
    cleanup_logging();
    
    return 0;
}
/**
 * Verify that all required modules are loaded
 */
void verify_module_loading(void) {
    /* Get the global module registry */
    struct module_registry *registry = global_module_registry;
    assert(registry != NULL && "Module registry should be initialized");
    
    LOG_INFO("Module registry has %d modules loaded", registry->num_modules);
    assert(registry->num_modules > 0 && "At least one module should be loaded");
    
    /* Check for placeholder module */
    bool found_placeholder = false;
    for (int i = 0; i < registry->num_modules; i++) {
        struct base_module *module = registry->modules[i].module;
        if (module != NULL && strncmp(module->name, "placeholder", 11) == 0) {
            found_placeholder = true;
            LOG_INFO("Found placeholder module: %s - OK", module->name);
            break;
        }
    }
    
    assert(found_placeholder && "A placeholder module should be registered");
    
    /* Verify pipeline is configured */
    struct module_pipeline *pipeline = pipeline_get_global();
    assert(pipeline != NULL && "Global pipeline should be initialized");
    assert(pipeline->num_steps > 0 && "Pipeline should have at least one step");
    
    LOG_INFO("Pipeline has %d steps - OK", pipeline->num_steps);
}

/**
 * Verify that the pipeline can be executed with all phases
 */
void verify_pipeline_execution(struct params *run_params) {
    struct module_pipeline *pipeline = pipeline_get_global();
    assert(pipeline != NULL && "Global pipeline should be initialized");
    
    /* Create a pipeline context with minimal data */
    struct pipeline_context context;
    memset(&context, 0, sizeof(struct pipeline_context));
    
    /* Create a small set of test galaxies */
    int ngal = 5;
    LOG_INFO("Creating %d test galaxies", ngal);
    struct GALAXY *galaxies = (struct GALAXY *)calloc(ngal, sizeof(struct GALAXY));
    assert(galaxies != NULL && "Failed to allocate test galaxies");
    
    /* Initialize galaxies with basic data */
    for (int i = 0; i < ngal; i++) {
        galaxies[i].SnapNum = 0;
        galaxies[i].Type = (i == 0) ? 0 : 1; /* First galaxy is central */
        galaxies[i].GalaxyIndex = i;
        
        /* Allocate properties */
        galaxies[i].properties = calloc(1, sizeof(galaxy_properties_t));
        assert(galaxies[i].properties != NULL && "Failed to allocate galaxy properties");
    }
    /* Initialize context with parameters */
    context.params = run_params;
    context.galaxies = galaxies;
    context.ngal = ngal;
    context.redshift = 0.0;
    
    /* Execute all phases */
    LOG_INFO("Executing HALO phase...");
    int status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_HALO);
    assert(status == 0 && "HALO phase execution failed");
    
    LOG_INFO("Executing GALAXY phase for each galaxy...");
    for (int i = 0; i < ngal; i++) {
        context.current_galaxy = i;
        status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_GALAXY);
        assert(status == 0 && "GALAXY phase execution failed");
    }
    
    LOG_INFO("Executing POST phase...");
    status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_POST);
    assert(status == 0 && "POST phase execution failed");
    
    LOG_INFO("Executing FINAL phase...");
    status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_FINAL);
    assert(status == 0 && "FINAL phase execution failed");
    
    /* Verify galaxies still have basic properties */
    verify_basic_galaxy_properties(galaxies, ngal);
    
    /* Clean up */
    for (int i = 0; i < ngal; i++) {
        if (galaxies[i].properties != NULL) {
            free(galaxies[i].properties);
            galaxies[i].properties = NULL;
        }
    }
    free(galaxies);
    
    LOG_INFO("All pipeline phases executed successfully");
}

/**
 * Verify that basic galaxy properties are intact
 */
void verify_basic_galaxy_properties(struct GALAXY *galaxies, int ngal) {
    for (int i = 0; i < ngal; i++) {
        struct GALAXY *gal = &galaxies[i];
        
        /* Check core identifiers */
        assert(gal->GalaxyIndex == (uint64_t)i && "GalaxyIndex should be preserved");
        assert((gal->Type == 0 || gal->Type == 1) && "Type should be valid");
        
        /* Check properties structure */
        assert(gal->properties != NULL && "Galaxy properties should be allocated");
        
        LOG_INFO("Galaxy %d properties verified - OK", i);
    }
}