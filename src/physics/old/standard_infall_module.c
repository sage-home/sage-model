#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../core/core_allvars.h"
#include "../core/core_logging.h"
#include "../core/core_module_system.h"
#include "../core/core_module_callback.h"
#include "../core/core_parameter_views.h"
#include "physics_modules.h"
#include "model_infall.h"

/* Module data structure */
struct standard_infall_data {
    struct infall_params_view params;  /* Parameter view */
    double total_infall;              /* Total infall mass for current halo */
    double *galaxy_infall;            /* Per-galaxy infall amounts */
    int ngal;                         /* Number of galaxies */
};

/* Forward declarations */
static int standard_infall_initialize(void **module_data);
static void standard_infall_cleanup(void *module_data);
static int standard_infall_execute_halo(void *module_data, struct pipeline_context *context);
static int standard_infall_execute_galaxy(void *module_data, struct pipeline_context *context);
static int standard_infall_execute_post(void *module_data, struct pipeline_context *context);
static double standard_infall_calculate(void *module_data, struct pipeline_context *context);
static int standard_infall_apply(void *module_data, struct pipeline_context *context, double infall_mass);

/* Module interface instance */
static struct infall_module standard_infall_module = {
    .base = {
        .base = {
            .name = "StandardInfall",
            .version = "1.0.0",
            .author = "SAGE Team",
            .initialize = standard_infall_initialize,
            .cleanup = standard_infall_cleanup,
            .phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_GALAXY | PIPELINE_PHASE_POST
        },
        .module_data = NULL,
        .execute_halo_phase = standard_infall_execute_halo,
        .execute_galaxy_phase = standard_infall_execute_galaxy,
        .execute_post_phase = standard_infall_execute_post
    },
    .calculate_infall = standard_infall_calculate,
    .apply_infall = standard_infall_apply
};

/* Create module instance */
int infall_module_create(struct infall_module **module) {
    *module = &standard_infall_module;
    return MODULE_STATUS_SUCCESS;
}

/* Initialize module */
static int standard_infall_initialize(void **module_data) {
    struct standard_infall_data *data = calloc(1, sizeof(struct standard_infall_data));
    if (data == NULL) {
        LOG_ERROR("Failed to allocate infall module data");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    *module_data = data;
    
    /* Register callable functions for the module callback system */
    int module_id = standard_infall_module.base.base.module_id;
    
    /* Register the calculate_infall function */
    int status = module_register_function(
        module_id,
        "calculate_infall",
        standard_infall_calculate,
        FUNCTION_TYPE_DOUBLE,
        "double (void*, struct pipeline_context*)",
        "Calculate infall mass for current halo"
    );
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register calculate_infall function: %d", status);
    }
    
    /* Register the apply_infall function */
    status = module_register_function(
        module_id,
        "apply_infall",
        standard_infall_apply,
        FUNCTION_TYPE_INT,
        "int (void*, struct pipeline_context*, double)",
        "Apply calculated infall to a galaxy"
    );
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register apply_infall function: %d", status);
    }
    
    /* Declare dependencies on other modules */
    /* Example: dependency on cooling module (optional) */
    status = module_declare_simple_dependency(
        module_id,
        MODULE_TYPE_COOLING,
        NULL, /* Any cooling module */
        false /* Optional dependency */
    );
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_WARNING("Failed to declare cooling dependency: %d", status);
        /* Continue anyway - this is an optional dependency */
    }
    
    LOG_DEBUG("Initialized standard infall module with callbacks");
    return MODULE_STATUS_SUCCESS;
}

/* Clean up module */
static void standard_infall_cleanup(void *module_data) {
    struct standard_infall_data *data = module_data;
    if (data != NULL) {
        if (data->galaxy_infall != NULL) {
            free(data->galaxy_infall);
        }
        free(data);
    }
}

/* Execute HALO phase - calculate total infall */
static int standard_infall_execute_halo(void *module_data, struct pipeline_context *context) {
    struct standard_infall_data *data = module_data;
    
    /* Initialize parameter view */
    initialize_infall_params_view(&data->params, context->params);
    
    /* Allocate/resize galaxy infall array if needed */
    if (data->galaxy_infall == NULL || data->ngal < context->ngal) {
        data->galaxy_infall = realloc(data->galaxy_infall, context->ngal * sizeof(double));
        if (data->galaxy_infall == NULL) {
            LOG_ERROR("Failed to allocate galaxy infall array");
            return MODULE_STATUS_OUT_OF_MEMORY;
        }
        data->ngal = context->ngal;
    }
    
    /* Reset infall amounts */
    data->total_infall = 0.0;
    memset(data->galaxy_infall, 0, context->ngal * sizeof(double));
    
    /* Calculate total infall for this halo */
    data->total_infall = standard_infall_calculate(module_data, context);
    
    /* Store result in context for other modules */
    context->infall_gas = data->total_infall;
    
    LOG_DEBUG("Calculated total infall mass %.2e for halo %d", 
             data->total_infall, context->halonr);
    
    return MODULE_STATUS_SUCCESS;
}

/* Execute GALAXY phase - apply infall to individual galaxies and demonstrate module callbacks */
static int standard_infall_execute_galaxy(void *module_data, struct pipeline_context *context) {
    struct standard_infall_data *data = module_data;
    int gal = context->current_galaxy;
    int module_id = standard_infall_module.base.base.module_id;
    
    /* Only apply infall to central galaxies */
    if (context->galaxies[gal].Type != 0) {
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Example of using module callback to get cooling information */
    double cooling_rate = 0.0;
    int error_code = 0;
    
    LOG_DEBUG("Infall module attempting to call cooling module via callback");
    
    /* Call cooling module using callback system */
    int status = module_invoke(
        module_id,                /* Caller module ID */
        MODULE_TYPE_COOLING,      /* Type of module to call */
        NULL,                     /* Use any cooling module */
        "get_cooling_rate",       /* Function name to call */
        &error_code,              /* Context for error handling */
        context->galaxies[gal].HotGas * 1e6,  /* Temperature argument (estimate) */
        context->galaxies[gal].MetalsHotGas / 
            (context->galaxies[gal].HotGas + 1e-10), /* Metallicity argument */
        &cooling_rate             /* Result */
    );
    
    /* Handle errors from the callback */
    if (status != MODULE_STATUS_SUCCESS || error_code != 0) {
        LOG_WARNING("Could not get cooling rate via callback: status=%d, error=%d", 
                   status, error_code);
        /* Continue anyway - this is an optional enhancement */
    } else {
        LOG_DEBUG("Successfully called cooling module, cooling_rate=%.2e", cooling_rate);
        
        /* We could use the cooling_rate to modify infall behavior if desired */
    }
    
    /* Apply infall to this galaxy */
    return standard_infall_apply(module_data, context, data->total_infall);
}

/* Execute POST phase - cleanup and validation */
static int standard_infall_execute_post(void *module_data, struct pipeline_context *context) {
    struct standard_infall_data *data = module_data;
    
    /* Validate total mass conservation */
    double total_applied = 0.0;
    for (int i = 0; i < context->ngal; i++) {
        total_applied += data->galaxy_infall[i];
    }
    
    if (fabs(total_applied - data->total_infall) > 1e-6 * data->total_infall) {
        LOG_WARNING("Infall mass conservation error: applied %.2e != total %.2e",
                   total_applied, data->total_infall);
    }
    
    return MODULE_STATUS_SUCCESS;
}

/* Calculate infall for current halo */
static double standard_infall_calculate(void *module_data, struct pipeline_context *context) {
    struct standard_infall_data *data = module_data;
    
    /* Get halo properties */
    int halonr = context->halonr;
    struct GALAXY *galaxies = context->galaxies;
    int ngal = context->ngal;
    
    /* Calculate total baryonic mass currently in halo */
    double total_baryon_mass = 0.0;
    for (int i = 0; i < ngal; i++) {
        total_baryon_mass += 
            galaxies[i].StellarMass +
            galaxies[i].ColdGas +
            galaxies[i].HotGas +
            galaxies[i].EjectedMass +
            galaxies[i].ICS +
            galaxies[i].BlackHoleMass;
    }
    
    /* Calculate expected baryon mass */
    double expected_baryons = galaxies[0].Mvir * data->params.BaryonFraction;
    
    /* Calculate infall as difference (if positive) */
    double infall = expected_baryons - total_baryon_mass;
    if (infall < 0.0) infall = 0.0;
    
    /* Apply reionization suppression if enabled */
    if (data->params.ReionizationOn) {
        double modifier = do_reionization(halonr, context->redshift, galaxies, context->params);
        infall *= modifier;
    }
    
    LOG_DEBUG("Calculated infall for halo %d: %.2e (total baryons: %.2e, expected: %.2e)",
             halonr, infall, total_baryon_mass, expected_baryons);
    
    return infall;
}

/* Apply infall to a specific galaxy */
static int standard_infall_apply(void *module_data, struct pipeline_context *context, double infall_mass) {
    struct standard_infall_data *data = module_data;
    int gal = context->current_galaxy;
    struct GALAXY *galaxies = context->galaxies;
    
    /* Record infall amount */
    data->galaxy_infall[gal] = infall_mass;
    
    /* Add mass to hot gas reservoir */
    galaxies[gal].HotGas += infall_mass;
    
    /* Add metals using primordial metallicity */
    galaxies[gal].MetalsHotGas += infall_mass * data->params.PrimordialMetallicity;
    
    LOG_DEBUG("Applied infall %.2e to galaxy %d", infall_mass, gal);
    
    return MODULE_STATUS_SUCCESS;
}