#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

#include "../core_allvars.h"


    /* Proto-Types */
    extern void get_forests_filename_ctr_ascii(char *filename, const size_t len, const struct params *run_params);
    extern int setup_forests_io_ctrees(struct forest_info *forests_info, const int ThisTask, const int NTasks, struct params *run_params);
    extern int64_t load_forest_ctrees(const int32_t forestnr, struct halo_data **halos, struct forest_info *forests_info, struct params *run_params);
    extern void cleanup_forests_io_ctrees(struct forest_info *forests_info);

#ifdef __cplusplus
}
#endif
