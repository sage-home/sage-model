#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /**
     * @brief Initialize a merger event queue
     * 
     * Sets up an empty queue ready to collect merger events during
     * galaxy evolution.
     * 
     * @param queue Pointer to the queue structure to initialize
     */
    void init_merger_queue(struct merger_event_queue *queue);

    /**
     * @brief Add a merger event to the queue
     * 
     * Stores information about a potential merger or disruption for 
     * later processing. Performs bounds checking to prevent overflow.
     * 
     * @param queue Pointer to the queue to add the event to
     * @param satellite_index Index of the satellite galaxy
     * @param central_index Index of the central galaxy
     * @param merger_time Remaining merger time
     * @param time Current simulation time
     * @param dt Timestep size
     * @param halo_nr Halo number
     * @param step Current timestep
     * @param merger_type Type of merger event (merger/disruption)
     * @return 0 on success, non-zero on failure
     */
    int queue_merger_event(
        struct merger_event_queue *queue,
        int satellite_index,
        int central_index,
        double merger_time,
        double time,
        double dt,
        int halo_nr,
        int step,
        int merger_type
    );

    /**
     * @brief Process all merger events in the queue
     * 
     * Executes all queued merger and disruption events after
     * all galaxies have had their physics processes for a timestep.
     * This ensures all galaxies see the same pre-merger environment.
     * 
     * @param queue Pointer to the queue containing events to process
     * @param galaxies Array of galaxies
     * @param run_params Simulation parameters
     * @return 0 on success, non-zero on failure
     */
    int process_merger_events(
        struct merger_event_queue *queue,
        struct GALAXY *galaxies,
        struct params *run_params
    );

#ifdef __cplusplus
}
#endif