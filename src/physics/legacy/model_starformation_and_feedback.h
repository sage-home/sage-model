#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "../../core/core_allvars.h"
    #include "../../core/core_parameter_views.h"

    /* 
     * Star formation and feedback module functions
     * 
     * These functions handle star formation and supernova feedback in galaxies.
     */
    
    /* New signatures using parameter views */
    extern void starformation_and_feedback(const int p, const int centralgal, const double time, 
                                          const double dt, const int halonr, const int step,
                                          struct GALAXY *galaxies, 
                                          const struct star_formation_params_view *sf_params,
                                          const struct feedback_params_view *fb_params);
    
    extern void update_from_star_formation(const int p, const double stars, const double metallicity, 
                                          struct GALAXY *galaxies, 
                                          const struct star_formation_params_view *sf_params);
    
    extern void update_from_feedback(const int p, const int centralgal, const double reheated_mass, 
                                    double ejected_mass, const double metallicity,
                                    struct GALAXY *galaxies, 
                                    const struct feedback_params_view *fb_params);
    
    /* Compatibility wrappers for backwards compatibility */
    extern void starformation_and_feedback_compat(const int p, const int centralgal, const double time, 
                                                const double dt, const int halonr, const int step,
                                                struct GALAXY *galaxies, const struct params *run_params);
    
    extern void update_from_star_formation_compat(const int p, const double stars, const double metallicity, 
                                                struct GALAXY *galaxies, const struct params *run_params);
    
    extern void update_from_feedback_compat(const int p, const int centralgal, const double reheated_mass, 
                                          double ejected_mass, const double metallicity,
                                          struct GALAXY *galaxies, const struct params *run_params);

#ifdef __cplusplus
}
#endif
