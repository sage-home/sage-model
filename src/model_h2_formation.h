#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

/* Functions in model_h2_formation.c */
extern double calculate_molecular_fraction_GD14(float gas_density, float metallicity);
extern double gd14_sigma_norm(float d_mw, float u_mw);
extern void update_gas_components(struct GALAXY *g, const struct params *run_params);
extern void init_gas_components(struct GALAXY *g);

#ifdef __cplusplus
}
#endif