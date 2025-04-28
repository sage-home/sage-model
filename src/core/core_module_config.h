#pragma once

#include <stdbool.h>
#include "core_module_system.h"

// Module framework configuration
struct module_framework_config {
    bool use_extensions;          // Use extension properties instead of direct access
    bool enable_events;           // Enable event system
    bool load_dynamic_modules;    // Load modules from external libraries
    char module_dir[256];         // Directory to search for modules
};

// Get the global module framework configuration
struct module_framework_config *get_module_framework_config(void);

// Initialize the module framework configuration
int module_framework_config_initialize(struct params *params);

// Apply the module framework configuration
int module_framework_config_apply(void);
