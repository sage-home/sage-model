#ifndef PHYSICS_MODULES_H
#define PHYSICS_MODULES_H

/**
 * @file physics_modules.h
 * @brief Transitional physics module header for core-only build
 * 
 * IMPORTANT NOTICE:
 * This file is part of the transitional architecture during the Phase 5.2.F 
 * (Core-Physics Separation) refactoring. It provides placeholder physics modules
 * to allow a core-only build without real physics implementations.
 * 
 * LIFECYCLE:
 * This file is scheduled for removal/replacement after the completion of 
 * Phase 5.2.G (Physics Module Migration), when all physics modules will be
 * implemented as pure add-ons to the core infrastructure.
 * 
 * REPLACEMENT MECHANISM:
 * In the final architecture, physics modules will register themselves through
 * the module system, with no need for this centralized include file. Each
 * module will be independently buildable and loadable.
 */

#include "../core/core_module_system.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_logging.h"

/**
 * PLACEHOLDER MODULES:
 * These are transitional stubs that maintain the pipeline structure
 * without implementing actual physics. They are used exclusively during
 * the Core-Physics Separation phase to validate that the core infrastructure
 * can operate independently of physics implementations.
 * 
 * In the final architecture, these placeholders will be replaced by
 * full physics modules that are dynamically registered at runtime.
 */
#include "placeholder_empty_module.h"           // Basic no-operation placeholder
#include "placeholder_cooling_module.h"         // Gas cooling processes 
#include "placeholder_infall_module.h"          // Gas infall processes
#include "placeholder_output_module.h"          // Output preparation 
#include "placeholder_starformation_module.h"   // Star formation processes
#include "placeholder_disk_instability_module.h"// Disk instability processes
#include "placeholder_reincorporation_module.h" // Gas reincorporation processes
#include "placeholder_mergers_module.h"         // Galaxy merger processes

/**
 * LEGACY MODULE INCLUDES (REMOVED):
 * These were the original physics module implementations that have been
 * replaced by the modular architecture. They are kept here (commented out)
 * temporarily for reference during refactoring.
 */
// #include "cooling_module.h"
// #include "infall_module.h"
// #include "output_preparation_module.h"

/**
 * @brief Initialize all physics modules
 * 
 * TRANSITIONAL IMPLEMENTATION:
 * This is a simplified version for the core-only build during refactoring.
 * In the final architecture, this function will be replaced by dynamic
 * module discovery and initialization through the module system.
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
 * TRANSITIONAL IMPLEMENTATION:
 * This is a simplified version for the core-only build during refactoring.
 * In the final architecture, module cleanup will be handled entirely by
 * the module system with no need for this centralized function.
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
 * TRANSITIONAL IMPLEMENTATION:
 * This is a simplified version for the core-only build during refactoring.
 * In the final architecture, modules will register themselves directly
 * with the pipeline system without requiring this centralized function.
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