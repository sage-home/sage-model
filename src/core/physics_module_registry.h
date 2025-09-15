#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "physics_module_interface.h"

/**
 * @brief Maximum number of physics modules supported
 * 
 * This limit ensures bounded memory usage and prevents
 * runaway module registration. Can be increased if needed.
 */
#define MAX_PHYSICS_MODULES 32

/**
 * @brief Physics module registry structure
 * 
 * Maintains the global registry of all available physics modules.
 * Provides functionality for module discovery, dependency resolution,
 * and execution ordering.
 */
typedef struct physics_module_registry {
    physics_module_t *modules[MAX_PHYSICS_MODULES];  /**< Array of registered modules */
    int module_count;                                /**< Number of registered modules */
    bool initialized;                                /**< Registry initialization state */
    bool modules_initialized;                        /**< Module initialization state */
} physics_module_registry_t;

/**
 * @brief Get the global module registry instance
 * @return Pointer to the global registry
 */
physics_module_registry_t* physics_module_registry_get(void);

/**
 * @brief Initialize the module registry
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_module_registry_initialize(void);

/**
 * @brief Shutdown the module registry and all registered modules
 */
void physics_module_registry_shutdown(void);

/**
 * @brief Register a physics module with the registry
 * @param module Pointer to the module to register
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_module_registry_register(physics_module_t *module);

/**
 * @brief Find a module by name
 * @param name Name of the module to find
 * @return Pointer to the module, or NULL if not found
 */
physics_module_t* physics_module_registry_find_by_name(const char *name);

/**
 * @brief Get all registered modules
 * @param modules Array to store module pointers
 * @param max_modules Maximum number of modules to return
 * @return Number of modules returned
 */
int physics_module_registry_get_all(physics_module_t **modules, int max_modules);

/**
 * @brief Get modules that provide a specific capability
 * @param capability_check Function to check if module provides capability
 * @param modules Array to store matching module pointers
 * @param max_modules Maximum number of modules to return
 * @return Number of matching modules found
 */
int physics_module_registry_get_by_capability(bool (*capability_check)(const physics_module_t*),
                                             physics_module_t **modules, 
                                             int max_modules);

/**
 * @brief Resolve module dependencies and return execution order
 * @param requested_modules Array of module names to include
 * @param num_requested Number of requested modules
 * @param ordered_modules Output array for ordered modules
 * @param max_modules Maximum number of modules in output array
 * @return Number of modules in execution order, or negative error code
 */
int physics_module_registry_resolve_dependencies(const char **requested_modules,
                                                int num_requested,
                                                physics_module_t **ordered_modules,
                                                int max_modules);

/**
 * @brief Validate all registered modules
 * @return PHYSICS_MODULE_SUCCESS if all modules valid, error code otherwise
 */
physics_module_result_t physics_module_registry_validate_all(void);

/**
 * @brief Initialize all registered modules
 * @param run_params Parameters to pass to module initialization
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_module_registry_initialize_modules(const struct params *run_params);

/**
 * @brief Print registry status and module information
 * @param verbose If true, print detailed module information
 */
void physics_module_registry_print_status(bool verbose);

#ifdef __cplusplus
}
#endif