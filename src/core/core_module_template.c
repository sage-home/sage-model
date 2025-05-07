#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <ctype.h>

#include "core_module_template.h"
#include "core_logging.h"
#include "core_mymalloc.h"

/* Platform-specific directory creation */
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/* Utility function to create directory with parents */
static int create_directory_recursive(const char *path) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;
    int result;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    /* Remove trailing slash */
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = 0;
    }
    
    /* Try creating the directory */
    result = mkdir(tmp, 0755);
    if (result == 0 || errno == EEXIST) {
        return 0;
    }
    
    /* If parent directory doesn't exist, create it */
    p = strrchr(tmp, '/');
    if (!p) {
        p = strrchr(tmp, '\\');
    }
    
    if (p) {
        *p = 0;
        create_directory_recursive(tmp);
        *p = '/';
        result = mkdir(tmp, 0755);
    }
    
    return result;
}

/* Utility function to ensure a valid C identifier */
static void __attribute__((unused)) make_valid_identifier(char *identifier) {
    if (!identifier) return;
    
    /* First character must be a letter or underscore */
    if (!isalpha(identifier[0]) && identifier[0] != '_') {
        identifier[0] = '_';
    }
    
    /* Replace invalid characters with underscores */
    for (int i = 0; identifier[i]; i++) {
        if (!isalnum(identifier[i]) && identifier[i] != '_') {
            identifier[i] = '_';
        }
    }
}

/* Utility function to get current date string */
static void get_current_date(char *date_str, size_t size) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(date_str, size, "%Y-%m-%d", tm);
}

/* Forward declaration of sample prop usage function */
static void generate_sample_prop_usage(FILE *file, enum module_type type);

/* Initialize template parameters with defaults */
int module_template_params_init(struct module_template_params *params) {
    if (!params) return -1;
    
    memset(params, 0, sizeof(struct module_template_params));
    
    /* Set default values */
    strcpy(params->version, "1.0.0");
    params->type = MODULE_TYPE_COOLING;  /* Default to cooling module */
    
    /* Enable standard features */
    params->include_manifest = true;
    params->include_readme = true;
    
    return 0;
}

/* Get interface name for module type */
const char *module_get_interface_name(enum module_type type) {
    switch (type) {
        case MODULE_TYPE_COOLING:
            return "cooling_module";
        case MODULE_TYPE_STAR_FORMATION:
            return "star_formation_module";
        case MODULE_TYPE_FEEDBACK:
            return "feedback_module";
        case MODULE_TYPE_AGN:
            return "agn_module";
        case MODULE_TYPE_MERGERS:
            return "mergers_module";
        case MODULE_TYPE_DISK_INSTABILITY:
            return "disk_instability_module";
        case MODULE_TYPE_REINCORPORATION:
            return "reincorporation_module";
        case MODULE_TYPE_INFALL:
            return "infall_module";
        case MODULE_TYPE_MISC:
            return "misc_module";
        default:
            return "unknown_module";
    }
}

/* Get function signatures for module type */
int module_get_function_signatures(enum module_type type, 
                                  char signatures[][256], 
                                  int max_signatures) {
    int count = 0;
    
    /* Common signatures for all module types */
    if (count < max_signatures) {
        snprintf(signatures[count++], 256, 
                "int initialize(struct params *params, void **module_data)");
    }
    
    if (count < max_signatures) {
        snprintf(signatures[count++], 256, 
                "int cleanup(void *module_data)");
    }
    
    /* Type-specific signatures */
    switch (type) {
        case MODULE_TYPE_COOLING:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "double calculate_cooling(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data)");
            }
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "double get_cooling_rate(int gal_idx, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        case MODULE_TYPE_STAR_FORMATION:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "double form_stars(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        case MODULE_TYPE_FEEDBACK:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "void apply_feedback(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        case MODULE_TYPE_AGN:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "void process_agn(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        case MODULE_TYPE_MERGERS:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "void process_mergers(int p, int q, double mass_ratio, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        case MODULE_TYPE_DISK_INSTABILITY:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "void check_disk_instability(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        case MODULE_TYPE_REINCORPORATION:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "double calculate_reincorporation(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        case MODULE_TYPE_INFALL:
            if (count < max_signatures) {
                snprintf(signatures[count++], 256, 
                        "double calculate_infall(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data)");
            }
            break;
            
        default:
            break;
    }
    
    return count;
}

/* Generate module header file */
int module_generate_header(const struct module_template_params *params, const char *output_path) {
    FILE *file;
    char date_str[20];
    char guard_name[MAX_MODULE_NAME * 2];
    int num_signatures;
    char signatures[10][256];
    const char *interface_name;
    
    /* Validate parameters */
    if (!params || !output_path) {
        LOG_ERROR("Invalid parameters to module_generate_header");
        return -1;
    }
    
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Get current date */
    get_current_date(date_str, sizeof(date_str));
    
    /* Create include guard name */
    snprintf(guard_name, sizeof(guard_name), "%s_H", params->module_name);
    for (int i = 0; guard_name[i]; i++) {
        guard_name[i] = toupper(guard_name[i]);
    }
    
    /* Get interface name and function signatures */
    interface_name = module_get_interface_name(params->type);
    num_signatures = module_get_function_signatures(params->type, signatures, 10);
    
    /* Write header content */
    fprintf(file, "/**\n");
    fprintf(file, " * @file %s.h\n", params->module_name);
    fprintf(file, " * @brief %s\n", params->description);
    fprintf(file, " * @author %s\n", params->author);
    fprintf(file, " * @date %s\n", date_str);
    fprintf(file, " */\n\n");
    
    fprintf(file, "#pragma once\n\n");
    
    fprintf(file, "#ifdef __cplusplus\n");
    fprintf(file, "extern \"C\" {\n");
    fprintf(file, "#endif\n\n");
    
    fprintf(file, "#include \"core_allvars.h\"\n");
    fprintf(file, "#include \"core_module_system.h\"\n");
    fprintf(file, "#include \"core_properties.h\"    // For GALAXY_PROP_* macros\n");
    
    if (params->include_galaxy_extension) {
        fprintf(file, "#include \"core_galaxy_extensions.h\"\n");
    }
    
    if (params->include_event_handler) {
        fprintf(file, "#include \"core_event_system.h\"\n");
    }
    
    if (params->include_callback_registration) {
        fprintf(file, "#include \"core_module_callback.h\"\n");
    }
    
    fprintf(file, "\n");
    
    /* Module data structure */
    fprintf(file, "/**\n");
    fprintf(file, " * Module-specific data structure\n");
    fprintf(file, " */\n");
    fprintf(file, "typedef struct {\n");
    fprintf(file, "    /* Add module-specific data fields here */\n");
    
    if (params->include_galaxy_extension) {
        fprintf(file, "    int property_ids[10];  /* IDs for registered galaxy properties */\n");
    }
    
    fprintf(file, "} %s_data_t;\n\n", params->module_prefix);
    
    /* Module interface declaration */
    fprintf(file, "/**\n");
    fprintf(file, " * Module interface structure\n");
    fprintf(file, " */\n");
    fprintf(file, "extern struct %s %s_interface;\n\n", interface_name, params->module_prefix);
    
    /* Function prototypes */
    fprintf(file, "/**\n");
    fprintf(file, " * Initialize the module\n");
    fprintf(file, " * \n");
    fprintf(file, " * @param params Global parameters\n");
    fprintf(file, " * @param module_data Output pointer for module-specific data\n");
    fprintf(file, " * @return 0 on success, error code on failure\n");
    fprintf(file, " */\n");
    fprintf(file, "int %s_initialize(struct params *params, void **module_data);\n\n", params->module_prefix);
    
    fprintf(file, "/**\n");
    fprintf(file, " * Clean up the module\n");
    fprintf(file, " * \n");
    fprintf(file, " * @param module_data Module-specific data\n");
    fprintf(file, " * @return 0 on success, error code on failure\n");
    fprintf(file, " */\n");
    fprintf(file, "int %s_cleanup(void *module_data);\n\n", params->module_prefix);
    
    /* Add any module-specific function prototypes */
    for (int i = 2; i < num_signatures; i++) {
        char function_name[256];
        char *sig_copy = strdup(signatures[i]);
        char *open_paren = strchr(sig_copy, '(');
        
        if (open_paren) {
            *open_paren = '\0';
            sprintf(function_name, "%s_%s", params->module_prefix, sig_copy);
            *open_paren = '(';
            
            fprintf(file, "/**\n");
            fprintf(file, " * Module-specific function\n");
            fprintf(file, " */\n");
            fprintf(file, "%s%s;\n\n", function_name, open_paren);
        }
        
        free(sig_copy);
    }
    
    if (params->include_event_handler) {
        fprintf(file, "/**\n");
        fprintf(file, " * Event handler function\n");
        fprintf(file, " * \n");
        fprintf(file, " * @param event Event data\n");
        fprintf(file, " * @param user_data User data (module data)\n");
        fprintf(file, " * @return 0 on success, error code on failure\n");
        fprintf(file, " */\n");
        fprintf(file, "int %s_handle_event(const struct event *event, void *user_data);\n\n", params->module_prefix);
    }
    
    fprintf(file, "#ifdef __cplusplus\n");
    fprintf(file, "}\n");
    fprintf(file, "#endif\n");
    
    fclose(file);
    
    LOG_INFO("Generated module header file: %s", output_path);
    return 0;
}

/* Generate module implementation file */
int module_generate_implementation(const struct module_template_params *params, const char *output_path) {
    FILE *file;
    char date_str[20];
    int num_signatures;
    char signatures[10][256];
    const char *interface_name;
    
    /* Validate parameters */
    if (!params || !output_path) {
        LOG_ERROR("Invalid parameters to module_generate_implementation");
        return -1;
    }
    
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Get current date */
    get_current_date(date_str, sizeof(date_str));
    
    /* Get interface name and function signatures */
    interface_name = module_get_interface_name(params->type);
    num_signatures = module_get_function_signatures(params->type, signatures, 10);
    
    /* Write implementation content */
    fprintf(file, "/**\n");
    fprintf(file, " * @file %s.c\n", params->module_name);
    fprintf(file, " * @brief %s\n", params->description);
    fprintf(file, " * @author %s\n", params->author);
    fprintf(file, " * @date %s\n", date_str);
    fprintf(file, " */\n\n");
    
    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include <stdlib.h>\n");
    fprintf(file, "#include <string.h>\n");
    fprintf(file, "#include <math.h>\n\n");
    
    fprintf(file, "#include \"%s.h\"\n", params->module_name);
    fprintf(file, "#include \"core_logging.h\"\n");
    fprintf(file, "#include \"core_mymalloc.h\"\n");
    fprintf(file, "#include \"core_properties.h\"\n\n");
    
    /* Add sample property usage demonstration */
    generate_sample_prop_usage(file, params->type);
    
    /* Module initialization function */
    fprintf(file, "/**\n");
    fprintf(file, " * Initialize the module\n");
    fprintf(file, " */\n");
    fprintf(file, "int %s_initialize(struct params *params, void **module_data) {\n", params->module_prefix);
    fprintf(file, "    /* Validate parameters */\n");
    fprintf(file, "    if (!params || !module_data) {\n");
    fprintf(file, "        LOG_ERROR(\"Invalid parameters to %s_initialize\");\n", params->module_prefix);
    fprintf(file, "        return MODULE_STATUS_INVALID_ARGS;\n");
    fprintf(file, "    }\n\n");
    
    fprintf(file, "    /* Allocate module data */\n");
    fprintf(file, "    %s_data_t *data = mymalloc(sizeof(%s_data_t));\n", params->module_prefix, params->module_prefix);
    fprintf(file, "    if (!data) {\n");
    fprintf(file, "        LOG_ERROR(\"Failed to allocate memory for module data\");\n");
    fprintf(file, "        return MODULE_STATUS_OUT_OF_MEMORY;\n");
    fprintf(file, "    }\n\n");
    
    fprintf(file, "    /* Initialize module data */\n");
    fprintf(file, "    memset(data, 0, sizeof(%s_data_t));\n\n", params->module_prefix);
    
    if (params->include_galaxy_extension) {
        fprintf(file, "    /* Register galaxy properties */\n");
        fprintf(file, "    galaxy_property_t property = {\n");
        fprintf(file, "        .name = \"%s_example_property\",\n", params->module_prefix);
        fprintf(file, "        .size = sizeof(float),\n");
        fprintf(file, "        .module_id = getCurrentModuleId(),\n");
        fprintf(file, "        .serialize = serialize_float,\n");
        fprintf(file, "        .deserialize = deserialize_float,\n");
        fprintf(file, "        .description = \"Example property for %s\",\n", params->module_name);
        fprintf(file, "        .units = \"\"\n");
        fprintf(file, "    };\n");
        fprintf(file, "    data->property_ids[0] = register_galaxy_property(&property);\n\n");
    }
    
    if (params->include_event_handler) {
        fprintf(file, "    /* Register event handlers */\n");
        fprintf(file, "    event_register_handler(EVENT_GALAXY_CREATED, %s_handle_event, data);\n\n", params->module_prefix);
    }
    
    if (params->include_callback_registration) {
        fprintf(file, "    /* Register callback functions */\n");
        fprintf(file, "    module_register_function(\n");
        fprintf(file, "        getCurrentModuleId(),\n");
        fprintf(file, "        \"%s_example_function\",\n", params->module_prefix);
        fprintf(file, "        (void *)%s_example_function,\n", params->module_prefix);
        fprintf(file, "        FUNCTION_TYPE_INT,\n");
        fprintf(file, "        \"int (int, struct GALAXY *, void *)\",\n");
        fprintf(file, "        \"Example function for %s\"\n", params->module_name);
        fprintf(file, "    );\n\n");
    }
    
    fprintf(file, "    /* Store module data */\n");
    fprintf(file, "    *module_data = data;\n\n");
    
    fprintf(file, "    LOG_INFO(\"%s module initialized\");\n", params->module_name);
    fprintf(file, "    return MODULE_STATUS_SUCCESS;\n");
    fprintf(file, "}\n\n");
    
    /* Module cleanup function */
    fprintf(file, "/**\n");
    fprintf(file, " * Clean up the module\n");
    fprintf(file, " */\n");
    fprintf(file, "int %s_cleanup(void *module_data) {\n", params->module_prefix);
    fprintf(file, "    /* Validate parameters */\n");
    fprintf(file, "    if (!module_data) {\n");
    fprintf(file, "        LOG_ERROR(\"Invalid parameters to %s_cleanup\");\n", params->module_prefix);
    fprintf(file, "        return MODULE_STATUS_INVALID_ARGS;\n");
    fprintf(file, "    }\n\n");
    
    fprintf(file, "    /* Cast to module data type */\n");
    fprintf(file, "    %s_data_t *data = (%s_data_t *)module_data;\n\n", params->module_prefix, params->module_prefix);
    
    if (params->include_event_handler) {
        fprintf(file, "    /* Unregister event handlers */\n");
        fprintf(file, "    event_unregister_handler(EVENT_GALAXY_CREATED, %s_handle_event, data);\n\n", params->module_prefix);
    }
    
    fprintf(file, "    /* Free module data */\n");
    fprintf(file, "    myfree(data);\n\n");
    
    fprintf(file, "    LOG_INFO(\"%s module cleaned up\");\n", params->module_name);
    fprintf(file, "    return MODULE_STATUS_SUCCESS;\n");
    fprintf(file, "}\n\n");
    
    /* Module-specific functions */
    for (int i = 2; i < num_signatures; i++) {
        char function_name[256];
        char *sig_copy = strdup(signatures[i]);
        char *open_paren = strchr(sig_copy, '(');
        
        if (open_paren) {
            *open_paren = '\0';
            sprintf(function_name, "%s_%s", params->module_prefix, sig_copy);
            *open_paren = '(';
            
            fprintf(file, "/**\n");
            fprintf(file, " * Module-specific function\n");
            fprintf(file, " */\n");
            fprintf(file, "%s%s {\n", function_name, open_paren);
            fprintf(file, "    /* Validate parameters */\n");
            fprintf(file, "    if (!module_data) {\n");
            fprintf(file, "        LOG_ERROR(\"Invalid parameters to %s\");\n", function_name);
            fprintf(file, "        return 0; /* Return appropriate error value */\n");
            fprintf(file, "    }\n\n");
            
            fprintf(file, "    /* Cast to module data type */\n");
            fprintf(file, "    %s_data_t *data = (%s_data_t *)module_data;\n\n", params->module_prefix, params->module_prefix);
            
            // Add example implementations using GALAXY_PROP_* macros
            if (strstr(sig_copy, "cooling")) {
                fprintf(file, "    /* Example implementation using property accessors */\n");
                fprintf(file, "    double result = 0.0;\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* Access galaxy properties using GALAXY_PROP_* macros */\n");
                fprintf(file, "    if (GALAXY_PROP_HotGas(&galaxies[gal_idx]) > 0.0 && GALAXY_PROP_Vvir(&galaxies[gal_idx]) > 0.0) {\n");
                fprintf(file, "        const double tcool = GALAXY_PROP_Rvir(&galaxies[gal_idx]) / GALAXY_PROP_Vvir(&galaxies[gal_idx]);\n");
                fprintf(file, "        const double temp = 35.9 * GALAXY_PROP_Vvir(&galaxies[gal_idx]) * GALAXY_PROP_Vvir(&galaxies[gal_idx]);\n");
                fprintf(file, "        \n");
                fprintf(file, "        /* Calculate cooling rate based on properties */\n");
                fprintf(file, "        result = 0.1 * GALAXY_PROP_HotGas(&galaxies[gal_idx]) / tcool * dt;\n");
                fprintf(file, "        \n");
                fprintf(file, "        /* Ensure we don't cool more than available */\n");
                fprintf(file, "        if (result > GALAXY_PROP_HotGas(&galaxies[gal_idx]))\n");
                fprintf(file, "            result = GALAXY_PROP_HotGas(&galaxies[gal_idx]);\n");
                fprintf(file, "    }\n\n");
                fprintf(file, "    /* TODO: Implement your actual cooling logic here */\n\n");
            } else if (strstr(sig_copy, "infall")) {
                fprintf(file, "    /* Example implementation using property accessors */\n");
                fprintf(file, "    double result = 0.0;\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* Access galaxy properties using GALAXY_PROP_* macros */\n");
                fprintf(file, "    double baryon_fraction = 0.17; /* Example value, should come from params */\n");
                fprintf(file, "    double total_baryons = baryon_fraction * GALAXY_PROP_Mvir(&galaxies[gal_idx]);\n");
                fprintf(file, "    double current_baryons = GALAXY_PROP_StellarMass(&galaxies[gal_idx]) + \n");
                fprintf(file, "                             GALAXY_PROP_ColdGas(&galaxies[gal_idx]) + \n");
                fprintf(file, "                             GALAXY_PROP_HotGas(&galaxies[gal_idx]) + \n");
                fprintf(file, "                             GALAXY_PROP_EjectedMass(&galaxies[gal_idx]);\n");
                fprintf(file, "    \n");
                fprintf(file, "    result = total_baryons - current_baryons;\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* TODO: Implement your actual infall logic here */\n\n");
            } else if (strstr(sig_copy, "star")) {
                fprintf(file, "    /* Example implementation using property accessors */\n");
                fprintf(file, "    double stars_formed = 0.0;\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* Access galaxy properties using GALAXY_PROP_* macros */\n");
                fprintf(file, "    if (GALAXY_PROP_ColdGas(&galaxies[gal_idx]) > 0.0) {\n");
                fprintf(file, "        double efficiency = 0.05; /* Example efficiency, should come from params */\n");
                fprintf(file, "        stars_formed = efficiency * GALAXY_PROP_ColdGas(&galaxies[gal_idx]) * dt;\n");
                fprintf(file, "        \n");
                fprintf(file, "        /* Update galaxy properties */\n");
                fprintf(file, "        double metallicity = GALAXY_PROP_MetalsColdGas(&galaxies[gal_idx]) / GALAXY_PROP_ColdGas(&galaxies[gal_idx]);\n");
                fprintf(file, "        \n");
                fprintf(file, "        /* Ensure we don't use more gas than available */\n");
                fprintf(file, "        if (stars_formed > GALAXY_PROP_ColdGas(&galaxies[gal_idx]))\n");
                fprintf(file, "            stars_formed = GALAXY_PROP_ColdGas(&galaxies[gal_idx]);\n");
                fprintf(file, "    }\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* TODO: Implement your actual star formation logic here */\n\n");
            } else if (strstr(sig_copy, "feedback")) {
                fprintf(file, "    /* Example implementation using property accessors */\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* Access galaxy properties using GALAXY_PROP_* macros */\n");
                fprintf(file, "    double stellar_mass = GALAXY_PROP_StellarMass(&galaxies[gal_idx]);\n");
                fprintf(file, "    double ejection_fraction = 0.1; /* Example fraction, should come from params */\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* Eject some hot gas */\n");
                fprintf(file, "    if (GALAXY_PROP_HotGas(&galaxies[gal_idx]) > 0.0) {\n");
                fprintf(file, "        double ejected = ejection_fraction * GALAXY_PROP_HotGas(&galaxies[gal_idx]);\n");
                fprintf(file, "        double metals_ejected = ejection_fraction * GALAXY_PROP_MetalsHotGas(&galaxies[gal_idx]);\n");
                fprintf(file, "        \n");
                fprintf(file, "        /* Update galaxy properties */\n");
                fprintf(file, "        GALAXY_PROP_HotGas(&galaxies[gal_idx]) -= ejected;\n");
                fprintf(file, "        GALAXY_PROP_MetalsHotGas(&galaxies[gal_idx]) -= metals_ejected;\n");
                fprintf(file, "        GALAXY_PROP_EjectedMass(&galaxies[gal_idx]) += ejected;\n");
                fprintf(file, "        GALAXY_PROP_MetalsEjectedMass(&galaxies[gal_idx]) += metals_ejected;\n");
                fprintf(file, "    }\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* TODO: Implement your actual feedback logic here */\n\n");
            } else {
                fprintf(file, "    /* Example implementation using property accessors */\n");
                fprintf(file, "    /* Access galaxy properties using GALAXY_PROP_* macros */\n");
                fprintf(file, "    /* For example: GALAXY_PROP_StellarMass(&galaxies[gal_idx]) */\n");
                fprintf(file, "    \n");
                fprintf(file, "    /* TODO: Implement function logic */\n\n");
            }
            
            /* Add appropriate return value */
            if (strstr(sig_copy, "double")) {
                fprintf(file, "    return 0.0;\n");
            } else if (strstr(sig_copy, "int")) {
                fprintf(file, "    return 0;\n");
            } else if (strstr(sig_copy, "void")) {
                fprintf(file, "    return;\n");
            } else {
                fprintf(file, "    return 0;\n");
            }
            
            fprintf(file, "}\n\n");
        }
        
        free(sig_copy);
    }
    
    /* Event handler function */
    if (params->include_event_handler) {
        fprintf(file, "/**\n");
        fprintf(file, " * Event handler function\n");
        fprintf(file, " */\n");
        fprintf(file, "int %s_handle_event(const struct event *event, void *user_data) {\n", params->module_prefix);
        fprintf(file, "    /* Validate parameters */\n");
        fprintf(file, "    if (!event || !user_data) {\n");
        fprintf(file, "        LOG_ERROR(\"Invalid parameters to %s_handle_event\");\n", params->module_prefix);
        fprintf(file, "        return -1;\n");
        fprintf(file, "    }\n\n");
        
        fprintf(file, "    /* Cast to module data type */\n");
        fprintf(file, "    %s_data_t *data = (%s_data_t *)user_data;\n\n", params->module_prefix, params->module_prefix);
        
        fprintf(file, "    /* Handle different event types */\n");
        fprintf(file, "    switch (event->type) {\n");
        fprintf(file, "        case EVENT_GALAXY_CREATED:\n");
        fprintf(file, "            /* Handle galaxy creation event */\n");
        fprintf(file, "            if (event->galaxy_index >= 0) {\n");
        fprintf(file, "                /* Can access galaxy properties with GALAXY_PROP_* macros */\n");
        fprintf(file, "                /* in the actual galaxy processing functions */\n");
        fprintf(file, "            }\n");
        fprintf(file, "            break;\n");
        fprintf(file, "        default:\n");
        fprintf(file, "            /* Ignore unknown events */\n");
        fprintf(file, "            break;\n");
        fprintf(file, "    }\n\n");
        
        fprintf(file, "    return 0;\n");
        fprintf(file, "}\n\n");
    }
    
    /* Callback function if needed */
    if (params->include_callback_registration) {
        fprintf(file, "/**\n");
        fprintf(file, " * Example callback function\n");
        fprintf(file, " */\n");
        fprintf(file, "int %s_example_function(int gal_idx, struct GALAXY *galaxies, void *module_data) {\n", params->module_prefix);
        fprintf(file, "    /* Validate parameters */\n");
        fprintf(file, "    if (!galaxies || !module_data) {\n");
        fprintf(file, "        LOG_ERROR(\"Invalid parameters to %s_example_function\");\n", params->module_prefix);
        fprintf(file, "        return -1;\n");
        fprintf(file, "    }\n\n");
        
        fprintf(file, "    /* Cast to module data type */\n");
        fprintf(file, "    %s_data_t *data = (%s_data_t *)module_data;\n\n", params->module_prefix, params->module_prefix);
        
        fprintf(file, "    /* Example using property macros */\n");
        fprintf(file, "    double stellar_mass = GALAXY_PROP_StellarMass(&galaxies[gal_idx]);\n");
        fprintf(file, "    double cold_gas = GALAXY_PROP_ColdGas(&galaxies[gal_idx]);\n");
        fprintf(file, "    \n");
        fprintf(file, "    /* TODO: Implement function logic */\n\n");
        
        fprintf(file, "    return 0;\n");
        fprintf(file, "}\n\n");
    }
    
    /* Module interface structure */
    fprintf(file, "/**\n");
    fprintf(file, " * Module interface structure\n");
    fprintf(file, " */\n");
    fprintf(file, "struct %s %s_interface = {\n", interface_name, params->module_prefix);
    fprintf(file, "    .base = {\n");
    fprintf(file, "        .name = \"%s\",\n", params->module_name);
    fprintf(file, "        .version = \"%s\",\n", params->version);
    fprintf(file, "        .author = \"%s\",\n", params->author);
    fprintf(file, "        .type = %s,\n", module_type_name(params->type));
    fprintf(file, "        .initialize = %s_initialize,\n", params->module_prefix);
    fprintf(file, "        .cleanup = %s_cleanup\n", params->module_prefix);
    fprintf(file, "    },\n");
    
    /* Add module-specific function pointers */
    for (int i = 2; i < num_signatures; i++) {
        char function_name[256];
        char *sig_copy = strdup(signatures[i]);
        char *open_paren = strchr(sig_copy, '(');
        
        if (open_paren) {
            *open_paren = '\0';
            strcpy(function_name, sig_copy);
            
            fprintf(file, "    .%s = %s_%s", function_name, params->module_prefix, function_name);
            
            if (i < num_signatures - 1) {
                fprintf(file, ",\n");
            } else {
                fprintf(file, "\n");
            }
        }
        
        free(sig_copy);
    }
    
    fprintf(file, "};\n");
    
    fclose(file);
    
    LOG_INFO("Generated module implementation file: %s", output_path);
    return 0;
}

/* Generate module manifest file */
int module_generate_manifest(const struct module_template_params *params, const char *output_path) {
    FILE *file;
    char date_str[20];
    
    /* Validate parameters */
    if (!params || !output_path) {
        LOG_ERROR("Invalid parameters to module_generate_manifest");
        return -1;
    }
    
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Get current date */
    get_current_date(date_str, sizeof(date_str));
    
    /* Write manifest content */
    fprintf(file, "# Manifest file for %s\n", params->module_name);
    fprintf(file, "# Generated on %s\n\n", date_str);
    
    fprintf(file, "name: %s\n", params->module_name);
    fprintf(file, "version: %s\n", params->version);
    fprintf(file, "author: %s\n", params->author);
    
    if (params->email[0]) {
        fprintf(file, "email: %s\n", params->email);
    }
    
    fprintf(file, "description: %s\n", params->description);
    fprintf(file, "type: %s\n", module_type_name(params->type));
    fprintf(file, "library: %s.so\n", params->module_name);
    fprintf(file, "api_version: 1\n");
    fprintf(file, "auto_initialize: true\n");
    fprintf(file, "auto_activate: false\n");
    fprintf(file, "capabilities: 0x0001\n");
    
    /* Add dependencies if needed */
    if (params->include_galaxy_extension) {
        fprintf(file, "\n# Dependencies\n");
        fprintf(file, "dependencies:\n");
        fprintf(file, "  - name: core_galaxy_extensions\n");
        fprintf(file, "    min_version: 1.0.0\n");
        fprintf(file, "    required: true\n");
    }
    
    if (params->include_event_handler) {
        if (!params->include_galaxy_extension) {
            fprintf(file, "\n# Dependencies\n");
            fprintf(file, "dependencies:\n");
        }
        fprintf(file, "  - name: core_event_system\n");
        fprintf(file, "    min_version: 1.0.0\n");
        fprintf(file, "    required: true\n");
    }
    
    fclose(file);
    
    LOG_INFO("Generated module manifest file: %s", output_path);
    return 0;
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Get current date */
    get_current_date(date_str, sizeof(date_str));
    
    /* Write manifest content */
    fprintf(file, "# Manifest file for %s\n", params->module_name);
    fprintf(file, "# Generated on %s\n\n", date_str);
    
    fprintf(file, "name: %s\n", params->module_name);
    fprintf(file, "version: %s\n", params->version);
    fprintf(file, "author: %s\n", params->author);
    
    if (params->email[0]) {
        fprintf(file, "email: %s\n", params->email);
    }
    
    fprintf(file, "description: %s\n", params->description);
    fprintf(file, "type: %s\n", module_type_name(params->type));
    fprintf(file, "library: %s.so\n", params->module_name);
    fprintf(file, "api_version: 1\n");
    fprintf(file, "auto_initialize: true\n");
    fprintf(file, "auto_activate: false\n");
    
    /* Add module-specific capabilities if applicable */
    switch (params->type) {
        case MODULE_TYPE_COOLING:
            fprintf(file, "capabilities: 0x0001\n");
            break;
        case MODULE_TYPE_STAR_FORMATION:
            fprintf(file, "capabilities: 0x0002\n");
            break;
        default:
            fprintf(file, "capabilities: 0x0000\n");
            break;
    }
    
    /* Add example dependencies if applicable */
    if (params->type == MODULE_TYPE_STAR_FORMATION) {
        fprintf(file, "dependency.cooling: cooling\n");
    } else if (params->type == MODULE_TYPE_FEEDBACK) {
        fprintf(file, "dependency.star_formation: star_formation\n");
    }
    
    fclose(file);
    
    LOG_INFO("Generated module manifest file: %s", output_path);
    return 0;
}

/* Generate sample function demonstrating GALAXY_PROP_* macros usage */
static void generate_sample_prop_usage(FILE *file, enum module_type type) {
    fprintf(file, "/**\n");
    fprintf(file, " * Sample function demonstrating how to use GALAXY_PROP_* macros\n");
    fprintf(file, " * This shows the proper way to access galaxy properties in SAGE modules\n");
    fprintf(file, " */\n");
    fprintf(file, "static void demonstrate_property_usage(struct GALAXY *galaxy) {\n");
    fprintf(file, "    /* Examples of accessing scalar properties */\n");
    
    switch (type) {
        case MODULE_TYPE_COOLING:
            fprintf(file, "    if (GALAXY_PROP_HotGas(galaxy) > 0.0 && GALAXY_PROP_Vvir(galaxy) > 0.0) {\n");
            fprintf(file, "        /* Calculate cooling properties */\n");
            fprintf(file, "        double temp = 35.9 * GALAXY_PROP_Vvir(galaxy) * GALAXY_PROP_Vvir(galaxy);\n");
            fprintf(file, "        \n");
            fprintf(file, "        /* Access metals with safety check */\n");
            fprintf(file, "        double metallicity = 0.0;\n");
            fprintf(file, "        if (GALAXY_PROP_MetalsHotGas(galaxy) > 0.0) {\n");
            fprintf(file, "            metallicity = GALAXY_PROP_MetalsHotGas(galaxy) / GALAXY_PROP_HotGas(galaxy);\n");
            fprintf(file, "        }\n");
            fprintf(file, "        \n");
            fprintf(file, "        /* Update a property */\n");
            fprintf(file, "        GALAXY_PROP_Cooling(galaxy) += 0.5 * GALAXY_PROP_Vvir(galaxy) * GALAXY_PROP_Vvir(galaxy);\n");
            fprintf(file, "    }\n");
            break;
            
        case MODULE_TYPE_STAR_FORMATION:
            fprintf(file, "    if (GALAXY_PROP_ColdGas(galaxy) > 0.0) {\n");
            fprintf(file, "        /* Calculate star formation properties */\n");
            fprintf(file, "        double sfr = 0.01 * GALAXY_PROP_ColdGas(galaxy);\n");
            fprintf(file, "        \n");
            fprintf(file, "        /* Update stellar mass and reduce cold gas */\n");
            fprintf(file, "        GALAXY_PROP_StellarMass(galaxy) += sfr;\n");
            fprintf(file, "        GALAXY_PROP_ColdGas(galaxy) -= sfr;\n");
            fprintf(file, "        \n");
            fprintf(file, "        /* Handle metals */\n");
            fprintf(file, "        double metallicity = 0.0;\n");
            fprintf(file, "        if (GALAXY_PROP_ColdGas(galaxy) > 0.0) {\n");
            fprintf(file, "            metallicity = GALAXY_PROP_MetalsColdGas(galaxy) / GALAXY_PROP_ColdGas(galaxy);\n");
            fprintf(file, "            GALAXY_PROP_MetalsStellarMass(galaxy) += sfr * metallicity;\n");
            fprintf(file, "            GALAXY_PROP_MetalsColdGas(galaxy) -= sfr * metallicity;\n");
            fprintf(file, "        }\n");
            fprintf(file, "    }\n");
            break;
            
        case MODULE_TYPE_DISK_INSTABILITY:
            fprintf(file, "    /* Calculate disk instability */\n");
            fprintf(file, "    double diskMass = GALAXY_PROP_ColdGas(galaxy) + (GALAXY_PROP_StellarMass(galaxy) - GALAXY_PROP_BulgeMass(galaxy));\n");
            fprintf(file, "    \n");
            fprintf(file, "    if (diskMass > 0.0) {\n");
            fprintf(file, "        double diskRadius = GALAXY_PROP_DiskScaleRadius(galaxy);\n");
            fprintf(file, "        /* Stability calculation would go here */\n");
            fprintf(file, "        \n");
            fprintf(file, "        /* Example of updating properties */\n");
            fprintf(file, "        double unstable_stars = 0.1 * diskMass; /* Example value */\n");
            fprintf(file, "        GALAXY_PROP_BulgeMass(galaxy) += unstable_stars;\n");
            fprintf(file, "    }\n");
            break;
            
        default:
            fprintf(file, "    /* Basic property access examples */\n");
            fprintf(file, "    double stellar_mass = GALAXY_PROP_StellarMass(galaxy);\n");
            fprintf(file, "    double gas_mass = GALAXY_PROP_ColdGas(galaxy) + GALAXY_PROP_HotGas(galaxy);\n");
            fprintf(file, "    \n");
            fprintf(file, "    /* Position vector access */\n");
            fprintf(file, "    double x = GALAXY_PROP_Pos_ELEM(galaxy, 0);\n");
            fprintf(file, "    double y = GALAXY_PROP_Pos_ELEM(galaxy, 1);\n");
            fprintf(file, "    double z = GALAXY_PROP_Pos_ELEM(galaxy, 2);\n");
            break;
    }
    
    fprintf(file, "    \n");
    fprintf(file, "    /* Example of array access */\n");
    fprintf(file, "    int step = 0; /* Example step */\n");
    fprintf(file, "    \n");
    fprintf(file, "    /* Fixed-size array access (SFR history) */\n");
    fprintf(file, "    double disk_sfr = GALAXY_PROP_SfrDisk_ELEM(galaxy, step);\n");
    fprintf(file, "    double bulge_sfr = GALAXY_PROP_SfrBulge_ELEM(galaxy, step);\n");
    fprintf(file, "    \n");
    fprintf(file, "    /* Dynamic array access (if available) */\n");
    fprintf(file, "    if (GALAXY_PROP_StarFormationHistory_SIZE(galaxy) > step) {\n");
    fprintf(file, "        double sf_history = GALAXY_PROP_StarFormationHistory_ELEM(galaxy, step);\n");
    fprintf(file, "        /* Use sf_history */\n");
    fprintf(file, "    }\n");
    fprintf(file, "}\n\n");
}

/* Generate module Makefile */
int module_generate_makefile(const struct module_template_params *params, const char *output_path) {
    FILE *file;
    
    /* Validate parameters */
    if (!params || !output_path) {
        LOG_ERROR("Invalid parameters to module_generate_makefile");
        return -1;
    }
    
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Write Makefile content */
    fprintf(file, "# Makefile for %s\n\n", params->module_name);
    
    fprintf(file, "CC := gcc\n");
    fprintf(file, "CFLAGS := -fPIC -Wall -Wextra -g\n");
    fprintf(file, "LDFLAGS := -shared\n");
    fprintf(file, "SAGE_DIR := $(shell dirname $(CURDIR))\n\n");
    
    fprintf(file, "# Include paths\n");
    fprintf(file, "INCLUDES := -I$(SAGE_DIR) -I$(SAGE_DIR)/src -I$(SAGE_DIR)/src/core\n\n");
    
    fprintf(file, "# Source files\n");
    fprintf(file, "SRCS := %s.c\n", params->module_name);
    fprintf(file, "OBJS := $(SRCS:.c=.o)\n");
    fprintf(file, "TARGET := %s.so\n\n", params->module_name);
    
    fprintf(file, "# Test files\n");
    fprintf(file, "TEST_SRCS := test_%s.c\n", params->module_name);
    fprintf(file, "TEST_OBJS := $(TEST_SRCS:.c=.o)\n");
    fprintf(file, "TEST_TARGET := test_%s\n\n", params->module_name);
    
    fprintf(file, "# Default target\n");
    fprintf(file, "all: $(TARGET)\n\n");
    
    fprintf(file, "# Build shared library\n");
    fprintf(file, "$(TARGET): $(OBJS)\n");
    fprintf(file, "\t$(CC) $(LDFLAGS) -o $@ $^\n\n");
    
    fprintf(file, "# Build object files\n");
    fprintf(file, "%%.o: %%.c\n");
    fprintf(file, "\t$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@\n\n");
    
    fprintf(file, "# Build test executable\n");
    fprintf(file, "test: $(TEST_TARGET)\n\n");
    
    fprintf(file, "$(TEST_TARGET): $(TEST_OBJS) $(OBJS)\n");
    fprintf(file, "\t$(CC) -o $@ $^ -L$(SAGE_DIR) -lsage\n\n");
    
    fprintf(file, "# Install target\n");
    fprintf(file, "install: $(TARGET)\n");
    fprintf(file, "\tmkdir -p $(SAGE_DIR)/modules\n");
    fprintf(file, "\tcp $(TARGET) $(SAGE_DIR)/modules/\n");
    fprintf(file, "\tcp %s.manifest $(SAGE_DIR)/modules/\n\n", params->module_name);
    
    fprintf(file, "# Clean target\n");
    fprintf(file, "clean:\n");
    fprintf(file, "\trm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET)\n\n");
    
    fprintf(file, ".PHONY: all test install clean\n");
    
    fclose(file);
    
    LOG_INFO("Generated module Makefile: %s", output_path);
    return 0;
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Write Makefile content */
    fprintf(file, "# Makefile for %s module\n\n", params->module_name);
    
    fprintf(file, "# Compiler settings\n");
    fprintf(file, "CC ?= gcc\n");
    fprintf(file, "CFLAGS := -fPIC -shared -Wall -g -O2\n\n");
    
    fprintf(file, "# Include path for SAGE headers\n");
    fprintf(file, "SAGE_DIR ?= ../sage-model\n");
    fprintf(file, "INCLUDES := -I$(SAGE_DIR)/src -I$(SAGE_DIR)/src/core\n\n");
    
    fprintf(file, "# Module source files\n");
    fprintf(file, "SOURCES := %s.c\n", params->module_name);
    fprintf(file, "OBJECTS := $(SOURCES:.c=.o)\n");
    fprintf(file, "TARGET := %s.so\n\n", params->module_name);
    
    fprintf(file, "# Default target\n");
    fprintf(file, "all: $(TARGET)\n\n");
    
    fprintf(file, "# Build module\n");
    fprintf(file, "$(TARGET): $(OBJECTS)\n");
    fprintf(file, "\t$(CC) $(CFLAGS) -o $@ $^\n\n");
    
    fprintf(file, "# Build object files\n");
    fprintf(file, "%%.o: %%.c\n");
    fprintf(file, "\t$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@\n\n");
    
    fprintf(file, "# Clean target\n");
    fprintf(file, "clean:\n");
    fprintf(file, "\trm -f $(OBJECTS) $(TARGET)\n\n");
    
    fprintf(file, "# Install target\n");
    fprintf(file, "install: $(TARGET)\n");
    fprintf(file, "\tmkdir -p $(SAGE_DIR)/modules\n");
    fprintf(file, "\tcp $(TARGET) $(SAGE_DIR)/modules/\n");
    fprintf(file, "\tcp %s.manifest $(SAGE_DIR)/modules/\n", params->module_name);
    
    fclose(file);
    
    LOG_INFO("Generated module Makefile: %s", output_path);
    return 0;
}

/* Generate module README file */
int module_generate_readme(const struct module_template_params *params, const char *output_path) {
    FILE *file;
    char date_str[20];
    
    /* Validate parameters */
    if (!params || !output_path) {
        LOG_ERROR("Invalid parameters to module_generate_readme");
        return -1;
    }
    
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Get current date */
    get_current_date(date_str, sizeof(date_str));
    
    /* Write README content */
    fprintf(file, "# %s\n\n", params->module_name);
    fprintf(file, "%s\n\n", params->description);
    fprintf(file, "Author: %s", params->author);
    if (params->email[0]) {
        fprintf(file, " <%s>", params->email);
    }
    fprintf(file, "\n\nVersion: %s\n", params->version);
    fprintf(file, "Date: %s\n\n", date_str);
    
    fprintf(file, "## Overview\n\n");
    fprintf(file, "This module implements %s physics for the SAGE semi-analytic galaxy evolution model.\n\n", 
            module_type_name(params->type));
    
    fprintf(file, "## Implementation Details\n\n");
    fprintf(file, "Describe implementation details here.\n\n");
    
    fprintf(file, "## API Reference\n\n");
    fprintf(file, "### Functions\n\n");
    
    /* List module functions */
    char signatures[10][256];
    int num_signatures = module_get_function_signatures(params->type, signatures, 10);
    
    for (int i = 0; i < num_signatures; i++) {
        fprintf(file, "- `%s`\n", signatures[i]);
    }
    
    fprintf(file, "\n### Galaxy Property Access\n\n");
    fprintf(file, "This module uses the GALAXY_PROP_* macros to access galaxy properties. These macros provide\n");
    fprintf(file, "type-safe access to the centrally-defined galaxy properties in the SAGE model.\n\n");
    fprintf(file, "#### Examples:\n\n");
    fprintf(file, "```c\n");
    fprintf(file, "// Access basic scalar properties\n");
    fprintf(file, "double stellar_mass = GALAXY_PROP_StellarMass(galaxy);\n");
    fprintf(file, "double hot_gas = GALAXY_PROP_HotGas(galaxy);\n\n");
    
    fprintf(file, "// Update properties\n");
    fprintf(file, "GALAXY_PROP_ColdGas(galaxy) += gas_cooling_amount;\n");
    fprintf(file, "GALAXY_PROP_HotGas(galaxy) -= gas_cooling_amount;\n\n");
    
    fprintf(file, "// Access array elements (fixed-size arrays)\n");
    fprintf(file, "float disk_sfr = GALAXY_PROP_SfrDisk_ELEM(galaxy, step);\n\n");
    
    fprintf(file, "// Access dynamic arrays (with size checking)\n");
    fprintf(file, "if (step < GALAXY_PROP_StarFormationHistory_SIZE(galaxy)) {\n");
    fprintf(file, "    float history_value = GALAXY_PROP_StarFormationHistory_ELEM(galaxy, step);\n");
    fprintf(file, "}\n");
    fprintf(file, "```\n\n");
    
    fprintf(file, "#### Best Practices:\n\n");
    fprintf(file, "1. **Always use GALAXY_PROP_* macros** instead of direct field access\n");
    fprintf(file, "2. Check sizes before accessing array elements\n");
    fprintf(file, "3. For frequently accessed properties in tight loops, consider caching the value in a local variable\n");
    fprintf(file, "4. For metallicity calculations, always check if the mass is > 0 before dividing\n\n");
    
    fprintf(file, "### Module-Specific Properties\n\n");
    
    if (params->include_galaxy_extension) {
        fprintf(file, "- `%s_example_property`: Example property\n", params->module_prefix);
    } else {
        fprintf(file, "This module does not define any additional galaxy properties.\n");
    }
    
    fprintf(file, "\n## Building and Installation\n\n");
    fprintf(file, "To build and install the module:\n\n");
    fprintf(file, "```bash\n");
    fprintf(file, "# Build the module\n");
    fprintf(file, "make\n\n");
    fprintf(file, "# Install the module to SAGE's modules directory\n");
    fprintf(file, "make install\n```\n\n");
    
    fprintf(file, "## Testing\n\n");
    fprintf(file, "To build and run the tests:\n\n");
    fprintf(file, "```bash\n");
    fprintf(file, "make test\n");
    fprintf(file, "./test_%s\n```\n", params->module_name);
    
    fclose(file);
    
    LOG_INFO("Generated module README file: %s", output_path);
    return 0;
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Get current date */
    get_current_date(date_str, sizeof(date_str));
    
    /* Write README content */
    fprintf(file, "# %s\n\n", params->module_name);
    fprintf(file, "%s\n\n", params->description);
    
    fprintf(file, "## Overview\n\n");
    fprintf(file, "This module provides a custom implementation of %s for the SAGE semi-analytic model.\n\n", module_type_name(params->type));
    
    fprintf(file, "## Author\n\n");
    fprintf(file, "- **Name**: %s\n", params->author);
    
    if (params->email[0]) {
        fprintf(file, "- **Email**: %s\n", params->email);
    }
    
    fprintf(file, "\n## Version\n\n");
    fprintf(file, "- Current version: %s\n", params->version);
    fprintf(file, "- Release date: %s\n\n", date_str);
    
    fprintf(file, "## Dependencies\n\n");
    fprintf(file, "- SAGE (Semi-Analytic Galaxy Evolution model)\n");
    
    /* Add example-specific dependencies */
    switch (params->type) {
        case MODULE_TYPE_STAR_FORMATION:
            fprintf(file, "- Cooling module (dependency)\n");
            break;
        case MODULE_TYPE_FEEDBACK:
            fprintf(file, "- Star Formation module (dependency)\n");
            break;
        default:
            break;
    }
    
    fprintf(file, "\n## Building and Installation\n\n");
    fprintf(file, "```bash\n");
    fprintf(file, "# Set SAGE_DIR to the path of your SAGE installation\n");
    fprintf(file, "export SAGE_DIR=/path/to/sage-model\n\n");
    
    fprintf(file, "# Build the module\n");
    fprintf(file, "make\n\n");
    
    fprintf(file, "# Install the module to SAGE's modules directory\n");
    fprintf(file, "make install\n");
    fprintf(file, "```\n\n");
    
    fprintf(file, "## Usage\n\n");
    fprintf(file, "To use this module with SAGE, ensure the module directory is in the `ModulePath` configuration:\n\n");
    
    fprintf(file, "```\n");
    fprintf(file, "ModuleDir        ./modules     # Directory for module search\n");
    fprintf(file, "EnableModuleDiscovery 1        # Enable automatic module discovery\n");
    fprintf(file, "```\n\n");
    
    fprintf(file, "## Module Features\n\n");
    
    fprintf(file, "This module implements the following features:\n\n");
    
    fprintf(file, "- Custom %s implementation\n", module_type_name(params->type));
    
    if (params->include_galaxy_extension) {
        fprintf(file, "- Extended galaxy properties for advanced calculations\n");
    }
    
    if (params->include_event_handler) {
        fprintf(file, "- Event handlers for reactive behavior\n");
    }
    
    if (params->include_callback_registration) {
        fprintf(file, "- Callback functions for other modules to use\n");
    }
    
    fprintf(file, "\n## Configuration\n\n");
    fprintf(file, "This module uses the following configuration parameters:\n\n");
    fprintf(file, "| Parameter | Default | Description |\n");
    fprintf(file, "|-----------|---------|-------------|\n");
    fprintf(file, "| %sEnabled | 1 | Enable/disable this module |\n", params->module_prefix);
    fprintf(file, "| %sParameter1 | 0.5 | Example parameter 1 |\n", params->module_prefix);
    fprintf(file, "| %sParameter2 | 1.0 | Example parameter 2 |\n", params->module_prefix);
    
    fprintf(file, "\n## License\n\n");
    fprintf(file, "This module is provided under the same license as SAGE.\n");
    
    fclose(file);
    
    LOG_INFO("Generated module README file: %s", output_path);
    return 0;
}

/* Generate module test file */
int module_generate_test(const struct module_template_params *params, const char *output_path) {
    FILE *file;
    
    /* Validate parameters */
    if (!params || !output_path) {
        LOG_ERROR("Invalid parameters to module_generate_test");
        return -1;
    }
    
    /* Create the file */
    file = fopen(output_path, "w");
    if (!file) {
        LOG_ERROR("Failed to create output file: %s", output_path);
        return -1;
    }
    
    /* Write test file content */
    fprintf(file, "/**\n");
    fprintf(file, " * @file test_%s.c\n", params->module_name);
    fprintf(file, " * @brief Test suite for %s module\n", params->module_name);
    fprintf(file, " */\n\n");
    
    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include <stdlib.h>\n");
    fprintf(file, "#include <string.h>\n");
    fprintf(file, "#include <assert.h>\n\n");
    
    fprintf(file, "#include \"../src/core/core_module_system.h\"\n");
    fprintf(file, "#include \"../src/core/core_logging.h\"\n");
    fprintf(file, "#include \"../src/core/core_properties.h\"\n");
    fprintf(file, "#include \"%s.h\"\n\n", params->module_name);
    
    fprintf(file, "/**\n");
    fprintf(file, " * Mock parameters for testing\n");
    fprintf(file, " */\n");
    fprintf(file, "struct params test_params;\n\n");
    
    fprintf(file, "/**\n");
    fprintf(file, " * Initialize test parameters\n");
    fprintf(file, " */\n");
    fprintf(file, "void setup_test_params(void) {\n");
    fprintf(file, "    memset(&test_params, 0, sizeof(struct params));\n");
    fprintf(file, "    /* Set up any parameters needed for testing */\n");
    fprintf(file, "}\n\n");
    
    fprintf(file, "/**\n");
    fprintf(file, " * Test module initialization and cleanup\n");
    fprintf(file, " */\n");
    fprintf(file, "void test_initialize_cleanup(void) {\n");
    fprintf(file, "    printf(\"Testing initialization and cleanup...\\n\");\n\n");
    
    fprintf(file, "    /* Test initialization */\n");
    fprintf(file, "    void *module_data = NULL;\n");
    fprintf(file, "    int result = %s_initialize(&test_params, &module_data);\n", params->module_prefix);
    fprintf(file, "    assert(result == MODULE_STATUS_SUCCESS);\n");
    fprintf(file, "    assert(module_data != NULL);\n\n");
    
    fprintf(file, "    /* Test cleanup */\n");
    fprintf(file, "    result = %s_cleanup(module_data);\n", params->module_prefix);
    fprintf(file, "    assert(result == MODULE_STATUS_SUCCESS);\n\n");
    
    fprintf(file, "    printf(\"Initialization and cleanup tests passed.\\n\");\n");
    fprintf(file, "}\n\n");
    
    /* Add module-specific tests */
    fprintf(file, "/**\n");
    fprintf(file, " * Test module-specific functionality\n");
    fprintf(file, " */\n");
    fprintf(file, "void test_module_functionality(void) {\n");
    fprintf(file, "    printf(\"Testing module functionality...\\n\");\n\n");
    
    fprintf(file, "    /* Initialize module */\n");
    fprintf(file, "    void *module_data = NULL;\n");
    fprintf(file, "    int result = %s_initialize(&test_params, &module_data);\n", params->module_prefix);
    fprintf(file, "    assert(result == MODULE_STATUS_SUCCESS);\n\n");
    
    /* Add test for module-specific functions */
    fprintf(file, "    /* Test module-specific functions */\n");
    fprintf(file, "    /* Create a test galaxy with GALAXY_PROP macros */\n");
    fprintf(file, "    struct GALAXY test_galaxy;\n");
    fprintf(file, "    memset(&test_galaxy, 0, sizeof(struct GALAXY));\n");
    fprintf(file, "    \n");
    fprintf(file, "    /* Initialize property system */\n");
    fprintf(file, "    if (allocate_galaxy_properties(&test_galaxy, &test_params) != 0) {\n");
    fprintf(file, "        printf(\"Failed to allocate galaxy properties\\n\");\n");
    fprintf(file, "        return;\n");
    fprintf(file, "    }\n");
    fprintf(file, "    \n");
    fprintf(file, "    /* Set some test values */\n");
    fprintf(file, "    GALAXY_PROP_StellarMass(&test_galaxy) = 1.0;\n");
    fprintf(file, "    GALAXY_PROP_ColdGas(&test_galaxy) = 2.0;\n");
    fprintf(file, "    GALAXY_PROP_HotGas(&test_galaxy) = 5.0;\n");
    fprintf(file, "    \n");
    fprintf(file, "    /* TODO: Add your actual module function tests here */\n");
    fprintf(file, "    \n");
    fprintf(file, "    /* Clean up */\n");
    fprintf(file, "    free_galaxy_properties(&test_galaxy);\n\n");
    
    fprintf(file, "    /* Cleanup module */\n");
    fprintf(file, "    result = %s_cleanup(module_data);\n", params->module_prefix);
    fprintf(file, "    assert(result == MODULE_STATUS_SUCCESS);\n\n");
    
    fprintf(file, "    printf(\"Module functionality tests passed.\\n\");\n");
    fprintf(file, "}\n\n");
    
    if (params->include_galaxy_extension) {
        fprintf(file, "/**\n");
        fprintf(file, " * Test galaxy property extensions\n");
        fprintf(file, " */\n");
        fprintf(file, "void test_galaxy_extensions(void) {\n");
        fprintf(file, "    printf(\"Testing galaxy property extensions...\\n\");\n\n");
        
        fprintf(file, "    /* Initialize module system */\n");
        fprintf(file, "    int status = module_system_initialize();\n");
        fprintf(file, "    assert(status == MODULE_STATUS_SUCCESS);\n\n");
        
        fprintf(file, "    /* Register module */\n");
        fprintf(file, "    status = module_register(&%s_interface.base);\n", params->module_prefix);
        fprintf(file, "    assert(status == MODULE_STATUS_SUCCESS);\n\n");
        
        fprintf(file, "    /* Initialize module */\n");
        fprintf(file, "    status = module_initialize(%s_interface.base.module_id, &test_params);\n", params->module_prefix);
        fprintf(file, "    assert(status == MODULE_STATUS_SUCCESS);\n\n");
        
        fprintf(file, "    /* TODO: Test galaxy extension properties */\n\n");
        
        fprintf(file, "    /* Cleanup module */\n");
        fprintf(file, "    status = module_cleanup(%s_interface.base.module_id);\n", params->module_prefix);
        fprintf(file, "    assert(status == MODULE_STATUS_SUCCESS);\n\n");
        
        fprintf(file, "    /* Cleanup module system */\n");
        fprintf(file, "    status = module_system_cleanup();\n");
        fprintf(file, "    assert(status == MODULE_STATUS_SUCCESS);\n\n");
        
        fprintf(file, "    printf(\"Galaxy extension tests passed.\\n\");\n");
        fprintf(file, "}\n\n");
    }
    
    /* Main function */
    fprintf(file, "/**\n");
    fprintf(file, " * Main test function\n");
    fprintf(file, " */\n");
    fprintf(file, "int main(void) {\n");
    fprintf(file, "    /* Initialize logging */\n");
    fprintf(file, "    initialize_logging(NULL);\n\n");
    
    fprintf(file, "    printf(\"=== %s Module Tests ===\\n\\n\");\n\n", params->module_name);
    
    fprintf(file, "    /* Set up test environment */\n");
    fprintf(file, "    setup_test_params();\n\n");
    
    fprintf(file, "    /* Initialize property system for testing */\n");
    fprintf(file, "    if (initialize_property_system(&test_params) != 0) {\n");
    fprintf(file, "        printf(\"Failed to initialize property system\\n\");\n");
    fprintf(file, "        return -1;\n");
    fprintf(file, "    }\n\n");
    
    fprintf(file, "    /* Run tests */\n");
    fprintf(file, "    test_initialize_cleanup();\n");
    fprintf(file, "    test_module_functionality();\n");
    
    if (params->include_galaxy_extension) {
        fprintf(file, "    test_galaxy_extensions();\n");
    }
    
    fprintf(file, "\n    /* Clean up property system */\n");
    fprintf(file, "    cleanup_property_system();\n\n");
    
    fprintf(file, "    printf(\"\\nAll tests passed!\\n\");\n");
    fprintf(file, "    return 0;\n");
    fprintf(file, "}\n");
    
    fclose(file);
    
    LOG_INFO("Generated module test file: %s", output_path);
    return 0;
}

/* Generate all module template files */
int module_generate_template(const struct module_template_params *params) {
    char header_path[PATH_MAX];
    char impl_path[PATH_MAX];
    char manifest_path[PATH_MAX];
    char makefile_path[PATH_MAX];
    char readme_path[PATH_MAX];
    char test_path[PATH_MAX];
    int result;
    
    /* Validate parameters */
    if (!params) {
        LOG_ERROR("Invalid parameters to module_generate_template");
        return -1;
    }
    
    /* Create output directory if it doesn't exist */
    result = create_directory_recursive(params->output_dir);
    if (result != 0 && errno != EEXIST) {
        LOG_ERROR("Failed to create output directory: %s", params->output_dir);
        return -1;
    }
    
    /* Generate file paths */
    snprintf(header_path, sizeof(header_path), "%s/%s.h", params->output_dir, params->module_name);
    snprintf(impl_path, sizeof(impl_path), "%s/%s.c", params->output_dir, params->module_name);
    snprintf(manifest_path, sizeof(manifest_path), "%s/%s.manifest", params->output_dir, params->module_name);
    snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", params->output_dir);
    snprintf(readme_path, sizeof(readme_path), "%s/README.md", params->output_dir);
    snprintf(test_path, sizeof(test_path), "%s/test_%s.c", params->output_dir, params->module_name);
    
    /* Generate files */
    result = module_generate_header(params, header_path);
    if (result != 0) {
        LOG_ERROR("Failed to generate header file");
        return -1;
    }
    
    result = module_generate_implementation(params, impl_path);
    if (result != 0) {
        LOG_ERROR("Failed to generate implementation file");
        return -1;
    }
    
    if (params->include_manifest) {
        result = module_generate_manifest(params, manifest_path);
        if (result != 0) {
            LOG_ERROR("Failed to generate manifest file");
            return -1;
        }
    }
    
    if (params->include_makefile) {
        result = module_generate_makefile(params, makefile_path);
        if (result != 0) {
            LOG_ERROR("Failed to generate Makefile");
            return -1;
        }
    }
    
    if (params->include_readme) {
        result = module_generate_readme(params, readme_path);
        if (result != 0) {
            LOG_ERROR("Failed to generate README file");
            return -1;
        }
    }
    
    if (params->include_test_file) {
        result = module_generate_test(params, test_path);
        if (result != 0) {
            LOG_ERROR("Failed to generate test file");
            return -1;
        }
    }
    
    LOG_INFO("Successfully generated module template: %s", params->module_name);
    return 0;
}
