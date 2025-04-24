#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <stdbool.h>
#include "core_allvars.h"
#include "core_logging.h"
#include "core_event_system.h"
#include "core_pipeline_system.h"  /* Added for pipeline_execution_phase enum */

/**
 * Evolution diagnostics data structure
 * 
 * Contains runtime statistics for the galaxy evolution process.
 */
struct evolution_diagnostics {
    /* General stats */
    int halo_nr;                   /* Current halo number */
    int ngal_initial;              /* Initial number of galaxies */
    int ngal_final;                /* Final number of galaxies */
    
    /* Processing stats */
    clock_t start_time;            /* Start time of evolution */
    clock_t end_time;              /* End time of evolution */
    double elapsed_seconds;        /* Elapsed time in seconds */
    
    /* Phase statistics */
    struct {
        clock_t start_time;        /* Phase start time */
        clock_t total_time;        /* Total time spent in this phase */
        int galaxy_count;          /* Number of galaxies processed */
        int step_count;            /* Number of timesteps processed */
    } phases[4];                   /* HALO, GALAXY, POST, FINAL phases */
    
    /* Event statistics */
    int event_counts[EVENT_TYPE_MAX]; /* Counts of each event type */
    
    /* Merger statistics */
    int mergers_detected;          /* Number of potential mergers detected */
    int mergers_processed;         /* Number of mergers actually processed */
    int major_mergers;             /* Number of major mergers */
    int minor_mergers;             /* Number of minor mergers */
    
    /* Galaxy property statistics */
    double total_stellar_mass_initial;   /* Total stellar mass before evolution */
    double total_stellar_mass_final;     /* Total stellar mass after evolution */
    double total_cold_gas_initial;       /* Total cold gas before evolution */
    double total_cold_gas_final;         /* Total cold gas after evolution */
    double total_hot_gas_initial;        /* Total hot gas before evolution */
    double total_hot_gas_final;          /* Total hot gas after evolution */
    double total_bulge_mass_initial;     /* Total bulge mass before evolution */
    double total_bulge_mass_final;       /* Total bulge mass after evolution */
    
    /* Pipeline statistics */
    int pipeline_steps_executed;   /* Number of pipeline steps executed */
    int module_callbacks_executed; /* Number of module callbacks executed */
    
    /* Performance metrics */
    double galaxies_per_second;    /* Processing rate in galaxies per second */
};

/**
 * Initialize the diagnostics for a new evolution run
 * 
 * Sets up a diagnostics structure at the start of galaxy evolution.
 * 
 * @param diag Pointer to the diagnostics structure to initialize
 * @param halo_nr Current halo number
 * @param ngal Initial number of galaxies
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_initialize(struct evolution_diagnostics *diag, int halo_nr, int ngal);

/**
 * Update phase statistics when entering a new phase
 * 
 * Records the start time of a pipeline execution phase.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param phase The execution phase being started
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_start_phase(struct evolution_diagnostics *diag, enum pipeline_execution_phase phase);

/**
 * Update phase statistics when exiting a phase
 * 
 * Records the end time of a pipeline execution phase and updates
 * the total time spent in this phase.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param phase The execution phase being ended
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_end_phase(struct evolution_diagnostics *diag, enum pipeline_execution_phase phase);

/**
 * Track an event occurrence
 * 
 * Increments the counter for a specific event type.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param event_type Type of event that occurred
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_add_event(struct evolution_diagnostics *diag, event_type_t event_type);

/**
 * Record a merger detection
 * 
 * Updates merger statistics when a potential merger is detected.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param merger_type Type of merger detected (1=minor, 2=major)
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_add_merger_detection(struct evolution_diagnostics *diag, int merger_type);

/**
 * Record a merger being processed
 * 
 * Updates merger statistics when a merger is actually processed.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param merger_type Type of merger processed (1=minor, 2=major)
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_add_merger_processed(struct evolution_diagnostics *diag, int merger_type);

/**
 * Record initial galaxy property statistics
 * 
 * Computes and stores aggregate galaxy properties at the start of evolution.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param galaxies Array of galaxies
 * @param ngal Number of galaxies
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_record_initial_properties(struct evolution_diagnostics *diag, struct GALAXY *galaxies, int ngal);

/**
 * Record final galaxy property statistics
 * 
 * Computes and stores aggregate galaxy properties at the end of evolution.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param galaxies Array of galaxies
 * @param ngal Number of galaxies
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_record_final_properties(struct evolution_diagnostics *diag, struct GALAXY *galaxies, int ngal);

/**
 * Finalize the diagnostics and compute derived metrics
 * 
 * Calculates final statistics and derived metrics after evolution is complete.
 * 
 * @param diag Pointer to the diagnostics structure
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_finalize(struct evolution_diagnostics *diag);

/**
 * Report diagnostics to log
 * 
 * Outputs diagnostic information to the log using the specified log level.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param log_level Log level to use for reporting
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_report(struct evolution_diagnostics *diag, log_level_t log_level);

#ifdef __cplusplus
}
#endif