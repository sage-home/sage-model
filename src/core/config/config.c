#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "config_legacy.h"
#ifdef CONFIG_JSON_SUPPORT
#include "config_json.h"
#endif
#include "config_validation.h"
#include "../memory.h"

// Create a new configuration structure
config_t* config_create(void) {
    config_t *config = sage_malloc(sizeof(config_t));
    if (!config) {
        return NULL;
    }
    
    // Initialize all fields
    memset(config, 0, sizeof(config_t));
    config->format = CONFIG_FORMAT_UNKNOWN;
    config->is_validated = false;
    config->owns_params = false;
    
    return config;
}

// Detect configuration format based on filename
config_format_t config_detect_format(const char *filename) {
    if (!filename) {
        return CONFIG_FORMAT_UNKNOWN;
    }
    
    const char *extension = strrchr(filename, '.');
    if (!extension) {
        // No extension - assume legacy .par format
        return CONFIG_FORMAT_LEGACY_PAR;
    }
    
    if (strcmp(extension, ".json") == 0) {
        return CONFIG_FORMAT_JSON;
    } else if (strcmp(extension, ".par") == 0) {
        return CONFIG_FORMAT_LEGACY_PAR;
    }
    
    // Default to legacy format for unknown extensions
    return CONFIG_FORMAT_LEGACY_PAR;
}

// Read configuration file
int config_read_file(config_t *config, const char *filename) {
    if (!config || !filename) {
        if (config) {
            snprintf(config->last_error, sizeof(config->last_error),
                    "Invalid parameters: config=%p, filename=%p", 
                    (void*)config, (void*)filename);
        }
        return CONFIG_ERROR_INVALID_STATE;
    }
    
    // Clear previous errors
    config->last_error[0] = '\0';
    config->is_validated = false;
    
    // Clean up previous params if we own them
    if (config->owns_params && config->params) {
        sage_free(config->params);
        config->params = NULL;
        config->owns_params = false;
    }
    
    // Detect format
    config->format = config_detect_format(filename);
    
    // Store source filename
    strncpy(config->source_file, filename, sizeof(config->source_file) - 1);
    config->source_file[sizeof(config->source_file) - 1] = '\0';
    
    // Read based on format
    int result = CONFIG_ERROR_NOT_SUPPORTED;
    
    switch (config->format) {
        case CONFIG_FORMAT_LEGACY_PAR:
            result = config_read_legacy_par(config, filename);
            break;
            
        case CONFIG_FORMAT_JSON:
#ifdef CONFIG_JSON_SUPPORT
            result = config_read_json(config, filename);
#else
            snprintf(config->last_error, sizeof(config->last_error),
                    "JSON support not compiled in. Recompile with -DSAGE_CONFIG_JSON_SUPPORT=ON or use .par format");
            result = CONFIG_ERROR_NOT_SUPPORTED;
#endif
            break;
            
        default:
            snprintf(config->last_error, sizeof(config->last_error),
                    "Unsupported configuration format for file: %s", filename);
            result = CONFIG_ERROR_NOT_SUPPORTED;
            break;
    }
    
    return result;
}

// Validate configuration
int config_validate(config_t *config) {
    if (!config) {
        return CONFIG_ERROR_INVALID_STATE;
    }
    
    if (!config->params) {
        snprintf(config->last_error, sizeof(config->last_error),
                "No configuration data loaded - call config_read_file() first");
        return CONFIG_ERROR_INVALID_STATE;
    }
    
    // Perform validation using the validation framework
    return config_validate_params(config);
}

// Destroy configuration structure
void config_destroy(config_t *config) {
    if (!config) {
        return;
    }
    
    // Free params if we own them
    if (config->owns_params && config->params) {
        sage_free(config->params);
    }
    
    sage_free(config);
}

// Get last error message
const char* config_get_last_error(const config_t *config) {
    if (!config) {
        return "Invalid configuration object";
    }
    
    if (config->last_error[0] == '\0') {
        return "No error";
    }
    
    return config->last_error;
}

// Print validation errors to stderr
void config_print_validation_errors(const config_t *config) {
    if (!config) {
        fprintf(stderr, "Error: Invalid configuration object\n");
        return;
    }
    
    if (config->last_error[0] != '\0') {
        fprintf(stderr, "Configuration validation errors:\n%s\n", config->last_error);
    } else {
        fprintf(stderr, "No validation errors found\n");
    }
}

// Convert format enum to string
const char* config_format_to_string(config_format_t format) {
    switch (format) {
        case CONFIG_FORMAT_UNKNOWN:
            return "unknown";
        case CONFIG_FORMAT_JSON:
            return "json";
        case CONFIG_FORMAT_LEGACY_PAR:
            return "legacy_par";
        default:
            return "invalid";
    }
}

// Convert error code to string
const char* config_error_to_string(config_error_t error) {
    switch (error) {
        case CONFIG_SUCCESS:
            return "success";
        case CONFIG_ERROR_MEMORY:
            return "memory_allocation_failed";
        case CONFIG_ERROR_FILE_READ:
            return "file_read_failed";
        case CONFIG_ERROR_PARSE:
            return "parse_error";
        case CONFIG_ERROR_VALIDATION:
            return "validation_failed";
        case CONFIG_ERROR_NOT_SUPPORTED:
            return "format_not_supported";
        case CONFIG_ERROR_INVALID_STATE:
            return "invalid_state";
        default:
            return "unknown_error";
    }
}