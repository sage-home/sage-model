/**
 * @file core_module_diagnostics.h
 * @brief Module diagnostic utilities for SAGE
 * 
 * This file provides utilities for generating user-friendly diagnostic
 * information about module errors, call stacks, and runtime state.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "core_module_system.h"
#include "core_module_error.h"
#include "core_module_callback.h"

/* Maximum size of a diagnostic output buffer */
#define MAX_DIAGNOSTIC_BUFFER 4096

/**
 * Diagnostic options structure
 * 
 * Controls the behavior of diagnostic utilities
 */
typedef struct {
    bool include_timestamps;      /* Whether to include timestamps in diagnostic output */
    bool include_file_info;       /* Whether to include file/line info in diagnostic output */
    bool include_call_stack;      /* Whether to include call stack in diagnostic output */
    bool verbose;                 /* Whether to generate verbose diagnostic information */
    int max_errors;               /* Maximum number of errors to include in diagnostics */
} module_diagnostic_options_t;

/**
 * Initialize diagnostic options with defaults
 * 
 * @param options Options structure to initialize
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_diagnostic_options_init(module_diagnostic_options_t *options);

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
                               const module_diagnostic_options_t *options);

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
                                    const module_diagnostic_options_t *options);

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
                                       const module_diagnostic_options_t *options);

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
                           const module_diagnostic_options_t *options);

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
int module_get_name_from_id(int module_id, char *buffer, size_t buffer_size);

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
                              const module_diagnostic_options_t *options);

#ifdef __cplusplus
}
#endif
