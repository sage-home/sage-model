#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

// Forward declaration for GalaxyArray
typedef struct GalaxyArray GalaxyArray;

    /* functions in core_build_model.c */
    extern int construct_galaxies(const int halonr, int *numgals, int32_t *galaxycounter, struct halo_data *halos,
                                  struct halo_aux_data *haloaux, GalaxyArray *galaxies_arr, GalaxyArray *halogal_arr,
                                  struct params *run_params);

#ifdef __cplusplus
}
#endif
