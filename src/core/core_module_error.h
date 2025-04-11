/**
 * @file core_module_error.h
 * @brief Enhanced error handling system for SAGE modules
 * 
 * This file defines an enhanced error context system for tracking and reporting
 * detailed error information within the SAGE module architecture. It provides
 * a history of errors, detailed context, and integration with the logging system.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include "core_allvars.h"
#include "core_logging.h"
#include "core_module_system.h"

/* Maximum number of errors to keep in history */
#define MAX_ERROR_HISTORY 10

/**
 * Module error information structure
 * 
 * Contains detailed information about a single error
 */
typedef struct {
    int code;                          /* Error code */
    log_level_t severity;              /* Error severity */
    char message[MAX_ERROR_MESSAGE];   /* Error message */
    char function[64];                 /* Function where error occurred */
    char file[128];                    /* File where error occurred */
    int line;                          /* Line number */
    double timestamp;                  /* When the error occurred */
    
    /* Call stack information if available */
    int call_stack_depth;              /* Depth of call stack when error occurred */
    int caller_module_id;              /* ID of the calling module (if applicable) */
} module_error_info_t;

/**
 * Module error context structure
 * 
 * Manages error history and state for a module
 */
struct module_error_context {
    module_error_info_t errors[MAX_ERROR_HISTORY]; /* Circular buffer of errors */
    int error_count;                   /* Total errors recorded */
    int current_index;                 /* Current position in circular buffer */
    bool overflow;                     /* Whether buffer has overflowed */
};

/**
 * Initialize a module error context
 * 
 * Allocates and initializes an error context structure for a module.
 * 
 * @param context Output pointer for the allocated context
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_error_context_init(struct module_error_context **context);

/**
 * Clean up a module error context
 * 
 * Releases resources used by an error context structure.
 * 
 * @param context Error context to clean up
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_error_context_cleanup(struct module_error_context *context);

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
                       const char *func, const char *format, ...);

/**
 * Get the most recent error from a module
 * 
 * Retrieves the most recently recorded error from a module's error context.
 * 
 * @param module Pointer to the module interface
 * @param error Output pointer for error information
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_latest_error(struct base_module *module, module_error_info_t *error);

/**
 * Get error history from a module
 * 
 * Retrieves the error history from a module's error context.
 * 
 * @param module Pointer to the module interface
 * @param errors Output array for error information
 * @param max_errors Maximum number of errors to retrieve
 * @param num_errors Output pointer for the number of errors retrieved
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_error_history(struct base_module *module, 
                           module_error_info_t *errors, 
                           int max_errors, int *num_errors);

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
                      char *buffer, size_t buffer_size);

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
                        const char *func, const char *format, ...);

/* Convenience macros for module error reporting */

/**
 * Report an error from a module with automatic context capture
 * 
 * @param module Pointer to the module interface
 * @param code Error code to report
 * @param format Format string for error message
 * @param ... Additional arguments for format string
 */
#define MODULE_ERROR(module, code, format, ...) \
    module_set_error_ex(module, code, LOG_LEVEL_ERROR, __FILE__, __LINE__, \
                      __func__, format, ##__VA_ARGS__)

/**
 * Report a warning from a module with automatic context capture
 * 
 * @param module Pointer to the module interface
 * @param code Warning code to report
 * @param format Format string for warning message
 * @param ... Additional arguments for format string
 */
#define MODULE_WARNING(module, code, format, ...) \
    module_set_error_ex(module, code, LOG_LEVEL_WARNING, __FILE__, __LINE__, \
                       __func__, format, ##__VA_ARGS__)

/**
 * Report debug information from a module with automatic context capture
 * 
 * @param module Pointer to the module interface
 * @param code Debug code to report
 * @param format Format string for debug message
 * @param ... Additional arguments for format string
 */
#define MODULE_DEBUG(module, code, format, ...) \
    module_set_error_ex(module, code, LOG_LEVEL_DEBUG, __FILE__, __LINE__, \
                      __func__, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
