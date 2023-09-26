#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#include "forest_utils.h"

static inline double compute_forest_cost_from_nhalos(const enum Valid_Forest_Distribution_Schemes forest_weighting, const int64_t nhalos, const double exponent);

int distribute_forests_over_ntasks(const int64_t totnforests, const int NTasks, const int ThisTask, int64_t *nforests_thistask, int64_t *start_forestnum_thistask)
{
    if(ThisTask > NTasks || ThisTask < 0 || NTasks < 1) {
        fprintf(stderr,"Error: ThisTask = %d and NTasks = %d must satisfy i) ThisTask < NTasks, ii) ThisTask > 0 and iii) NTasks >= 1\n",
                ThisTask, NTasks);
        return EXIT_FAILURE;
    }

    if(totnforests < 0) {
        fprintf(stderr,"Error: On ThisTask = %d: total number of forests = %"PRId64" must be >= 0\n", ThisTask, totnforests);
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



static inline double compute_forest_cost_from_nhalos(const enum Valid_Forest_Distribution_Schemes forest_weighting, const int64_t nhalos, const double exponent)
{
    /* Strategy for load-balancing across MPI tasks*/
    double cost;
    switch(forest_weighting) {
    case uniform_in_forests:
        /* Every forest is treated equally -> every forest contributes one unit
         of compute cost */
        cost = 1.0;
        break;

    case linear_in_nhalos:
        cost = nhalos;
        break;

    case quadratic_in_nhalos:
        cost = nhalos*nhalos;
        break;

    case exponent_in_nhalos:
        if(exponent < 0) {
            fprintf(stderr,"Error: The power-law index = %e (should be integer) for computing the weights of individual forests must be > 0\n", exponent);
            return -1;
        }
        const int index = (int) exponent;
        cost = 1.0;/* Or should I replace with a pow? MS 15/01/2020 */
        const double dbl_nhalos = (double) nhalos;
        for(int i=0;i<index;i++) {
            cost *= dbl_nhalos;
        }
        break;

    case generic_power_in_nhalos:
        if(exponent < 0) {
            fprintf(stderr,"Error: The power-law index = %e for computing the weights of individual forests must be > 0\n", exponent);
            return -1;
        }
        cost = pow((double) nhalos, exponent);
        break;

    default:
        cost = 1.0;
        break;
    }


    return cost;
}


int distribute_weighted_forests_over_ntasks(const int64_t totnforests, const int64_t *nhalos_per_forest,
                                            const enum Valid_Forest_Distribution_Schemes forest_weighting, const double power_law_index,
                                            const int NTasks, const int ThisTask, int64_t *nforests_thistask, int64_t *start_forestnum_thistask)
{
    if(ThisTask > NTasks || ThisTask < 0 || NTasks < 1) {
        fprintf(stderr,"Error: ThisTask = %d and NTasks = %d must satisfy i) ThisTask < NTasks, ii) ThisTask > 0 and iii) NTasks >= 1\n",
                ThisTask, NTasks);
        return EXIT_FAILURE;
    }

    if(totnforests < 0) {
        fprintf(stderr,"Error: On ThisTask = %d: total number of forests = %"PRId64" must be >= 0\n", ThisTask, totnforests);
        return EXIT_FAILURE;
    }

    if(totnforests == 0 || NTasks == 1) {
        *nforests_thistask = totnforests;/* totnforests could be 0, or all the forests if running on a single task*/
        *start_forestnum_thistask = 0;
        return EXIT_SUCCESS;
    }

    if(forest_weighting == uniform_in_forests || nhalos_per_forest == NULL) {
        // fprintf(stderr,"Warning: Based on the inputs, switching to the assigning forests *without* weights (might indicate bug in code, only affects load-balancing and the actual results)\n");
        return distribute_forests_over_ntasks(totnforests, NTasks, ThisTask, nforests_thistask, start_forestnum_thistask);
    }


    if((forest_weighting == exponent_in_nhalos || forest_weighting == generic_power_in_nhalos) && power_law_index < 0) {
        fprintf(stderr,"Error: You have requested a power-law exponent but the exponent = %e must be greater than 0\n", power_law_index);
        return EXIT_FAILURE;
    }

    double total_cost_across_all_forests = 0;
    int64_t totnhalos = 0;
    for(int64_t i=0;i<totnforests;i++) {
        const double cost_this_forest = compute_forest_cost_from_nhalos(forest_weighting, nhalos_per_forest[i], power_law_index);
        total_cost_across_all_forests += cost_this_forest;
        totnhalos += nhalos_per_forest[i];
    }

    double target_cost_per_task = total_cost_across_all_forests/NTasks;

    int64_t start_forestnum = 0, nforests_this_task = -1, nhalos_so_far = 0, nhalos_curr_task = 0;
    double curr_cost_target = target_cost_per_task, cost_so_far = 0.0;
    int currtask = 0;
    for(int64_t i=0;i<totnforests;i++) {
        const double cost_this_forest = compute_forest_cost_from_nhalos(forest_weighting, nhalos_per_forest[i], power_law_index);
        cost_so_far += cost_this_forest;
        nhalos_so_far += nhalos_per_forest[i];
        nhalos_curr_task += nhalos_per_forest[i];
        if (cost_so_far < curr_cost_target) continue;

        /* If we have reached here that means processing this forest
           will exceed the target cost. Therefore, we need to mark
           this forest as the end point for whichever task is getting
           assigned, and then repeat until we reach 'ThisTask'
         */

        if(ThisTask == currtask) {
            /* If we reach here, then we have the identified the final
             forest to process on ThisTask. `start_forestnum` is
             already set correctly

             The +1 is because the ThisTask needs to process this i'th
             forest (i.e., inclusive range of [start_forestnum, i])
            */
            fprintf(stderr,"[LOG]: Assigning forest-range = [%"PRId64", %"PRId64"] (containing %"PRId64" halos) to ThisTask = %d\n",
                    start_forestnum, i, nhalos_curr_task, ThisTask);
            nforests_this_task = i - start_forestnum + 1;
            break;
        }

        /* If we have reached here, that means we have to compute what
           range of forests the next task would process */
        currtask++;
        nhalos_curr_task = 0;
        start_forestnum = i + 1;

        /* If we are assinging the last task, then we can simply assign all the
         remaining forests and be done */
        if(currtask == (NTasks - 1)) {
            /* There is no '+1' here because the last forest index is (totnforests - 1)
               MS: 15/01/2020 */
            nhalos_curr_task = totnhalos - nhalos_so_far;
            fprintf(stderr,"[LOG]: Assigning all remaining forests to last task. Remaining cost = %g (ideal target cost = %g)\n",
                    total_cost_across_all_forests - cost_so_far, target_cost_per_task);
            fprintf(stderr,"[LOG]: Assigning forest-range = [%"PRId64", %"PRId64"] (containing %"PRId64" halos) to ThisTask = %d\n",
                    start_forestnum, totnforests - 1, nhalos_curr_task, ThisTask);
            nforests_this_task = totnforests - start_forestnum;
            break;
        }

        const double remaining_cost = total_cost_across_all_forests - cost_so_far;
        const int remaining_ntasks = NTasks - currtask;
        target_cost_per_task = remaining_cost/remaining_ntasks;
        curr_cost_target = cost_so_far + target_cost_per_task;
    }


    /* Now fill up the destination */
    *nforests_thistask = nforests_this_task;
    *start_forestnum_thistask = start_forestnum;

    return EXIT_SUCCESS;
}



int find_start_and_end_filenum(const int64_t start_forestnum, const int64_t end_forestnum,
                               const int64_t *totnforests_per_file, const int64_t totnforests,
                               const int firstfile, const int lastfile,
                               const int ThisTask, const int NTasks,
                               int64_t *num_forests_to_process_per_file, int64_t *start_forestnum_to_process_per_file,
                               int *start_file, int *end_file)
{

    if (start_forestnum >= end_forestnum) {
        fprintf(stderr,"Error (line %d in function '%s'): Expected that the starting forest number = %"PRId64 " "
                       "should be less end forest number = %"PRId64"... returning \n",
                       __LINE__, __FUNCTION__, start_forestnum, end_forestnum);
        return -1;
    }

    const int64_t nforests_this_task = end_forestnum - start_forestnum;
    int start_filenum = -1, end_filenum = -1;
    int64_t nforests_so_far = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        const int64_t nforests_this_file = totnforests_per_file[filenr];
        if(nforests_this_file < 0 ){
            fprintf(stderr,"Error: Number of forests = %"PRId64" in file = %d *must* be >= 0\n", nforests_this_file, filenr);
            return -1;
        }
        if(nforests_this_file == 0) continue;

        const int64_t end_forestnum_this_file = nforests_so_far + nforests_this_file;
        // fprintf(stderr,"filenr = %d end_forestnum_this_file = %"PRId64"\n", filenr, end_forestnum_this_file);
        start_forestnum_to_process_per_file[filenr] = 0;
        num_forests_to_process_per_file[filenr] = nforests_this_file;

        if(start_forestnum >= nforests_so_far && start_forestnum < end_forestnum_this_file) {
            start_filenum = filenr;
            start_forestnum_to_process_per_file[filenr] = start_forestnum - nforests_so_far;
            num_forests_to_process_per_file[filenr] = nforests_this_file - (start_forestnum - nforests_so_far);
        }


        if(end_forestnum >= nforests_so_far && end_forestnum <= end_forestnum_this_file) {
            num_forests_to_process_per_file[filenr] = end_forestnum - (start_forestnum_to_process_per_file[filenr] + nforests_so_far);
            end_filenum = filenr;

            XRETURN(num_forests_to_process_per_file[filenr] <= nforests_this_task, -1,
                    "Error: num_forests_to_process_per_file[%d] = %"PRId64" start_filenum = %d end_filenum = %d "
                    "start_forestnum = %"PRId64" end_forestnum = %"PRId64" nforests_this_task = %"PRId64" nforests_so_far = %"PRId64"\n",
                    filenr, num_forests_to_process_per_file[filenr], start_filenum, end_filenum, start_forestnum, end_forestnum, nforests_this_task, nforests_so_far);
            break;
        }
        nforests_so_far += nforests_this_file;
    }

    if(start_filenum == -1 || end_filenum == -1 ) {
        fprintf(stderr,"Error: Could not locate start (=%d) or end file number (=%d) for processing forests\n", start_filenum, end_filenum);
        fprintf(stderr,"Printing debug info\n");
        fprintf(stderr,"ThisTask = %d NTasks = %d totnforests = %"PRId64" start_forestnum = %"PRId64" end_forestnum = %"PRId64"\n",
                ThisTask, NTasks, totnforests, start_forestnum, end_forestnum);
        for(int filenr=firstfile;filenr<=lastfile;filenr++) {
            fprintf(stderr,"filenr := %d contains %"PRId64" forests ",filenr, totnforests_per_file[filenr]);
            fprintf(stderr,"\n");
        }

        return -1;
    }

    *start_file = start_filenum;
    *end_file = end_filenum;

    return EXIT_SUCCESS;
}