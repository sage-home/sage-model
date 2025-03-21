#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

/* Functions in model_h2_formation.c */
extern float calculate_H2_fraction(const float surface_density, const float metallicity, 
                                 const float DiskRadius, const struct params *run_params);
extern float calculate_H2_fraction_KD12(const float surface_density, const float metallicity, 
                                      const float clumping_factor);
extern float calculate_midplane_pressure(float gas_density, float stellar_density, 
                                       float radius, float stellar_scale_height);
extern float calculate_molecular_fraction_GD14(float gas_density, float metallicity, 
                                        float radiation_field, 
                                        const struct params *run_params);
extern float integrate_molecular_gas_radial(struct GALAXY *g, const struct params *run_params);
extern float calculate_bulge_molecular_gas(struct GALAXY *g, const struct params *run_params);
extern void update_gas_components(struct GALAXY *g, const struct params *run_params);
extern void init_gas_components(struct GALAXY *g);

/* New environmental effects functions */
extern void apply_environmental_effects(struct GALAXY *g, const struct params *run_params);

#ifdef __cplusplus
}
#endif