#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_module_callback.h"
#include "core_module_system.h"
#include "core_logging.h"
#include "core_mymalloc.h"
#include "core_pipeline_system.h"

/**
 * @file core_module_callback.c
 * @brief Implementation of the module callback system
 * 
 * This file contains the implementation of the module callback system, which
 * allows modules to call functions in other modules with proper dependency
 * tracking, error handling, and call stack management.
 * 
 * The callback system supports:
 * - Declaring dependencies between modules
 * - Calling functions in other modules
 * - Tracking the call stack for diagnostics
 * - Detecting and preventing circular dependencies
 * - Error propagation between modules
 * 
 * To use the callback system:
 * 1. A module must declare its dependencies using module_declare_dependency()
 * 2. It can then call functions in other modules using module_invoke()
 * 3. Errors are automatically tracked and can be handled at the call site
 */

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
    /* Allocate and initialize global call stack */
    global_call_stack = calloc(1, sizeof(module_call_stack_t));
    if (global_call_stack == NULL) {
        LOG_ERROR("Failed to allocate memory for module call stack");
        return -1;
    }
    
    global_call_stack->depth = 0;
    
    LOG_DEBUG("Module callback system initialized");
    return 0;
}

/**
 * Clean up the module callback system
 * 
 * Releases resources used by the callback system.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
void module_callback_system_cleanup(void) {
    if (global_call_stack != NULL) {
        /* Check for unclosed calls */
        if (global_call_stack->depth > 0) {
            LOG_WARNING("Cleaning up call stack with %d unclosed calls", 
                       global_call_stack->depth);
        }
        
        free(global_call_stack);
        global_call_stack = NULL;
    }
    
    LOG_DEBUG("Module callback system cleaned up");
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
    if (global_call_stack == NULL) {
        LOG_ERROR("Call stack not initialized");
        return -1;
    }
    
    if (global_call_stack->depth >= MAX_CALL_DEPTH) {
        LOG_ERROR("Call stack overflow (max depth %d)", MAX_CALL_DEPTH);
        return -1;
    }
    
    /* Check for circular dependencies */
    if (module_call_stack_check_circular(callee_id)) {
        LOG_ERROR("Circular dependency detected calling module %d", callee_id);
        return -1;
    }
    
    /* Create new frame */
    module_call_frame_t *frame = &global_call_stack->frames[global_call_stack->depth];
    frame->caller_module_id = caller_id;
    frame->callee_module_id = callee_id;
    frame->function_name = function_name;
    frame->context = context;
    frame->error_code = 0;
    frame->has_error = false;
    frame->error_message[0] = '\0';
    
    global_call_stack->depth++;
    
    LOG_DEBUG("Pushed call frame: %s (caller=%d, callee=%d)", 
              function_name, caller_id, callee_id);
    
    return 0;
}

/**
 * Pop a frame from the call stack
 * 
 * Removes the most recent invocation record.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
module_call_frame_t *module_call_stack_pop(void) {
    if (global_call_stack == NULL || global_call_stack->depth == 0) {
        return NULL;
    }
    
    global_call_stack->depth--;
    module_call_frame_t *frame = &global_call_stack->frames[global_call_stack->depth];
    
    LOG_DEBUG("Popped call frame: %s (caller=%d, callee=%d)", 
              frame->function_name, frame->caller_module_id, frame->callee_module_id);
    
    return frame;
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
    if (global_call_stack == NULL) {
        return false;
    }
    
    for (int i = 0; i < global_call_stack->depth; i++) {
        if (global_call_stack->frames[i].callee_module_id == module_id) {
            return true;
        }
    }
    
    return false;
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
    if (buffer == NULL || buffer_size == 0) {
        return -1;
    }
    
    if (global_call_stack == NULL || global_call_stack->depth == 0) {
        snprintf(buffer, buffer_size, "<empty call stack>");
        return 0;
    }
    
    size_t offset = 0;
    
    for (int i = 0; i < global_call_stack->depth && offset < buffer_size; i++) {
        module_call_frame_t *frame = &global_call_stack->frames[i];
        
        int written = snprintf(buffer + offset, buffer_size - offset,
                             "%s%s%s[%d->%d]",
                             (i > 0) ? " -> " : "",
                             frame->function_name,
                             frame->has_error ? "(error)" : "",
                             frame->caller_module_id,
                             frame->callee_module_id);
        
        if (written < 0 || written >= (int)(buffer_size - offset)) {
            /* Buffer full */
            break;
        }
        
        offset += written;
    }
    
    return 0;
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
 * Get the current call stack trace with error information
 * 
 * Returns a formatted string representation of the call stack including error details.
 * 
 * @param buffer Output buffer for the call stack trace
 * @param buffer_size Size of the output buffer
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_get_trace_with_errors(char *buffer, size_t buffer_size) {
    /* Check arguments */
    if (buffer == NULL || buffer_size == 0) {
        LOG_ERROR("Invalid arguments to module_call_stack_get_trace_with_errors");
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
        
        /* Format basic frame information */
        int written = snprintf(ptr, remaining, 
                              "%d: %s (ID: %d) -> %s (ID: %d)::%s", 
                              i, 
                              caller_name, frame->caller_module_id,
                              callee_name, frame->callee_module_id,
                              frame->function_name ? frame->function_name : "unknown");
        
        /* Check if we ran out of space */
        if (written < 0 || written >= (int)remaining) {
            /* Indicate truncation */
            strncpy(buffer + buffer_size - 12, " [truncated]", 11);
            buffer[buffer_size - 1] = '\0';
            break;
        }
        
        /* Update pointers */
        ptr += written;
        remaining -= written;
        
        /* Add error information if present */
        if (frame->has_error) {
            written = snprintf(ptr, remaining, 
                              " [ERROR %d: %s]", 
                              frame->error_code, 
                              frame->error_message);
            
            /* Check if we ran out of space */
            if (written < 0 || written >= (int)remaining) {
                /* Indicate truncation */
                strncpy(buffer + buffer_size - 12, " [truncated]", 11);
                buffer[buffer_size - 1] = '\0';
                break;
            }
            
            /* Update pointers */
            ptr += written;
            remaining -= written;
        }
        
        /* Add newline */
        if (remaining > 0) {
            *ptr++ = '\n';
            *ptr = '\0';
            remaining--;
        }
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set error information for a frame in the call stack
 * 
 * Associates error details with a specific call frame.
 * 
 * @param frame_index Index of the frame to set error on (0 is oldest, depth-1 is newest)
 * @param error_code Error code to set
 * @param error_message Error message to set
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_set_frame_error(int frame_index, int error_code, 
                                     const char *error_message) {
    /* Check arguments */
    if (error_message == NULL) {
        LOG_ERROR("Invalid arguments to module_call_stack_set_frame_error");
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
    
    /* Check if frame_index is valid */
    if (frame_index < 0 || frame_index >= global_call_stack->depth) {
        LOG_ERROR("Invalid frame index: %d (depth: %d)", 
                 frame_index, global_call_stack->depth);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Set error information in the frame */
    module_call_frame_t *frame = &global_call_stack->frames[frame_index];
    frame->error_code = error_code;
    strncpy(frame->error_message, error_message, MAX_ERROR_MESSAGE - 1);
    frame->error_message[MAX_ERROR_MESSAGE - 1] = '\0';
    frame->has_error = true;
    
    /* Log for debugging */
    LOG_DEBUG("Set error on call frame %d: code=%d, message='%s'", 
             frame_index, error_code, error_message);
    
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
    
    /* Parse the version strings */
    dep->has_parsed_versions = false;
    if (min_version_str != NULL && min_version_str[0] != '\0') {
        if (module_parse_version(min_version_str, &dep->min_version) == MODULE_STATUS_SUCCESS) {
            dep->has_parsed_versions = true;
        }
    }
    
    if (max_version_str != NULL && max_version_str[0] != '\0') {
        module_parse_version(max_version_str, &dep->max_version);
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
    /* Skip dependency validation for system calls (negative caller_ids) */
    if (caller_id < 0) {
        printf("DEBUG_INVOKE: Skipping dependency validation for system caller_id=%d\n", caller_id);
        return MODULE_STATUS_SUCCESS;
    }
    
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
            if ((int)caller->dependencies[i].type == (int)callee->type) {
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

/**
 * Execute a function in a module with callback tracking
 * 
 * This function can be used by core_pipeline_system.c to execute a 
 * function with proper call stack management.
 * 
 * @param context Pipeline context
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @param function_name Name of the function being called
 * @param callback_context Context data for the callback
 * @param func Function to execute with context
 * @return Result of the function execution
 */
int module_execute_with_callback(
    struct pipeline_context *context,
    int caller_id,
    int callee_id,
    const char *function_name,
    void *callback_context,
    int (*func)(void *, struct pipeline_context *)
);

/**
 * Get the current call frame
 * 
 * Returns a pointer to the current call frame (most recent).
 * 
 * @return Pointer to current call frame, or NULL if stack is empty
 */
module_call_frame_t *module_call_stack_current(void) {
    if (global_call_stack == NULL || global_call_stack->depth == 0) {
        return NULL;
    }
    
    return &global_call_stack->frames[global_call_stack->depth - 1];
}

/**
 * Set error information for the current frame
 *
 * A convenience function to set error information on the most recent frame.
 *
 * @param error_code Error code to set
 * @param error_message Error message to set
 */
void module_call_set_error(int error_code, const char *error_message) {
    if (global_call_stack == NULL || global_call_stack->depth == 0 || error_message == NULL) {
        return;
    }

    module_call_frame_t *frame = &global_call_stack->frames[global_call_stack->depth - 1];
    frame->error_code = error_code;
    
    /* Copy the error message */
    strncpy(frame->error_message, error_message, MAX_ERROR_MESSAGE - 1);
    frame->error_message[MAX_ERROR_MESSAGE - 1] = '\0';
    
    frame->error_message[MAX_ERROR_MESSAGE - 1] = '\0';
    frame->has_error = true;
    
    /* Log the error for debugging */
    LOG_DEBUG("Set error on current call frame: code=%d, message='%s'", 
             error_code, frame->error_message);
}

/**
 * Clear error information for the current frame
 * 
 * Removes any error information from the most recent frame.
 */
void module_call_clear_error(void) {
    if (global_call_stack == NULL || global_call_stack->depth == 0) {
        return;
    }
    
    module_call_frame_t *frame = &global_call_stack->frames[global_call_stack->depth - 1];
    frame->error_code = 0;
    frame->error_message[0] = '\0';
    frame->has_error = false;
}

/**
 * Invoke a function in another module
 * 
 * Calls a registered function in a module.
 * 
 * @param caller_id ID of the calling module
 * @param module_type Type of module to invoke (can be MODULE_TYPE_UNKNOWN if module_name is provided)
 * @param module_name Optional specific module name (can be NULL to use active module of type)
 * @param function_name Name of function to call
 * @param context Context to pass to function
 * @param args Arguments to pass to the function
 * @param result Optional pointer to store result (type depends on function)
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
/* The implementation of module_execute_with_callback below is used
 * by pipeline_execute_with_callback in physics_pipeline_executor.c
 */


int module_invoke(
    int caller_id,
    int module_type,
    const char *module_name,
    const char *function_name,
    void *context,
    void *args,
    void *result
) {
    printf("DEBUG_INVOKE_START: module_invoke called with caller_id=%d, module_name=%s, function_name=%s\n", 
           caller_id, module_name ? module_name : "NULL", function_name ? function_name : "NULL");
           
    /* Check arguments */
    if (function_name == NULL) {
        LOG_ERROR("Invalid arguments to module_invoke (function_name is NULL)");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* We need either a module type or a module name */
    if (module_type == MODULE_TYPE_UNKNOWN && (module_name == NULL || module_name[0] == '\0')) {
        LOG_ERROR("Invalid arguments to module_invoke (both module_type and module_name are invalid)");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Get the caller module (only for regular module IDs, not system callers) */
    struct base_module *caller = NULL;
    void *caller_data = NULL;
    int status; // Declare status variable for use throughout function
    
    if (caller_id >= 0) {
        int caller_status = module_get(caller_id, &caller, &caller_data);
        if (caller_status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Failed to get caller module %d: %d", caller_id, caller_status);
            return caller_status;
        }
    } else {
        /* System-level caller, no module struct to fetch */
        printf("DEBUG_INVOKE: System-level caller (ID: %d)\n", caller_id);
    }
    
    /* Find target module */
    int target_id = -1;
    struct base_module *target = NULL;
    void *target_data = NULL;
    
    if (module_name != NULL && module_name[0] != '\0') {
        /* Look for module by name */
        target_id = module_find_by_name(module_name);
        if (target_id < 0) {
            LOG_ERROR("Module '%s' not found", module_name);
            return MODULE_STATUS_MODULE_NOT_FOUND;
        }
        
        /* Get the module */
        status = module_get(target_id, &target, &target_data);
        if (status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Failed to get target module '%s': %d", module_name, status);
            return status;
        }
        
        /* Check that module is of expected type if one was specified */
        if ((int)module_type != (int)MODULE_TYPE_UNKNOWN && (int)target->type != (int)module_type) {
            LOG_ERROR("Module '%s' is of type %s, expected %s",
                     module_name, module_type_name(target->type), module_type_name(module_type));
            return MODULE_STATUS_ERROR;
        }
    } else {
        /* Look for active module of the specified type */
        status = module_get_active_by_type(module_type, &target, &target_data);
        if (status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("No active module of type %s found", module_type_name(module_type));
            return MODULE_STATUS_MODULE_NOT_FOUND;
        }
        
        /* Get the module ID */
        target_id = target->module_id;
    }
    
    /* Validate the call */
    printf("DEBUG_INVOKE: About to validate call from caller_id=%d to target_id=%d\n", caller_id, target_id);
    status = module_call_validate(caller_id, target_id);
    if (status != MODULE_STATUS_SUCCESS) {
        /* module_call_validate already logs detailed error messages */
        printf("DEBUG_INVOKE: Call validation failed with status %d\n", status);
        return status;
    }
    printf("DEBUG_INVOKE: Call validation successful\n");
    
    /* Find the function in the target module */
    if (target->function_registry == NULL) {
        printf("DEBUG_INVOKE: Target module '%s' has NULL function_registry!\n", target->name);
        LOG_ERROR("Target module '%s' has no function registry", target->name);
        return MODULE_STATUS_ERROR;
    }
    
    printf("DEBUG_INVOKE: In module '%s' (ID %d), searching for function: ----%s----\n",
           target->name, target_id, function_name);
    
    /* Search for the function by name */
    module_function_t *func = NULL;
    for (int i = 0; i < target->function_registry->num_functions; i++) {
        printf("DEBUG_INVOKE: Comparing with registered function: ----%s----\n",
               target->function_registry->functions[i].name);
        if (strcmp(target->function_registry->functions[i].name, function_name) == 0) {
            printf("DEBUG_INVOKE: Found match for function '%s'\n", function_name);
            func = &target->function_registry->functions[i];
            break;
        }
    }
    
    if (func == NULL) {
        printf("DEBUG_INVOKE: Function '%s' NOT FOUND in module '%s' after checking %d functions.\n",
               function_name, target->name, target->function_registry->num_functions);
        LOG_ERROR("Function '%s' not found in module '%s'", function_name, target->name);
        return MODULE_STATUS_ERROR;
    }
    
    /* Push to call stack */
    status = module_call_stack_push(caller_id, target_id, function_name, context);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to push call stack frame: %d", status);
        return status;
    }
    
    /* Execute the function based on its return type */
    switch (func->return_type) {
        case FUNCTION_TYPE_VOID: {
            /* Call void function */
            void (*func_ptr)(void *, void *) = (void (*)(void *, void *))func->function_ptr;
            func_ptr(args, context);
            break;
        }
        
        case FUNCTION_TYPE_INT: {
            /* Call int function */
            int (*func_ptr)(void *, void *) = (int (*)(void *, void *))func->function_ptr;
            int ret = func_ptr(args, context);
            
            /* Store result if requested */
            if (result != NULL) {
                *((int *)result) = ret;
            }
            break;
        }
        
        case FUNCTION_TYPE_DOUBLE: {
            /* Call double function */
            double (*func_ptr)(void *, void *) = (double (*)(void *, void *))func->function_ptr;
            double ret = func_ptr(args, context);
            
            /* Store result if requested */
            if (result != NULL) {
                *((double *)result) = ret;
            }
            break;
        }
        
        case FUNCTION_TYPE_POINTER: {
            /* Call pointer function */
            void *(*func_ptr)(void *, void *) = (void *(*)(void *, void *))func->function_ptr;
            void *ret = func_ptr(args, context);
            
            /* Store result if requested */
            if (result != NULL) {
                *((void **)result) = ret;
            }
            break;
        }
        
        default:
            LOG_ERROR("Unknown function return type: %d", func->return_type);
            module_call_stack_pop();
            return MODULE_STATUS_ERROR;
    }
    
    /* Pop from call stack */
    module_call_stack_pop();
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Execute a function in a module with callback tracking
 *
 * Wraps a module function execution in a callback tracking system.
 *
 * @param context Pipeline context for execution
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @param function_name Name of the function being called
 * @param callback_context Context data for the callback
 * @param func Function to execute
 * @return Result of the function execution
 */
int module_execute_with_callback(
    struct pipeline_context *context,
    int caller_id,
    int callee_id,
    const char *function_name,
    void *callback_context,
    int (*func)(void *, struct pipeline_context *)
) {
    /* Save previous callback state from context if any */
    int prev_caller_id = context->caller_module_id;
    const char *prev_function = context->current_function;
    void *prev_context = context->callback_context;
    
    /* Set new callback state */
    context->caller_module_id = caller_id;
    context->current_function = function_name;
    context->callback_context = callback_context;
    
    /* Push to call stack */
    int status = module_call_stack_push(caller_id, callee_id, function_name, callback_context);
    if (status != 0) {
        LOG_ERROR("Failed to push call stack frame: %d", status);
        
        /* Restore previous callback state */
        context->caller_module_id = prev_caller_id;
        context->current_function = prev_function;
        context->callback_context = prev_context;
        
        return status;
    }
    
    /* Execute the function */
    status = func(callback_context, context);
    
    /* Pop from call stack */
    module_call_stack_pop();
    
    /* Check for any leftover frames (module didn't clean up properly) */
    if (global_call_stack->depth > 0) {
        LOG_WARNING("Leftover call frames after module_execute_with_callback");
        while (global_call_stack->depth > 0) {
            module_call_stack_pop();
        }
    }
    
    /* Restore previous callback state */
    context->caller_module_id = prev_caller_id;
    context->current_function = prev_function;
    context->callback_context = prev_context;
    
    return status;
}