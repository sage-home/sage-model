#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* functions in model_misc.c */
    extern void init_galaxy(const int p, const int halonr, int *galaxycounter, const struct halo_data *halos, struct GALAXY *galaxies, const struct params *run_params);
    extern double get_metallicity(const double gas, const double metals);
    extern double get_virial_velocity(const int halonr, const struct halo_data *halos, const struct params *run_params);
    extern double get_virial_radius(const int halonr, const struct halo_data *halos, const struct params *run_params);
    extern double get_virial_mass(const int halonr, const struct halo_data *halos, const struct params *run_params);
    extern double get_disk_radius(const int halonr, const int p, const struct halo_data *halos, const struct GALAXY *galaxies);
    extern double dmax(const double x, const double y);
    extern void determine_and_store_regime(const int gal, struct GALAXY *galaxies);
    extern void final_regime_check(const int gal, struct GALAXY *galaxies, const struct params *run_params);
    extern double calculate_rcool_to_rvir_ratio(const int gal, struct GALAXY *galaxies, const struct params *run_params);
    extern float calculate_muratov_mass_loading(const int gal, struct GALAXY *galaxies, const double z);
    extern float calculate_molecular_fraction_BR06(float gas_surface_density, float stellar_surface_density, float disk_scale_length_pc);
    extern float calculate_molecular_fraction_darksage_pressure(float gas_surface_density_msun_pc2, 
                                                  float stellar_surface_density_msun_pc2,
                                                  float gas_velocity_dispersion_km_s,
                                                  float stellar_velocity_dispersion_km_s,
                                                  float disk_alignment_angle_deg,
                                                  const struct params *run_params);
    extern float calculate_stellar_scale_height_BR06(float disk_scale_length_pc);
    extern float calculate_midplane_pressure_BR06(float sigma_gas, float sigma_stars, float disk_scale_length_pc);
    extern float calculate_molecular_fraction_GD14(float gas_surface_density, float metallicity, float spatial_scale_pc);

#ifdef __cplusplus
}
#endif
