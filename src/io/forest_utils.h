#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif



enum forest_weight_type
{
    uniform_in_forests = 0,
    linear_in_nhalos = 1,
    quadratic_in_nhalos = 2,
    exponent_in_nhalos = 3,
    generic_power_in_nhalos = 5,
    num_forest_weight_types
};

    extern int distribute_forests_over_ntasks(const int64_t totnforests, const int NTasks, const int ThisTask,
                                              int64_t *nforests_thistask, int64_t *start_forestnum_thistask);

    extern int distribute_weighted_forests_over_ntasks(const int64_t totnforests, const int64_t *nhalos_per_forest,
                                                       const enum forest_weight_type forest_weighting, const double power_law_index,
                                                       const int NTasks, const int ThisTask, int64_t *nforests_thistask, int64_t *start_forestnum_thistask);
        
    
#ifdef __cplusplus
}
#endif
