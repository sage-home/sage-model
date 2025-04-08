#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>


#include "core_allvars.h"
#include "core_init.h"
#include "core_mymalloc.h"
#include "core_cool_func.h"
#include "core_tree_utils.h"
#include "core_logging.h"
#include "core_module_system.h"
#include "core_galaxy_extensions.h"
#include "core_event_system.h"
#include "core_pipeline_system.h"
#include "core_config_system.h"
#include "core_memory_pool.h"

/* Include physics module interfaces */
#include "../physics/module_cooling.h"
#include "../physics/example_event_handler.h"

/* These functions do not need to be exposed externally */
double integrand_time_to_present(const double a, void *param);
void read_snap_list(struct params *run_params);
double time_to_present(const double z, struct params *run_params);

/*
 * Main initialization function - calls component-specific initialization
 * 
 * This function initializes all the necessary components for the SAGE model.
 * Each component has its own initialization function that can be extended
 * independently, facilitating the plugin architecture.
 */
void init(struct params *run_params)
{
#ifdef VERBOSE
    const int ThisTask = run_params->runtime.ThisTask;
#endif

    /* Initialize components */
    initialize_units(run_params);
    LOG_DEBUG("Units initialized");
    
    initialize_simulation_times(run_params);
    LOG_DEBUG("Simulation times initialized");
    
    initialize_cooling();
    LOG_DEBUG("Cooling tables initialized");
    
    /* Initialize module system */
    initialize_module_system(run_params);
    LOG_DEBUG("Module system initialized");
    
    /* Initialize galaxy extension system */
    initialize_galaxy_extension_system();
    LOG_DEBUG("Galaxy extension system initialized");
    
    /* Initialize event system */
    initialize_event_system();
    LOG_DEBUG("Event system initialized");
    
    /* Initialize pipeline system */
    initialize_pipeline_system();
    LOG_DEBUG("Pipeline system initialized");
    
    /* Initialize configuration system */
    initialize_config_system(NULL); /* Use default configuration */
    LOG_DEBUG("Configuration system initialized");
    
    /* Initialize memory pool system if enabled */
    if (run_params->runtime.EnableGalaxyMemoryPool) {
        galaxy_pool_initialize();
        LOG_DEBUG("Galaxy memory pool initialized");
    } else {
        LOG_DEBUG("Galaxy memory pool disabled");
    }

#ifdef VERBOSE
    if(ThisTask == 0) {
        fprintf(stdout, "Initialization complete\n");
    }
#endif
}

/*
 * Initialize the module system
 * 
 * Sets up the module system and registers the default modules
 */
void initialize_module_system(struct params *run_params)
{
    /* Initialize the module system */
    int status = module_system_initialize();
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize module system, status = %d", status);
        return;
    }
    
    /* Configure the module system with parameters if provided */
    status = module_system_configure(run_params);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_WARNING("Failed to configure module system, status = %d", status);
        LOG_WARNING("Using default module system configuration");
        /* Continue anyway - configure handles NULL params with defaults */
    }
    
    /* Create and register the default cooling module */
    struct cooling_module *cooling_module = create_default_cooling_module();
    if (cooling_module == NULL) {
        LOG_ERROR("Failed to create default cooling module");
        module_system_cleanup();  /* Clean up to avoid memory leaks */
        return;
    }
    
    /* Register the module with the system */
    status = cooling_module_register(cooling_module);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register cooling module, status = %d", status);
        module_system_cleanup();  /* Clean up to avoid memory leaks */
        return;
    }
    
    /* Initialize the cooling module */
    status = cooling_module_initialize(cooling_module, run_params);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize cooling module, status = %d", status);
        module_system_cleanup();  /* Clean up to avoid memory leaks */
        return;
    }
    
    /* Set the module as active */
    status = module_set_active(cooling_module->base.module_id);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to set cooling module as active, status = %d", status);
        module_system_cleanup();  /* Clean up to avoid memory leaks */
        return;
    }
    
    LOG_INFO("Default cooling module registered and activated");
    
    /* Discover additional modules if enabled and registry is initialized */
    if (global_module_registry != NULL && global_module_registry->discovery_enabled) {
        LOG_INFO("Starting module discovery");
        
        int modules_found = module_discover(run_params);
        
        if (modules_found > 0) {
            LOG_INFO("Discovered %d additional modules", modules_found);
        } else if (modules_found < 0) {
            LOG_ERROR("Module discovery failed, status = %d", modules_found);
            /* Continue anyway - failing to discover additional modules is not fatal */
        } else {
            LOG_DEBUG("No additional modules found");
        }
    } else {
        LOG_DEBUG("Module discovery not enabled or registry not initialized");
    }
}

/*
 * Initialize units and physical constants
 * 
 * Calculates derived units and physical constants from the basic
 * units specified in the parameter file. This includes:
 * - Time units
 * - Energy units
 * - Density units
 * - Gravitational constant in code units
 * - Critical density
 */
void initialize_units(struct params *run_params)
{
    /* Derive time units from length and velocity */
    run_params->units.UnitTime_in_s = run_params->units.UnitLength_in_cm / run_params->units.UnitVelocity_in_cm_per_s;
    run_params->units.UnitTime_in_Megayears = run_params->units.UnitTime_in_s / SEC_PER_MEGAYEAR;
    
    /* Set gravitational constant in code units */
    run_params->cosmology.G = GRAVITY / CUBE(run_params->units.UnitLength_in_cm) * 
                             run_params->units.UnitMass_in_g * 
                             SQR(run_params->units.UnitTime_in_s);
    
    /* Derive density, pressure, cooling rate and energy units */
    run_params->units.UnitDensity_in_cgs = run_params->units.UnitMass_in_g / CUBE(run_params->units.UnitLength_in_cm);
    run_params->units.UnitPressure_in_cgs = run_params->units.UnitMass_in_g / 
                                           run_params->units.UnitLength_in_cm / 
                                           SQR(run_params->units.UnitTime_in_s);
    run_params->units.UnitCoolingRate_in_cgs = run_params->units.UnitPressure_in_cgs / run_params->units.UnitTime_in_s;
    run_params->units.UnitEnergy_in_cgs = run_params->units.UnitMass_in_g * 
                                         SQR(run_params->units.UnitLength_in_cm) / 
                                         SQR(run_params->units.UnitTime_in_s);

    /* Convert supernova parameters to code units */
    run_params->physics.EnergySNcode = run_params->physics.EnergySN / 
                                      run_params->units.UnitEnergy_in_cgs * 
                                      run_params->cosmology.Hubble_h;
    run_params->physics.EtaSNcode = run_params->physics.EtaSN * 
                                   (run_params->units.UnitMass_in_g / SOLAR_MASS) / 
                                   run_params->cosmology.Hubble_h;

    /* Set Hubble parameter in internal units */
    run_params->cosmology.Hubble = HUBBLE * run_params->units.UnitTime_in_s;

    /* Compute critical density */
    run_params->cosmology.RhoCrit = 3.0 * run_params->cosmology.Hubble * run_params->cosmology.Hubble / 
                                   (8 * M_PI * run_params->cosmology.G);
}

/*
 * Cleanup units and constants
 * 
 * Currently empty as no memory is allocated specifically for units,
 * but included for future extensions and consistency.
 */
void cleanup_units(struct params *run_params)
{
    /* No resources to free at this time */
    (void)run_params; /* Suppress unused parameter warning */
}

/*
 * Initialize simulation times and redshifts
 * 
 * Reads the snapshot list from file and calculates:
 * - Redshift for each snapshot
 * - Age of the universe at each snapshot
 * - Reionization parameters
 */
void initialize_simulation_times(struct params *run_params)
{
    /* Allocate memory for age array */
    run_params->simulation.Age = mymalloc(ABSOLUTEMAXSNAPS*sizeof(run_params->simulation.Age[0]));
    
    /* Read list of snapshots from file */
    read_snap_list(run_params);

    /* Initialize ages array with hack to fix deltaT for snapshot 0 */
    run_params->simulation.Age[0] = time_to_present(1000.0, run_params); /* lookback time from z=1000 */
    run_params->simulation.Age++;

    /* Calculate redshift and age for each snapshot */
    for(int i = 0; i < run_params->simulation.Snaplistlen; i++) {
        run_params->simulation.ZZ[i] = 1 / run_params->simulation.AA[i] - 1;
        run_params->simulation.Age[i] = time_to_present(run_params->simulation.ZZ[i], run_params);
    }

    /* Set reionization parameters */
    run_params->physics.a0 = 1.0 / (1.0 + run_params->physics.Reionization_z0);
    run_params->physics.ar = 1.0 / (1.0 + run_params->physics.Reionization_zr);
}

/*
 * Cleanup simulation times resources
 * 
 * Frees the memory allocated for the Age array.
 */
void cleanup_simulation_times(struct params *run_params)
{
    /* Reset Age pointer to its original allocation address */
    run_params->simulation.Age--;
    
    /* Free Age array */
    myfree(run_params->simulation.Age);
}

/*
 * Initialize cooling functions
 * 
 * Reads the cooling tables and prepares them for use in the model
 */
void initialize_cooling(void)
{
    read_cooling_functions();
}

/*
 * Cleanup cooling resources
 * 
 * Frees any memory allocated for cooling tables.
 */
void cleanup_cooling(void)
{
    /* Currently, there's no explicit cleanup for cooling tables.
       This function is a placeholder for future extensions. */
}

/*
 * Main cleanup function - calls component-specific cleanup
 * 
 * This function releases all resources allocated during initialization.
 * Each component has its own cleanup function to ensure all resources
 * are properly freed.
 */
void cleanup(struct params *run_params)
{
    /* Clean up components in reverse order of initialization */
    LOG_DEBUG("Starting component cleanup");
    
    /* Clean up systems in reverse order of initialization */
    cleanup_config_system();
    cleanup_pipeline_system();
    cleanup_event_system();
    cleanup_galaxy_extension_system();
    cleanup_module_system();
    
    /* Clean up memory pool if it was enabled */
    if (galaxy_pool_is_enabled()) {
        galaxy_pool_cleanup();
        LOG_DEBUG("Galaxy memory pool cleaned up");
    }
    
    cleanup_cooling();
    cleanup_simulation_times(run_params);
    cleanup_units(run_params);
    
    LOG_DEBUG("Component cleanup completed");
}

/*
 * Clean up the module system
 * 
 * Shuts down the module system and cleans up resources
 */
void cleanup_module_system(void)
{
    int status = module_system_cleanup();
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to clean up module system, status = %d", status);
    } else {
        LOG_DEBUG("Module system cleaned up");
    }
}

/*
 * Initialize the galaxy extension system
 * 
 * Sets up the extension system for adding custom properties to galaxies.
 */
void initialize_galaxy_extension_system(void)
{
    int status = galaxy_extension_system_initialize();
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize galaxy extension system, status = %d", status);
    } else {
        LOG_INFO("Galaxy extension system initialized");
    }
}

/**
 * Initialize the event system
 * 
 * Sets up the event system and registers example event handlers.
 */
void initialize_event_system(void)
{
    /* Initialize the event system */
    event_status_t status = event_system_initialize();
    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize event system, status = %d", status);
        return;
    }
    
    /* Register example event handlers for demonstration */
    int handler_status = register_example_event_handlers();
    if (handler_status != 0) {
        LOG_ERROR("Failed to register example event handlers, status = %d", handler_status);
        return;
    }
    
    LOG_INFO("Event system initialized with example handlers");
}

/*
 * Clean up the galaxy extension system
 * 
 * Shuts down the extension system and releases resources.
 */
void cleanup_galaxy_extension_system(void)
{
    int status = galaxy_extension_system_cleanup();
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to clean up galaxy extension system, status = %d", status);
    } else {
        LOG_DEBUG("Galaxy extension system cleaned up");
    }
}

/**
 * Clean up the event system
 * 
 * Unregisters event handlers and cleans up resources.
 */
void cleanup_event_system(void)
{
    /* Unregister example event handlers */
    int handler_status = unregister_example_event_handlers();
    if (handler_status != 0) {
        LOG_ERROR("Failed to unregister example event handlers, status = %d", handler_status);
    } else {
        LOG_DEBUG("Example event handlers unregistered");
    }
    
    /* Clean up the event system */
    event_status_t status = event_system_cleanup();
    if (status != EVENT_STATUS_SUCCESS) {
        LOG_ERROR("Failed to clean up event system, status = %d", status);
    } else {
        LOG_DEBUG("Event system cleaned up");
    }
}

/**
 * Initialize the pipeline system
 * 
 * Sets up the pipeline system and creates the default pipeline.
 */
void initialize_pipeline_system(void)
{
    /* Initialize the pipeline system */
    int status = pipeline_system_initialize();
    if (status != 0) {
        LOG_ERROR("Failed to initialize pipeline system, status = %d", status);
        return;
    }
    
    /* Register pipeline events if event system is available */
    if (event_system_is_initialized()) {
        status = pipeline_register_events();
        if (status != 0) {
            LOG_WARNING("Failed to register pipeline events, status = %d", status);
            /* Continue anyway - failing to register events is not fatal */
        }
    }
    
    /* Create the default pipeline */
    struct module_pipeline *default_pipeline = pipeline_create_default();
    if (default_pipeline == NULL) {
        LOG_ERROR("Failed to create default pipeline");
        return;
    }
    
    /* Validate the pipeline */
    if (!pipeline_validate(default_pipeline)) {
        LOG_WARNING("Default pipeline validation failed");
        /* Continue anyway - validation failures are warnings */
    }
    
    /* Set as global pipeline */
    if (pipeline_set_global(default_pipeline) != 0) {
        LOG_ERROR("Failed to set global pipeline");
        pipeline_destroy(default_pipeline);
        return;
    }
    
    LOG_INFO("Pipeline system initialized with default pipeline");
}

/**
 * Clean up the pipeline system
 * 
 * Releases resources used by the pipeline system.
 */
void cleanup_pipeline_system(void)
{
    int status = pipeline_system_cleanup();
    if (status != 0) {
        LOG_ERROR("Failed to clean up pipeline system, status = %d", status);
    } else {
        LOG_DEBUG("Pipeline system cleaned up");
    }
}

/**
 * Initialize the configuration system
 * 
 * Sets up the configuration system and loads the specified config file.
 * 
 * @param config_file Path to the config file, or NULL to use defaults
 */
void initialize_config_system(const char *config_file)
{
    /* Initialize the configuration system */
    int status = config_system_initialize();
    if (status != 0) {
        LOG_ERROR("Failed to initialize configuration system, status = %d", status);
        return;
    }
    
    /* Load configuration file if specified */
    if (config_file != NULL) {
        status = config_load_file(config_file);
        if (status != 0) {
            LOG_WARNING("Failed to load configuration file '%s', status = %d", config_file, status);
            LOG_WARNING("Using default configuration instead");
        } else {
            LOG_INFO("Loaded configuration from '%s'", config_file);
            
            /* Apply configuration to module system if available */
            if (global_module_registry != NULL) {
                status = config_configure_modules(NULL);
                if (status != 0) {
                    LOG_WARNING("Failed to configure modules from configuration, status = %d", status);
                } else {
                    LOG_INFO("Modules configured from configuration file");
                }
            }
            
            /* Configure pipeline from configuration */
            status = config_configure_pipeline();
            if (status != 0) {
                LOG_WARNING("Failed to configure pipeline from configuration, status = %d", status);
            } else {
                LOG_INFO("Pipeline configured from configuration file");
            }
        }
    } else {
        LOG_INFO("Using default configuration (no file specified)");
    }
}

/**
 * Clean up the configuration system
 * 
 * Releases resources used by the configuration system.
 */
void cleanup_config_system(void)
{
    int status = config_system_cleanup();
    if (status != 0) {
        LOG_ERROR("Failed to clean up configuration system, status = %d", status);
    } else {
        LOG_DEBUG("Configuration system cleaned up");
    }
}

/*
 * Initialize the galaxy evolution context
 * 
 * Sets up the context structure used during galaxy evolution with all
 * necessary pointers and initial values. This encapsulates the state
 * that was previously scattered throughout the evolve_galaxies function.
 */
/*
 * Validate evolution context for internal consistency
 * 
 * Checks that the evolution context is in a valid state with consistent values
 * across all fields. This catches potential numerical issues and inconsistencies
 * that could lead to crashes or incorrect physical results.
 * 
 * @param ctx The evolution context to validate
 * @return true if context is valid, false otherwise
 */
bool validate_evolution_context(const struct evolution_context *ctx)
{
    if (ctx == NULL) {
        LOG_ERROR("Null context passed to validate_evolution_context");
        return false;
    }
    
    /* Validate galaxy references */
    if (ctx->galaxies == NULL) {
        LOG_ERROR("Galaxy array is NULL in evolution context");
        return false;
    }
    
    if (ctx->ngal <= 0) {
        LOG_ERROR("Invalid number of galaxies in evolution context: ngal=%d", ctx->ngal);
        return false;
    }
    
    /* Validate central galaxy index is within bounds */
    if (ctx->centralgal < 0 || ctx->centralgal >= ctx->ngal) {
        LOG_ERROR("Invalid central galaxy index: centralgal=%d, ngal=%d", ctx->centralgal, ctx->ngal);
        return false;
    }
    
    /* Validate the central galaxy type */
    if (ctx->galaxies[ctx->centralgal].Type != 0) {
        LOG_WARNING("Central galaxy has unexpected type: %d (should be 0)", ctx->galaxies[ctx->centralgal].Type);
        /* Not a critical error, just a warning */
    }
    
    /* Validate time values */
    if (!isfinite(ctx->redshift) || ctx->redshift < 0.0) {
        LOG_ERROR("Invalid redshift in evolution context: redshift=%g", ctx->redshift);
        return false;
    }
    
    if (!isfinite(ctx->halo_age)) {
        LOG_ERROR("Invalid halo age in evolution context: halo_age=%g", ctx->halo_age);
        return false;
    }
    
    /* Validate that deltaT is finite when non-zero (might be 0.0 initially) */
    if (ctx->deltaT != 0.0 && !isfinite(ctx->deltaT)) {
        LOG_ERROR("Invalid time step in evolution context: deltaT=%g", ctx->deltaT);
        return false;
    }
    
    /* Validate that parameters reference is valid */
    if (ctx->params == NULL) {
        LOG_ERROR("Parameters reference is NULL in evolution context");
        return false;
    }
    
    /* Validate snapshot number is within bounds */
    if (ctx->halo_snapnum < 0 || ctx->halo_snapnum >= ctx->params->simulation.Snaplistlen) {
        LOG_ERROR("Invalid snapshot number in evolution context: snapnum=%d, max=%d", 
                 ctx->halo_snapnum, ctx->params->simulation.Snaplistlen);
        return false;
    }
    
    return true;
}

void initialize_evolution_context(struct evolution_context *ctx, 
                                 const int halonr,
                                 struct GALAXY *galaxies, 
                                 const int ngal,
                                 struct halo_data *halos,
                                 struct params *run_params)
{
    if (ctx == NULL) {
        LOG_ERROR("Null pointer passed to initialize_evolution_context");
        return;
    }
    
    /* Initialize galaxy references */
    ctx->galaxies = galaxies;
    ctx->ngal = ngal;
    ctx->centralgal = galaxies[0].CentralGal;
    
    /* Initialize halo properties */
    ctx->halo_nr = halonr;
    ctx->halo_snapnum = halos[halonr].SnapNum;
    ctx->redshift = run_params->simulation.ZZ[ctx->halo_snapnum];
    ctx->halo_age = run_params->simulation.Age[ctx->halo_snapnum];
    
    /* Initialize time integration */
    ctx->deltaT = 0.0;  /* Will be set during evolution */
    
    /* Initialize parameters reference */
    ctx->params = run_params;
    
    /* Validate the newly initialized context */
    if (!validate_evolution_context(ctx)) {
        LOG_WARNING("Evolution context validation failed after initialization");
    }
    
    /* Ensure all galaxies have their extension fields properly initialized */
    for (int i = 0; i < ctx->ngal; i++) {
        if (ctx->galaxies[i].extension_data == NULL) {
            ctx->galaxies[i].num_extensions = 0;
            ctx->galaxies[i].extension_flags = 0;
        }
    }
}

/*
 * Clean up the galaxy evolution context
 * 
 * Performs any necessary cleanup for the context structure.
 * Currently minimal as the context doesn't own any memory,
 * but included for future extensibility.
 */
void cleanup_evolution_context(struct evolution_context *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    /* Reset pointers to prevent dangling references */
    ctx->galaxies = NULL;
    ctx->params = NULL;
    
    /* Reset values */
    ctx->ngal = 0;
    ctx->centralgal = -1;
    ctx->halo_nr = -1;
    ctx->deltaT = 0.0;
}

/*
 * Read snapshot list from file
 */
void read_snap_list(struct params *run_params)
{
#ifdef VERBOSE
    const int ThisTask = run_params->runtime.ThisTask;
#endif

    char fname[MAX_STRING_LEN+1];

    snprintf(fname, MAX_STRING_LEN, "%s", run_params->io.FileWithSnapList);
    FILE *fd = fopen(fname, "r");
    if(fd == NULL) {
        fprintf(stderr, "can't read output list in file '%s'\n", fname);
        ABORT(0);
    }

    run_params->simulation.Snaplistlen = 0;
    do {
        if(fscanf(fd, " %lg ", &(run_params->simulation.AA[run_params->simulation.Snaplistlen])) == 1) {
            run_params->simulation.Snaplistlen++;
        } else {
            break;
        }
    } while(run_params->simulation.Snaplistlen < run_params->simulation.SimMaxSnaps);
    fclose(fd);

#ifdef VERBOSE
    if(ThisTask == 0) {
        fprintf(stdout, "found %d defined times in snaplist\n", run_params->simulation.Snaplistlen);
    }
#endif
}

/*
 * Calculate time to present from redshift
 */
double time_to_present(const double z, struct params *run_params)
{
    const double end_limit = 1.0;
    const double start_limit = 1.0/(1 + z);
    double result;
    
    /* Integrate numerically using the trapezoidal rule */
    const double step  = 1e-7;
    const int64_t nsteps = (end_limit - start_limit)/step;
    result = 0.0;
    const double y0 = integrand_time_to_present(start_limit + 0*step, run_params);
    const double yn = integrand_time_to_present(start_limit + nsteps*step, run_params);
    for(int64_t i=1; i<nsteps; i++) {
        result  += integrand_time_to_present(start_limit + i*step, run_params);
    }

    result = (step*0.5)*(y0 + yn + 2.0*result);

    /* convert into Myrs/h (I think -> MS 23/6/2018) */
    const double time = 1.0 / run_params->cosmology.Hubble * result;

    // return time to present as a function of redshift
    return time;
}

/*
 * Integrand for time to present calculation
 */
double integrand_time_to_present(const double a, void *param)
{
    const struct params *run_params = (struct params *) param;
    return 1.0 / sqrt(run_params->cosmology.Omega / a + 
                     (1.0 - run_params->cosmology.Omega - run_params->cosmology.OmegaLambda) + 
                     run_params->cosmology.OmegaLambda * a * a);
}
