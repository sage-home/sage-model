#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#ifdef GSL_FOUND
#include <gsl/gsl_integration.h>
#endif

#include "core_allvars.h"
#include "core_init.h"
#include "core_mymalloc.h"
#include "core_cool_func.h"
#include "core_tree_utils.h"

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
    const int ThisTask = run_params->ThisTask;
#endif

    /* Initialize components */
    initialize_units(run_params);
    initialize_simulation_times(run_params);
    initialize_cooling();

#ifdef VERBOSE
    if(ThisTask == 0) {
        fprintf(stdout, "Initialization complete\n\n");
    }
#endif
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
    run_params->UnitTime_in_s = run_params->UnitLength_in_cm / run_params->UnitVelocity_in_cm_per_s;
    run_params->UnitTime_in_Megayears = run_params->UnitTime_in_s / SEC_PER_MEGAYEAR;
    
    /* Set gravitational constant in code units */
    run_params->G = GRAVITY / CUBE(run_params->UnitLength_in_cm) * run_params->UnitMass_in_g * SQR(run_params->UnitTime_in_s);
    
    /* Derive density, pressure, cooling rate and energy units */
    run_params->UnitDensity_in_cgs = run_params->UnitMass_in_g / CUBE(run_params->UnitLength_in_cm);
    run_params->UnitPressure_in_cgs = run_params->UnitMass_in_g / run_params->UnitLength_in_cm / SQR(run_params->UnitTime_in_s);
    run_params->UnitCoolingRate_in_cgs = run_params->UnitPressure_in_cgs / run_params->UnitTime_in_s;
    run_params->UnitEnergy_in_cgs = run_params->UnitMass_in_g * SQR(run_params->UnitLength_in_cm) / SQR(run_params->UnitTime_in_s);

    /* Convert supernova parameters to code units */
    run_params->EnergySNcode = run_params->EnergySN / run_params->UnitEnergy_in_cgs * run_params->Hubble_h;
    run_params->EtaSNcode = run_params->EtaSN * (run_params->UnitMass_in_g / SOLAR_MASS) / run_params->Hubble_h;

    /* Set Hubble parameter in internal units */
    run_params->Hubble = HUBBLE * run_params->UnitTime_in_s;

    /* Compute critical density */
    run_params->RhoCrit = 3.0 * run_params->Hubble * run_params->Hubble / (8 * M_PI * run_params->G);
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
    run_params->Age = mymalloc(ABSOLUTEMAXSNAPS*sizeof(run_params->Age[0]));
    
    /* Read list of snapshots from file */
    read_snap_list(run_params);

    /* Initialize ages array with hack to fix deltaT for snapshot 0 */
    run_params->Age[0] = time_to_present(1000.0, run_params); /* lookback time from z=1000 */
    run_params->Age++;

    /* Calculate redshift and age for each snapshot */
    for(int i = 0; i < run_params->Snaplistlen; i++) {
        run_params->ZZ[i] = 1 / run_params->AA[i] - 1;
        run_params->Age[i] = time_to_present(run_params->ZZ[i], run_params);
    }

    /* Set reionization parameters */
    run_params->a0 = 1.0 / (1.0 + run_params->Reionization_z0);
    run_params->ar = 1.0 / (1.0 + run_params->Reionization_zr);
}

/*
 * Cleanup simulation times resources
 * 
 * Frees the memory allocated for the Age array.
 */
void cleanup_simulation_times(struct params *run_params)
{
    /* Reset Age pointer to its original allocation address */
    run_params->Age--;
    
    /* Free Age array */
    myfree(run_params->Age);
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
    cleanup_cooling();
    cleanup_simulation_times(run_params);
    cleanup_units(run_params);
}

/*
 * Initialize the galaxy evolution context
 * 
 * Sets up the context structure used during galaxy evolution with all
 * necessary pointers and initial values. This encapsulates the state
 * that was previously scattered throughout the evolve_galaxies function.
 */
void initialize_evolution_context(struct evolution_context *ctx, 
                                 const int halonr,
                                 struct GALAXY *galaxies, 
                                 const int ngal,
                                 struct halo_data *halos,
                                 struct params *run_params)
{
    if (ctx == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_evolution_context\n");
        return;
    }
    
    /* Initialize galaxy references */
    ctx->galaxies = galaxies;
    ctx->ngal = ngal;
    ctx->centralgal = galaxies[0].CentralGal;
    
    /* Initialize halo properties */
    ctx->halo_nr = halonr;
    ctx->halo_snapnum = halos[halonr].SnapNum;
    ctx->redshift = run_params->ZZ[ctx->halo_snapnum];
    ctx->halo_age = run_params->Age[ctx->halo_snapnum];
    
    /* Initialize time integration */
    ctx->deltaT = 0.0;  /* Will be set during evolution */
    
    /* Initialize parameters reference */
    ctx->params = run_params;
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
    const int ThisTask = run_params->ThisTask;
#endif

    char fname[MAX_STRING_LEN+1];

    snprintf(fname, MAX_STRING_LEN, "%s", run_params->FileWithSnapList);
    FILE *fd = fopen(fname, "r");
    if(fd == NULL) {
        fprintf(stderr, "can't read output list in file '%s'\n", fname);
        ABORT(0);
    }

    run_params->Snaplistlen = 0;
    do {
        if(fscanf(fd, " %lg ", &(run_params->AA[run_params->Snaplistlen])) == 1) {
            run_params->Snaplistlen++;
        } else {
            break;
        }
    } while(run_params->Snaplistlen < run_params->SimMaxSnaps);
    fclose(fd);

#ifdef VERBOSE
    if(ThisTask == 0) {
        fprintf(stdout, "found %d defined times in snaplist\n", run_params->Snaplistlen);
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
#ifdef GSL_FOUND
#define WORKSIZE 1000
    gsl_function F;
    gsl_integration_workspace *workspace;
    double abserr;

    workspace = gsl_integration_workspace_alloc(WORKSIZE);
    F.function = &integrand_time_to_present;
    F.params = run_params;

    gsl_integration_qag(&F, start_limit, end_limit, 1.0 / run_params->Hubble,
                        1.0e-9, WORKSIZE, GSL_INTEG_GAUSS21, workspace, &result, &abserr);

    gsl_integration_workspace_free(workspace);

#undef WORKSIZE
#else
    /* Do not have GSL - let's integrate numerically ourselves */
    const double step  = 1e-7;
    const int64_t nsteps = (end_limit - start_limit)/step;
    result = 0.0;
    const double y0 = integrand_time_to_present(start_limit + 0*step, run_params);
    const double yn = integrand_time_to_present(start_limit + nsteps*step, run_params);
    for(int64_t i=1; i<nsteps; i++) {
        result  += integrand_time_to_present(start_limit + i*step, run_params);
    }

    result = (step*0.5)*(y0 + yn + 2.0*result);
#endif

    /* convert into Myrs/h (I think -> MS 23/6/2018) */
    const double time = 1.0 / run_params->Hubble * result;

    // return time to present as a function of redshift
    return time;
}

/*
 * Integrand for time to present calculation
 */
double integrand_time_to_present(const double a, void *param)
{
    const struct params *run_params = (struct params *) param;
    return 1.0 / sqrt(run_params->Omega / a + (1.0 - run_params->Omega - run_params->OmegaLambda) + run_params->OmegaLambda * a * a);
}
