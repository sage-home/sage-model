#include "core_pipeline_registry.h"
#include "core_galaxy_accessors.h"
#include "core_logging.h"
#include "core_config_system.h"
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

/* Helper function to get a boolean value from a config object */
static bool get_boolean_from_object(const struct config_object *obj, const char *key, bool default_value) {
    if (obj == NULL || key == NULL) {
        return default_value;
    }

    for (int i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            const struct config_value *value = &obj->entries[i].value;
            
            if (value->type == CONFIG_VALUE_BOOLEAN) {
                return value->u.boolean;
            } else if (value->type == CONFIG_VALUE_INTEGER) {
                return value->u.integer != 0;
            } else if (value->type == CONFIG_VALUE_DOUBLE) {
                return value->u.floating != 0.0;
            } else if (value->type == CONFIG_VALUE_STRING) {
                if (strcmp(value->u.string, "true") == 0 ||
                    strcmp(value->u.string, "yes") == 0 ||
                    strcmp(value->u.string, "1") == 0) {
                    return true;
                } else if (strcmp(value->u.string, "false") == 0 ||
                           strcmp(value->u.string, "no") == 0 ||
                           strcmp(value->u.string, "0") == 0) {
                    return false;
                }
            }
        }
    }

    return default_value;
}

/* Helper function to get a string value from a config object */
static const char *get_string_from_object(const struct config_object *obj, const char *key, const char *default_value) {
    if (obj == NULL || key == NULL) {
        return default_value;
    }

    for (int i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            const struct config_value *value = &obj->entries[i].value;
            
            if (value->type == CONFIG_VALUE_STRING) {
                return value->u.string;
            }
        }
    }

    return default_value;
}

/* Function to add modules to the pipeline from registered factories */
static void add_all_registered_modules_to_pipeline(struct module_pipeline *pipeline) {
    // Track modules we've already processed to avoid duplicates
    int processed_module_ids[MAX_MODULES] = {0};
    int num_processed = 0;

    LOG_INFO("Creating pipeline with all %d registered factories", num_factories);

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
                LOG_WARNING("Exceeded capacity to track processed modules for deduplication (%d).", MAX_MODULES);
            }
            
            // Add the module to the pipeline
            int result = pipeline_add_step(pipeline, module->type, module->name, module->name, true, false);
            
            // pipeline_add_step returns 0 on success.
            if (result == 0) { 
                LOG_DEBUG("Module %s (ID: %d) added to pipeline as step '%s' successfully.", module->name, module->module_id, module->name);
                module_set_active(module->module_id);
            } else {
                LOG_ERROR("Failed to add module %s (ID: %d) to pipeline as step. Error code: %d", module->name, module->module_id, result);
            }
        } else {
            LOG_ERROR("Factory for module %s failed to create module instance.", module_factories[i].name);
        }
    }

    LOG_DEBUG("Finished processing module factories for pipeline creation.");
}

struct module_pipeline *pipeline_create_with_standard_modules(void) {
    LOG_DEBUG("Creating pipeline based on configuration and registered factories...");
    pipeline_register_standard_modules();

    // Create an empty pipeline
    struct module_pipeline *pipeline = pipeline_create("configured_pipeline");
    if (!pipeline) {
        LOG_ERROR("Failed to create pipeline in pipeline_create_with_standard_modules.");
        return NULL;
    }
    LOG_DEBUG("Empty pipeline created successfully. Found %d registered module factories.", num_factories);

    // Check if configuration system is initialized
    if (global_config == NULL || global_config->root == NULL) {
        LOG_WARNING("No global JSON configuration loaded. Using all registered modules.");
        add_all_registered_modules_to_pipeline(pipeline);
        return pipeline;
    }

    // Get the modules.instances array from configuration
    const struct config_value *modules_instances_val = config_get_value("modules.instances");
    if (modules_instances_val == NULL || modules_instances_val->type != CONFIG_VALUE_ARRAY) {
        LOG_WARNING("'modules.instances' array not found or not an array in JSON config. Using all registered modules.");
        add_all_registered_modules_to_pipeline(pipeline);
        return pipeline;
    }

    // Process modules from configuration
    LOG_INFO("Creating pipeline from configuration with %d module instance(s)", modules_instances_val->u.array.count);

    // Track modules we've already processed to avoid duplicates
    int processed_module_ids[MAX_MODULES] = {0};
    int num_processed = 0;

    // Iterate through configured modules
    for (int i = 0; i < modules_instances_val->u.array.count; i++) {
        const struct config_value *module_config_val = modules_instances_val->u.array.items[i];
        if (module_config_val->type != CONFIG_VALUE_OBJECT) {
            LOG_WARNING("Module instance #%d is not an object, skipping.", i);
            continue;
        }

        // Get module name and enabled status from configuration
        const char *config_module_name = get_string_from_object(module_config_val->u.object, "name", NULL);
        bool enabled = get_boolean_from_object(module_config_val->u.object, "enabled", false);
        
        // For backwards compatibility, also check "active" flag
        if (!enabled) {
            enabled = get_boolean_from_object(module_config_val->u.object, "active", false);
        }

        if (!enabled || !config_module_name) {
            if (!config_module_name) {
                LOG_WARNING("Module instance #%d has no name, skipping.", i);
            } else {
                LOG_DEBUG("Module '%s' is disabled in configuration, skipping.", config_module_name);
            }
            continue;
        }

        LOG_DEBUG("Configuration requests module: '%s' (enabled: true)", config_module_name);

        // Find matching factory
        bool factory_found = false;
        for (int j = 0; j < num_factories; j++) {
            if (strcmp(module_factories[j].name, config_module_name) == 0) {
                LOG_DEBUG("Found matching registered factory for '%s'", config_module_name);
                struct base_module *module = module_factories[j].factory();
                if (module) {
                    // Check if we've already processed this module
                    bool already_processed = false;
                    for (int k = 0; k < num_processed; k++) {
                        if (processed_module_ids[k] == module->module_id) {
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
                        LOG_WARNING("Exceeded capacity to track processed modules for deduplication (%d)", MAX_MODULES);
                    }
                    
                    // Add the module to the pipeline
                    int result = pipeline_add_step(pipeline, module->type, module->name, module->name, true, false);
                    
                    if (result == 0) {
                        LOG_INFO("Added module '%s' to pipeline from configuration.", module->name);
                        module_set_active(module->module_id);
                    } else {
                        LOG_ERROR("Failed to add module '%s' to pipeline as step. Error code: %d", module->name, result);
                    }
                } else {
                    LOG_ERROR("Factory for '%s' returned NULL.", config_module_name);
                }
                factory_found = true;
                break;
            }
        }
        if (!factory_found) {
            LOG_WARNING("Module '%s' specified in config, but no matching factory registered.", config_module_name);
        }
    }
    
    LOG_INFO("Finished creating pipeline with %d configured module(s).", pipeline->num_steps);
    return pipeline;
}

// Define the variable as static
static int use_extension_properties = 1; // 0 = direct access, 1 = extensions

void pipeline_set_use_extensions(int enable) {
    use_extension_properties = enable;
}
