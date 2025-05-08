#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/physics/output_preparation_module.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_logging.h"

// Test function prototypes
static void test_output_preparation_init(void);
static void test_output_preparation_execute(void);

int main(void) {
    printf("Testing output preparation module...\n");
    
    // Initialize core systems
    logging_init(LOG_LEVEL_DEBUG, stderr);
    module_system_initialize();
    pipeline_system_initialize();
    
    // Run tests
    test_output_preparation_init();
    test_output_preparation_execute();
    
    // Cleanup
    pipeline_system_cleanup();
    module_system_cleanup();
    
    printf("All output preparation tests passed!\n");
    return 0;
}

static void test_output_preparation_init(void) {
    printf("Testing output preparation module initialization...\n");
    
    int ret = init_output_preparation_module();
    assert(ret == 0);
    
    ret = cleanup_output_preparation_module();
    assert(ret == 0);
    
    printf("Initialization test passed!\n");
}

static void test_output_preparation_execute(void) {
    printf("Testing output preparation execution...\n");
    
    // Initialize module
    int ret = init_output_preparation_module();
    assert(ret == 0);
    
    // Create a test galaxy
    struct GALAXY galaxy;
    memset(&galaxy, 0, sizeof(struct GALAXY));
    
    // Set basic galaxy properties
    galaxy.Type = 0;
    galaxy.GalaxyNr = 1;
    galaxy.HaloNr = 1;
    galaxy.CentralGal = 0;
    
    // Allocate properties
    galaxy.properties = calloc(1, sizeof(galaxy_properties_t));
    assert(galaxy.properties != NULL);
    
    // Reset properties to defaults
    reset_galaxy_properties(&galaxy);
    
    // Set up test properties
    GALAXY_PROP_StellarMass(&galaxy) = 1.0;
    GALAXY_PROP_ColdGas(&galaxy) = 0.5;
    GALAXY_PROP_HotGas(&galaxy) = 2.0;
    GALAXY_PROP_DiskScaleRadius(&galaxy) = 1000.0;
    
    for (int i = 0; i < STEPS; i++) {
        GALAXY_PROP_SfrDisk_ELEM(&galaxy, i) = 0.01;
    }
    
    // Create an array for StarFormationHistory
    ret = galaxy_set_StarFormationHistory_size(&galaxy, 10);
    assert(ret == 0);
    
    for (int i = 0; i < 10; i++) {
        GALAXY_PROP_StarFormationHistory_ELEM(&galaxy, i) = 0.02 * i;
    }
    
    // Create pipeline context
    struct pipeline_context ctx;
    memset(&ctx, 0, sizeof(struct pipeline_context));
    ctx.galaxies = &galaxy;
    ctx.ngal = 1;
    ctx.centralgal = 0;
    ctx.current_galaxy = 0;
    ctx.execution_phase = PIPELINE_PHASE_FINAL;
    
    // Store initial values for comparison
    float initial_disk_scale_radius = GALAXY_PROP_DiskScaleRadius(&galaxy);
    
    // Execute output preparation
    ret = output_preparation_execute(NULL, &ctx);
    assert(ret == 0);
    
    // Verify results
    float expected_log_radius = log10(initial_disk_scale_radius);
    float actual_radius = GALAXY_PROP_DiskScaleRadius(&galaxy);
    
    // Check that disk scale radius was converted to log scale
    assert(fabs(actual_radius - expected_log_radius) < 1e-6);
    
    // Check that SFH values are all non-negative
    for (int i = 0; i < GALAXY_PROP_StarFormationHistory_SIZE(&galaxy); i++) {
        assert(GALAXY_PROP_StarFormationHistory_ELEM(&galaxy, i) >= 0.0);
    }
    
    // Clean up
    free_galaxy_properties(&galaxy);
    ret = cleanup_output_preparation_module();
    assert(ret == 0);
    
    printf("Execution test passed!\n");
}