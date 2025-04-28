#pragma once

#include "../../core/core_module_system.h"
#include "../../core/core_allvars.h"
#include "../../core/core_parameter_views.h"
#include "../../core/core_galaxy_extensions.h"
#include <limits.h> // For PATH_MAX

// Property IDs structure - internal to the module
struct cooling_property_ids {
    int cooling_rate_id;
    int heating_rate_id;
    int cooling_radius_id;
};

// Cooling module function declarations (internal to the module)
double cooling_recipe(const int gal, const double dt, struct GALAXY *galaxies, const struct cooling_params_view *cooling_params);
void cool_gas_onto_galaxy(const int centralgal, const double coolingGas, struct GALAXY *galaxies);
double do_AGN_heating(double coolingGas, const int centralgal, const double dt, const double x, const double rcool, 
                     struct GALAXY *galaxies, const struct agn_params_view *agn_params);

// Property registration and access functions
int register_cooling_properties(int module_id);
const struct cooling_property_ids* get_cooling_property_ids(void);

// Module-specific accessors for cooling-related properties
double galaxy_get_cooling_rate(const struct GALAXY *galaxy);
void galaxy_set_cooling_rate(struct GALAXY *galaxy, double value);
double galaxy_get_heating_rate(const struct GALAXY *galaxy);
void galaxy_set_heating_rate(struct GALAXY *galaxy, double value);

// Compatibility function for backward compatibility
double cooling_recipe_compat(const int gal, const double dt, 
                           struct GALAXY *galaxies, 
                           const struct params *run_params);

// Cooling module interface - only this function is visible outside this module
struct base_module *cooling_module_create(void);
