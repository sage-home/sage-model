#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "../core/core_allvars.h"
    #include "../core/core_parameter_views.h"

    /* 
     * Reincorporation module functions
     * 
     * These functions handle gas reincorporation from the ejected reservoir
     * back into the hot gas component.
     */
    
    /* New signature using parameter views */
    extern void reincorporate_gas(const int centralgal, const double dt, struct GALAXY *galaxies, 
                                 const struct reincorporation_params_view *reincorp_params);
    
    /* Compatibility wrapper for backwards compatibility */
    extern void reincorporate_gas_compat(const int centralgal, const double dt, struct GALAXY *galaxies, 
                                        const struct params *run_params);

#ifdef __cplusplus
}
#endif
