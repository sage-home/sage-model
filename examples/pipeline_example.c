#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_event_system.h"
#include "../src/core/core_pipeline_system.h"

/* Dummy physics module functions */
static double infall_recipe(int centralgal, int ngal, double z, struct GALAXY *galaxies) {
    printf("Calculating infall for central galaxy %d\n", centralgal);
    return 1.0; /* Mock infall amount */
}

static void add_infall_to_hot(int centralgal, double infall, struct GALAXY *galaxies) {
    printf("Adding %.2f infall gas to hot component of galaxy %d\n", infall, centralgal);
}

static double cooling_recipe(int p, double dt, struct GALAXY *galaxies) {
    printf("Calculating cooling for galaxy %d with dt=%.2f\n", p, dt);
    return 0.5; /* Mock cooling amount */
}

static void cool_gas_onto_galaxy(int p, double cooling_gas, struct GALAXY *galaxies) {
    printf("Adding %.2f cooled gas to galaxy %d\n", cooling_gas, p);
}

/**
 * Example of how to implement a pipeline step executor
 */
static int example_physics_step(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
) {
    /* Suppress unused parameter warnings */
    (void)module;
    (void)module_data;
    
    /* Extract context data */
    struct GALAXY *galaxies = context->galaxies;
    int ngal = context->ngal;
    int centralgal = context->centralgal;
    double dt = context->dt;
    
    /* Process based on module type */
    switch (step->type) {
        case MODULE_TYPE_INFALL:
            printf("Executing INFALL step\n");
            
            /* Calculate infall for central galaxy */
            double infall_gas = infall_recipe(centralgal, ngal, 0.0, galaxies);
            
            /* Apply infall to central galaxy */
            add_infall_to_hot(centralgal, infall_gas, galaxies);
            break;
            
        case MODULE_TYPE_COOLING:
            printf("Executing COOLING step\n");
            
            /* Process cooling for all galaxies */
            for (int p = 0; p < ngal; p++) {
                double cooling_gas = cooling_recipe(p, dt, galaxies);
                cool_gas_onto_galaxy(p, cooling_gas, galaxies);
            }
            break;
            
        default:
            printf("Step type %d not implemented in this example\n", step->type);
            break;
    }
    
    return 0; /* Success */
}

int main(int argc, char *argv[]) {
    /* Initialize systems */
    initialize_logging(NULL);
    event_system_initialize();
    pipeline_system_initialize();
    
    /* Create mock galaxies */
    const int ngal = 3;
    struct GALAXY galaxies[3];
    memset(galaxies, 0, sizeof(galaxies));
    
    galaxies[0].Type = 0; /* Central */
    galaxies[1].Type = 1; /* Satellite */
    galaxies[2].Type = 1; /* Satellite */
    
    /* Set up parameters */
    struct params run_params;
    memset(&run_params, 0, sizeof(run_params));
    
    /* Create a pipeline */
    struct module_pipeline *pipeline = pipeline_create("galaxy_evolution");
    pipeline_add_step(pipeline, MODULE_TYPE_INFALL, NULL, "infall", true, false);
    pipeline_add_step(pipeline, MODULE_TYPE_COOLING, NULL, "cooling", true, false);
    
    /* Set up the context for pipeline execution */
    struct pipeline_context context;
    pipeline_context_init(
        &context,
        &run_params,
        galaxies,
        ngal,
        0, /* centralgal */
        100.0, /* time */
        0.1, /* dt */
        1, /* halonr */
        0, /* step */
        NULL /* user_data */
    );
    
    /* Execute the pipeline */
    printf("\nExecuting pipeline with %d steps\n", pipeline->num_steps);
    printf("-------------------------------------\n");
    int status = pipeline_execute_custom(pipeline, &context, example_physics_step);
    printf("-------------------------------------\n");
    printf("Pipeline execution %s (status: %d)\n\n", 
           (status == 0) ? "succeeded" : "failed", status);
    
    /* Clean up */
    pipeline_destroy(pipeline);
    pipeline_system_cleanup();
    event_system_cleanup();
    cleanup_logging();
    
    return 0;
}
