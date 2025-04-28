#include "cooling_module.h"
#include "../standard_physics_properties.h"
#include "../../core/core_event_system.h"
#include "../../core/core_logging.h"
#include "../../core/core_pipeline_system.h"
#include "../../core/core_module_system.h"
#include "../model_cooling_heating.h"
#include <stdlib.h>
#include <string.h>

// Module data structure
struct cooling_module_data {
    int module_id;
    struct cooling_property_ids prop_ids;
};

static int cooling_module_initialize(struct params *params, void **module_data) {
    // Unused parameter
    (void)params;
    
    struct cooling_module_data *data = malloc(sizeof(struct cooling_module_data));
    if (!data) return -1;
    
    // Get the current module ID (or use 0 if not available)
    int module_id = module_get_active_by_type(MODULE_TYPE_COOLING, NULL, NULL);
    if (module_id < 0) module_id = 0;
    
    register_cooling_properties(module_id);
    data->prop_ids = *get_cooling_property_ids();
    data->module_id = module_id;
    *module_data = data;
    return 0;
}

static int cooling_module_execute_galaxy_phase(void *module_data, struct pipeline_context *context) {
    struct cooling_module_data *data = (struct cooling_module_data*)module_data;
    struct GALAXY *galaxies = context->galaxies;
    int p = context->current_galaxy;
    double dt = context->dt / STEPS;
    // Example: call legacy or new cooling calculation here
    struct cooling_params_view cooling_params;
    initialize_cooling_params_view(&cooling_params, context->params);
    double coolingGas = cooling_recipe(p, dt, galaxies, &cooling_params);
    cool_gas_onto_galaxy(p, coolingGas, galaxies);
    galaxy_set_cooling_rate(&galaxies[p], 0.5 * coolingGas * galaxies[p].Vvir * galaxies[p].Vvir);
    // Emit event if needed
    if (event_system_is_initialized() && coolingGas > 0.0) {
        struct { float cooling_rate; float cooling_radius; float hot_gas_cooled; } cooling_data = {
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
