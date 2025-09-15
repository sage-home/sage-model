#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "physics_module_registry.h"
#include "core_allvars.h"

// Global registry instance
static physics_module_registry_t g_registry = {
    .modules = {NULL},
    .module_count = 0,
    .initialized = false,
    .modules_initialized = false
};

physics_module_registry_t* physics_module_registry_get(void)
{
    return &g_registry;
}

physics_module_result_t physics_module_registry_initialize(void)
{
    if (g_registry.initialized) {
        return PHYSICS_MODULE_SUCCESS;
    }
    
    // Initialize registry state
    memset(g_registry.modules, 0, sizeof(g_registry.modules));
    g_registry.module_count = 0;
    g_registry.modules_initialized = false;
    g_registry.initialized = true;
    
    return PHYSICS_MODULE_SUCCESS;
}

void physics_module_registry_shutdown(void)
{
    if (!g_registry.initialized) {
        return;
    }
    
    // Shutdown all initialized modules
    if (g_registry.modules_initialized) {
        for (int i = 0; i < g_registry.module_count; i++) {
            physics_module_t *module = g_registry.modules[i];
            if (module && module->shutdown) {
                module->shutdown();
            }
        }
        g_registry.modules_initialized = false;
    }
    
    // Clear registry
    memset(g_registry.modules, 0, sizeof(g_registry.modules));
    g_registry.module_count = 0;
    g_registry.initialized = false;
}

physics_module_result_t physics_module_registry_register(physics_module_t *module)
{
    if (!module) {
        fprintf(stderr, "Error: Cannot register NULL module\n");
        return PHYSICS_MODULE_ERROR;
    }
    
    // Initialize registry if needed
    if (!g_registry.initialized) {
        physics_module_result_t result = physics_module_registry_initialize();
        if (result != PHYSICS_MODULE_SUCCESS) {
            return result;
        }
    }
    
    // Check for duplicate names
    if (physics_module_registry_find_by_name(module->name)) {
        fprintf(stderr, "Error: Module '%s' already registered\n", module->name);
        return PHYSICS_MODULE_ERROR;
    }
    
    // Check registry capacity
    if (g_registry.module_count >= MAX_PHYSICS_MODULES) {
        fprintf(stderr, "Error: Maximum number of physics modules (%d) exceeded\n", 
                MAX_PHYSICS_MODULES);
        return PHYSICS_MODULE_ERROR;
    }
    
    // Validate module
    if (!physics_module_validate(module)) {
        fprintf(stderr, "Error: Module '%s' failed validation\n", module->name);
        return PHYSICS_MODULE_ERROR;
    }
    
    // Register module
    g_registry.modules[g_registry.module_count] = module;
    g_registry.module_count++;
    
    return PHYSICS_MODULE_SUCCESS;
}

physics_module_t* physics_module_registry_find_by_name(const char *name)
{
    if (!name || !g_registry.initialized) {
        return NULL;
    }
    
    for (int i = 0; i < g_registry.module_count; i++) {
        physics_module_t *module = g_registry.modules[i];
        if (module && strcmp(module->name, name) == 0) {
            return module;
        }
    }
    
    return NULL;
}

int physics_module_registry_get_all(physics_module_t **modules, int max_modules)
{
    if (!modules || max_modules <= 0 || !g_registry.initialized) {
        return 0;
    }
    
    int count = g_registry.module_count;
    if (count > max_modules) {
        count = max_modules;
    }
    
    for (int i = 0; i < count; i++) {
        modules[i] = g_registry.modules[i];
    }
    
    return count;
}

int physics_module_registry_get_by_capability(bool (*capability_check)(const physics_module_t*),
                                             physics_module_t **modules, 
                                             int max_modules)
{
    if (!capability_check || !modules || max_modules <= 0 || !g_registry.initialized) {
        return 0;
    }
    
    int found = 0;
    for (int i = 0; i < g_registry.module_count && found < max_modules; i++) {
        physics_module_t *module = g_registry.modules[i];
        if (module && capability_check(module)) {
            modules[found] = module;
            found++;
        }
    }
    
    return found;
}

// Topological sort for dependency resolution
static int topological_sort(physics_module_t **modules, int count)
{
    // Simple implementation using Kahn's algorithm
    int *in_degree = calloc(count, sizeof(int));
    int *queue = malloc(count * sizeof(int));
    int *result_order = malloc(count * sizeof(int));
    
    if (!in_degree || !queue || !result_order) {
        free(in_degree);
        free(queue);
        free(result_order);
        return -1;
    }
    
    // Calculate in-degrees
    for (int i = 0; i < count; i++) {
        physics_module_t *module = modules[i];
        if (module->dependencies) {
            for (const char **dep = module->dependencies; *dep; dep++) {
                // Find dependency in modules array
                for (int j = 0; j < count; j++) {
                    if (strcmp(modules[j]->name, *dep) == 0) {
                        in_degree[i]++;
                        break;
                    }
                }
            }
        }
    }
    
    // Initialize queue with modules that have no dependencies
    int queue_front = 0, queue_back = 0;
    for (int i = 0; i < count; i++) {
        if (in_degree[i] == 0) {
            queue[queue_back++] = i;
        }
    }
    
    // Process queue
    int result_count = 0;
    while (queue_front < queue_back) {
        int current = queue[queue_front++];
        result_order[result_count++] = current;
        
        // Update in-degrees for modules that depend on current
        physics_module_t *current_module = modules[current];
        for (int i = 0; i < count; i++) {
            physics_module_t *module = modules[i];
            if (module->dependencies) {
                for (const char **dep = module->dependencies; *dep; dep++) {
                    if (strcmp(current_module->name, *dep) == 0) {
                        in_degree[i]--;
                        if (in_degree[i] == 0) {
                            queue[queue_back++] = i;
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Check for cycles
    if (result_count != count) {
        free(in_degree);
        free(queue);
        free(result_order);
        return -1; // Circular dependency detected
    }
    
    // Reorder modules array according to topological order
    physics_module_t **temp_modules = malloc(count * sizeof(physics_module_t*));
    if (!temp_modules) {
        free(in_degree);
        free(queue);
        free(result_order);
        return -1;
    }
    
    for (int i = 0; i < count; i++) {
        temp_modules[i] = modules[result_order[i]];
    }
    
    for (int i = 0; i < count; i++) {
        modules[i] = temp_modules[i];
    }
    
    free(in_degree);
    free(queue);
    free(result_order);
    free(temp_modules);
    
    return result_count;
}

int physics_module_registry_resolve_dependencies(const char **requested_modules,
                                                int num_requested,
                                                physics_module_t **ordered_modules,
                                                int max_modules)
{
    if (!requested_modules || num_requested <= 0 || !ordered_modules || max_modules <= 0) {
        return 0;
    }
    
    // Find all requested modules
    physics_module_t *temp_modules[MAX_PHYSICS_MODULES];
    int found_count = 0;
    
    for (int i = 0; i < num_requested && found_count < max_modules; i++) {
        physics_module_t *module = physics_module_registry_find_by_name(requested_modules[i]);
        if (module) {
            temp_modules[found_count] = module;
            found_count++;
        } else {
            fprintf(stderr, "Warning: Requested module '%s' not found\n", requested_modules[i]);
        }
    }
    
    if (found_count == 0) {
        return 0;
    }
    
    // Add dependencies recursively
    bool added_dependency = true;
    while (added_dependency && found_count < max_modules) {
        added_dependency = false;
        
        for (int i = 0; i < found_count; i++) {
            physics_module_t *module = temp_modules[i];
            if (module->dependencies) {
                for (const char **dep = module->dependencies; *dep && found_count < max_modules; dep++) {
                    // Check if dependency is already included
                    bool already_included = false;
                    for (int j = 0; j < found_count; j++) {
                        if (strcmp(temp_modules[j]->name, *dep) == 0) {
                            already_included = true;
                            break;
                        }
                    }
                    
                    if (!already_included) {
                        physics_module_t *dep_module = physics_module_registry_find_by_name(*dep);
                        if (dep_module) {
                            temp_modules[found_count] = dep_module;
                            found_count++;
                            added_dependency = true;
                        } else {
                            fprintf(stderr, "Error: Dependency '%s' not found for module '%s'\n", 
                                    *dep, module->name);
                            return PHYSICS_MODULE_DEPENDENCY_MISSING;
                        }
                    }
                }
            }
        }
    }
    
    // Sort by dependencies
    int sorted_count = topological_sort(temp_modules, found_count);
    if (sorted_count < 0) {
        fprintf(stderr, "Error: Circular dependency detected in physics modules\n");
        return PHYSICS_MODULE_ERROR;
    }
    
    // Copy to output array
    int output_count = (sorted_count < max_modules) ? sorted_count : max_modules;
    for (int i = 0; i < output_count; i++) {
        ordered_modules[i] = temp_modules[i];
    }
    
    return output_count;
}

physics_module_result_t physics_module_registry_validate_all(void)
{
    if (!g_registry.initialized) {
        return PHYSICS_MODULE_ERROR;
    }
    
    for (int i = 0; i < g_registry.module_count; i++) {
        physics_module_t *module = g_registry.modules[i];
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

physics_module_result_t physics_module_registry_initialize_modules(const struct params *run_params)
{
    if (!g_registry.initialized || g_registry.modules_initialized) {
        return PHYSICS_MODULE_ERROR;
    }
    
    if (!run_params) {
        fprintf(stderr, "Error: Cannot initialize modules with NULL parameters\n");
        return PHYSICS_MODULE_ERROR;
    }
    
    // Validate all modules first
    physics_module_result_t validation_result = physics_module_registry_validate_all();
    if (validation_result != PHYSICS_MODULE_SUCCESS) {
        return validation_result;
    }
    
    // Initialize modules in dependency order
    physics_module_t *ordered_modules[MAX_PHYSICS_MODULES];
    const char *all_module_names[MAX_PHYSICS_MODULES];
    
    // Get all module names
    for (int i = 0; i < g_registry.module_count; i++) {
        all_module_names[i] = g_registry.modules[i]->name;
    }
    
    int ordered_count = physics_module_registry_resolve_dependencies(
        all_module_names, g_registry.module_count, ordered_modules, MAX_PHYSICS_MODULES);
    
    if (ordered_count < 0) {
        return PHYSICS_MODULE_ERROR;
    }
    
    // Initialize modules in order
    for (int i = 0; i < ordered_count; i++) {
        physics_module_t *module = ordered_modules[i];
        if (module->initialize) {
            physics_module_result_t result = module->initialize(run_params);
            if (result != PHYSICS_MODULE_SUCCESS) {
                fprintf(stderr, "Error: Failed to initialize module '%s': %s\n", 
                        module->name, physics_module_result_string(result));
                
                // Shutdown previously initialized modules
                for (int j = 0; j < i; j++) {
                    if (ordered_modules[j]->shutdown) {
                        ordered_modules[j]->shutdown();
                    }
                }
                return result;
            }
        }
    }
    
    g_registry.modules_initialized = true;
    return PHYSICS_MODULE_SUCCESS;
}

void physics_module_registry_print_status(bool verbose)
{
    printf("Physics Module Registry Status:\n");
    printf("  Initialized: %s\n", g_registry.initialized ? "Yes" : "No");
    printf("  Modules Initialized: %s\n", g_registry.modules_initialized ? "Yes" : "No");
    printf("  Registered Modules: %d/%d\n", g_registry.module_count, MAX_PHYSICS_MODULES);
    
    if (verbose && g_registry.module_count > 0) {
        printf("\nRegistered Modules:\n");
        for (int i = 0; i < g_registry.module_count; i++) {
            physics_module_t *module = g_registry.modules[i];
            printf("  %d. %s (v%s)\n", i + 1, module->name, module->version);
            printf("     Description: %s\n", module->description ? module->description : "None");
            printf("     Phases: 0x%08X\n", module->supported_phases);
            
            if (module->dependencies && module->dependencies[0]) {
                printf("     Dependencies: ");
                for (const char **dep = module->dependencies; *dep; dep++) {
                    printf("%s%s", *dep, (*(dep + 1)) ? ", " : "");
                }
                printf("\n");
            }
        }
    }
}

// Interface function implementations (called from physics_module_interface.h)
physics_module_result_t physics_module_register(physics_module_t *module)
{
    return physics_module_registry_register(module);
}

physics_module_t* physics_module_find_by_name(const char *name)
{
    return physics_module_registry_find_by_name(name);
}

int physics_module_get_count(void)
{
    return g_registry.initialized ? g_registry.module_count : 0;
}

physics_module_result_t physics_module_initialize_all(const struct params *run_params)
{
    return physics_module_registry_initialize_modules(run_params);
}

void physics_module_shutdown_all(void)
{
    physics_module_registry_shutdown();
}