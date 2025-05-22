#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core_evolution_diagnostics.h"
#include "core_logging.h"
#include "core_allvars.h"
#include "core_event_system.h"
#include "core_pipeline_system.h"
#include "core_galaxy_accessors.h" // For property accessor functions
#include "core_property_utils.h"   // For generic property accessors
#include "core_properties.h"       // For property definitions

/**
 * Initialize the diagnostics for a new evolution run
 */
int evolution_diagnostics_initialize(struct evolution_diagnostics *diag, int halo_nr, int ngal) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to evolution_diagnostics_initialize");
        return -1;
    }
    
    /* Clear the structure */
    memset(diag, 0, sizeof(struct evolution_diagnostics));
    
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
    
    /* Initialize event statistics */
    for (int i = 0; i < EVENT_TYPE_MAX; i++) {
        diag->event_counts[i] = 0;
    }
    
    /* Initialize merger statistics */
    diag->mergers_detected = 0;
    diag->mergers_processed = 0;
    diag->major_mergers = 0;
    diag->minor_mergers = 0;
    
    /* Initialize pipeline statistics */
    diag->pipeline_steps_executed = 0;
    diag->module_callbacks_executed = 0;
    
    LOG_DEBUG("Initialized evolution diagnostics for halo %d with %d galaxies", halo_nr, ngal);
    
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
 * 
 * Records the start time of a pipeline execution phase.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param phase The execution phase being started (PIPELINE_PHASE_HALO, PIPELINE_PHASE_GALAXY, 
 *              PIPELINE_PHASE_POST, or PIPELINE_PHASE_FINAL)
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_start_phase(struct evolution_diagnostics *diag, enum pipeline_execution_phase phase) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to evolution_diagnostics_start_phase");
        return -1;
    }
    
    /* Convert phase bit flag to array index */
    int phase_index = phase_to_index(phase);
    if (phase_index < 0) {
        LOG_ERROR("Invalid phase %d passed to evolution_diagnostics_start_phase", phase);
        return -1;
    }
    
    /* Record start time for this phase */
    diag->phases[phase_index].start_time = clock();
    
    LOG_DEBUG("Starting phase %d for halo %d", phase, diag->halo_nr);
    
    return 0;
}

/**
 * Update phase statistics when exiting a phase
 * 
 * Records the end time of a pipeline execution phase and updates
 * the total time spent in this phase.
 * 
 * @param diag Pointer to the diagnostics structure
 * @param phase The execution phase being ended (PIPELINE_PHASE_HALO, PIPELINE_PHASE_GALAXY, 
 *              PIPELINE_PHASE_POST, or PIPELINE_PHASE_FINAL)
 * @return 0 on success, error code on failure
 */
int evolution_diagnostics_end_phase(struct evolution_diagnostics *diag, enum pipeline_execution_phase phase) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to evolution_diagnostics_end_phase");
        return -1;
    }
    
    /* Convert phase bit flag to array index */
    int phase_index = phase_to_index(phase);
    if (phase_index < 0) {
        LOG_ERROR("Invalid phase %d passed to evolution_diagnostics_end_phase", phase);
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
    
    LOG_DEBUG("Ending phase %d for halo %d, step %d", phase, diag->halo_nr, diag->phases[phase_index].step_count);
    
    return 0;
}

/**
 * Track an event occurrence
 */
int evolution_diagnostics_add_event(struct evolution_diagnostics *diag, event_type_t event_type) {
    if (diag == NULL) {
        LOG_DEBUG("NULL diagnostics pointer provided to evolution_diagnostics_add_event");
        return -1;
    }
    
    if (event_type < 0 || event_type >= EVENT_TYPE_MAX) {
        LOG_ERROR("Invalid event type %d passed to evolution_diagnostics_add_event", event_type);
        return -1;
    }
    
    /* Increment counter for this event type */
    diag->event_counts[event_type]++;
    
    LOG_DEBUG("Added event of type %d to diagnostics for halo %d", event_type, diag->halo_nr);
    
    return 0;
}

/**
 * Record a merger detection
 */
int evolution_diagnostics_add_merger_detection(struct evolution_diagnostics *diag, int merger_type) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to evolution_diagnostics_add_merger_detection");
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
int evolution_diagnostics_add_merger_processed(struct evolution_diagnostics *diag, int merger_type) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to evolution_diagnostics_add_merger_processed");
        return -1;
    }
    
    /* Increment merger processed counter */
    diag->mergers_processed++;
    
    LOG_DEBUG("Added merger processing of type %d to diagnostics for halo %d", merger_type, diag->halo_nr);
    
    return 0;
}

/**
 * Record initial galaxy property statistics
 */
int evolution_diagnostics_record_initial_properties(struct evolution_diagnostics *diag, struct GALAXY *galaxies, int ngal) {
    if (diag == NULL || galaxies == NULL) {
        LOG_ERROR("NULL pointer passed to evolution_diagnostics_record_initial_properties");
        return -1;
    }
    
    /* Reset property totals */
    diag->total_stellar_mass_initial = 0.0;
    diag->total_cold_gas_initial = 0.0;
    diag->total_hot_gas_initial = 0.0;
    diag->total_bulge_mass_initial = 0.0;
    
    /* Get property IDs for physics properties */
    property_id_t stellar_mass_id = get_cached_property_id("StellarMass");
    property_id_t cold_gas_id = get_cached_property_id("ColdGas"); 
    property_id_t hot_gas_id = get_cached_property_id("HotGas");
    property_id_t bulge_mass_id = get_cached_property_id("BulgeMass");
    
    /* Sum properties across all galaxies using generic property accessors */
    for (int i = 0; i < ngal; i++) {
        if (stellar_mass_id != PROP_COUNT && has_property(&galaxies[i], stellar_mass_id)) {
            diag->total_stellar_mass_initial += get_float_property(&galaxies[i], stellar_mass_id, 0.0f);
        }
        if (cold_gas_id != PROP_COUNT && has_property(&galaxies[i], cold_gas_id)) {
            diag->total_cold_gas_initial += get_float_property(&galaxies[i], cold_gas_id, 0.0f);
        }
        if (hot_gas_id != PROP_COUNT && has_property(&galaxies[i], hot_gas_id)) {
            diag->total_hot_gas_initial += get_float_property(&galaxies[i], hot_gas_id, 0.0f);
        }
        if (bulge_mass_id != PROP_COUNT && has_property(&galaxies[i], bulge_mass_id)) {
            diag->total_bulge_mass_initial += get_float_property(&galaxies[i], bulge_mass_id, 0.0f);
        }
    }
    
    LOG_DEBUG("Recorded initial properties for %d galaxies in halo %d", ngal, diag->halo_nr);
    
    return 0;
}

/**
 * Record final galaxy property statistics
 */
int evolution_diagnostics_record_final_properties(struct evolution_diagnostics *diag, struct GALAXY *galaxies, int ngal) {
    if (diag == NULL || galaxies == NULL) {
        LOG_ERROR("NULL pointer passed to evolution_diagnostics_record_final_properties");
        return -1;
    }
    
    /* Set final galaxy count */
    diag->ngal_final = ngal;
    
    /* Reset property totals */
    diag->total_stellar_mass_final = 0.0;
    diag->total_cold_gas_final = 0.0;
    diag->total_hot_gas_final = 0.0;
    diag->total_bulge_mass_final = 0.0;
    
    /* Get property IDs for physics properties */
    property_id_t stellar_mass_id = get_cached_property_id("StellarMass");
    property_id_t cold_gas_id = get_cached_property_id("ColdGas"); 
    property_id_t hot_gas_id = get_cached_property_id("HotGas");
    property_id_t bulge_mass_id = get_cached_property_id("BulgeMass");
    
    /* Sum properties across all galaxies using generic property accessors */
    for (int i = 0; i < ngal; i++) {
        if (stellar_mass_id != PROP_COUNT && has_property(&galaxies[i], stellar_mass_id)) {
            diag->total_stellar_mass_final += get_float_property(&galaxies[i], stellar_mass_id, 0.0f);
        }
        if (cold_gas_id != PROP_COUNT && has_property(&galaxies[i], cold_gas_id)) {
            diag->total_cold_gas_final += get_float_property(&galaxies[i], cold_gas_id, 0.0f);
        }
        if (hot_gas_id != PROP_COUNT && has_property(&galaxies[i], hot_gas_id)) {
            diag->total_hot_gas_final += get_float_property(&galaxies[i], hot_gas_id, 0.0f);
        }
        if (bulge_mass_id != PROP_COUNT && has_property(&galaxies[i], bulge_mass_id)) {
            diag->total_bulge_mass_final += get_float_property(&galaxies[i], bulge_mass_id, 0.0f);
        }
    }
    
    LOG_DEBUG("Recorded final properties for %d galaxies in halo %d", ngal, diag->halo_nr);
    
    return 0;
}

/**
 * Finalize the diagnostics and compute derived metrics
 */
int evolution_diagnostics_finalize(struct evolution_diagnostics *diag) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to evolution_diagnostics_finalize");
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
    
    LOG_DEBUG("Finalized diagnostics for halo %d, elapsed time: %.3f seconds", 
              diag->halo_nr, diag->elapsed_seconds);
    
    return 0;
}

/**
 * Report diagnostics to log
 */
int evolution_diagnostics_report(struct evolution_diagnostics *diag, log_level_t log_level) {
    if (diag == NULL) {
        LOG_ERROR("NULL diagnostics pointer passed to evolution_diagnostics_report");
        return -1;
    }
    
    /* Log basic information */
    if (log_level == LOG_LEVEL_DEBUG) {
        LOG_DEBUG("=== Evolution Diagnostics for Halo %d ===", diag->halo_nr);
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
        
        /* Log galaxy property changes */
        LOG_DEBUG("--- Galaxy Property Changes ---");
        LOG_DEBUG("Stellar Mass: Initial=%.3e, Final=%.3e, Change=%.3e",
                diag->total_stellar_mass_initial, diag->total_stellar_mass_final,
                diag->total_stellar_mass_final - diag->total_stellar_mass_initial);
        
        LOG_DEBUG("Cold Gas: Initial=%.3e, Final=%.3e, Change=%.3e",
                diag->total_cold_gas_initial, diag->total_cold_gas_final,
                diag->total_cold_gas_final - diag->total_cold_gas_initial);
        
        LOG_DEBUG("Hot Gas: Initial=%.3e, Final=%.3e, Change=%.3e",
                diag->total_hot_gas_initial, diag->total_hot_gas_final,
                diag->total_hot_gas_final - diag->total_hot_gas_initial);
        
        LOG_DEBUG("Bulge Mass: Initial=%.3e, Final=%.3e, Change=%.3e",
                diag->total_bulge_mass_initial, diag->total_bulge_mass_final,
                diag->total_bulge_mass_final - diag->total_bulge_mass_initial);
        
        /* Log event statistics if there were any events */
        int total_events = 0;
        for (int i = 0; i < EVENT_TYPE_MAX; i++) {
            total_events += diag->event_counts[i];
        }
        
        if (total_events > 0) {
            LOG_DEBUG("--- Event Statistics ---");
            for (int i = 0; i < EVENT_TYPE_MAX; i++) {
                if (diag->event_counts[i] > 0) {
                    LOG_DEBUG("Event %s: %d occurrences",
                            event_type_name((event_type_t)i), diag->event_counts[i]);
                }
            }
        }
        
        LOG_DEBUG("=====================================");
    }
    else if (log_level == LOG_LEVEL_INFO) {
        /* Reduce verbosity by only logging summary information at INFO level */
        /* For detailed per-halo information, use DEBUG level instead */
    }
    else if (log_level == LOG_LEVEL_WARNING) {
        LOG_WARNING("=== Evolution Diagnostics for Halo %d ===", diag->halo_nr);
        LOG_WARNING("Galaxies: Initial=%d, Final=%d", diag->ngal_initial, diag->ngal_final);
        LOG_WARNING("Processing Time: %.3f seconds", diag->elapsed_seconds);
        
        /* Only log merger statistics for warnings */
        LOG_WARNING("Mergers: Detected=%d, Processed=%d", 
                  diag->mergers_detected, diag->mergers_processed);
        
        LOG_WARNING("=====================================");
    }
    else {
        /* Fall back to ERROR level with minimal output */
        LOG_ERROR("Evolution Diagnostics for Halo %d: %d->%d galaxies, %.3f seconds",
                diag->halo_nr, diag->ngal_initial, diag->ngal_final, diag->elapsed_seconds);
    }
    
    return 0;
}