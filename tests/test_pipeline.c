/**
 * Test suite for Pipeline Phase System
 * 
 * Tests cover:
 * - Basic functionality (4-phase pipeline system: HALO, GALAXY, POST, FINAL)
 * - Error handling (invalid parameters, module failures)
 * - Edge cases (empty pipelines, invalid phase values)
 * - Integration points (module phase support)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

/* Mock definitions to avoid external dependencies */
#define MAX_MODULE_NAME 64
#define MAX_STEP_NAME 64
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

/* Test counter for reporting */
static int tests_run = 0;
static int tests_passed = 0;

/* Helper macro for test assertions */
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* Module type identifiers */
enum module_type {
    MODULE_TYPE_UNKNOWN = 0,
    MODULE_TYPE_COOLING = 1,
    MODULE_TYPE_STAR_FORMATION = 2,
    MODULE_TYPE_FEEDBACK = 3,
    MODULE_TYPE_AGN = 4,
    MODULE_TYPE_MERGERS = 5,
    MODULE_TYPE_DISK_INSTABILITY = 6,
    MODULE_TYPE_REINCORPORATION = 7,
    MODULE_TYPE_INFALL = 8,
    MODULE_TYPE_MISC = 9,
    MODULE_TYPE_MAX
};

/* Pipeline execution phases */
enum pipeline_execution_phase {
    PIPELINE_PHASE_HALO = 1,    /* Execute once per halo (outside galaxy loop) */
    PIPELINE_PHASE_GALAXY = 2,  /* Execute for each galaxy (inside galaxy loop) */
    PIPELINE_PHASE_POST = 4,    /* Execute after processing all galaxies (for each integration step) */
    PIPELINE_PHASE_FINAL = 8    /* Execute after all steps are complete, before exiting evolve_galaxies */
};

/* Base module structure */
struct base_module {
    char name[MAX_MODULE_NAME];           /* Module name */
    char version[MAX_MODULE_NAME];        /* Module version */
    char author[MAX_MODULE_NAME];         /* Module author */
    int module_id;                        /* Module ID */
    enum module_type type;                /* Module type */
    uint32_t phases;                      /* Supported execution phases */
};

/* Pipeline step structure */
struct pipeline_step {
    enum module_type type;               /* Type of module to execute */
    char module_name[MAX_MODULE_NAME];   /* Optional specific module name (empty for any) */
    char step_name[MAX_STEP_NAME];       /* Optional name for this step (for logging/config) */
    bool enabled;                        /* Whether this step is enabled */
    bool optional;                       /* Whether this step is optional (pipeline continues if missing) */
};

/* Pipeline execution context */
struct pipeline_context {
    void *params;                        /* Parameters */
    void *galaxies;                      /* Galaxy array */
    int ngal;                            /* Number of galaxies */
    int centralgal;                      /* Central galaxy index */
    double time;                         /* Current time */
    double dt;                           /* Time step */
    int halonr;                          /* Halo number */
    int step;                            /* Current step */
    void *user_data;                     /* User data */
    int current_galaxy;                  /* Current galaxy index */
    double infall_gas;                   /* Infall gas amount */
    double redshift;                     /* Current redshift */
    enum pipeline_execution_phase execution_phase; /* Current execution phase */
};

/* Pipeline structure */
struct module_pipeline {
    struct pipeline_step steps[32];      /* Pipeline steps */
    int num_steps;                       /* Number of steps */
    char name[MAX_MODULE_NAME];          /* Pipeline name */
    bool initialized;                    /* Initialization flag */
    int current_step_index;              /* Current step index */
};

/* Simple test modules for phase tests */
struct test_module_data {
    int counter;
    bool should_fail;  /* For error testing */
};

/* Dummy modules for testing phases */
struct base_module infall_module = {
    .name = "TestInfall",
    .version = "1.0.0",
    .author = "Test Author",
    .module_id = 1,
    .type = MODULE_TYPE_INFALL,
    .phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_GALAXY
};

struct base_module cooling_module = {
    .name = "TestCooling",
    .version = "1.0.0",
    .author = "Test Author",
    .module_id = 2,
    .type = MODULE_TYPE_COOLING,
    .phases = PIPELINE_PHASE_GALAXY
};

struct base_module mergers_module = {
    .name = "TestMergers",
    .version = "1.0.0",
    .author = "Test Author",
    .module_id = 3,
    .type = MODULE_TYPE_MERGERS,
    .phases = PIPELINE_PHASE_POST
};

struct base_module misc_module = {
    .name = "TestMisc",
    .version = "1.0.0",
    .author = "Test Author",
    .module_id = 4,
    .type = MODULE_TYPE_MISC,
    .phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_GALAXY | PIPELINE_PHASE_POST | PIPELINE_PHASE_FINAL
};

struct test_module_data infall_data = { 0, false };
struct test_module_data cooling_data = { 0, false };
struct test_module_data mergers_data = { 0, false };
struct test_module_data misc_data = { 0, false };

/* Mock function for module_type_name */
const char *module_type_name(enum module_type type) {
    static const char *type_names[] = {
        "unknown",
        "cooling",
        "star_formation",
        "feedback",
        "agn",
        "mergers",
        "disk_instability",
        "reincorporation",
        "infall",
        "misc"
    };
    
    if (type >= 0 && type < MODULE_TYPE_MAX) {
        return type_names[type];
    }
    
    return "invalid";
}

/* Mock function to create a pipeline */
struct module_pipeline *pipeline_create(const char *name) {
    struct module_pipeline *pipeline = calloc(1, sizeof(struct module_pipeline));
    if (pipeline == NULL) {
        LOG_ERROR("Failed to allocate memory for pipeline");
        return NULL;
    }
    
    if (name != NULL) {
        strncpy(pipeline->name, name, MAX_MODULE_NAME - 1);
        pipeline->name[MAX_MODULE_NAME - 1] = '\0';
    } else {
        strcpy(pipeline->name, "unnamed_pipeline");
    }
    
    pipeline->num_steps = 0;
    pipeline->initialized = true;
    pipeline->current_step_index = -1;
    
    LOG_DEBUG("Created new pipeline '%s'", pipeline->name);
    return pipeline;
}

/* Mock function to destroy a pipeline */
void pipeline_destroy(struct module_pipeline *pipeline) {
    if (pipeline == NULL) {
        return;
    }
    
    LOG_DEBUG("Destroying pipeline '%s'", pipeline->name);
    free(pipeline);
}

/* Mock function to add a step to a pipeline */
int pipeline_add_step(
    struct module_pipeline *pipeline,
    enum module_type type,
    const char *module_name,
    const char *step_name,
    bool enabled,
    bool optional
) {
    if (pipeline == NULL) {
        LOG_ERROR("Pipeline is NULL");
        return -1;
    }
    
    if (pipeline->num_steps >= 32) {
        LOG_ERROR("Pipeline '%s' already has maximum number of steps", pipeline->name);
        return -1;
    }
    
    struct pipeline_step *step = &pipeline->steps[pipeline->num_steps];
    memset(step, 0, sizeof(struct pipeline_step));
    
    step->type = type;
    step->enabled = enabled;
    step->optional = optional;
    
    if (module_name != NULL) {
        strncpy(step->module_name, module_name, MAX_MODULE_NAME - 1);
        step->module_name[MAX_MODULE_NAME - 1] = '\0';
    }
    
    if (step_name != NULL) {
        strncpy(step->step_name, step_name, MAX_STEP_NAME - 1);
        step->step_name[MAX_STEP_NAME - 1] = '\0';
    } else {
        /* Generate default step name if none provided */
        snprintf(step->step_name, MAX_STEP_NAME, "%s_%d", 
                 module_type_name(type), pipeline->num_steps);
    }
    
    pipeline->num_steps++;
    
    LOG_DEBUG("Added step '%s' (type %s, module '%s') to pipeline '%s'",
             step->step_name, module_type_name(type), 
             module_name ? module_name : "any", pipeline->name);
    
    return 0;
}

/* Custom step execution function for testing */
static int test_execute_step(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
) {
    /* Silence unused parameter warnings if module is NULL */
    if (module == NULL) {
        printf("Executing step: '%s' (type: %s) [No module]\n", 
               step->step_name, 
               module_type_name(step->type));
        return 0;  /* Success */
    }
    
    struct test_module_data *data = (struct test_module_data *)module_data;
    
    /* Check if this module should fail (for error testing) */
    if (data->should_fail) {
        printf("SIMULATED FAILURE in step: '%s' (type: %s, module: '%s')\n", 
               step->step_name, 
               module_type_name(step->type),
               module->name);
        return -1;  /* Failure */
    }
    
    data->counter++;
    
    printf("Executing step: '%s' (type: %s, module: '%s') [Phase: %d, Counter: %d]\n", 
           step->step_name, 
           module_type_name(step->type),
           module->name,
           context->execution_phase,
           data->counter);
    
    printf("  Context: time=%f, dt=%f, step=%d\n",
           context->time,
           context->dt,
           context->step);
    
    return 0;  /* Success */
}

/* Mock step module getter for testing */
int pipeline_get_step_module(
    struct pipeline_step *step,
    struct base_module **module,
    void **module_data
) {
    if (strcmp(step->step_name, "infall_step") == 0) {
        *module = &infall_module;
        *module_data = &infall_data;
        return 0;
    } else if (strcmp(step->step_name, "cooling_step") == 0) {
        *module = &cooling_module;
        *module_data = &cooling_data;
        return 0;
    } else if (strcmp(step->step_name, "mergers_step") == 0) {
        *module = &mergers_module;
        *module_data = &mergers_data;
        return 0;
    } else if (strcmp(step->step_name, "misc_step") == 0) {
        *module = &misc_module;
        *module_data = &misc_data;
        return 0;
    }
    
    /* Not found */
    return -1;
}

/* Mock pipeline context initialization */
void pipeline_context_init(
    struct pipeline_context *context,
    void *params,
    void *galaxies,
    int ngal,
    int centralgal,
    double time,
    double dt,
    int halonr,
    int step,
    void *user_data
) {
    context->params = params;
    context->galaxies = galaxies;
    context->ngal = ngal;
    context->centralgal = centralgal;
    context->time = time;
    context->dt = dt;
    context->halonr = halonr;
    context->step = step;
    context->user_data = user_data;
    context->redshift = 0.0;
}

/* Mock pipeline execution function */
int pipeline_execute_custom(
    struct module_pipeline *pipeline,
    struct pipeline_context *context,
    int (*exec_fn)(struct pipeline_step*, struct base_module*, void*, struct pipeline_context*)
) {
    if (pipeline == NULL || context == NULL || exec_fn == NULL) {
        LOG_ERROR("Invalid arguments for pipeline execution");
        return -1;
    }
    
    LOG_INFO("Executing pipeline '%s' with %d steps", pipeline->name, pipeline->num_steps);
    
    /* Execute each enabled step */
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];
        
        /* Skip disabled steps */
        if (!step->enabled) {
            LOG_DEBUG("Skipping disabled step '%s'", step->step_name);
            continue;
        }
        
        /* Get the module for this step */
        struct base_module *module = NULL;
        void *module_data = NULL;
        
        pipeline_get_step_module(step, &module, &module_data);
        
        /* Execute the step */
        LOG_DEBUG("Executing step '%s'", step->step_name);
        int result = exec_fn(step, module, module_data, context);
        
        /* Handle step failure */
        if (result != 0) {
            if (step->optional) {
                LOG_DEBUG("Optional step '%s' failed, continuing pipeline", step->step_name);
            } else {
                LOG_ERROR("Required step '%s' failed, stopping pipeline", step->step_name);
                return result;
            }
        }
    }
    
    LOG_INFO("Pipeline '%s' completed successfully", pipeline->name);
    return 0;
}

/* Mock pipeline phase execution function */
int pipeline_execute_phase(
    struct module_pipeline *pipeline,
    struct pipeline_context *context,
    enum pipeline_execution_phase phase
) {
    if (pipeline == NULL || context == NULL) {
        LOG_ERROR("Invalid arguments for pipeline phase execution");
        return -1;
    }
    
    /* Set the execution phase in the context */
    context->execution_phase = phase;
    
    LOG_INFO("Executing pipeline '%s' for phase %d", pipeline->name, phase);
    
    /* Execute each enabled step that supports this phase */
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];
        
        /* Skip disabled steps */
        if (!step->enabled) {
            LOG_DEBUG("Skipping disabled step '%s'", step->step_name);
            continue;
        }
        
        /* Get the module for this step */
        struct base_module *module = NULL;
        void *module_data = NULL;
        
        int status = pipeline_get_step_module(step, &module, &module_data);
        if (status != 0) {
            LOG_DEBUG("No module found for step '%s'", step->step_name);
            continue;
        }
        
        /* Skip modules that don't support this execution phase */
        if (!(module->phases & phase)) {
            LOG_DEBUG("Skipping step '%s' as it doesn't support phase %d", 
                     step->step_name, phase);
            continue;
        }
        
        /* Execute the step */
        LOG_DEBUG("Executing step '%s' for phase %d", step->step_name, phase);
        int result = test_execute_step(step, module, module_data, context);
        
        /* Handle step failure */
        if (result != 0) {
            if (step->optional) {
                LOG_DEBUG("Optional step '%s' failed in phase %d, continuing", step->step_name, phase);
            } else {
                LOG_ERROR("Required step '%s' failed in phase %d, stopping", step->step_name, phase);
                return result;
            }
        }
    }
    
    LOG_INFO("Pipeline phase %d execution completed successfully", phase);
    return 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Basic pipeline functionality
 */
static void test_basic_pipeline_functionality(void) {
    printf("\n=== Testing basic pipeline functionality ===\n");
    
    /* Create a test pipeline */
    struct module_pipeline *pipeline = pipeline_create("test_pipeline");
    TEST_ASSERT(pipeline != NULL, "pipeline_create should succeed");
    TEST_ASSERT(strcmp(pipeline->name, "test_pipeline") == 0, "pipeline should have correct name");
    TEST_ASSERT(pipeline->initialized == true, "pipeline should be initialized");
    TEST_ASSERT(pipeline->num_steps == 0, "new pipeline should have 0 steps");
    
    /* Add steps to the pipeline */
    int result = pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall_step", true, false);
    TEST_ASSERT(result == 0, "pipeline_add_step should succeed");
    TEST_ASSERT(pipeline->num_steps == 1, "pipeline should have 1 step after adding");
    
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MISC, NULL, "misc_step", true, false);
    
    TEST_ASSERT(pipeline->num_steps == 4, "pipeline should have 4 steps");
    
    /* Create a test context */
    struct pipeline_context context;
    memset(&context, 0, sizeof(context));
    
    pipeline_context_init(&context, NULL, NULL, 0, -1, 100.0, 0.1, 0, 0, NULL);
    
    /* Reset module counters */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    mergers_data.counter = 0;
    misc_data.counter = 0;
    
    infall_data.should_fail = false;
    cooling_data.should_fail = false;
    mergers_data.should_fail = false;
    misc_data.should_fail = false;
    
    /* Test standard pipeline execution */
    result = pipeline_execute_custom(pipeline, &context, test_execute_step);
    TEST_ASSERT(result == 0, "pipeline_execute_custom should succeed");
    
    /* Verify all modules executed */
    TEST_ASSERT(infall_data.counter == 1, "infall module should execute once");
    TEST_ASSERT(cooling_data.counter == 1, "cooling module should execute once");
    TEST_ASSERT(mergers_data.counter == 1, "mergers module should execute once");
    TEST_ASSERT(misc_data.counter == 1, "misc module should execute once");
    
    pipeline_destroy(pipeline);
}

/**
 * Test: Phase-based execution
 */
static void test_phase_based_execution(void) {
    printf("\n=== Testing phase-based execution ===\n");
    
    struct module_pipeline *pipeline = pipeline_create("phase_test_pipeline");
    TEST_ASSERT(pipeline != NULL, "pipeline_create should succeed");
    
    /* Add steps to the pipeline */
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MISC, NULL, "misc_step", true, false);
    
    struct pipeline_context context;
    pipeline_context_init(&context, NULL, NULL, 0, -1, 100.0, 0.1, 0, 0, NULL);
    
    /* Reset module counters */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    mergers_data.counter = 0;
    misc_data.counter = 0;
    
    infall_data.should_fail = false;
    cooling_data.should_fail = false;
    mergers_data.should_fail = false;
    misc_data.should_fail = false;
    
    /* Test HALO phase - should execute infall and misc */
    int result = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_HALO);
    TEST_ASSERT(result == 0, "HALO phase execution should succeed");
    
    /* Test GALAXY phase - should execute infall, cooling, and misc */
    result = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_GALAXY);
    TEST_ASSERT(result == 0, "GALAXY phase execution should succeed");
    
    /* Test POST phase - should execute mergers and misc */
    result = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_POST);
    TEST_ASSERT(result == 0, "POST phase execution should succeed");
    
    /* Test FINAL phase - should execute misc only */
    result = pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_FINAL);
    TEST_ASSERT(result == 0, "FINAL phase execution should succeed");
    
    /* Verify execution counts */
    TEST_ASSERT(infall_data.counter == 2, "infall should execute in HALO + GALAXY phases (2 times)");
    TEST_ASSERT(cooling_data.counter == 1, "cooling should execute in GALAXY phase only (1 time)");
    TEST_ASSERT(mergers_data.counter == 1, "mergers should execute in POST phase only (1 time)");
    TEST_ASSERT(misc_data.counter == 4, "misc should execute in all phases (4 times)");
    
    pipeline_destroy(pipeline);
}

/**
 * Test: Error handling
 */
static void test_error_handling(void) {
    printf("\n=== Testing error handling ===\n");
    
    /* Test NULL parameters */
    int result = pipeline_execute_custom(NULL, NULL, NULL);
    TEST_ASSERT(result != 0, "pipeline_execute_custom with NULL params should fail");
    
    struct module_pipeline *pipeline = pipeline_create("error_test_pipeline");
    TEST_ASSERT(pipeline != NULL, "pipeline_create should succeed");
    
    /* Test adding too many steps (if we had that limitation) */
    result = pipeline_add_step(NULL, MODULE_TYPE_INFALL, NULL, "test", true, false);
    TEST_ASSERT(result != 0, "pipeline_add_step with NULL pipeline should fail");
    
    /* Add steps for module failure testing */
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall_step", true, false);  /* required */
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling_step", true, true); /* optional */
    
    struct pipeline_context context;
    pipeline_context_init(&context, NULL, NULL, 0, -1, 100.0, 0.1, 0, 0, NULL);
    
    /* Reset counters */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    
    /* Test required module failure */
    infall_data.should_fail = true;
    cooling_data.should_fail = false;
    
    result = pipeline_execute_custom(pipeline, &context, test_execute_step);
    TEST_ASSERT(result != 0, "pipeline should fail when required module fails");
    
    /* Test optional module failure */
    infall_data.should_fail = false;
    cooling_data.should_fail = true;
    
    result = pipeline_execute_custom(pipeline, &context, test_execute_step);
    TEST_ASSERT(result == 0, "pipeline should succeed when optional module fails");
    
    /* Reset failure flags */
    infall_data.should_fail = false;
    cooling_data.should_fail = false;
    
    pipeline_destroy(pipeline);
}

/**
 * Test: Edge cases
 */
static void test_edge_cases(void) {
    printf("\n=== Testing edge cases ===\n");
    
    /* Test empty pipeline */
    struct module_pipeline *empty_pipeline = pipeline_create("empty_pipeline");
    TEST_ASSERT(empty_pipeline != NULL, "empty pipeline creation should succeed");
    TEST_ASSERT(empty_pipeline->num_steps == 0, "empty pipeline should have 0 steps");
    
    struct pipeline_context context;
    pipeline_context_init(&context, NULL, NULL, 0, -1, 100.0, 0.1, 0, 0, NULL);
    
    int result = pipeline_execute_custom(empty_pipeline, &context, test_execute_step);
    TEST_ASSERT(result == 0, "empty pipeline execution should succeed");
    
    result = pipeline_execute_phase(empty_pipeline, &context, PIPELINE_PHASE_HALO);
    TEST_ASSERT(result == 0, "empty pipeline phase execution should succeed");
    
    pipeline_destroy(empty_pipeline);
    
    /* Test invalid phase values */
    struct module_pipeline *test_pipeline = pipeline_create("invalid_phase_test");
    pipeline_add_step(test_pipeline, MODULE_TYPE_MISC, NULL, "misc_step", true, false);
    
    /* Test with invalid phase (0) */
    result = pipeline_execute_phase(test_pipeline, &context, 0);
    TEST_ASSERT(result == 0, "pipeline should handle invalid phase gracefully");
    
    /* Test with very large phase value */
    result = pipeline_execute_phase(test_pipeline, &context, 0xFFFFFFFF);
    TEST_ASSERT(result == 0, "pipeline should handle large phase value gracefully");
    
    /* Test pipeline with NULL name */
    struct module_pipeline *null_name_pipeline = pipeline_create(NULL);
    TEST_ASSERT(null_name_pipeline != NULL, "pipeline with NULL name should still be created");
    TEST_ASSERT(strcmp(null_name_pipeline->name, "unnamed_pipeline") == 0, 
                "pipeline should have default name");
    
    pipeline_destroy(null_name_pipeline);
    pipeline_destroy(test_pipeline);
}

/**
 * Test: Integration with multiple phases
 */
static void test_integration_multiple_phases(void) {
    printf("\n=== Testing integration with multiple phases ===\n");
    
    struct module_pipeline *pipeline = pipeline_create("integration_test");
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MISC, NULL, "misc_step", true, false);
    
    struct pipeline_context context;
    pipeline_context_init(&context, NULL, NULL, 0, -1, 100.0, 0.1, 0, 0, NULL);
    
    /* Reset counters */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    mergers_data.counter = 0;
    misc_data.counter = 0;
    
    infall_data.should_fail = false;
    cooling_data.should_fail = false;
    mergers_data.should_fail = false;
    misc_data.should_fail = false;
    
    /* Test combined phase execution */
    enum pipeline_execution_phase combined_phase = PIPELINE_PHASE_HALO | PIPELINE_PHASE_POST;
    
    /* Manually test modules that support combined phases */
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];
        struct base_module *module = NULL;
        void *module_data = NULL;
        
        int status = pipeline_get_step_module(step, &module, &module_data);
        if (status == 0 && module != NULL) {
            if (module->phases & combined_phase) {
                context.execution_phase = combined_phase;
                test_execute_step(step, module, module_data, &context);
            }
        }
    }
    
    /* Verify expected execution counts */
    TEST_ASSERT(infall_data.counter == 1, "infall should execute (supports HALO)");
    TEST_ASSERT(cooling_data.counter == 0, "cooling should not execute (doesn't support HALO or POST)");
    TEST_ASSERT(mergers_data.counter == 1, "mergers should execute (supports POST)");
    TEST_ASSERT(misc_data.counter == 1, "misc should execute (supports both HALO and POST)");
    
    pipeline_destroy(pipeline);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(void) {
    printf("Starting Pipeline Phase System tests...\n");
    
    /* Run tests */
    test_basic_pipeline_functionality();
    test_phase_based_execution();
    test_error_handling();
    test_edge_cases();
    test_integration_multiple_phases();
    
    /* Report results */
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");
    
    return (tests_run == tests_passed) ? EXIT_SUCCESS : EXIT_FAILURE;
}