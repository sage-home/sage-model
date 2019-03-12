#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

#include "../core_allvars.h"

    
    /* Proto-Types */
    extern void initialize_hdf5_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info, const struct params *run_params);
    
#ifdef __cplusplus
}
#endif
