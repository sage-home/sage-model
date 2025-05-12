#ifndef PHYSICS_MODULES_H
#define PHYSICS_MODULES_H

#include "../core/core_module_system.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_logging.h"

// Only include placeholder modules
#include "placeholder_empty_module.h"
#include "placeholder_cooling_module.h"
#include "placeholder_infall_module.h"
#include "placeholder_output_module.h"
#include "placeholder_starformation_module.h"
#include "placeholder_disk_instability_module.h"
#include "placeholder_reincorporation_module.h"
#include "placeholder_mergers_module.h"

// Remove includes for non-placeholder modules
// #include "cooling_module.h"
// #include "infall_module.h"
// #include "output_preparation_module.h"

/**
 * @brief Initialize all physics modules
 * 
 * Empty implementation for core-only build with placeholder modules.
 * Real physics modules register themselves automatically via constructor functions.
 * 
 * @return 0 (always success)
 */
static inline int init_physics_modules(void) {
    LOG_INFO("Initializing placeholder physics modules");
    // All placeholder modules are auto-registered via constructor attributes
    return 0;
}

/**
 * @brief Clean up all physics modules
 * 
 * Empty implementation for core-only build with placeholder modules.
 * Module cleanup is handled by the module system.
 * 
 * @return 0 (always success)
 */
static inline int cleanup_physics_modules(void) {
    LOG_INFO("Cleaning up placeholder physics modules");
    // Cleanup is handled by the module system
    return 0;
}

/**
 * @brief Register all physics modules with the pipeline
 * 
 * For placeholder modules, registration happens automatically through 
 * the module system's discovery mechanism. This function exists
 * to maintain API compatibility with the full physics version.
 * 
 * @param pipeline Pipeline configuration
 * @return 0 (always success)
 */
static inline int register_physics_modules(struct module_pipeline *pipeline) {
    LOG_INFO("Using placeholder physics modules only");
    // Placeholder modules automatically register themselves
    // with the pipeline via their constructor functions
    
    (void)pipeline; // Mark as unused
    return 0;
}

#endif // PHYSICS_MODULES_H