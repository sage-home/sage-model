#include "cooling_module.h"
#include "cooling_tables.h"
#include "../core/core_event_system.h"
#include "../core/core_logging.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_module_system.h"
#include "../core/core_galaxy_accessors.h"
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

    if(galaxies[gal].HotGas > 0.0 && galaxies[gal].Vvir > 0.0) {
        const double tcool = galaxies[gal].Rvir / galaxies[gal].Vvir;
        const double temp = 35.9 * galaxies[gal].Vvir * galaxies[gal].Vvir;         // in Kelvin

        double logZ = -10.0;
        if(galaxies[gal].MetalsHotGas > 0) {
            logZ = log10(galaxies[gal].MetalsHotGas / galaxies[gal].HotGas);
        }

        double lambda = get_metaldependent_cooling_rate(log10(temp), logZ);
        double x = PROTONMASS * BOLTZMANN * temp / lambda;        // now this has units sec g/cm^3
        x /= (cooling_params->UnitDensity_in_cgs * cooling_params->UnitTime_in_s);  // now in internal units
        const double rho_rcool = x / tcool * 0.885;  // 0.885 = 3/2 * mu, mu=0.59 for a fully ionized gas

        // an isothermal density profile for the hot gas is assumed here
        const double rho0 = galaxies[gal].HotGas / (4 * M_PI * galaxies[gal].Rvir);
        const double rcool = sqrt(rho0 / rho_rcool);

        coolingGas = 0.0;
        if(rcool > galaxies[gal].Rvir) {
            // "cold accretion" regime
            coolingGas = galaxies[gal].HotGas / (galaxies[gal].Rvir / galaxies[gal].Vvir) * dt;
        } else {
            // "hot halo cooling" regime
            coolingGas = (galaxies[gal].HotGas / galaxies[gal].Rvir) * (rcool / (2.0 * tcool)) * dt;
        }

        if(coolingGas > galaxies[gal].HotGas) {
            coolingGas = galaxies[gal].HotGas;
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
			galaxies[gal].Cooling += 0.5 * coolingGas * galaxies[gal].Vvir * galaxies[gal].Vvir;
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
	if(galaxies[centralgal].r_heat < rcool) {
		coolingGas = (1.0 - galaxies[centralgal].r_heat / rcool) * coolingGas;
    } else {
		coolingGas = 0.0;
    }

	XASSERT(coolingGas >= 0.0, -1,
            "Error: Cooling gas mass = %g should be >= 0.0", coolingGas);

	// now calculate the new heating rate
    if(galaxies[centralgal].HotGas > 0.0) {
        const struct params *run_params = agn_params->full_params; // For G access
        
        if(agn_params->AGNrecipeOn == 2) {
            // Bondi-Hoyle accretion recipe
            AGNrate = (2.5 * M_PI * run_params->cosmology.G) * (0.375 * 0.6 * x) * 
                    galaxies[centralgal].BlackHoleMass * agn_params->RadioModeEfficiency;
        } else if(agn_params->AGNrecipeOn == 3) {
            // Cold cloud accretion: trigger: rBH > 1.0e-4 Rsonic, and accretion rate = 0.01% cooling rate
            if(galaxies[centralgal].BlackHoleMass > 0.0001 * galaxies[centralgal].Mvir * CUBE(rcool/galaxies[centralgal].Rvir)) {
                AGNrate = 0.0001 * coolingGas / dt;
            } else {
                AGNrate = 0.0;
            }
        } else {
            // empirical (standard) accretion recipe
            if(galaxies[centralgal].Mvir > 0.0) {
                AGNrate = agn_params->RadioModeEfficiency / 
                        (agn_params->UnitMass_in_g / agn_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS) * 
                        (galaxies[centralgal].BlackHoleMass / 0.01) * 
                        CUBE(galaxies[centralgal].Vvir / 200.0) * 
                        ((galaxies[centralgal].HotGas / galaxies[centralgal].Mvir) / 0.1);
            } else {
                AGNrate = agn_params->RadioModeEfficiency / 
                        (agn_params->UnitMass_in_g / agn_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS) * 
                        (galaxies[centralgal].BlackHoleMass / 0.01) * 
                        CUBE(galaxies[centralgal].Vvir / 200.0);
            }
        }

        // Eddington rate
        EDDrate = (1.3e38 * galaxies[centralgal].BlackHoleMass * 1e10 / run_params->cosmology.Hubble_h) / 
                 (agn_params->UnitEnergy_in_cgs / agn_params->UnitTime_in_s) / (0.1 * 9e10);

        // accretion onto BH is always limited by the Eddington rate
        if(AGNrate > EDDrate) {
            AGNrate = EDDrate;
        }

        // accreted mass onto black hole
        AGNaccreted = AGNrate * dt;

        // cannot accrete more mass than is available!
        if(AGNaccreted > galaxies[centralgal].HotGas) {
            AGNaccreted = galaxies[centralgal].HotGas;
        }

        // coefficient to heat the cooling gas back to the virial temperature of the halo
        // 1.34e5 = sqrt(2*eta*c^2), eta=0.1 (standard efficiency) and c in km/s
        AGNcoeff = (1.34e5 / galaxies[centralgal].Vvir) * (1.34e5 / galaxies[centralgal].Vvir);

        // cooling mass that can be suppresed from AGN heating
        AGNheating = AGNcoeff * AGNaccreted;

        // The above is the maximal heating rate. We now limit it to the current cooling rate
        if(AGNheating > coolingGas) {
            AGNaccreted = coolingGas / AGNcoeff;
            AGNheating = coolingGas;
        }

        // accreted mass onto black hole
        metallicity = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
        galaxies[centralgal].BlackHoleMass += AGNaccreted;
        galaxies[centralgal].HotGas -= AGNaccreted;
        galaxies[centralgal].MetalsHotGas -= metallicity * AGNaccreted;

        // update the heating radius as needed
        if(galaxies[centralgal].r_heat < rcool && coolingGas > 0.0) {
            double r_heat_new = (AGNheating / coolingGas) * rcool;
            if(r_heat_new > galaxies[centralgal].r_heat) {
                galaxies[centralgal].r_heat = r_heat_new;
            }
        }

        if (AGNheating > 0.0) {
            galaxies[centralgal].Heating += 0.5 * AGNheating * galaxies[centralgal].Vvir * galaxies[centralgal].Vvir;
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
        
        if(coolingGas < galaxies[centralgal].HotGas) {
            const double metallicity = get_metallicity(galaxies[centralgal].HotGas, galaxies[centralgal].MetalsHotGas);
            galaxies[centralgal].ColdGas += coolingGas;
            galaxies[centralgal].MetalsColdGas += metallicity * coolingGas;
            galaxies[centralgal].HotGas -= coolingGas;
            galaxies[centralgal].MetalsHotGas -= metallicity * coolingGas;
            actual_cooled_gas = coolingGas;
        } else {
            actual_cooled_gas = galaxies[centralgal].HotGas;
            galaxies[centralgal].ColdGas += galaxies[centralgal].HotGas;
            galaxies[centralgal].MetalsColdGas += galaxies[centralgal].MetalsHotGas;
            galaxies[centralgal].HotGas = 0.0;
            galaxies[centralgal].MetalsHotGas = 0.0;
        }
        
        // If the event system is initialized, emit a cooling event
        if (event_system_is_initialized()) {
            // Calculate cooling radius - this is approximate since we don't have rcool here
            if(galaxies[centralgal].Rvir > 0.0 && galaxies[centralgal].Vvir > 0.0) {
                cooling_radius = galaxies[centralgal].Rvir * sqrt(actual_cooled_gas / galaxies[centralgal].HotGas);
                if(cooling_radius > galaxies[centralgal].Rvir) {
                    cooling_radius = galaxies[centralgal].Rvir;
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
    galaxy_set_cooling_rate(&galaxies[p], 0.5 * coolingGas * galaxies[p].Vvir * galaxies[p].Vvir);
    
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
