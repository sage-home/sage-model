#include "cooling_module.h"
#include "cooling_tables.h"
#include "../core/core_event_system.h"
#include "../core/core_logging.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_module_system.h"
#include "../core/core_galaxy_accessors.h"
#include "../core/core_properties.h"    // For GALAXY_PROP_* macros
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Internal constants for the cooling module
#define PROTONMASS 1.6726e-24
#define BOLTZMANN 1.3806e-16
#define SEC_PER_YEAR 3.155e7
#define SOLAR_MASS 1.989e33

// Module data structure
struct cooling_module_data {
    int module_id;
    struct cooling_property_ids prop_ids;
    char root_dir[PATH_MAX];  // Store root directory for locating data files
};

// Internal static property ID storage
static struct cooling_property_ids cooling_ids = { -1, -1, -1 };

// Helper function to get metallicity ratio safely
static double get_metallicity(double mass, double metals) {
    if (mass > 0.0) {
        return metals / mass;
    }
    return 0.0;
}

// Property registration implementation
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

// Getter for property ID struct
const struct cooling_property_ids* get_cooling_property_ids(void) { 
    return &cooling_ids; 
}

// Primary cooling recipe implementation
double cooling_recipe(const int gal, const double dt, struct GALAXY *galaxies, const struct cooling_params_view *cooling_params) {
    double coolingGas;

    if(GALAXY_PROP_HotGas(&galaxies[gal]) > 0.0 && GALAXY_PROP_Vvir(&galaxies[gal]) > 0.0) {
        const double tcool = GALAXY_PROP_Rvir(&galaxies[gal]) / GALAXY_PROP_Vvir(&galaxies[gal]);
        const double temp = 35.9 * GALAXY_PROP_Vvir(&galaxies[gal]) * GALAXY_PROP_Vvir(&galaxies[gal]);         // in Kelvin

        double logZ = -10.0;
        if(GALAXY_PROP_MetalsHotGas(&galaxies[gal]) > 0) {
            logZ = log10(GALAXY_PROP_MetalsHotGas(&galaxies[gal]) / GALAXY_PROP_HotGas(&galaxies[gal]));
        }

        double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
        double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
        x /= (cooling_params->UnitDensity_in_cgs * cooling_params->UnitTime_in_s);  // now in internal units
        const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

        // an isothermal density profile for the hot gas is assumed here
        const double rho0 = GALAXY_PROP_HotGas(&galaxies[gal]) / (4 * M_PI * GALAXY_PROP_Rvir(&galaxies[gal]));
        const double rcool = sqrt(rho0 / rho_rcool);

        coolingGas = 0.0;
        if(rcool > GALAXY_PROP_Rvir(&galaxies[gal])) {
            // "cold accretion" regime
            coolingGas = GALAXY_PROP_HotGas(&galaxies[gal]) / (GALAXY_PROP_Rvir(&galaxies[gal]) / GALAXY_PROP_Vvir(&galaxies[gal])) * dt;
        } else {
            // "hot halo cooling" regime
            coolingGas = (GALAXY_PROP_HotGas(&galaxies[gal]) / GALAXY_PROP_Rvir(&galaxies[gal])) * (rcool / (2.0 * tcool)) * dt;
        }

        if(coolingGas > GALAXY_PROP_HotGas(&galaxies[gal])) {
            coolingGas = GALAXY_PROP_HotGas(&galaxies[gal]);
        } else {
			if(coolingGas < 0.0) coolingGas = 0.0;
        }

		// at this point we have calculated the maximal cooling rate
		// if AGNrecipeOn we now reduce it in line with past heating before proceeding
        if(cooling_params->AGNrecipeOn > 0 && coolingGas > 0.0) {
            // Create AGN params view for do_AGN_heating
            struct agn_params_view agn_params;
            initialize_agn_params_view(&agn_params, cooling_params->full_params);
            
            coolingGas = do_AGN_heating(coolingGas, gal, dt, x, rcool, galaxies, &agn_params);
        }

		if (coolingGas > 0.0) {
			GALAXY_PROP_Cooling(&galaxies[gal]) += 0.5 * coolingGas * GALAXY_PROP_Vvir(&galaxies[gal]) * GALAXY_PROP_Vvir(&galaxies[gal]);
        }
	} else {
		coolingGas = 0.0;
    }

	XASSERT(coolingGas >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);
    return coolingGas;
}

// AGN heating implementation
double do_AGN_heating(double coolingGas, const int centralgal, const double dt, const double x, 
                     const double rcool, struct GALAXY *galaxies, const struct agn_params_view *agn_params) {
    double AGNrate, EDDrate, AGNaccreted, AGNcoeff, AGNheating, metallicity;

	// first update the cooling rate based on the past AGN heating
	if(GALAXY_PROP_r_heat(&galaxies[centralgal]) < rcool) {
		coolingGas = (1.0 - GALAXY_PROP_r_heat(&galaxies[centralgal]) / rcool) * coolingGas;
    } else {
		coolingGas = 0.0;
    }

	XASSERT(coolingGas >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);

	// now calculate the new heating rate
    if(GALAXY_PROP_HotGas(&galaxies[centralgal]) > 0.0) {
        const struct params *run_params = agn_params->full_params; // For G access
        
        if(agn_params->AGNrecipeOn == 2) {
            // Bondi-Hoyle accretion recipe
            AGNrate = (2.5 * M_PI * run_params->cosmology.G) * (0.375 * 0.6 * x) * 
                    GALAXY_PROP_BlackHoleMass(&galaxies[centralgal]) * agn_params->RadioModeEfficiency;
        } else if(agn_params->AGNrecipeOn == 3) {
            // Cold cloud accretion: trigger: rBH > 1.0e-4 Rsonic, and accretion rate = 0.01% cooling rate
            if(GALAXY_PROP_BlackHoleMass(&galaxies[centralgal]) > 0.0001 * GALAXY_PROP_Mvir(&galaxies[centralgal]) * 
               CUBE(rcool/GALAXY_PROP_Rvir(&galaxies[centralgal]))) {
                AGNrate = 0.0001 * coolingGas / dt;
            } else {
                AGNrate = 0.0;
            }
        } else {
            // empirical (standard) accretion recipe
            if(GALAXY_PROP_Mvir(&galaxies[centralgal]) > 0.0) {
                AGNrate = agn_params->RadioModeEfficiency / 
                        (agn_params->UnitMass_in_g / agn_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS) * 
                        (GALAXY_PROP_BlackHoleMass(&galaxies[centralgal]) / 0.01) * 
                        CUBE(GALAXY_PROP_Vvir(&galaxies[centralgal]) / 200.0) * 
                        ((GALAXY_PROP_HotGas(&galaxies[centralgal]) / GALAXY_PROP_Mvir(&galaxies[centralgal])) / 0.1);
            } else {
                AGNrate = agn_params->RadioModeEfficiency / 
                        (agn_params->UnitMass_in_g / agn_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS) * 
                        (GALAXY_PROP_BlackHoleMass(&galaxies[centralgal]) / 0.01) * 
                        CUBE(GALAXY_PROP_Vvir(&galaxies[centralgal]) / 200.0);
            }
        }

        // Eddington rate
        EDDrate = (1.3e38 * GALAXY_PROP_BlackHoleMass(&galaxies[centralgal]) * 1e10 / run_params->cosmology.Hubble_h) / 
                 (agn_params->UnitEnergy_in_cgs / agn_params->UnitTime_in_s) / (0.1 * 9e10);

        // accretion onto BH is always limited by the Eddington rate
        if(AGNrate > EDDrate) {
            AGNrate = EDDrate;
        }

        // accreted mass onto black hole
        AGNaccreted = AGNrate * dt;

        // cannot accrete more mass than is available!
        if(AGNaccreted > GALAXY_PROP_HotGas(&galaxies[centralgal])) {
            AGNaccreted = GALAXY_PROP_HotGas(&galaxies[centralgal]);
        }

        // coefficient to heat the cooling gas back to the virial temperature of the halo
        // 1.34e5 = sqrt(2*eta*c^2), eta=0.1 (standard efficiency) and c in km/s
        AGNcoeff = (1.34e5 / GALAXY_PROP_Vvir(&galaxies[centralgal])) * (1.34e5 / GALAXY_PROP_Vvir(&galaxies[centralgal]));

        // cooling mass that can be suppresed from AGN heating
        AGNheating = AGNcoeff * AGNaccreted;

        // The above is the maximal heating rate. We now limit it to the current cooling rate
        if(AGNheating > coolingGas) {
            AGNaccreted = coolingGas / AGNcoeff;
            AGNheating = coolingGas;
        }

        // accreted mass onto black hole
        metallicity = get_metallicity(GALAXY_PROP_HotGas(&galaxies[centralgal]), GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]));
        GALAXY_PROP_BlackHoleMass(&galaxies[centralgal]) += AGNaccreted;
        GALAXY_PROP_HotGas(&galaxies[centralgal]) -= AGNaccreted;
        GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]) -= metallicity * AGNaccreted;

        // update the heating radius as needed
        if(GALAXY_PROP_r_heat(&galaxies[centralgal]) < rcool && coolingGas > 0.0) {
            double r_heat_new = (AGNheating / coolingGas) * rcool;
            if(r_heat_new > GALAXY_PROP_r_heat(&galaxies[centralgal])) {
                GALAXY_PROP_r_heat(&galaxies[centralgal]) = r_heat_new;
            }
        }

        if (AGNheating > 0.0) {
            GALAXY_PROP_Heating(&galaxies[centralgal]) += 0.5 * AGNheating * GALAXY_PROP_Vvir(&galaxies[centralgal]) * GALAXY_PROP_Vvir(&galaxies[centralgal]);
        }
    }

    return coolingGas;
}

// Function to transfer cooled gas to the galaxy
void cool_gas_onto_galaxy(const int centralgal, const double coolingGas, struct GALAXY *galaxies) {
    // add the fraction 1/STEPS of the total cooling gas to the cold disk
    if(coolingGas > 0.0) {
        double actual_cooled_gas = 0.0;
        double cooling_radius = 0.0;
        
        if(coolingGas < GALAXY_PROP_HotGas(&galaxies[centralgal])) {
            const double metallicity = get_metallicity(GALAXY_PROP_HotGas(&galaxies[centralgal]), GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]));
            GALAXY_PROP_ColdGas(&galaxies[centralgal]) += coolingGas;
            GALAXY_PROP_MetalsColdGas(&galaxies[centralgal]) += metallicity * coolingGas;
            GALAXY_PROP_HotGas(&galaxies[centralgal]) -= coolingGas;
            GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]) -= metallicity * coolingGas;
            actual_cooled_gas = coolingGas;
        } else {
            actual_cooled_gas = GALAXY_PROP_HotGas(&galaxies[centralgal]);
            GALAXY_PROP_ColdGas(&galaxies[centralgal]) += GALAXY_PROP_HotGas(&galaxies[centralgal]);
            GALAXY_PROP_MetalsColdGas(&galaxies[centralgal]) += GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]);
            GALAXY_PROP_HotGas(&galaxies[centralgal]) = 0.0;
            GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]) = 0.0;
        }
        
        // If the event system is initialized, emit a cooling event
        if (event_system_is_initialized()) {
            // Calculate cooling radius - this is approximate since we don't have rcool here
            if(GALAXY_PROP_Rvir(&galaxies[centralgal]) > 0.0 && GALAXY_PROP_Vvir(&galaxies[centralgal]) > 0.0) {
                cooling_radius = GALAXY_PROP_Rvir(&galaxies[centralgal]) * sqrt(actual_cooled_gas / GALAXY_PROP_HotGas(&galaxies[centralgal]));
                if(cooling_radius > GALAXY_PROP_Rvir(&galaxies[centralgal])) {
                    cooling_radius = GALAXY_PROP_Rvir(&galaxies[centralgal]);
                }
            }
            
            // Prepare cooling event data
            struct { 
                float cooling_rate; 
                float cooling_radius; 
                float hot_gas_cooled; 
            } cooling_data = {
                .cooling_rate = (float)(actual_cooled_gas),
                .cooling_radius = (float)(cooling_radius),
                .hot_gas_cooled = (float)(actual_cooled_gas)
            };
            
            // Emit the cooling event
            event_emit(
                EVENT_COOLING_COMPLETED,    // Event type
                0,                          // Source module ID (0 = traditional code)
                centralgal,                 // Galaxy index
                -1,                         // Step not available in this context
                &cooling_data,              // Event data
                sizeof(cooling_data),       // Size of event data
                EVENT_FLAG_NONE             // No special flags
            );
        }
    }
}

static int cooling_module_initialize(struct params *params, void **module_data) {
    /* Silence unused parameter warning */
    (void)params;
    struct cooling_module_data *data = malloc(sizeof(struct cooling_module_data));
    if (!data) return -1;

    // Get the current module ID (or use 0 if not available)
    int module_id = module_get_active_by_type(MODULE_TYPE_COOLING, NULL, NULL);
    if (module_id < 0) module_id = 0; // Use 0 if not active (e.g., during tests)

    // Register extension properties (if any are specific to this module)
    // register_cooling_properties(module_id); // This registers properties, not accessors
    // data->prop_ids = cooling_ids; // Store IDs if needed by the module

    data->module_id = module_id;

    // REMOVED THIS LINE: register_cooling_accessors(module_id);

    // Store the root directory for data file access
    // Use ROOT_DIR from Makefile if defined and available via params
    #ifdef ROOT_DIR
    strncpy(data->root_dir, ROOT_DIR, PATH_MAX - 1);
    #else
    // Fallback to current directory
    strncpy(data->root_dir, ".", PATH_MAX - 1);
    #endif
    data->root_dir[PATH_MAX - 1] = '\0';  // Ensure null termination

    // Initialize cooling tables
    read_cooling_functions(data->root_dir);

    *module_data = data;
    return 0;
}

/**
 * Compatibility function for legacy cooling calculations
 * 
 * This function provides a backward compatibility interface for modules 
 * that need to access the cooling implementation when called outside
 * the module framework.
 * 
 * @param gal Galaxy index
 * @param dt Time step
 * @param galaxies Array of galaxies
 * @param run_params Global parameters
 * @return Amount of gas that cooled
 */
double cooling_recipe_compat(const int gal, const double dt, struct GALAXY *galaxies, const struct params *run_params)
{
    struct cooling_params_view cooling_params;
    initialize_cooling_params_view(&cooling_params, run_params);
    
    return cooling_recipe(gal, dt, galaxies, &cooling_params);
}

static int cooling_module_execute_galaxy_phase(void *module_data, struct pipeline_context *context) {
    struct cooling_module_data *data = (struct cooling_module_data*)module_data;
    struct GALAXY *galaxies = context->galaxies;
    int p = context->current_galaxy;
    double dt = context->dt / STEPS;
    
    // Call cooling calculation
    struct cooling_params_view cooling_params;
    initialize_cooling_params_view(&cooling_params, context->params);
    double coolingGas = cooling_recipe(p, dt, galaxies, &cooling_params);
    cool_gas_onto_galaxy(p, coolingGas, galaxies);
    galaxy_set_cooling_rate(&galaxies[p], 0.5 * coolingGas * GALAXY_PROP_Vvir(&galaxies[p]) * GALAXY_PROP_Vvir(&galaxies[p]));
    
    // Emit event if needed
    if (event_system_is_initialized() && coolingGas > 0.0) {
        struct { 
            float cooling_rate; 
            float cooling_radius; 
            float hot_gas_cooled; 
        } cooling_data = {
            .cooling_rate = (float)(coolingGas / dt),
            .cooling_radius = 0.0f, // Could be calculated if needed
            .hot_gas_cooled = (float)coolingGas
        };
        event_emit(EVENT_COOLING_COMPLETED, data->module_id, p, context->step, &cooling_data, sizeof(cooling_data), EVENT_FLAG_NONE);
    }
    return 0;
}

struct base_module *cooling_module_create(void) {
    struct base_module *module = malloc(sizeof(struct base_module));
    if (!module) return NULL;
    memset(module, 0, sizeof(struct base_module));
    snprintf(module->name, MAX_MODULE_NAME, "StandardCooling");
    snprintf(module->version, MAX_MODULE_VERSION, "1.0.0");
    snprintf(module->author, MAX_MODULE_AUTHOR, "SAGE Team");
    module->type = MODULE_TYPE_COOLING;
    module->module_id = -1;
    module->initialize = cooling_module_initialize;
    module->cleanup = NULL;
    module->execute_galaxy_phase = cooling_module_execute_galaxy_phase;
    module->phases = PIPELINE_PHASE_GALAXY;
    return module;
}
