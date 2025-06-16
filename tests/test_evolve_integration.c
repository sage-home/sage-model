/**
 * @file test_evolve_integration.c
 * @brief Integration test for the refactored evolve_galaxies loop
 * 
 * This test verifies that the pipeline phase system, event handling,
 * and diagnostics correctly work together within the context of the
 * refactored evolve_galaxies loop. It simulates the loop structure 
 * with mock modules that declare support for specific phases.
 * 
 * The key components tested include:
 * - Phase-based pipeline execution (HALO, GALAXY, POST, FINAL)
 * - Module execution based on declared phase support (via physics_step_executor)
 * - Evolution diagnostics for phase timing and metrics
 * - Event system integration (events emitted by mock modules)
 * 
 * This integration test is important to ensure these systems work correctly
 * together before beginning the migration of complex physics logic into
 * modules in Phase 5.2, thereby reducing debugging complexity.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* Mock module types for testing (compatible with core-physics separation) */
#define MOCK_TYPE_INFALL    201
#define MOCK_TYPE_COOLING   202  
#define MOCK_TYPE_MERGERS   203
#define MOCK_TYPE_MISC      204

/* Core system headers */
#include "../src/core/core_allvars.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_pipeline_system.h"
#include "../src/core/core_event_system.h"
#include "../src/core/core_evolution_diagnostics.h"
#include "../src/core/physics_pipeline_executor.h" // For physics_step_executor (used by pipeline_execute_phase)
#include "../src/core/core_property_utils.h"
#include "../src/core/core_properties.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* Module data structure for tracking execution */
struct mock_module_data {
    int module_id;               /* ID of the module this data belongs to */
    const char *name;            /* Name of the module for debugging */
    int halo_phase_executions;   /* Count of HALO phase executions */
    int galaxy_phase_executions; /* Count of GALAXY phase executions */
    int post_phase_executions;   /* Count of POST phase executions */
    int final_phase_executions;  /* Count of FINAL phase executions */
    int total_executions;        /* Total executions across all phases */
};

/* Forward declarations for mock module structs */
struct base_module mock_infall_module;
struct base_module mock_galaxy_module;
struct base_module mock_post_module;
struct base_module mock_final_module;
struct base_module mock_multi_phase_module;


/* Forward declarations for functions */
static void setup_mock_modules(struct params *p); /* Now takes params */
static void cleanup_mock_modules(void);                              /* Clean up mock modules */
static struct evolution_context *setup_mock_evolution_context(void); /* Create and init a mock evolution context */
static struct pipeline_context *setup_mock_pipeline_context(struct evolution_context *evo_ctx); /* Create and init a mock pipeline context */
static void cleanup_mock_evolution_context(struct evolution_context *ctx); /* Clean up evolution context resources */
static void cleanup_mock_pipeline_context(struct pipeline_context *ctx);   /* Clean up pipeline context resources */
// static void test_diagnostics_api_basic(void);                        /* Basic test for diagnostics API - can be removed if redundant */
static void test_full_pipeline_integration(void);                    /* Main integration test function */
static int test_phase_to_index(enum pipeline_execution_phase phase); /* Convert phase flag to array index */
static void update_module_execution_counters(void *module_data, enum pipeline_execution_phase phase); /* Update execution counters */
static void verify_phase_execution_counters(                         /* Verify module execution counts match expectations */
    struct mock_module_data *infall_data,
    struct mock_module_data *galaxy_data,
    struct mock_module_data *post_data,
    struct mock_module_data *final_data,
    struct mock_module_data *multi_phase_data,
    int num_mock_galaxies);
static void verify_diagnostics_results(struct core_evolution_diagnostics *diag, int num_mock_galaxies, int num_galaxy_phase_modules); /* Verify diagnostics */

/* Test event type and structure */
struct test_event_data {
    int galaxy_index;
};

/* Global storage for module data (for testing) */
void *g_mock_module_data[MAX_MODULES] = {0};

/* Generic Initialize mock module data - now takes module_id and name */
static int mock_module_initialize_generic(struct params *params, void **module_data, int assigned_module_id, const char* module_name) {
    (void)params; /* Unused parameter */
    
    struct mock_module_data *data = malloc(sizeof(struct mock_module_data));
    if (!data) return -1;
    
    memset(data, 0, sizeof(struct mock_module_data));
    data->module_id = assigned_module_id; 
    data->name = module_name;
    
    *module_data = data; // This sets the module_data pointer for the system
    // Store it also in our test global array, indexed by the true module ID
    if (assigned_module_id >= 0 && assigned_module_id < MAX_MODULES) {
        g_mock_module_data[assigned_module_id] = data;
    } else {
        printf("ERROR: Invalid assigned_module_id %d for module %s\n", assigned_module_id, module_name);
    }
    return 0;
}

/* Cleanup mock module data */
static int mock_module_cleanup(void *module_data) {
    if (module_data) {
        struct mock_module_data *data = (struct mock_module_data *)module_data;
        printf("Cleaning up module data for %s (ID %d)\n", data->name, data->module_id);
        // g_mock_module_data[data->module_id] should be set to NULL by the caller if needed
        free(module_data);
    }
    return 0;
}

/* Mock execution functions for different phases */
static int mock_halo_execute(void *module_data, struct pipeline_context *context) {
    if (module_data && context) {
        update_module_execution_counters(module_data, PIPELINE_PHASE_HALO);
    }
    return 0;
}

static int mock_galaxy_execute(void *module_data, struct pipeline_context *context) {
    if (module_data && context) {
        struct mock_module_data *data = (struct mock_module_data *)module_data;
        update_module_execution_counters(module_data, PIPELINE_PHASE_GALAXY);

        struct test_event_data event_data = { context->current_galaxy };
        event_status_t event_status = event_emit(EVENT_GALAXY_CREATED, data->module_id, 
                                                context->current_galaxy, context->step, 
                                                &event_data, sizeof(event_data), EVENT_FLAG_NONE);
        
        if (event_status == EVENT_STATUS_SUCCESS && context->user_data) {
            struct evolution_context *evo_ctx = (struct evolution_context *)context->user_data;
            if (evo_ctx && evo_ctx->diagnostics) { 
                core_evolution_diagnostics_add_event(evo_ctx->diagnostics, CORE_EVENT_GALAXY_CREATED);
            }
        }
    }
    return 0;
}

static int mock_post_execute(void *module_data, struct pipeline_context *context) {
    if (module_data && context) {
        update_module_execution_counters(module_data, PIPELINE_PHASE_POST);
    }
    return 0;
}

static int mock_final_execute(void *module_data, struct pipeline_context *context) {
    if (module_data && context) {
        update_module_execution_counters(module_data, PIPELINE_PHASE_FINAL);
    }
    return 0;
}

/* Specific initialize function for mock_infall_module */
static int mock_infall_module_init_actual(struct params *p, void **md) {
    return mock_module_initialize_generic(p, md, mock_infall_module.module_id, mock_infall_module.name);
}
/* Specific initialize function for mock_galaxy_module */
static int mock_galaxy_module_init_actual(struct params *p, void **md) {
    return mock_module_initialize_generic(p, md, mock_galaxy_module.module_id, mock_galaxy_module.name);
}
/* Specific initialize function for mock_post_module */
static int mock_post_module_init_actual(struct params *p, void **md) {
    return mock_module_initialize_generic(p, md, mock_post_module.module_id, mock_post_module.name);
}
/* Specific initialize function for mock_final_module */
static int mock_final_module_init_actual(struct params *p, void **md) {
    return mock_module_initialize_generic(p, md, mock_final_module.module_id, mock_final_module.name);
}
/* Specific initialize function for mock_multi_phase_module */
static int mock_multi_phase_module_init_actual(struct params *p, void **md) {
    return mock_module_initialize_generic(p, md, mock_multi_phase_module.module_id, mock_multi_phase_module.name);
}


/* Mock module for HALO phase only */
struct base_module mock_infall_module = {
    .name = "MockInfall", .version = "1.0.0", .author = "Test Author", .module_id = -1,
    .type = MOCK_TYPE_INFALL, .phases = PIPELINE_PHASE_HALO,
    .initialize = mock_infall_module_init_actual, 
    .cleanup = mock_module_cleanup,
    .execute_halo_phase = mock_halo_execute
};

/* Mock module for GALAXY phase only */
struct base_module mock_galaxy_module = {
    .name = "MockGalaxy", .version = "1.0.0", .author = "Test Author", .module_id = -1,
    .type = MOCK_TYPE_COOLING, .phases = PIPELINE_PHASE_GALAXY,
    .initialize = mock_galaxy_module_init_actual, 
    .cleanup = mock_module_cleanup,
    .execute_galaxy_phase = mock_galaxy_execute
};

/* Mock module for POST phase only */
struct base_module mock_post_module = {
    .name = "MockPost", .version = "1.0.0", .author = "Test Author", .module_id = -1,
    .type = MOCK_TYPE_MERGERS, .phases = PIPELINE_PHASE_POST,
    .initialize = mock_post_module_init_actual, 
    .cleanup = mock_module_cleanup,
    .execute_post_phase = mock_post_execute
};

/* Mock module for FINAL phase only */
struct base_module mock_final_module = {
    .name = "MockFinal", .version = "1.0.0", .author = "Test Author", .module_id = -1,
    .type = MOCK_TYPE_MISC, .phases = PIPELINE_PHASE_FINAL,
    .initialize = mock_final_module_init_actual, 
    .cleanup = mock_module_cleanup,
    .execute_final_phase = mock_final_execute
};

/* Mock module for multiple phases */
struct base_module mock_multi_phase_module = {
    .name = "MockMultiPhase", .version = "1.0.0", .author = "Test Author", .module_id = -1,
    .type = MOCK_TYPE_MISC, .phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_GALAXY,
    .initialize = mock_multi_phase_module_init_actual, 
    .cleanup = mock_module_cleanup,
    .execute_halo_phase = mock_halo_execute, .execute_galaxy_phase = mock_galaxy_execute
};

/**
 * Create mock galaxies for testing
 */
static struct GALAXY *create_mock_galaxies(int num_galaxies) {
    struct GALAXY *galaxies = calloc(num_galaxies, sizeof(struct GALAXY));
    if (!galaxies) {
        printf("ERROR: Failed to allocate mock galaxies\n");
        exit(1);
    }
    
    struct params test_params = {0};
    test_params.simulation.NumSnapOutputs = 1; 
    
    for (int i = 0; i < num_galaxies; i++) {
        if (allocate_galaxy_properties(&galaxies[i], &test_params) != 0) {
            printf("ERROR: Failed to allocate properties for galaxy %d\n", i);
            for (int j = 0; j < i; j++) free_galaxy_properties(&galaxies[j]);
            free(galaxies);
            exit(1);
        }
        initialize_all_properties(&galaxies[i]);
        GALAXY_PROP_Type(&galaxies[i]) = (i == 0) ? 0 : 1;
        GALAXY_PROP_CentralGal(&galaxies[i]) = 0;
        GALAXY_PROP_HaloNr(&galaxies[i]) = 1; 
        GALAXY_PROP_GalaxyNr(&galaxies[i]) = i;
        printf("Initialized mock galaxy %d with allocated properties\n", i);
    }
    return galaxies;
}

/**
 * Set up a mock evolution context
 */
static struct evolution_context *setup_mock_evolution_context(void) {
    struct evolution_context *ctx = malloc(sizeof(struct evolution_context));
    if (!ctx) { printf("ERROR: Failed to allocate mock evolution context\n"); exit(1); }
    
    ctx->halo_nr = 1; ctx->halo_snapnum = 63; ctx->redshift = 0.0; ctx->halo_age = 13.8;
    int num_galaxies = 3;
    ctx->galaxies = create_mock_galaxies(num_galaxies);
    ctx->ngal = num_galaxies; ctx->centralgal = 0;
    ctx->deltaT = 0.1; ctx->time = 13.7;
    
    struct params *params = malloc(sizeof(struct params));
    if (!params) { printf("ERROR: Failed to allocate mock params\n"); free(ctx->galaxies); free(ctx); exit(1); }
    memset(params, 0, sizeof(struct params));
    params->simulation.NumSnapOutputs = 1; 
    ctx->params = params;
    
    struct merger_event_queue *queue = malloc(sizeof(struct merger_event_queue));
    if (!queue) { printf("ERROR: Failed to allocate mock merger queue\n"); free((void*)ctx->params); free(ctx->galaxies); free(ctx); exit(1); }
    memset(queue, 0, sizeof(struct merger_event_queue));
    ctx->merger_queue = queue;
    
    ctx->current_phase = 0; ctx->current_galaxy = -1; ctx->callback_context = NULL;
    ctx->diagnostics = NULL;
    return ctx;
}

/**
 * Set up a mock pipeline context, linking to an evolution_context
 */
static struct pipeline_context *setup_mock_pipeline_context(struct evolution_context *evo_ctx) {
    struct pipeline_context *ctx = malloc(sizeof(struct pipeline_context));
    if (!ctx) { printf("ERROR: Failed to allocate mock pipeline context\n"); exit(1); }
    memset(ctx, 0, sizeof(struct pipeline_context));

    pipeline_context_init(
        ctx,
        (struct params*)evo_ctx->params, 
        evo_ctx->galaxies,
        evo_ctx->ngal,
        evo_ctx->centralgal,
        evo_ctx->time,
        evo_ctx->deltaT,
        evo_ctx->halo_nr,
        0, 
        evo_ctx 
    );
    ctx->redshift = evo_ctx->redshift;
    return ctx;
}

/**
 * Clean up mock evolution context resources
 */
static void cleanup_mock_evolution_context(struct evolution_context *ctx) {
    if (ctx) {
        if (ctx->galaxies) {
            for(int i=0; i < ctx->ngal; ++i) free_galaxy_properties(&ctx->galaxies[i]);
            free(ctx->galaxies);
        }
        if (ctx->params) free((void*)ctx->params);
        if (ctx->merger_queue) free(ctx->merger_queue);
        free(ctx);
    }
}

/**
 * Clean up mock pipeline context resources
 */
static void cleanup_mock_pipeline_context(struct pipeline_context *ctx) {
    if (ctx) {
        ctx->galaxies = NULL; 
        ctx->params = NULL;
        free(ctx);
    }
}

/**
 * Helper function to convert phase flag to array index (from core_evolution_diagnostics.c)
 */
static int test_phase_to_index(enum pipeline_execution_phase phase) {
    switch (phase) {
        case PIPELINE_PHASE_HALO:   return 0;
        case PIPELINE_PHASE_GALAXY: return 1;
        case PIPELINE_PHASE_POST:   return 2;
        case PIPELINE_PHASE_FINAL:  return 3;
        case PIPELINE_PHASE_NONE:
        default:                    return -1;
    }
}

/**
 * Update module execution counters
 */
static void update_module_execution_counters(void *module_data, enum pipeline_execution_phase phase) {
    if (!module_data) return;
    struct mock_module_data *data = (struct mock_module_data *)module_data;
    switch (phase) {
        case PIPELINE_PHASE_HALO:   data->halo_phase_executions++;   break;
        case PIPELINE_PHASE_GALAXY: data->galaxy_phase_executions++; break;
        case PIPELINE_PHASE_POST:   data->post_phase_executions++;   break;
        case PIPELINE_PHASE_FINAL:  data->final_phase_executions++;  break;
        case PIPELINE_PHASE_NONE: default: break;
    }
    data->total_executions++;
}

/**
 * Setup and register all mock modules with the module system
 */
static void setup_mock_modules(struct params *p) { 
    printf("Setting up mock modules\n");

#define REGISTER_AND_INIT_MOCK(module_struct_ptr) \
    do { \
        int returned_status_from_register = module_register((module_struct_ptr)); \
        if (returned_status_from_register == MODULE_STATUS_SUCCESS) { \
            int assigned_id = (module_struct_ptr)->module_id; \
            if (assigned_id >= 0 && assigned_id < MAX_MODULES) { \
                int init_status = module_initialize(assigned_id, p); \
                if (init_status == MODULE_STATUS_SUCCESS) { \
                    printf("Registered and initialized %s with ID %d\n", (module_struct_ptr)->name, assigned_id); \
                    module_set_active(assigned_id); \
                } else { \
                    printf("ERROR: System module_initialize for %s (ID %d) failed with status %d\n", (module_struct_ptr)->name, assigned_id, init_status); \
                } \
            } else { \
                 printf("ERROR: Module registration for %s succeeded, but assigned_id %d is invalid. Skipping initialization.\n", (module_struct_ptr)->name, assigned_id); \
            } \
        } else { \
            printf("ERROR: Module registration for %s failed with status %d.\n", (module_struct_ptr)->name, returned_status_from_register); \
        } \
    } while(0)

    REGISTER_AND_INIT_MOCK(&mock_infall_module);
    REGISTER_AND_INIT_MOCK(&mock_galaxy_module);
    REGISTER_AND_INIT_MOCK(&mock_post_module);
    REGISTER_AND_INIT_MOCK(&mock_final_module);
    REGISTER_AND_INIT_MOCK(&mock_multi_phase_module);

    printf("All mock modules registered and activated\n");
}

/**
 * Clean up all mock modules
 */
static void cleanup_mock_modules(void) {
#define CLEANUP_MOCK(module_struct) \
    do { \
        if (module_struct.module_id >= 0 && module_struct.module_id < MAX_MODULES) { \
            module_cleanup(module_struct.module_id); \
            g_mock_module_data[module_struct.module_id] = NULL;  \
        } \
    } while(0)

    CLEANUP_MOCK(mock_infall_module);
    CLEANUP_MOCK(mock_galaxy_module);
    CLEANUP_MOCK(mock_post_module);    CLEANUP_MOCK(mock_final_module);
    CLEANUP_MOCK(mock_multi_phase_module);

    printf("All mock modules cleaned up\n");
}

/**
 * Verify that modules were executed in the correct phases and the correct number of times
 */
static void verify_phase_execution_counters(
    struct mock_module_data *infall_data,
    struct mock_module_data *galaxy_data,
    struct mock_module_data *post_data,
    struct mock_module_data *final_data,
    struct mock_module_data *multi_phase_data,
    int num_mock_galaxies
) {
    TEST_ASSERT(infall_data->halo_phase_executions == 1, 
                "Infall module should execute once in HALO phase");
    TEST_ASSERT(infall_data->galaxy_phase_executions == 0, 
                "Infall module should not execute in GALAXY phase");
    TEST_ASSERT(infall_data->total_executions == 1, 
                "Infall module total executions should be 1");

    TEST_ASSERT(galaxy_data->galaxy_phase_executions == num_mock_galaxies, 
                "Galaxy module should execute once per galaxy in GALAXY phase");
    TEST_ASSERT(galaxy_data->halo_phase_executions == 0, 
                "Galaxy module should not execute in HALO phase");
    TEST_ASSERT(galaxy_data->total_executions == num_mock_galaxies, 
                "Galaxy module total executions should match galaxy count");

    TEST_ASSERT(post_data->post_phase_executions == 1, 
                "Post module should execute once in POST phase");
    TEST_ASSERT(post_data->total_executions == 1, 
                "Post module total executions should be 1");

    TEST_ASSERT(final_data->final_phase_executions == 1, 
                "Final module should execute once in FINAL phase");
    TEST_ASSERT(final_data->total_executions == 1, 
                "Final module total executions should be 1");

    TEST_ASSERT(multi_phase_data->halo_phase_executions == 1, 
                "Multi-phase module should execute once in HALO phase");
    TEST_ASSERT(multi_phase_data->galaxy_phase_executions == num_mock_galaxies, 
                "Multi-phase module should execute once per galaxy in GALAXY phase");    TEST_ASSERT(multi_phase_data->total_executions == (1 + num_mock_galaxies),
                "Multi-phase module total executions should be 1 + galaxy count");

    printf("Module execution phase verification: PASSED\n");
}

/**
 * Verify diagnostics results including timing and event counts
 */
static void verify_diagnostics_results(
    struct core_evolution_diagnostics *diag, 
    int num_mock_galaxies,
    int num_galaxy_phase_modules 
) {
    TEST_ASSERT(diag->elapsed_seconds >= 0.0, 
                "Elapsed time should be non-negative");
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_HALO)].total_time >= 0,
                "HALO phase timing should be non-negative");
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_GALAXY)].total_time >= 0,
                "GALAXY phase timing should be non-negative");
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_POST)].total_time >= 0,
                "POST phase timing should be non-negative");
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_FINAL)].total_time >= 0,
                "FINAL phase timing should be non-negative");
    
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_HALO)].step_count == 1,
                "HALO phase should have 1 step");
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_GALAXY)].step_count == 1,
                "GALAXY phase should have 1 step");
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_POST)].step_count == 1,
                "POST phase should have 1 step");
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_FINAL)].step_count == 1,
                "FINAL phase should have 1 step");
    
    TEST_ASSERT(diag->phases[test_phase_to_index(PIPELINE_PHASE_GALAXY)].galaxy_count == num_mock_galaxies,
                "GALAXY phase should process correct number of galaxies");
    
    int expected_galaxy_created_events = num_mock_galaxies * num_galaxy_phase_modules;
    TEST_ASSERT(diag->core_event_counts[CORE_EVENT_GALAXY_CREATED] == expected_galaxy_created_events,
                "Should have correct number of GALAXY_CREATED events");
    
    printf("Diagnostics verification: PASSED\n");
}

/**
 * Test the basic diagnostics API functionality (can be removed if redundant)
 */
/*
static void test_diagnostics_api_basic(void) {
    // ... (content as before, can be removed if test_full_pipeline_integration is sufficient)
}
*/

/**
 * Test actual integration of pipeline, modules, and diagnostics working together
 */
static void test_full_pipeline_integration(void) {
    printf("Starting full pipeline integration test\n");
    int status;

    struct module_pipeline *pipeline = pipeline_create("test_integration_pipeline");
    assert(pipeline != NULL);

#define ADD_MOCK_STEP(module_struct, step_name_suffix) \
    status = pipeline_add_step(pipeline, module_struct.type, module_struct.name, \
                               #module_struct "_" step_name_suffix, true, false); \
    assert(status == 0)
    
    ADD_MOCK_STEP(mock_infall_module, "step");
    ADD_MOCK_STEP(mock_galaxy_module, "step");
    ADD_MOCK_STEP(mock_post_module, "step");
    ADD_MOCK_STEP(mock_final_module, "step");
    ADD_MOCK_STEP(mock_multi_phase_module, "step");
    printf("Pipeline populated with all mock modules for integration test\n");

    struct mock_module_data *infall_data = g_mock_module_data[mock_infall_module.module_id];
    struct mock_module_data *galaxy_data = g_mock_module_data[mock_galaxy_module.module_id];
    struct mock_module_data *post_data = g_mock_module_data[mock_post_module.module_id];
    struct mock_module_data *final_data = g_mock_module_data[mock_final_module.module_id];
    struct mock_module_data *multi_phase_data = g_mock_module_data[mock_multi_phase_module.module_id];
    
    TEST_ASSERT(infall_data && galaxy_data && post_data && final_data && multi_phase_data,
                "All mock module data structures should be available");
    printf("Mock module data structures retrieved for integration test\n");
    
    struct evolution_context *evo_ctx = setup_mock_evolution_context();
    struct pipeline_context *pipe_ctx = setup_mock_pipeline_context(evo_ctx); 

    struct core_evolution_diagnostics diag;
    core_evolution_diagnostics_initialize(&diag, evo_ctx->halo_nr, evo_ctx->ngal);
    evo_ctx->diagnostics = &diag; 

    printf("Testing HALO phase pipeline execution\n");
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_HALO);
    status = pipeline_execute_phase(pipeline, pipe_ctx, PIPELINE_PHASE_HALO);
    assert(status == 0);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_HALO);

    printf("Testing GALAXY phase pipeline execution\n");
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_GALAXY);
    for (int i = 0; i < pipe_ctx->ngal; i++) {
        pipe_ctx->current_galaxy = i;
        diag.phases[test_phase_to_index(PIPELINE_PHASE_GALAXY)].galaxy_count++; 
        status = pipeline_execute_phase(pipeline, pipe_ctx, PIPELINE_PHASE_GALAXY);
        assert(status == 0);
    }
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_GALAXY);

    printf("Testing POST phase pipeline execution\n");
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_POST);
    status = pipeline_execute_phase(pipeline, pipe_ctx, PIPELINE_PHASE_POST);
    assert(status == 0);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_POST);

    printf("Testing FINAL phase pipeline execution\n");
    core_evolution_diagnostics_start_phase(&diag, PIPELINE_PHASE_FINAL);
    status = pipeline_execute_phase(pipeline, pipe_ctx, PIPELINE_PHASE_FINAL);
    assert(status == 0);
    core_evolution_diagnostics_end_phase(&diag, PIPELINE_PHASE_FINAL);

    verify_phase_execution_counters(
        infall_data, galaxy_data, post_data, final_data, multi_phase_data,
        pipe_ctx->ngal
    );

    core_evolution_diagnostics_finalize(&diag);
    verify_diagnostics_results(&diag, pipe_ctx->ngal, 2); 
    printf("Diagnostics verification: PASSED\n");

    cleanup_mock_pipeline_context(pipe_ctx); 
    cleanup_mock_evolution_context(evo_ctx); 
    pipeline_destroy(pipeline);
    
    printf("Full pipeline integration test completed successfully\n");
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv; 
    struct params p = {0}; // Minimal params for module_initialize

    printf("\n========================================\n");
    printf("Starting tests for test_evolve_integration\n");
    printf("========================================\n\n");
    
    // Initialize systems
    module_system_initialize(); 
    event_system_initialize(); 
    pipeline_system_initialize(); 
    
    setup_mock_modules(&p);
    test_full_pipeline_integration();
    cleanup_mock_modules();
    
    pipeline_system_cleanup(); 
    event_system_cleanup(); 
    module_system_cleanup();
    
    // Report results in standard format
    printf("\n========================================\n");
    printf("Test results for test_evolve_integration:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}