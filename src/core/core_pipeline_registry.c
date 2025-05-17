#include "core_pipeline_registry.h"
#include "core_galaxy_accessors.h"
#include "core_logging.h"
#include <string.h>

struct module_factory_entry {
    enum module_type type;
    char name[MAX_MODULE_NAME];
    module_factory_fn factory;
} module_factories[MAX_MODULE_FACTORIES];
int num_factories = 0;

int pipeline_register_module_factory(enum module_type type, const char *name, module_factory_fn factory) {
    if (num_factories >= MAX_MODULE_FACTORIES) return -1;
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

    // Create an empty pipeline specifically for standard modules
    struct module_pipeline *pipeline = pipeline_create("standard_modules");
    if (!pipeline) {
        LOG_ERROR("Failed to create pipeline in pipeline_create_with_standard_modules.");
        return NULL;
    }
    LOG_DEBUG("Empty pipeline created successfully. Found %d registered module factories.", num_factories);

    // Track modules we've already processed to avoid duplicates
    // (in case multiple factories return the same module)
    int processed_module_ids[MAX_MODULES] = {0};
    int num_processed = 0;

    // For each registered module factory, create and add the module
    for (int i = 0; i < num_factories; i++) {
        LOG_DEBUG("Processing factory for module: %s (type %d)", module_factories[i].name, module_factories[i].type);
        struct base_module *module = module_factories[i].factory();
        if (module) {
            LOG_DEBUG("Module %s (type %d, ID %d) created successfully by factory.", module->name, module->type, module->module_id);
            
            // Check if we've already processed this module
            bool already_processed = false;
            for (int j = 0; j < num_processed; j++) {
                if (processed_module_ids[j] == module->module_id) {
                    LOG_DEBUG("Module ID %d already processed, skipping duplicate", module->module_id);
                    already_processed = true;
                    break;
                }
            }
            if (already_processed) {
                continue;
            }
            
            // Track this module as processed
            if (num_processed < MAX_MODULES) {
                processed_module_ids[num_processed++] = module->module_id;
            } else {
                LOG_WARNING("Exceeded capacity to track processed modules for deduplication (%d). Some modules might be added multiple times to the pipeline if factories produce identical module instances beyond this limit.", MAX_MODULES);
            }
            
            // Check if we need to register the module with the module system
            struct base_module *existing_module = NULL;
            void *existing_data = NULL;
            if (module_get(module->module_id, &existing_module, &existing_data) != MODULE_STATUS_SUCCESS) {
                // Module not registered yet, register it
                LOG_DEBUG("Registering module %s with module system", module->name);
                if (module_register(module) != MODULE_STATUS_SUCCESS) {
                    LOG_ERROR("Failed to register module %s with module system", module->name);
                    continue; // Skip this module
                }
            }
            
            // Add the module to the pipeline
            int result = pipeline_add_step(pipeline, module->type, module->name, module->name, true, true);
            
            // pipeline_add_step returns 0 on success.
            if (result == 0) { 
                LOG_DEBUG("Module %s (ID: %d) added to pipeline as step '%s' successfully.", module->name, module->module_id, module->name);
            } else {
                LOG_ERROR("Failed to add module %s (ID: %d) to pipeline as step. Error code: %d", module->name, module->module_id, result);
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
