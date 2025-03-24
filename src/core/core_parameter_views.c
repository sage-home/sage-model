#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "core_allvars.h"
#include "core_parameter_views.h"
#include "core_logging.h"

/*
 * Validate cooling parameters view
 * 
 * Checks for internal consistency and numerical validity in cooling parameters.
 * Ensures critical parameters that will be used in calculations are not NaN or Infinity.
 * 
 * @param view The cooling parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_cooling_params_view(const struct cooling_params_view *view)
{
    if (view == NULL) {
        LOG_ERROR("Null pointer passed to validate_cooling_params_view");
        return false;
    }
    
    /* Check cosmology parameters */
    if (!isfinite(view->Omega)) {
        LOG_ERROR("Invalid Omega value in cooling params: %g", view->Omega);
        return false;
    }
    
    if (!isfinite(view->OmegaLambda)) {
        LOG_ERROR("Invalid OmegaLambda value in cooling params: %g", view->OmegaLambda);
        return false;
    }
    
    if (!isfinite(view->Hubble_h) || view->Hubble_h <= 0.0) {
        LOG_ERROR("Invalid Hubble_h value in cooling params: %g", view->Hubble_h);
        return false;
    }
    
    /* Check units - these must be positive numbers */
    if (!isfinite(view->UnitDensity_in_cgs) || view->UnitDensity_in_cgs <= 0.0) {
        LOG_ERROR("Invalid UnitDensity_in_cgs value in cooling params: %g", view->UnitDensity_in_cgs);
        return false;
    }
    
    if (!isfinite(view->UnitTime_in_s) || view->UnitTime_in_s <= 0.0) {
        LOG_ERROR("Invalid UnitTime_in_s value in cooling params: %g", view->UnitTime_in_s);
        return false;
    }
    
    if (!isfinite(view->UnitEnergy_in_cgs) || view->UnitEnergy_in_cgs <= 0.0) {
        LOG_ERROR("Invalid UnitEnergy_in_cgs value in cooling params: %g", view->UnitEnergy_in_cgs);
        return false;
    }
    
    if (!isfinite(view->UnitMass_in_g) || view->UnitMass_in_g <= 0.0) {
        LOG_ERROR("Invalid UnitMass_in_g value in cooling params: %g", view->UnitMass_in_g);
        return false;
    }
    
    /* Validate AGN parameters if AGN recipe is enabled */
    if (view->AGNrecipeOn != 0 && !isfinite(view->RadioModeEfficiency)) {
        LOG_ERROR("AGN recipe is enabled but RadioModeEfficiency is not finite: %g", view->RadioModeEfficiency);
        return false;
    }
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        LOG_ERROR("Null full_params reference in cooling params view");
        return false;
    }
    
    return true;
}

/*
 * Validate star formation parameters view
 * 
 * Checks for internal consistency and numerical validity in star formation parameters.
 * 
 * @param view The star formation parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_star_formation_params_view(const struct star_formation_params_view *view)
{
    if (view == NULL) {
        LOG_ERROR("Null pointer passed to validate_star_formation_params_view");
        return false;
    }
    
    /* Check SF prescription is a valid value */
    if (view->SFprescription < 0) {
        LOG_ERROR("Invalid SFprescription value in star formation params: %d", view->SFprescription);
        return false;
    }
    
    /* Check efficiency parameters are finite - no negative check as 0 could be intentional */
    if (!isfinite(view->SfrEfficiency)) {
        LOG_ERROR("Invalid SfrEfficiency value in star formation params: %g", view->SfrEfficiency);
        return false;
    }
    
    if (!isfinite(view->RecycleFraction)) {
        LOG_ERROR("Invalid RecycleFraction value in star formation params: %g", view->RecycleFraction);
        return false;
    }
    
    if (!isfinite(view->Yield)) {
        LOG_ERROR("Invalid Yield value in star formation params: %g", view->Yield);
        return false;
    }
    
    if (!isfinite(view->FracZleaveDisk)) {
        LOG_ERROR("Invalid FracZleaveDisk value in star formation params: %g", view->FracZleaveDisk);
        return false;
    }
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        LOG_ERROR("Null full_params reference in star formation params view");
        return false;
    }
    
    return true;
}

/*
 * Validate feedback parameters view
 * 
 * Checks for internal consistency and numerical validity in feedback parameters.
 * 
 * @param view The feedback parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_feedback_params_view(const struct feedback_params_view *view)
{
    if (view == NULL) {
        LOG_ERROR("Null pointer passed to validate_feedback_params_view");
        return false;
    }
    
    /* If supernova recipe is enabled, validate required parameters */
    if (view->SupernovaRecipeOn != 0) {
        if (!isfinite(view->FeedbackReheatingEpsilon)) {
            LOG_ERROR("Invalid FeedbackReheatingEpsilon value in feedback params: %g", view->FeedbackReheatingEpsilon);
            return false;
        }
        
        if (!isfinite(view->FeedbackEjectionEfficiency)) {
            LOG_ERROR("Invalid FeedbackEjectionEfficiency value in feedback params: %g", view->FeedbackEjectionEfficiency);
            return false;
        }
        
        if (!isfinite(view->EnergySNcode) || view->EnergySNcode <= 0.0) {
            LOG_ERROR("Invalid EnergySNcode value in feedback params: %g", view->EnergySNcode);
            return false;
        }
        
        if (!isfinite(view->EtaSNcode) || view->EtaSNcode <= 0.0) {
            LOG_ERROR("Invalid EtaSNcode value in feedback params: %g", view->EtaSNcode);
            return false;
        }
    }
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        LOG_ERROR("Null full_params reference in feedback params view");
        return false;
    }
    
    return true;
}

/*
 * Validate AGN parameters view
 * 
 * Checks for internal consistency and numerical validity in AGN parameters.
 * 
 * @param view The AGN parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_agn_params_view(const struct agn_params_view *view)
{
    if (view == NULL) {
        LOG_ERROR("Null pointer passed to validate_agn_params_view");
        return false;
    }
    
    /* If AGN recipe is enabled, validate required parameters */
    if (view->AGNrecipeOn != 0) {
        if (!isfinite(view->RadioModeEfficiency)) {
            LOG_ERROR("Invalid RadioModeEfficiency value in AGN params: %g", view->RadioModeEfficiency);
            return false;
        }
        
        if (!isfinite(view->QuasarModeEfficiency)) {
            LOG_ERROR("Invalid QuasarModeEfficiency value in AGN params: %g", view->QuasarModeEfficiency);
            return false;
        }
        
        if (!isfinite(view->BlackHoleGrowthRate)) {
            LOG_ERROR("Invalid BlackHoleGrowthRate value in AGN params: %g", view->BlackHoleGrowthRate);
            return false;
        }
    }
    
    /* Check units - these must be positive numbers */
    if (!isfinite(view->UnitMass_in_g) || view->UnitMass_in_g <= 0.0) {
        LOG_ERROR("Invalid UnitMass_in_g value in AGN params: %g", view->UnitMass_in_g);
        return false;
    }
    
    if (!isfinite(view->UnitTime_in_s) || view->UnitTime_in_s <= 0.0) {
        LOG_ERROR("Invalid UnitTime_in_s value in AGN params: %g", view->UnitTime_in_s);
        return false;
    }
    
    if (!isfinite(view->UnitEnergy_in_cgs) || view->UnitEnergy_in_cgs <= 0.0) {
        LOG_ERROR("Invalid UnitEnergy_in_cgs value in AGN params: %g", view->UnitEnergy_in_cgs);
        return false;
    }
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        LOG_ERROR("Null full_params reference in AGN params view");
        return false;
    }
    
    return true;
}

/*
 * Validate merger parameters view
 * 
 * Checks for internal consistency and numerical validity in merger parameters.
 * 
 * @param view The merger parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_merger_params_view(const struct merger_params_view *view)
{
    if (view == NULL) {
        LOG_ERROR("Null pointer passed to validate_merger_params_view");
        return false;
    }
    
    /* Check merger parameters are finite */
    if (!isfinite(view->ThreshMajorMerger)) {
        LOG_ERROR("Invalid ThreshMajorMerger value in merger params: %g", view->ThreshMajorMerger);
        return false;
    }
    
    if (!isfinite(view->ThresholdSatDisruption)) {
        LOG_ERROR("Invalid ThresholdSatDisruption value in merger params: %g", view->ThresholdSatDisruption);
        return false;
    }
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        LOG_ERROR("Null full_params reference in merger params view");
        return false;
    }
    
    return true;
}

/*
 * Validate reincorporation parameters view
 * 
 * Checks for internal consistency and numerical validity in reincorporation parameters.
 * 
 * @param view The reincorporation parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_reincorporation_params_view(const struct reincorporation_params_view *view)
{
    if (view == NULL) {
        LOG_ERROR("Null pointer passed to validate_reincorporation_params_view");
        return false;
    }
    
    /* Check reincorporation factor is finite */
    if (!isfinite(view->ReIncorporationFactor)) {
        LOG_ERROR("Invalid ReIncorporationFactor value in reincorporation params: %g", view->ReIncorporationFactor);
        return false;
    }
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        LOG_ERROR("Null full_params reference in reincorporation params view");
        return false;
    }
    
    return true;
}

/*
 * Validate disk instability parameters view
 * 
 * Checks for internal consistency and numerical validity in disk instability parameters.
 * 
 * @param view The disk instability parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_disk_instability_params_view(const struct disk_instability_params_view *view)
{
    if (view == NULL) {
        LOG_ERROR("Null pointer passed to validate_disk_instability_params_view");
        return false;
    }
    
    /* No numerical checks needed for disk instability flag */
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        LOG_ERROR("Null full_params reference in disk instability params view");
        return false;
    }
    
    return true;
}

/*
 * Initialize cooling parameters view
 * 
 * Extracts parameters needed for cooling calculations from the main params structure.
 */
void initialize_cooling_params_view(struct cooling_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        LOG_ERROR("Null pointer passed to initialize_cooling_params_view");
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
    
    /* Validate parameter view */
    if (!validate_cooling_params_view(view)) {
        LOG_WARNING("Cooling parameters validation failed");
    }
}

/*
 * Initialize star formation parameters view
 * 
 * Extracts parameters needed for star formation calculations from the main params structure.
 */
void initialize_star_formation_params_view(struct star_formation_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        LOG_ERROR("Null pointer passed to initialize_star_formation_params_view");
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
    
    /* Validate parameter view */
    if (!validate_star_formation_params_view(view)) {
        LOG_WARNING("Star formation parameters validation failed");
    }
}

/*
 * Initialize feedback parameters view
 * 
 * Extracts parameters needed for feedback calculations from the main params structure.
 */
void initialize_feedback_params_view(struct feedback_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        LOG_ERROR("Null pointer passed to initialize_feedback_params_view");
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
    
    /* Validate parameter view */
    if (!validate_feedback_params_view(view)) {
        LOG_WARNING("Feedback parameters validation failed");
    }
}

/*
 * Initialize AGN parameters view
 * 
 * Extracts parameters needed for AGN calculations from the main params structure.
 */
void initialize_agn_params_view(struct agn_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        LOG_ERROR("Null pointer passed to initialize_agn_params_view");
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
    
    /* Validate parameter view */
    if (!validate_agn_params_view(view)) {
        LOG_WARNING("AGN parameters validation failed");
    }
}

/*
 * Initialize merger parameters view
 * 
 * Extracts parameters needed for merger calculations from the main params structure.
 */
void initialize_merger_params_view(struct merger_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        LOG_ERROR("Null pointer passed to initialize_merger_params_view");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->ThreshMajorMerger = params->physics.ThreshMajorMerger;
    view->ThresholdSatDisruption = params->physics.ThresholdSatDisruption;
    
    /* Store reference to full params */
    view->full_params = params;
    
    /* Validate parameter view */
    if (!validate_merger_params_view(view)) {
        LOG_WARNING("Merger parameters validation failed");
    }
}

/*
 * Initialize reincorporation parameters view
 * 
 * Extracts parameters needed for reincorporation calculations from the main params structure.
 */
void initialize_reincorporation_params_view(struct reincorporation_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        LOG_ERROR("Null pointer passed to initialize_reincorporation_params_view");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->ReIncorporationFactor = params->physics.ReIncorporationFactor;
    
    /* Store reference to full params */
    view->full_params = params;
    
    /* Validate parameter view */
    if (!validate_reincorporation_params_view(view)) {
        LOG_WARNING("Reincorporation parameters validation failed");
    }
}

/*
 * Initialize disk instability parameters view
 * 
 * Extracts parameters needed for disk instability calculations from the main params structure.
 */
void initialize_disk_instability_params_view(struct disk_instability_params_view *view, const struct params *params)
{
    if (view == NULL || params == NULL) {
        LOG_ERROR("Null pointer passed to initialize_disk_instability_params_view");
        return;
    }
    
    /* Copy relevant physics parameters */
    view->DiskInstabilityOn = params->physics.DiskInstabilityOn;
    
    /* Store reference to full params */
    view->full_params = params;
    
    /* Validate parameter view */
    if (!validate_disk_instability_params_view(view)) {
        LOG_WARNING("Disk instability parameters validation failed");
    }
}

/*
 * Validate logging parameters view
 * 
 * Checks for internal consistency and validates logging configuration.
 * 
 * @param view The logging parameter view to validate
 * @return true if validation passed, false if any issues found
 */
bool validate_logging_params_view(const struct logging_params_view *view)
{
    if (view == NULL) {
        /* Can't log with logging system yet */
        fprintf(stderr, "Error: Null pointer passed to validate_logging_params_view\n");
        return false;
    }
    
    /* Check that log level is in valid range */
    if (view->min_level < LOG_LEVEL_DEBUG || view->min_level > LOG_LEVEL_FATAL) {
        fprintf(stderr, "Error: Invalid minimum log level: %d\n", view->min_level);
        return false;
    }
    
    /* Check that prefix style is in valid range */
    if (view->prefix_style < LOG_PREFIX_NONE || view->prefix_style > LOG_PREFIX_DETAILED) {
        fprintf(stderr, "Error: Invalid log prefix style: %d\n", view->prefix_style);
        return false;
    }
    
    /* Check that at least one destination is set */
    if ((view->destinations & (LOG_DEST_STDOUT | LOG_DEST_STDERR | LOG_DEST_FILE)) == 0) {
        fprintf(stderr, "Error: No log destinations specified\n");
        return false;
    }
    
    /* If file destination is set, check that path is non-empty */
    if ((view->destinations & LOG_DEST_FILE) && view->log_file_path[0] == '\0') {
        fprintf(stderr, "Error: Log file destination set but no file path provided\n");
        return false;
    }
    
    /* Check for null reference to full parameters */
    if (view->full_params == NULL) {
        fprintf(stderr, "Error: Null full_params reference in logging params view\n");
        return false;
    }
    
    return true;
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
    
    /* Validate parameter view (using fprintf instead of LOG functions since logging might not be initialized yet) */
    if (!validate_logging_params_view(view)) {
        fprintf(stderr, "Warning: Logging parameters validation failed\n");
    }
}
