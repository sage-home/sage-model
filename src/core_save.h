#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* Functions in core_save.c */
    extern void initialize_galaxy_files(const int filenr, const int ntrees, int *save_fd, const struct params *run_params);
    extern void save_galaxies(const int filenr, const int tree, const int numgals, struct halo_data *halos,
                              struct halo_aux_data *haloaux, struct GALAXY *halogal, int **treengals, int *totgalaxies,
                              const int *save_fd, const struct params *run_params);
    extern void prepare_galaxy_for_output(int filenr, int tree, struct GALAXY *g, struct GALAXY_OUTPUT *o, struct halo_data *halos,
                                          struct halo_aux_data *haloaux, struct GALAXY *halogal, const struct params *run_params);
    extern int finalize_galaxy_file(const int ntrees, const int *totgalaxies, const int **treengals, int *save_fd, const struct params *run_params);
    
#ifdef __cplusplus
}
#endif
