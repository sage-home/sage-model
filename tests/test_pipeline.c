#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/core_logging.h"
#include "../src/core/core_event_system.h"
#include "../src/core/core_pipeline_system.h"

/* Custom step execution function for testing */
static int test_execute_step(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
) {
    /* Silence unused parameter warnings */
    (void)module;
    (void)module_data;
    
    printf("Executing step: '%s' (type: %s)\n", 
           step->step_name, 
           module_type_name(step->type));
    
    printf("  Context: time=%f, dt=%f, step=%d\n",
           context->time,
           context->dt,
           context->step);
    
    return 0;  /* Success */
}

int main(int argc, char *argv[]) {
    /* Initialize logging */
    initialize_logging(NULL);
    LOG_INFO("Pipeline test starting");
    
    /* Initialize event system */
    event_system_initialize();
    LOG_INFO("Event system initialized");
    
    /* Initialize pipeline system */
    int status = pipeline_system_initialize();
    if (status != 0) {
        LOG_ERROR("Failed to initialize pipeline system");
        return EXIT_FAILURE;
    }
    LOG_INFO("Pipeline system initialized");
    
    /* Create a test pipeline */
    struct module_pipeline *pipeline = pipeline_create("test_pipeline");
    if (pipeline == NULL) {
        LOG_ERROR("Failed to create test pipeline");
        return EXIT_FAILURE;
    }
    
    /* Add steps to the pipeline */
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_STAR_FORMATION, NULL, "star_formation_step", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_MERGERS, NULL, "mergers_step", true, false);
    
    LOG_INFO("Created pipeline with %d steps", pipeline->num_steps);
    
    /* Create a test context */
    struct pipeline_context context;
    struct params test_params;
    memset(&test_params, 0, sizeof(test_params));
    
    double test_time = 100.0;
    double test_dt = 0.1;
    
    pipeline_context_init(
        &context,
        &test_params,           /* Parameters */
        NULL,                   /* Galaxies (none for test) */
        0,                      /* Number of galaxies */
        -1,                     /* Central galaxy */
        test_time,              /* Time */
        test_dt,                /* Time step */
        0,                      /* Halo number */
        0,                      /* Step */
        NULL                    /* User data */
    );
    
    /* Execute the pipeline with our custom executor */
    status = pipeline_execute_custom(pipeline, &context, test_execute_step);
    if (status != 0) {
        LOG_ERROR("Pipeline execution failed with status %d", status);
        return EXIT_FAILURE;
    }
    
    LOG_INFO("Pipeline execution completed successfully");
    
    /* Clean up */
    pipeline_destroy(pipeline);
    pipeline_system_cleanup();
    event_system_cleanup();
    cleanup_logging();
    
    return EXIT_SUCCESS;
}
