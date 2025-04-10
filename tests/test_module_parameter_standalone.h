#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Stub implementation of the logging system for standalone testing
 * This avoids having to link against the full core_parameter_views.c
 */
#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)

/* Stub for parameter views and logging */
struct params;
struct logging_params_view {
    int dummy;  /* Just a placeholder */
};

/* Function prototype for the missing logging params view initialization */
void initialize_logging_params_view(struct logging_params_view *view, const struct params *params);

/* Implementation of the missing function - will be provided in test file */
void initialize_logging_params_view(struct logging_params_view *view, const struct params *params) {
    /* Check for NULL parameters */
    if (view == NULL) {
        fprintf(stderr, "Error: Null pointer passed to initialize_logging_params_view\n");
        return;
    }
    
    /* Unused parameter (suppresses warning) */
    (void)params;
    
    /* Initialize with dummy values */
    view->dummy = 0;
}

/* Constants for parameter system */
#define MAX_PARAM_NAME 64
#define MAX_PARAM_STRING 256
#define MAX_PARAM_DESCRIPTION 256
#define MAX_PARAM_UNITS 32
#define MAX_MODULE_PARAMETERS 128
#define MAX_ERROR_MESSAGE_LENGTH 256

/**
 * Parameter status codes
 * 
 * Return codes used by parameter functions to indicate success or failure
 */
enum module_parameter_status {
    MODULE_PARAM_SUCCESS = 0,
    MODULE_PARAM_ERROR = -1,
    MODULE_PARAM_INVALID_ARGS = -2,
    MODULE_PARAM_NOT_FOUND = -3,
    MODULE_PARAM_TYPE_MISMATCH = -4,
    MODULE_PARAM_OUT_OF_BOUNDS = -5,
    MODULE_PARAM_OUT_OF_MEMORY = -6,
    MODULE_PARAM_ALREADY_EXISTS = -7,
    MODULE_PARAM_FILE_ERROR = -8,
    MODULE_PARAM_PARSE_ERROR = -9
};

/**
 * Parameter types
 * 
 * Supported parameter data types
 */
enum module_parameter_type {
    MODULE_PARAM_TYPE_INT = 0,
    MODULE_PARAM_TYPE_FLOAT = 1,
    MODULE_PARAM_TYPE_DOUBLE = 2,
    MODULE_PARAM_TYPE_BOOL = 3,
    MODULE_PARAM_TYPE_STRING = 4
};

/**
 * Parameter structure
 * 
 * Contains all information about a module parameter
 */
struct module_parameter {
    char name[MAX_PARAM_NAME];           /* Parameter name */
    enum module_parameter_type type;     /* Parameter data type */
    
    /* Value limits (for numeric types) */
    union {
        struct { int min; int max; } int_range;
        struct { float min; float max; } float_range;
        struct { double min; double max; } double_range;
    } limits;
    
    /* Parameter value */
    union {
        int int_val;
        float float_val;
        double double_val;
        bool bool_val;
        char string_val[MAX_PARAM_STRING];
    } value;
    
    bool has_limits;                     /* Whether bounds checking is enabled */
    char description[MAX_PARAM_DESCRIPTION]; /* Parameter description */
    char units[MAX_PARAM_UNITS];         /* Parameter units (e.g., "Mpc/h") */
    int module_id;                       /* ID of module that owns this parameter */
};
typedef struct module_parameter module_parameter_t;

/**
 * Parameter registry
 * 
 * Stores all parameters for a module
 */
struct module_parameter_registry {
    module_parameter_t *parameters;      /* Array of parameters */
    int num_parameters;                  /* Number of registered parameters */
    int capacity;                        /* Total capacity of the registry */
};
typedef struct module_parameter_registry module_parameter_registry_t;

/* Function prototypes for testing */
int module_parameter_registry_init(module_parameter_registry_t *registry);
int module_parameter_registry_free(module_parameter_registry_t *registry);
int module_register_parameter(module_parameter_registry_t *registry, const module_parameter_t *param);
int module_find_parameter(module_parameter_registry_t *registry, const char *name, int module_id);
int module_get_parameter_by_index(module_parameter_registry_t *registry, int index, module_parameter_t *param);
int module_get_parameter(module_parameter_registry_t *registry, const char *name, int module_id, module_parameter_t *param);
bool module_validate_parameter(const module_parameter_t *param);
bool module_check_parameter_bounds(const module_parameter_t *param);
const char *module_parameter_type_to_string(enum module_parameter_type type);
enum module_parameter_type module_parameter_type_from_string(const char *type_str);
int module_get_parameter_int(module_parameter_registry_t *registry, const char *name, int module_id, int *value);
int module_get_parameter_float(module_parameter_registry_t *registry, const char *name, int module_id, float *value);
int module_get_parameter_double(module_parameter_registry_t *registry, const char *name, int module_id, double *value);
int module_get_parameter_bool(module_parameter_registry_t *registry, const char *name, int module_id, bool *value);
int module_get_parameter_string(module_parameter_registry_t *registry, const char *name, int module_id, char *value, size_t max_len);
int module_set_parameter_int(module_parameter_registry_t *registry, const char *name, int module_id, int value);
int module_set_parameter_float(module_parameter_registry_t *registry, const char *name, int module_id, float value);
int module_set_parameter_double(module_parameter_registry_t *registry, const char *name, int module_id, double value);
int module_set_parameter_bool(module_parameter_registry_t *registry, const char *name, int module_id, bool value);
int module_set_parameter_string(module_parameter_registry_t *registry, const char *name, int module_id, const char *value);
int module_create_parameter_int(module_parameter_t *param, const char *name, int value, int min, int max, const char *description, const char *units, int module_id);
int module_create_parameter_float(module_parameter_t *param, const char *name, float value, float min, float max, const char *description, const char *units, int module_id);
int module_create_parameter_double(module_parameter_t *param, const char *name, double value, double min, double max, const char *description, const char *units, int module_id);
int module_create_parameter_bool(module_parameter_t *param, const char *name, bool value, const char *description, int module_id);
int module_create_parameter_string(module_parameter_t *param, const char *name, const char *value, const char *description, int module_id);

/* Parameter import/export functions */
int module_load_parameters_from_file(module_parameter_registry_t *registry, const char *filename);
int module_save_parameters_to_file(module_parameter_registry_t *registry, const char *filename);