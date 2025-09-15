#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "physics_module_interface.h"

/**
 * @brief Maximum number of modules in a physics pipeline
 */
#define MAX_PIPELINE_MODULES 16

/**
 * @brief Physics execution pipeline structure
 * 
 * Manages the execution of physics modules in the correct order
 * for a specific configuration. Maintains execution context and
 * provides controlled access to physics calculations.
 */
typedef struct physics_pipeline {
    physics_module_t **active_modules;     /**< Array of active modules in execution order */
    int num_active_modules;                /**< Number of active modules */
    physics_execution_context_t context;   /**< Current execution context */
    bool initialized;                      /**< Pipeline initialization state */
    
    // Pipeline configuration
    bool enable_halo_phase;                /**< Enable halo phase execution */
    bool enable_galaxy_phase;              /**< Enable galaxy phase execution */
    bool enable_post_phase;                /**< Enable post phase execution */
    bool enable_final_phase;               /**< Enable final phase execution */
    
    // Module storage (owned by pipeline)
    physics_module_t *module_storage[MAX_PIPELINE_MODULES];
    
} physics_pipeline_t;

/**
 * @brief Create a new physics pipeline
 * @return Pointer to new pipeline, or NULL on failure
 */
physics_pipeline_t* physics_pipeline_create(void);

/**
 * @brief Destroy a physics pipeline and free resources
 * @param pipeline Pipeline to destroy (can be NULL)
 */
void physics_pipeline_destroy(physics_pipeline_t *pipeline);

/**
 * @brief Add a module to the pipeline
 * @param pipeline Target pipeline
 * @param module Module to add
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_pipeline_add_module(physics_pipeline_t *pipeline, 
                                                   physics_module_t *module);

/**
 * @brief Configure pipeline from module names
 * @param pipeline Target pipeline
 * @param module_names Array of module names to include
 * @param num_modules Number of module names
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_pipeline_configure(physics_pipeline_t *pipeline,
                                                  const char **module_names,
                                                  int num_modules);

/**
 * @brief Initialize pipeline execution context
 * @param pipeline Target pipeline
 * @param halos Halo data array
 * @param haloaux Auxiliary halo data array
 * @param galaxies Galaxy data array
 * @param run_params Simulation parameters
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_pipeline_initialize_context(physics_pipeline_t *pipeline,
                                                           const struct halo_data *halos,
                                                           const struct halo_aux_data *haloaux,
                                                           struct GALAXY *galaxies,
                                                           const struct params *run_params);

/**
 * @brief Execute halo phase for all modules
 * @param pipeline Active pipeline
 * @param halonr Current halo number
 * @param ngal Number of galaxies in halo
 * @param redshift Current redshift
 * @return Calculated infall gas amount, or negative value on error
 */
double physics_pipeline_execute_halo_phase(physics_pipeline_t *pipeline,
                                          int halonr,
                                          int ngal,
                                          double redshift);

/**
 * @brief Execute galaxy phase for a specific galaxy
 * @param pipeline Active pipeline
 * @param galaxy_idx Galaxy index in arrays
 * @param central_galaxy_idx Central galaxy index
 * @param time Current cosmic time
 * @param delta_t Time step
 * @param step Integration step number
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_pipeline_execute_galaxy_phase(physics_pipeline_t *pipeline,
                                                             int galaxy_idx,
                                                             int central_galaxy_idx,
                                                             double time,
                                                             double delta_t,
                                                             int step);

/**
 * @brief Execute post phase for all modules
 * @param pipeline Active pipeline
 * @param halonr Current halo number
 * @param ngal Number of galaxies in halo
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_pipeline_execute_post_phase(physics_pipeline_t *pipeline,
                                                           int halonr,
                                                           int ngal);

/**
 * @brief Execute final phase for all modules
 * @param pipeline Active pipeline
 * @return PHYSICS_MODULE_SUCCESS on success, error code on failure
 */
physics_module_result_t physics_pipeline_execute_final_phase(physics_pipeline_t *pipeline);

/**
 * @brief Check if pipeline has modules for a specific capability
 * @param pipeline Pipeline to check
 * @param capability_check Function to test capability
 * @return true if at least one module provides the capability
 */
bool physics_pipeline_has_capability(const physics_pipeline_t *pipeline,
                                    bool (*capability_check)(const physics_module_t*));

/**
 * @brief Get modules in pipeline that provide a specific capability
 * @param pipeline Pipeline to search
 * @param capability_check Function to test capability
 * @param modules Output array for matching modules
 * @param max_modules Maximum modules to return
 * @return Number of modules found
 */
int physics_pipeline_get_modules_by_capability(const physics_pipeline_t *pipeline,
                                              bool (*capability_check)(const physics_module_t*),
                                              physics_module_t **modules,
                                              int max_modules);

/**
 * @brief Print pipeline configuration and status
 * @param pipeline Pipeline to print
 * @param verbose If true, print detailed module information
 */
void physics_pipeline_print_status(const physics_pipeline_t *pipeline, bool verbose);

/**
 * @brief Validate pipeline configuration
 * @param pipeline Pipeline to validate
 * @return PHYSICS_MODULE_SUCCESS if valid, error code otherwise
 */
physics_module_result_t physics_pipeline_validate(const physics_pipeline_t *pipeline);

#ifdef __cplusplus
}
#endif