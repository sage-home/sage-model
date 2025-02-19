#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

/* Functions in model_h2_formation.c */
extern float calculate_H2_fraction(const float surface_density, const float metallicity, 
                                 const float DiskRadius, const struct params *run_params);
extern void update_gas_components(struct GALAXY *g, const struct params *run_params);
extern void init_gas_components(struct GALAXY *g);

#ifdef __cplusplus
}
#endif