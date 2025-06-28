#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* functions in model_reincorporation.c */
    extern void inflow_gas(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params);
    extern void transfer_cgm_to_hot(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params);
    extern void mix_cgm_components(const int centralgal, const double dt, struct GALAXY *galaxies, const struct params *run_params);

#ifdef __cplusplus
}
#endif
