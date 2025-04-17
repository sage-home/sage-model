/* Test for pipeline phase system (HALO, GALAXY, POST, FINAL)
 * 
 * This simplified test doesn't need external dependencies as it
 * uses mock functions for core components.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Mock definitions to avoid external dependencies */
#define MAX_MODULE_NAME 64
#define MAX_STEP_NAME 64
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

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

struct test_module_data infall_data = { 0 };
struct test_module_data cooling_data = { 0 };
struct test_module_data mergers_data = { 0 };
struct test_module_data misc_data = { 0 };

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
    } else {
        struct test_module_data *data = (struct test_module_data *)module_data;
        data->counter++;
        
        printf("Executing step: '%s' (type: %s, module: '%s') [Phase: %d, Counter: %d]\n", 
               step->step_name, 
               module_type_name(step->type),
               module->name,
               context->execution_phase,
               data->counter);
    }
    
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
        exec_fn(step, module, module_data, context);
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
        test_execute_step(step, module, module_data, context);
    }
    
    LOG_INFO("Pipeline phase %d execution completed successfully", phase);
    return 0;
}

int main(int argc, char *argv[]) {
    printf("Pipeline phase system test starting\n");
    
    /* Create a test pipeline */
    struct module_pipeline *pipeline = pipeline_create("test_pipeline");
    if (pipeline == NULL) {
        LOG_ERROR("Failed to create test pipeline");
        return EXIT_FAILURE;
    }
    
    /* Add steps to the pipeline */
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MISC, NULL, "misc_step", true, false);
    
    LOG_INFO("Created pipeline with %d steps", pipeline->num_steps);
    
    /* Create a test context */
    struct pipeline_context context;
    memset(&context, 0, sizeof(context));
    
    double test_time = 100.0;
    double test_dt = 0.1;
    
    pipeline_context_init(
        &context,
        NULL,                   /* Parameters */
        NULL,                   /* Galaxies (none for test) */
        0,                      /* Number of galaxies */
        -1,                     /* Central galaxy */
        test_time,              /* Time */
        test_dt,                /* Time step */
        0,                      /* Halo number */
        0,                      /* Step */
        NULL                    /* User data */
    );
    
    /* Reset module counters */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    mergers_data.counter = 0;
    misc_data.counter = 0;
    
    /* Test 1: Standard pipeline execution */
    printf("\n=== Test 1: Standard Pipeline Execution ===\n");
    pipeline_execute_custom(pipeline, &context, test_execute_step);
    
    /* Reset module counters */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    mergers_data.counter = 0;
    misc_data.counter = 0;
    
    /* Test 2: Phase-based execution - HALO phase */
    printf("\n=== Test 2: HALO Phase Execution ===\n");
    pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_HALO);
    
    /* Test 3: Phase-based execution - GALAXY phase */
    printf("\n=== Test 3: GALAXY Phase Execution ===\n");
    pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_GALAXY);
    
    /* Test 4: Phase-based execution - POST phase */
    printf("\n=== Test 4: POST Phase Execution ===\n");
    pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_POST);
    
    /* Test 5: Phase-based execution - FINAL phase */
    printf("\n=== Test 5: FINAL Phase Execution ===\n");
    pipeline_execute_phase(pipeline, &context, PIPELINE_PHASE_FINAL);
    
    /* Test 6: Edge case - Multiple phases simultaneously */
    printf("\n=== Test 6: Multiple Phase Support Testing ===\n");
    
    /* Reset counters for multi-phase test */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    mergers_data.counter = 0;
    misc_data.counter = 0;
    
    /* Set up a custom bitmask combining HALO and POST phases */
    enum pipeline_execution_phase combined_phase = PIPELINE_PHASE_HALO | PIPELINE_PHASE_POST;
    
    /* Manually set up a context with this special phase for testing purposes */
    pipeline_context_init(&context, NULL, NULL, 0, -1, test_time, test_dt, 0, 0, NULL);
    context.execution_phase = combined_phase;
    
    /* Loop through all steps manually to test multi-phase support */
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];
        struct base_module *module = NULL;
        void *module_data = NULL;
        
        pipeline_get_step_module(step, &module, &module_data);
        
        if (module != NULL) {
            /* A module should execute if it supports ANY of the phases in the bitmask */
            if (module->phases & combined_phase) {
                printf("Module '%s' supports at least one of the combined phases\n", module->name);
                test_execute_step(step, module, module_data, &context);
            } else {
                printf("Module '%s' doesn't support any of the combined phases\n", module->name);
            }
        }
    }
    
    /* Clean up */
    pipeline_destroy(pipeline);
    
    /* Verify that only modules that support each phase were executed */
    int success = 1;
    
    /* Expected execution counts for the main tests */
    if (infall_data.counter != 1) {  /* HALO supported in combined test */
        printf("ERROR: Infall module should have executed 1 time (combined HALO+POST test), but executed %d times\n", 
               infall_data.counter);
        success = 0;
    }
    
    if (cooling_data.counter != 0) {  /* Doesn't support HALO or POST */
        printf("ERROR: Cooling module should not have executed in combined HALO+POST test, but executed %d times\n", 
               cooling_data.counter);
        success = 0;
    }
    
    if (mergers_data.counter != 1) {  /* POST supported in combined test */
        printf("ERROR: Mergers module should have executed 1 time (combined HALO+POST test), but executed %d times\n", 
               mergers_data.counter);
        success = 0;
    }
    
    if (misc_data.counter != 1) {  /* Supports both HALO and POST */
        printf("ERROR: Misc module should have executed 1 time (combined HALO+POST test), but executed %d times\n", 
               misc_data.counter);
        success = 0;
    }
    
    /* Reset counters again */
    infall_data.counter = 0;
    cooling_data.counter = 0;
    mergers_data.counter = 0;
    misc_data.counter = 0;
    
    /* Run the standard tests again */
    struct module_pipeline *test_pipeline = pipeline_create("reset_test_pipeline");
    pipeline_add_step(test_pipeline, MODULE_TYPE_INFALL, NULL, "infall_step", true, false);
    pipeline_add_step(test_pipeline, MODULE_TYPE_COOLING, NULL, "cooling_step", true, false);
    pipeline_add_step(test_pipeline, MODULE_TYPE_MERGERS, NULL, "mergers_step", true, false);
    pipeline_add_step(test_pipeline, MODULE_TYPE_MISC, NULL, "misc_step", true, false);
    
    pipeline_context_init(&context, NULL, NULL, 0, -1, test_time, test_dt, 0, 0, NULL);
    
    /* Run all standard phase tests again */
    pipeline_execute_phase(test_pipeline, &context, PIPELINE_PHASE_HALO);
    pipeline_execute_phase(test_pipeline, &context, PIPELINE_PHASE_GALAXY);
    pipeline_execute_phase(test_pipeline, &context, PIPELINE_PHASE_POST);
    pipeline_execute_phase(test_pipeline, &context, PIPELINE_PHASE_FINAL);
    
    pipeline_destroy(test_pipeline);
    
    /* Check standard test results */
    if (infall_data.counter != 2) {  /* HALO + GALAXY */
        printf("ERROR: Infall module should have executed 2 times (HALO + GALAXY), but executed %d times\n", 
               infall_data.counter);
        success = 0;
    }
    
    if (cooling_data.counter != 1) {  /* GALAXY only */
        printf("ERROR: Cooling module should have executed 1 time (GALAXY), but executed %d times\n", 
               cooling_data.counter);
        success = 0;
    }
    
    if (mergers_data.counter != 1) {  /* POST only */
        printf("ERROR: Mergers module should have executed 1 time (POST), but executed %d times\n", 
               mergers_data.counter);
        success = 0;
    }
    
    if (misc_data.counter != 4) {  /* All phases */
        printf("ERROR: Misc module should have executed 4 times (all phases), but executed %d times\n", 
               misc_data.counter);
        success = 0;
    }
    
    if (success) {
        printf("\nAll pipeline phase tests PASSED!\n");
        return EXIT_SUCCESS;
    } else {
        printf("\nSome pipeline phase tests FAILED!\n");
        return EXIT_FAILURE;
    }
}