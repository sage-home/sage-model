#include "infall_module.h"
#include "../standard_physics_properties.h"
#include "../../core/core_event_system.h"
#include "../../core/core_logging.h"
#include "../../core/core_pipeline_system.h"
#include "../../core/core_module_system.h"
#include "../model_infall.h"
#include <stdlib.h>
#include <string.h>

// Module data structure
struct infall_module_data {
    int module_id;
    struct infall_property_ids prop_ids;
    double current_infall;
};

static int infall_module_initialize(struct params *params, void **module_data) {
    // Unused parameter
    (void)params;
    
    struct infall_module_data *data = malloc(sizeof(struct infall_module_data));
    if (!data) return -1;
    
    // Get the current module ID (or use 0 if not available)
    int module_id = module_get_active_by_type(MODULE_TYPE_INFALL, NULL, NULL);
    if (module_id < 0) module_id = 0;
    
    register_infall_properties(module_id);
    data->prop_ids = *get_infall_property_ids();
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
    // Example: call legacy or new infall calculation here
    infallingGas = infall_recipe(centralgal, context->ngal, redshift, galaxies, context->params);
    data->current_infall = infallingGas;
    pipeline_context_set_data(context, "infallingGas", infallingGas);
    // Emit event if needed
    if (event_system_is_initialized()) {
        struct { float infalling_mass; float reionization_modifier; } infall_data = {
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
    } else if (galaxies[p].Type == 1 && galaxies[p].HotGas > 0.0) {
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
