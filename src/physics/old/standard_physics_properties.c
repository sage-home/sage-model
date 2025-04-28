#include "standard_physics_properties.h"
#include "../core/core_logging.h"
#include "../core/core_galaxy_extensions.h"
#include <string.h>

// Internal static property ID storage
static struct cooling_property_ids cooling_ids = { -1, -1, -1 };
static struct starformation_property_ids starformation_ids = { -1, -1, -1, -1, -1, -1 };
static struct agn_property_ids agn_ids = { -1, -1, -1 };
static struct infall_property_ids infall_ids = { -1, -1 };

// Registration stubs
int register_cooling_properties(int module_id) {
    LOG_DEBUG("register_cooling_properties() called for module_id=%d", module_id);
    galaxy_property_t prop;
    memset(&prop, 0, sizeof(prop));
    prop.module_id = module_id;
    prop.size = sizeof(double);
    
    strcpy(prop.name, "cooling_rate");
    strcpy(prop.description, "Gas cooling rate (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    cooling_ids.cooling_rate_id = galaxy_extension_register(&prop);
    if (cooling_ids.cooling_rate_id < 0) {
        LOG_ERROR("Failed to register cooling_rate property");
        return -1;
    }
    
    strcpy(prop.name, "heating_rate");
    strcpy(prop.description, "Gas heating rate (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    cooling_ids.heating_rate_id = galaxy_extension_register(&prop);
    if (cooling_ids.heating_rate_id < 0) {
        LOG_ERROR("Failed to register heating_rate property");
        return -1;
    }
    
    strcpy(prop.name, "cooling_radius");
    strcpy(prop.description, "Cooling radius (kpc)");
    strcpy(prop.units, "kpc");
    cooling_ids.cooling_radius_id = galaxy_extension_register(&prop);
    if (cooling_ids.cooling_radius_id < 0) {
        LOG_ERROR("Failed to register cooling_radius property");
        return -1;
    }
    return 0;
}

int register_starformation_properties(int module_id) {
    LOG_DEBUG("register_starformation_properties() called for module_id=%d", module_id);
    galaxy_property_t prop;
    memset(&prop, 0, sizeof(prop));
    prop.module_id = module_id;
    prop.size = sizeof(double);

    strcpy(prop.name, "sfr_disk");
    strcpy(prop.description, "Star formation rate in disk (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    starformation_ids.sfr_disk_id = galaxy_extension_register(&prop);
    if (starformation_ids.sfr_disk_id < 0) {
        LOG_ERROR("Failed to register sfr_disk property");
        return -1;
    }

    strcpy(prop.name, "sfr_bulge");
    strcpy(prop.description, "Star formation rate in bulge (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    starformation_ids.sfr_bulge_id = galaxy_extension_register(&prop);
    if (starformation_ids.sfr_bulge_id < 0) {
        LOG_ERROR("Failed to register sfr_bulge property");
        return -1;
    }

    strcpy(prop.name, "sfr_disk_cold_gas");
    strcpy(prop.description, "Cold gas used for disk SFR (Msun)");
    strcpy(prop.units, "Msun");
    starformation_ids.sfr_disk_cold_gas_id = galaxy_extension_register(&prop);
    if (starformation_ids.sfr_disk_cold_gas_id < 0) {
        LOG_ERROR("Failed to register sfr_disk_cold_gas property");
        return -1;
    }

    strcpy(prop.name, "sfr_disk_cold_gas_metals");
    strcpy(prop.description, "Metals in cold gas for disk SFR (Msun)");
    strcpy(prop.units, "Msun");
    starformation_ids.sfr_disk_cold_gas_metals_id = galaxy_extension_register(&prop);
    if (starformation_ids.sfr_disk_cold_gas_metals_id < 0) {
        LOG_ERROR("Failed to register sfr_disk_cold_gas_metals property");
        return -1;
    }

    strcpy(prop.name, "sfr_bulge_cold_gas");
    strcpy(prop.description, "Cold gas used for bulge SFR (Msun)");
    strcpy(prop.units, "Msun");
    starformation_ids.sfr_bulge_cold_gas_id = galaxy_extension_register(&prop);
    if (starformation_ids.sfr_bulge_cold_gas_id < 0) {
        LOG_ERROR("Failed to register sfr_bulge_cold_gas property");
        return -1;
    }

    strcpy(prop.name, "sfr_bulge_cold_gas_metals");
    strcpy(prop.description, "Metals in cold gas for bulge SFR (Msun)");
    strcpy(prop.units, "Msun");
    starformation_ids.sfr_bulge_cold_gas_metals_id = galaxy_extension_register(&prop);
    if (starformation_ids.sfr_bulge_cold_gas_metals_id < 0) {
        LOG_ERROR("Failed to register sfr_bulge_cold_gas_metals property");
        return -1;
    }
    return 0;
}

int register_agn_properties(int module_id) {
    LOG_DEBUG("register_agn_properties() called for module_id=%d", module_id);
    galaxy_property_t prop;
    memset(&prop, 0, sizeof(prop));
    prop.module_id = module_id;
    prop.size = sizeof(double);

    strcpy(prop.name, "quasar_accretion");
    strcpy(prop.description, "Quasar mode black hole accretion rate (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    agn_ids.quasar_accretion_id = galaxy_extension_register(&prop);
    if (agn_ids.quasar_accretion_id < 0) {
        LOG_ERROR("Failed to register quasar_accretion property");
        return -1;
    }

    strcpy(prop.name, "radio_accretion");
    strcpy(prop.description, "Radio mode black hole accretion rate (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    agn_ids.radio_accretion_id = galaxy_extension_register(&prop);
    if (agn_ids.radio_accretion_id < 0) {
        LOG_ERROR("Failed to register radio_accretion property");
        return -1;
    }

    strcpy(prop.name, "r_heat");
    strcpy(prop.description, "AGN heating radius (kpc)");
    strcpy(prop.units, "kpc");
    agn_ids.r_heat_id = galaxy_extension_register(&prop);
    if (agn_ids.r_heat_id < 0) {
        LOG_ERROR("Failed to register r_heat property");
        return -1;
    }
    return 0;
}

int register_infall_properties(int module_id) {
    LOG_DEBUG("register_infall_properties() called for module_id=%d", module_id);
    galaxy_property_t prop;
    memset(&prop, 0, sizeof(prop));
    prop.module_id = module_id;
    prop.size = sizeof(double);

    strcpy(prop.name, "infall_rate");
    strcpy(prop.description, "Gas infall rate (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    infall_ids.infall_rate_id = galaxy_extension_register(&prop);
    if (infall_ids.infall_rate_id < 0) {
        LOG_ERROR("Failed to register infall_rate property");
        return -1;
    }

    strcpy(prop.name, "outflow_rate");
    strcpy(prop.description, "Gas outflow rate (Msun/yr)");
    strcpy(prop.units, "Msun/yr");
    infall_ids.outflow_rate_id = galaxy_extension_register(&prop);
    if (infall_ids.outflow_rate_id < 0) {
        LOG_ERROR("Failed to register outflow_rate property");
        return -1;
    }
    return 0;
}

// Getters for property ID structs
const struct cooling_property_ids* get_cooling_property_ids(void) { return &cooling_ids; }
const struct starformation_property_ids* get_starformation_property_ids(void) { return &starformation_ids; }
const struct agn_property_ids* get_agn_property_ids(void) { return &agn_ids; }
const struct infall_property_ids* get_infall_property_ids(void) { return &infall_ids; }

// Utility accessors
double galaxy_get_cooling_rate(const struct GALAXY *galaxy) {
    int prop_id = cooling_ids.cooling_rate_id;
    if (prop_id < 0) {
        LOG_ERROR("cooling_rate property not registered");
        return 0.0;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to get cooling_rate property for galaxy");
        return 0.0;
    }
    return *value_ptr;
}

void galaxy_set_cooling_rate(struct GALAXY *galaxy, double value) {
    int prop_id = cooling_ids.cooling_rate_id;
    if (prop_id < 0) {
        LOG_ERROR("cooling_rate property not registered");
        return;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to set cooling_rate property for galaxy");
        return;
    }
    *value_ptr = value;
}

double galaxy_get_heating_rate(const struct GALAXY *galaxy) {
    int prop_id = cooling_ids.heating_rate_id;
    if (prop_id < 0) {
        LOG_ERROR("heating_rate property not registered");
        return 0.0;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to get heating_rate property for galaxy");
        return 0.0;
    }
    return *value_ptr;
}

void galaxy_set_heating_rate(struct GALAXY *galaxy, double value) {
    int prop_id = cooling_ids.heating_rate_id;
    if (prop_id < 0) {
        LOG_ERROR("heating_rate property not registered");
        return;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to set heating_rate property for galaxy");
        return;
    }
    *value_ptr = value;
}

double galaxy_get_quasar_accretion_rate(const struct GALAXY *galaxy) {
    int prop_id = agn_ids.quasar_accretion_id;
    if (prop_id < 0) {
        LOG_ERROR("quasar_accretion property not registered");
        return 0.0;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to get quasar_accretion property for galaxy");
        return 0.0;
    }
    return *value_ptr;
}

void galaxy_set_quasar_accretion_rate(struct GALAXY *galaxy, double value) {
    int prop_id = agn_ids.quasar_accretion_id;
    if (prop_id < 0) {
        LOG_ERROR("quasar_accretion property not registered");
        return;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to set quasar_accretion property for galaxy");
        return;
    }
    *value_ptr = value;
}

double galaxy_get_outflow_value(const struct GALAXY *galaxy) {
    int prop_id = infall_ids.outflow_rate_id;
    if (prop_id < 0) {
        LOG_ERROR("outflow_rate property not registered");
        return 0.0;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to get outflow_rate property for galaxy");
        return 0.0;
    }
    return *value_ptr;
}

void galaxy_set_outflow_value(struct GALAXY *galaxy, double value) {
    int prop_id = infall_ids.outflow_rate_id;
    if (prop_id < 0) {
        LOG_ERROR("outflow_rate property not registered");
        return;
    }
    double *value_ptr = (double*)galaxy_extension_get_data(galaxy, prop_id);
    if (value_ptr == NULL) {
        LOG_ERROR("Failed to set outflow_rate property for galaxy");
        return;
    }
    *value_ptr = value;
}
