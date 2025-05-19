#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "../core/core_logging.h"
#include "../core/core_galaxy_extensions.h"
#include "io_interface.h"
#include "io_property_serialization.h"

/**
 * @file io_validation.h
 * @brief Data validation framework for SAGE I/O operations
 *
 * This file defines a comprehensive validation system for ensuring data consistency
 * across different I/O formats. It provides utilities for validating galaxy data,
 * checking format capabilities, and verifying extended property compatibility.
 */

/**
 * @brief Validation error codes
 *
 * These codes identify specific validation failures.
 */
enum validation_error_code {
    VALIDATION_SUCCESS                  = 0,    /**< Validation passed */
    VALIDATION_ERROR_NULL_POINTER       = 1,    /**< NULL pointer encountered */
    VALIDATION_ERROR_ARRAY_BOUNDS       = 2,    /**< Array index out of bounds */
    VALIDATION_ERROR_INVALID_VALUE      = 3,    /**< Invalid value (e.g., NaN, Inf) */
    VALIDATION_ERROR_TYPE_MISMATCH      = 4,    /**< Type mismatch (e.g., wrong data type) */
    VALIDATION_ERROR_LOGICAL_CONSTRAINT = 5,    /**< Logical constraint violated */
    VALIDATION_ERROR_FORMAT_INCOMPATIBLE = 6,   /**< Format doesn't support required feature */
    VALIDATION_ERROR_PROPERTY_INCOMPATIBLE = 7, /**< Property incompatible with format */
    VALIDATION_ERROR_PROPERTY_MISSING   = 8,    /**< Required property is missing */
    VALIDATION_ERROR_REFERENCE_INVALID  = 9,    /**< Invalid reference (e.g., invalid galaxy ID) */
    VALIDATION_ERROR_DATA_INCONSISTENT  = 10,   /**< Data inconsistency detected */
    VALIDATION_ERROR_RESOURCE_LIMIT     = 11,   /**< Resource limit exceeded */
    VALIDATION_ERROR_INTERNAL           = 12,   /**< Internal validation error */
    VALIDATION_ERROR_UNKNOWN            = 13,   /**< Unknown validation error */
    VALIDATION_ERROR_NONE               = 14    /**< No error (used for warnings) */
};

/**
 * @brief Validation severity levels
 *
 * These levels determine how validation results are treated.
 */
enum validation_severity {
    VALIDATION_SEVERITY_INFO     = 0,    /**< Informational message */
    VALIDATION_SEVERITY_WARNING  = 1,    /**< Warning that might need attention */
    VALIDATION_SEVERITY_ERROR    = 2,    /**< Error that should be addressed */
    VALIDATION_SEVERITY_FATAL    = 3     /**< Fatal error that prevents operation */
};

/**
 * @brief Validation strictness levels
 *
 * These levels control how strict validation is applied.
 */
enum validation_strictness {
    VALIDATION_STRICTNESS_RELAXED = 0,   /**< Skip non-critical checks, warnings ignored */
    VALIDATION_STRICTNESS_NORMAL  = 1,   /**< Perform all checks, warnings logged */
    VALIDATION_STRICTNESS_STRICT  = 2    /**< Perform all checks, warnings treated as errors */
};

/**
 * @brief Validation check types
 *
 * These types categorize different kinds of validation checks.
 */
enum validation_check_type {
    VALIDATION_CHECK_GALAXY_DATA  = 0,   /**< Galaxy property data validation */
    VALIDATION_CHECK_GALAXY_REFS  = 1,   /**< Galaxy reference validation */
    VALIDATION_CHECK_FORMAT_CAPS  = 2,   /**< Format capability validation */
    VALIDATION_CHECK_PROPERTY_COMPAT = 3,/**< Property compatibility validation */
    VALIDATION_CHECK_IO_PARAMS    = 4,   /**< I/O parameter validation */
    VALIDATION_CHECK_RESOURCE     = 5,   /**< Resource validation */
    VALIDATION_CHECK_CONSISTENCY  = 6    /**< Logical consistency validation */
};

/**
 * @brief Maximum length of validation component name
 */
#define MAX_VALIDATION_COMPONENT 32

/**
 * @brief Maximum length of validation message
 */
#define MAX_VALIDATION_MESSAGE 256

/**
 * @brief Maximum number of validation results to store
 */
#define MAX_VALIDATION_RESULTS 64

/**
 * @brief Validation result structure
 *
 * Contains information about a single validation result.
 */
struct validation_result {
    enum validation_error_code code;                 /**< Error code */
    enum validation_severity severity;               /**< Severity level */
    enum validation_check_type check_type;           /**< Type of validation check */
    char component[MAX_VALIDATION_COMPONENT];        /**< Component being validated */
    char message[MAX_VALIDATION_MESSAGE];            /**< Detailed message */
    const char *file;                                /**< Source file */
    int line;                                        /**< Source line */
    void *context;                                   /**< Context pointer (format-specific) */
};

/**
 * @brief Validation context structure
 *
 * Contains the state and configuration for a validation session.
 */
struct validation_context {
    enum validation_strictness strictness;           /**< Strictness level */
    struct validation_result results[MAX_VALIDATION_RESULTS]; /**< Result array */
    int num_results;                                 /**< Number of collected results */
    int max_results;                                 /**< Maximum results to collect */
    int error_count;                                 /**< Number of errors */
    int warning_count;                               /**< Number of warnings */
    bool abort_on_first_error;                       /**< Whether to abort on first error */
    void *custom_context;                            /**< Custom context pointer */
};

/**
 * @brief Initialize a validation context
 *
 * Sets up a validation context with the specified strictness level.
 *
 * @param ctx Validation context to initialize
 * @param strictness Strictness level
 * @return 0 on success, non-zero on failure
 */
extern int validation_init(struct validation_context *ctx, 
                           enum validation_strictness strictness);

/**
 * @brief Clean up a validation context
 *
 * Releases resources associated with a validation context.
 *
 * @param ctx Validation context to clean up
 */
extern void validation_cleanup(struct validation_context *ctx);

/**
 * @brief Reset a validation context
 *
 * Clears all results but keeps the configuration.
 *
 * @param ctx Validation context to reset
 */
extern void validation_reset(struct validation_context *ctx);

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
extern void validation_configure(struct validation_context *ctx,
                                int strictness,
                                int max_results,
                                int abort_on_first_error);

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
extern int validation_add_result(struct validation_context *ctx,
                                enum validation_error_code code,
                                enum validation_severity severity,
                                enum validation_check_type check_type,
                                const char *component,
                                const char *file,
                                int line,
                                const char *message_format,
                                ...);

/**
 * @brief Check if validation passed
 *
 * @param ctx Validation context
 * @return true if validation passed, false otherwise
 */
extern bool validation_passed(const struct validation_context *ctx);

/**
 * @brief Report validation results
 *
 * Logs validation results with appropriate severity.
 *
 * @param ctx Validation context
 * @return Number of validation errors
 */
extern int validation_report(const struct validation_context *ctx);

/**
 * @brief Get validation result count
 *
 * @param ctx Validation context
 * @return Total number of validation results
 */
extern int validation_get_result_count(const struct validation_context *ctx);

/**
 * @brief Get validation error count
 *
 * @param ctx Validation context
 * @return Number of validation errors
 */
extern int validation_get_error_count(const struct validation_context *ctx);

/**
 * @brief Get validation warning count
 *
 * @param ctx Validation context
 * @return Number of validation warnings
 */
extern int validation_get_warning_count(const struct validation_context *ctx);

/**
 * @brief Check if validation has errors
 *
 * @param ctx Validation context
 * @return true if validation has errors, false otherwise
 */
extern bool validation_has_errors(const struct validation_context *ctx);

/**
 * @brief Check if validation has warnings
 *
 * @param ctx Validation context
 * @return true if validation has warnings, false otherwise
 */
extern bool validation_has_warnings(const struct validation_context *ctx);

/**
 * @brief Map validation error to I/O error
 *
 * Converts a validation error to an I/O error.
 *
 * @param validation_error Validation error code
 * @return Corresponding I/O error code
 */
extern int validation_map_to_io_error(enum validation_error_code validation_error);

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
extern int validation_check_not_null(struct validation_context *ctx,
                                   const void *ptr,
                                   const char *component,
                                   const char *file,
                                   int line,
                                   const char *message_format,
                                   ...);

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
extern int validation_check_finite(struct validation_context *ctx,
                                 double value,
                                 const char *component,
                                 const char *file,
                                 int line,
                                 const char *message_format,
                                 ...);

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
extern int validation_check_bounds(struct validation_context *ctx,
                                 int64_t index,
                                 int64_t min_value,
                                 int64_t max_value,
                                 const char *component,
                                 const char *file,
                                 int line,
                                 const char *message_format,
                                 ...);

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
extern int validation_check_condition(struct validation_context *ctx,
                                    bool condition,
                                    enum validation_severity severity,
                                    enum validation_error_code code,
                                    enum validation_check_type check_type,
                                    const char *component,
                                    const char *file,
                                    int line,
                                    const char *message_format,
                                    ...);

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
extern int validation_check_capability(struct validation_context *ctx,
                                     const struct io_interface *handler,
                                     enum io_capabilities capability,
                                     const char *component,
                                     const char *file,
                                     int line,
                                     const char *message_format,
                                     ...);

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
extern int validation_check_galaxies(struct validation_context *ctx,
                                   const struct GALAXY *galaxies,
                                   int count,
                                   const char *component,
                                   enum validation_check_type check_type);

/**
 * @brief Validate format capabilities for a specific operation
 *
 * Checks if a handler supports all the capabilities required for an operation.
 *
 * @param ctx Validation context
 * @param handler I/O interface handler
 * @param required_caps Array of required capabilities
 * @param num_caps Number of capabilities in the array
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param operation_name Name of the operation being validated
 * @return 0 if validation passed, non-zero otherwise
 */
extern int validation_check_format_capabilities(struct validation_context *ctx,
                                             const struct io_interface *handler,
                                             enum io_capabilities *required_caps,
                                             int num_caps,
                                             const char *component,
                                             const char *file,
                                             int line,
                                             const char *operation_name);

/* Binary format validation has been removed as part of standardization on HDF5 */

/**
 * @brief Validate HDF5 format compatibility
 *
 * Checks if a handler is compatible with HDF5 format requirements.
 *
 * @param ctx Validation context
 * @param handler I/O interface handler
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @return 0 if validation passed, non-zero otherwise
 */
extern int validation_check_hdf5_compatibility(struct validation_context *ctx,
                                            const struct io_interface *handler,
                                            const char *component,
                                            const char *file,
                                            int line);

/**
 * @brief Validate property type compatibility
 *
 * Checks if a property type is compatible with serialization.
 *
 * @param ctx Validation context
 * @param type Property type to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @param property_name Name of the property
 * @return 0 if validation passed, non-zero otherwise
 */
extern int validation_check_property_type(struct validation_context *ctx,
                                       enum galaxy_property_type type,
                                       const char *component,
                                       const char *file,
                                       int line,
                                       const char *property_name);

/**
 * @brief Validate property serialization functions
 *
 * Checks if a property has valid serialization functions.
 *
 * @param ctx Validation context
 * @param property Property to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @return 0 if validation passed, non-zero otherwise
 */
extern int validation_check_property_serialization(struct validation_context *ctx,
                                                const galaxy_property_t *property,
                                                const char *component,
                                                const char *file,
                                                int line);

/**
 * @brief Validate property name uniqueness
 *
 * Checks if a property name is unique among registered properties.
 *
 * @param ctx Validation context
 * @param property Property to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @return 0 if validation passed, non-zero otherwise
 */
extern int validation_check_property_uniqueness(struct validation_context *ctx,
                                            const galaxy_property_t *property,
                                            const char *component,
                                            const char *file,
                                            int line);

/**
 * @brief Validate serialization context
 *
 * Checks if a property serialization context is valid.
 *
 * @param ctx Validation context
 * @param ser_ctx Serialization context to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @return 0 if validation passed, non-zero otherwise
 */
extern int validation_check_serialization_context(struct validation_context *ctx,
                                              const struct property_serialization_context *ser_ctx,
                                              const char *component,
                                              const char *file,
                                              int line);

/* Binary property compatibility validation has been removed as part of standardization on HDF5 */

/**
 * @brief Validate HDF5 property compatibility
 *
 * Checks if a property is compatible with HDF5 serialization.
 *
 * @param ctx Validation context
 * @param property Property to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @return 0 if validation passed, non-zero otherwise
 */
extern int validation_check_hdf5_property_compatibility(struct validation_context *ctx,
                                                    const galaxy_property_t *property,
                                                    const char *component,
                                                    const char *file,
                                                    int line);

// Convenience macros for validation

/**
 * @brief Validate that a condition is true (with error severity)
 */
#define VALIDATE_CONDITION(ctx, condition, component, format, ...) \
    validation_check_condition(ctx, condition, VALIDATION_SEVERITY_ERROR, \
                             VALIDATION_ERROR_LOGICAL_CONSTRAINT, \
                             VALIDATION_CHECK_CONSISTENCY, \
                             component, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Validate that a pointer is not NULL
 */
#define VALIDATE_NOT_NULL(ctx, ptr, component, format, ...) \
    validation_check_not_null(ctx, ptr, component, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Validate that a value is not NaN or infinity
 */
#define VALIDATE_FINITE(ctx, value, component, format, ...) \
    validation_check_finite(ctx, value, component, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Validate that an index is within bounds
 */
#define VALIDATE_BOUNDS(ctx, index, min, max, component, format, ...) \
    validation_check_bounds(ctx, index, min, max, component, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Validate that a format supports a capability
 */
#define VALIDATE_CAPABILITY(ctx, handler, capability, component, format, ...) \
    validation_check_capability(ctx, handler, capability, component, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Validate format capabilities for an operation
 */
#define VALIDATE_FORMAT_CAPABILITIES(ctx, handler, caps, num_caps, component, operation) \
    validation_check_format_capabilities(ctx, handler, caps, num_caps, component, __FILE__, __LINE__, operation)

/* Binary compatibility validation has been removed as part of standardization on HDF5 */

/**
 * @brief Validate HDF5 format compatibility
 */
#define VALIDATE_HDF5_COMPATIBILITY(ctx, handler, component) \
    validation_check_hdf5_compatibility(ctx, handler, component, __FILE__, __LINE__)

/**
 * @brief Validate property type
 */
#define VALIDATE_PROPERTY_TYPE(ctx, type, component, property_name) \
    validation_check_property_type(ctx, type, component, __FILE__, __LINE__, property_name)

/**
 * @brief Validate property serialization
 */
#define VALIDATE_PROPERTY_SERIALIZATION(ctx, property, component) \
    validation_check_property_serialization(ctx, property, component, __FILE__, __LINE__)

/**
 * @brief Validate property uniqueness
 */
#define VALIDATE_PROPERTY_UNIQUENESS(ctx, property, component) \
    validation_check_property_uniqueness(ctx, property, component, __FILE__, __LINE__)

/**
 * @brief Validate serialization context
 */
#define VALIDATE_SERIALIZATION_CONTEXT(ctx, ser_ctx, component) \
    validation_check_serialization_context(ctx, ser_ctx, component, __FILE__, __LINE__)

/* Binary property compatibility validation has been removed as part of standardization on HDF5 */

/**
 * @brief Validate HDF5 property compatibility
 */
#define VALIDATE_HDF5_PROPERTY_COMPATIBILITY(ctx, property, component) \
    validation_check_hdf5_property_compatibility(ctx, property, component, __FILE__, __LINE__)

/**
 * @brief Add a warning result
 */
#define VALIDATION_WARN(ctx, code, check_type, component, format, ...) \
    validation_add_result(ctx, code, VALIDATION_SEVERITY_WARNING, check_type, \
                        component, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Add an error result
 */
#define VALIDATION_ERROR(ctx, code, check_type, component, format, ...) \
    validation_add_result(ctx, code, VALIDATION_SEVERITY_ERROR, check_type, \
                        component, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Add a fatal result
 */
#define VALIDATION_FATAL(ctx, code, check_type, component, format, ...) \
    validation_add_result(ctx, code, VALIDATION_SEVERITY_FATAL, check_type, \
                        component, __FILE__, __LINE__, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif