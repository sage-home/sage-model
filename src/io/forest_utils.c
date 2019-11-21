#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "io_utils.h"

int distribute_forests_over_ntasks(const int64_t totnforests, const int NTasks, const int ThisTask, int64_t *nforests_thistask, int64_t *start_forestnum_thistask)
{
    if(ThisTask > NTasks || ThisTask < 0 || NTasks < 1) {
        fprintf(stderr,"Error: ThisTask = %d and NTasks = %d must satisfy i) ThisTask < NTasks, ii) ThisTask > 0 and iii) NTasks >= 1\n",
                ThisTask, NTasks);
        return EXIT_FAILURE;
    }

    if(totnforests < 0) {
        fprintf(stderr,"Error: On ThisTask = %d: total number of forests = %"PRId64" must be >= 0\n", totnforests);
        return EXIT_FAILURE;
    }

    if(totnforests == 0) {
        fprintf(stderr,"Warning: Got 0 forests to distribute! Returning\n");
        *nforests_thistask = 0;
        *start_forestnum_thistask = 0;
        return EXIT_SUCCESS;
    }


    // Assign each task an equal number of forests. If we can't equally assign each task
    // the EXACT same number of forests, give each task an extra forest (if required).
    const int64_t nforests_per_cpu = (int64_t) (totnforests/(int64_t) NTasks);
    const int64_t rem_nforests = totnforests % NTasks;
    int64_t nforests_this_task = nforests_per_cpu;
    if(ThisTask < rem_nforests) {
        nforests_this_task++;
    }

    int64_t start_forestnum = nforests_per_cpu * ThisTask;
    if(ThisTask < rem_nforests) {
        start_forestnum += ThisTask;
    } else {
        start_forestnum += rem_nforests;  // All tasks that weren't given an extra forest will be offset by a constant amount.
    }

    /* Now fill up the destination */
    *nforests_thistask = nforests_this_task;
    *start_forestnum_thistask = start_forestnum;

    return EXIT_SUCCESS;
}
