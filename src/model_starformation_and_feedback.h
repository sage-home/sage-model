#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* functions in model_starformation_and_feedback.c */
    extern void starformation_and_feedback(const int p, const int centralgal, const double time, const double dt, const int halonr, const int step,
                                           struct GALAXY *galaxies, const struct params *run_params);
    extern void update_from_star_formation(const int p, const double stars, const double metallicity, struct GALAXY *galaxies, const struct params *run_params);
    extern void update_from_feedback(const int p, const int centralgal, const double reheated_mass, double ejected_mass, const double metallicity,
                                     struct GALAXY *galaxies, const struct params *run_params);
    extern double calculate_muratov_mass_loading(const int p, const double z, struct GALAXY *galaxies);

#ifdef __cplusplus
}
#endif

