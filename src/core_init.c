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


/* These functions do not need to be exposed externally */
double integrand_time_to_present(const double a, void *param);
void set_units(struct params *run_params);
void read_snap_list(struct params *run_params);
double time_to_present(const double z, struct params *run_params);

void init(struct params *run_params)
{
#ifdef VERBOSE
    const int ThisTask = run_params->ThisTask;
#endif

    run_params->Age = mymalloc(ABSOLUTEMAXSNAPS*sizeof(run_params->Age[0]));
    
    // Initialize H2 fraction parameters with default values if not set and H2-based SF is enabled
    if(run_params->SFprescription == 1) {
        if(run_params->H2FractionFactor <= 0.0) {
            run_params->H2FractionFactor = 1.0;  // Default normalization factor for H2/HI ratio
        }
        if(run_params->H2FractionExponent <= 0.0) {
            run_params->H2FractionExponent = 0.92;  // Default power-law exponent for pressure relation
        }
    }

    // Initialize Krumholz & Dekel (2012) model parameters with default values if not set
    if(run_params->SFprescription == 2) {
        if(run_params->ClumpFactor <= 0.0) {
            run_params->ClumpFactor = 5.0;  // Default clumping factor
        }
        if(run_params->ClumpExponent <= 0.0) {
            run_params->ClumpExponent = 0.5;  // Default metallicity dependence exponent
        }
    }

    // Gnedin & Draine (2014) model parameters 
    if(run_params->SFprescription == 3) {
        if(run_params->RadiationFieldNorm <= 0.0) {
            run_params->RadiationFieldNorm = 0.5;  // Lower now, was 1.0
        }
        if(run_params->MetallicityExponent <= 0.0) {
            run_params->MetallicityExponent = 0.7;  // Default metallicity scaling exponent
        }
        if(run_params->IntegrationBins <= 0) {
            run_params->IntegrationBins = 30;  // Default number of radial bins
        }
    }
    
    // Initialize environmental effects parameters
    if (run_params->EnvironmentalEffectsOn != 0 && run_params->EnvironmentalEffectsOn != 1) {
        run_params->EnvironmentalEffectsOn = 1;  // Enable by default
    }
    
    if (run_params->EnvEffectStrength <= 0.0) {
        run_params->EnvEffectStrength = 1.0;  // Default strength
    }

    // Initialize preventative feedback parameters with default values if not set
    if (run_params->CGMBuildingOn != 0 && run_params->CGMBuildingOn != 1) {
        run_params->CGMBuildingOn = 1;  // Enable by default
    }
    if (run_params->CGMBuildingVcrit <= 0.0) {
        run_params->CGMBuildingVcrit = 30.0;  // Default critical velocity in km/s
    }
    if (run_params->CGMBuildingAlpha <= 0.0) {
        run_params->CGMBuildingAlpha = 2.0;  // Default power-law index
    }
    if (run_params->CGMBuildingZdep <= 0.0) {
        run_params->CGMBuildingZdep = 0.5;  // Default redshift dependence
    }

    if (run_params->MassLoadingModel != 0 && 
        run_params->MassLoadingModel != 1 && 
        run_params->MassLoadingModel != 2) {
        run_params->MassLoadingModel = 0;  // Default to standard model
    }

    // Initialize redshift reincorporation parameters with default values if not set
    if (run_params->RedshiftReincorporationOn != 0 && run_params->RedshiftReincorporationOn != 1) {
        run_params->RedshiftReincorporationOn = 0;  // Disabled by default
    }
    if (run_params->ReincorporationRedshiftExp <= 0.0) {
        run_params->ReincorporationRedshiftExp = 2.0;  // Default exponent value
    }

    // Initialize low-mass high-z suppression parameters
    if (run_params->LowMassHighzSuppressionOn != 0 && run_params->LowMassHighzSuppressionOn != 1) {
        run_params->LowMassHighzSuppressionOn = 0;  // Disabled by default
    }
    if (run_params->SuppressionMassThreshold <= 0.0) {
        run_params->SuppressionMassThreshold = 0.3;  // Default: 3×10^9 Msun/h
    }
    if (run_params->SuppressionMassExponent <= 0.0) {
        run_params->SuppressionMassExponent = 4.0;  // Default: sharp transition
    }
    if (run_params->SuppressionRedshiftExp <= 0.0) {
        run_params->SuppressionRedshiftExp = 1.0;  // Default: linear with redshift
    }

    // Initialize reionization parameters with default values
    if (run_params->ReionizationModel != 0 && 
        run_params->ReionizationModel != 1 && 
        run_params->ReionizationModel != 2 && 
        run_params->ReionizationModel != 3) {
        run_params->ReionizationModel = 0;  // Default to simple model
    }

    if (run_params->FilteringMassNorm <= 0.0) {
        run_params->FilteringMassNorm = 0.7;  // Based on Okamoto et al. (2008)
    }

    if (run_params->UVBackgroundStrength < 0.0) {
        run_params->UVBackgroundStrength = 0.1;  // Default mild pre-reionization UV 
    }

    if (run_params->PatchyReionWidth <= 0.0) {
        run_params->PatchyReionWidth = 1.0;  // Default reionization width dz
    }

    if (run_params->LocalReionVariance < 0.0) {
        run_params->LocalReionVariance = 0.5;  // Default variance strength
    }

    if (run_params->PostReionSlope <= 0.0) {
        run_params->PostReionSlope = -2.0/3.0;  // Default from Sobacchi & Mesinger (2013)
    }

    // Initialize mass-dependent SF parameters with default values if not set
    if (run_params->MassDependentSFEnabled != 0 && run_params->MassDependentSFEnabled != 1) {
        run_params->MassDependentSFEnabled = 0;  // Disabled by default
    }
    
    if (run_params->SFMassPivot <= 0.0) {
        run_params->SFMassPivot = 1.0;  // Default pivot mass (10^10 M_sun/h)
    }
    
    if (run_params->SFLowMassSlope <= 0.0) {
        run_params->SFLowMassSlope = 0.5;  // Default low-mass slope
    }
    
    if (run_params->SFHighMassSlope >= 0.0) {
        run_params->SFHighMassSlope = -0.3;  // Default high-mass slope (negative)
    }

    set_units(run_params);

    read_snap_list(run_params);

    //Hack to fix deltaT for snapshot 0
    //This way, galsnapnum = -1 will not segfault.
    run_params->Age[0] = time_to_present(1000.0, run_params);//lookback time from z=1000
    run_params->Age++;

    for(int i = 0; i < run_params->Snaplistlen; i++) {
        run_params->ZZ[i] = 1 / run_params->AA[i] - 1;
        run_params->Age[i] = time_to_present(run_params->ZZ[i], run_params);
    }

    run_params->a0 = 1.0 / (1.0 + run_params->Reionization_z0);
    run_params->ar = 1.0 / (1.0 + run_params->Reionization_zr);

    read_cooling_functions();
#ifdef VERBOSE
    if(ThisTask == 0) {
        fprintf(stdout, "cooling functions read\n\n");
    }
#endif
}



void set_units(struct params *run_params)
{

    run_params->UnitTime_in_s = run_params->UnitLength_in_cm / run_params->UnitVelocity_in_cm_per_s;
    run_params->UnitTime_in_Megayears = run_params->UnitTime_in_s / SEC_PER_MEGAYEAR;
    run_params->G = GRAVITY / CUBE(run_params->UnitLength_in_cm) * run_params->UnitMass_in_g * SQR(run_params->UnitTime_in_s);
    run_params->UnitDensity_in_cgs = run_params->UnitMass_in_g / CUBE(run_params->UnitLength_in_cm);
    run_params->UnitPressure_in_cgs = run_params->UnitMass_in_g / run_params->UnitLength_in_cm / SQR(run_params->UnitTime_in_s);
    run_params->UnitCoolingRate_in_cgs = run_params->UnitPressure_in_cgs / run_params->UnitTime_in_s;
    run_params->UnitEnergy_in_cgs = run_params->UnitMass_in_g * SQR(run_params->UnitLength_in_cm) / SQR(run_params->UnitTime_in_s);

    run_params->EnergySNcode = run_params->EnergySN / run_params->UnitEnergy_in_cgs * run_params->Hubble_h;
    run_params->EtaSNcode = run_params->EtaSN * (run_params->UnitMass_in_g / SOLAR_MASS) / run_params->Hubble_h;

    // convert some physical input parameters to internal units
    run_params->Hubble = HUBBLE * run_params->UnitTime_in_s;

    // compute a few quantitites
    run_params->RhoCrit = 3.0 * run_params->Hubble * run_params->Hubble / (8 * M_PI * run_params->G);
}



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

double integrand_time_to_present(const double a, void *param)
{
    const struct params *run_params = (struct params *) param;
    return 1.0 / sqrt(run_params->Omega / a + (1.0 - run_params->Omega - run_params->OmegaLambda) + run_params->OmegaLambda * a * a);
}
