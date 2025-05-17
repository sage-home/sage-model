#include "core_pipeline_registry.h"
#include "core_galaxy_accessors.h"
#include "core_logging.h"
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

// Standard modules are registered automatically via constructor annotations
// This function is kept for API compatibility
void pipeline_register_standard_modules(void) {
    // The placeholder modules register themselves at startup via their constructor functions
    LOG_INFO("Using placeholder modules only - registered via constructors");
    LOG_DEBUG("pipeline_register_standard_modules called. Module self-registration is active.");
}

struct module_pipeline *pipeline_create_with_standard_modules(void) {
    LOG_DEBUG("Attempting to create pipeline with standard modules...");
    pipeline_register_standard_modules();

    struct module_pipeline *pipeline = pipeline_create_default();
    if (!pipeline) {
        LOG_ERROR("Failed to create default pipeline in pipeline_create_with_standard_modules.");
        return NULL;
    }
    LOG_DEBUG("Default pipeline created successfully. Found %d registered module factories.", num_factories);

    for (int i = 0; i < num_factories; i++) {
        LOG_DEBUG("Processing factory for module: %s (type %d)", module_factories[i].name, module_factories[i].type);
        struct base_module *module = module_factories[i].factory();
        if (module) {
            LOG_DEBUG("Module %s (type %d, ID %d) created successfully by factory. Attempting to add to pipeline as a step.", module->name, module->type, module->module_id);
            // Ensure module_name and step_name are valid, using module->name for both.
            // Set enabled = true, optional = true (can be adjusted based on requirements).
            int result = pipeline_add_step(pipeline, module->type, module->name, module->name, true, true);
            
            // pipeline_add_step returns 0 on success.
            if (result == 0) { 
                LOG_DEBUG("Module %s (ID: %d) added to pipeline as step '%s' successfully.", module->name, module->module_id, module->name);
                // Activation is typically handled by the module system or pipeline execution,
                // but retaining module_set_active if it's part of the current expected global state management.
                module_set_active(module->module_id);
                LOG_DEBUG("Module %s (ID: %d) set to active.", module->name, module->module_id);
            } else {
                LOG_ERROR("Failed to add module %s (ID: %d) to pipeline as step. Error code: %d", module->name, module->module_id, result);
                // Consider freeing 'module' if not added and not managed elsewhere, though factories might handle this.
            }
        } else {
            LOG_ERROR("Factory for module %s failed to create module instance.", module_factories[i].name);
        }
    }
    LOG_DEBUG("Finished processing all module factories for pipeline creation.");
    return pipeline;
}

// Define the variable as static
static int use_extension_properties = 1; // 0 = direct access, 1 = extensions

void pipeline_set_use_extensions(int enable) {
    use_extension_properties = enable;
}
