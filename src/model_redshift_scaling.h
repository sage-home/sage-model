#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

// Scaling method definitions
#define SCALING_NONE 0
#define SCALING_POWER_LAW 1
#define SCALING_EXPONENTIAL 2

// Functions to get scaled parameters
double get_redshift_scaled_sf_efficiency(const struct params *run_params, double redshift);
double get_redshift_scaled_feedback_ejection(const struct params *run_params, double redshift);
double get_redshift_scaled_reincorp_factor(const struct params *run_params, double redshift);
double get_redshift_scaled_quasar_efficiency(const struct params *run_params, double redshift);
double get_redshift_scaled_radio_efficiency(const struct params *run_params, double redshift);

// Initialization function
void init_redshift_scaling_params(struct params *run_params);

#ifdef __cplusplus
}
#endif