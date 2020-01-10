#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    extern int distribute_forests_over_ntasks(const int64_t totnforests, const int NTasks, const int ThisTask,
                                              int64_t *nforests_thistask, int64_t *start_forestnum_thistask);

#ifdef __cplusplus
}
#endif
