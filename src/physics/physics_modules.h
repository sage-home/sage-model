#ifndef PHYSICS_MODULES_H
#define PHYSICS_MODULES_H

#include "../core/core_module_system.h"
#include "../core/core_pipeline_system.h"
#include "../core/core_logging.h"

// Include module headers
#include "cooling_module.h"
#include "infall_module.h"
#include "output_preparation_module.h"
#include "placeholder_empty_module.h"

/**
 * @brief Initialize all physics modules
 * 
 * @return 0 on success, non-zero on failure
 */
static inline int init_physics_modules(void) {
    int ret;
    
    // Initialize output preparation module
    ret = init_output_preparation_module();
    if (ret != 0) {
        LOG_ERROR("Failed to initialize output preparation module");
        return ret;
    }
    
    // For cooling and infall modules, we need to create and initialize them
    // using their create functions, which we'll do in register_physics_modules
    
    return 0;
}

/**
 * @brief Clean up all physics modules
 * 
 * @return 0 on success, non-zero on failure
 */
static inline int cleanup_physics_modules(void) {
    int ret;
    
    // Clean up output preparation module
    ret = cleanup_output_preparation_module();
    if (ret != 0) {
        LOG_ERROR("Failed to clean up output preparation module");
        return ret;
    }
    
    // For cooling and infall modules, cleanup is handled by the module system
    
    return 0;
}

/**
 * @brief Register all physics modules with the pipeline
 * 
 * @param config Pipeline configuration
 * @return 0 on success, non-zero on failure
 */
static inline int register_physics_modules(struct module_pipeline *pipeline) {
    int ret;
    
    // Register output preparation module
    ret = register_output_preparation_module(pipeline);
    if (ret != 0) {
        LOG_ERROR("Failed to register output preparation module");
        return ret;
    }
    
    // For cooling and infall modules, we would register them here
    // but their module-specific registration functions aren't exposed
    
    return 0;
}

#endif // PHYSICS_MODULES_H