#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

#include "../core_allvars.h"

    
    /* Proto-Types */
    extern void get_forests_filename_lht_binary(char *filename, const size_t len, const int filenr, const struct params *run_params);
    extern int setup_forests_io_lht_binary(struct forest_info *forests_info, const int firstfile, const int lastfile,
                                           const int ThisTask, const int NTasks, const struct params *run_params);
    extern int64_t load_forest_lht_binary(const int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info);
    extern void cleanup_forests_io_lht_binary(struct forest_info *forests_info);
    
#ifdef __cplusplus
}
#endif
