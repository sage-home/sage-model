/**
 * @file placeholder_model_misc.h
 * @brief Placeholder implementations for model_misc.c functions in core-only mode
 *
 * This file provides declarations for placeholder functions that replace
 * physics-specific functions from model_misc.c in core-only builds.
 */

#ifndef PLACEHOLDER_MODEL_MISC_H
#define PLACEHOLDER_MODEL_MISC_H

#include "../core/core_allvars.h"

/* Virial-related placeholder functions */
float get_virial_mass(int halonr, const struct halo_data *halos, const struct params *run_params);
float get_virial_radius(int halonr, const struct halo_data *halos, const struct params *run_params);
float get_virial_velocity(int halonr, const struct halo_data *halos, const struct params *run_params);

/* Other placeholder functions needed */
void init_galaxy(struct GALAXY *g, int p, int snap);
double estimate_merging_time(int halonr, struct halo_data *halos, const struct params *run_params);
void deal_with_galaxy_merger(int p, int merger_centralgal, int centralgal, double time, 
                           int ngal, struct GALAXY *galaxy, struct params *run_params);
void disrupt_satellite_to_ICS(int gal, int centralgal, int ngal, struct GALAXY *galaxy);
void *cooling_module_create(void);
void *infall_module_create(void);

#endif /* PLACEHOLDER_MODEL_MISC_H */
