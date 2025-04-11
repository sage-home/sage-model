/**
 * @file core_module_error.c
 * @brief Implementation of the enhanced error handling system for SAGE modules
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "core_module_error.h"
#include "core_logging.h"
#include "core_module_callback.h"
#include "core_mymalloc.h"

/**
 * Initialize a module error context
 * 
 * Allocates and initializes an error context structure for a module.
 * 
 * @param context Output pointer for the allocated context
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_error_context_init(struct module_error_context **context) {
    if (context == NULL) {
        LOG_ERROR("Invalid arguments to module_error_context_init");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Allocate memory for the error context */
    *context = (struct module_error_context *)mymalloc(sizeof(struct module_error_context));
    if (*context == NULL) {
        LOG_ERROR("Failed to allocate memory for module error context");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize the context fields */
    memset(*context, 0, sizeof(struct module_error_context));
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up a module error context
 * 
 * Releases resources used by an error context structure.
 * 
 * @param context Error context to clean up
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_error_context_cleanup(struct module_error_context *context) {
    if (context == NULL) {
        LOG_WARNING("NULL context passed to module_error_context_cleanup");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Free the context structure */
    myfree(context);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Record an error in a module's error context
 * 
 * Logs detailed error information to a module's error context.
 * 
 * @param module Pointer to the module interface
 * @param error_code Error code to record
 * @param severity Severity level of the error
 * @param file Source file where the error occurred
 * @param line Line number where the error occurred
 * @param func Function where the error occurred
 * @param format Format string for error message
 * @param ... Additional arguments for format string
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_record_error(struct base_module *module, int error_code, 
                       log_level_t severity, const char *file, int line,
                       const char *func, const char *format, ...) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer passed to module_record_error");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Ensure error context is initialized */
    if (module->error_context == NULL) {
        int status = module_error_context_init(&module->error_context);
        if (status != MODULE_STATUS_SUCCESS) {
            return status;
        }
    }
    
    /* Get the next error slot in the circular buffer */
    struct module_error_context *ctx = module->error_context;
    module_error_info_t *error = &ctx->errors[ctx->current_index];
    
    /* Fill in error information */
    error->code = error_code;
    error->severity = severity;
    error->line = line;
    
    /* Copy file and function names safely */
    if (file != NULL) {
        strncpy(error->file, file, sizeof(error->file) - 1);
        error->file[sizeof(error->file) - 1] = '\0';
    } else {
        error->file[0] = '\0';
    }
    
    if (func != NULL) {
        strncpy(error->function, func, sizeof(error->function) - 1);
        error->function[sizeof(error->function) - 1] = '\0';
    } else {
        error->function[0] = '\0';
    }
    
    /* Format the error message */
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    
    /* Record timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    error->timestamp = ts.tv_sec + (ts.tv_nsec / 1.0e9);
    
    /* Capture call stack information */
    if (global_call_stack != NULL) {
        error->call_stack_depth = global_call_stack->depth;
        
        /* Get caller module ID if available */
        if (global_call_stack->depth > 0) {
            module_call_frame_t frame;
            if (module_call_stack_get_current_frame(&frame) == MODULE_STATUS_SUCCESS) {
                error->caller_module_id = frame.caller_module_id;
            } else {
                error->caller_module_id = -1;
            }
        } else {
            error->caller_module_id = -1;
        }
    } else {
        error->call_stack_depth = 0;
        error->caller_module_id = -1;
    }
    
    /* For backward compatibility, also update the last_error fields */
    module->last_error = error_code;
    strncpy(module->error_message, error->message, sizeof(module->error_message) - 1);
    module->error_message[sizeof(module->error_message) - 1] = '\0';
    
    /* Update error context counters */
    ctx->error_count++;
    ctx->current_index = (ctx->current_index + 1) % MAX_ERROR_HISTORY;
    if (ctx->error_count > MAX_ERROR_HISTORY) {
        ctx->overflow = true;
    }
    
    /* Log the error using the core logging system */
    log_message(severity, file, line, func, "Module %s error (%d): %s", 
               module->name, error_code, error->message);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get the most recent error from a module
 * 
 * Retrieves the most recently recorded error from a module's error context.
 * 
 * @param module Pointer to the module interface
 * @param error Output pointer for error information
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_latest_error(struct base_module *module, module_error_info_t *error) {
    if (module == NULL || error == NULL) {
        LOG_ERROR("Invalid arguments to module_get_latest_error");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if the module has an error context */
    if (module->error_context == NULL) {
        LOG_WARNING("Module %s has no error context", module->name);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if any errors have been recorded */
    struct module_error_context *ctx = module->error_context;
    if (ctx->error_count == 0) {
        LOG_DEBUG("No errors recorded for module %s", module->name);
        return MODULE_STATUS_ERROR;
    }
    
    /* Get the index of the most recent error */
    int latest_idx = (ctx->current_index == 0) ? 
                    (MAX_ERROR_HISTORY - 1) : 
                    (ctx->current_index - 1);
    
    /* Copy the error information */
    *error = ctx->errors[latest_idx];
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get error history from a module
 * 
 * Retrieves the error history from a module's error context. Errors are returned
 * in chronological order, with the oldest available error first.
 * 
 * @param module Pointer to the module interface
 * @param errors Output array for error information
 * @param max_errors Maximum number of errors to retrieve
 * @param num_errors Output pointer for the number of errors retrieved
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_error_history(struct base_module *module, 
                           module_error_info_t *errors, 
                           int max_errors, int *num_errors) {
    if (module == NULL || errors == NULL || num_errors == NULL) {
        LOG_ERROR("Invalid arguments to module_get_error_history");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Initialize output */
    *num_errors = 0;
    
    /* Check if the module has an error context */
    if (module->error_context == NULL) {
        LOG_WARNING("Module %s has no error context", module->name);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if any errors have been recorded */
    struct module_error_context *ctx = module->error_context;
    if (ctx->error_count == 0) {
        LOG_DEBUG("No errors recorded for module %s", module->name);
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Determine how many errors to return */
    int available = (ctx->error_count > MAX_ERROR_HISTORY) ? MAX_ERROR_HISTORY : ctx->error_count;
    int to_return = (available < max_errors) ? available : max_errors;
    
    /* Fill the output array */
    int start_idx;
    if (ctx->error_count <= MAX_ERROR_HISTORY) {
        /* Buffer hasn't wrapped yet, start from the beginning */
        start_idx = 0;
    } else {
        /* Buffer has wrapped, start from oldest available error */
        start_idx = ctx->current_index;
    }
    
    for (int i = 0; i < to_return; i++) {
        int idx = (start_idx + i) % MAX_ERROR_HISTORY;
        errors[i] = ctx->errors[idx];
        (*num_errors)++;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Format an error as text
 * 
 * Creates a human-readable string representation of an error.
 * 
 * @param error Error information to format
 * @param buffer Output buffer for formatted text
 * @param buffer_size Size of the output buffer
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_format_error(const module_error_info_t *error, 
                      char *buffer, size_t buffer_size) {
    if (error == NULL || buffer == NULL) {
        LOG_ERROR("Invalid arguments to module_format_error");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Format timestamp */
    char time_buffer[32];
    time_t time_sec = (time_t)error->timestamp;
    struct tm tm_info;
    localtime_r(&time_sec, &tm_info);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
    
    /* Get severity name */
    const char *severity_name = "UNKNOWN";
    switch (error->severity) {
        case LOG_LEVEL_DEBUG:   severity_name = "DEBUG";   break;
        case LOG_LEVEL_INFO:    severity_name = "INFO";    break;
        case LOG_LEVEL_WARNING: severity_name = "WARNING"; break;
        case LOG_LEVEL_ERROR:   severity_name = "ERROR";   break;
        case LOG_LEVEL_FATAL:   severity_name = "FATAL";   break;
    }
    
    /* Format the error */
    int written = snprintf(buffer, buffer_size,
                         "[%s] %s (%d): %s\n"
                         "  Location: %s:%d in %s\n"
                         "  Call stack depth: %d\n"
                         "  Caller module ID: %d",
                         time_buffer, severity_name, error->code, error->message,
                         error->file, error->line, error->function,
                         error->call_stack_depth, error->caller_module_id);
    
    if (written < 0 || written >= (int)buffer_size) {
        LOG_WARNING("Error formatting truncated (buffer size: %zu)", buffer_size);
        /* Ensure null termination */
        buffer[buffer_size - 1] = '\0';
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Enhanced set_error function with detailed context
 * 
 * Updates a module's error state with detailed contextual information.
 * This is an enhanced version of the basic module_set_error function.
 * 
 * @param module Pointer to the module interface
 * @param error_code Error code to set
 * @param severity Severity level of the error
 * @param file Source file where the error occurred
 * @param line Line number where the error occurred
 * @param func Function where the error occurred
 * @param format Format string for error message
 * @param ... Additional arguments for format string
 */
void module_set_error_ex(struct base_module *module, int error_code,
                        log_level_t severity, const char *file, int line,
                        const char *func, const char *format, ...) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer passed to module_set_error_ex");
        return;
    }
    
    /* Format the error message */
    char message[MAX_ERROR_MESSAGE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* For backward compatibility, update the basic error fields */
    module->last_error = error_code;
    strncpy(module->error_message, message, sizeof(module->error_message) - 1);
    module->error_message[sizeof(module->error_message) - 1] = '\0';
    
    /* Record the error with full context */
    module_record_error(module, error_code, severity, file, line, func, "%s", message);
}
