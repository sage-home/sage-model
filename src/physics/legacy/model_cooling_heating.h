#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "../../core/core_allvars.h"
    #include "../../core/core_parameter_views.h"

    /* 
     * Cooling module functions
     * 
     * These functions handle gas cooling and heating processes in galaxies.
     */
    
    /* New signature using parameter views */
    extern double cooling_recipe(const int gal, const double dt, struct GALAXY *galaxies, 
                                const struct cooling_params_view *cooling_params);
    
    extern void cool_gas_onto_galaxy(const int centralgal, const double coolingGas, struct GALAXY *galaxies);
    
    extern double do_AGN_heating(double coolingGas, const int centralgal, const double dt, 
                                const double x, const double rcool, struct GALAXY *galaxies, 
                                const struct agn_params_view *agn_params);
    
    /* Compatibility wrappers for backwards compatibility */
    extern double cooling_recipe_compat(const int gal, const double dt, struct GALAXY *galaxies, 
                                       const struct params *run_params);
    
    extern double do_AGN_heating_compat(double coolingGas, const int centralgal, const double dt, 
                                       const double x, const double rcool, struct GALAXY *galaxies, 
                                       const struct params *run_params);

#ifdef __cplusplus
}
#endif
