#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* functions in model_infall.c */
    extern double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params, const double dt);
    extern double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
    extern void add_infall_to_hot(const int gal, double infallingGas, const double z,struct GALAXY *galaxies, const struct params *run_params);
    extern double calculate_critical_mass_dekel_birnboim_2006(const double z, const struct params *run_params);
    extern double calculate_mshock_with_sage_metallicity(const double z, const struct params *run_params);

    // Equation (18): A parameter calculation
    extern double calculate_A_parameter(const double z, const struct params *run_params);
    
    // Equation (30): F factor calculation  
    extern double calculate_F_factor(void);
    // Equation (40): Maximum mass for cold streams in hot halos
    extern double calculate_mstream_max(const double z, const struct params *run_params);
    
    // Press-Schechter characteristic mass M* for equation (43)
    extern double calculate_press_schechter_mass(const double z, const struct params *run_params);
    

#ifdef __cplusplus
}
#endif
