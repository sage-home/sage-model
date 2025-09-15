#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Forward declarations to avoid circular dependencies
struct halo_data;
struct halo_aux_data;
struct GALAXY;
struct params;

/**
 * @brief Physics execution phases in the galaxy evolution pipeline
 * 
 * These phases correspond to the natural execution order in the
 * galaxy evolution process, allowing modules to declare which
 * phases they participate in.
 */
typedef enum {
    PHYSICS_PHASE_HALO = 0x01,      /**< Halo-level calculations (infall) */
    PHYSICS_PHASE_GALAXY = 0x02,    /**< Galaxy-level calculations (cooling, SF) */
    PHYSICS_PHASE_POST = 0x04,      /**< Post-processing (mergers, disruption) */
    PHYSICS_PHASE_FINAL = 0x08      /**< Final calculations and cleanup */
} physics_phase_t;

/**
 * @brief Execution context passed to physics modules
 * 
 * Contains all state and data needed for physics calculations,
 * ensuring modules have access to required information without
 * direct coupling to core data structures.
 */
typedef struct physics_execution_context {
    // Current execution state
    int current_halo;               /**< Current halo being processed */
    int total_galaxies_in_halo;     /**< Number of galaxies in current halo */
    int current_galaxy;             /**< Current galaxy index (for galaxy phase) */
    int central_galaxy;             /**< Index of central galaxy in halo */
    int step;                       /**< Integration step (0 to STEPS-1) */
    double time;                    /**< Current cosmic time */
    double delta_t;                 /**< Time step for integration */
    double redshift;                /**< Current redshift */
    
    // Data pointers (read-only for modules)
    const struct halo_data *halos;
    const struct halo_aux_data *haloaux;
    struct GALAXY *galaxies;        /**< Mutable galaxy data */
    const struct params *run_params;
    
    // Results storage for inter-module communication
    double halo_infall_gas;         /**< Calculated by halo phase modules */
    double galaxy_cooling_gas;      /**< Calculated by cooling modules */
    double galaxy_stellar_mass;     /**< Calculated by star formation modules */
    
} physics_execution_context_t;

/**
 * @brief Return codes for physics module operations
 */
typedef enum {
    PHYSICS_MODULE_SUCCESS = 0,
    PHYSICS_MODULE_ERROR = -1,
    PHYSICS_MODULE_SKIP = 1,        /**< Module chooses to skip this execution */
    PHYSICS_MODULE_DEPENDENCY_MISSING = -2
} physics_module_result_t;

/**
 * @brief Physics module interface structure
 * 
 * Defines the complete interface that physics modules must implement
 * to integrate with the physics-agnostic core. Modules declare their
 * capabilities and provide execution functions for relevant phases.
 */
typedef struct physics_module {
    // Module identification and metadata
    const char *name;               /**< Unique module name */
    const char *version;            /**< Module version string */
    const char *description;        /**< Brief description of module functionality */
    const char **dependencies;     /**< NULL-terminated list of required modules */
    uint32_t supported_phases;     /**< Bitmask of physics_phase_t values */
    
    // Module lifecycle management
    physics_module_result_t (*initialize)(const struct params *run_params);
    void (*shutdown)(void);
    
    // Physics execution phases
    /**
     * @brief Execute halo-level physics calculations
     * @param ctx Execution context with halo data
     * @return Result code indicating success/failure/skip
     */
    physics_module_result_t (*execute_halo_phase)(physics_execution_context_t *ctx);
    
    /**
     * @brief Execute galaxy-level physics calculations
     * @param ctx Execution context with galaxy data
     * @return Result code indicating success/failure/skip
     */
    physics_module_result_t (*execute_galaxy_phase)(physics_execution_context_t *ctx);
    
    /**
     * @brief Execute post-processing calculations
     * @param ctx Execution context for cleanup/finalization
     * @return Result code indicating success/failure/skip
     */
    physics_module_result_t (*execute_post_phase)(physics_execution_context_t *ctx);
    
    /**
     * @brief Execute final phase calculations
     * @param ctx Execution context for final cleanup
     * @return Result code indicating success/failure/skip
     */
    physics_module_result_t (*execute_final_phase)(physics_execution_context_t *ctx);
    
    // Module capability declarations
    bool (*provides_infall)(void);          /**< Provides gas infall calculations */
    bool (*provides_cooling)(void);         /**< Provides gas cooling calculations */
    bool (*provides_starformation)(void);   /**< Provides star formation calculations */
    bool (*provides_feedback)(void);        /**< Provides stellar feedback calculations */
    bool (*provides_reincorporation)(void); /**< Provides gas reincorporation calculations */
    bool (*provides_mergers)(void);         /**< Provides galaxy merger calculations */
    
    // Internal module state (opaque to core)
    void *module_data;              /**< Module-specific data storage */
    
} physics_module_t;

/**
 * @brief Module registration function signature
 * 
 * Physics modules must provide a function of this signature to register
 * themselves with the module system. This enables runtime module discovery.
 */
typedef physics_module_result_t (*physics_module_register_func_t)(physics_module_t *module);

/**
 * @brief Standard module registration macro
 * 
 * Physics modules should use this macro to auto-register themselves
 * when the library is loaded. This ensures modules are available
 * without explicit initialization code in the core.
 */
#define PHYSICS_MODULE_REGISTER(module_instance) \
    static void __attribute__((constructor)) register_##module_instance(void) { \
        extern physics_module_result_t physics_module_register(physics_module_t *module); \
        physics_module_register(&module_instance); \
    }

// Core interface functions (implemented in physics_module_registry.c)
physics_module_result_t physics_module_register(physics_module_t *module);
physics_module_t* physics_module_find_by_name(const char *name);
int physics_module_get_count(void);
physics_module_result_t physics_module_initialize_all(const struct params *run_params);
void physics_module_shutdown_all(void);

// Utility functions for module validation
bool physics_module_validate(const physics_module_t *module);
physics_module_result_t physics_module_check_dependencies(const physics_module_t *module);
const char* physics_module_result_string(physics_module_result_t result);

#ifdef __cplusplus
}
#endif