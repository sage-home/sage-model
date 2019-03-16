#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* Functions in core_save.c */
    extern int32_t initialize_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info,
                                           const struct params *run_params);
    extern void save_galaxies(const int filenr, const int tree, const int numgals, struct halo_data *halos,
                              struct halo_aux_data *haloaux, struct GALAXY *halogal, struct save_info *save_info,
                              const struct params *run_params);
    extern int finalize_galaxy_files(const int ntrees, struct save_info *save_info, const struct params *run_params);
    
#ifdef __cplusplus
}
#endif
