#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

#include "../core_allvars.h"

    /* Proto-Types */
    extern int setup_forests_io_lht_binary(struct forest_info *forests_info,
                                           const int ThisTask, const int NTasks, struct params *run_params);
    extern int64_t load_forest_lht_binary(const int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info);
    extern void cleanup_forests_io_lht_binary(struct forest_info *forests_info);

#ifdef __cplusplus
}
#endif
