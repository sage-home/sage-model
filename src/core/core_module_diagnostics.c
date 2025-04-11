/**
 * @file core_module_diagnostics.c
 * @brief Implementation of module diagnostic utilities for SAGE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core_module_diagnostics.h"
#include "core_logging.h"
#include "core_mymalloc.h"

/**
 * Initialize diagnostic options with defaults
 * 
 * @param options Options structure to initialize
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_diagnostic_options_init(module_diagnostic_options_t *options) {
    if (options == NULL) {
        LOG_ERROR("NULL options pointer passed to module_diagnostic_options_init");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Set default options */
    options->include_timestamps = true;
    options->include_file_info = true;
    options->include_call_stack = true;
    options->verbose = false;
    options->max_errors = 5;  /* Show at most 5 errors by default */
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get diagnostic information about a module's error state
 * 
 * Generates a user-friendly diagnostic report for a module's errors.
 * 
 * @param module_id ID of the module to diagnose
 * @param buffer Output buffer for diagnostic information
 * @param buffer_size Size of the output buffer
 * @param options Diagnostic options (can be NULL for defaults)
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_error_diagnostics(int module_id, char *buffer, size_t buffer_size,
                               const module_diagnostic_options_t *options) {
    /* Check arguments */
    if (buffer == NULL || buffer_size == 0) {
        LOG_ERROR("Invalid arguments to module_get_error_diagnostics");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Use default options if not provided */
    module_diagnostic_options_t default_options;
    if (options == NULL) {
        module_diagnostic_options_init(&default_options);
        options = &default_options;
    }
    
    /* Get the module */
    struct base_module *module = NULL;
    void *module_data = NULL;
    int status = module_get(module_id, &module, &module_data);
    if (status != MODULE_STATUS_SUCCESS) {
        snprintf(buffer, buffer_size, "Error: Failed to get module with ID %d", module_id);
        return status;
    }
    
    /* Check if the module has an error context */
    if (module->error_context == NULL) {
        snprintf(buffer, buffer_size, "Module '%s' (ID: %d) has no error context",
                module->name, module_id);
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Check if any errors have been recorded */
    struct module_error_context *ctx = module->error_context;
    if (ctx->error_count == 0) {
        snprintf(buffer, buffer_size, "Module '%s' (ID: %d) has no recorded errors",
                module->name, module_id);
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Get error history */
    module_error_info_t errors[MAX_ERROR_HISTORY];
    int num_errors = 0;
    status = module_get_error_history(module, errors, options->max_errors, &num_errors);
    if (status != MODULE_STATUS_SUCCESS) {
        snprintf(buffer, buffer_size, "Failed to retrieve error history for module '%s' (ID: %d)",
                module->name, module_id);
        return status;
    }
    
    /* Format the diagnostic report */
    int written = 0;
    written += snprintf(buffer + written, buffer_size - written,
                      "Diagnostic report for module '%s' (ID: %d):\n",
                      module->name, module_id);
    
    if (written >= (int)buffer_size) {
        goto truncated;
    }
    
    written += snprintf(buffer + written, buffer_size - written,
                      "Total recorded errors: %d\n\n",
                      ctx->error_count);
    
    if (written >= (int)buffer_size) {
        goto truncated;
    }
    
    /* Format each error */
    for (int i = 0; i < num_errors; i++) {
        if (written >= (int)buffer_size) {
            goto truncated;
        }
        
        written += snprintf(buffer + written, buffer_size - written,
                          "Error %d/%d:\n", i + 1, num_errors);
        
        if (written >= (int)buffer_size) {
            goto truncated;
        }
        
        char error_buffer[512];
        module_format_error(&errors[i], error_buffer, sizeof(error_buffer));
        
        written += snprintf(buffer + written, buffer_size - written,
                          "%s\n\n", error_buffer);
    }
    
    if (ctx->error_count > num_errors) {
        written += snprintf(buffer + written, buffer_size - written,
                          "(Showing %d of %d total errors)\n",
                          num_errors, ctx->error_count);
    }
    
    return MODULE_STATUS_SUCCESS;
    
truncated:
    /* Indicate truncation */
    strncpy(buffer + buffer_size - 13, " [truncated]", 12);
    buffer[buffer_size - 1] = '\0';
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get the name of a module from its ID
 * 
 * Utility function for diagnostic output.
 * 
 * @param module_id ID of the module
 * @param buffer Output buffer for the module name
 * @param buffer_size Size of the output buffer
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_name_from_id(int module_id, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        LOG_ERROR("Invalid arguments to module_get_name_from_id");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Special case for unknown or invalid module ID */
    if (module_id < 0) {
        strncpy(buffer, "unknown", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Get the module */
    struct base_module *module = NULL;
    void *module_data = NULL;
    int status = module_get(module_id, &module, &module_data);
    if (status != MODULE_STATUS_SUCCESS) {
        snprintf(buffer, buffer_size, "unknown (ID: %d)", module_id);
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Copy the module name */
    strncpy(buffer, module->name, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Format a call frame in a user-friendly way
 * 
 * Creates a readable representation of a single call frame.
 * 
 * @param frame Call frame to format
 * @param buffer Output buffer for formatted text
 * @param buffer_size Size of the output buffer
 * @param options Diagnostic options (can be NULL for defaults)
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_format_call_frame(const module_call_frame_t *frame, char *buffer, size_t buffer_size,
                           const module_diagnostic_options_t *options) {
    if (frame == NULL || buffer == NULL || buffer_size == 0) {
        LOG_ERROR("Invalid arguments to module_format_call_frame");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Use default options if not provided */
    module_diagnostic_options_t default_options;
    if (options == NULL) {
        module_diagnostic_options_init(&default_options);
        options = &default_options;
    }
    
    /* Get module names */
    char caller_name[MAX_MODULE_NAME];
    char callee_name[MAX_MODULE_NAME];
    
    module_get_name_from_id(frame->caller_module_id, caller_name, sizeof(caller_name));
    module_get_name_from_id(frame->callee_module_id, callee_name, sizeof(callee_name));
    
    /* Format the frame */
    int written = snprintf(buffer, buffer_size,
                         "%s (ID: %d) -> %s (ID: %d)::%s",
                         caller_name, frame->caller_module_id,
                         callee_name, frame->callee_module_id,
                         frame->function_name ? frame->function_name : "unknown");
    
    if (written < 0 || written >= (int)buffer_size) {
        /* Indicate truncation */
        strncpy(buffer + buffer_size - 13, " [truncated]", 12);
        buffer[buffer_size - 1] = '\0';
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Convert a call stack to a user-friendly string
 * 
 * Formats the entire call stack as a readable string.
 * 
 * @param buffer Output buffer for the formatted call stack
 * @param buffer_size Size of the output buffer
 * @param options Diagnostic options (can be NULL for defaults)
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_to_string(char *buffer, size_t buffer_size,
                              const module_diagnostic_options_t *options) {
    if (buffer == NULL || buffer_size == 0) {
        LOG_ERROR("Invalid arguments to module_call_stack_to_string");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Use default options if not provided */
    module_diagnostic_options_t default_options;
    if (options == NULL) {
        module_diagnostic_options_init(&default_options);
        options = &default_options;
    }
    
    /* Check if callback system is initialized */
    if (global_call_stack == NULL) {
        strncpy(buffer, "Call stack not initialized", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Handle empty call stack */
    if (global_call_stack->depth == 0) {
        strncpy(buffer, "Call stack is empty", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Format call stack */
    int written = 0;
    written += snprintf(buffer + written, buffer_size - written,
                      "Call stack (depth: %d):\n", global_call_stack->depth);
    
    for (int i = 0; i < global_call_stack->depth; i++) {
        if (written >= (int)buffer_size) {
            goto truncated;
        }
        
        module_call_frame_t *frame = &global_call_stack->frames[i];
        
        written += snprintf(buffer + written, buffer_size - written, "%d: ", i);
        
        if (written >= (int)buffer_size) {
            goto truncated;
        }
        
        char frame_buffer[256];
        module_format_call_frame(frame, frame_buffer, sizeof(frame_buffer), options);
        
        written += snprintf(buffer + written, buffer_size - written, "%s\n", frame_buffer);
    }
    
    return MODULE_STATUS_SUCCESS;
    
truncated:
    /* Indicate truncation */
    strncpy(buffer + buffer_size - 13, " [truncated]", 12);
    buffer[buffer_size - 1] = '\0';
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get diagnostic information about the current call stack
 * 
 * Generates a user-friendly representation of the current module call stack.
 * 
 * @param buffer Output buffer for diagnostic information
 * @param buffer_size Size of the output buffer
 * @param options Diagnostic options (can be NULL for defaults)
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_call_stack_diagnostics(char *buffer, size_t buffer_size,
                                    const module_diagnostic_options_t *options) {
    return module_call_stack_to_string(buffer, buffer_size, options);
}

/**
 * Get comprehensive diagnostic information for a runtime error
 * 
 * Combines error and call stack diagnostics into a comprehensive report.
 * 
 * @param module_id ID of the module that encountered the error
 * @param buffer Output buffer for diagnostic information
 * @param buffer_size Size of the output buffer
 * @param options Diagnostic options (can be NULL for defaults)
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_comprehensive_diagnostics(int module_id, char *buffer, size_t buffer_size,
                                       const module_diagnostic_options_t *options) {
    if (buffer == NULL || buffer_size == 0) {
        LOG_ERROR("Invalid arguments to module_get_comprehensive_diagnostics");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Use default options if not provided */
    module_diagnostic_options_t default_options;
    if (options == NULL) {
        module_diagnostic_options_init(&default_options);
        options = &default_options;
    }
    
    int written = 0;
    
    /* Add a header */
    written += snprintf(buffer + written, buffer_size - written,
                      "=== Comprehensive Diagnostic Report ===\n\n");
    
    if (written >= (int)buffer_size) {
        goto truncated;
    }
    
    /* Add module error diagnostics */
    char error_buffer[MAX_DIAGNOSTIC_BUFFER / 2];
    module_get_error_diagnostics(module_id, error_buffer, sizeof(error_buffer), options);
    
    written += snprintf(buffer + written, buffer_size - written,
                      "--- Module Error Information ---\n%s\n\n", error_buffer);
    
    if (written >= (int)buffer_size) {
        goto truncated;
    }
    
    /* Add call stack diagnostics if enabled */
    if (options->include_call_stack) {
        char stack_buffer[MAX_DIAGNOSTIC_BUFFER / 2];
        
        /* Use enhanced call stack trace with errors if available */
        if (global_call_stack != NULL && global_call_stack->depth > 0) {
            module_call_stack_get_trace_with_errors(stack_buffer, sizeof(stack_buffer));
            written += snprintf(buffer + written, buffer_size - written,
                              "--- Call Stack With Errors ---\n%s\n", stack_buffer);
        } else {
            module_get_call_stack_diagnostics(stack_buffer, sizeof(stack_buffer), options);
            written += snprintf(buffer + written, buffer_size - written,
                              "--- Call Stack Information ---\n%s\n", stack_buffer);
        }
    }
    
    if (written >= (int)buffer_size) {
        goto truncated;
    }
    
    return MODULE_STATUS_SUCCESS;
    
truncated:
    /* Indicate truncation */
    strncpy(buffer + buffer_size - 13, " [truncated]", 12);
    buffer[buffer_size - 1] = '\0';
    return MODULE_STATUS_SUCCESS;
}
