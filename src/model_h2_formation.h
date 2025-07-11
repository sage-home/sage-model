#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

/* Functions in model_h2_formation.c */
extern float calculate_molecular_fraction_GD14(float gas_density, float metallicity);
extern float gd14_sigma_norm(float d_mw, float u_mw);
extern float integrate_molecular_gas_radial(struct GALAXY *g, const struct params *run_params);
extern float calculate_bulge_molecular_gas(struct GALAXY *g, const struct params *run_params);
extern void update_gas_components(struct GALAXY *g, const struct params *run_params);
extern void init_gas_components(struct GALAXY *g);
extern void diagnose_cgm_h2_interaction(struct GALAXY *g, const struct params *run_params);
extern void apply_environmental_effects(struct GALAXY *g, struct GALAXY *galaxies, 
                                 int central_gal_index, 
                                 const struct params *run_params);
extern float calculate_midplane_pressure_BR06(float sigma_gas, float sigma_stars, 
                                               float radius_pc);
extern float calculate_molecular_fraction_BR06(float gas_surface_density, 
                                                float stellar_surface_density, 
                                                float radius_pc);

#ifdef __cplusplus
}
#endif