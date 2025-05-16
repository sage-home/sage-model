#include "core_pipeline_registry.h"
#include "../physics/infall_module.h"
#include "../physics/cooling_module.h"
#include "core_galaxy_accessors.h"
#include <string.h>

struct {
    enum module_type type;
    char name[MAX_MODULE_NAME];
    module_factory_fn factory;
} module_factories[MAX_MODULES];
int num_factories = 0;

int pipeline_register_module_factory(enum module_type type, const char *name, module_factory_fn factory) {
    if (num_factories >= MAX_MODULES) return -1;
    module_factories[num_factories].type = type;
    strncpy(module_factories[num_factories].name, name, MAX_MODULE_NAME - 1);
    module_factories[num_factories].name[MAX_MODULE_NAME - 1] = '\0';
    module_factories[num_factories].factory = factory;
    return num_factories++;
}

void pipeline_register_standard_modules(void) {
    pipeline_register_module_factory(MODULE_TYPE_INFALL, "StandardInfall", infall_module_create);
    pipeline_register_module_factory(MODULE_TYPE_COOLING, "StandardCooling", cooling_module_create);
    // Add other modules as they are implemented
}

struct module_pipeline *pipeline_create_with_standard_modules(void) {
    pipeline_register_standard_modules();
    struct module_pipeline *pipeline = pipeline_create_default();
    if (!pipeline) return NULL;
    for (int i = 0; i < num_factories; i++) {
        struct base_module *module = module_factories[i].factory();
        if (module) {
            module_register(module);
            module_set_active(module->module_id);
        }
    }
    return pipeline;
}

// Define the variable as static
static int use_extension_properties = 1; // 0 = direct access, 1 = extensions

void pipeline_set_use_extensions(int enable) {
    use_extension_properties = enable;
}
