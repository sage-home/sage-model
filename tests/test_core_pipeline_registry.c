#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/core_allvars.h" // Added for struct params
#include "../src/core/core_pipeline_registry.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_init.h"
#include "../src/core/core_logging.h" // Ensure this is included
#include "test_core_pipeline_registry.h"

#include "core_module_system.h" // For base_module and module_find_by_name, module_get
#include "core_pipeline_system.h" // For pipeline_context

// --- Mock Modules ---
// Mock Infall Module
static int mock_initialize(struct params *p, void **md) {
    LOG_DEBUG("Mock initialize called for %p", md ? *md : NULL);
    // Simulate successful initialization, potentially allocating mock_data if needed by tests
    if (md && *md == NULL) {
        *md = malloc(sizeof(int)); // Minimal allocation
        if (*md == NULL) return MODULE_STATUS_OUT_OF_MEMORY;
        *(int*)(*md) = 123; // Dummy data
    }
    return MODULE_STATUS_SUCCESS;
}

static int mock_cleanup(void *md) {
    LOG_DEBUG("Mock cleanup called for %p", md);
    if (md) {
        free(md); // Free allocated data
    }
    return MODULE_STATUS_SUCCESS;
}

static int mock_execute_physics(void *md, struct pipeline_context *ctx) {
    LOG_DEBUG("Mock execute_physics called for %p with context %p", md, ctx);
    // Simulate successful execution
    return MODULE_STATUS_SUCCESS;
}

static struct base_module mock_infall_module_instance = {
    .name = "MockInfall",
    .version = "1.0",
    .author = "TestSystem",
    .module_id = 0, 
    .type = MODULE_TYPE_INFALL,
    .initialize = mock_initialize, .cleanup = mock_cleanup, .configure = NULL,
    .execute_halo_phase = NULL, .execute_galaxy_phase = NULL, 
    .execute_post_phase = NULL, .execute_final_phase = NULL,
    .execute_physics = mock_execute_physics, .post_process = NULL, .process_halo = NULL, .finalize = NULL,
    .error_context = NULL, .debug_context = NULL, .manifest = NULL,
    .function_registry = NULL, .dependencies = NULL, .num_dependencies = 0, .phases = 0 // Set to 0 as placeholder, assuming this means it can run in any phase or default
};

struct base_module* mock_infall_module_factory(void) {
    LOG_DEBUG("MockInfall factory called.");
    // In a real scenario, module_register would have been called, setting the ID.
    // For this test, we pre-assign a test ID if module_register isn't called directly.
    // However, module_register IS called by the helper register_mock_module.
    return &mock_infall_module_instance;
}

// Mock Cooling Module
static struct base_module mock_cooling_module_instance = {
    .name = "MockCooling",
    .version = "1.0",
    .author = "TestSystem",
    .module_id = 1, 
    .type = MODULE_TYPE_COOLING,
    .initialize = mock_initialize, .cleanup = mock_cleanup, .configure = NULL,
    .execute_halo_phase = NULL, .execute_galaxy_phase = NULL, 
    .execute_post_phase = NULL, .execute_final_phase = NULL,
    .execute_physics = mock_execute_physics, .post_process = NULL, .process_halo = NULL, .finalize = NULL,
    .error_context = NULL, .debug_context = NULL, .manifest = NULL,
    .function_registry = NULL, .dependencies = NULL, .num_dependencies = 0, .phases = 0 // Set to 0 as placeholder
};

struct base_module* mock_cooling_module_factory(void) {
    LOG_DEBUG("MockCooling factory called.");
    return &mock_cooling_module_instance;
}

// Helper function to register a mock module
static int register_mock_module(struct base_module *module_instance, const char* name) {
    LOG_DEBUG("Attempting to register mock module: %s", name);

    // Check if module is already registered by name
    int existing_module_id = module_find_by_name(name);
    if (existing_module_id >= 0) {
        struct base_module *existing_module_ptr = NULL;
        void *existing_module_data_ptr = NULL;
        if (module_get(existing_module_id, &existing_module_ptr, &existing_module_data_ptr) == MODULE_STATUS_SUCCESS) {
            LOG_DEBUG("Mock module '%s' already registered with ID %d. Skipping registration.", name, existing_module_id);
            // Ensure the test uses the already registered module's ID.
            // The factory function in a real scenario would return this existing module or its factory.
            // For this test, we assume the factory would handle this.
            // We just need to ensure our test logic can proceed.
            // If the test requires the specific instance passed to this function to be THE registered one,
            // this logic might need adjustment or the test needs to unregister first.
            // For now, we'll assume this is fine and the test will use the name to fetch it.
            return existing_module_id; // Return existing ID
        } else {
            LOG_ERROR("Failed to retrieve already registered module '%s' (ID: %d). This is unexpected.", name, existing_module_id);
            // This case is problematic, implies an issue with module_get or internal state.
            return MODULE_STATUS_ERROR;
        }
    }

    // If not already registered, proceed with registration
    strncpy(module_instance->name, name, MAX_MODULE_NAME - 1);
    module_instance->name[MAX_MODULE_NAME - 1] = '\0';
    module_instance->type = MODULE_TYPE_MISC; // Use MISC as a generic type
    module_instance->phases = 0; // Example phases, adjust if specific phases are needed for tests

    // Assign the corrected dummy functions
    module_instance->initialize = mock_initialize;
    module_instance->cleanup = mock_cleanup;
    module_instance->execute_physics = mock_execute_physics;
    // Ensure other relevant function pointers are NULL or valid dummies if accessed by module_register or subsequent calls
    module_instance->configure = NULL;
    module_instance->execute_halo_phase = NULL;
    module_instance->execute_galaxy_phase = NULL;
    module_instance->execute_post_phase = NULL;
    module_instance->execute_final_phase = NULL;
    module_instance->post_process = NULL;
    module_instance->process_halo = NULL;
    module_instance->finalize = NULL;


    if (!module_validate(module_instance)) {
        LOG_ERROR("Mock module %s is invalid before registration.", name);
        return MODULE_STATUS_ERROR; // Or an appropriate error code
    }

    int status = module_register(module_instance);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register mock module '%s' with module system. Error: %d", name, status);
        // Try to get more details if possible, e.g., from a global error state if the module system provides one.
        return status;
    }
    LOG_DEBUG("Successfully registered mock module: %s with ID %d", name, module_instance->module_id);
    return module_instance->module_id; // Return the new module's ID
}

// --- Test Function ---
static int test_module_registration_and_pipeline_creation(void) { 
    LOG_INFO("Starting test_module_registration_and_pipeline_creation...");
    int status = 0; // Success

    // Register the module factories directly with the pipeline system
    if (pipeline_register_module_factory(MODULE_TYPE_COOLING, "MockCooling", mock_cooling_module_factory) < 0) {
        LOG_ERROR("Failed to register MockCooling factory");
        status = 1;
    }
    if (pipeline_register_module_factory(MODULE_TYPE_INFALL, "MockInfall", mock_infall_module_factory) < 0) {
        LOG_ERROR("Failed to register MockInfall factory");
        status = 1;
    }
    
    if (status == 1) {
        LOG_ERROR("One or more module factories failed to register.");
        return 1; // Early exit if registration failed
    }

    // Also register the module instances with the module system
    module_register(&mock_infall_module_instance);
    module_register(&mock_cooling_module_instance);
    
    LOG_INFO("All mock modules and factories registered. Creating pipeline...");
    struct module_pipeline *pipeline = pipeline_create_with_standard_modules();

    if (!pipeline) {
        LOG_ERROR("pipeline_create_with_standard_modules returned NULL.");
        status = 1;
    } else {
        LOG_INFO("Pipeline created. Number of steps: %d", pipeline->num_steps);
        // Expected: 2 steps, one for infall, one for cooling
        if (pipeline->num_steps != 2) {
            LOG_ERROR("Expected 2 pipeline steps, got %d.", pipeline->num_steps);
            status = 1;
        } else {
            // Check step 0 (Order might depend on registration order or internal sorting)
            // For now, assume registration order = pipeline order.
            struct pipeline_step* step0 = &pipeline->steps[0];
            struct pipeline_step* step1 = &pipeline->steps[1];

            // Tolerate either order due to potential hash table effects in registry or future changes
            bool found_infall = false;
            bool found_cooling = false;

            if (step0->type == MODULE_TYPE_INFALL && strcmp(step0->module_name, "MockInfall") == 0) {
                found_infall = true;
            } else if (step0->type == MODULE_TYPE_COOLING && strcmp(step0->module_name, "MockCooling") == 0) {
                found_cooling = true;
            }
            if (step1->type == MODULE_TYPE_INFALL && strcmp(step1->module_name, "MockInfall") == 0) {
                found_infall = true;
            } else if (step1->type == MODULE_TYPE_COOLING && strcmp(step1->module_name, "MockCooling") == 0) {
                found_cooling = true;
            }

            if (!found_infall) {
                LOG_ERROR("MockInfall module not found or mismatched in pipeline steps.");
                status = 1;
            } else {
                LOG_INFO("MockInfall module correctly found in pipeline.");
            }
            if (!found_cooling) {
                LOG_ERROR("MockCooling module not found or mismatched in pipeline steps.");
                status = 1;
            } else {
                LOG_INFO("MockCooling module correctly found in pipeline.");
            }
        }
        pipeline_destroy(pipeline);
        LOG_INFO("Pipeline destroyed.");
    }
    
    if (status == 0) {
        LOG_INFO("test_module_registration_and_pipeline_creation: PASSED");
    } else {
        LOG_ERROR("test_module_registration_and_pipeline_creation: FAILED");
    }
    return status;
}

// Main test runner function
int run_all_pipeline_registry_tests(void) {
    LOG_INFO("Running All Pipeline Registry Tests...");
    int failures = 0;
    // struct params p = {0}; // No longer needed here

    // Core systems are now initialized and cleaned up by main.
    // initialize_module_system(&p); 
    // initialize_pipeline_system(); 

    if (test_module_registration_and_pipeline_creation() != 0) {
        failures++;
        LOG_ERROR("test_module_registration_and_pipeline_creation: FAILED");
    } else {
        LOG_INFO("test_module_registration_and_pipeline_creation: PASSED");
    }

    // Core systems are now initialized and cleaned up by main.
    // cleanup_pipeline_system(); 
    // cleanup_module_system();   

    return failures;
}

// Main function to run the test executable
int main(void) {
    // Initialize a dummy params structure
    struct params run_params = {0}; // Initialize with zeros

    // Initialize necessary systems.
    initialize_logging(&run_params); // Initialize logging system
    logging_set_level(LOG_LEVEL_DEBUG); // Enable debug logging

    cleanup_module_system(); // Clean up any existing module system first
    initialize_module_system(&run_params); 
    cleanup_pipeline_system(); // Clean up any existing pipeline system first
    initialize_pipeline_system();      

    LOG_INFO("Starting pipeline registry tests from main...");

    int failures = run_all_pipeline_registry_tests();

    if (failures == 0) {
        LOG_INFO("All pipeline registry tests PASSED.");
    } else {
        LOG_ERROR("%d pipeline registry test(s) FAILED.", failures);
    }

    // Cleanup systems
    cleanup_pipeline_system();
    cleanup_module_system();
    cleanup_logging(); // Cleanup logging system

    return failures;
}

