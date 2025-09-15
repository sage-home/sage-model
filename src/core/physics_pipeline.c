#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "physics_pipeline.h"
#include "physics_module_registry.h"
#include "core_allvars.h"

physics_pipeline_t* physics_pipeline_create(void)
{
    physics_pipeline_t *pipeline = calloc(1, sizeof(physics_pipeline_t));
    if (!pipeline) {
        fprintf(stderr, "Error: Failed to allocate memory for physics pipeline\n");
        return NULL;
    }
    
    // Initialize pipeline state
    pipeline->active_modules = pipeline->module_storage;
    pipeline->num_active_modules = 0;
    pipeline->initialized = false;
    
    // Enable all phases by default
    pipeline->enable_halo_phase = true;
    pipeline->enable_galaxy_phase = true;
    pipeline->enable_post_phase = true;
    pipeline->enable_final_phase = true;
    
    // Initialize context to safe defaults
    memset(&pipeline->context, 0, sizeof(physics_execution_context_t));
    pipeline->context.current_halo = -1;
    pipeline->context.current_galaxy = -1;
    pipeline->context.central_galaxy = -1;
    
    return pipeline;
}

void physics_pipeline_destroy(physics_pipeline_t *pipeline)
{
    if (!pipeline) {
        return;
    }
    
    // Pipeline doesn't own the modules, just clear references
    pipeline->active_modules = NULL;
    pipeline->num_active_modules = 0;
    pipeline->initialized = false;
    
    free(pipeline);
}

physics_module_result_t physics_pipeline_add_module(physics_pipeline_t *pipeline, 
                                                   physics_module_t *module)
{
    if (!pipeline || !module) {
        return PHYSICS_MODULE_ERROR;
    }
    
    if (pipeline->num_active_modules >= MAX_PIPELINE_MODULES) {
        fprintf(stderr, "Error: Pipeline module limit (%d) exceeded\n", MAX_PIPELINE_MODULES);
        return PHYSICS_MODULE_ERROR;
    }
    
    // Check for duplicate modules
    for (int i = 0; i < pipeline->num_active_modules; i++) {
        if (pipeline->active_modules[i] == module) {
            fprintf(stderr, "Warning: Module '%s' already in pipeline\n", module->name);
            return PHYSICS_MODULE_SUCCESS;
        }
    }
    
    // Validate module
    if (!physics_module_validate(module)) {
        fprintf(stderr, "Error: Module '%s' failed validation\n", module->name);
        return PHYSICS_MODULE_ERROR;
    }
    
    // Add module to pipeline
    pipeline->active_modules[pipeline->num_active_modules] = module;
    pipeline->num_active_modules++;
    
    return PHYSICS_MODULE_SUCCESS;
}

physics_module_result_t physics_pipeline_configure(physics_pipeline_t *pipeline,
                                                  const char **module_names,
                                                  int num_modules)
{
    if (!pipeline || !module_names || num_modules <= 0) {
        return PHYSICS_MODULE_ERROR;
    }
    
    // Clear existing modules
    pipeline->num_active_modules = 0;
    
    // Resolve dependencies and get ordered modules
    physics_module_t *ordered_modules[MAX_PIPELINE_MODULES];
    int ordered_count = physics_module_registry_resolve_dependencies(
        module_names, num_modules, ordered_modules, MAX_PIPELINE_MODULES);
    
    if (ordered_count < 0) {
        fprintf(stderr, "Error: Failed to resolve module dependencies\n");
        return PHYSICS_MODULE_ERROR;
    }
    
    if (ordered_count > MAX_PIPELINE_MODULES) {
        fprintf(stderr, "Error: Too many modules after dependency resolution (%d > %d)\n", 
                ordered_count, MAX_PIPELINE_MODULES);
        return PHYSICS_MODULE_ERROR;
    }
    
    // Add modules to pipeline in dependency order
    for (int i = 0; i < ordered_count; i++) {
        physics_module_result_t result = physics_pipeline_add_module(pipeline, ordered_modules[i]);
        if (result != PHYSICS_MODULE_SUCCESS) {
            fprintf(stderr, "Error: Failed to add module '%s' to pipeline\n", 
                    ordered_modules[i]->name);
            return result;
        }
    }
    
    return PHYSICS_MODULE_SUCCESS;
}

physics_module_result_t physics_pipeline_initialize_context(physics_pipeline_t *pipeline,
                                                           const struct halo_data *halos,
                                                           const struct halo_aux_data *haloaux,
                                                           struct GALAXY *galaxies,
                                                           const struct params *run_params)
{
    if (!pipeline || !halos || !haloaux || !galaxies || !run_params) {
        return PHYSICS_MODULE_ERROR;
    }
    
    // Initialize execution context
    pipeline->context.halos = halos;
    pipeline->context.haloaux = haloaux;
    pipeline->context.galaxies = galaxies;
    pipeline->context.run_params = run_params;
    
    // Reset execution state
    pipeline->context.current_halo = -1;
    pipeline->context.current_galaxy = -1;
    pipeline->context.central_galaxy = -1;
    pipeline->context.total_galaxies_in_halo = 0;
    pipeline->context.step = 0;
    pipeline->context.time = 0.0;
    pipeline->context.delta_t = 0.0;
    pipeline->context.redshift = 0.0;
    
    // Reset inter-module communication
    pipeline->context.halo_infall_gas = 0.0;
    pipeline->context.galaxy_cooling_gas = 0.0;
    
    pipeline->initialized = true;
    
    return PHYSICS_MODULE_SUCCESS;
}

double physics_pipeline_execute_halo_phase(physics_pipeline_t *pipeline,
                                          int halonr,
                                          int ngal,
                                          double redshift)
{
    if (!pipeline || !pipeline->initialized || !pipeline->enable_halo_phase) {
        return 0.0;
    }
    
    // Update context for halo phase
    pipeline->context.current_halo = halonr;
    pipeline->context.total_galaxies_in_halo = ngal;
    pipeline->context.redshift = redshift;
    pipeline->context.halo_infall_gas = 0.0;
    
    // Execute halo phase for all modules that support it
    for (int i = 0; i < pipeline->num_active_modules; i++) {
        physics_module_t *module = pipeline->active_modules[i];
        
        if ((module->supported_phases & PHYSICS_PHASE_HALO) && module->execute_halo_phase) {
            physics_module_result_t result = module->execute_halo_phase(&pipeline->context);
            
            if (result == PHYSICS_MODULE_ERROR) {
                fprintf(stderr, "Error: Module '%s' failed in halo phase\n", module->name);
                return -1.0;
            }
            // Continue execution even if module skips (PHYSICS_MODULE_SKIP)
        }
    }
    
    return pipeline->context.halo_infall_gas;
}

physics_module_result_t physics_pipeline_execute_galaxy_phase(physics_pipeline_t *pipeline,
                                                             int galaxy_idx,
                                                             int central_galaxy_idx,
                                                             double time,
                                                             double delta_t,
                                                             int step)
{
    if (!pipeline || !pipeline->initialized || !pipeline->enable_galaxy_phase) {
        return PHYSICS_MODULE_ERROR;
    }
    
    // Update context for galaxy phase
    pipeline->context.current_galaxy = galaxy_idx;
    pipeline->context.central_galaxy = central_galaxy_idx;
    pipeline->context.time = time;
    pipeline->context.delta_t = delta_t;
    pipeline->context.step = step;
    pipeline->context.galaxy_cooling_gas = 0.0;
    
    // Execute galaxy phase for all modules that support it
    for (int i = 0; i < pipeline->num_active_modules; i++) {
        physics_module_t *module = pipeline->active_modules[i];
        
        if ((module->supported_phases & PHYSICS_PHASE_GALAXY) && module->execute_galaxy_phase) {
            physics_module_result_t result = module->execute_galaxy_phase(&pipeline->context);
            
            if (result == PHYSICS_MODULE_ERROR) {
                fprintf(stderr, "Error: Module '%s' failed in galaxy phase\n", module->name);
                return PHYSICS_MODULE_ERROR;
            }
            // Continue execution even if module skips (PHYSICS_MODULE_SKIP)
        }
    }
    
    return PHYSICS_MODULE_SUCCESS;
}

physics_module_result_t physics_pipeline_execute_post_phase(physics_pipeline_t *pipeline,
                                                           int halonr,
                                                           int ngal)
{
    if (!pipeline || !pipeline->initialized || !pipeline->enable_post_phase) {
        return PHYSICS_MODULE_SUCCESS;
    }
    
    // Update context for post phase
    pipeline->context.current_halo = halonr;
    pipeline->context.total_galaxies_in_halo = ngal;
    
    // Execute post phase for all modules that support it
    for (int i = 0; i < pipeline->num_active_modules; i++) {
        physics_module_t *module = pipeline->active_modules[i];
        
        if ((module->supported_phases & PHYSICS_PHASE_POST) && module->execute_post_phase) {
            physics_module_result_t result = module->execute_post_phase(&pipeline->context);
            
            if (result == PHYSICS_MODULE_ERROR) {
                fprintf(stderr, "Error: Module '%s' failed in post phase\n", module->name);
                return PHYSICS_MODULE_ERROR;
            }
            // Continue execution even if module skips (PHYSICS_MODULE_SKIP)
        }
    }
    
    return PHYSICS_MODULE_SUCCESS;
}

physics_module_result_t physics_pipeline_execute_final_phase(physics_pipeline_t *pipeline)
{
    if (!pipeline || !pipeline->initialized || !pipeline->enable_final_phase) {
        return PHYSICS_MODULE_SUCCESS;
    }
    
    // Execute final phase for all modules that support it
    for (int i = 0; i < pipeline->num_active_modules; i++) {
        physics_module_t *module = pipeline->active_modules[i];
        
        if ((module->supported_phases & PHYSICS_PHASE_FINAL) && module->execute_final_phase) {
            physics_module_result_t result = module->execute_final_phase(&pipeline->context);
            
            if (result == PHYSICS_MODULE_ERROR) {
                fprintf(stderr, "Error: Module '%s' failed in final phase\n", module->name);
                return PHYSICS_MODULE_ERROR;
            }
            // Continue execution even if module skips (PHYSICS_MODULE_SKIP)
        }
    }
    
    return PHYSICS_MODULE_SUCCESS;
}

bool physics_pipeline_has_capability(const physics_pipeline_t *pipeline,
                                    bool (*capability_check)(const physics_module_t*))
{
    if (!pipeline || !capability_check) {
        return false;
    }
    
    for (int i = 0; i < pipeline->num_active_modules; i++) {
        if (capability_check(pipeline->active_modules[i])) {
            return true;
        }
    }
    
    return false;
}

int physics_pipeline_get_modules_by_capability(const physics_pipeline_t *pipeline,
                                              bool (*capability_check)(const physics_module_t*),
                                              physics_module_t **modules,
                                              int max_modules)
{
    if (!pipeline || !capability_check || !modules || max_modules <= 0) {
        return 0;
    }
    
    int found = 0;
    for (int i = 0; i < pipeline->num_active_modules && found < max_modules; i++) {
        if (capability_check(pipeline->active_modules[i])) {
            modules[found] = pipeline->active_modules[i];
            found++;
        }
    }
    
    return found;
}

void physics_pipeline_print_status(const physics_pipeline_t *pipeline, bool verbose)
{
    if (!pipeline) {
        printf("Physics Pipeline: NULL\n");
        return;
    }
    
    printf("Physics Pipeline Status:\n");
    printf("  Initialized: %s\n", pipeline->initialized ? "Yes" : "No");
    printf("  Active Modules: %d/%d\n", pipeline->num_active_modules, MAX_PIPELINE_MODULES);
    printf("  Enabled Phases: %s%s%s%s\n",
           pipeline->enable_halo_phase ? "HALO " : "",
           pipeline->enable_galaxy_phase ? "GALAXY " : "",
           pipeline->enable_post_phase ? "POST " : "",
           pipeline->enable_final_phase ? "FINAL" : "");
    
    if (verbose && pipeline->num_active_modules > 0) {
        printf("\nActive Modules (execution order):\n");
        for (int i = 0; i < pipeline->num_active_modules; i++) {
            physics_module_t *module = pipeline->active_modules[i];
            printf("  %d. %s (v%s)\n", i + 1, module->name, module->version);
            printf("     Phases: 0x%08X\n", module->supported_phases);
        }
    }
    
    if (pipeline->initialized) {
        printf("\nCurrent Context:\n");
        printf("  Halo: %d, Galaxy: %d, Central: %d\n",
               pipeline->context.current_halo,
               pipeline->context.current_galaxy,
               pipeline->context.central_galaxy);
        printf("  Step: %d, Time: %.3f, DeltaT: %.6f\n",
               pipeline->context.step,
               pipeline->context.time,
               pipeline->context.delta_t);
    }
}

physics_module_result_t physics_pipeline_validate(const physics_pipeline_t *pipeline)
{
    if (!pipeline) {
        fprintf(stderr, "Error: Cannot validate NULL pipeline\n");
        return PHYSICS_MODULE_ERROR;
    }
    
    // Check basic pipeline state
    if (pipeline->num_active_modules < 0 || 
        pipeline->num_active_modules > MAX_PIPELINE_MODULES) {
        fprintf(stderr, "Error: Invalid number of active modules: %d\n", 
                pipeline->num_active_modules);
        return PHYSICS_MODULE_ERROR;
    }
    
    // Validate all active modules
    for (int i = 0; i < pipeline->num_active_modules; i++) {
        physics_module_t *module = pipeline->active_modules[i];
        if (!module) {
            fprintf(stderr, "Error: NULL module at index %d\n", i);
            return PHYSICS_MODULE_ERROR;
        }
        
        if (!physics_module_validate(module)) {
            fprintf(stderr, "Error: Module '%s' failed validation\n", module->name);
            return PHYSICS_MODULE_ERROR;
        }
        
        physics_module_result_t dep_result = physics_module_check_dependencies(module);
        if (dep_result != PHYSICS_MODULE_SUCCESS) {
            fprintf(stderr, "Error: Module '%s' has dependency issues\n", module->name);
            return dep_result;
        }
    }
    
    return PHYSICS_MODULE_SUCCESS;
}