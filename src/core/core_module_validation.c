#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_module_validation.h"
#include "core_logging.h"
#include "core_dynamic_library.h"
#include "core_mymalloc.h"

/* Module interface definitions */
struct cooling_module {
    struct base_module base;
    double (*calculate_cooling)(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data);
    double (*get_cooling_rate)(int gal_idx, struct GALAXY *galaxies, void *module_data);
};

struct star_formation_module {
    struct base_module base;
    double (*form_stars)(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data);
};

/* Initialize a validation result structure */
bool module_validation_result_init(module_validation_result_t *result) {
    if (!result) {
        return false;
    }
    
    memset(result, 0, sizeof(module_validation_result_t));
    return true;
}

/* Add an issue to a validation result */
bool module_validation_add_issue(
    module_validation_result_t *result,
    validation_severity_t severity,
    const char *message,
    const char *component,
    const char *file,
    int line
) {
    if (!result || !message) {
        return false;
    }
    
    /* Check if we can add more issues */
    if (result->num_issues >= MAX_VALIDATION_ISSUES) {
        LOG_WARNING("Maximum validation issues reached, some issues may be omitted");
        return false;
    }
    
    /* Add the issue */
    module_validation_issue_t *issue = &result->issues[result->num_issues++];
    
    issue->severity = severity;
    strncpy(issue->message, message, MAX_VALIDATION_MESSAGE - 1);
    issue->message[MAX_VALIDATION_MESSAGE - 1] = '\0';
    
    if (component) {
        strncpy(issue->component, component, sizeof(issue->component) - 1);
        issue->component[sizeof(issue->component) - 1] = '\0';
    } else {
        issue->component[0] = '\0';
    }
    
    if (file) {
        strncpy(issue->file, file, sizeof(issue->file) - 1);
        issue->file[sizeof(issue->file) - 1] = '\0';
    } else {
        issue->file[0] = '\0';
    }
    
    issue->line = line;
    
    /* Update count based on severity */
    switch (severity) {
        case VALIDATION_ERROR:
            result->error_count++;
            break;
        case VALIDATION_WARNING:
            result->warning_count++;
            break;
        case VALIDATION_INFO:
            result->info_count++;
            break;
    }
    
    return true;
}

/* Format a validation result as text */
bool module_validation_result_format(
    const module_validation_result_t *result,
    char *output,
    size_t size
) {
    if (!result || !output || size == 0) {
        return false;
    }
    
    char *cursor = output;
    size_t remaining = size;
    int written;
    
    /* Write summary */
    written = snprintf(cursor, remaining,
                      "Validation Summary:\n"
                      "  Total issues: %d\n"
                      "  Errors: %d\n"
                      "  Warnings: %d\n"
                      "  Info: %d\n\n",
                      result->num_issues,
                      result->error_count,
                      result->warning_count,
                      result->info_count);
    
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    
    cursor += written;
    remaining -= written;
    
    /* If no issues, we're done */
    if (result->num_issues == 0) {
        written = snprintf(cursor, remaining, "No issues found.\n");
        return true;
    }
    
    /* Write issues */
    written = snprintf(cursor, remaining, "Issues:\n");
    if (written < 0 || (size_t)written >= remaining) {
        return false;
    }
    
    cursor += written;
    remaining -= written;
    
    for (int i = 0; i < result->num_issues; i++) {
        const module_validation_issue_t *issue = &result->issues[i];
        
        /* Format the severity */
        const char *severity_str;
        switch (issue->severity) {
            case VALIDATION_ERROR:
                severity_str = "ERROR";
                break;
            case VALIDATION_WARNING:
                severity_str = "WARNING";
                break;
            case VALIDATION_INFO:
                severity_str = "INFO";
                break;
            default:
                severity_str = "UNKNOWN";
                break;
        }
        
        /* Format the issue */
        written = snprintf(cursor, remaining,
                          "[%s] %s",
                          severity_str, issue->message);
        
        if (written < 0 || (size_t)written >= remaining) {
            return false;
        }
        
        cursor += written;
        remaining -= written;
        
        /* Add component and location if available */
        if (issue->component[0]) {
            written = snprintf(cursor, remaining, " [%s]", issue->component);
            
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            
            cursor += written;
            remaining -= written;
        }
        
        if (issue->file[0]) {
            written = snprintf(cursor, remaining, " (%s", issue->file);
            
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            
            cursor += written;
            remaining -= written;
            
            if (issue->line > 0) {
                written = snprintf(cursor, remaining, ":%d", issue->line);
                
                if (written < 0 || (size_t)written >= remaining) {
                    return false;
                }
                
                cursor += written;
                remaining -= written;
            }
            
            written = snprintf(cursor, remaining, ")");
            
            if (written < 0 || (size_t)written >= remaining) {
                return false;
            }
            
            cursor += written;
            remaining -= written;
        }
        
        /* Add newline */
        written = snprintf(cursor, remaining, "\n");
        
        if (written < 0 || (size_t)written >= remaining) {
            return false;
        }
        
        cursor += written;
        remaining -= written;
    }
    
    return true;
}

/* Initialize validation options with defaults */
bool module_validation_options_init(module_validation_options_t *options) {
    if (!options) {
        return false;
    }
    
    /* Set default options */
    options->validate_interface = true;
    options->validate_function_sigs = true;
    options->validate_dependencies = true;
    options->validate_manifest = true;
    options->verbose = false;
    options->strict = false;
    
    return true;
}

/* Validate a module's interface */
bool module_validate_interface(
    const struct base_module *module,
    module_validation_result_t *result,
    const module_validation_options_t *options
) {
    if (!module || !result) {
        return false;
    }
    
    bool valid = true;
    
    /* Check module name */
    if (!module->name[0]) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module name cannot be empty",
            "interface",
            NULL,
            0
        );
        valid = false;
    }
    
    /* Check module version */
    if (!module->version[0]) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module version cannot be empty",
            "interface",
            NULL,
            0
        );
        valid = false;
    }
    
    /* Check module type */
    if (module->type <= MODULE_TYPE_UNKNOWN || module->type >= MODULE_TYPE_MAX) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Invalid module type",
            "interface",
            NULL,
            0
        );
        valid = false;
    }
    
    /* Check lifecycle functions */
    if (!module->initialize) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module must implement initialize function",
            "interface",
            NULL,
            0
        );
        valid = false;
    }
    
    if (!module->cleanup) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module must implement cleanup function",
            "interface",
            NULL,
            0
        );
        valid = false;
    }
    
    /* Check module type-specific requirements */
    switch (module->type) {
        case MODULE_TYPE_COOLING:
            /* Validate cooling module */
            {
                struct cooling_module *cooling = (struct cooling_module *)module;
                
                if (!cooling->calculate_cooling) {
                    module_validation_add_issue(
                        result,
                        VALIDATION_ERROR,
                        "Cooling module must implement calculate_cooling function",
                        "interface",
                        NULL,
                        0
                    );
                    valid = false;
                }
            }
            break;
            
        case MODULE_TYPE_STAR_FORMATION:
            /* Validate star formation module */
            {
                struct star_formation_module *sf = (struct star_formation_module *)module;
                
                if (!sf->form_stars) {
                    module_validation_add_issue(
                        result,
                        VALIDATION_ERROR,
                        "Star formation module must implement form_stars function",
                        "interface",
                        NULL,
                        0
                    );
                    valid = false;
                }
            }
            break;
            
        /* Add other module types as needed */
        default:
            /* No specific requirements for other types yet */
            break;
    }
    
    /* If we have a manifest, validate it */
    if (module->manifest && options->validate_manifest) {
        module_validate_manifest_structure(module->manifest, result, options);
    } else if (!module->manifest && options->verbose) {
        module_validation_add_issue(
            result,
            VALIDATION_INFO,
            "Module has no manifest",
            "interface",
            NULL,
            0
        );
    }
    
    return valid;
}

/* Validate a module's function signatures */
bool module_validate_function_signatures(
    const struct base_module *module,
    module_validation_result_t *result,
    const module_validation_options_t *options
) {
    if (!module || !result) {
        return false;
    }
    
    /* Currently a placeholder - would require reflection or runtime type checking */
    
    /* Add informational message */
    if (options->verbose) {
        module_validation_add_issue(
            result,
            VALIDATION_INFO,
            "Function signature validation not implemented",
            "signatures",
            NULL,
            0
        );
    }
    
    return true;
}

/* Validate a module's dependencies */
bool module_validate_dependencies(
    int module_id,
    module_validation_result_t *result,
    const module_validation_options_t *options
) {
    if (module_id < 0 || !result) {
        return false;
    }
    
    /* Get the module */
    struct base_module *module;
    void *module_data;
    
    int status = module_get(module_id, &module, &module_data);
    if (status != MODULE_STATUS_SUCCESS) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Failed to get module",
            "dependencies",
            NULL,
            0
        );
        return false;
    }
    
    bool valid = true;
    
    /* Check if the module has dependencies */
    if (!module->dependencies || module->num_dependencies <= 0) {
        if (options->verbose) {
            module_validation_add_issue(
                result,
                VALIDATION_INFO,
                "Module has no dependencies",
                "dependencies",
                NULL,
                0
            );
        }
        return true;
    }
    
    /* Check each dependency */
    for (int i = 0; i < module->num_dependencies; i++) {
        const module_dependency_t *dep = &module->dependencies[i];
        
        /* Skip optional dependencies */
        if (dep->optional) {
            continue;
        }
        
        /* Try to find the dependency */
        int found = false;
        
        if (dep->type != MODULE_TYPE_UNKNOWN) {
            /* Look up module by type */
            struct base_module *dep_module;
            void *dep_module_data;
            
            status = module_get_active_by_type(dep->type, &dep_module, &dep_module_data);
            if (status == MODULE_STATUS_SUCCESS) {
                found = true;
                
                /* Check version compatibility if applicable */
                if (dep->exact_match || dep->min_version_str[0] || dep->max_version_str[0]) {
                    /* This would need to be implemented */
                }
            }
        } else if (dep->name[0]) {
            /* Look up module by name */
            int dep_module_id = module_find_by_name(dep->name);
            if (dep_module_id >= 0) {
                found = true;
                
                /* Check version compatibility if applicable */
                if (dep->exact_match || dep->min_version_str[0] || dep->max_version_str[0]) {
                    /* This would need to be implemented */
                }
            }
        }
        
        /* Report error if dependency not found */
        if (!found) {
            char message[MAX_VALIDATION_MESSAGE];
            
            if (dep->type != MODULE_TYPE_UNKNOWN) {
                snprintf(message, sizeof(message),
                        "Required dependency not found: type %s",
                        module_type_name(dep->type));
            } else if (dep->name[0]) {
                snprintf(message, sizeof(message),
                        "Required dependency not found: %s",
                        dep->name);
            } else {
                snprintf(message, sizeof(message),
                        "Invalid dependency: no type or name specified");
            }
            
            module_validation_add_issue(
                result,
                VALIDATION_ERROR,
                message,
                "dependencies",
                NULL,
                0
            );
            
            valid = false;
        }
    }
    
    return valid;
}

/* Validate a module manifest structure */
bool module_validate_manifest_structure(
    const struct module_manifest *manifest,
    module_validation_result_t *result,
    const module_validation_options_t *options __attribute__((unused))
) {
    if (!manifest || !result) {
        return false;
    }
    
    bool valid = true;
    
    /* Check required fields */
    if (manifest->name[0] == '\0') {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module name cannot be empty",
            "manifest",
            NULL,
            0
        );
        valid = false;
    }
    
    if (manifest->version_str[0] == '\0') {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module version cannot be empty",
            "manifest",
            NULL,
            0
        );
        valid = false;
    }
    
    if (manifest->type <= MODULE_TYPE_UNKNOWN || manifest->type >= MODULE_TYPE_MAX) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Invalid module type",
            "manifest",
            NULL,
            0
        );
        valid = false;
    }
    
    if (manifest->library_path[0] == '\0') {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module library path cannot be empty",
            "manifest",
            NULL,
            0
        );
        valid = false;
    }
    
    /* API version must be valid */
    if (manifest->api_version <= 0) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Invalid API version",
            "manifest",
            NULL,
            0
        );
        valid = false;
    }
    
    /* Check dependencies if any */
    for (int i = 0; i < manifest->num_dependencies; i++) {
        const module_dependency_t *dep = &manifest->dependencies[i];
        
        /* Either name or type must be specified */
        if (dep->name[0] == '\0' && dep->type == MODULE_TYPE_UNKNOWN) {
            module_validation_add_issue(
                result,
                VALIDATION_ERROR,
                "Dependency must have either name or type specified",
                "manifest",
                NULL,
                0
            );
            valid = false;
            continue;
        }
        
        /* Check version constraints if specified */
        if ((dep->min_version_str[0] || dep->max_version_str[0]) && dep->name[0] == '\0') {
            module_validation_add_issue(
                result,
                VALIDATION_WARNING,
                "Version constraints without module name may not be applied correctly",
                "manifest",
                NULL,
                0
            );
        }
    }
    
    return valid;
}

/* Load a module from a file and get its interface */
static struct base_module *load_module_from_file(
    const char *path,
    module_validation_result_t *result
) {
    if (!path || !result) {
        return NULL;
    }
    
    /* Load the library */
    dynamic_library_handle_t lib_handle;
    dl_error_t error = dynamic_library_open(path, &lib_handle);
    
    if (error != DL_SUCCESS) {
        char error_msg[MAX_DL_ERROR_LENGTH];
        dynamic_library_get_error(error_msg, sizeof(error_msg));
        
        char message[MAX_VALIDATION_MESSAGE];
        snprintf(message, sizeof(message),
                "Failed to load library: %s",
                error_msg);
        
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            message,
            "loader",
            path,
            0
        );
        
        return NULL;
    }
    
    /* Get the module interface */
    void *symbol;
    error = dynamic_library_get_symbol(lib_handle, "module_get_interface", &symbol);
    
    if (error != DL_SUCCESS) {
        char error_msg[MAX_DL_ERROR_LENGTH];
        dynamic_library_get_error(error_msg, sizeof(error_msg));
        
        char message[MAX_VALIDATION_MESSAGE];
        snprintf(message, sizeof(message),
                "Failed to get module interface: %s",
                error_msg);
        
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            message,
            "loader",
            path,
            0
        );
        
        dynamic_library_close(lib_handle);
        return NULL;
    }
    
    /* Call the function to get the interface */
    struct base_module *(*get_interface)() = symbol;
    struct base_module *module = get_interface();
    
    if (!module) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Module interface function returned NULL",
            "loader",
            path,
            0
        );
        
        dynamic_library_close(lib_handle);
        return NULL;
    }
    
    /* Close the library - we've got what we need */
    dynamic_library_close(lib_handle);
    
    return module;
}

/* Validate a module from a file */
bool module_validate_file(
    const char *path,
    module_validation_result_t *result,
    const module_validation_options_t *options
) {
    if (!path || !result || !options) {
        return false;
    }
    
    /* Load the module */
    struct base_module *module = load_module_from_file(path, result);
    if (!module) {
        return false;
    }
    
    bool valid = true;
    
    /* Validate the interface */
    if (options->validate_interface) {
        valid = module_validate_interface(module, result, options) && valid;
    }
    
    /* Validate function signatures */
    if (options->validate_function_sigs) {
        valid = module_validate_function_signatures(module, result, options) && valid;
    }
    
    /* Note: dependency validation requires the module to be registered in the system */
    
    return valid;
}

/* Validate a module by ID */
bool module_validate_by_id(
    int module_id,
    module_validation_result_t *result,
    const module_validation_options_t *options
) {
    if (module_id < 0 || !result || !options) {
        return false;
    }
    
    /* Get the module */
    struct base_module *module;
    void *module_data;
    
    int status = module_get(module_id, &module, &module_data);
    if (status != MODULE_STATUS_SUCCESS) {
        module_validation_add_issue(
            result,
            VALIDATION_ERROR,
            "Failed to get module",
            "system",
            NULL,
            0
        );
        return false;
    }
    
    bool valid = true;
    
    /* Validate the interface */
    if (options->validate_interface) {
        valid = module_validate_interface(module, result, options) && valid;
    }
    
    /* Validate function signatures */
    if (options->validate_function_sigs) {
        valid = module_validate_function_signatures(module, result, options) && valid;
    }
    
    /* Validate dependencies */
    if (options->validate_dependencies) {
        valid = module_validate_dependencies(module_id, result, options) && valid;
    }
    
    return valid;
}

/* Determine if a validation result contains errors */
bool module_validation_has_errors(
    const module_validation_result_t *result,
    const module_validation_options_t *options
) {
    if (!result) {
        return true;
    }
    
    if (result->error_count > 0) {
        return true;
    }
    
    if (options && options->strict && result->warning_count > 0) {
        return true;
    }
    
    return false;
}
