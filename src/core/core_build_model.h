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
                                 struct params *run_params, int nhalos);
    
    extern void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params);

#ifdef __cplusplus
}
#endif
