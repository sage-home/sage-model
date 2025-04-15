/**
 * @file test_module_template_validation.c
 * @brief Implementation of validation utilities for module template generation tests
 * @author SAGE Development Team
 */

#include "test_module_template_validation.h"
#include "../src/core/core_logging.h"

/* Read a file into a buffer */
bool read_file_to_buffer(const char *filepath, char **buffer, size_t *size) {
    FILE *file;
    size_t file_size;
    char *file_buffer;
    
    /* Open the file */
    file = fopen(filepath, "r");
    if (!file) {
        printf("Failed to open file: %s\n", filepath);
        return false;
    }
    
    /* Get file size */
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* Allocate buffer */
    file_buffer = (char *)malloc(file_size + 1);
    if (!file_buffer) {
        printf("Failed to allocate memory for file buffer\n");
        fclose(file);
        return false;
    }
    
    /* Read file */
    if (fread(file_buffer, 1, file_size, file) != file_size) {
        printf("Failed to read file: %s\n", filepath);
        free(file_buffer);
        fclose(file);
        return false;
    }
    
    /* Null-terminate buffer */
    file_buffer[file_size] = '\0';
    
    /* Close file */
    fclose(file);
    
    /* Set output parameters */
    *buffer = file_buffer;
    *size = file_size;
    
    return true;
}

/* Check if all patterns are present in content */
bool check_patterns(const char *content, const char **patterns, int num_patterns, const char **missing_pattern) {
    for (int i = 0; i < num_patterns; i++) {
        if (strstr(content, patterns[i]) == NULL) {
            if (missing_pattern != NULL) {
                *missing_pattern = patterns[i];
            }
            return false;
        }
    }
    return true;
}

/* Validate a module header file */
bool validate_module_header(const char *filepath, const struct module_template_params *params) {
    char *content;
    size_t size;
    bool result = true;
    const char *missing_pattern = NULL;
    
    /* Read file */
    if (!read_file_to_buffer(filepath, &content, &size)) {
        return false;
    }
    
    /* Basic patterns that should always be present */
    const char *basic_patterns[] = {
        "#pragma once",
        "extern \"C\"",
        params->module_name,
        params->module_prefix,
        params->description,
        "Module-specific data structure",
        "Module interface structure",
        "Initialize the module",
        "Clean up the module"
    };
    
    result = check_patterns(content, basic_patterns, sizeof(basic_patterns) / sizeof(basic_patterns[0]), &missing_pattern);
    if (!result) {
        printf("Header file missing basic pattern: %s\n", missing_pattern);
        free(content);
        return false;
    }
    
    /* Feature-specific patterns */
    if (params->include_galaxy_extension) {
        const char *galaxy_patterns[] = {
            "#include \"core_galaxy_extensions.h\"",
            "property_ids[10]"
        };
        
        result = check_patterns(content, galaxy_patterns, sizeof(galaxy_patterns) / sizeof(galaxy_patterns[0]), &missing_pattern);
        if (!result) {
            printf("Header file missing galaxy extension pattern: %s\n", missing_pattern);
            free(content);
            return false;
        }
    }
    
    if (params->include_event_handler) {
        const char *event_patterns[] = {
            "#include \"core_event_system.h\"",
            "handle_event",
            "Event handler function"
        };
        
        result = check_patterns(content, event_patterns, sizeof(event_patterns) / sizeof(event_patterns[0]), &missing_pattern);
        if (!result) {
            printf("Header file missing event handler pattern: %s\n", missing_pattern);
            free(content);
            return false;
        }
    }
    
    if (params->include_callback_registration) {
        const char *callback_patterns[] = {
            "#include \"core_module_callback.h\""
        };
        
        result = check_patterns(content, callback_patterns, sizeof(callback_patterns) / sizeof(callback_patterns[0]), &missing_pattern);
        if (!result) {
            printf("Header file missing callback pattern: %s\n", missing_pattern);
            free(content);
            return false;
        }
    }
    
    /* Check for module type-specific functions */
    switch (params->type) {
        case MODULE_TYPE_COOLING:
            {
                const char *cooling_patterns[] = {
                    "calculate_cooling",
                    "get_cooling_rate"
                };
                
                result = check_patterns(content, cooling_patterns, sizeof(cooling_patterns) / sizeof(cooling_patterns[0]), &missing_pattern);
                if (!result) {
                    printf("Header file missing cooling pattern: %s\n", missing_pattern);
                    free(content);
                    return false;
                }
            }
            break;
            
        case MODULE_TYPE_STAR_FORMATION:
            {
                const char *sf_patterns[] = {
                    "form_stars"
                };
                
                result = check_patterns(content, sf_patterns, sizeof(sf_patterns) / sizeof(sf_patterns[0]), &missing_pattern);
                if (!result) {
                    printf("Header file missing star formation pattern: %s\n", missing_pattern);
                    free(content);
                    return false;
                }
            }
            break;
            
        case MODULE_TYPE_FEEDBACK:
            {
                const char *feedback_patterns[] = {
                    "apply_feedback"
                };
                
                result = check_patterns(content, feedback_patterns, sizeof(feedback_patterns) / sizeof(feedback_patterns[0]), &missing_pattern);
                if (!result) {
                    printf("Header file missing feedback pattern: %s\n", missing_pattern);
                    free(content);
                    return false;
                }
            }
            break;
            
        default:
            /* No specific patterns for other module types */
            break;
    }
    
    free(content);
    return true;
}

/* Validate a module implementation file */
bool validate_module_implementation(const char *filepath, const struct module_template_params *params) {
    char *content;
    size_t size;
    bool result = true;
    const char *missing_pattern = NULL;
    
    /* Read file */
    if (!read_file_to_buffer(filepath, &content, &size)) {
        return false;
    }
    
    /* Basic patterns that should always be present */
    const char *basic_patterns[] = {
        "#include <stdio.h>",
        "#include <stdlib.h>",
        "#include <string.h>",
        params->module_name,
        params->module_prefix,
        params->description,
        "Initialize the module",
        "Clean up the module",
        "initialize",
        "cleanup",
        "MODULE_STATUS_SUCCESS",
        "mymalloc",
        "myfree",
        "Module interface structure"
    };
    
    result = check_patterns(content, basic_patterns, sizeof(basic_patterns) / sizeof(basic_patterns[0]), &missing_pattern);
    if (!result) {
        printf("Implementation file missing basic pattern: %s\n", missing_pattern);
        free(content);
        return false;
    }
    
    /* Feature-specific patterns */
    if (params->include_galaxy_extension) {
        const char *galaxy_patterns[] = {
            "Register galaxy properties",
            "galaxy_property_t property",
            "property_ids[0] = register_galaxy_property"
        };
        
        result = check_patterns(content, galaxy_patterns, sizeof(galaxy_patterns) / sizeof(galaxy_patterns[0]), &missing_pattern);
        if (!result) {
            printf("Implementation file missing galaxy extension pattern: %s\n", missing_pattern);
            free(content);
            return false;
        }
    }
    
    if (params->include_event_handler) {
        const char *event_patterns[] = {
            "Register event handlers",
            "event_register_handler",
            "handle_event",
            "EVENT_GALAXY_CREATED",
            "Unregister event handlers",
            "event_unregister_handler"
        };
        
        result = check_patterns(content, event_patterns, sizeof(event_patterns) / sizeof(event_patterns[0]), &missing_pattern);
        if (!result) {
            printf("Implementation file missing event handler pattern: %s\n", missing_pattern);
            free(content);
            return false;
        }
    }
    
    if (params->include_callback_registration) {
        const char *callback_patterns[] = {
            "Register callback functions",
            "module_register_function",
            "example_function",
            "getCurrentModuleId"
        };
        
        result = check_patterns(content, callback_patterns, sizeof(callback_patterns) / sizeof(callback_patterns[0]), &missing_pattern);
        if (!result) {
            printf("Implementation file missing callback pattern: %s\n", missing_pattern);
            free(content);
            return false;
        }
    }
    
    /* Check for module type-specific functions */
    switch (params->type) {
        case MODULE_TYPE_COOLING:
            {
                const char *cooling_patterns[] = {
                    "calculate_cooling",
                    "get_cooling_rate"
                };
                
                result = check_patterns(content, cooling_patterns, sizeof(cooling_patterns) / sizeof(cooling_patterns[0]), &missing_pattern);
                if (!result) {
                    printf("Implementation file missing cooling pattern: %s\n", missing_pattern);
                    free(content);
                    return false;
                }
            }
            break;
            
        case MODULE_TYPE_STAR_FORMATION:
            {
                const char *sf_patterns[] = {
                    "form_stars"
                };
                
                result = check_patterns(content, sf_patterns, sizeof(sf_patterns) / sizeof(sf_patterns[0]), &missing_pattern);
                if (!result) {
                    printf("Implementation file missing star formation pattern: %s\n", missing_pattern);
                    free(content);
                    return false;
                }
            }
            break;
            
        case MODULE_TYPE_FEEDBACK:
            {
                const char *feedback_patterns[] = {
                    "apply_feedback"
                };
                
                result = check_patterns(content, feedback_patterns, sizeof(feedback_patterns) / sizeof(feedback_patterns[0]), &missing_pattern);
                if (!result) {
                    printf("Implementation file missing feedback pattern: %s\n", missing_pattern);
                    free(content);
                    return false;
                }
            }
            break;
            
        default:
            /* No specific patterns for other module types */
            break;
    }
    
    free(content);
    return true;
}

/* Validate a module manifest file */
bool validate_module_manifest(const char *filepath, const struct module_template_params *params) {
    char *content;
    size_t size;
    bool result = true;
    const char *missing_pattern = NULL;
    
    /* Read file */
    if (!read_file_to_buffer(filepath, &content, &size)) {
        return false;
    }
    
    /* Basic patterns that should always be present */
    const char *basic_patterns[] = {
        "name",
        "version",
        "type",
        "author",
        params->module_name,
        params->version,
        params->author
    };
    
    result = check_patterns(content, basic_patterns, sizeof(basic_patterns) / sizeof(basic_patterns[0]), &missing_pattern);
    if (!result) {
        printf("Manifest file missing basic pattern: %s\n", missing_pattern);
        free(content);
        return false;
    }
    
    /* Check for module type */
    const char *module_type_str = NULL;
    switch (params->type) {
        case MODULE_TYPE_COOLING:
            module_type_str = "cooling";
            break;
        case MODULE_TYPE_STAR_FORMATION:
            module_type_str = "star_formation";
            break;
        case MODULE_TYPE_FEEDBACK:
            module_type_str = "feedback";
            break;
        case MODULE_TYPE_AGN:
            module_type_str = "agn";
            break;
        case MODULE_TYPE_MERGERS:
            module_type_str = "mergers";
            break;
        case MODULE_TYPE_DISK_INSTABILITY:
            module_type_str = "disk_instability";
            break;
        case MODULE_TYPE_REINCORPORATION:
            module_type_str = "reincorporation";
            break;
        case MODULE_TYPE_INFALL:
            module_type_str = "infall";
            break;
        case MODULE_TYPE_MISC:
            module_type_str = "misc";
            break;
        default:
            module_type_str = "unknown";
            break;
    }
    
    if (module_type_str && strstr(content, module_type_str) == NULL) {
        printf("Manifest file missing module type: %s\n", module_type_str);
        free(content);
        return false;
    }
    
    free(content);
    return true;
}

/* Validate a module makefile */
bool validate_module_makefile(const char *filepath, const struct module_template_params *params) {
    char *content;
    size_t size;
    bool result = true;
    const char *missing_pattern = NULL;
    
    /* Read file */
    if (!read_file_to_buffer(filepath, &content, &size)) {
        return false;
    }
    
    /* Basic patterns that should always be present */
    const char *basic_patterns[] = {
        "CFLAGS",
        "all:",
        "clean:",
        params->module_name
    };
    
    result = check_patterns(content, basic_patterns, sizeof(basic_patterns) / sizeof(basic_patterns[0]), &missing_pattern);
    if (!result) {
        printf("Makefile missing basic pattern: %s\n", missing_pattern);
        free(content);
        return false;
    }
    
    free(content);
    return true;
}

/* Validate a module test file */
bool validate_module_test_file(const char *filepath, const struct module_template_params *params) {
    char *content;
    size_t size;
    bool result = true;
    const char *missing_pattern = NULL;
    
    /* Read file */
    if (!read_file_to_buffer(filepath, &content, &size)) {
        return false;
    }
    
    /* Basic patterns that should always be present */
    const char *basic_patterns[] = {
        "#include",
        "main",
        "test",
        params->module_name
    };
    
    result = check_patterns(content, basic_patterns, sizeof(basic_patterns) / sizeof(basic_patterns[0]), &missing_pattern);
    if (!result) {
        printf("Test file missing basic pattern: %s\n", missing_pattern);
        free(content);
        return false;
    }
    
    free(content);
    return true;
}

/* Validate a module README file */
bool validate_module_readme(const char *filepath, const struct module_template_params *params) {
    char *content;
    size_t size;
    bool result = true;
    const char *missing_pattern = NULL;
    
    /* Read file */
    if (!read_file_to_buffer(filepath, &content, &size)) {
        return false;
    }
    
    /* Basic patterns that should always be present */
    const char *basic_patterns[] = {
        params->module_name,
        params->description,
        "Building and Installation",
        "Author",
        params->author
    };
    
    result = check_patterns(content, basic_patterns, sizeof(basic_patterns) / sizeof(basic_patterns[0]), &missing_pattern);
    if (!result) {
        printf("README file missing basic pattern: %s\n", missing_pattern);
        free(content);
        return false;
    }
    
    free(content);
    return true;
}

/* Setup template parameters for minimal configuration */
void setup_minimal_template_params(struct module_template_params *params, const char *output_dir, enum module_type module_type) {
    module_template_params_init(params);
    
    /* Basic parameters */
    switch (module_type) {
        case MODULE_TYPE_COOLING:
            strcpy(params->module_name, TEST_MODULE_COOLING);
            strcpy(params->module_prefix, TEST_MODULE_COOLING_PREFIX);
            break;
        case MODULE_TYPE_STAR_FORMATION:
            strcpy(params->module_name, TEST_MODULE_STAR_FORMATION);
            strcpy(params->module_prefix, TEST_MODULE_STAR_FORMATION_PREFIX);
            break;
        case MODULE_TYPE_FEEDBACK:
            strcpy(params->module_name, TEST_MODULE_FEEDBACK);
            strcpy(params->module_prefix, TEST_MODULE_FEEDBACK_PREFIX);
            break;
        default:
            strcpy(params->module_name, TEST_MODULE_COOLING);
            strcpy(params->module_prefix, TEST_MODULE_COOLING_PREFIX);
            break;
    }
    
    strcpy(params->author, "SAGE Test Framework");
    strcpy(params->email, "test@example.com");
    strcpy(params->description, "Minimal test module for SAGE");
    strcpy(params->version, "1.0.0");
    params->type = module_type;
    
    /* Features - minimal configuration */
    params->include_galaxy_extension = false;
    params->include_event_handler = false;
    params->include_callback_registration = false;
    params->include_manifest = true;
    params->include_makefile = true;
    params->include_test_file = false;
    params->include_readme = true;
    
    /* Output directory */
    strcpy(params->output_dir, output_dir);
}

/* Setup template parameters for full configuration */
void setup_full_template_params(struct module_template_params *params, const char *output_dir, enum module_type module_type) {
    module_template_params_init(params);
    
    /* Basic parameters */
    switch (module_type) {
        case MODULE_TYPE_COOLING:
            strcpy(params->module_name, TEST_MODULE_COOLING);
            strcpy(params->module_prefix, TEST_MODULE_COOLING_PREFIX);
            break;
        case MODULE_TYPE_STAR_FORMATION:
            strcpy(params->module_name, TEST_MODULE_STAR_FORMATION);
            strcpy(params->module_prefix, TEST_MODULE_STAR_FORMATION_PREFIX);
            break;
        case MODULE_TYPE_FEEDBACK:
            strcpy(params->module_name, TEST_MODULE_FEEDBACK);
            strcpy(params->module_prefix, TEST_MODULE_FEEDBACK_PREFIX);
            break;
        default:
            strcpy(params->module_name, TEST_MODULE_COOLING);
            strcpy(params->module_prefix, TEST_MODULE_COOLING_PREFIX);
            break;
    }
    
    strcpy(params->author, "SAGE Test Framework");
    strcpy(params->email, "test@example.com");
    strcpy(params->description, "Full featured test module for SAGE");
    strcpy(params->version, "1.0.0");
    params->type = module_type;
    
    /* Features - full configuration */
    params->include_galaxy_extension = true;
    params->include_event_handler = true;
    params->include_callback_registration = true;
    params->include_manifest = true;
    params->include_makefile = true;
    params->include_test_file = true;
    params->include_readme = true;
    
    /* Output directory */
    strcpy(params->output_dir, output_dir);
}

/* Setup template parameters for mixed configuration */
void setup_mixed_template_params(struct module_template_params *params, const char *output_dir, enum module_type module_type) {
    module_template_params_init(params);
    
    /* Basic parameters */
    switch (module_type) {
        case MODULE_TYPE_COOLING:
            strcpy(params->module_name, TEST_MODULE_COOLING);
            strcpy(params->module_prefix, TEST_MODULE_COOLING_PREFIX);
            break;
        case MODULE_TYPE_STAR_FORMATION:
            strcpy(params->module_name, TEST_MODULE_STAR_FORMATION);
            strcpy(params->module_prefix, TEST_MODULE_STAR_FORMATION_PREFIX);
            break;
        case MODULE_TYPE_FEEDBACK:
            strcpy(params->module_name, TEST_MODULE_FEEDBACK);
            strcpy(params->module_prefix, TEST_MODULE_FEEDBACK_PREFIX);
            break;
        default:
            strcpy(params->module_name, TEST_MODULE_COOLING);
            strcpy(params->module_prefix, TEST_MODULE_COOLING_PREFIX);
            break;
    }
    
    strcpy(params->author, "SAGE Test Framework");
    strcpy(params->email, "test@example.com");
    strcpy(params->description, "Mixed feature test module for SAGE");
    strcpy(params->version, "1.0.0");
    params->type = module_type;
    
    /* Features - mixed configuration */
    params->include_galaxy_extension = true;
    params->include_event_handler = false;
    params->include_callback_registration = true;
    params->include_manifest = true;
    params->include_makefile = false;
    params->include_test_file = true;
    params->include_readme = false;
    
    /* Output directory */
    strcpy(params->output_dir, output_dir);
}

/* Validate all generated module files */
bool validate_all_module_files(const struct module_template_params *params) {
    char filepath[PATH_MAX];
    bool result = true;
    
    /* Header file */
    snprintf(filepath, sizeof(filepath), "%s/%s.h", params->output_dir, params->module_name);
    result = validate_module_header(filepath, params);
    if (!result) {
        printf("Header file validation failed: %s\n", filepath);
        return false;
    }
    
    /* Implementation file */
    snprintf(filepath, sizeof(filepath), "%s/%s.c", params->output_dir, params->module_name);
    result = validate_module_implementation(filepath, params);
    if (!result) {
        printf("Implementation file validation failed: %s\n", filepath);
        return false;
    }
    
    /* Manifest file */
    if (params->include_manifest) {
        snprintf(filepath, sizeof(filepath), "%s/%s.manifest", params->output_dir, params->module_name);
        result = validate_module_manifest(filepath, params);
        if (!result) {
            printf("Manifest file validation failed: %s\n", filepath);
            return false;
        }
    }
    
    /* Makefile */
    if (params->include_makefile) {
        snprintf(filepath, sizeof(filepath), "%s/Makefile", params->output_dir);
        result = validate_module_makefile(filepath, params);
        if (!result) {
            printf("Makefile validation failed: %s\n", filepath);
            return false;
        }
    }
    
    /* Test file */
    if (params->include_test_file) {
        snprintf(filepath, sizeof(filepath), "%s/test_%s.c", params->output_dir, params->module_name);
        result = validate_module_test_file(filepath, params);
        if (!result) {
            printf("Test file validation failed: %s\n", filepath);
            return false;
        }
    }
    
    /* README file */
    if (params->include_readme) {
        snprintf(filepath, sizeof(filepath), "%s/README.md", params->output_dir);
        result = validate_module_readme(filepath, params);
        if (!result) {
            printf("README file validation failed: %s\n", filepath);
            return false;
        }
    }
    
    return true;
}
