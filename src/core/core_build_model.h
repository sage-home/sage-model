#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"
#include "galaxy_array.h"

    /* functions in core_build_model.c */
    extern int process_fof_group(int fof_halonr, GalaxyArray* galaxies_prev_snap, 
                                 GalaxyArray* galaxies_this_snap, struct halo_data *halos,
                                 struct halo_aux_data *haloaux, int32_t *galaxycounter,
                                 struct params *run_params, bool *processed_flags);
    
    extern int identify_and_process_orphans(const int fof_halonr, GalaxyArray* temp_fof_galaxies,
                                           const GalaxyArray* galaxies_prev_snap, bool *processed_flags,
                                           struct halo_data *halos, struct params *run_params);
    
    extern void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params);
    
    extern int evolve_galaxies_wrapper(const int fof_root_halonr, 
                                      GalaxyArray* temp_fof_galaxies, 
                                      int *numgals, 
                                      struct halo_data *halos, 
                                      struct halo_aux_data *haloaux,
                                      GalaxyArray *galaxies_this_snap, 
                                      struct params *run_params);
    
    extern int construct_galaxies(const int halonr, int *numgals, int *galaxycounter, 
                                 GalaxyArray **working_galaxies, GalaxyArray **output_galaxies,
                                 struct halo_data *halos, struct halo_aux_data *haloaux, 
                                 bool *DoneFlag, int *HaloFlag, struct params *run_params);

#ifdef __cplusplus
}
#endif
