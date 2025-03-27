#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_module_callback.h"
#include "core_module_system.h"
#include "core_logging.h"
#include "core_mymalloc.h"

/* Global call stack */
module_call_stack_t *global_call_stack = NULL;

/**
 * Initialize the module callback system
 * 
 * Sets up the global call stack and prepares it for module callbacks.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_callback_system_initialize(void) {
    /* Check if already initialized */
    if (global_call_stack != NULL) {
        LOG_WARNING("Module callback system already initialized");
        return MODULE_STATUS_ALREADY_INITIALIZED;
    }
    
    /* Allocate memory for the call stack */
    global_call_stack = (module_call_stack_t *)mymalloc(sizeof(module_call_stack_t));
    if (global_call_stack == NULL) {
        LOG_ERROR("Failed to allocate memory for call stack");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize call stack fields */
    memset(global_call_stack, 0, sizeof(module_call_stack_t));
    global_call_stack->depth = 0;
    
    LOG_INFO("Module callback system initialized");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up the module callback system
 * 
 * Releases resources used by the callback system.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_callback_system_cleanup(void) {
    if (global_call_stack == NULL) {
        LOG_WARNING("Module callback system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Check for leftover call frames */
    if (global_call_stack->depth > 0) {
        LOG_WARNING("Call stack not empty during cleanup (depth: %d)", global_call_stack->depth);
    }
    
    /* Free the call stack */
    myfree(global_call_stack);
    global_call_stack = NULL;
    
    LOG_INFO("Module callback system cleaned up");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Register a function with a module
 * 
 * Makes a function callable from other modules.
 * 
 * @param module_id ID of the module containing the function
 * @param name Name of the function (used for lookup)
 * @param function_ptr Pointer to the function
 * @param return_type Type of value returned by the function
 * @param signature Optional signature string
 * @param description Optional description
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_register_function(
    int module_id,
    const char *name,
    void *function_ptr,
    module_function_type_t return_type,
    const char *signature,
    const char *description
) {
    /* Check arguments */
    if (name == NULL || function_ptr == NULL) {
        LOG_ERROR("Invalid arguments to module_register_function");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Get the module */
    struct base_module *module = NULL;
    void *module_data = NULL;
    int status = module_get(module_id, &module, &module_data);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to get module %d: %d", module_id, status);
        return status;
    }
    
    /* Initialize function registry if needed */
    if (module->function_registry == NULL) {
        module->function_registry = (module_function_registry_t *)mymalloc(sizeof(module_function_registry_t));
        if (module->function_registry == NULL) {
            LOG_ERROR("Failed to allocate memory for function registry");
            return MODULE_STATUS_OUT_OF_MEMORY;
        }
        memset(module->function_registry, 0, sizeof(module_function_registry_t));
    }
    
    /* Check for duplicate function */
    module_function_registry_t *registry = module->function_registry;
    for (int i = 0; i < registry->num_functions; i++) {
        if (strcmp(registry->functions[i].name, name) == 0) {
            LOG_ERROR("Function '%s' already registered with module %d", name, module_id);
            return MODULE_STATUS_ERROR;
        }
    }
    
    /* Check if registry is full */
    if (registry->num_functions >= MAX_MODULE_FUNCTIONS) {
        LOG_ERROR("Function registry full for module %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Add the function to the registry */
    module_function_t *func = &registry->functions[registry->num_functions];
    strncpy(func->name, name, MAX_FUNCTION_NAME - 1);
    func->function_ptr = function_ptr;
    func->return_type = return_type;
    func->signature = signature;
    func->description = description;
    
    registry->num_functions++;
    
    LOG_INFO("Registered function '%s' with module %d", name, module_id);
    return MODULE_STATUS_SUCCESS;
}

/**
 * Push a frame onto the call stack
 * 
 * Records a module invocation.
 * 
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @param function_name Name of the function being called
 * @param context Context for the call
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_push(
    int caller_id,
    int callee_id,
    const char *function_name,
    void *context
) {
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Check if stack is full */
    if (global_call_stack->depth >= MAX_CALL_DEPTH) {
        LOG_ERROR("Call stack overflow (max depth: %d)", MAX_CALL_DEPTH);
        return MODULE_STATUS_ERROR;
    }
    
    /* Add frame to stack */
    module_call_frame_t *frame = &global_call_stack->frames[global_call_stack->depth];
    frame->caller_module_id = caller_id;
    frame->callee_module_id = callee_id;
    frame->function_name = function_name;
    frame->context = context;
    
    global_call_stack->depth++;
    
    /* Log the call for debugging */
    LOG_DEBUG("Module call: %d -> %d::%s (depth: %d)", 
             caller_id, callee_id, function_name, global_call_stack->depth);
             
    return MODULE_STATUS_SUCCESS;
}

/**
 * Pop a frame from the call stack
 * 
 * Removes the most recent invocation record.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_pop(void) {
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Check if stack is empty */
    if (global_call_stack->depth <= 0) {
        LOG_ERROR("Call stack underflow");
        return MODULE_STATUS_ERROR;
    }
    
    /* Remove frame from stack */
    global_call_stack->depth--;
    
    /* Log the return for debugging */
    module_call_frame_t *frame = &global_call_stack->frames[global_call_stack->depth];
    LOG_DEBUG("Module return: %d <- %d::%s (depth: %d)", 
             frame->caller_module_id, frame->callee_module_id, 
             frame->function_name, global_call_stack->depth);
             
    return MODULE_STATUS_SUCCESS;
}

/**
 * Check for circular dependencies
 * 
 * Examines the call stack for loops.
 * 
 * @param module_id ID of the module to check
 * @return true if a circular dependency exists, false otherwise
 */
bool module_call_stack_check_circular(int module_id) {
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return false;
    }
    
    /* Search for the module in the call stack */
    for (int i = 0; i < global_call_stack->depth; i++) {
        if (global_call_stack->frames[i].callee_module_id == module_id) {
            /* Get the module name for better diagnostics */
            const char *module_name = "unknown";
            struct base_module *module = NULL;
            void *module_data = NULL;
            if (module_get(module_id, &module, &module_data) == MODULE_STATUS_SUCCESS && module != NULL) {
                module_name = module->name;
            }
            
            LOG_WARNING("Circular dependency detected: module %s (ID: %d) already in call chain at depth %d", 
                       module_name, module_id, i);
            
            /* Print the call chain for debugging */
            char trace_buffer[1024];
            module_call_stack_get_trace(trace_buffer, sizeof(trace_buffer));
            LOG_DEBUG("Call stack trace:\n%s", trace_buffer);
            
            return true;
        }
    }
    
    return false;
}

/**
 * Get the current call depth
 * 
 * Returns the number of frames on the call stack.
 * 
 * @return Current call depth
 */
int module_call_stack_get_depth(void) {
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return -1;
    }
    
    return global_call_stack->depth;
}

/**
 * Get the current call stack trace
 * 
 * Returns a formatted string representation of the call stack for debugging.
 * 
 * @param buffer Output buffer for the call stack trace
 * @param buffer_size Size of the output buffer
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_get_trace(char *buffer, size_t buffer_size) {
    /* Check arguments */
    if (buffer == NULL || buffer_size == 0) {
        LOG_ERROR("Invalid arguments to module_call_stack_get_trace");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Start with an empty string */
    buffer[0] = '\0';
    size_t remaining = buffer_size - 1;  /* Leave room for null terminator */
    char *ptr = buffer;
    
    /* Handle empty call stack */
    if (global_call_stack->depth == 0) {
        strncpy(ptr, "Call stack is empty", remaining);
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Format each frame in the call stack */
    for (int i = 0; i < global_call_stack->depth; i++) {
        module_call_frame_t *frame = &global_call_stack->frames[i];
        
        /* Get module names for better diagnostics */
        const char *caller_name = "unknown";
        const char *callee_name = "unknown";
        struct base_module *caller_module = NULL;
        struct base_module *callee_module = NULL;
        void *dummy_data = NULL;
        
        if (module_get(frame->caller_module_id, &caller_module, &dummy_data) == MODULE_STATUS_SUCCESS && 
            caller_module != NULL) {
            caller_name = caller_module->name;
        }
        
        if (module_get(frame->callee_module_id, &callee_module, &dummy_data) == MODULE_STATUS_SUCCESS && 
            callee_module != NULL) {
            callee_name = callee_module->name;
        }
        
        /* Format this frame */
        int written = snprintf(ptr, remaining, 
                              "%d: %s (ID: %d) -> %s (ID: %d)::%s\n", 
                              i, 
                              caller_name, frame->caller_module_id,
                              callee_name, frame->callee_module_id,
                              frame->function_name ? frame->function_name : "unknown");
        
        /* Check if we ran out of space */
        if (written < 0 || written >= remaining) {
            /* Indicate truncation */
            strncpy(buffer + buffer_size - 12, " [truncated]", 11);
            buffer[buffer_size - 1] = '\0';
            break;
        }
        
        /* Update pointers */
        ptr += written;
        remaining -= written;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get information about a specific call frame
 * 
 * Retrieves details about a call frame at the specified depth.
 * 
 * @param depth Index of the call frame to retrieve (0 is the oldest frame)
 * @param frame Output pointer to store the call frame information
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_get_frame(int depth, module_call_frame_t *frame) {
    /* Check arguments */
    if (frame == NULL) {
        LOG_ERROR("Invalid arguments to module_call_stack_get_frame");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Check if depth is valid */
    if (depth < 0 || depth >= global_call_stack->depth) {
        LOG_ERROR("Invalid call stack depth: %d (current depth: %d)", 
                 depth, global_call_stack->depth);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Copy the frame */
    *frame = global_call_stack->frames[depth];
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Find module in call stack
 * 
 * Searches the call stack for a specific module and returns its position.
 * 
 * @param module_id ID of the module to look for
 * @param as_caller Whether to look for the module as a caller (true) or callee (false)
 * @return Position in the call stack, or -1 if not found
 */
int module_call_stack_find_module(int module_id, bool as_caller) {
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return -1;
    }
    
    /* Search for the module in the call stack */
    for (int i = 0; i < global_call_stack->depth; i++) {
        if ((as_caller && global_call_stack->frames[i].caller_module_id == module_id) ||
            (!as_caller && global_call_stack->frames[i].callee_module_id == module_id)) {
            return i;
        }
    }
    
    return -1;  /* Not found */
}

/**
 * Get the most recent call frame
 * 
 * Returns the call frame at the top of the stack.
 * 
 * @param frame Output pointer to store the call frame information
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_get_current_frame(module_call_frame_t *frame) {
    /* Check arguments */
    if (frame == NULL) {
        LOG_ERROR("Invalid arguments to module_call_stack_get_current_frame");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        LOG_ERROR("Module callback system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Check if call stack is empty */
    if (global_call_stack->depth <= 0) {
        LOG_ERROR("Call stack is empty");
        return MODULE_STATUS_ERROR;
    }
    
    /* Get the most recent frame */
    *frame = global_call_stack->frames[global_call_stack->depth - 1];
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Declare a module dependency
 * 
 * Adds a dependency to a module with version constraints.
 * 
 * @param module_id ID of the module declaring the dependency
 * @param module_type Type of module depended on
 * @param module_name Optional specific module name
 * @param required Whether dependency is required
 * @param min_version_str Minimum version required (can be NULL)
 * @param max_version_str Maximum version allowed (can be NULL)
 * @param exact_match Whether an exact version match is required
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_declare_dependency(
    int module_id,
    int module_type,
    const char *module_name,
    bool required,
    const char *min_version_str,
    const char *max_version_str,
    bool exact_match
) {
    /* Get the module */
    struct base_module *module = NULL;
    void *module_data = NULL;
    int status = module_get(module_id, &module, &module_data);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to get module %d: %d", module_id, status);
        return status;
    }
    
    /* Reallocate dependencies array if needed */
    if (module->dependencies == NULL) {
        module->dependencies = (module_dependency_t *)mymalloc(sizeof(module_dependency_t));
        if (module->dependencies == NULL) {
            LOG_ERROR("Failed to allocate memory for dependency");
            return MODULE_STATUS_OUT_OF_MEMORY;
        }
        module->num_dependencies = 0;
    } else {
        module_dependency_t *new_deps = (module_dependency_t *)myrealloc(
            module->dependencies,
            sizeof(module_dependency_t) * (module->num_dependencies + 1)
        );
        if (new_deps == NULL) {
            LOG_ERROR("Failed to reallocate memory for dependencies");
            return MODULE_STATUS_OUT_OF_MEMORY;
        }
        module->dependencies = new_deps;
    }
    
    /* Add the new dependency */
    module_dependency_t *dep = &module->dependencies[module->num_dependencies];
    memset(dep, 0, sizeof(module_dependency_t));
    
    /* Set dependency properties */
    if (module_name != NULL) {
        strncpy(dep->name, module_name, MAX_DEPENDENCY_NAME - 1);
    }
    
    const char *type_name = module_type_name(module_type);
    strncpy(dep->module_type, type_name, sizeof(dep->module_type) - 1);
    
    dep->type = module_type;
    dep->optional = !required;
    dep->exact_match = exact_match;
    
    /* Handle version constraints */
    if (min_version_str != NULL) {
        strncpy(dep->min_version_str, min_version_str, sizeof(dep->min_version_str) - 1);
    } else {
        dep->min_version_str[0] = '\0';
    }
    
    if (max_version_str != NULL) {
        strncpy(dep->max_version_str, max_version_str, sizeof(dep->max_version_str) - 1);
    } else {
        dep->max_version_str[0] = '\0';
    }
    
    /* Increment dependency count */
    module->num_dependencies++;
    
    LOG_INFO("Added %s dependency from module %d to %s%s%s%s%s%s%s",
             required ? "required" : "optional",
             module_id,
             dep->module_type,
             module_name ? " " : "",
             module_name ? dep->name : "",
             min_version_str ? " (min version: " : "",
             min_version_str ? min_version_str : "",
             min_version_str ? ")" : "",
             exact_match ? " (exact match required)" : "");
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Simplified dependency declaration
 * 
 * Adds a basic dependency without version constraints.
 * 
 * @param module_id ID of the module declaring the dependency
 * @param module_type Type of module depended on
 * @param module_name Optional specific module name
 * @param required Whether dependency is required
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_declare_simple_dependency(
    int module_id,
    int module_type,
    const char *module_name,
    bool required
) {
    return module_declare_dependency(
        module_id,
        module_type,
        module_name,
        required,
        NULL,  /* No minimum version */
        NULL,  /* No maximum version */
        false  /* No exact match required */
    );
}

/**
 * Check if a module call is allowed
 * 
 * Validates if a call from one module to another is valid based on dependencies.
 * 
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_validate(int caller_id, int callee_id) {
    /* Get the caller module */
    struct base_module *caller = NULL;
    void *caller_data = NULL;
    int status = module_get(caller_id, &caller, &caller_data);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to get caller module %d: %d", caller_id, status);
        return status;
    }
    
    /* Get the callee module */
    struct base_module *callee = NULL;
    void *callee_data = NULL;
    status = module_get(callee_id, &callee, &callee_data);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to get callee module %d: %d", callee_id, status);
        return status;
    }
    
    /* Check if caller has declared dependency on callee */
    bool dependency_found = false;
    
    /* Look for direct dependency on the callee by name */
    for (int i = 0; i < caller->num_dependencies; i++) {
        if (caller->dependencies[i].name[0] != '\0' && 
            strcmp(caller->dependencies[i].name, callee->name) == 0) {
            dependency_found = true;
            break;
        }
    }
    
    /* If no direct dependency by name, look for dependency on callee's type */
    if (!dependency_found) {
        for (int i = 0; i < caller->num_dependencies; i++) {
            if (caller->dependencies[i].type == callee->type) {
                dependency_found = true;
                break;
            }
        }
    }
    
    /* Check for circular dependencies */
    if (module_call_stack_check_circular(callee_id)) {
        LOG_ERROR("Circular dependency detected when calling from %s to %s",
                 caller->name, callee->name);
        return MODULE_STATUS_DEPENDENCY_CONFLICT;
    }
    
    /* If dependency was found and no circular reference, call is valid */
    if (dependency_found) {
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Log detailed error to help diagnose missing dependencies */
    LOG_ERROR("Invalid module call: %s (ID: %d) has not declared dependency on %s (ID: %d, type: %s)",
              caller->name, caller_id, 
              callee->name, callee_id, 
              module_type_name(callee->type));
    
    return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
}