#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core_allvars.h"

    extern int distribute_forests_over_ntasks(const int64_t totnforests, const int NTasks, const int ThisTask,
                                              int64_t *nforests_thistask, int64_t *start_forestnum_thistask);

    extern int distribute_weighted_forests_over_ntasks(const int64_t totnforests, const int64_t *nhalos_per_forest,
                                                       const enum Valid_Forest_Distribution_Schemes forest_weighting, const double power_law_index,
                                                       const int NTasks, const int ThisTask, int64_t *nforests_thistask, int64_t *start_forestnum_thistask);
        
    
#ifdef __cplusplus
}
#endif
