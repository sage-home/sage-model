/**
 * @file placeholder_model_misc.c
 * @brief Placeholder implementations for model_misc.c functions in core-only mode
 *
 * This file provides placeholder implementations for physics functions from
 * model_misc.c that are needed in core-only builds. These functions return
 * sensible default values rather than accessing physics properties.
 */

#include <stdlib.h>
#include <math.h>
#include "../core/core_allvars.h"
#include "placeholder_model_misc.h"

/**
 * @brief Placeholder for get_virial_mass that returns the mass directly
 */
float get_virial_mass(int halonr, const struct halo_data *halos, const struct params *run_params)
{
    if (halonr < 0) {
        return 0.0;
    }
    /* Simply return the mass directly without physics-specific calculations */
    return halos[halonr].Mvir;
}

/**
 * @brief Placeholder for get_virial_radius that returns a default value
 */
float get_virial_radius(int halonr, const struct halo_data *halos, const struct params *run_params)
{
    if (halonr < 0) {
        return 0.0;
    }
    /* In core-only mode, calculate a simple estimate based on mass */
    float mvir = halos[halonr].Mvir;
    /* R_vir ∝ M_vir^(1/3) - simplified scale factor of 0.1 */
    return 0.1f * powf(mvir, 1.0f/3.0f);
}

/**
 * @brief Placeholder for get_virial_velocity that returns a default value
 */
float get_virial_velocity(int halonr, const struct halo_data *halos, const struct params *run_params)
{
    if (halonr < 0) {
        return 0.0;
    }
    /* In core-only mode, calculate a simple estimate based on mass */
    float mvir = halos[halonr].Mvir;
    /* V_vir ∝ M_vir^(1/3) - simplified scale factor of 0.5 */
    return 0.5f * powf(mvir, 1.0f/3.0f);
}
