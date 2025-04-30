#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "core_allvars.h"
#include "core_merger_queue.h"
#include "core_logging.h"

#include "../physics/legacy/model_mergers.h"

void init_merger_queue(struct merger_event_queue *queue)
{
    if (queue == NULL) {
        LOG_ERROR("Null pointer passed to init_merger_queue");
        return;
    }
    
    queue->num_events = 0;
}

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
)
{
    // Check for null pointer
    if (queue == NULL) {
        LOG_ERROR("Null queue pointer passed to queue_merger_event");
        return -1;
    }
    
    // Check for queue overflow
    if (queue->num_events >= MAX_GALAXIES_PER_HALO) {
        LOG_ERROR("Merger event queue overflow: num_events=%d, MAX_GALAXIES_PER_HALO=%d",
                 queue->num_events, MAX_GALAXIES_PER_HALO);
        return -1;
    }
    
    // Add event to queue
    struct merger_event *event = &queue->events[queue->num_events++];
    event->satellite_index = satellite_index;
    event->central_index = central_index;
    event->merger_time = merger_time;
    event->time = time;
    event->dt = dt;
    event->halo_nr = halo_nr;
    event->step = step;
    event->merger_type = merger_type;
    
    LOG_DEBUG("Added merger event to queue: satellite=%d, central=%d, merger_time=%g",
             satellite_index, central_index, merger_time);
    
    return 0;
}

int process_merger_events(
    struct merger_event_queue *queue,
    struct GALAXY *galaxies,
    struct params *run_params
)
{
    // Check for null pointers
    if (queue == NULL || galaxies == NULL || run_params == NULL) {
        LOG_ERROR("Null pointer passed to process_merger_events");
        return -1;
    }
    
    LOG_DEBUG("Processing %d merger events", queue->num_events);
    
// Process all events in the queue
    for (int i = 0; i < queue->num_events; i++) {
        struct merger_event *event = &queue->events[i];
        int p = event->satellite_index;
        int merger_centralgal = event->central_index;
        
        // Note: mergeIntoID is already set in evolve_galaxies before queuing the event
        
        // Process merger or disruption based on merger time
        if (event->merger_time > 0.0) {
            // Disruption
            LOG_DEBUG("Processing disruption event: satellite=%d, central=%d", 
                     p, merger_centralgal);
            disrupt_satellite_to_ICS(merger_centralgal, p, galaxies);
        } else {
            // Merger
            LOG_DEBUG("Processing merger event: satellite=%d, central=%d", 
                     p, merger_centralgal);
            deal_with_galaxy_merger(
                p, 
                merger_centralgal, 
                event->halo_nr,  // Pass the centralgal value
                event->time, 
                event->dt, 
                event->halo_nr, 
                event->step, 
                galaxies, 
                run_params
            );
        }
    }
    
    // Reset the queue
    queue->num_events = 0;
    
    return 0;
}