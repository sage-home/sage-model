#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* functions in model_lowmass_suppression.c */
    extern double calculate_lowmass_suppression(const int gal, const double redshift, struct GALAXY *galaxies, const struct params *run_params);

#ifdef __cplusplus
}
#endif