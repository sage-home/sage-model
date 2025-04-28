#pragma once

#include "../core/core_galaxy_extensions.h"
#include "../core/core_module_system.h"

// Standard property IDs - global references to extension properties
struct cooling_property_ids {
    int cooling_rate_id;
    int heating_rate_id;
    int cooling_radius_id;
};

struct starformation_property_ids {
    int sfr_disk_id;
    int sfr_bulge_id;
    int sfr_disk_cold_gas_id;
    int sfr_disk_cold_gas_metals_id;
    int sfr_bulge_cold_gas_id;
    int sfr_bulge_cold_gas_metals_id;
};

struct agn_property_ids {
    int quasar_accretion_id;
    int radio_accretion_id;
    int r_heat_id;
};

struct infall_property_ids {
    int infall_rate_id;
    int outflow_rate_id;
};

// Register extension properties for each physics domain
int register_cooling_properties(int module_id);
int register_starformation_properties(int module_id);
int register_agn_properties(int module_id);
int register_infall_properties(int module_id);

// Get registered property IDs for each module type
const struct cooling_property_ids* get_cooling_property_ids(void);
const struct starformation_property_ids* get_starformation_property_ids(void);
const struct agn_property_ids* get_agn_property_ids(void);
const struct infall_property_ids* get_infall_property_ids(void);

// Utility functions to access properties with error handling
double galaxy_get_cooling_rate(const struct GALAXY *galaxy);
void galaxy_set_cooling_rate(struct GALAXY *galaxy, double value);
double galaxy_get_heating_rate(const struct GALAXY *galaxy);
void galaxy_set_heating_rate(struct GALAXY *galaxy, double value);
double galaxy_get_quasar_accretion_rate(const struct GALAXY *galaxy);
void galaxy_set_quasar_accretion_rate(struct GALAXY *galaxy, double value);
double galaxy_get_outflow_value(const struct GALAXY *galaxy);
void galaxy_set_outflow_value(struct GALAXY *galaxy, double value);
