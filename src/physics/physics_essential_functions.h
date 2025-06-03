/**
 * @file physics_essential_functions.h
 * @brief Header for essential physics functions required for core-physics separation
 *
 * Provides declarations for minimal physics functions that enable core SAGE
 * infrastructure to operate independently from complex physics implementations.
 * These functions support "physics-free mode" execution with empty pipelines.
 */

#pragma once

#include "core_allvars.h"

/* Essential physics function declarations for physics-free mode */

void init_galaxy(int p, int halonr, const struct halo_data *halos, 
                 struct GALAXY *galaxies, const struct params *run_params);

double get_virial_mass(const int halonr, const struct halo_data *halos, const struct params *run_params);
double get_virial_radius(const int halonr, const struct halo_data *halos, const struct params *run_params);
double get_virial_velocity(const int halonr, const struct halo_data *halos, const struct params *run_params);
double estimate_merging_time(const int halonr, const int mother_halo, const struct halo_data *halos,
                            const double time, const struct params *run_params);

void deal_with_galaxy_merger(int p, int merger_centralgal, int centralgal, double time,
                            int ngal, struct GALAXY *galaxy, struct params *run_params);
