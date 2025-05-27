#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "core_allvars.h"
#include "core_merger_queue.h"
#include "core_logging.h"

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