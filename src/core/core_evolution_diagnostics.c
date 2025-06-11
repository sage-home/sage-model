#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core_evolution_diagnostics.h"
#include "core_logging.h"
#include "core_allvars.h"
#include "core_pipeline_system.h"

/**
 * Initialize the diagnostics for a new evolution run
 */
int core_evolution_diagnostics_initialize(struct core_evolution_diagnostics *diag, int halo_nr, int ngal) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to core_evolution_diagnostics_initialize");
        return -1;
    }
    
    /* Clear the structure */
    memset(diag, 0, sizeof(struct core_evolution_diagnostics));
    
    /* Set basic information */
    diag->halo_nr = halo_nr;
    diag->ngal_initial = ngal;
    diag->ngal_final = 0; /* Will be set during finalization */
    
    /* Initialize timing */
    diag->start_time = clock();
    diag->end_time = 0;
    diag->elapsed_seconds = 0.0;
    
    /* Initialize phase statistics */
    for (int i = 0; i < 4; i++) {
        diag->phases[i].start_time = 0;
        diag->phases[i].total_time = 0;
        diag->phases[i].galaxy_count = 0;
        diag->phases[i].step_count = 0;
    }
    
    /* Initialize core event statistics */
    for (int i = 0; i < CORE_EVENT_TYPE_MAX; i++) {
        diag->core_event_counts[i] = 0;
    }
    
    /* Initialize merger statistics */
    diag->mergers_detected = 0;
    diag->mergers_processed = 0;
    diag->major_mergers = 0;
    diag->minor_mergers = 0;
    
    /* Initialize pipeline statistics */
    diag->pipeline_steps_executed = 0;
    diag->module_callbacks_executed = 0;
    
    /* Interval-based debug logging (first 5 initializations only) */
    static int init_debug_count = 0;
    init_debug_count++;
    if (init_debug_count <= 5) {
        if (init_debug_count == 5) {
            LOG_DEBUG("Initialized core evolution diagnostics for halo %d with %d galaxies (init #%d - further messages suppressed)", halo_nr, ngal, init_debug_count);
        } else {
            LOG_DEBUG("Initialized core evolution diagnostics for halo %d with %d galaxies (init #%d)", halo_nr, ngal, init_debug_count);
        }
    }
    
    return 0;
}

/**
 * Convert pipeline phase bit flag to array index
 * 
 * Maps pipeline execution phase flags to zero-based array indices.
 * 
 * @param phase Pipeline execution phase bit flag (1, 2, 4, or 8)
 * @return Array index (0-3) or -1 if invalid
 */
static int phase_to_index(enum pipeline_execution_phase phase) {
    switch (phase) {
        case PIPELINE_PHASE_HALO:   return 0;
        case PIPELINE_PHASE_GALAXY: return 1;
        case PIPELINE_PHASE_POST:   return 2;
        case PIPELINE_PHASE_FINAL:  return 3;
        default:                    return -1;
    }
}

/**
 * Update phase statistics when entering a new phase
 */
int core_evolution_diagnostics_start_phase(struct core_evolution_diagnostics *diag, enum pipeline_execution_phase phase) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to core_evolution_diagnostics_start_phase");
        return -1;
    }
    
    /* Convert phase bit flag to array index */
    int phase_index = phase_to_index(phase);
    if (phase_index < 0) {
        LOG_ERROR("Invalid phase %d passed to core_evolution_diagnostics_start_phase", phase);
        return -1;
    }
    
    /* Record start time for this phase */
    diag->phases[phase_index].start_time = clock();
    
    /* Interval-based debug logging (first 5 phase starts only) */
    static int phase_start_debug_count = 0;
    phase_start_debug_count++;
    if (phase_start_debug_count <= 5) {
        if (phase_start_debug_count == 5) {
            LOG_DEBUG("Starting phase %d for halo %d (start #%d - further messages suppressed)", phase, diag->halo_nr, phase_start_debug_count);
        } else {
            LOG_DEBUG("Starting phase %d for halo %d (start #%d)", phase, diag->halo_nr, phase_start_debug_count);
        }
    }
    
    return 0;
}

/**
 * Update phase statistics when exiting a phase
 */
int core_evolution_diagnostics_end_phase(struct core_evolution_diagnostics *diag, enum pipeline_execution_phase phase) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to core_evolution_diagnostics_end_phase");
        return -1;
    }
    
    /* Convert phase bit flag to array index */
    int phase_index = phase_to_index(phase);
    if (phase_index < 0) {
        LOG_ERROR("Invalid phase %d passed to core_evolution_diagnostics_end_phase", phase);
        return -1;
    }
    
    /* Calculate time spent in this phase */
    clock_t end_time = clock();
    if (diag->phases[phase_index].start_time == 0) {
        LOG_WARNING("Phase %d was never started", phase);
        return -1;
    }
    
    diag->phases[phase_index].total_time += (end_time - diag->phases[phase_index].start_time);
    diag->phases[phase_index].step_count++;
    
    /* Interval-based debug logging (first 5 phase ends only) */
    static int phase_end_debug_count = 0;
    phase_end_debug_count++;
    if (phase_end_debug_count <= 5) {
        if (phase_end_debug_count == 5) {
            LOG_DEBUG("Ending phase %d for halo %d, step %d (end #%d - further messages suppressed)", phase, diag->halo_nr, diag->phases[phase_index].step_count, phase_end_debug_count);
        } else {
            LOG_DEBUG("Ending phase %d for halo %d, step %d (end #%d)", phase, diag->halo_nr, diag->phases[phase_index].step_count, phase_end_debug_count);
        }
    }
    
    return 0;
}

/**
 * Track a core infrastructure event occurrence
 */
int core_evolution_diagnostics_add_event(struct core_evolution_diagnostics *diag, core_event_type_t event_type) {
    if (diag == NULL) {
        LOG_DEBUG("NULL diagnostics pointer provided to core_evolution_diagnostics_add_event");
        return -1;
    }
    
    if (event_type < 0 || event_type >= CORE_EVENT_TYPE_MAX) {
        LOG_ERROR("Invalid core event type %d passed to core_evolution_diagnostics_add_event", event_type);
        return -1;
    }
    
    /* Increment counter for this core event type */
    diag->core_event_counts[event_type]++;
    
    LOG_DEBUG("Added core event of type %d to diagnostics for halo %d", event_type, diag->halo_nr);
    
    return 0;
}

/**
 * Record a merger detection
 */
int core_evolution_diagnostics_add_merger_detection(struct core_evolution_diagnostics *diag, int merger_type) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to core_evolution_diagnostics_add_merger_detection");
        return -1;
    }
    
    /* Increment merger detection counter */
    diag->mergers_detected++;
    
    /* Track merger type (1=minor, 2=major) */
    if (merger_type == 1) {
        diag->minor_mergers++;
    } else if (merger_type == 2) {
        diag->major_mergers++;
    }
    
    LOG_DEBUG("Added merger detection of type %d to diagnostics for halo %d", merger_type, diag->halo_nr);
    
    return 0;
}

/**
 * Record a merger being processed
 */
int core_evolution_diagnostics_add_merger_processed(struct core_evolution_diagnostics *diag, int merger_type) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to core_evolution_diagnostics_add_merger_processed");
        return -1;
    }
    
    /* Increment merger processed counter */
    diag->mergers_processed++;
    
    LOG_DEBUG("Added merger processing of type %d to diagnostics for halo %d", merger_type, diag->halo_nr);
    
    return 0;
}

/**
 * Finalize the diagnostics and compute derived metrics
 */
int core_evolution_diagnostics_finalize(struct core_evolution_diagnostics *diag) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to core_evolution_diagnostics_finalize");
        return -1;
    }
    
    /* Record end time and calculate elapsed time */
    diag->end_time = clock();
    diag->elapsed_seconds = ((double)(diag->end_time - diag->start_time)) / CLOCKS_PER_SEC;
    
    /* Calculate performance metrics */
    if (diag->elapsed_seconds > 0.0) {
        diag->galaxies_per_second = diag->ngal_initial / diag->elapsed_seconds;
    } else {
        diag->galaxies_per_second = 0.0;
    }
    
    /* Interval-based debug logging (first 5 finalizations only) */
    static int finalize_debug_count = 0;
    finalize_debug_count++;
    if (finalize_debug_count <= 5) {
        if (finalize_debug_count == 5) {
            LOG_DEBUG("Finalized core diagnostics for halo %d, elapsed time: %.3f seconds (finalize #%d - further messages suppressed)", 
                      diag->halo_nr, diag->elapsed_seconds, finalize_debug_count);
        } else {
            LOG_DEBUG("Finalized core diagnostics for halo %d, elapsed time: %.3f seconds (finalize #%d)", 
                      diag->halo_nr, diag->elapsed_seconds, finalize_debug_count);
        }
    }
    
    return 0;
}

/**
 * Get the name of a core event type
 * 
 * Returns a string description of a core event type.
 * 
 * @param type Core event type to describe
 * @return String description of the core event type
 */
static const char *core_event_type_name(core_event_type_t type) {
    switch (type) {
        case CORE_EVENT_PIPELINE_STARTED:    return "PIPELINE_STARTED";
        case CORE_EVENT_PIPELINE_COMPLETED:  return "PIPELINE_COMPLETED";
        case CORE_EVENT_PHASE_STARTED:       return "PHASE_STARTED";
        case CORE_EVENT_PHASE_COMPLETED:     return "PHASE_COMPLETED";
        case CORE_EVENT_GALAXY_CREATED:      return "GALAXY_CREATED";
        case CORE_EVENT_GALAXY_COPIED:       return "GALAXY_COPIED";
        case CORE_EVENT_GALAXY_MERGED:       return "GALAXY_MERGED";
        case CORE_EVENT_MODULE_ACTIVATED:    return "MODULE_ACTIVATED";
        case CORE_EVENT_MODULE_DEACTIVATED:  return "MODULE_DEACTIVATED";
        default:                             return "UNKNOWN";
    }
}

/**
 * Report diagnostics to log
 */
int core_evolution_diagnostics_report(struct core_evolution_diagnostics *diag, log_level_t log_level) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to core_evolution_diagnostics_report");
        return -1;
    }
    
    /* Log basic information */
    if (log_level == LOG_LEVEL_DEBUG) {
        LOG_DEBUG("=== Core Evolution Diagnostics for Halo %d ===", diag->halo_nr);
        LOG_DEBUG("Galaxies: Initial=%d, Final=%d", diag->ngal_initial, diag->ngal_final);
        LOG_DEBUG("Processing Time: %.3f seconds (%.1f galaxies/second)", 
                diag->elapsed_seconds, diag->galaxies_per_second);
        
        /* Log phase statistics */
        LOG_DEBUG("--- Phase Statistics ---");
        const char *phase_names[] = {"HALO", "GALAXY", "POST", "FINAL"};
        for (int i = 0; i < 4; i++) {
            double phase_seconds = ((double)diag->phases[i].total_time) / CLOCKS_PER_SEC;
            double phase_percent = (diag->elapsed_seconds > 0.0) 
                                ? (phase_seconds / diag->elapsed_seconds) * 100.0
                                : 0.0;
            
            LOG_DEBUG("Phase %s: %.3f seconds (%.1f%%), %d steps, %d galaxies",
                    phase_names[i], phase_seconds, phase_percent,
                    diag->phases[i].step_count, diag->phases[i].galaxy_count);
        }
        
        /* Log merger statistics */
        LOG_DEBUG("--- Merger Statistics ---");
        LOG_DEBUG("Mergers: Detected=%d, Processed=%d (Major=%d, Minor=%d)",
                diag->mergers_detected, diag->mergers_processed,
                diag->major_mergers, diag->minor_mergers);
        
        /* Log core event statistics if there were any events */
        int total_core_events = 0;
        for (int i = 0; i < CORE_EVENT_TYPE_MAX; i++) {
            total_core_events += diag->core_event_counts[i];
        }
        
        if (total_core_events > 0) {
            LOG_DEBUG("--- Core Event Statistics ---");
            for (int i = 0; i < CORE_EVENT_TYPE_MAX; i++) {
                if (diag->core_event_counts[i] > 0) {
                    LOG_DEBUG("Core Event %s: %d occurrences",
                            core_event_type_name((core_event_type_t)i), diag->core_event_counts[i]);
                }
            }
        }
        
        /* Log pipeline statistics */
        LOG_DEBUG("--- Pipeline Statistics ---");
        LOG_DEBUG("Pipeline Steps Executed: %d", diag->pipeline_steps_executed);
        LOG_DEBUG("Module Callbacks Executed: %d", diag->module_callbacks_executed);
        
        LOG_DEBUG("=====================================");
    }
    else if (log_level == LOG_LEVEL_INFO) {
        /* Reduce verbosity by only logging summary information at INFO level */
        /* For detailed per-halo information, use DEBUG level instead */
    }
    else if (log_level == LOG_LEVEL_WARNING) {
        LOG_WARNING("=== Core Evolution Diagnostics for Halo %d ===", diag->halo_nr);
        LOG_WARNING("Galaxies: Initial=%d, Final=%d", diag->ngal_initial, diag->ngal_final);
        LOG_WARNING("Processing Time: %.3f seconds", diag->elapsed_seconds);
        
        /* Only log merger statistics for warnings */
        LOG_WARNING("Mergers: Detected=%d, Processed=%d", 
                  diag->mergers_detected, diag->mergers_processed);
        
        LOG_WARNING("=====================================");
    }
    else {
        /* Fall back to ERROR level with minimal output */
        LOG_ERROR("Core Evolution Diagnostics for Halo %d: %d->%d galaxies, %.3f seconds",
                diag->halo_nr, diag->ngal_initial, diag->ngal_final, diag->elapsed_seconds);
    }
    
    return 0;
}
