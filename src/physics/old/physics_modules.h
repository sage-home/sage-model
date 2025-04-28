#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "../core/core_parameter_views.h"
#include "../core/core_pipeline_system.h"

/* Common physics module interface */
struct physics_module_interface {
    struct base_module base;         /* Base module fields */
    void *module_data;              /* Module-specific data */
    
    /* Initialization/cleanup */
    int (*initialize)(void **module_data);
    void (*cleanup)(void *module_data);
    
    /* Core physics interface */
    int (*execute_halo_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_galaxy_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_post_phase)(void *module_data, struct pipeline_context *context);
    int (*execute_final_phase)(void *module_data, struct pipeline_context *context);
};

/* Common callback functions that modules can register */
struct physics_module_callbacks {
    /* Property getters */
    double (*get_metallicity)(struct GALAXY *galaxy);
    double (*get_cooling_rate)(struct GALAXY *galaxy);
    double (*get_star_formation_rate)(struct GALAXY *galaxy);
    double (*get_black_hole_accretion_rate)(struct GALAXY *galaxy);
    
    /* Property setters */
    void (*set_metallicity)(struct GALAXY *galaxy, double value);
    void (*set_cooling_rate)(struct GALAXY *galaxy, double value);
    void (*set_star_formation_rate)(struct GALAXY *galaxy, double value);
    void (*set_black_hole_accretion_rate)(struct GALAXY *galaxy, double value);
    
    /* Common calculations */
    double (*calculate_cooling_radius)(struct GALAXY *galaxy, double cooling_rate);
    double (*calculate_disk_radius)(struct GALAXY *galaxy);
    double (*calculate_bulge_radius)(struct GALAXY *galaxy);
    double (*calculate_dynamical_time)(struct GALAXY *galaxy);
};

/* Individual physics module interfaces */

/* Infall module */
struct infall_module {
    struct physics_module_interface base;
    
    /* Infall-specific methods */
    double (*calculate_infall)(void *module_data, struct pipeline_context *context);
    int (*apply_infall)(void *module_data, struct pipeline_context *context, double infall_mass);
};

/* Cooling module */
struct cooling_module {
    struct physics_module_interface base;
    
    /* Cooling-specific methods */
    double (*calculate_cooling)(void *module_data, struct pipeline_context *context);
    int (*apply_cooling)(void *module_data, struct pipeline_context *context, double cooling_mass);
    double (*get_cooling_radius)(void *module_data, struct pipeline_context *context);
};

/* Star formation module */
struct star_formation_module {
    struct physics_module_interface base;
    
    /* Star formation methods */
    double (*calculate_star_formation)(void *module_data, struct pipeline_context *context);
    int (*form_stars)(void *module_data, struct pipeline_context *context, double star_mass);
};

/* Feedback module */
struct feedback_module {
    struct physics_module_interface base;
    
    /* Feedback methods */
    double (*calculate_feedback)(void *module_data, struct pipeline_context *context);
    int (*apply_feedback)(void *module_data, struct pipeline_context *context);
    int (*calculate_metals)(void *module_data, struct pipeline_context *context);
};

/* AGN module */
struct agn_module {
    struct physics_module_interface base;
    
    /* AGN methods */
    double (*calculate_accretion)(void *module_data, struct pipeline_context *context);
    int (*apply_feedback)(void *module_data, struct pipeline_context *context);
    double (*calculate_heating)(void *module_data, struct pipeline_context *context);
};

/* Disk instability module */
struct disk_instability_module {
    struct physics_module_interface base;
    
    /* Disk instability methods */
    double (*check_stability)(void *module_data, struct pipeline_context *context);
    int (*handle_instability)(void *module_data, struct pipeline_context *context);
};

/* Merger module */
struct merger_module {
    struct physics_module_interface base;
    
    /* Merger methods */
    double (*calculate_merger_time)(void *module_data, struct pipeline_context *context);
    int (*process_merger)(void *module_data, struct pipeline_context *context);
    int (*handle_disruption)(void *module_data, struct pipeline_context *context);
};

/* Module initialization functions */
extern int infall_module_create(struct infall_module **module);
extern int cooling_module_create(struct cooling_module **module);
extern int star_formation_module_create(struct star_formation_module **module);
extern int feedback_module_create(struct feedback_module **module);
extern int agn_module_create(struct agn_module **module);
extern int disk_instability_module_create(struct disk_instability_module **module);
extern int merger_module_create(struct merger_module **module);

#ifdef __cplusplus
}
#endif