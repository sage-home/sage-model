#pragma once

#include "../core/core_module_system.h"
#include "../core/core_allvars.h"
#include "../core/core_parameter_views.h"
#include "../core/core_galaxy_extensions.h"

// Property IDs structure - internal to the module
struct infall_property_ids {
    int infall_rate_id;
    int outflow_rate_id;
};

// Infall module function declarations (internal to the module)
double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params);
void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies);

// Property registration and access functions
int register_infall_properties(int module_id);
const struct infall_property_ids* get_infall_property_ids(void);
double galaxy_get_outflow_value(const struct GALAXY *galaxy);
void galaxy_set_outflow_value(struct GALAXY *galaxy, double value);

// Infall module interface - only this function is visible outside this module
struct base_module *infall_module_create(void);
