#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"
#include "galaxy_array.h"

    /* functions in core_build_model.c */
    extern int construct_galaxies(const int halonr, int *numgals, int32_t *galaxycounter, struct halo_data *halos,
                                  struct halo_aux_data *haloaux, GalaxyArray *galaxies_this_snap,
                                  GalaxyArray *galaxies_prev_snap, struct params *run_params);
    
    extern void deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params);

#ifdef __cplusplus
}
#endif
