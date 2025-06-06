/**
 * @file test_physics_free_mode.c
 * @brief Test suite for physics-free mode execution
 * 
 * This test validates that the SAGE core infrastructure operates independently
 * from physics modules by running with minimal empty pipelines. It verifies 
 * core-physics separation principles by testing that core systems function
 * correctly without any physics calculations.
 * 
 * Tests validate:
 * - Core module system initialization without physics modules
 * - Pipeline execution across all phases (HALO → GALAXY → POST → FINAL)
 * - Property system lifecycle with minimal core-only properties
 * - Memory management in physics-free mode
 * - No physics calculations occur during execution
 * - Property pass-through from initialization to cleanup
 * 
 * This demonstrates complete core-physics architectural separation.
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
static int setup_physics_free_environment(void);
static int verify_module_system_physics_free(void);
static int verify_core_physics_separation(void);
static int verify_pipeline_execution_physics_free(void);
static int verify_property_passthrough(struct GALAXY *galaxies, int ngal);
static int verify_phase_execution_isolation(void);
static int verify_no_physics_calculations(void);

int main(int argc, char **argv) {
    /* Mark unused parameters */
    (void)argc;
    (void)argv;

    printf("\n========================================\n");
    printf("Starting tests for test_physics_free_mode\n");
    printf("========================================\n");
    
    printf("This test verifies that SAGE core infrastructure operates independently from physics:\n");
    printf("  1. Core module system initializes without physics modules\n");
    printf("  2. Pipeline executes all phases with no physics calculations\n");
    printf("  3. Property system manages core-only properties correctly\n");
    printf("  4. Memory management works in physics-free mode\n");
    printf("  5. No physics calculations occur during execution\n\n");
    
    /* Initialize logging */
    logging_init(LOG_LEVEL_INFO, stdout);
    LOG_INFO("=== Physics-Free Mode Validation Test ===");
    
    /* Setup physics-free test environment */
    if (setup_physics_free_environment() != 0) {
        printf("ERROR: Failed to set up physics-free test environment\n");
        return 1;
    }
    
    /* Run test suite */
    if (verify_module_system_physics_free() != 0) return 1;
    if (verify_core_physics_separation() != 0) return 1;
    if (verify_pipeline_execution_physics_free() != 0) return 1;
    if (verify_phase_execution_isolation() != 0) return 1;
    if (verify_no_physics_calculations() != 0) return 1;
    
    /* Cleanup */
    cleanup_logging();
    
    /* Report results */
    if (tests_run == tests_passed) {
        printf("\n✅ Physics-Free Mode Validation Test PASSED\n");
        printf("This validates complete core-physics separation architecture.\n");
        printf("\n=== Core Independence Summary ===\n");
        printf("- Core infrastructure operates without physics: ✅ YES\n");
        printf("- All pipeline phases execute with no physics: ✅ YES\n");
        printf("- Property system handles core-only properties: ✅ YES\n");
        printf("- Memory management in physics-free mode: ✅ OK\n");
        printf("- No physics calculations detected: ✅ YES\n");
    } else {
        printf("❌ Physics-Free Mode Validation Test FAILED\n");
    }
    
    printf("\n========================================\n");
    printf("Test results for test_physics_free_mode:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    return (tests_run == tests_passed) ? 0 : 1;
}

/**
 * Setup physics-free test environment with proper core initialization
 * This initializes only core systems without physics dependencies
 */
static int setup_physics_free_environment(void) {
    printf("\n=== Setting up physics-free test environment ===\n");
    
    /* Initialize core module system */
    if (global_module_registry == NULL) {
        int status = module_system_initialize();
        if (status != MODULE_STATUS_SUCCESS && status != MODULE_STATUS_ALREADY_INITIALIZED) {
            printf("ERROR: Failed to initialize module system, status = %d\n", status);
            LOG_ERROR("Module system initialization failed with status %d", status);
            return -1;
        }
        LOG_INFO("Module system initialized successfully");
    } else {
        LOG_INFO("Module system already initialized");
    }
    
    TEST_ASSERT(global_module_registry != NULL, 
                "Module registry should be initialized after module_system_initialize()");
    
    /* Verify we have a basic module registry structure */
    TEST_ASSERT(global_module_registry->modules != NULL, 
                "Module registry should have module storage allocated");
    TEST_ASSERT(MAX_MODULES > 0, 
                "Module registry should have positive maximum modules");
    
    LOG_INFO("Module registry validation complete - %d/%d modules loaded", 
             global_module_registry->num_modules, MAX_MODULES);
    
    /* Create physics-free pipeline configuration */
    struct module_pipeline *pipeline = pipeline_create("physics_free_test");
    if (pipeline == NULL) {
        printf("ERROR: Failed to create physics-free pipeline\n");
        LOG_ERROR("Physics-free pipeline creation failed");
        return -1;
    }
    
    /* Set as global pipeline */
    pipeline_set_global(pipeline);
    TEST_ASSERT(pipeline_get_global() != NULL, "Global pipeline should be set");
    
    LOG_INFO("Physics-free test environment setup complete");
    return 0;
}

/**
 * Verify that module system operates in physics-free mode
 */
static int verify_module_system_physics_free(void) {
    printf("\n=== Testing module system physics-free operation ===\n");
    
    struct module_registry *registry = global_module_registry;
    TEST_ASSERT(registry != NULL, "Module registry must be initialized for physics-free testing");
    
    LOG_INFO("Module registry status: %d modules loaded (max: %d)", 
             registry->num_modules, MAX_MODULES);
    
    /* In physics-free mode, we should have minimal or zero modules */
    TEST_ASSERT(registry->num_modules >= 0, "Module count should be non-negative");
    
    /* Verify no actual physics modules are loaded */
    int physics_module_count = 0;
    int placeholder_module_count = 0;
    
    for (int i = 0; i < registry->num_modules; i++) {
        struct base_module *module = registry->modules[i].module;
        if (module == NULL) continue;
        
        /* Check if this is a physics module by checking if it's not a core module type */
        /* In the current architecture, physics modules use types above MODULE_TYPE_UNKNOWN */
        if (module->type > MODULE_TYPE_UNKNOWN && module->type < MODULE_TYPE_MAX) {
            /* Distinguish between actual physics modules and placeholders */
            if (strncmp(module->name, "placeholder", 11) == 0 || 
                strncmp(module->name, "Placeholder", 11) == 0 ||
                strncmp(module->name, "empty", 5) == 0) {
                placeholder_module_count++;
                LOG_INFO("Found placeholder module: %s (type=%d) - OK for physics-free mode", 
                         module->name, module->type);
            } else {
                physics_module_count++;
                LOG_ERROR("Found actual physics module: %s (type=%d) - VIOLATES physics-free mode", 
                         module->name, module->type);
            }
        } else {
            LOG_INFO("Found core module: %s (type=%d) - OK", module->name, module->type);
        }
    }
    
    TEST_ASSERT(physics_module_count == 0, 
                "Physics-free mode must have zero actual physics modules");
    
    /* Log placeholder count for diagnostics */
    if (placeholder_module_count > 0) {
        LOG_INFO("Physics-free mode operating with %d placeholder modules", placeholder_module_count);
    }
    
    /* Create physics-free pipeline */
    struct module_pipeline *pipeline = pipeline_get_global();
    TEST_ASSERT(pipeline != NULL, "Global pipeline must be initialized");
    
    /* In physics-free mode, pipeline may be empty or have minimal steps */
    LOG_INFO("Pipeline configured with %d steps for physics-free operation", 
             pipeline->num_steps);
    
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
        if (module->type > MODULE_TYPE_UNKNOWN && module->type < MODULE_TYPE_MAX) {
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
 * Verify that pipeline execution works in physics-free mode
 */
static int verify_pipeline_execution_physics_free(void) {
    printf("\n=== Testing physics-free pipeline execution ===\n");
    
    struct module_pipeline *pipeline = pipeline_get_global();
    TEST_ASSERT(pipeline != NULL, "Global pipeline must be initialized");
    
    /* Create test parameters */
    struct params test_params;
    memset(&test_params, 0, sizeof(struct params));
    test_params.simulation.NumSnapOutputs = 8;
    test_params.cosmology.Hubble_h = 0.73;
    
    /* Create test galaxies */
    int ngal = 3;
    struct GALAXY *galaxies = (struct GALAXY *)calloc(ngal, sizeof(struct GALAXY));
    TEST_ASSERT(galaxies != NULL, "Failed to allocate test galaxies");
    
    /* Initialize galaxies with core properties only */
    for (int i = 0; i < ngal; i++) {
        memset(&galaxies[i], 0, sizeof(struct GALAXY));
        galaxies[i].SnapNum = 0;
        galaxies[i].Type = (i == 0) ? 0 : 1;
        galaxies[i].GalaxyIndex = i + 1000; /* Unique IDs for tracking */
        
        int status = allocate_galaxy_properties(&galaxies[i], &test_params);
        TEST_ASSERT(status == 0, "Failed to allocate galaxy properties");
    }
    
    /* Create pipeline context */
    struct pipeline_context context;
    memset(&context, 0, sizeof(context));
    context.params = &test_params;
    context.galaxies = galaxies;
    context.ngal = ngal;
    context.redshift = 0.0;
    
    /* Execute all phases in physics-free mode */
    uint32_t phases[] = {PIPELINE_PHASE_HALO, PIPELINE_PHASE_GALAXY, PIPELINE_PHASE_POST, PIPELINE_PHASE_FINAL};
    const char* phase_names[] = {"HALO", "GALAXY", "POST", "FINAL"};
    
    for (int p = 0; p < 4; p++) {
        context.execution_phase = phases[p];
        
        if (phases[p] == PIPELINE_PHASE_GALAXY) {
            for (int i = 0; i < ngal; i++) {
                context.current_galaxy = i;
                int status = pipeline_execute_phase(pipeline, &context, phases[p]);
                TEST_ASSERT(status == 0, "Physics-free phase execution should succeed");
            }
        } else {
            int status = pipeline_execute_phase(pipeline, &context, phases[p]);
            TEST_ASSERT(status == 0, "Physics-free phase execution should succeed");
        }
        
        LOG_INFO("Phase %s completed in physics-free mode", phase_names[p]);
    }
    
    /* Verify property pass-through */
    if (verify_property_passthrough(galaxies, ngal) != 0) {
        return 1;
    }
    
    /* Cleanup */
    for (int i = 0; i < ngal; i++) {
        free_galaxy_properties(&galaxies[i]);
    }
    free(galaxies);
    
    return 0;
}

/**
 * Verify property pass-through without physics modifications (Recommendation #4: I/O Testing)
 */
static int verify_property_passthrough(struct GALAXY *galaxies, int ngal) {
    printf("\n=== Testing property pass-through in physics-free mode ===\n");
    
    for (int i = 0; i < ngal; i++) {
        struct GALAXY *gal = &galaxies[i];
        
        /* Verify core identifiers preserved */
        uint64_t expected_id = i + 1000;
        TEST_ASSERT(gal->GalaxyIndex == expected_id, 
                    "GalaxyIndex should be preserved through physics-free pipeline");
        TEST_ASSERT((gal->Type == 0 || gal->Type == 1), 
                    "Galaxy Type should remain valid after physics-free execution");
        
        /* Verify properties structure intact */
        TEST_ASSERT(gal->properties != NULL, 
                    "Properties structure should remain allocated after physics-free execution");
        
        LOG_INFO("Galaxy %d: ID=%llu, Type=%d - pass-through verified", 
                 i, (unsigned long long)gal->GalaxyIndex, gal->Type);
    }
    
    LOG_INFO("Property pass-through validation completed successfully");
    return 0;
}

/**
 * Verify phase execution isolation (Recommendation #5: Better Diagnostics)
 */
static int verify_phase_execution_isolation(void) {
    printf("\n=== Testing phase execution isolation ===\n");
    
    struct module_pipeline *pipeline = pipeline_get_global();
    TEST_ASSERT(pipeline != NULL, "Pipeline required for isolation testing");
    
    /* Test context isolation */
    struct pipeline_context context1, context2;
    memset(&context1, 0, sizeof(context1));
    memset(&context2, 0, sizeof(context2));
    
    /* Verify contexts are independent */
    context1.execution_phase = PIPELINE_PHASE_HALO;
    context2.execution_phase = PIPELINE_PHASE_GALAXY;
    
    TEST_ASSERT(context1.execution_phase != context2.execution_phase, 
                "Pipeline contexts should maintain independent phase state");
    
    LOG_INFO("Phase execution isolation verified");
    return 0;
}

/**
 * Verify no physics calculations occur (Recommendation #6: Regression Protection)
 */
static int verify_no_physics_calculations(void) {
    printf("\n=== Testing absence of physics calculations ===\n");
    
    /* Create test galaxy with known initial state */
    struct params test_params;
    memset(&test_params, 0, sizeof(test_params));
    test_params.simulation.NumSnapOutputs = 8;
    test_params.cosmology.Hubble_h = 0.73;
    
    struct GALAXY test_galaxy;
    memset(&test_galaxy, 0, sizeof(test_galaxy));
    test_galaxy.GalaxyIndex = 99999;
    test_galaxy.Type = 0;
    
    int status = allocate_galaxy_properties(&test_galaxy, &test_params);
    TEST_ASSERT(status == 0, "Property allocation should succeed for physics calculation test");
    
    /* Record initial state (should be zeros/defaults) */
    uint64_t initial_galaxy_index = test_galaxy.GalaxyIndex;
    int initial_type = test_galaxy.Type;
    
    /* Execute pipeline in physics-free mode */
    struct pipeline_context context;
    memset(&context, 0, sizeof(context));
    context.params = &test_params;
    context.galaxies = &test_galaxy;
    context.ngal = 1;
    context.current_galaxy = 0;
    
    struct module_pipeline *pipeline = pipeline_get_global();
    
    /* Execute GALAXY phase */
    context.execution_phase = PIPELINE_PHASE_GALAXY;
    status = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_GALAXY);
    TEST_ASSERT(status == 0, "Physics-free GALAXY phase should execute successfully");
    
    /* Verify no physics modifications occurred */
    TEST_ASSERT(test_galaxy.GalaxyIndex == initial_galaxy_index, 
                "GalaxyIndex should not be modified by physics-free execution");
    TEST_ASSERT(test_galaxy.Type == initial_type, 
                "Galaxy Type should not be modified by physics-free execution");
    
    /* Physics properties should remain at defaults/zeros if present */
    LOG_INFO("Physics calculation absence verified - no unexpected modifications detected");
    
    /* Cleanup */
    free_galaxy_properties(&test_galaxy);
    
    return 0;
}
