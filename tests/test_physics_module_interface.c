#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/core_allvars.h"
#include "../src/core/physics_module_interface.h"
#include "../src/core/physics_module_registry.h"
#include "../src/core/physics_pipeline.h"

// Mock test module implementation
static physics_module_result_t test_module_initialize(const struct params *run_params)
{
    (void)run_params; // Mark parameter as intentionally unused
    printf("Test module initialized\n");
    return PHYSICS_MODULE_SUCCESS;
}

static void test_module_shutdown(void)
{
    printf("Test module shutdown\n");
}

static physics_module_result_t test_module_execute_halo(physics_execution_context_t *ctx)
{
    printf("Test module halo phase executed for halo %d\n", ctx->current_halo);
    ctx->halo_infall_gas = 1.0; // Mock infall calculation
    return PHYSICS_MODULE_SUCCESS;
}

static physics_module_result_t test_module_execute_galaxy(physics_execution_context_t *ctx)
{
    printf("Test module galaxy phase executed for galaxy %d\n", ctx->current_galaxy);
    ctx->galaxy_cooling_gas = 0.5; // Mock cooling calculation
    return PHYSICS_MODULE_SUCCESS;
}

static bool test_module_provides_infall(void)
{
    return true;
}

static bool test_module_provides_cooling(void)
{
    return true;
}

static bool test_module_provides_starformation(void)
{
    return false;
}

static bool test_module_provides_feedback(void)
{
    return false;
}

static bool test_module_provides_reincorporation(void)
{
    return false;
}

static bool test_module_provides_mergers(void)
{
    return false;
}

// Second test module functions (depends on test_module)
static physics_module_result_t test_module2_initialize(const struct params *run_params)
{
    (void)run_params; // Mark parameter as intentionally unused
    printf("Test module 2 initialized\n");
    return PHYSICS_MODULE_SUCCESS;
}

static void test_module2_shutdown(void)
{
    printf("Test module 2 shutdown\n");
}

static physics_module_result_t test_module2_execute_galaxy(physics_execution_context_t *ctx)
{
    printf("Test module 2 galaxy phase executed for galaxy %d\n", ctx->current_galaxy);
    // This module uses results from cooling (provided by test_module)
    ctx->galaxy_stellar_mass = ctx->galaxy_cooling_gas * 0.1; // Mock star formation
    return PHYSICS_MODULE_SUCCESS;
}

static bool test_module2_provides_infall(void) { return false; }
static bool test_module2_provides_cooling(void) { return false; }
static bool test_module2_provides_starformation(void) { return true; }
static bool test_module2_provides_feedback(void) { return true; }
static bool test_module2_provides_reincorporation(void) { return false; }
static bool test_module2_provides_mergers(void) { return false; }

// Dependencies array for test_module2
static const char* test_module2_deps[] = {"test_module", NULL};

// Test module instances
static physics_module_t test_module = {
    .name = "test_module",
    .version = "1.0.0",
    .description = "Test physics module for interface validation",
    .dependencies = NULL,
    .supported_phases = PHYSICS_PHASE_HALO | PHYSICS_PHASE_GALAXY,
    
    .initialize = test_module_initialize,
    .shutdown = test_module_shutdown,
    .execute_halo_phase = test_module_execute_halo,
    .execute_galaxy_phase = test_module_execute_galaxy,
    .execute_post_phase = NULL,
    .execute_final_phase = NULL,
    
    .provides_infall = test_module_provides_infall,
    .provides_cooling = test_module_provides_cooling,
    .provides_starformation = test_module_provides_starformation,
    .provides_feedback = test_module_provides_feedback,
    .provides_reincorporation = test_module_provides_reincorporation,
    .provides_mergers = test_module_provides_mergers,
    
    .module_data = NULL
};

static physics_module_t test_module2 = {
    .name = "test_module2",
    .version = "1.0.0",
    .description = "Second test module with dependencies",
    .dependencies = test_module2_deps,
    .supported_phases = PHYSICS_PHASE_GALAXY,
    
    .initialize = test_module2_initialize,
    .shutdown = test_module2_shutdown,
    .execute_halo_phase = NULL,
    .execute_galaxy_phase = test_module2_execute_galaxy,
    .execute_post_phase = NULL,
    .execute_final_phase = NULL,
    
    .provides_infall = test_module2_provides_infall,
    .provides_cooling = test_module2_provides_cooling,
    .provides_starformation = test_module2_provides_starformation,
    .provides_feedback = test_module2_provides_feedback,
    .provides_reincorporation = test_module2_provides_reincorporation,
    .provides_mergers = test_module2_provides_mergers,
    
    .module_data = NULL
};

// Helper functions for capability checking
static bool check_infall_capability(const physics_module_t *m) { 
    return m->provides_infall(); 
}

static bool check_merger_capability(const physics_module_t *m) { 
    return m->provides_mergers(); 
}

// Test functions
static void test_module_registration(void)
{
    printf("Testing module registration...\n");
    
    // Initialize registry
    physics_module_result_t result = physics_module_registry_initialize();
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Register both test modules
    result = physics_module_register(&test_module);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    result = physics_module_register(&test_module2);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Find modules by name
    physics_module_t *found1 = physics_module_find_by_name("test_module");
    assert(found1 != NULL);
    assert(strcmp(found1->name, "test_module") == 0);
    
    physics_module_t *found2 = physics_module_find_by_name("test_module2");
    assert(found2 != NULL);
    assert(strcmp(found2->name, "test_module2") == 0);
    
    // Check module count
    int count = physics_module_get_count();
    assert(count == 2);
    
    printf("✓ Module registration test passed\n");
}

static void test_dependency_resolution(void)
{
    printf("Testing dependency resolution...\n");
    
    // Request both modules and test dependency resolution
    const char *requested_modules[] = {"test_module2", "test_module"};  // Note: reverse order
    physics_module_t *ordered_modules[10];
    
    int result_count = physics_module_registry_resolve_dependencies(requested_modules, 2, 
                                                                   ordered_modules, 10);
    assert(result_count == 2);
    
    // Verify dependency order: test_module should come before test_module2
    assert(strcmp(ordered_modules[0]->name, "test_module") == 0);
    assert(strcmp(ordered_modules[1]->name, "test_module2") == 0);
    
    // Verify test_module2 depends on test_module
    assert(ordered_modules[1]->dependencies != NULL);
    assert(strcmp(ordered_modules[1]->dependencies[0], "test_module") == 0);
    
    // Test requesting only the dependent module (should include dependency)
    const char *dependent_only[] = {"test_module2"};
    result_count = physics_module_registry_resolve_dependencies(dependent_only, 1,
                                                              ordered_modules, 10);
    assert(result_count == 2);  // Should include test_module as dependency
    assert(strcmp(ordered_modules[0]->name, "test_module") == 0);
    assert(strcmp(ordered_modules[1]->name, "test_module2") == 0);
    
    printf("✓ Dependency resolution test passed\n");
}

static void test_module_validation(void)
{
    printf("Testing module validation...\n");
    
    // Test valid module
    bool valid = physics_module_validate(&test_module);
    assert(valid == true);
    
    // Test invalid module (NULL name)
    physics_module_t invalid_module = test_module;
    invalid_module.name = NULL;
    valid = physics_module_validate(&invalid_module);
    assert(valid == false);
    
    // Test invalid module (no phases)
    invalid_module = test_module;
    invalid_module.name = "invalid";
    invalid_module.supported_phases = 0;
    valid = physics_module_validate(&invalid_module);
    assert(valid == false);
    
    // Test invalid module (missing function for supported phase)
    invalid_module = test_module;
    invalid_module.name = "invalid";
    invalid_module.supported_phases = PHYSICS_PHASE_HALO;
    invalid_module.execute_halo_phase = NULL;
    valid = physics_module_validate(&invalid_module);
    assert(valid == false);
    
    printf("✓ Module validation test passed\n");
}

static void test_pipeline_creation(void)
{
    printf("Testing pipeline creation and configuration...\n");
    
    // Create pipeline
    physics_pipeline_t *pipeline = physics_pipeline_create();
    assert(pipeline != NULL);
    
    // Add module to pipeline
    physics_module_result_t result = physics_pipeline_add_module(pipeline, &test_module);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Validate pipeline
    result = physics_pipeline_validate(pipeline);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Test capability checking with proper C function pointers
    bool has_infall = physics_pipeline_has_capability(pipeline, check_infall_capability);
    assert(has_infall == true);
    
    bool has_mergers = physics_pipeline_has_capability(pipeline, check_merger_capability);
    assert(has_mergers == false);
    
    // Clean up
    physics_pipeline_destroy(pipeline);
    
    printf("✓ Pipeline creation test passed\n");
}

static void test_pipeline_execution(void)
{
    printf("Testing pipeline execution...\n");
    
    // Create and configure pipeline
    physics_pipeline_t *pipeline = physics_pipeline_create();
    assert(pipeline != NULL);
    
    physics_module_result_t result = physics_pipeline_add_module(pipeline, &test_module);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Mock data structures for context initialization
    struct halo_data mock_halos[1];
    struct halo_aux_data mock_haloaux[1];
    struct GALAXY mock_galaxies[1];
    struct params mock_params;
    
    memset(&mock_halos, 0, sizeof(mock_halos));
    memset(&mock_haloaux, 0, sizeof(mock_haloaux));
    memset(&mock_galaxies, 0, sizeof(mock_galaxies));
    memset(&mock_params, 0, sizeof(mock_params));
    
    // Initialize pipeline context
    result = physics_pipeline_initialize_context(pipeline, mock_halos, mock_haloaux, 
                                                mock_galaxies, &mock_params);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Test halo phase execution
    double infall_gas = physics_pipeline_execute_halo_phase(pipeline, 0, 1, 2.0);
    assert(infall_gas == 1.0); // Should match mock value from test module
    
    // Test galaxy phase execution
    result = physics_pipeline_execute_galaxy_phase(pipeline, 0, 0, 1.0, 0.1, 0);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Test post phase execution (should succeed even with no modules supporting it)
    result = physics_pipeline_execute_post_phase(pipeline, 0, 1);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Test final phase execution (should succeed even with no modules supporting it)
    result = physics_pipeline_execute_final_phase(pipeline);
    assert(result == PHYSICS_MODULE_SUCCESS);
    
    // Clean up
    physics_pipeline_destroy(pipeline);
    
    printf("✓ Pipeline execution test passed\n");
}

static void test_registry_shutdown(void)
{
    printf("Testing registry shutdown...\n");
    
    // Shutdown registry (this should call module shutdown functions)
    physics_module_registry_shutdown();
    
    // Check that registry is now empty
    int count = physics_module_get_count();
    assert(count == 0);
    
    printf("✓ Registry shutdown test passed\n");
}

int main(void)
{
    printf("=== Physics Module Interface Tests ===\n\n");
    
    test_module_registration();
    test_dependency_resolution();
    test_module_validation();
    test_pipeline_creation();
    test_pipeline_execution();
    test_registry_shutdown();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}