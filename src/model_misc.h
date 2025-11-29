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
    extern double get_bulge_radius(const int p, struct GALAXY *galaxies, const struct params *run_params);
    extern double dmax(const double x, const double y);
    extern void determine_and_store_regime(const int ngal, struct GALAXY *galaxies, 
                                const struct params *run_params);
    extern float calculate_molecular_fraction_BR06(float gas_surface_density, float stellar_surface_density, float disk_scale_length_pc);
    extern float calculate_stellar_scale_height_BR06(float disk_scale_length_pc);
    extern float calculate_midplane_pressure_BR06(float sigma_gas, float sigma_stars, float disk_scale_length_pc);

    extern float calculate_molecular_fraction_radial_integration(const int gal, struct GALAXY *galaxies, 
                                                      const struct params *run_params);

    extern double calculate_ffb_threshold_mass(const double z, const struct params *run_params);
    extern double calculate_ffb_fraction(const double Mvir, const double z, const struct params *run_params);

    extern void determine_and_store_ffb_regime(const int ngal, struct GALAXY *galaxies,
                                            const struct params *run_params);
    extern void update_instability_bulge_radius(const int p, const double delta_mass, 
                                     struct GALAXY *galaxies, const struct params *run_params);


#ifdef __cplusplus
}
#endif
