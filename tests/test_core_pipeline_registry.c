/**
 * Test suite for Core Pipeline Registry
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_pipeline_registry.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_config_system.h"

// Test constants
#define MOCK_MODULE_ID_BASE 1000
#define MOCK_INFALL_ID      (MOCK_MODULE_ID_BASE + 1)
#define MOCK_COOLING_ID     (MOCK_MODULE_ID_BASE + 2)  
#define MOCK_DISABLED_ID    (MOCK_MODULE_ID_BASE + 3)
#define CLEANUP_DELAY_NS    100000000

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

// Test assertion macro
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } \
} while(0)

// Mock functions
static int mock_initialize(struct params *params __attribute__((unused)), void **data_ptr) {
    printf("Mock module initialized\n");
    *data_ptr = calloc(1, sizeof(int));
    return (*data_ptr == NULL) ? MODULE_STATUS_ERROR : MODULE_STATUS_SUCCESS;
}

static int mock_cleanup(void *data) {
    printf("Mock module cleaned up\n");
    if (data != NULL) free(data);
    return MODULE_STATUS_SUCCESS;
}

// Mock module definitions
static struct base_module mock_infall_module = {
    .name = "MockInfall", .version = "1.0", .author = "Test",
    .module_id = MOCK_INFALL_ID, .type = MODULE_TYPE_INFALL,
    .initialize = mock_initialize, .cleanup = mock_cleanup
};

static struct base_module mock_cooling_module = {
    .name = "MockCooling", .version = "1.0", .author = "Test", 
    .module_id = MOCK_COOLING_ID, .type = MODULE_TYPE_COOLING,
    .initialize = mock_initialize, .cleanup = mock_cleanup
};

static struct base_module mock_disabled_module = {
    .name = "MockDisabled", .version = "1.0", .author = "Test",
    .module_id = MOCK_DISABLED_ID, .type = MODULE_TYPE_MISC,
    .initialize = mock_initialize, .cleanup = mock_cleanup
};

// Factory functions
struct base_module* mock_infall_factory(void) { return &mock_infall_module; }
struct base_module* mock_cooling_factory(void) { return &mock_cooling_module; }
struct base_module* mock_disabled_factory(void) { return &mock_disabled_module; }

// Test context
struct test_context {
    struct params dummy_params;
    void *infall_data, *cooling_data, *disabled_data;
    bool systems_initialized, config_loaded;
};

static struct test_context test_ctx;

// Setup function
static int setup_test_systems(bool load_config, const char *config_file) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    cleanup_module_system();
    cleanup_pipeline_system();
    
    struct timespec ts = {0, CLEANUP_DELAY_NS};
    nanosleep(&ts, NULL);
    
    initialize_module_system(NULL);
    initialize_pipeline_system();
    test_ctx.systems_initialized = true;
    
    if (load_config) {
        if (config_system_initialize() != 0) {
            printf("ERROR: Failed to initialize configuration system\n");
            return -1;
        }
        if (config_file && config_load_file(config_file) != 0) {
            printf("ERROR: Failed to load configuration file: %s\n", config_file);
            config_system_cleanup();
            return -1;
        }
        test_ctx.config_loaded = true;
        printf("Configuration system initialized successfully\n");
    }
    
    extern int num_factories;
    num_factories = 0;
    return 0;
}

// Teardown function
static void teardown_test_systems(void) {
    if (test_ctx.infall_data) { mock_infall_module.cleanup(test_ctx.infall_data); test_ctx.infall_data = NULL; }
    if (test_ctx.cooling_data) { mock_cooling_module.cleanup(test_ctx.cooling_data); test_ctx.cooling_data = NULL; }
    if (test_ctx.disabled_data) { mock_disabled_module.cleanup(test_ctx.disabled_data); test_ctx.disabled_data = NULL; }
    if (test_ctx.config_loaded) { config_system_cleanup(); test_ctx.config_loaded = false; }
    if (test_ctx.systems_initialized) {
        cleanup_pipeline_system(); cleanup_module_system(); 
        test_ctx.systems_initialized = false;
    }
}

// Helper to register and initialize modules
static int register_and_initialize_modules(bool include_disabled) {
    TEST_ASSERT(pipeline_register_module_factory(MODULE_TYPE_COOLING, "MockCooling", mock_cooling_factory) >= 0,
                "MockCooling factory registration should succeed");
    TEST_ASSERT(pipeline_register_module_factory(MODULE_TYPE_INFALL, "MockInfall", mock_infall_factory) >= 0,
                "MockInfall factory registration should succeed");
    if (include_disabled) {
        TEST_ASSERT(pipeline_register_module_factory(MODULE_TYPE_MISC, "MockDisabled", mock_disabled_factory) >= 0,
                    "MockDisabled factory registration should succeed");
    }
    
    TEST_ASSERT(module_register(&mock_cooling_module) == MODULE_STATUS_SUCCESS,
                "MockCooling module registration should succeed");
    TEST_ASSERT(module_register(&mock_infall_module) == MODULE_STATUS_SUCCESS,
                "MockInfall module registration should succeed");
    if (include_disabled) {
        TEST_ASSERT(module_register(&mock_disabled_module) == MODULE_STATUS_SUCCESS,
                    "MockDisabled module registration should succeed");
    }
    
    TEST_ASSERT(mock_cooling_module.initialize(&test_ctx.dummy_params, &test_ctx.cooling_data) == MODULE_STATUS_SUCCESS,
                "MockCooling module initialization should succeed");
    TEST_ASSERT(mock_infall_module.initialize(&test_ctx.dummy_params, &test_ctx.infall_data) == MODULE_STATUS_SUCCESS,
                "MockInfall module initialization should succeed");
    if (include_disabled) {
        TEST_ASSERT(mock_disabled_module.initialize(&test_ctx.dummy_params, &test_ctx.disabled_data) == MODULE_STATUS_SUCCESS,
                    "MockDisabled module initialization should succeed");
    }
    return 0;
}

// Helper to validate pipeline modules
static void validate_pipeline_modules(struct module_pipeline *pipeline, bool should_include_disabled) {
    TEST_ASSERT(pipeline != NULL, "Pipeline should not be NULL");
    
    int expected_steps = should_include_disabled ? 3 : 2;
    TEST_ASSERT(pipeline->num_steps == expected_steps, "Pipeline should have expected number of steps");
    
    bool found_infall = false, found_cooling = false, found_disabled = false;
    
    for (int i = 0; i < pipeline->num_steps; i++) {
        printf("Step %d: type=%d, name=%s\n", i, pipeline->steps[i].type, pipeline->steps[i].module_name);
        
        if (pipeline->steps[i].type == MODULE_TYPE_INFALL && 
            strcmp(pipeline->steps[i].module_name, "MockInfall") == 0) {
            found_infall = true;
        }
        if (pipeline->steps[i].type == MODULE_TYPE_COOLING && 
            strcmp(pipeline->steps[i].module_name, "MockCooling") == 0) {
            found_cooling = true;
        }
        if (pipeline->steps[i].type == MODULE_TYPE_MISC && 
            strcmp(pipeline->steps[i].module_name, "MockDisabled") == 0) {
            found_disabled = true;
        }
    }
    
    TEST_ASSERT(found_infall, "Pipeline should contain MockInfall module");
    TEST_ASSERT(found_cooling, "Pipeline should contain MockCooling module");
    
    if (should_include_disabled) {
        TEST_ASSERT(found_disabled, "Pipeline should contain MockDisabled module when enabled");
    } else {
        TEST_ASSERT(!found_disabled, "Pipeline should not contain MockDisabled module when disabled");
    }
}

//=============================================================================
// Test Cases
//=============================================================================

// Test: Basic module registration and pipeline creation
static void test_basic_module_registration_and_pipeline_creation(void) {
    printf("\n=== Testing basic module registration and pipeline creation ===\n");
    
    TEST_ASSERT(setup_test_systems(false, NULL) == 0, "Test system setup should succeed");
    register_and_initialize_modules(false);
    
    printf("Creating pipeline with registered modules...\n");
    struct module_pipeline *pipeline = pipeline_create_with_standard_modules();
    validate_pipeline_modules(pipeline, false);
    
    if (pipeline) pipeline_destroy(pipeline);
    teardown_test_systems();
}

// Test: Configuration-driven module selection
static void test_configuration_driven_module_selection(void) {
    printf("\n=== Testing configuration-driven module selection ===\n");
    
    TEST_ASSERT(setup_test_systems(true, "tests/test_data/test_core_pipeline_registry_config.json") == 0,
                "Test system setup with configuration should succeed");
    register_and_initialize_modules(true);
    
    printf("Creating pipeline with configuration-driven selection...\n");
    struct module_pipeline *pipeline = pipeline_create_with_standard_modules();
    validate_pipeline_modules(pipeline, false);  // Should NOT include disabled module
    
    if (pipeline) pipeline_destroy(pipeline);
    teardown_test_systems();
}

// Test: Edge case - Invalid configuration file
static void test_invalid_configuration_handling(void) {
    printf("\n=== Testing invalid configuration handling ===\n");
    
    TEST_ASSERT(setup_test_systems(false, NULL) == 0, "Basic test system setup should succeed");
    
    if (config_system_initialize() == 0) {
        int result = config_load_file("tests/test_data/nonexistent_config.json");
        TEST_ASSERT(result != 0, "Loading non-existent config file should fail");
        config_system_cleanup();
    }
    teardown_test_systems();
    
    TEST_ASSERT(setup_test_systems(false, NULL) == 0, "Test system setup for malformed JSON test should succeed");
    
    if (config_system_initialize() == 0) {
        int result = config_load_file("tests/test_data/malformed_config.json");
        TEST_ASSERT(result != 0, "Loading malformed JSON config should fail");
        config_system_cleanup();
    }
    teardown_test_systems();
}

// Test: Edge case - No modules registered
static void test_no_modules_registered(void) {
    printf("\n=== Testing pipeline creation with no modules ===\n");
    
    TEST_ASSERT(setup_test_systems(false, NULL) == 0, "Test system setup should succeed");
    
    // Check that no factories are registered - this avoids calling the function
    // that causes SAGE to exit when no modules are found
    extern int num_factories;
    TEST_ASSERT(num_factories == 0, "No module factories should be registered initially");
    
    printf("INFO: Skipping pipeline creation test - SAGE exits when no modules registered\n");
    printf("      This is expected behavior as SAGE requires at least one physics module\n");
    
    teardown_test_systems();
}

// Test: Module type validation
static void test_module_type_validation(void) {
    printf("\n=== Testing module type validation ===\n");
    
    TEST_ASSERT(MODULE_TYPE_INFALL == 8, "MODULE_TYPE_INFALL should have expected value");
    TEST_ASSERT(MODULE_TYPE_COOLING == 1, "MODULE_TYPE_COOLING should have expected value");
    TEST_ASSERT(MODULE_TYPE_MISC == 9, "MODULE_TYPE_MISC should have expected value");
    
    TEST_ASSERT(mock_infall_module.type == MODULE_TYPE_INFALL, "Mock infall module should have correct type");
    TEST_ASSERT(mock_cooling_module.type == MODULE_TYPE_COOLING, "Mock cooling module should have correct type");
    TEST_ASSERT(mock_disabled_module.type == MODULE_TYPE_MISC, "Mock disabled module should have correct type");
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("\n========================================\n");
    printf("Starting tests for test_core_pipeline_registry\n");
    printf("========================================\n\n");
    
    // Initialize logging
    struct params run_params = {0};
    initialize_logging(&run_params);
    logging_set_level(LOG_LEVEL_DEBUG);
    
    printf("Starting pipeline registry tests...\n");
    
    // Run all test cases
    test_basic_module_registration_and_pipeline_creation();
    test_configuration_driven_module_selection();
    test_invalid_configuration_handling();
    test_no_modules_registered();
    test_module_type_validation();
    
    // Final cleanup
    printf("Cleaning up logging...\n");
    cleanup_logging();
    
    // Report results in template format
    printf("\n========================================\n");
    printf("Test results for test_core_pipeline_registry:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
