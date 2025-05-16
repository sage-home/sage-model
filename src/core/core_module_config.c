#include "core_module_config.h"
#include "core_galaxy_accessors.h"
#include "core_event_system.h"
#include <string.h>

// Global configuration
static struct module_framework_config global_config = {
    .use_extensions = false,       // Default to direct access
    .enable_events = true,         // Enable events by default
    .load_dynamic_modules = false, // Default to static modules
    .module_dir = "modules"       // Default module directory
};

struct module_framework_config *get_module_framework_config(void) {
    return &global_config;
}

int module_framework_config_initialize(struct params *params) {
    // Unused parameter
    (void)params;
    
    // In Phase 5, these parameters are not yet in the runtime params
    // This is a simplified implementation that will be enhanced in later phases
    
    // Default to not using extensions
    global_config.use_extensions = false;
    
    // Use default module directory
    strcpy(global_config.module_dir, "modules");
    
    // Disable dynamic module loading by default
    global_config.load_dynamic_modules = false;
    
    return 0;
}

// Forward declaration of the function to avoid direct variable access
void pipeline_set_use_extensions(int enable);

int module_framework_config_apply(void) {
    // Call the pipeline function rather than accessing the variable directly
    pipeline_set_use_extensions(global_config.use_extensions);
    
    if (global_config.enable_events && !event_system_is_initialized()) {
        event_system_initialize();
    }
    return 0;
}
