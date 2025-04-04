#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "io_validation.h"
#include "io_interface.h"
#include "../core/core_logging.h"

/**
 * @brief Initialize a validation context
 *
 * Sets up a validation context with the specified strictness level.
 *
 * @param ctx Validation context to initialize
 * @param strictness Strictness level
 * @return 0 on success, non-zero on failure
 */
int validation_init(struct validation_context *ctx, 
                    enum validation_strictness strictness) {
    if (ctx == NULL) {
        return -1;
    }
    
    memset(ctx, 0, sizeof(struct validation_context));
    ctx->strictness = strictness;
    ctx->max_results = MAX_VALIDATION_RESULTS;
    ctx->num_results = 0;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->abort_on_first_error = false;
    ctx->custom_context = NULL;
    
    return 0;
}

/**
 * @brief Clean up a validation context
 *
 * Releases resources associated with a validation context.
 *
 * @param ctx Validation context to clean up
 */
void validation_cleanup(struct validation_context *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Reset all fields
    memset(ctx, 0, sizeof(struct validation_context));
}

/**
 * @brief Reset a validation context
 *
 * Clears all results but keeps the configuration.
 *
 * @param ctx Validation context to reset
 */
void validation_reset(struct validation_context *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Clear results but keep configuration
    enum validation_strictness strictness = ctx->strictness;
    int max_results = ctx->max_results;
    bool abort_on_first_error = ctx->abort_on_first_error;
    void *custom_context = ctx->custom_context;
    
    // Clear all results
    memset(ctx->results, 0, sizeof(ctx->results));
    ctx->num_results = 0;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    
    // Restore configuration
    ctx->strictness = strictness;
    ctx->max_results = max_results;
    ctx->abort_on_first_error = abort_on_first_error;
    ctx->custom_context = custom_context;
}

/**
 * @brief Configure validation context
 *
 * Updates validation context configuration.
 *
 * @param ctx Validation context to configure
 * @param strictness Strictness level (or -1 to leave unchanged)
 * @param max_results Maximum results to collect (or -1 to leave unchanged)
 * @param abort_on_first_error Whether to abort on first error (or -1 to leave unchanged)
 */
void validation_configure(struct validation_context *ctx,
                         int strictness,
                         int max_results,
                         int abort_on_first_error) {
    if (ctx == NULL) {
        return;
    }
    
    if (strictness >= 0 && strictness <= VALIDATION_STRICTNESS_STRICT) {
        ctx->strictness = (enum validation_strictness)strictness;
    }
    
    if (max_results > 0) {
        ctx->max_results = max_results < MAX_VALIDATION_RESULTS ? 
                           max_results : MAX_VALIDATION_RESULTS;
    }
    
    if (abort_on_first_error >= 0) {
        ctx->abort_on_first_error = abort_on_first_error != 0;
    }
}

/**
 * @brief Add a validation result
 *
 * Adds a new validation result to the context.
 *
 * @param ctx Validation context
 * @param code Error code
 * @param severity Severity level
 * @param check_type Type of validation check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param message_format Message format string
 * @param ... Message format arguments
 * @return 0 if validation can continue, non-zero if validation should stop
 */
int validation_add_result(struct validation_context *ctx,
                         enum validation_error_code code,
                         enum validation_severity severity,
                         enum validation_check_type check_type,
                         const char *component,
                         const char *file,
                         int line,
                         const char *message_format,
                         ...) {
    if (ctx == NULL) {
        return -1;
    }
    
    // Apply strictness rules to decide whether to record the result
    if (severity == VALIDATION_SEVERITY_WARNING && 
        ctx->strictness == VALIDATION_STRICTNESS_RELAXED) {
        // Skip warnings in relaxed mode
        return 0;
    }
    
    // In strict mode, warnings are treated as errors
    if (severity == VALIDATION_SEVERITY_WARNING && 
        ctx->strictness == VALIDATION_STRICTNESS_STRICT) {
        severity = VALIDATION_SEVERITY_ERROR;
    }
    
    // Update counters
    if (severity >= VALIDATION_SEVERITY_ERROR) {
        ctx->error_count++;
    } else if (severity == VALIDATION_SEVERITY_WARNING) {
        ctx->warning_count++;
    }
    
    // Store the result if there's room and it's not a duplicate
    if (ctx->num_results < ctx->max_results) {
        struct validation_result *result = &ctx->results[ctx->num_results++];
        
        result->code = code;
        result->severity = severity;
        result->check_type = check_type;
        result->file = file;
        result->line = line;
        result->context = ctx->custom_context;
        
        // Copy component name with truncation if necessary
        if (component != NULL) {
            strncpy(result->component, component, MAX_VALIDATION_COMPONENT - 1);
            result->component[MAX_VALIDATION_COMPONENT - 1] = '\0';
        } else {
            result->component[0] = '\0';
        }
        
        // Format the message
        va_list args;
        va_start(args, message_format);
        vsnprintf(result->message, MAX_VALIDATION_MESSAGE, message_format, args);
        va_end(args);
    }
    
    // Determine whether to continue validation
    if (severity >= VALIDATION_SEVERITY_ERROR && ctx->abort_on_first_error) {
        return -1;  // Stop validation
    }
    
    if (severity == VALIDATION_SEVERITY_FATAL) {
        return -1;  // Always stop on fatal errors
    }
    
    return 0;  // Continue validation
}

/**
 * @brief Check if validation passed
 *
 * @param ctx Validation context
 * @return true if validation passed, false otherwise
 */
bool validation_passed(const struct validation_context *ctx) {
    if (ctx == NULL) {
        return false;
    }
    
    // Validation passes if there are no errors
    return ctx->error_count == 0;
}

/**
 * @brief Get validation result count
 *
 * @param ctx Validation context
 * @return Total number of validation results
 */
int validation_get_result_count(const struct validation_context *ctx) {
    if (ctx == NULL) {
        return 0;
    }
    
    return ctx->num_results;
}

/**
 * @brief Get validation error count
 *
 * @param ctx Validation context
 * @return Number of validation errors
 */
int validation_get_error_count(const struct validation_context *ctx) {
    if (ctx == NULL) {
        return 0;
    }
    
    return ctx->error_count;
}

/**
 * @brief Get validation warning count
 *
 * @param ctx Validation context
 * @return Number of validation warnings
 */
int validation_get_warning_count(const struct validation_context *ctx) {
    if (ctx == NULL) {
        return 0;
    }
    
    return ctx->warning_count;
}

/**
 * @brief Check if validation has errors
 *
 * @param ctx Validation context
 * @return true if validation has errors, false otherwise
 */
bool validation_has_errors(const struct validation_context *ctx) {
    if (ctx == NULL) {
        return false;
    }
    
    return ctx->error_count > 0;
}

/**
 * @brief Check if validation has warnings
 *
 * @param ctx Validation context
 * @return true if validation has warnings, false otherwise
 */
bool validation_has_warnings(const struct validation_context *ctx) {
    if (ctx == NULL) {
        return false;
    }
    
    return ctx->warning_count > 0;
}

/**
 * @brief Map validation severity to log level
 *
 * @param severity Validation severity
 * @return Corresponding log level
 */
static log_level_t severity_to_log_level(enum validation_severity severity) {
    switch (severity) {
        case VALIDATION_SEVERITY_INFO:
            return LOG_LEVEL_INFO;
        case VALIDATION_SEVERITY_WARNING:
            return LOG_LEVEL_WARNING;
        case VALIDATION_SEVERITY_ERROR:
            return LOG_LEVEL_ERROR;
        case VALIDATION_SEVERITY_FATAL:
            return LOG_LEVEL_FATAL;
        default:
            return LOG_LEVEL_INFO;
    }
}

/**
 * @brief Get string representation of validation error code
 * 
 * @param code Validation error code
 * @return String representation
 */
static const char *validation_error_string(enum validation_error_code code) {
    switch (code) {
        case VALIDATION_SUCCESS:
            return "Success";
        case VALIDATION_ERROR_NULL_POINTER:
            return "Null Pointer";
        case VALIDATION_ERROR_ARRAY_BOUNDS:
            return "Array Bounds";
        case VALIDATION_ERROR_INVALID_VALUE:
            return "Invalid Value";
        case VALIDATION_ERROR_TYPE_MISMATCH:
            return "Type Mismatch";
        case VALIDATION_ERROR_LOGICAL_CONSTRAINT:
            return "Logical Constraint";
        case VALIDATION_ERROR_FORMAT_INCOMPATIBLE:
            return "Format Incompatible";
        case VALIDATION_ERROR_PROPERTY_INCOMPATIBLE:
            return "Property Incompatible";
        case VALIDATION_ERROR_PROPERTY_MISSING:
            return "Property Missing";
        case VALIDATION_ERROR_REFERENCE_INVALID:
            return "Reference Invalid";
        case VALIDATION_ERROR_DATA_INCONSISTENT:
            return "Data Inconsistent";
        case VALIDATION_ERROR_RESOURCE_LIMIT:
            return "Resource Limit";
        case VALIDATION_ERROR_INTERNAL:
            return "Internal Error";
        case VALIDATION_ERROR_UNKNOWN:
        default:
            return "Unknown Error";
    }
}

/**
 * @brief Get string representation of validation check type
 * 
 * @param check_type Validation check type
 * @return String representation
 */
static const char *validation_check_type_string(enum validation_check_type check_type) {
    switch (check_type) {
        case VALIDATION_CHECK_GALAXY_DATA:
            return "Galaxy Data";
        case VALIDATION_CHECK_GALAXY_REFS:
            return "Galaxy References";
        case VALIDATION_CHECK_FORMAT_CAPS:
            return "Format Capabilities";
        case VALIDATION_CHECK_PROPERTY_COMPAT:
            return "Property Compatibility";
        case VALIDATION_CHECK_IO_PARAMS:
            return "I/O Parameters";
        case VALIDATION_CHECK_RESOURCE:
            return "Resources";
        case VALIDATION_CHECK_CONSISTENCY:
            return "Consistency";
        default:
            return "Unknown Check";
    }
}

/**
 * @brief Report validation results
 *
 * Logs validation results with appropriate severity.
 *
 * @param ctx Validation context
 * @return Number of validation errors
 */
int validation_report(const struct validation_context *ctx) {
    if (ctx == NULL) {
        return 0;
    }
    
    if (ctx->num_results == 0) {
        // Log a success message if there are no results
        LOG_INFO("Validation passed with no issues");
        return 0;
    }
    
    // Log a summary
    if (ctx->error_count > 0) {
        LOG_ERROR("Validation failed with %d error(s) and %d warning(s)",
                 ctx->error_count, ctx->warning_count);
    } else if (ctx->warning_count > 0) {
        LOG_WARNING("Validation passed with %d warning(s)", ctx->warning_count);
    } else {
        LOG_INFO("Validation passed with %d info message(s)", ctx->num_results);
    }
    
    // Log each result
    for (int i = 0; i < ctx->num_results; i++) {
        const struct validation_result *result = &ctx->results[i];
        log_level_t level = severity_to_log_level(result->severity);
        
        // Build a detailed message
        char detail[MAX_VALIDATION_MESSAGE + 128];
        snprintf(detail, sizeof(detail), "[%s/%s] %s: %s",
                validation_check_type_string(result->check_type),
                result->component,
                validation_error_string(result->code),
                result->message);
        
        // Log with appropriate level and location info
        log_message(level, result->file, result->line, __func__, detail);
    }
    
    return ctx->error_count;
}

/**
 * @brief Map validation error to I/O error
 *
 * Converts a validation error to an I/O error.
 *
 * @param validation_error Validation error code
 * @return Corresponding I/O error code
 */
int validation_map_to_io_error(enum validation_error_code validation_error) {
    switch (validation_error) {
        case VALIDATION_SUCCESS:
            return IO_ERROR_NONE;
        case VALIDATION_ERROR_NULL_POINTER:
        case VALIDATION_ERROR_ARRAY_BOUNDS:
        case VALIDATION_ERROR_INVALID_VALUE:
        case VALIDATION_ERROR_TYPE_MISMATCH:
        case VALIDATION_ERROR_LOGICAL_CONSTRAINT:
        case VALIDATION_ERROR_DATA_INCONSISTENT:
            return IO_ERROR_VALIDATION_FAILED;
        case VALIDATION_ERROR_FORMAT_INCOMPATIBLE:
            return IO_ERROR_UNSUPPORTED_OP;
        case VALIDATION_ERROR_PROPERTY_INCOMPATIBLE:
        case VALIDATION_ERROR_PROPERTY_MISSING:
            return IO_ERROR_FORMAT_ERROR;
        case VALIDATION_ERROR_REFERENCE_INVALID:
            return IO_ERROR_HANDLE_INVALID;
        case VALIDATION_ERROR_RESOURCE_LIMIT:
            return IO_ERROR_RESOURCE_LIMIT;
        case VALIDATION_ERROR_INTERNAL:
        case VALIDATION_ERROR_UNKNOWN:
        default:
            return IO_ERROR_UNKNOWN;
    }
}

/**
 * @brief Validate that a pointer is not NULL
 *
 * @param ctx Validation context
 * @param ptr Pointer to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param message_format Message format string
 * @param ... Message format arguments
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_not_null(struct validation_context *ctx,
                            const void *ptr,
                            const char *component,
                            const char *file,
                            int line,
                            const char *message_format,
                            ...) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (ptr != NULL) {
        return 0;  // Validation passed
    }
    
    // Format the message
    char message[MAX_VALIDATION_MESSAGE];
    va_list args;
    va_start(args, message_format);
    vsnprintf(message, sizeof(message), message_format, args);
    va_end(args);
    
    // Add the result - note we always return non-zero for NULL pointers
    // regardless of what add_result returns
    validation_add_result(ctx, VALIDATION_ERROR_NULL_POINTER,
                        VALIDATION_SEVERITY_ERROR,
                        VALIDATION_CHECK_CONSISTENCY,
                        component, file, line, "%s", message);
    return -1;  // Always return error for NULL pointers
}

/**
 * @brief Validate that a value is not NaN or infinity
 *
 * @param ctx Validation context
 * @param value Value to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param message_format Message format string
 * @param ... Message format arguments
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_finite(struct validation_context *ctx,
                          double value,
                          const char *component,
                          const char *file,
                          int line,
                          const char *message_format,
                          ...) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (isfinite(value)) {
        return 0;  // Validation passed
    }
    
    // Format the message
    char message[MAX_VALIDATION_MESSAGE];
    va_list args;
    va_start(args, message_format);
    vsnprintf(message, sizeof(message), message_format, args);
    va_end(args);
    
    // Add details about the value
    char detail[MAX_VALIDATION_MESSAGE];
    if (isnan(value)) {
        snprintf(detail, sizeof(detail), "%s (NaN)", message);
    } else if (isinf(value) && value > 0) {
        snprintf(detail, sizeof(detail), "%s (+Infinity)", message);
    } else if (isinf(value)) {
        snprintf(detail, sizeof(detail), "%s (-Infinity)", message);
    } else {
        snprintf(detail, sizeof(detail), "%s", message);
    }
    
    // Add the result - always return non-zero for non-finite values
    validation_add_result(ctx, VALIDATION_ERROR_INVALID_VALUE,
                        VALIDATION_SEVERITY_ERROR,
                        VALIDATION_CHECK_GALAXY_DATA,
                        component, file, line, "%s", detail);
    return -1;  // Always return error for non-finite
}

/**
 * @brief Validate that an index is within bounds
 *
 * @param ctx Validation context
 * @param index Index to check
 * @param min_value Minimum valid value (inclusive)
 * @param max_value Maximum valid value (exclusive)
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param message_format Message format string
 * @param ... Message format arguments
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_bounds(struct validation_context *ctx,
                          int64_t index,
                          int64_t min_value,
                          int64_t max_value,
                          const char *component,
                          const char *file,
                          int line,
                          const char *message_format,
                          ...) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (index >= min_value && index < max_value) {
        return 0;  // Validation passed
    }
    
    // Format the message
    char message[MAX_VALIDATION_MESSAGE];
    va_list args;
    va_start(args, message_format);
    vsnprintf(message, sizeof(message), message_format, args);
    va_end(args);
    
    // Add details about the bounds
    char detail[MAX_VALIDATION_MESSAGE];
    snprintf(detail, sizeof(detail), 
            "%s (value = %"PRId64", valid range = [%"PRId64", %"PRId64"))",
            message, index, min_value, max_value);
    
    // Add the result - always return non-zero for out-of-bounds
    validation_add_result(ctx, VALIDATION_ERROR_ARRAY_BOUNDS,
                        VALIDATION_SEVERITY_ERROR,
                        VALIDATION_CHECK_GALAXY_REFS,
                        component, file, line, "%s", detail);
    return -1;  // Always return error for out-of-bounds
}

/**
 * @brief Validate that a condition is true
 *
 * @param ctx Validation context
 * @param condition Condition to check
 * @param severity Severity if condition fails
 * @param code Error code if condition fails
 * @param check_type Type of validation check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param message_format Message format string
 * @param ... Message format arguments
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_condition(struct validation_context *ctx,
                             bool condition,
                             enum validation_severity severity,
                             enum validation_error_code code,
                             enum validation_check_type check_type,
                             const char *component,
                             const char *file,
                             int line,
                             const char *message_format,
                             ...) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (condition) {
        return 0;  // Validation passed
    }
    
    // Format the message
    char message[MAX_VALIDATION_MESSAGE];
    va_list args;
    va_start(args, message_format);
    vsnprintf(message, sizeof(message), message_format, args);
    va_end(args);
    
    // Add the result, but for error severity always return error code
    int result = validation_add_result(ctx, code, severity, check_type,
                               component, file, line, "%s", message);
    
    // For error or fatal severity, always return non-zero
    if (severity >= VALIDATION_SEVERITY_ERROR) {
        return (result != 0) ? result : -1;
    }
    
    return result;
}

/**
 * @brief Validate that a format supports a capability
 *
 * @param ctx Validation context
 * @param handler I/O interface handler
 * @param capability Capability to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param message_format Message format string
 * @param ... Message format arguments
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_capability(struct validation_context *ctx,
                              const struct io_interface *handler,
                              enum io_capabilities capability,
                              const char *component,
                              const char *file,
                              int line,
                              const char *message_format,
                              ...) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (handler == NULL) {
        // Handler is NULL, report that
        return validation_add_result(ctx, VALIDATION_ERROR_NULL_POINTER,
                                   VALIDATION_SEVERITY_ERROR,
                                   VALIDATION_CHECK_FORMAT_CAPS,
                                   component, file, line,
                                   "I/O handler is NULL");
    }
    
    if (io_has_capability((struct io_interface *)handler, capability)) {
        return 0;  // Validation passed
    }
    
    // Format the message
    char message[MAX_VALIDATION_MESSAGE];
    va_list args;
    va_start(args, message_format);
    vsnprintf(message, sizeof(message), message_format, args);
    va_end(args);
    
    // Add details about the capability
    char detail[MAX_VALIDATION_MESSAGE];
    snprintf(detail, sizeof(detail), 
            "%s (handler %s does not support required capability)",
            message, handler->name);
    
    // Add the result - always return non-zero for incompatible formats
    validation_add_result(ctx, VALIDATION_ERROR_FORMAT_INCOMPATIBLE,
                        VALIDATION_SEVERITY_ERROR,
                        VALIDATION_CHECK_FORMAT_CAPS,
                        component, file, line, "%s", detail);
    return -1;  // Always return error for format incompatibility
}

/**
 * @brief Check if a galaxy has valid references
 * 
 * Validates that a galaxy's internal references are correct.
 * 
 * @param ctx Validation context
 * @param galaxy Galaxy to check
 * @param index Galaxy index
 * @param count Total number of galaxies
 * @param component Component being validated
 * @return 0 if validation passed, non-zero otherwise
 */
static int validate_galaxy_references(struct validation_context *ctx,
                                    const struct GALAXY *galaxy,
                                    int index,
                                    int count,
                                    const char *component) {
    int status = 0;
    
    // Check mergeIntoID is valid
    if (galaxy->mergeIntoID != -1) {
        if (galaxy->mergeIntoID < -1 || galaxy->mergeIntoID >= count) {
            status |= validation_add_result(ctx, VALIDATION_ERROR_REFERENCE_INVALID,
                                          VALIDATION_SEVERITY_ERROR,
                                          VALIDATION_CHECK_GALAXY_REFS, component,
                                          __FILE__, __LINE__,
                                          "Galaxy %d has invalid mergeIntoID = %d (valid range: -1 to %d)",
                                          index, galaxy->mergeIntoID, count - 1);
        }
    }
    
    // More checks can be added here as needed...
    
    return status;
}

/**
 * @brief Check if a galaxy has valid numeric values
 * 
 * Validates that a galaxy's numeric properties are not NaN/Inf.
 * 
 * @param ctx Validation context
 * @param galaxy Galaxy to check
 * @param index Galaxy index
 * @param component Component being validated
 * @return 0 if validation passed, non-zero otherwise
 */
static int validate_galaxy_values(struct validation_context *ctx,
                                const struct GALAXY *galaxy,
                                int index,
                                const char *component) {
    int status = 0;
    
    // Check numeric fields for NaN/Inf
    status |= validation_check_finite(ctx, galaxy->StellarMass, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid StellarMass", index);
    status |= validation_check_finite(ctx, galaxy->BulgeMass, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid BulgeMass", index);
    status |= validation_check_finite(ctx, galaxy->HotGas, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid HotGas", index);
    status |= validation_check_finite(ctx, galaxy->ColdGas, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid ColdGas", index);
    
    // More checks can be added here as needed...
    
    return status;
}

/**
 * @brief Check if a galaxy has logical consistency
 * 
 * Validates that a galaxy's properties follow logical constraints.
 * 
 * @param ctx Validation context
 * @param galaxy Galaxy to check
 * @param index Galaxy index
 * @param component Component being validated
 * @return 0 if validation passed, non-zero otherwise
 */
static int validate_galaxy_consistency(struct validation_context *ctx,
                                     const struct GALAXY *galaxy,
                                     int index,
                                     const char *component) {
    int status = 0;
    
    // Check masses are non-negative
    if (galaxy->StellarMass < 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_CONSISTENCY, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has negative StellarMass = %g",
                                      index, galaxy->StellarMass);
    }
    
    if (galaxy->BulgeMass < 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_CONSISTENCY, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has negative BulgeMass = %g",
                                      index, galaxy->BulgeMass);
    }
    
    if (galaxy->HotGas < 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_CONSISTENCY, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has negative HotGas = %g",
                                      index, galaxy->HotGas);
    }
    
    if (galaxy->ColdGas < 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_CONSISTENCY, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has negative ColdGas = %g",
                                      index, galaxy->ColdGas);
    }
    
    // BulgeMass should not exceed StellarMass
    if (galaxy->BulgeMass > galaxy->StellarMass && galaxy->StellarMass > 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                       VALIDATION_SEVERITY_WARNING,
                                       VALIDATION_CHECK_CONSISTENCY, component,
                                       __FILE__, __LINE__,
                                       "Galaxy %d has BulgeMass (%g) > StellarMass (%g)",
                                       index, galaxy->BulgeMass, galaxy->StellarMass);
    }
    
    // More checks can be added here as needed...
    
    return status;
}

/**
 * @brief Validate each galaxy in an array
 * 
 * @param ctx Validation context
 * @param galaxies Galaxy array
 * @param count Number of galaxies
 * @param component Component being validated
 * @param check_type Type of validation check
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_galaxies(struct validation_context *ctx,
                            const struct GALAXY *galaxies,
                            int count,
                            const char *component,
                            enum validation_check_type check_type) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (galaxies == NULL) {
        return validation_add_result(ctx, VALIDATION_ERROR_NULL_POINTER,
                                   VALIDATION_SEVERITY_ERROR,
                                   check_type, component, __FILE__, __LINE__,
                                   "Galaxy array is NULL");
    }
    
    if (count <= 0) {
        return validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                   VALIDATION_SEVERITY_WARNING,
                                   check_type, component, __FILE__, __LINE__,
                                   "Galaxy count is %d, expected positive value", count);
    }
    
    int status = 0;
    int error_found = 0;
    
    // Validate each galaxy
    for (int i = 0; i < count; i++) {
        // Skip validation after too many errors
        if (ctx->error_count > 10 && ctx->strictness != VALIDATION_STRICTNESS_STRICT) {
            validation_add_result(ctx, VALIDATION_ERROR_RESOURCE_LIMIT,
                                VALIDATION_SEVERITY_WARNING,
                                check_type, component, __FILE__, __LINE__,
                                "Stopping validation after %d errors", ctx->error_count);
            error_found = 1;
            break;
        }
        
        const struct GALAXY *galaxy = &galaxies[i];
        
        // Validate values (NaN/Inf)
        if (check_type == VALIDATION_CHECK_GALAXY_DATA || 
            check_type == VALIDATION_CHECK_CONSISTENCY) {
            status |= validate_galaxy_values(ctx, galaxy, i, component);
        }
        
        // Validate references
        if (check_type == VALIDATION_CHECK_GALAXY_REFS || 
            check_type == VALIDATION_CHECK_CONSISTENCY) {
            status |= validate_galaxy_references(ctx, galaxy, i, count, component);
        }
        
        // Validate logical consistency
        if (check_type == VALIDATION_CHECK_CONSISTENCY) {
            status |= validate_galaxy_consistency(ctx, galaxy, i, component);
        }
    }
    
    // If any errors were found or status is non-zero, return non-zero
    return (ctx->error_count > 0 || error_found || status != 0) ? -1 : 0;
}
