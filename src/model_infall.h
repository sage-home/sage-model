#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

   /* functions in model_infall.c */
    extern double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies, const struct params *run_params);
    extern void update_gas_components(struct GALAXY *g, const struct params *run_params);

    /* New enhanced reionization functions */
    extern double do_reionization_enhanced(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern double calculate_filtering_mass(const double z, const struct params *run_params);
    extern double calculate_local_reionization_redshift(const int gal, struct GALAXY *galaxies, const struct params *run_params);
    extern double apply_early_uv_background(const int gal, const double z, double reion_modifier, struct GALAXY *galaxies, const struct params *run_params);
    extern double enhanced_gnedin_model(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params);
    extern double sobacchi_mesinger_model(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params);
    extern double patchy_reionization_model(const int gal, const double z, struct GALAXY *galaxies, const struct params *run_params);


#ifdef __cplusplus
}
#endif
