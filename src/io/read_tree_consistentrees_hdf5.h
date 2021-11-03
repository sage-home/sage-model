#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

/* for definition of struct halo_data */
#include "../core_allvars.h"

/* Proto-Types */
    extern int setup_forests_io_ctrees_hdf5(struct forest_info *forests_info, const int ThisTask, const int NTasks, struct params *run_params);
    extern int64_t load_forest_ctrees_hdf5(int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info, struct params *run_params);
    extern void cleanup_forests_io_ctrees_hdf5(struct forest_info *forests_info);

#ifdef __cplusplus
}
#endif /* working with c++ compiler */
