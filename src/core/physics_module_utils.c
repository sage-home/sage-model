#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "physics_module_interface.h"
#include "physics_module_registry.h"

bool physics_module_validate(const physics_module_t *module)
{
    if (!module) {
        fprintf(stderr, "Error: Module validation failed - NULL module\n");
        return false;
    }
    
    // Check required fields
    if (!module->name || strlen(module->name) == 0) {
        fprintf(stderr, "Error: Module validation failed - missing or empty name\n");
        return false;
    }
    
    if (!module->version || strlen(module->version) == 0) {
        fprintf(stderr, "Error: Module validation failed - missing or empty version\n");
        return false;
    }
    
    // Check that module supports at least one phase
    if (module->supported_phases == 0) {
        fprintf(stderr, "Error: Module '%s' validation failed - no supported phases\n", 
                module->name);
        return false;
    }
    
    // Validate phase functions are present for supported phases
    if (module->supported_phases & PHYSICS_PHASE_HALO) {
        if (!module->execute_halo_phase) {
            fprintf(stderr, "Error: Module '%s' supports HALO phase but missing execute_halo_phase function\n", 
                    module->name);
            return false;
        }
    }
    
    if (module->supported_phases & PHYSICS_PHASE_GALAXY) {
        if (!module->execute_galaxy_phase) {
            fprintf(stderr, "Error: Module '%s' supports GALAXY phase but missing execute_galaxy_phase function\n", 
                    module->name);
            return false;
        }
    }
    
    if (module->supported_phases & PHYSICS_PHASE_POST) {
        if (!module->execute_post_phase) {
            fprintf(stderr, "Error: Module '%s' supports POST phase but missing execute_post_phase function\n", 
                    module->name);
            return false;
        }
    }
    
    if (module->supported_phases & PHYSICS_PHASE_FINAL) {
        if (!module->execute_final_phase) {
            fprintf(stderr, "Error: Module '%s' supports FINAL phase but missing execute_final_phase function\n", 
                    module->name);
            return false;
        }
    }
    
    // Validate that invalid phase bits are not set
    uint32_t valid_phases = PHYSICS_PHASE_HALO | PHYSICS_PHASE_GALAXY | 
                           PHYSICS_PHASE_POST | PHYSICS_PHASE_FINAL;
    if (module->supported_phases & ~valid_phases) {
        fprintf(stderr, "Error: Module '%s' has invalid phase bits set: 0x%08X\n", 
                module->name, module->supported_phases & ~valid_phases);
        return false;
    }
    
    return true;
}

physics_module_result_t physics_module_check_dependencies(const physics_module_t *module)
{
    if (!module) {
        return PHYSICS_MODULE_ERROR;
    }
    
    // If module has no dependencies, it's valid
    if (!module->dependencies) {
        return PHYSICS_MODULE_SUCCESS;
    }
    
    // Check each dependency exists in registry
    for (const char **dep = module->dependencies; *dep; dep++) {
        if (!*dep || strlen(*dep) == 0) {
            fprintf(stderr, "Error: Module '%s' has empty dependency name\n", module->name);
            return PHYSICS_MODULE_ERROR;
        }
        
        physics_module_t *dep_module = physics_module_find_by_name(*dep);
        if (!dep_module) {
            fprintf(stderr, "Error: Module '%s' depends on '%s' which is not registered\n", 
                    module->name, *dep);
            return PHYSICS_MODULE_DEPENDENCY_MISSING;
        }
    }
    
    return PHYSICS_MODULE_SUCCESS;
}

const char* physics_module_result_string(physics_module_result_t result)
{
    switch (result) {
        case PHYSICS_MODULE_SUCCESS:
            return "Success";
        case PHYSICS_MODULE_ERROR:
            return "Error";
        case PHYSICS_MODULE_SKIP:
            return "Skip";
        case PHYSICS_MODULE_DEPENDENCY_MISSING:
            return "Dependency Missing";
        default:
            return "Unknown";
    }
}