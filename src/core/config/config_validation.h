#pragma once

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Parameter validation types
typedef enum {
    PARAM_INT32,
    PARAM_DOUBLE,
    PARAM_STRING,
    PARAM_ENUM
} param_type_t;

// Validation constraint structure
typedef struct config_validator {
    const char *param_name;
    param_type_t type;
    union {
        struct { int32_t min, max; } int_range;
        struct { double min, max; } double_range;
        struct { size_t max_len; } string_constraint;
        struct { const char **valid_values; size_t count; } enum_constraint;
    } constraint;
    bool required;
    const char *description;
} config_validator_t;

// Main validation function
extern int config_validate_params(config_t *config);

// Individual parameter validation
extern bool validate_parameter(const struct params *params, const config_validator_t *rule, 
                             char *error_buffer, size_t buffer_size);

// Validation rule accessors
extern const config_validator_t* get_validation_rules(void);
extern size_t get_validation_rules_count(void);

#ifdef __cplusplus
}
#endif