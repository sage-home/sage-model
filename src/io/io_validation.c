#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

#include "io_validation.h"
#include "io_interface.h"
#include "io_property_serialization.h"
#include "../core/core_logging.h"
#include "../core/core_galaxy_extensions.h"

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
int validation_check_property_type(struct validation_context *ctx,
                                enum galaxy_property_type type,
                                const char *component,
                                const char *file,
                                int line,
                                const char *property_name) {
    // Mark unused parameters to avoid compiler warnings
    (void)file;
    (void)line;
    
    if (ctx == NULL) {
        return -1;
    }
    
    // Check if type is valid
    if (type < 0 || type >= PROPERTY_TYPE_MAX) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Property '%s' has invalid type: %d", 
                      property_name, type);
        return -1;
    }
    
    // Check if type is serializable
    switch (type) {
        case PROPERTY_TYPE_FLOAT:
        case PROPERTY_TYPE_DOUBLE:
        case PROPERTY_TYPE_INT32:
        case PROPERTY_TYPE_INT64:
        case PROPERTY_TYPE_UINT32:
        case PROPERTY_TYPE_UINT64:
        case PROPERTY_TYPE_BOOL:
            // Basic types are supported
            return 0;
            
        case PROPERTY_TYPE_STRUCT:
            // Structs need custom serializers
            VALIDATION_WARN(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                         component, "Property '%s' is a struct and requires custom serializers",
                         property_name);
            return 0;
            
        case PROPERTY_TYPE_ARRAY:
            // Arrays need special handling
            VALIDATION_WARN(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                         component, "Property '%s' is an array and requires careful serialization",
                         property_name);
            return 0;
            
        default:
            VALIDATION_ERROR(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Property '%s' has unsupported type: %d",
                          property_name, type);
            return -1;
    }
}

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
int validation_check_property_serialization(struct validation_context *ctx,
                                         const galaxy_property_t *property,
                                         const char *component,
                                         const char *file,
                                         int line) {
    // Mark unused parameters to avoid compiler warnings
    (void)file;
    (void)line;
    
    if (ctx == NULL || property == NULL) {
        return -1;
    }
    
    // Skip check if property doesn't need serialization
    if (!(property->flags & PROPERTY_FLAG_SERIALIZE)) {
        return 0;
    }
    
    // Check both serialization functions exist
    if (property->serialize == NULL) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_PROPERTY_INCOMPATIBLE, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Property '%s' is marked for serialization but has no serialize function",
                      property->name);
        return -1;
    }
    
    if (property->deserialize == NULL) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_PROPERTY_INCOMPATIBLE, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Property '%s' is marked for serialization but has no deserialize function",
                      property->name);
        return -1;
    }
    
    // For basic types, check if they have a default serializer
    void (*default_serializer)(const void *, void *, int) = 
        property_serialization_get_default_serializer(property->type);
    
    void (*default_deserializer)(const void *, void *, int) = 
        property_serialization_get_default_deserializer(property->type);
    
    // If defaults exist but custom functions are used, issue a warning
    if (default_serializer != NULL && property->serialize != default_serializer) {
        VALIDATION_WARN(ctx, VALIDATION_ERROR_NONE, VALIDATION_CHECK_PROPERTY_COMPAT,
                     component, "Property '%s' uses a custom serializer instead of the default",
                     property->name);
    }
    
    if (default_deserializer != NULL && property->deserialize != default_deserializer) {
        VALIDATION_WARN(ctx, VALIDATION_ERROR_NONE, VALIDATION_CHECK_PROPERTY_COMPAT,
                     component, "Property '%s' uses a custom deserializer instead of the default",
                     property->name);
    }
    
    return 0;
}

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
int validation_check_property_uniqueness(struct validation_context *ctx,
                                      const galaxy_property_t *property,
                                      const char *component,
                                      const char *file,
                                      int line) {
    // Mark unused parameters to avoid compiler warnings
    (void)file;
    (void)line;
    
    if (ctx == NULL || property == NULL) {
        return -1;
    }
    
    // Check if we have a registry
    if (global_extension_registry == NULL) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_INTERNAL, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Extension registry not initialized");
        return -1;
    }
    
    // Check for duplicate name
    for (int i = 0; i < global_extension_registry->num_extensions; i++) {
        // Skip the property itself
        if (global_extension_registry->extensions[i].extension_id == property->extension_id) {
            continue;
        }
        
        // Check for name collision
        if (strcmp(global_extension_registry->extensions[i].name, property->name) == 0) {
            VALIDATION_ERROR(ctx, VALIDATION_ERROR_PROPERTY_INCOMPATIBLE, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Property name '%s' is not unique (extension_id %d and %d)",
                          property->name, property->extension_id, 
                          global_extension_registry->extensions[i].extension_id);
            return -1;
        }
    }
    
    return 0;
}

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
int validation_check_serialization_context(struct validation_context *ctx,
                                        const struct property_serialization_context *ser_ctx,
                                        const char *component,
                                        const char *file,
                                        int line) {
    // Mark unused parameters to avoid compiler warnings
    (void)file;
    (void)line;
    
    if (ctx == NULL || ser_ctx == NULL) {
        return -1;
    }
    
    // Check basic validity
    if (ser_ctx->version != PROPERTY_SERIALIZATION_VERSION) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Serialization context has incorrect version (got %d, expected %d)",
                      ser_ctx->version, PROPERTY_SERIALIZATION_VERSION);
        return -1;
    }
    
    // Check property count
    if (ser_ctx->num_properties < 0) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Serialization context has invalid property count: %d",
                      ser_ctx->num_properties);
        return -1;
    }
    
    // If we have properties, validate property metadata array and ID map
    if (ser_ctx->num_properties > 0) {
        if (ser_ctx->properties == NULL) {
            VALIDATION_ERROR(ctx, VALIDATION_ERROR_NULL_POINTER, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Serialization context has %d properties but NULL properties array",
                          ser_ctx->num_properties);
            return -1;
        }
        
        if (ser_ctx->property_id_map == NULL) {
            VALIDATION_ERROR(ctx, VALIDATION_ERROR_NULL_POINTER, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Serialization context has %d properties but NULL property ID map",
                          ser_ctx->num_properties);
            return -1;
        }
        
        // Check total size per galaxy
        if (ser_ctx->total_size_per_galaxy <= 0) {
            VALIDATION_ERROR(ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Serialization context has invalid total size per galaxy: %zu",
                          ser_ctx->total_size_per_galaxy);
            return -1;
        }
        
        // Validate individual properties
        for (int i = 0; i < ser_ctx->num_properties; i++) {
            const struct serialized_property_meta *property = &ser_ctx->properties[i];
            
            // Check name is non-empty
            if (property->name[0] == '\0') {
                VALIDATION_ERROR(ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_CHECK_PROPERTY_COMPAT,
                              component, "Property at index %d has empty name", i);
                return -1;
            }
            
            // Check size is valid
            if (property->size == 0) {
                VALIDATION_ERROR(ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_CHECK_PROPERTY_COMPAT,
                              component, "Property '%s' has zero size", property->name);
                return -1;
            }
            
            // Check type is valid
            if (property->type < 0 || property->type >= PROPERTY_TYPE_MAX) {
                VALIDATION_ERROR(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                              component, "Property '%s' has invalid type: %d", 
                              property->name, property->type);
                return -1;
            }
            
            // Check offset is within bounds
            if (property->offset < 0 || 
                property->offset + property->size > ser_ctx->total_size_per_galaxy) {
                VALIDATION_ERROR(ctx, VALIDATION_ERROR_ARRAY_BOUNDS, VALIDATION_CHECK_PROPERTY_COMPAT,
                              component, "Property '%s' has invalid offset or size (offset: %lld, size: %zu, total: %zu)",
                              property->name, (long long)property->offset, property->size, 
                              ser_ctx->total_size_per_galaxy);
                return -1;
            }
        }
        
        // Validate property ID map
        for (int i = 0; i < ser_ctx->num_properties; i++) {
            int ext_id = ser_ctx->property_id_map[i];
            
            // Check ID exists
            if (global_extension_registry == NULL) {
                VALIDATION_ERROR(ctx, VALIDATION_ERROR_INTERNAL, VALIDATION_CHECK_PROPERTY_COMPAT,
                              component, "Extension registry not initialized when validating property ID map");
                return -1;
            }
            
            if (ext_id < 0 || ext_id >= global_extension_registry->num_extensions) {
                VALIDATION_ERROR(ctx, VALIDATION_ERROR_REFERENCE_INVALID, VALIDATION_CHECK_PROPERTY_COMPAT,
                              component, "Property ID map contains invalid extension ID: %d", ext_id);
                return -1;
            }
        }
    }
    
    return 0;
}

/**
 * @brief Validate binary property compatibility
 *
 * Checks if a property is compatible with binary serialization.
 *
 * @param ctx Validation context
 * @param property Property to check
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_binary_property_compatibility(struct validation_context *ctx,
                                               const galaxy_property_t *property,
                                               const char *component,
                                               const char *file,
                                               int line) {
    // Mark unused parameters to avoid compiler warnings
    (void)file;
    (void)line;
    
    if (ctx == NULL || property == NULL) {
        return -1;
    }
    
    // Skip check if property doesn't need serialization
    if (!(property->flags & PROPERTY_FLAG_SERIALIZE)) {
        return 0;
    }
    
    // Check for serialization functions
    if (property->serialize == NULL || property->deserialize == NULL) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_PROPERTY_INCOMPATIBLE, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Property '%s' lacks required serialization functions for binary format",
                      property->name);
        return -1;
    }
    
    // Binary format has fewer restrictions, but warn for complex types
    if (property->type == PROPERTY_TYPE_STRUCT) {
        VALIDATION_WARN(ctx, VALIDATION_ERROR_NONE, VALIDATION_CHECK_PROPERTY_COMPAT,
                     component, "Property '%s' is a struct, ensure proper binary serialization",
                     property->name);
    } else if (property->type == PROPERTY_TYPE_ARRAY) {
        VALIDATION_WARN(ctx, VALIDATION_ERROR_NONE, VALIDATION_CHECK_PROPERTY_COMPAT,
                     component, "Property '%s' is an array, ensure proper binary serialization",
                     property->name);
    }
    
    // Validate size is reasonable
    if (property->size > MAX_SERIALIZED_ARRAY_SIZE) {
        VALIDATION_WARN(ctx, VALIDATION_ERROR_RESOURCE_LIMIT, VALIDATION_CHECK_PROPERTY_COMPAT,
                     component, "Property '%s' size (%zu) exceeds recommended maximum (%d)",
                     property->name, property->size, MAX_SERIALIZED_ARRAY_SIZE);
    }
    
    return 0;
}

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
int validation_check_hdf5_property_compatibility(struct validation_context *ctx,
                                             const galaxy_property_t *property,
                                             const char *component,
                                             const char *file,
                                             int line) {
    // Mark unused parameters to avoid compiler warnings
    (void)file;
    (void)line;
    
    if (ctx == NULL || property == NULL) {
        return -1;
    }
    
    // Skip check if property doesn't need serialization
    if (!(property->flags & PROPERTY_FLAG_SERIALIZE)) {
        return 0;
    }
    
    // Check for serialization functions
    if (property->serialize == NULL || property->deserialize == NULL) {
        VALIDATION_ERROR(ctx, VALIDATION_ERROR_PROPERTY_INCOMPATIBLE, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Property '%s' lacks required serialization functions for HDF5 format",
                      property->name);
        return -1;
    }
    
    // HDF5 requires proper type mapping
    switch (property->type) {
        case PROPERTY_TYPE_FLOAT:
        case PROPERTY_TYPE_DOUBLE:
        case PROPERTY_TYPE_INT32:
        case PROPERTY_TYPE_INT64:
        case PROPERTY_TYPE_UINT32:
        case PROPERTY_TYPE_UINT64:
        case PROPERTY_TYPE_BOOL:
            // Basic types are supported
            break;
            
        case PROPERTY_TYPE_STRUCT:
            VALIDATION_WARN(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Property '%s' is a struct which requires compound datatype in HDF5",
                          property->name);
            break;
            
        case PROPERTY_TYPE_ARRAY:
            VALIDATION_WARN(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Property '%s' is an array which requires special handling in HDF5",
                          property->name);
            break;
            
        default:
            VALIDATION_ERROR(ctx, VALIDATION_ERROR_TYPE_MISMATCH, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Property '%s' has unsupported type for HDF5: %d",
                          property->name, property->type);
            return -1;
    }
    
    // Validate size for HDF5
    if (property->size > MAX_SERIALIZED_ARRAY_SIZE) {
        VALIDATION_WARN(ctx, VALIDATION_ERROR_RESOURCE_LIMIT, VALIDATION_CHECK_PROPERTY_COMPAT,
                      component, "Property '%s' size (%zu) exceeds maximum for HDF5 (%d)",
                      property->name, property->size, MAX_SERIALIZED_ARRAY_SIZE);
    }
    
    // Validate property name for HDF5 (no special characters)
    for (const char *c = property->name; *c != '\0'; c++) {
        if (!isalnum(*c) && *c != '_' && *c != '-') {
            VALIDATION_WARN(ctx, VALIDATION_ERROR_INVALID_VALUE, VALIDATION_CHECK_PROPERTY_COMPAT,
                          component, "Property name '%s' contains characters not allowed in HDF5 attributes",
                          property->name);
            break;
        }
    }
    
    return 0;
}

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
            return LOG_LEVEL_CRITICAL;
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
    
    // Check CentralGal is valid
    if (galaxy->CentralGal >= count) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_REFERENCE_INVALID,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_GALAXY_REFS, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has invalid CentralGal = %d (max valid value: %d)",
                                      index, galaxy->CentralGal, count - 1);
    }
    
    // Check Type is valid (0=central, 1=satellite, 2=orphan)
    if (galaxy->Type < 0 || galaxy->Type > 2) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_REFERENCE_INVALID,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_GALAXY_REFS, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has invalid Type = %d (valid values: 0, 1, 2)",
                                      index, galaxy->Type);
    }
    
    // Check GalaxyNr is valid (should be >= 0)
    if (galaxy->GalaxyNr < 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_REFERENCE_INVALID,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_GALAXY_REFS, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has invalid GalaxyNr = %d (should be >= 0)",
                                      index, galaxy->GalaxyNr);
    }
    
    // Check mergeType is valid (0-4)
    if (galaxy->mergeType < 0 || galaxy->mergeType > 4) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_REFERENCE_INVALID,
                                      VALIDATION_SEVERITY_ERROR,
                                      VALIDATION_CHECK_GALAXY_REFS, component,
                                      __FILE__, __LINE__,
                                      "Galaxy %d has invalid mergeType = %d (valid range: 0-4)",
                                      index, galaxy->mergeType);
    }
    
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
    
    // Check main mass reservoirs for NaN/Inf
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
    status |= validation_check_finite(ctx, galaxy->EjectedMass, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid EjectedMass", index);
    status |= validation_check_finite(ctx, galaxy->BlackHoleMass, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid BlackHoleMass", index);
    status |= validation_check_finite(ctx, galaxy->ICS, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid ICS", index);
    
    // Check metals for NaN/Inf
    status |= validation_check_finite(ctx, galaxy->MetalsStellarMass, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid MetalsStellarMass", index);
    status |= validation_check_finite(ctx, galaxy->MetalsBulgeMass, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid MetalsBulgeMass", index);
    status |= validation_check_finite(ctx, galaxy->MetalsHotGas, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid MetalsHotGas", index);
    status |= validation_check_finite(ctx, galaxy->MetalsColdGas, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid MetalsColdGas", index);
    status |= validation_check_finite(ctx, galaxy->MetalsEjectedMass, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid MetalsEjectedMass", index);
    status |= validation_check_finite(ctx, galaxy->MetalsICS, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid MetalsICS", index);
    
    // Check halo/subhalo properties
    status |= validation_check_finite(ctx, galaxy->Mvir, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid Mvir", index);
    status |= validation_check_finite(ctx, galaxy->Rvir, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid Rvir", index);
    status |= validation_check_finite(ctx, galaxy->Vvir, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid Vvir", index);
    status |= validation_check_finite(ctx, galaxy->Vmax, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid Vmax", index);
    
    // Check position components
    for (int i = 0; i < 3; i++) {
        status |= validation_check_finite(ctx, galaxy->Pos[i], component, 
                                        __FILE__, __LINE__,
                                        "Galaxy %d has invalid Pos[%d]", index, i);
    }
    
    // Check velocity components
    for (int i = 0; i < 3; i++) {
        status |= validation_check_finite(ctx, galaxy->Vel[i], component, 
                                        __FILE__, __LINE__,
                                        "Galaxy %d has invalid Vel[%d]", index, i);
    }
    
    // Check disk properties
    status |= validation_check_finite(ctx, galaxy->DiskScaleRadius, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid DiskScaleRadius", index);
    
    // Check dynamical properties
    status |= validation_check_finite(ctx, galaxy->Cooling, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid Cooling", index);
    status |= validation_check_finite(ctx, galaxy->Heating, component, 
                                    __FILE__, __LINE__,
                                    "Galaxy %d has invalid Heating", index);
    
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
    
    // Note: We removed strict checking for negative values as per the human's instructions,
    // since sometimes a mass might be set to a negative value as a flag for a future condition.
    // We'll only check StellarMass, BulgeMass, etc. if they are NaN/Infinity in validate_galaxy_values.
    
    // BulgeMass should not exceed StellarMass (but only warn about it)
    if (galaxy->BulgeMass > galaxy->StellarMass && galaxy->StellarMass > 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                       VALIDATION_SEVERITY_WARNING,
                                       VALIDATION_CHECK_CONSISTENCY, component,
                                       __FILE__, __LINE__,
                                       "Galaxy %d has BulgeMass (%g) > StellarMass (%g)",
                                       index, galaxy->BulgeMass, galaxy->StellarMass);
    }
    
    // Warn about metals exceeding total mass (only if both are positive)
    if (galaxy->MetalsStellarMass > galaxy->StellarMass && 
        galaxy->StellarMass > 0 && galaxy->MetalsStellarMass > 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                       VALIDATION_SEVERITY_WARNING,
                                       VALIDATION_CHECK_CONSISTENCY, component,
                                       __FILE__, __LINE__,
                                       "Galaxy %d has MetalsStellarMass (%g) > StellarMass (%g)",
                                       index, galaxy->MetalsStellarMass, galaxy->StellarMass);
    }
    
    if (galaxy->MetalsBulgeMass > galaxy->BulgeMass && 
        galaxy->BulgeMass > 0 && galaxy->MetalsBulgeMass > 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                       VALIDATION_SEVERITY_WARNING,
                                       VALIDATION_CHECK_CONSISTENCY, component,
                                       __FILE__, __LINE__,
                                       "Galaxy %d has MetalsBulgeMass (%g) > BulgeMass (%g)",
                                       index, galaxy->MetalsBulgeMass, galaxy->BulgeMass);
    }
    
    if (galaxy->MetalsHotGas > galaxy->HotGas && 
        galaxy->HotGas > 0 && galaxy->MetalsHotGas > 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                       VALIDATION_SEVERITY_WARNING,
                                       VALIDATION_CHECK_CONSISTENCY, component,
                                       __FILE__, __LINE__,
                                       "Galaxy %d has MetalsHotGas (%g) > HotGas (%g)",
                                       index, galaxy->MetalsHotGas, galaxy->HotGas);
    }
    
    if (galaxy->MetalsColdGas > galaxy->ColdGas && 
        galaxy->ColdGas > 0 && galaxy->MetalsColdGas > 0) {
        status |= validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                       VALIDATION_SEVERITY_WARNING,
                                       VALIDATION_CHECK_CONSISTENCY, component,
                                       __FILE__, __LINE__,
                                       "Galaxy %d has MetalsColdGas (%g) > ColdGas (%g)",
                                       index, galaxy->MetalsColdGas, galaxy->ColdGas);
    }
    
    // Check physical consistency of velocities and positions
    // Note: We don't check for position values to be within the simulation box
    // since these values come directly from the input treefiles and are assumed
    // to be correct, as per the human's instructions
    
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
int validation_check_format_capabilities(struct validation_context *ctx,
                                       const struct io_interface *handler,
                                       enum io_capabilities *required_caps,
                                       int num_caps,
                                       const char *component,
                                       const char *file,
                                       int line,
                                       const char *operation_name) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (handler == NULL) {
        return validation_add_result(ctx, VALIDATION_ERROR_NULL_POINTER,
                                   VALIDATION_SEVERITY_ERROR,
                                   VALIDATION_CHECK_FORMAT_CAPS,
                                   component, file, line,
                                   "I/O handler is NULL");
    }
    
    if (required_caps == NULL || num_caps <= 0) {
        return validation_add_result(ctx, VALIDATION_ERROR_LOGICAL_CONSTRAINT,
                                   VALIDATION_SEVERITY_ERROR,
                                   VALIDATION_CHECK_FORMAT_CAPS,
                                   component, file, line,
                                   "Invalid required capabilities array");
    }
    
    int status = 0;
    
    // Check each required capability
    for (int i = 0; i < num_caps; i++) {
        enum io_capabilities cap = required_caps[i];
        if (!io_has_capability((struct io_interface *)handler, cap)) {
            char cap_name[64];
            switch (cap) {
                case IO_CAP_RANDOM_ACCESS:
                    strcpy(cap_name, "random access");
                    break;
                case IO_CAP_MULTI_FILE:
                    strcpy(cap_name, "multi-file support");
                    break;
                case IO_CAP_METADATA_QUERY:
                    strcpy(cap_name, "metadata queries");
                    break;
                case IO_CAP_PARALLEL_READ:
                    strcpy(cap_name, "parallel reading");
                    break;
                case IO_CAP_COMPRESSION:
                    strcpy(cap_name, "compression");
                    break;
                case IO_CAP_EXTENDED_PROPS:
                    strcpy(cap_name, "extended properties");
                    break;
                case IO_CAP_APPEND:
                    strcpy(cap_name, "append operations");
                    break;
                case IO_CAP_CHUNKED_WRITE:
                    strcpy(cap_name, "chunked writing");
                    break;
                case IO_CAP_METADATA_ATTRS:
                    strcpy(cap_name, "metadata attributes");
                    break;
                default:
                    snprintf(cap_name, sizeof(cap_name), "capability %d", cap);
                    break;
            }
            
            status |= validation_add_result(ctx, VALIDATION_ERROR_FORMAT_INCOMPATIBLE,
                                         VALIDATION_SEVERITY_ERROR,
                                         VALIDATION_CHECK_FORMAT_CAPS,
                                         component, file, line,
                                         "Format '%s' does not support %s required for '%s' operation",
                                         handler->name, cap_name, operation_name);
        }
    }
    
    return status;
}

/**
 * @brief Validate binary format compatibility
 *
 * Checks if a handler is compatible with binary format requirements.
 *
 * @param ctx Validation context
 * @param handler I/O interface handler
 * @param component Component being validated
 * @param file Source file
 * @param line Source line
 * @return 0 if validation passed, non-zero otherwise
 */
int validation_check_binary_compatibility(struct validation_context *ctx,
                                        const struct io_interface *handler,
                                        const char *component,
                                        const char *file,
                                        int line) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (handler == NULL) {
        return validation_add_result(ctx, VALIDATION_ERROR_NULL_POINTER,
                                   VALIDATION_SEVERITY_ERROR,
                                   VALIDATION_CHECK_FORMAT_CAPS,
                                   component, file, line,
                                   "I/O handler is NULL");
    }
    
    int status = 0;
    
    // Check binary format specific requirements
    if (handler->format_id == IO_FORMAT_LHALO_BINARY || 
        handler->format_id == IO_FORMAT_BINARY_OUTPUT) {
        
        // For binary formats, extended properties require specific capability
        if (io_has_capability((struct io_interface *)handler, IO_CAP_EXTENDED_PROPS)) {
            // This is good, format claims to support extended properties
        } else {
            // Binary format might have issues with extended properties
            status |= validation_add_result(ctx, VALIDATION_ERROR_PROPERTY_INCOMPATIBLE,
                                         VALIDATION_SEVERITY_WARNING,
                                         VALIDATION_CHECK_PROPERTY_COMPAT,
                                         component, file, line,
                                         "Binary format '%s' may have limited support for extended properties",
                                         handler->name);
        }
    } else {
        // Not a binary format
        status |= validation_add_result(ctx, VALIDATION_ERROR_FORMAT_INCOMPATIBLE,
                                     VALIDATION_SEVERITY_ERROR,
                                     VALIDATION_CHECK_FORMAT_CAPS,
                                     component, file, line,
                                     "Format '%s' is not a binary format",
                                     handler->name);
    }
    
    return status;
}

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
int validation_check_hdf5_compatibility(struct validation_context *ctx,
                                      const struct io_interface *handler,
                                      const char *component,
                                      const char *file,
                                      int line) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (handler == NULL) {
        return validation_add_result(ctx, VALIDATION_ERROR_NULL_POINTER,
                                   VALIDATION_SEVERITY_ERROR,
                                   VALIDATION_CHECK_FORMAT_CAPS,
                                   component, file, line,
                                   "I/O handler is NULL");
    }
    
    int status = 0;
    
    // Check HDF5 format specific requirements
    if (handler->format_id == IO_FORMAT_LHALO_HDF5 || 
        handler->format_id == IO_FORMAT_CONSISTENT_TREES_HDF5 ||
        handler->format_id == IO_FORMAT_GADGET4_HDF5 ||
        handler->format_id == IO_FORMAT_GENESIS_HDF5 ||
        handler->format_id == IO_FORMAT_HDF5_OUTPUT) {
        
        // For HDF5 formats, metadata attributes are essential
        if (io_has_capability((struct io_interface *)handler, IO_CAP_METADATA_ATTRS)) {
            // This is good, format claims to support metadata attributes
        } else {
            status |= validation_add_result(ctx, VALIDATION_ERROR_FORMAT_INCOMPATIBLE,
                                         VALIDATION_SEVERITY_WARNING,
                                         VALIDATION_CHECK_FORMAT_CAPS,
                                         component, file, line,
                                         "HDF5 format '%s' should support metadata attributes",
                                         handler->name);
        }
        
        // Extended properties should be supported by HDF5 formats
        if (!io_has_capability((struct io_interface *)handler, IO_CAP_EXTENDED_PROPS)) {
            status |= validation_add_result(ctx, VALIDATION_ERROR_PROPERTY_INCOMPATIBLE,
                                         VALIDATION_SEVERITY_WARNING,
                                         VALIDATION_CHECK_PROPERTY_COMPAT,
                                         component, file, line,
                                         "HDF5 format '%s' should support extended properties",
                                         handler->name);
        }
    } else {
        // Not an HDF5 format
        status |= validation_add_result(ctx, VALIDATION_ERROR_FORMAT_INCOMPATIBLE,
                                     VALIDATION_SEVERITY_ERROR,
                                     VALIDATION_CHECK_FORMAT_CAPS,
                                     component, file, line,
                                     "Format '%s' is not an HDF5 format",
                                     handler->name);
    }
    
    return status;
}
