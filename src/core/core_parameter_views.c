#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_allvars.h"
#include "core_parameter_views.h"
#include "core_logging.h"

/*
 * Initialize cooling parameters view
 * 
 * Extracts parameters needed for cooling calculations from the main params structure.
 */
void initialize_cooling_params_view(struct cooling_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_cooling_params_view\n");
        return;
    }
    
    /* Copy relevant cosmology parameters */
    view->Omega = params->cosmology.Omega;
    view->OmegaLambda = params->cosmology.OmegaLambda;
    view->Hubble_h = params->cosmology.Hubble_h;
    
    /* Copy relevant physics parameters */
    view->AGNrecipeOn = params->physics.AGNrecipeOn;
    view->RadioModeEfficiency = params->physics.RadioModeEfficiency;
    
    /* Copy relevant unit parameters */
    view->UnitDensity_in_cgs = params->units.UnitDensity_in_cgs;
    view->UnitTime_in_s = params->units.UnitTime_in_s;
    view->UnitEnergy_in_cgs = params->units.UnitEnergy_in_cgs;
    view->UnitMass_in_g = params->units.UnitMass_in_g;
    
    /* Store reference to full params */
    view->full_params = params;
}

/*
 * Initialize star formation parameters view
 * 
 * Extracts parameters needed for star formation calculations from the main params structure.
 */
void initialize_star_formation_params_view(struct star_formation_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_star_formation_params_view\n");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->SFprescription = params->physics.SFprescription;
    view->SfrEfficiency = params->physics.SfrEfficiency;
    view->RecycleFraction = params->physics.RecycleFraction;
    view->Yield = params->physics.Yield;
    view->FracZleaveDisk = params->physics.FracZleaveDisk;
    
    /* Store reference to full params */
    view->full_params = params;
}

/*
 * Initialize feedback parameters view
 * 
 * Extracts parameters needed for feedback calculations from the main params structure.
 */
void initialize_feedback_params_view(struct feedback_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_feedback_params_view\n");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->SupernovaRecipeOn = params->physics.SupernovaRecipeOn;
    view->FeedbackReheatingEpsilon = params->physics.FeedbackReheatingEpsilon;
    view->FeedbackEjectionEfficiency = params->physics.FeedbackEjectionEfficiency;
    view->EnergySNcode = params->physics.EnergySNcode;
    view->EtaSNcode = params->physics.EtaSNcode;
    
    /* Store reference to full params */
    view->full_params = params;
}

/*
 * Initialize AGN parameters view
 * 
 * Extracts parameters needed for AGN calculations from the main params structure.
 */
void initialize_agn_params_view(struct agn_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_agn_params_view\n");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->AGNrecipeOn = params->physics.AGNrecipeOn;
    view->RadioModeEfficiency = params->physics.RadioModeEfficiency;
    view->QuasarModeEfficiency = params->physics.QuasarModeEfficiency;
    view->BlackHoleGrowthRate = params->physics.BlackHoleGrowthRate;
    
    /* Copy relevant unit parameters */
    view->UnitMass_in_g = params->units.UnitMass_in_g;
    view->UnitTime_in_s = params->units.UnitTime_in_s;
    view->UnitEnergy_in_cgs = params->units.UnitEnergy_in_cgs;
    
    /* Store reference to full params */
    view->full_params = params;
}

/*
 * Initialize merger parameters view
 * 
 * Extracts parameters needed for merger calculations from the main params structure.
 */
void initialize_merger_params_view(struct merger_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_merger_params_view\n");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->ThreshMajorMerger = params->physics.ThreshMajorMerger;
    view->ThresholdSatDisruption = params->physics.ThresholdSatDisruption;
    
    /* Store reference to full params */
    view->full_params = params;
}

/*
 * Initialize reincorporation parameters view
 * 
 * Extracts parameters needed for reincorporation calculations from the main params structure.
 */
void initialize_reincorporation_params_view(struct reincorporation_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_reincorporation_params_view\n");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->ReIncorporationFactor = params->physics.ReIncorporationFactor;
    
    /* Store reference to full params */
    view->full_params = params;
}

/*
 * Initialize disk instability parameters view
 * 
 * Extracts parameters needed for disk instability calculations from the main params structure.
 */
void initialize_disk_instability_params_view(struct disk_instability_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_disk_instability_params_view\n");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->DiskInstabilityOn = params->physics.DiskInstabilityOn;
    
    /* Store reference to full params */
    view->full_params = params;
}

/*
 * Initialize logging parameters view
 * 
 * Extracts parameters needed for logging configuration from the main params structure.
 */
void initialize_logging_params_view(struct logging_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_logging_params_view\n");
        return;
    }
    
    /* Set default logging parameters */
    /* Always use LOG_LEVEL_INFO to ensure critical informational messages get through */
    view->min_level = LOG_LEVEL_INFO;
    
#ifdef VERBOSE
    view->prefix_style = LOG_PREFIX_DETAILED;  /* Detailed prefix with context information */
    /* Set output destinations - in verbose mode, show everything */
    view->destinations = LOG_DEST_STDERR;
#else
    view->prefix_style = LOG_PREFIX_SIMPLE;    /* Simple prefix */
    /* In non-verbose mode, only show errors and warnings on stderr */
    view->destinations = LOG_DEST_STDERR;
#endif
    
    /* Set output destinations - always use stderr for errors */
    view->destinations = LOG_DEST_STDERR;
    
    /* No log file by default */
    view->log_file_path[0] = '\0';
    
    /* Include MPI rank in messages when running with MPI */
#ifdef MPI
    view->include_mpi_rank = true;
#else
    view->include_mpi_rank = false;
#endif
    
    /* Enable context information in logs */
    view->include_extra_context = true;
    
    /* Enable assertions in debug builds, disable in release */
#ifdef NDEBUG
    view->disable_assertions = true;
#else
    view->disable_assertions = false;
#endif
    
    /* Store reference to full params */
    view->full_params = params;
}
