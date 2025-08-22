#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "../core_allvars.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration format enumeration
typedef enum {
    CONFIG_FORMAT_UNKNOWN = 0,
    CONFIG_FORMAT_JSON,
    CONFIG_FORMAT_LEGACY_PAR
} config_format_t;

// Configuration error codes
typedef enum {
    CONFIG_SUCCESS = 0,
    CONFIG_ERROR_MEMORY,
    CONFIG_ERROR_FILE_READ,
    CONFIG_ERROR_PARSE,
    CONFIG_ERROR_VALIDATION,
    CONFIG_ERROR_NOT_SUPPORTED,
    CONFIG_ERROR_INVALID_STATE
} config_error_t;

// Main configuration structure
typedef struct config {
    char source_file[MAX_STRING_LEN];
    config_format_t format;
    struct params *params;     // Direct access to legacy params
    bool is_validated;
    char last_error[4096];     // Enhanced error buffer for multiple validation errors
    
    // Internal state management
    bool owns_params;          // Track if we need to free params
} config_t;

// Core configuration functions
extern config_t* config_create(void);
extern int config_read_file(config_t *config, const char *filename);
extern int config_validate(config_t *config);
extern void config_destroy(config_t *config);

// Format detection
extern config_format_t config_detect_format(const char *filename);

// Error handling
extern const char* config_get_last_error(const config_t *config);
extern void config_print_validation_errors(const config_t *config);

// Configuration utility functions
extern const char* config_format_to_string(config_format_t format);
extern const char* config_error_to_string(config_error_t error);

#ifdef __cplusplus
}
#endif