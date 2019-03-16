#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

#include "../core_allvars.h"

    
    /* Proto-Types */
    extern int32_t initialize_binary_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info,
                                                  const struct params *run_params);
    
    extern int32_t save_binary_galaxies(const int32_t filenr, const int32_t treenr, const int32_t num_gals,
                                        const int32_t *OutputGalCount, struct halo_data *halos, struct halo_aux_data *haloaux,
                                        struct GALAXY *halogal, struct save_info *save_info, const struct params *run_params);

    extern int32_t finalize_binary_galaxy_files(const int ntrees, struct save_info *save_info, const struct params *run_params);

#ifdef __cplusplus
}
#endif
