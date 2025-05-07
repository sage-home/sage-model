#include "infall_module.h"
#include "../core/core_event_system.h"
#include "../core/core_logging.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_module_system.h"
#include "../core/core_properties.h"    // For GALAXY_PROP_* macros
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Internal constants and definitions
#ifndef CUBE
#define CUBE(x) ((x)*(x)*(x))
#endif
#define dmax(a,b) ((a)>(b)?(a):(b))

// Module data structure
struct infall_module_data {
    int module_id;
    struct infall_property_ids prop_ids;
    double current_infall;
};

// Internal static property ID storage
static struct infall_property_ids infall_ids = { -1, -1 };

// Helper function to get metallicity ratio safely
static double get_metallicity(double mass, double metals) {
    if (mass > 0.0) {
        return metals / mass;
    }
    return 0.0;
}

// Property registration implementation
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

// Getter for property ID struct
const struct infall_property_ids* get_infall_property_ids(void) {
    return &infall_ids;
}

// Utility accessors
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

// Main infall recipe implementation
double infall_recipe(const int centralgal, const int ngal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params) {
    double tot_stellarMass, tot_BHMass, tot_coldMass, tot_hotMass, tot_ejected, tot_ICS;
    double tot_ejectedMetals, tot_ICSMetals;
    double infallingMass, reionization_modifier;

    // need to add up all the baryonic mass associated with the full halo
    tot_stellarMass = tot_coldMass = tot_hotMass = tot_ejected = tot_BHMass = tot_ejectedMetals = tot_ICS = tot_ICSMetals = 0.0;

    // loop over all galaxies in the FoF-halo
    for(int i = 0; i < ngal; i++) {
        tot_stellarMass += GALAXY_PROP_StellarMass(&galaxies[i]);
        tot_BHMass += GALAXY_PROP_BlackHoleMass(&galaxies[i]);
        tot_coldMass += GALAXY_PROP_ColdGas(&galaxies[i]);
        tot_hotMass += GALAXY_PROP_HotGas(&galaxies[i]);
        tot_ejected += GALAXY_PROP_EjectedMass(&galaxies[i]);
        tot_ejectedMetals += GALAXY_PROP_MetalsEjectedMass(&galaxies[i]);
        tot_ICS += GALAXY_PROP_ICS(&galaxies[i]);
        tot_ICSMetals += GALAXY_PROP_MetalsICS(&galaxies[i]);

        if(i != centralgal) {
            // satellite ejected gas goes to central ejected reservoir
            GALAXY_PROP_EjectedMass(&galaxies[i]) = 0.0;
            GALAXY_PROP_MetalsEjectedMass(&galaxies[i]) = 0.0;

            // satellite ICS goes to central ICS
            GALAXY_PROP_ICS(&galaxies[i]) = 0.0;
            GALAXY_PROP_MetalsICS(&galaxies[i]) = 0.0;
        }
    }

    // include reionization if necessary
    if(run_params->physics.ReionizationOn) {
        reionization_modifier = do_reionization(centralgal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

    infallingMass =
        reionization_modifier * run_params->physics.BaryonFrac * GALAXY_PROP_Mvir(&galaxies[centralgal]) - 
        (tot_stellarMass + tot_coldMass + tot_hotMass + tot_ejected + tot_BHMass + tot_ICS);

    // the central galaxy keeps all the ejected mass
    GALAXY_PROP_EjectedMass(&galaxies[centralgal]) = tot_ejected;
    GALAXY_PROP_MetalsEjectedMass(&galaxies[centralgal]) = tot_ejectedMetals;

    if(GALAXY_PROP_MetalsEjectedMass(&galaxies[centralgal]) > GALAXY_PROP_EjectedMass(&galaxies[centralgal])) {
        GALAXY_PROP_MetalsEjectedMass(&galaxies[centralgal]) = GALAXY_PROP_EjectedMass(&galaxies[centralgal]);
    }

    if(GALAXY_PROP_EjectedMass(&galaxies[centralgal]) < 0.0) {
        GALAXY_PROP_EjectedMass(&galaxies[centralgal]) = 0.0;
        GALAXY_PROP_MetalsEjectedMass(&galaxies[centralgal]) = 0.0;
    }

    if(GALAXY_PROP_MetalsEjectedMass(&galaxies[centralgal]) < 0.0) {
        GALAXY_PROP_MetalsEjectedMass(&galaxies[centralgal]) = 0.0;
    }

    // the central galaxy keeps all the ICS (mostly for numerical convenience)
    GALAXY_PROP_ICS(&galaxies[centralgal]) = tot_ICS;
    GALAXY_PROP_MetalsICS(&galaxies[centralgal]) = tot_ICSMetals;

    if(GALAXY_PROP_MetalsICS(&galaxies[centralgal]) > GALAXY_PROP_ICS(&galaxies[centralgal])) {
        GALAXY_PROP_MetalsICS(&galaxies[centralgal]) = GALAXY_PROP_ICS(&galaxies[centralgal]);
    }

    if(GALAXY_PROP_ICS(&galaxies[centralgal]) < 0.0) {
        GALAXY_PROP_ICS(&galaxies[centralgal]) = 0.0;
        GALAXY_PROP_MetalsICS(&galaxies[centralgal]) = 0.0;
    }

    if(GALAXY_PROP_MetalsICS(&galaxies[centralgal]) < 0.0) {
        GALAXY_PROP_MetalsICS(&galaxies[centralgal]) = 0.0;
    }
    
    // Emit infall event if system is initialized
    if (event_system_is_initialized()) {
        // Event data structure
        struct { 
            float infalling_mass;
            float reionization_modifier;
            float baryon_fraction;
        } infall_data = {
            .infalling_mass = (float)infallingMass,
            .reionization_modifier = (float)reionization_modifier,
            .baryon_fraction = (float)run_params->physics.BaryonFrac
        };
        
        // Emit the infall event
        event_emit(
            EVENT_INFALL_COMPUTED,       // Event type
            0,                           // Source module ID (0 = traditional code)
            centralgal,                  // Galaxy index
            -1,                          // Step not available in this context
            &infall_data,                // Event data
            sizeof(infall_data),         // Size of event data
            EVENT_FLAG_NONE              // No special flags
        );
    }

    return infallingMass;
}

// Strip gas from satellites
void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params) {
    double reionization_modifier;

    if(run_params->physics.ReionizationOn) {
        reionization_modifier = do_reionization(gal, Zcurr, galaxies, run_params);
    } else {
        reionization_modifier = 1.0;
    }

    double strippedGas = -1.0 * 
        (reionization_modifier * run_params->physics.BaryonFrac * GALAXY_PROP_Mvir(&galaxies[gal]) - 
         (GALAXY_PROP_StellarMass(&galaxies[gal]) + GALAXY_PROP_ColdGas(&galaxies[gal]) + 
          GALAXY_PROP_HotGas(&galaxies[gal]) + GALAXY_PROP_EjectedMass(&galaxies[gal]) + 
          GALAXY_PROP_BlackHoleMass(&galaxies[gal]) + GALAXY_PROP_ICS(&galaxies[gal]))) / STEPS;

    if(strippedGas > 0.0) {
        const double metallicity = get_metallicity(GALAXY_PROP_HotGas(&galaxies[gal]), GALAXY_PROP_MetalsHotGas(&galaxies[gal]));
        double strippedGasMetals = strippedGas * metallicity;

        if(strippedGas > GALAXY_PROP_HotGas(&galaxies[gal])) strippedGas = GALAXY_PROP_HotGas(&galaxies[gal]);
        if(strippedGasMetals > GALAXY_PROP_MetalsHotGas(&galaxies[gal])) strippedGasMetals = GALAXY_PROP_MetalsHotGas(&galaxies[gal]);

        GALAXY_PROP_HotGas(&galaxies[gal]) -= strippedGas;
        GALAXY_PROP_MetalsHotGas(&galaxies[gal]) -= strippedGasMetals;

        GALAXY_PROP_HotGas(&galaxies[centralgal]) += strippedGas;
        GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]) += strippedGas * metallicity;
    }
}

// Calculate reionization effects
double do_reionization(const int gal, const double Zcurr, struct GALAXY *galaxies, const struct params *run_params) {
    double f_of_a;

    // we employ the reionization recipe described in Gnedin (2000), however use the fitting
    // formulas given by Kravtsov et al (2004) Appendix B

    // here are two parameters that Kravtsov et al keep fixed, alpha gives the best fit to the Gnedin data
    const double alpha = 6.0;
    const double Tvir = 1e4;

    // calculate the filtering mass
    const double a = 1.0 / (1.0 + Zcurr);
    const double a_on_a0 = a / run_params->physics.a0;
    const double a_on_ar = a / run_params->physics.ar;

    if(a <= run_params->physics.a0) {
        f_of_a = 3.0 * a / ((2.0 + alpha) * (5.0 + 2.0 * alpha)) * pow(a_on_a0, alpha);
    } else if((a > run_params->physics.a0) && (a < run_params->physics.ar)) {
        f_of_a =
            (3.0 / a) * run_params->physics.a0 * run_params->physics.a0 * (1.0 / (2.0 + alpha) - 2.0 * pow(a_on_a0, -0.5) / (5.0 + 2.0 * alpha)) +
            a * a / 10.0 - (run_params->physics.a0 * run_params->physics.a0 / 10.0) * (5.0 - 4.0 * pow(a_on_a0, -0.5));
    } else {
        f_of_a =
            (3.0 / a) * (run_params->physics.a0 * run_params->physics.a0 * (1.0 / (2.0 + alpha) - 2.0 * pow(a_on_a0, -0.5) / (5.0 + 2.0 * alpha)) +
                         (run_params->physics.ar * run_params->physics.ar / 10.0) * (5.0 - 4.0 * pow(a_on_ar, -0.5)) - 
                         (run_params->physics.a0 * run_params->physics.a0 / 10.0) * (5.0 - 4.0 / sqrt(a_on_a0)) +
                         a * run_params->physics.ar / 3.0 - (run_params->physics.ar * run_params->physics.ar / 3.0) * (3.0 - 2.0 / sqrt(a_on_ar)));
    }

    // this is in units of 10^10Msun/h, note mu=0.59 and mu^-1.5 = 2.21
    const double Mjeans = 25.0 / sqrt(run_params->cosmology.Omega) * 2.21;
    const double Mfiltering = Mjeans * pow(f_of_a, 1.5);

    // calculate the characteristic mass corresponding to a halo temperature of 10^4K
    const double Vchar = sqrt(Tvir / 36.0);
    const double omegaZ = run_params->cosmology.Omega * (CUBE(1.0 + Zcurr) / (run_params->cosmology.Omega * CUBE(1.0 + Zcurr) + run_params->cosmology.OmegaLambda));
    const double xZ = omegaZ - 1.0;
    const double deltacritZ = 18.0 * M_PI * M_PI + 82.0 * xZ - 39.0 * xZ * xZ;
    const double HubbleZ = run_params->cosmology.Hubble * sqrt(run_params->cosmology.Omega * CUBE(1.0 + Zcurr) + run_params->cosmology.OmegaLambda);

    const double Mchar = Vchar * Vchar * Vchar / (run_params->cosmology.G * HubbleZ * sqrt(0.5 * deltacritZ));

    // we use the maximum of Mfiltering and Mchar
    const double mass_to_use = dmax(Mfiltering, Mchar);
    const double modifier = 1.0 / CUBE(1.0 + 0.26 * (mass_to_use / GALAXY_PROP_Mvir(&galaxies[gal])));

    return modifier;
}

// Add infalling gas to the hot gas component
void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies) {
    float metallicity;

    // if the halo has lost mass, subtract baryons from the ejected mass first, then the hot gas
    if(infallingGas < 0.0 && GALAXY_PROP_EjectedMass(&galaxies[gal]) > 0.0) {
        metallicity = get_metallicity(GALAXY_PROP_EjectedMass(&galaxies[gal]), GALAXY_PROP_MetalsEjectedMass(&galaxies[gal]));
        GALAXY_PROP_MetalsEjectedMass(&galaxies[gal]) += infallingGas * metallicity;
        if(GALAXY_PROP_MetalsEjectedMass(&galaxies[gal]) < 0.0) GALAXY_PROP_MetalsEjectedMass(&galaxies[gal]) = 0.0;

        GALAXY_PROP_EjectedMass(&galaxies[gal]) += infallingGas;
        if(GALAXY_PROP_EjectedMass(&galaxies[gal]) < 0.0) {
            infallingGas = GALAXY_PROP_EjectedMass(&galaxies[gal]);
            GALAXY_PROP_EjectedMass(&galaxies[gal]) = 0.0;
            GALAXY_PROP_MetalsEjectedMass(&galaxies[gal]) = 0.0;
        } else {
            infallingGas = 0.0;
        }
    }

    // if the halo has lost mass, subtract hot metals mass next, then the hot gas
    if(infallingGas < 0.0 && GALAXY_PROP_MetalsHotGas(&galaxies[gal]) > 0.0) {
        metallicity = get_metallicity(GALAXY_PROP_HotGas(&galaxies[gal]), GALAXY_PROP_MetalsHotGas(&galaxies[gal]));
        GALAXY_PROP_MetalsHotGas(&galaxies[gal]) += infallingGas * metallicity;
        if(GALAXY_PROP_MetalsHotGas(&galaxies[gal]) < 0.0) GALAXY_PROP_MetalsHotGas(&galaxies[gal]) = 0.0;
    }

    // add (subtract) the ambient (enriched) infalling gas to the central galaxy hot component
    GALAXY_PROP_HotGas(&galaxies[gal]) += infallingGas;
    if(GALAXY_PROP_HotGas(&galaxies[gal]) < 0.0) {
        GALAXY_PROP_HotGas(&galaxies[gal]) = 0.0;
        GALAXY_PROP_MetalsHotGas(&galaxies[gal]) = 0.0;
    }
}

static int infall_module_initialize(struct params *params, void **module_data) {
    // Unused parameter
    (void)params;
    
    struct infall_module_data *data = malloc(sizeof(struct infall_module_data));
    if (!data) return -1;
    
    // Get the current module ID (or use 0 if not available)
    int module_id = module_get_active_by_type(MODULE_TYPE_INFALL, NULL, NULL);
    if (module_id < 0) module_id = 0;
    
    register_infall_properties(module_id);
    data->prop_ids = infall_ids;
    data->module_id = module_id;
    data->current_infall = 0.0;
    *module_data = data;
    return 0;
}

static int infall_module_execute_halo_phase(void *module_data, struct pipeline_context *context) {
    struct infall_module_data *data = (struct infall_module_data*)module_data;
    struct GALAXY *galaxies = context->galaxies;
    int centralgal = context->centralgal;
    double redshift = context->redshift;
    double infallingGas = 0.0;
    
    infallingGas = infall_recipe(centralgal, context->ngal, redshift, galaxies, context->params);
    data->current_infall = infallingGas;
    pipeline_context_set_data(context, "infallingGas", infallingGas);
    
    // Emit event
    if (event_system_is_initialized()) {
        struct { 
            float infalling_mass; 
            float reionization_modifier; 
        } infall_data = {
            .infalling_mass = (float)infallingGas,
            .reionization_modifier = 1.0f
        };
        event_emit(EVENT_INFALL_COMPUTED, data->module_id, centralgal, context->step, &infall_data, sizeof(infall_data), EVENT_FLAG_NONE);
    }
    return 0;
}

static int infall_module_execute_galaxy_phase(void *module_data, struct pipeline_context *context) {
    struct infall_module_data *data = (struct infall_module_data*)module_data;
    struct GALAXY *galaxies = context->galaxies;
    int p = context->current_galaxy;
    int centralgal = context->centralgal;
    
    if (p == centralgal) {
        // Apply infall to central galaxy
        add_infall_to_hot(p, data->current_infall / STEPS, galaxies);
    } else if (GALAXY_PROP_Type(&galaxies[p]) == 1 && GALAXY_PROP_HotGas(&galaxies[p]) > 0.0) {
        // Strip gas from satellite
        strip_from_satellite(centralgal, p, context->redshift, galaxies, context->params);
    }
    return 0;
}

struct base_module *infall_module_create(void) {
    struct base_module *module = malloc(sizeof(struct base_module));
    if (!module) return NULL;
    memset(module, 0, sizeof(struct base_module));
    snprintf(module->name, MAX_MODULE_NAME, "StandardInfall");
    snprintf(module->version, MAX_MODULE_VERSION, "1.0.0");
    snprintf(module->author, MAX_MODULE_AUTHOR, "SAGE Team");
    module->type = MODULE_TYPE_INFALL;
    module->module_id = -1;
    module->initialize = infall_module_initialize;
    module->cleanup = NULL;
    module->execute_halo_phase = infall_module_execute_halo_phase;
    module->execute_galaxy_phase = infall_module_execute_galaxy_phase;
    module->phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_GALAXY;
    return module;
}
