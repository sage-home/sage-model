#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* functions in model_infall.c */
    extern double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies);
    extern void add_infall_to_cgm(const int gal, double infallingGas, struct GALAXY *galaxies);
    extern void add_infall_to_hot_pure(const int gal, double infallingGas, struct GALAXY *galaxies);
    extern double calculate_rcool_to_rvir_ratio(const int gal, struct GALAXY *galaxies, const struct params *run_params);
    extern void handle_regime_transition(const int gal, struct GALAXY *galaxies, const struct params *run_params);

#ifdef __cplusplus
}
#endif
