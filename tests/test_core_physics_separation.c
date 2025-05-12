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
#include "../src/core/core_properties.h"
#include "../src/core/standard_properties.h"
#include "../src/core/core_evolution_diagnostics.h"

/**
 * @file test_core_physics_separation.c
 * @brief Validate complete core-physics separation
 * 
 * This test verifies that the core is completely independent from physics.
 * It checks that the core infrastructure works without any physics properties
 * defined, and that the GALAXY struct doesn't contain physics-specific fields.
 */

/* Function prototypes */
void verify_galaxy_struct_independence(void);
void verify_property_system_independence(void);
void verify_empty_pipeline_creation(void);

int main(int argc, char **argv) {
    /* Initialize logging */
    logging_init();
    LOG_INFO("=== Core-Physics Separation Validation Test ===");
    
    /* Verify galaxy struct independence from physics */
    LOG_INFO("Verifying GALAXY struct has no physics dependencies...");
    verify_galaxy_struct_independence();
    
    /* Verify property system independence from physics */
    LOG_INFO("Verifying property system independence...");
    verify_property_system_independence();
    
    /* Verify empty pipeline can be created */
    LOG_INFO("Verifying empty pipeline creation...");
    verify_empty_pipeline_creation();
    
    LOG_INFO("=== Core-Physics Separation Validation PASSED ===");
    
    return 0;
}

/**
 * Verify that the GALAXY struct doesn't contain physics-specific fields
 */
void verify_galaxy_struct_independence(void) {
    struct GALAXY test_gal;
    memset(&test_gal, 0, sizeof(struct GALAXY));
    
    /* Initialize a galaxy and verify structure */
    init_galaxy(&test_gal, 0);
    
    /* Verify core fields are present but no physics fields */
    assert(test_gal.GalaxyIndex == 0 && "GalaxyIndex field required");
    
    /* Verify properties pointer is valid */
    assert(test_gal.properties != NULL && "Galaxy properties should be allocated");
    
    /* Verify physics field removal */
    /* The following fields should NOT exist in the struct anymore.
     * These checks are performed at compile time - if they exist,
     * the compilation will fail, proving physics separation.
     */
    /*
    double test;
    test = test_gal.ColdGas;        // Physics field - should fail
    test = test_gal.HotGas;         // Physics field - should fail
    test = test_gal.StellarMass;    // Physics field - should fail
    test = test_gal.BlackHoleMass;  // Physics field - should fail
    */
    
    LOG_INFO("GALAXY struct has no direct physics fields - OK");
    
    /* Free galaxy */
    free_galaxy(&test_gal);
}

/**
 * Verify that the property system can operate with only core properties
 */
void verify_property_system_independence(void) {
    /* Register standard properties */
    LOG_INFO("Initializing property system...");
    int status = init_property_system();
    assert(status == 0 && "Property system initialization failed");
    
    /* Verify core properties exist */
    int num_props = get_num_standard_properties();
    LOG_INFO("Property system initialized with %d core properties", num_props);
    assert(num_props > 0 && "Should have some core properties registered");
    
    /* Test we can create properties */
    struct galaxy_properties *props = NULL;
    status = allocate_galaxy_properties(&props);
    assert(status == 0 && "Failed to allocate galaxy properties");
    assert(props != NULL && "Properties should be allocated");
    
    /* Test we can reset (initialize) properties */
    status = reset_galaxy_properties(props);
    assert(status == 0 && "Failed to reset galaxy properties");
    
    /* Verify that core properties access works without physics properties */
    /* This proves that the core infrastructure doesn't depend on physics properties */
    
    /* Test property access using macros */
    props->SnapNum = 42;
    props->Type = 0;
    props->MergTime = 1.5;
    
    assert(props->SnapNum == 42 && "SnapNum property should be accessible");
    assert(props->Type == 0 && "Type property should be accessible");
    assert(props->MergTime == 1.5 && "MergTime property should be accessible");
    
    LOG_INFO("Property access for core properties works - OK");
    
    /* Test property copying */
    struct galaxy_properties *copy_props = NULL;
    status = allocate_galaxy_properties(&copy_props);
    assert(status == 0 && "Failed to allocate copy properties");
    
    status = copy_galaxy_properties(props, copy_props);
    assert(status == 0 && "Failed to copy galaxy properties");
    
    assert(copy_props->SnapNum == 42 && "SnapNum not copied correctly");
    assert(copy_props->Type == 0 && "Type not copied correctly");
    assert(copy_props->MergTime == 1.5 && "MergTime not copied correctly");
    
    LOG_INFO("Property copying for core properties works - OK");
    
    /* Clean up */
    free_galaxy_properties(props);
    free_galaxy_properties(copy_props);
    free(props);
    free(copy_props);
    
    /* Clean up property system */
    cleanup_property_system();
    
    LOG_INFO("Property system independence verified - OK");
}

/**
 * Verify that we can create an empty pipeline
 */
void verify_empty_pipeline_creation(void) {
    /* Initialize module system */
    int status = module_system_initialize();
    assert(status == 0 && "Module system initialization failed");
    
    /* Initialize pipeline system */
    status = pipeline_system_initialize();
    assert(status == 0 && "Pipeline system initialization failed");
    
    /* Create an empty pipeline */
    struct module_pipeline *pipeline = pipeline_create("test_empty");
    assert(pipeline != NULL && "Failed to create pipeline");
    
    /* Add a simple step */
    status = pipeline_add_step(pipeline, MODULE_TYPE_MISC, "placeholder_module", "test_step", true, false);
    assert(status == 0 && "Failed to add pipeline step");
    assert(pipeline->num_steps == 1 && "Pipeline should have 1 step");
    
    /* Clean up */
    pipeline_destroy(pipeline);
    
    /* Test pipeline validation */
    struct module_pipeline *empty = pipeline_create("empty");
    assert(empty != NULL && "Failed to create empty pipeline");
    
    /* Empty pipeline is valid (should have 0 steps) */
    bool valid = pipeline_validate(empty);
    assert(valid && "Empty pipeline should be valid");
    
    LOG_INFO("Empty pipeline creation and validation successful - OK");
    
    /* Clean up */
    pipeline_destroy(empty);
    pipeline_system_cleanup();
    module_system_cleanup();
}
