#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* Functions in core_save.c */
    extern int32_t initialize_galaxy_files(const int filenr, const struct forest_info *forest_info,
                                           struct save_info *save_info,
                                           const struct params *run_params);

    extern int32_t save_galaxies(const int64_t task_forestnr, const int numgals, struct halo_data *halos, struct forest_info *forest_info,
                                 struct halo_aux_data *haloaux, struct GALAXY *halogal, struct save_info *save_info,
                                 const struct params *run_params);

    extern int32_t finalize_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info,
                                         const struct params *run_params);

#ifdef __cplusplus
}
#endif
