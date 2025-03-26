#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    #include "core_allvars.h"

    /* Main initialization and cleanup functions */
    extern void init(struct params *run_params);
    extern void cleanup(struct params *run_params);

    /* Units and constants initialization */
    extern void initialize_units(struct params *run_params);
    extern void cleanup_units(struct params *run_params);
    
    /* Simulation times initialization */
    extern void initialize_simulation_times(struct params *run_params);
    extern void cleanup_simulation_times(struct params *run_params);
    
    /* Cooling system initialization */
    extern void initialize_cooling(void);
    extern void cleanup_cooling(void);
    
    /* Module system initialization */
    extern void initialize_module_system(struct params *run_params);
    extern void cleanup_module_system(void);
    
    /* Galaxy extension system initialization */
    extern void initialize_galaxy_extension_system(void);
    extern void cleanup_galaxy_extension_system(void);
    
    /* Event system initialization */
    extern void initialize_event_system(void);
    extern void cleanup_event_system(void);
    
    /* Pipeline system initialization */
    extern void initialize_pipeline_system(void);
    extern void cleanup_pipeline_system(void);
    
    /* Configuration system initialization */
    extern void initialize_config_system(const char *config_file);
    extern void cleanup_config_system(void);
    
    /* Evolution context initialization and validation */
    extern void initialize_evolution_context(struct evolution_context *ctx, 
                                           const int halonr,
                                           struct GALAXY *galaxies, 
                                           const int ngal,
                                           struct halo_data *halos,
                                           struct params *run_params);
    extern void cleanup_evolution_context(struct evolution_context *ctx);
    extern bool validate_evolution_context(const struct evolution_context *ctx);

#ifdef __cplusplus
}
#endif

