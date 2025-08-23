#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

/* Functions in model_h2_formation.c */
extern double calculate_molecular_fraction_GD14(float gas_surface_density, float metallicity);
extern float calculate_stellar_scale_height_BR06(float disk_scale_length_pc);
extern float calculate_midplane_pressure_BR06(float sigma_gas, float sigma_stars, float disk_scale_length_pc);
extern double calculate_molecular_fraction_BR06(float gas_surface_density, float stellar_surface_density, 
                                               float disk_scale_length_pc);
extern void update_gas_components(struct GALAXY *g, const struct params *run_params);
extern void init_gas_components(struct GALAXY *g);