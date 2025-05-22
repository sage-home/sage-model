#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h> /* For nanosleep */

#include "../src/core/core_allvars.h"
#include "../src/core/core_pipeline_registry.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_config_system.h"
#include "test_core_pipeline_registry.h"

// --- Mock Module Functions ---
static int mock_initialize(struct params *params __attribute__((unused)), void **data_ptr) {
    printf("Mock module initialized\n");
    // Create a dummy data structure to signal initialization
    *data_ptr = calloc(1, sizeof(int));
    if (*data_ptr == NULL) {
        return MODULE_STATUS_ERROR;
    }
    return MODULE_STATUS_SUCCESS;
}

static int mock_cleanup(void *data) {
    printf("Mock module cleaned up\n");
    if (data != NULL) {
        free(data);
    }
    return MODULE_STATUS_SUCCESS;
}

// --- Mock Modules ---
// Mock Infall Module - simplified version just for this test
static struct base_module mock_infall_module = {
    .name = "MockInfall",
    .version = "1.0",
    .author = "Test",
    .module_id = 1001, // Using higher IDs to avoid conflicts
    .type = MODULE_TYPE_INFALL,
    .initialize = mock_initialize,
    .cleanup = mock_cleanup
};

struct base_module* mock_infall_factory(void) {
    return &mock_infall_module;
}

// Mock Cooling Module - simplified version just for this test
static struct base_module mock_cooling_module = {
    .name = "MockCooling",
    .version = "1.0",
    .author = "Test",
    .module_id = 1002, // Using higher IDs to avoid conflicts
    .type = MODULE_TYPE_COOLING,
    .initialize = mock_initialize,
    .cleanup = mock_cleanup
};

struct base_module* mock_cooling_factory(void) {
    return &mock_cooling_module;
}

// Mock Disabled Module - will be disabled in config
static struct base_module mock_disabled_module = {
    .name = "MockDisabled",
    .version = "1.0",
    .author = "Test",
    .module_id = 1003,
    .type = MODULE_TYPE_MISC,
    .initialize = mock_initialize,
    .cleanup = mock_cleanup
};

struct base_module* mock_disabled_factory(void) {
    return &mock_disabled_module;
}

// Test function (original implementation without config)
static int test_module_registration_and_pipeline_creation(void) {
    printf("\n===== Starting test_module_registration_and_pipeline_creation... =====\n");
    int status = 0;

    // Clean up and reinitialize all systems
    cleanup_module_system();
    cleanup_pipeline_system();
    
    // Sleep briefly to ensure cleanup is complete
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100000000; // 100ms
    nanosleep(&ts, NULL);
    
    initialize_module_system(NULL);
    initialize_pipeline_system();

    // Clear any existing module factories
    extern int num_factories;
    num_factories = 0;

    // Register the mock module factories
    printf("Registering mock module factories...\n");
    if (pipeline_register_module_factory(MODULE_TYPE_COOLING, "MockCooling", mock_cooling_factory) < 0) {
        printf("ERROR: Failed to register MockCooling factory\n");
        return 1;
    }

    if (pipeline_register_module_factory(MODULE_TYPE_INFALL, "MockInfall", mock_infall_factory) < 0) {
        printf("ERROR: Failed to register MockInfall factory\n");
        return 1;
    }

    printf("Module factories registered successfully (%d factories)\n", num_factories);

    // Register the modules with the module system first
    struct params dummy_params = {0}; // Create empty params for module initialization
    struct base_module *infall = mock_infall_factory();
    struct base_module *cooling = mock_cooling_factory();
    
    // Pre-register modules with module system
    void *infall_data = NULL;
    void *cooling_data = NULL;
    
    if (module_register(infall) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to register MockInfall module\n");
        return 1;
    }
    
    if (module_register(cooling) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to register MockCooling module\n");
        return 1;
    }
    
    // Initialize modules
    if (infall->initialize(&dummy_params, &infall_data) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to initialize MockInfall module\n");
        return 1;
    }
    
    if (cooling->initialize(&dummy_params, &cooling_data) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to initialize MockCooling module\n");
        return 1;
    }

    printf("Modules registered and initialized successfully\n");

    // Create pipeline with registered modules
    printf("Creating pipeline with registered modules...\n");
    struct module_pipeline *pipeline = pipeline_create_with_standard_modules();
    if (!pipeline) {
        printf("ERROR: pipeline_create_with_standard_modules returned NULL\n");
        return 1;
    }

    printf("Pipeline created with %d steps\n", pipeline->num_steps);

    // Verify that pipeline has exactly 2 steps (for our 2 registered factories)
    if (pipeline->num_steps != 2) {
        printf("ERROR: Expected 2 pipeline steps, got %d\n", pipeline->num_steps);
        status = 1;
    } else {
        // Verify the steps contain our modules (order doesn't matter)
        bool found_infall = false;
        bool found_cooling = false;

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
        }

        if (!found_infall) {
            printf("ERROR: MockInfall module not found in pipeline\n");
            status = 1;
        }
        if (!found_cooling) {
            printf("ERROR: MockCooling module not found in pipeline\n");
            status = 1;
        }
    }

    // Cleanup
    pipeline_destroy(pipeline);
    
    // Cleanup modules if needed
    printf("Cleaning up modules...\n");
    infall->cleanup(infall_data);
    cooling->cleanup(cooling_data);
    
    if (status == 0) {
        printf("Test PASSED!\n");
    } else {
        printf("Test FAILED!\n");
    }
    printf("===== test_module_registration_and_pipeline_creation complete =====\n\n");
    return status;
}

// Test function with configuration
static int test_module_registration_and_pipeline_creation_with_config(void) {
    printf("\n===== Starting test_module_registration_and_pipeline_creation_with_config... =====\n");
    int status = 0;

    // Clean up and reinitialize all systems
    cleanup_module_system();
    cleanup_pipeline_system();
    
    // Sleep briefly to ensure cleanup is complete
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100000000; // 100ms
    nanosleep(&ts, NULL);
    
    initialize_module_system(NULL);
    initialize_pipeline_system();
    
    // Initialize the configuration system
    if (config_system_initialize() != 0) {
        printf("ERROR: Failed to initialize configuration system\n");
        return 1;
    }

    // Load test configuration file
    if (config_load_file("tests/test_config.json") != 0) {
        printf("ERROR: Failed to load test configuration file\n");
        config_system_cleanup();
        return 1;
    }
    printf("Loaded test configuration successfully\n");

    // Clear any existing module factories
    extern int num_factories;
    num_factories = 0;

    // Register the mock module factories
    printf("Registering mock module factories...\n");
    if (pipeline_register_module_factory(MODULE_TYPE_COOLING, "MockCooling", mock_cooling_factory) < 0) {
        printf("ERROR: Failed to register MockCooling factory\n");
        config_system_cleanup();
        return 1;
    }

    if (pipeline_register_module_factory(MODULE_TYPE_INFALL, "MockInfall", mock_infall_factory) < 0) {
        printf("ERROR: Failed to register MockInfall factory\n");
        config_system_cleanup();
        return 1;
    }

    if (pipeline_register_module_factory(MODULE_TYPE_MISC, "MockDisabled", mock_disabled_factory) < 0) {
        printf("ERROR: Failed to register MockDisabled factory\n");
        config_system_cleanup();
        return 1;
    }

    printf("Module factories registered successfully (%d factories)\n", num_factories);

    // Pre-register modules with module system
    struct params dummy_params = {0}; // Create empty params for module initialization
    struct base_module *infall = mock_infall_factory();
    struct base_module *cooling = mock_cooling_factory();
    struct base_module *disabled = mock_disabled_factory();
    
    void *infall_data = NULL;
    void *cooling_data = NULL;
    void *disabled_data = NULL;
    
    if (module_register(infall) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to register MockInfall module\n");
        config_system_cleanup();
        return 1;
    }
    
    if (module_register(cooling) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to register MockCooling module\n");
        config_system_cleanup();
        return 1;
    }
    
    if (module_register(disabled) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to register MockDisabled module\n");
        config_system_cleanup();
        return 1;
    }
    
    // Initialize modules
    if (infall->initialize(&dummy_params, &infall_data) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to initialize MockInfall module\n");
        config_system_cleanup();
        return 1;
    }
    
    if (cooling->initialize(&dummy_params, &cooling_data) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to initialize MockCooling module\n");
        config_system_cleanup();
        return 1;
    }
    
    if (disabled->initialize(&dummy_params, &disabled_data) != MODULE_STATUS_SUCCESS) {
        printf("ERROR: Failed to initialize MockDisabled module\n");
        config_system_cleanup();
        return 1;
    }

    printf("Modules registered and initialized successfully\n");

    // Create pipeline with registered modules using configuration
    printf("Creating pipeline with registered modules using configuration...\n");
    struct module_pipeline *pipeline = pipeline_create_with_standard_modules();
    if (!pipeline) {
        printf("ERROR: pipeline_create_with_standard_modules returned NULL\n");
        config_system_cleanup();
        return 1;
    }

    printf("Pipeline created with %d steps\n", pipeline->num_steps);

    // Verify that pipeline has exactly 2 steps (for our 2 enabled modules)
    if (pipeline->num_steps != 2) {
        printf("ERROR: Expected 2 pipeline steps, got %d\n", pipeline->num_steps);
        status = 1;
    } else {
        // Verify the steps contain our enabled modules (order doesn't matter)
        bool found_infall = false;
        bool found_cooling = false;
        bool found_disabled = false;

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

        if (!found_infall) {
            printf("ERROR: MockInfall module not found in pipeline\n");
            status = 1;
        }
        if (!found_cooling) {
            printf("ERROR: MockCooling module not found in pipeline\n");
            status = 1;
        }
        if (found_disabled) {
            printf("ERROR: MockDisabled module found in pipeline but should be disabled\n");
            status = 1;
        }
    }

    // Cleanup
    pipeline_destroy(pipeline);
    
    // Cleanup modules if needed
    printf("Cleaning up modules...\n");
    infall->cleanup(infall_data);
    cooling->cleanup(cooling_data);
    disabled->cleanup(disabled_data);
    
    // Clean up configuration system
    config_system_cleanup();
    
    if (status == 0) {
        printf("Test PASSED!\n");
    } else {
        printf("Test FAILED!\n");
    }
    printf("===== test_module_registration_and_pipeline_creation_with_config complete =====\n\n");
    return status;
}

// Main test runner
int run_all_pipeline_registry_tests(void) {
    int failures = 0;
    
    printf("\n===== Starting pipeline registry test suite =====\n");
    if (test_module_registration_and_pipeline_creation() != 0) {
        failures++;
        printf("ERROR: test_module_registration_and_pipeline_creation: FAILED\n");
    } else {
        printf("test_module_registration_and_pipeline_creation: PASSED\n");
    }
    
    if (test_module_registration_and_pipeline_creation_with_config() != 0) {
        failures++;
        printf("ERROR: test_module_registration_and_pipeline_creation_with_config: FAILED\n");
    } else {
        printf("test_module_registration_and_pipeline_creation_with_config: PASSED\n");
    }
    
    if (failures == 0) {
        printf("===== All pipeline registry tests PASSED =====\n\n");
    } else {
        printf("===== %d pipeline registry test(s) FAILED =====\n\n", failures);
    }
    
    return failures;
}

// Main function
int main(void) {
    printf("\n========== CORE PIPELINE REGISTRY TEST ==========\n");
    
    // Initialize logging but each test will handle its own module and pipeline systems
    struct params run_params = {0};
    initialize_logging(&run_params);
    logging_set_level(LOG_LEVEL_DEBUG); // Increase verbosity
    
    printf("Starting pipeline registry tests...\n");
    
    int failures = run_all_pipeline_registry_tests();
    
    if (failures == 0) {
        printf("\nFINAL RESULT: All pipeline registry tests PASSED\n");
    } else {
        printf("\nFINAL RESULT: %d pipeline registry test(s) FAILED\n", failures);
    }
    
    printf("Cleaning up pipeline system...\n");
    cleanup_pipeline_system();
    printf("Cleaning up module system...\n");
    cleanup_module_system();
    printf("Cleaning up logging...\n");
    cleanup_logging();
    
    printf("========== TEST COMPLETE ==========\n\n");
    
    return failures;
}
