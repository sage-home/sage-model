#include <stdio.h>
#include <string.h>
#include <math.h>

#include "config_validation.h"

// Validation rules for key parameters
static const config_validator_t validation_rules[] = {
    {
        .param_name = "BoxSize",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.1, 10000.0},
        .required = true,
        .description = "Simulation box size in Mpc/h"
    },
    {
        .param_name = "FirstFile",
        .type = PARAM_INT32,
        .constraint.int_range = {0, 10000},
        .required = true,
        .description = "First tree file number"
    },
    {
        .param_name = "LastFile",
        .type = PARAM_INT32,
        .constraint.int_range = {0, 10000},
        .required = true,
        .description = "Last tree file number"
    },
    {
        .param_name = "Omega",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.0, 1.0},
        .required = true,
        .description = "Matter density parameter"
    },
    {
        .param_name = "OmegaLambda",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.0, 1.0},
        .required = true,
        .description = "Dark energy density parameter"
    },
    {
        .param_name = "Hubble_h",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.1, 2.0},
        .required = true,
        .description = "Hubble parameter"
    },
    {
        .param_name = "BaryonFrac",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.0, 1.0},
        .required = true,
        .description = "Baryon fraction"
    },
    {
        .param_name = "PartMass",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.0, 1e12},
        .required = true,
        .description = "Particle mass in 10^10 Msun/h"
    },
    {
        .param_name = "SFprescription",
        .type = PARAM_INT32,
        .constraint.int_range = {0, 2},
        .required = true,
        .description = "Star formation prescription (0-2)"
    },
    {
        .param_name = "AGNrecipeOn",
        .type = PARAM_INT32,
        .constraint.int_range = {0, 3},
        .required = true,
        .description = "AGN recipe setting (0-3)"
    },
    {
        .param_name = "SupernovaRecipeOn",
        .type = PARAM_INT32,
        .constraint.int_range = {0, 1},
        .required = true,
        .description = "Supernova recipe (0-1)"
    },
    {
        .param_name = "ReionizationOn",
        .type = PARAM_INT32,
        .constraint.int_range = {0, 1},
        .required = true,
        .description = "Reionization recipe (0-1)"
    },
    {
        .param_name = "DiskInstabilityOn",
        .type = PARAM_INT32,
        .constraint.int_range = {0, 1},
        .required = true,
        .description = "Disk instability recipe (0-1)"
    },
    {
        .param_name = "SfrEfficiency",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.0, 1.0},
        .required = true,
        .description = "Star formation efficiency"
    },
    {
        .param_name = "FeedbackReheatingEpsilon",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.0, 100.0},
        .required = true,
        .description = "Feedback reheating efficiency"
    },
    {
        .param_name = "FeedbackEjectionEfficiency",
        .type = PARAM_DOUBLE,
        .constraint.double_range = {0.0, 100.0},
        .required = true,
        .description = "Feedback ejection efficiency"
    },
    {
        .param_name = "OutputDir",
        .type = PARAM_STRING,
        .constraint.string_constraint = {MAX_STRING_LEN - 1},
        .required = true,
        .description = "Output directory path"
    },
    {
        .param_name = "SimulationDir",
        .type = PARAM_STRING,
        .constraint.string_constraint = {MAX_STRING_LEN - 1},
        .required = true,
        .description = "Tree file directory path"
    },
    {
        .param_name = "TreeName",
        .type = PARAM_STRING,
        .constraint.string_constraint = {MAX_STRING_LEN - 1},
        .required = true,
        .description = "Tree file base name"
    }
};

// Get validation rules
const config_validator_t* get_validation_rules(void) {
    return validation_rules;
}

size_t get_validation_rules_count(void) {
    return sizeof(validation_rules) / sizeof(validation_rules[0]);
}

// Validate individual parameter
bool validate_parameter(const struct params *params, const config_validator_t *rule, 
                       char *error_buffer, size_t buffer_size) {
    if (!params || !rule || !error_buffer || buffer_size == 0) {
        return false;
    }
    
    error_buffer[0] = '\0';
    
    switch (rule->type) {
        case PARAM_DOUBLE: {
            double value = 0.0;
            
            // Get parameter value by name (using string comparison)
            if (strcmp(rule->param_name, "BoxSize") == 0) {
                value = params->BoxSize;
            } else if (strcmp(rule->param_name, "Omega") == 0) {
                value = params->Omega;
            } else if (strcmp(rule->param_name, "OmegaLambda") == 0) {
                value = params->OmegaLambda;
            } else if (strcmp(rule->param_name, "Hubble_h") == 0) {
                value = params->Hubble_h;
            } else if (strcmp(rule->param_name, "BaryonFrac") == 0) {
                value = params->BaryonFrac;
            } else if (strcmp(rule->param_name, "PartMass") == 0) {
                value = params->PartMass;
            } else if (strcmp(rule->param_name, "SfrEfficiency") == 0) {
                value = params->SfrEfficiency;
            } else if (strcmp(rule->param_name, "FeedbackReheatingEpsilon") == 0) {
                value = params->FeedbackReheatingEpsilon;
            } else if (strcmp(rule->param_name, "FeedbackEjectionEfficiency") == 0) {
                value = params->FeedbackEjectionEfficiency;
            } else {
                // Unknown parameter name
                snprintf(error_buffer, buffer_size, 
                        "Unknown double parameter: %s", rule->param_name);
                return false;
            }
            
            // Check range
            if (value < rule->constraint.double_range.min || 
                value > rule->constraint.double_range.max) {
                snprintf(error_buffer, buffer_size,
                        "%s = %.6g is outside valid range [%.6g, %.6g]: %s",
                        rule->param_name, value,
                        rule->constraint.double_range.min,
                        rule->constraint.double_range.max,
                        rule->description);
                return false;
            }
            break;
        }
        
        case PARAM_INT32: {
            int32_t value = 0;
            
            // Get parameter value by name
            if (strcmp(rule->param_name, "FirstFile") == 0) {
                value = params->FirstFile;
            } else if (strcmp(rule->param_name, "LastFile") == 0) {
                value = params->LastFile;
            } else if (strcmp(rule->param_name, "SFprescription") == 0) {
                value = params->SFprescription;
            } else if (strcmp(rule->param_name, "AGNrecipeOn") == 0) {
                value = params->AGNrecipeOn;
            } else if (strcmp(rule->param_name, "SupernovaRecipeOn") == 0) {
                value = params->SupernovaRecipeOn;
            } else if (strcmp(rule->param_name, "ReionizationOn") == 0) {
                value = params->ReionizationOn;
            } else if (strcmp(rule->param_name, "DiskInstabilityOn") == 0) {
                value = params->DiskInstabilityOn;
            } else {
                // Unknown parameter name
                snprintf(error_buffer, buffer_size, 
                        "Unknown integer parameter: %s", rule->param_name);
                return false;
            }
            
            // Check range
            if (value < rule->constraint.int_range.min || 
                value > rule->constraint.int_range.max) {
                snprintf(error_buffer, buffer_size,
                        "%s = %d is outside valid range [%d, %d]: %s",
                        rule->param_name, value,
                        rule->constraint.int_range.min,
                        rule->constraint.int_range.max,
                        rule->description);
                return false;
            }
            break;
        }
        
        case PARAM_STRING: {
            const char *value = NULL;
            
            // Get parameter value by name
            if (strcmp(rule->param_name, "OutputDir") == 0) {
                value = params->OutputDir;
            } else if (strcmp(rule->param_name, "SimulationDir") == 0) {
                value = params->SimulationDir;
            } else if (strcmp(rule->param_name, "TreeName") == 0) {
                value = params->TreeName;
            } else {
                // Unknown parameter name
                snprintf(error_buffer, buffer_size, 
                        "Unknown string parameter: %s", rule->param_name);
                return false;
            }
            
            // Check string requirements
            if (!value || strlen(value) == 0) {
                snprintf(error_buffer, buffer_size,
                        "%s is empty or null: %s",
                        rule->param_name, rule->description);
                return false;
            }
            
            if (strlen(value) > rule->constraint.string_constraint.max_len) {
                snprintf(error_buffer, buffer_size,
                        "%s is too long (%zu chars, max %zu): %s",
                        rule->param_name, strlen(value),
                        rule->constraint.string_constraint.max_len,
                        rule->description);
                return false;
            }
            break;
        }
        
        case PARAM_ENUM:
            // Enum validation would be implemented here for more complex constraints
            break;
    }
    
    return true;
}

// Main validation function
int config_validate_params(config_t *config) {
    if (!config || !config->params) {
        return CONFIG_ERROR_INVALID_STATE;
    }
    
    // Clear previous validation errors
    config->last_error[0] = '\0';
    bool has_errors = false;
    
    const size_t num_rules = get_validation_rules_count();
    const config_validator_t *rules = get_validation_rules();
    
    for (size_t i = 0; i < num_rules; i++) {
        const config_validator_t *rule = &rules[i];
        char validation_buffer[512];
        
        if (!validate_parameter(config->params, rule, validation_buffer, sizeof(validation_buffer))) {
            has_errors = true;
            
            // Append validation error to buffer (don't overwrite)
            size_t current_len = strlen(config->last_error);
            size_t remaining = sizeof(config->last_error) - current_len - 1;
            
            if (remaining > 0) {
                if (current_len > 0) {
                    strncat(config->last_error, "\n", remaining);
                    remaining--;
                }
                strncat(config->last_error, validation_buffer, remaining);
            }
        }
    }
    
    // Additional cross-parameter validation
    if (!has_errors) {
        // Validate FirstFile <= LastFile
        if (config->params->FirstFile > config->params->LastFile) {
            has_errors = true;
            size_t current_len = strlen(config->last_error);
            size_t remaining = sizeof(config->last_error) - current_len - 1;
            
            if (remaining > 0) {
                if (current_len > 0) {
                    strncat(config->last_error, "\n", remaining);
                    remaining--;
                }
                strncat(config->last_error, 
                       "FirstFile must be <= LastFile", remaining);
            }
        }
        
        // Validate Omega + OmegaLambda approximately equals 1 (allow some tolerance)
        double omega_total = config->params->Omega + config->params->OmegaLambda;
        if (fabs(omega_total - 1.0) > 0.1) {
            has_errors = true;
            size_t current_len = strlen(config->last_error);
            size_t remaining = sizeof(config->last_error) - current_len - 1;
            
            if (remaining > 0) {
                if (current_len > 0) {
                    strncat(config->last_error, "\n", remaining);
                    remaining--;
                }
                char omega_error[256];
                snprintf(omega_error, sizeof(omega_error),
                        "Omega + OmegaLambda = %.3f should be approximately 1.0",
                        omega_total);
                strncat(config->last_error, omega_error, remaining);
            }
        }
    }
    
    config->is_validated = !has_errors;
    return has_errors ? CONFIG_ERROR_VALIDATION : CONFIG_SUCCESS;
}