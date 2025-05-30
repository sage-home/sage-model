/**
 * @file test_empty_pipeline.c
 * @brief Test suite for physics-agnostic core infrastructure validation
 * 
 * This test validates that the SAGE core infrastructure can run 
 * with no physics components at all, using just placeholder modules
 * in a completely empty pipeline. It executes all pipeline phases
 * with no physics operations to validate the core-physics separation.
 * 
 * Tests cover:
 * - Core-physics separation principle validation
 * - Module system functionality with placeholder modules
 * - Pipeline execution across all phases (HALO, GALAXY, POST, FINAL)
 * - Memory management with minimal properties
 * 
 * The test is self-contained and doesn't require external scripts,
 * making it consistent with other SAGE unit tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_build_model.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_evolution_diagnostics.h"
#include "../src/core/physics_pipeline_executor.h"
#include "../src/core/core_read_parameter_file.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_pipeline_registry.h"

// Test counter for reporting (following test_template.c)
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions with statistics
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } else { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } \
} while(0)

/* Function prototypes */
static int setup_minimal_test_environment(void);
static int verify_module_loading(void);
static int verify_core_physics_separation(void);
static int verify_pipeline_execution(void);
static int verify_basic_galaxy_properties(struct GALAXY *galaxies, int ngal);
static int verify_phase_specific_execution(void);

int main(int argc, char **argv) {
    /* Mark unused parameters */
    (void)argc;
    (void)argv;

    printf("\n========================================\n");
    printf("Starting tests for test_empty_pipeline\n");
    printf("========================================\n");
    
    /* Initialize logging */
    logging_init(LOG_LEVEL_INFO, stdout);
    LOG_INFO("=== Empty Pipeline Validation Test ===");
    
    /* Setup minimal test environment without full SAGE initialization */
    if (setup_minimal_test_environment() != 0) {
        printf("ERROR: Failed to set up minimal test environment\n");
        return 1;
    }
    
    /* Run test suite */
    if (verify_module_loading() != 0) return 1;
    if (verify_core_physics_separation() != 0) return 1;
    if (verify_pipeline_execution() != 0) return 1;
    if (verify_phase_specific_execution() != 0) return 1;
    
    /* Cleanup */
    cleanup_logging();
    
    /* Report results */
    if (tests_run == tests_passed) {
        printf("\n✅ Empty Pipeline Validation Test PASSED\n");
        printf("This validates that the core can run without actual physics modules.\n");
        printf("\n=== Core-Physics Separation Summary ===\n");
        printf("- Core infrastructure operates independently: ✅ YES\n");
        printf("- All pipeline phases executed successfully: ✅ YES\n");
        printf("- Memory management with minimal properties: ✅ OK\n");
        printf("- Module system handles placeholder modules: ✅ YES\n");
    } else {
        printf("❌ Empty Pipeline Validation Test FAILED\n");
    }
    
    printf("\n========================================\n");
    printf("Test results for test_empty_pipeline:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    return (tests_run == tests_passed) ? 0 : 1;

}

/**
 * Setup minimal test environment without full SAGE initialization
 * This allows us to test core functionality without complex dependencies
 */
static int setup_minimal_test_environment(void) {
    /* Check if module system is initialized (should be via constructor functions) */
    TEST_ASSERT(global_module_registry != NULL, "Module registry should be initialized by placeholder module constructors");
    TEST_ASSERT(global_module_registry->num_modules > 0, "Pre-registered modules should be found");
    
    LOG_INFO("Found %d pre-registered modules", global_module_registry->num_modules);
    
    /* Create a pipeline using the registry system */
    struct module_pipeline *pipeline = pipeline_create_with_standard_modules();
    if (pipeline == NULL) {
        printf("ERROR: Failed to create pipeline\n");
        return -1;
    }
    
    /* Set as global pipeline */
    pipeline_set_global(pipeline);
    
    TEST_ASSERT(pipeline_get_global() != NULL, "Global pipeline should be set");
    
    LOG_INFO("Minimal test environment setup complete");
    return 0;
}

/**
 * Verify that all required modules are loaded and pipeline is configured
 */
static int verify_module_loading(void) {
    printf("\n=== Testing module loading and pipeline configuration ===\n");
    
    /* Get the global module registry */
    struct module_registry *registry = global_module_registry;
    TEST_ASSERT(registry != NULL, "Module registry should be initialized");
    
    LOG_INFO("Module registry has %d modules loaded", registry->num_modules);
    TEST_ASSERT(registry->num_modules > 0, "At least one module should be loaded");
    
    /* Check for placeholder modules */
    bool found_placeholder = false;
    int placeholder_count = 0;
    for (int i = 0; i < registry->num_modules; i++) {
        struct base_module *module = registry->modules[i].module;
        if (module != NULL && 
            (strncmp(module->name, "placeholder", 11) == 0 || 
             strncmp(module->name, "Placeholder", 11) == 0)) {
            found_placeholder = true;
            placeholder_count++;
            LOG_INFO("Found placeholder module: %s - OK", module->name);
        }
    }
    
    TEST_ASSERT(found_placeholder, "At least one placeholder module should be registered");
    LOG_INFO("Total placeholder modules found: %d", placeholder_count);
    
    /* Verify pipeline is configured */
    struct module_pipeline *pipeline = pipeline_get_global();
    TEST_ASSERT(pipeline != NULL, "Global pipeline should be initialized");
    TEST_ASSERT(pipeline->num_steps > 0, "Pipeline should have at least one step");
    
    LOG_INFO("Pipeline has %d steps - OK", pipeline->num_steps);
    
    return 0;
}

/**
 * Verify core-physics separation principle
 */
static int verify_core_physics_separation(void) {
    printf("\n=== Testing core-physics separation principle ===\n");
    
    struct module_registry *registry = global_module_registry;
    
    /* Verify no actual physics modules are loaded */
    bool found_physics_module = false;
    for (int i = 0; i < registry->num_modules; i++) {
        struct base_module *module = registry->modules[i].module;
        if (module->type >= MODULE_TYPE_COOLING && module->type <= MODULE_TYPE_MERGERS) {
            // Check if this is actually a physics module vs placeholder
            // Handle both "placeholder_" and "Placeholder" naming conventions
            if (strncmp(module->name, "placeholder", 11) != 0 && 
                strncmp(module->name, "Placeholder", 11) != 0) {
                found_physics_module = true;
                LOG_ERROR("Found non-placeholder physics module: %s", module->name);
            }
        }
    }
    
    TEST_ASSERT(!found_physics_module, "Core should run with only placeholder modules");
    
    /* Verify all loaded modules are placeholder modules */
    int non_placeholder_count = 0;
    for (int i = 0; i < registry->num_modules; i++) {
        struct base_module *module = registry->modules[i].module;
        if (module != NULL && 
            strncmp(module->name, "placeholder", 11) != 0 && 
            strncmp(module->name, "Placeholder", 11) != 0) {
            non_placeholder_count++;
            LOG_WARNING("Non-placeholder module found: %s", module->name);
        }
    }
    
    TEST_ASSERT(non_placeholder_count == 0, "All modules should be placeholder modules for core-physics separation test");
    LOG_INFO("Core-physics separation verified: only placeholder modules loaded");
    
    return 0;
}

/**
 * Verify that the pipeline can be executed with all phases
 */
static int verify_pipeline_execution(void) {
    printf("\n=== Testing pipeline execution across all phases ===\n");
    
    struct module_pipeline *pipeline = pipeline_get_global();
    TEST_ASSERT(pipeline != NULL, "Global pipeline should be initialized");
    
    /* Create a pipeline context with minimal data */
    struct pipeline_context context;
    memset(&context, 0, sizeof(struct pipeline_context));
    
    /* Create a minimal params structure with required fields */
    struct params test_params;
    memset(&test_params, 0, sizeof(struct params));
    
    /* Set minimal required parameters for property allocation */
    test_params.simulation.NumSnapOutputs = 8;  /* Required for StarFormationHistory array */
    test_params.cosmology.Hubble_h = 0.73;      /* Common parameter that might be needed */
    
    /* Create a small set of test galaxies using proper core APIs */
    int ngal = 5;
    LOG_INFO("Creating %d test galaxies", ngal);
    struct GALAXY *galaxies = (struct GALAXY *)calloc(ngal, sizeof(struct GALAXY));
    TEST_ASSERT(galaxies != NULL, "Failed to allocate test galaxies");
    
    /* Initialize galaxies using proper core APIs */
    for (int i = 0; i < ngal; i++) {
        /* Initialize the galaxy structure properly */
        memset(&galaxies[i], 0, sizeof(struct GALAXY));
        
        /* Set basic identifiers */
        galaxies[i].SnapNum = 0;
        galaxies[i].Type = (i == 0) ? 0 : 1; /* First galaxy is central */
        galaxies[i].GalaxyIndex = i;
        
        /* Allocate properties using core API */
        int status = allocate_galaxy_properties(&galaxies[i], &test_params);
        TEST_ASSERT(status == 0, "Failed to allocate galaxy properties using core API");
        TEST_ASSERT(galaxies[i].properties != NULL, "Galaxy properties should be allocated");
    }
    
    /* Initialize context with parameters */
    context.params = &test_params;
    context.galaxies = galaxies;
    context.ngal = ngal;
    context.redshift = 0.0;
    
    /* Execute all phases systematically */
    LOG_INFO("Executing HALO phase...");
    context.execution_phase = PIPELINE_PHASE_HALO;
    int status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_HALO);
    TEST_ASSERT(status == 0, "HALO phase execution failed");
    
    LOG_INFO("Executing GALAXY phase for each galaxy...");
    for (int i = 0; i < ngal; i++) {
        context.current_galaxy = i;
        context.execution_phase = PIPELINE_PHASE_GALAXY;
        status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_GALAXY);
        TEST_ASSERT(status == 0, "GALAXY phase execution failed");
        TEST_ASSERT(context.current_galaxy == i, "Galaxy index should be preserved during execution");
    }
    
    LOG_INFO("Executing POST phase...");
    context.execution_phase = PIPELINE_PHASE_POST;
    status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_POST);
    TEST_ASSERT(status == 0, "POST phase execution failed");
    
    LOG_INFO("Executing FINAL phase...");
    context.execution_phase = PIPELINE_PHASE_FINAL;
    status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_FINAL);
    TEST_ASSERT(status == 0, "FINAL phase execution failed");
    
    /* Verify galaxies still have basic properties */
    if (verify_basic_galaxy_properties(galaxies, ngal) != 0) {
        return 1;
    }
    
    /* Clean up using proper core APIs */
    for (int i = 0; i < ngal; i++) {
        if (galaxies[i].properties != NULL) {
            free_galaxy_properties(&galaxies[i]);
            galaxies[i].properties = NULL;
        }
    }
    free(galaxies);
    
    LOG_INFO("All pipeline phases executed successfully");
    
    return 0;
}

/**
 * Verify that basic galaxy properties are intact after pipeline execution
 */
static int verify_basic_galaxy_properties(struct GALAXY *galaxies, int ngal) {
    printf("\n=== Testing galaxy property integrity ===\n");
    
    for (int i = 0; i < ngal; i++) {
        struct GALAXY *gal = &galaxies[i];
        
        /* Check core identifiers */
        TEST_ASSERT(gal->GalaxyIndex == (uint64_t)i, "GalaxyIndex should be preserved");
        TEST_ASSERT((gal->Type == 0 || gal->Type == 1), "Type should be valid (0 or 1)");
        
        /* Check properties structure */
        TEST_ASSERT(gal->properties != NULL, "Galaxy properties should remain allocated");
        
        LOG_INFO("Galaxy %d properties verified - OK", i);
    }
    
    return 0;
}

/**
 * Verify phase-specific execution behavior
 */
static int verify_phase_specific_execution(void) {
    printf("\n=== Testing phase-specific execution behavior ===\n");
    
    struct module_pipeline *pipeline = pipeline_get_global();
    struct pipeline_context context;
    memset(&context, 0, sizeof(struct pipeline_context));
    
    /* Create minimal test data */
    struct params test_params;
    memset(&test_params, 0, sizeof(struct params));
    
    /* Set minimal required parameters for property allocation */
    test_params.simulation.NumSnapOutputs = 8;  /* Required for StarFormationHistory array */
    test_params.cosmology.Hubble_h = 0.73;      /* Common parameter that might be needed */
    
    int ngal = 2;
    struct GALAXY *galaxies = (struct GALAXY *)calloc(ngal, sizeof(struct GALAXY));
    TEST_ASSERT(galaxies != NULL, "Failed to allocate galaxies for phase testing");
    
    for (int i = 0; i < ngal; i++) {
        memset(&galaxies[i], 0, sizeof(struct GALAXY));
        galaxies[i].GalaxyIndex = i;
        galaxies[i].Type = i;
        int status = allocate_galaxy_properties(&galaxies[i], &test_params);
        TEST_ASSERT(status == 0, "Failed to allocate galaxy properties for phase testing");
    }
    
    context.params = &test_params;
    context.galaxies = galaxies;
    context.ngal = ngal;
    context.redshift = 1.0;
    
    /* Test each phase individually with proper context */
    uint32_t phases[] = {PIPELINE_PHASE_HALO, PIPELINE_PHASE_GALAXY, PIPELINE_PHASE_POST, PIPELINE_PHASE_FINAL};
    const char* phase_names[] = {"HALO", "GALAXY", "POST", "FINAL"};
    
    for (int p = 0; p < 4; p++) {
        context.execution_phase = phases[p];
        
        if (phases[p] == PIPELINE_PHASE_GALAXY) {
            /* GALAXY phase should be executed per galaxy */
            for (int i = 0; i < ngal; i++) {
                context.current_galaxy = i;
                int status = pipeline_execute_phase(pipeline, &context, phases[p]);
                TEST_ASSERT(status == 0, "Phase-specific execution failed");
            }
        } else {
            /* Other phases execute once */
            int status = pipeline_execute_phase(pipeline, &context, phases[p]);
            TEST_ASSERT(status == 0, "Phase-specific execution failed");
        }
        
        LOG_INFO("Phase %s executed successfully", phase_names[p]);
    }
    
    /* Cleanup */
    for (int i = 0; i < ngal; i++) {
        free_galaxy_properties(&galaxies[i]);
    }
    free(galaxies);
    
    return 0;
}
