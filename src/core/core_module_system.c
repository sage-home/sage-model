#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core_module_system.h"
#include "core_logging.h"
#include "core_mymalloc.h"
#include "core_module_callback.h"

/* Global module registry */
struct module_registry *global_module_registry = NULL;

/* Utility function to check if a file exists */
static bool file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

/* Utility function to check if a path is a directory */
static bool is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

/* Utility function to join paths */
static void path_join(char *dest, size_t dest_size, const char *base, const char *component) {
    if (base[strlen(base) - 1] == '/') {
        snprintf(dest, dest_size, "%s%s", base, component);
    } else {
        snprintf(dest, dest_size, "%s/%s", base, component);
    }
}

/**
 * Parse a semantic version string
 * 
 * Converts a version string (e.g., "1.2.3") to a module_version structure.
 * 
 * @param version_str Version string to parse
 * @param version Output version structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_parse_version(const char *version_str, struct module_version *version) {
    if (version_str == NULL || version == NULL) {
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Initialize to zeros in case of partial version */
    version->major = 0;
    version->minor = 0;
    version->patch = 0;
    
    /* Parse the version string */
    int matched = sscanf(version_str, "%d.%d.%d", 
                        &version->major, &version->minor, &version->patch);
    
    if (matched < 1) {
        LOG_ERROR("Invalid version format: %s (expects major.minor.patch)", version_str);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Compare two semantic versions
 * 
 * Compares two version structures according to semver rules.
 * 
 * @param v1 First version to compare
 * @param v2 Second version to compare
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
int module_compare_versions(const struct module_version *v1, const struct module_version *v2) {
    if (v1 == NULL || v2 == NULL) {
        return 0;  /* Treat as equal if invalid arguments */
    }
    
    /* Compare major version */
    if (v1->major < v2->major) return -1;
    if (v1->major > v2->major) return 1;
    
    /* Major versions are equal, compare minor versions */
    if (v1->minor < v2->minor) return -1;
    if (v1->minor > v2->minor) return 1;
    
    /* Minor versions are equal, compare patch versions */
    if (v1->patch < v2->patch) return -1;
    if (v1->patch > v2->patch) return 1;
    
    /* All components are equal */
    return 0;
}

/**
 * Check version compatibility
 * 
 * Checks if a module version is compatible with required constraints.
 * 
 * @param version Version to check
 * @param min_version Minimum required version
 * @param max_version Maximum allowed version (can be NULL)
 * @param exact_match Whether an exact match is required
 * @return true if compatible, false otherwise
 */
bool module_check_version_compatibility(
    const struct module_version *version,
    const struct module_version *min_version,
    const struct module_version *max_version,
    bool exact_match) {
    
    /* Check for invalid parameters */
    if (version == NULL || min_version == NULL) {
        return false;
    }
    
    /* For exact match, compare only with min_version */
    if (exact_match) {
        return module_compare_versions(version, min_version) == 0;
    }
    
    /* Check minimum version requirement */
    if (module_compare_versions(version, min_version) < 0) {
        return false;  /* Version is less than minimum required */
    }
    
    /* Check maximum version constraint if provided */
    if (max_version != NULL && module_compare_versions(version, max_version) > 0) {
        return false;  /* Version is greater than maximum allowed */
    }
    
    return true;  /* All constraints satisfied */
}

/**
 * Get module type name
 * 
 * Returns a string representation of a module type.
 * 
 * @param type Module type
 * @return String name of the module type
 */
const char *module_type_name(enum module_type type) {
    switch (type) {
        case MODULE_TYPE_UNKNOWN:
            return "unknown";
        case MODULE_TYPE_COOLING:
            return "cooling";
        case MODULE_TYPE_STAR_FORMATION:
            return "star_formation";
        case MODULE_TYPE_FEEDBACK:
            return "feedback";
        case MODULE_TYPE_AGN:
            return "agn";
        case MODULE_TYPE_MERGERS:
            return "mergers";
        case MODULE_TYPE_DISK_INSTABILITY:
            return "disk_instability";
        case MODULE_TYPE_REINCORPORATION:
            return "reincorporation";
        case MODULE_TYPE_INFALL:
            return "infall";
        case MODULE_TYPE_MISC:
            return "misc";
        default:
            return "invalid";
    }
}

/**
 * Parse module type from string
 * 
 * Converts a string module type name to the enum value.
 * 
 * @param type_name String name of the type
 * @return Module type enum value, or MODULE_TYPE_UNKNOWN if not recognized
 */
enum module_type module_type_from_string(const char *type_name) {
    if (type_name == NULL) {
        return MODULE_TYPE_UNKNOWN;
    }
    
    /* Convert type name to lowercase for case-insensitive comparison */
    char type_lower[MAX_MODULE_NAME];
    size_t i;
    size_t len = strlen(type_name);
    
    if (len >= MAX_MODULE_NAME) {
        len = MAX_MODULE_NAME - 1;
    }
    
    for (i = 0; i < len; i++) {
        type_lower[i] = tolower(type_name[i]);
    }
    type_lower[i] = '\0';
    
    /* Compare with known type names */
    if (strcmp(type_lower, "cooling") == 0) {
        return MODULE_TYPE_COOLING;
    } else if (strcmp(type_lower, "star_formation") == 0 || 
               strcmp(type_lower, "starformation") == 0) {
        return MODULE_TYPE_STAR_FORMATION;
    } else if (strcmp(type_lower, "feedback") == 0) {
        return MODULE_TYPE_FEEDBACK;
    } else if (strcmp(type_lower, "agn") == 0) {
        return MODULE_TYPE_AGN;
    } else if (strcmp(type_lower, "mergers") == 0) {
        return MODULE_TYPE_MERGERS;
    } else if (strcmp(type_lower, "disk_instability") == 0 || 
               strcmp(type_lower, "diskinstability") == 0) {
        return MODULE_TYPE_DISK_INSTABILITY;
    } else if (strcmp(type_lower, "reincorporation") == 0) {
        return MODULE_TYPE_REINCORPORATION;
    } else if (strcmp(type_lower, "infall") == 0) {
        return MODULE_TYPE_INFALL;
    } else if (strcmp(type_lower, "misc") == 0) {
        return MODULE_TYPE_MISC;
    }
    
    return MODULE_TYPE_UNKNOWN;
}

/**
 * Load a module manifest from a file
 * 
 * Reads and parses a module manifest file (in a simple key-value format).
 * 
 * @param filename Path to the manifest file
 * @param manifest Output manifest structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_load_manifest(const char *filename, struct module_manifest *manifest) {
    if (filename == NULL || manifest == NULL) {
        LOG_ERROR("Invalid arguments to module_load_manifest");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Initialize manifest to default values */
    memset(manifest, 0, sizeof(struct module_manifest));
    manifest->type = MODULE_TYPE_UNKNOWN;
    manifest->auto_initialize = true;  /* Default to auto-initialize */
    manifest->auto_activate = false;   /* Default to not auto-activate */
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        LOG_WARNING("Failed to open manifest file: %s", filename);
        return MODULE_STATUS_ERROR;
    }
    
    char line[MAX_MANIFEST_FILE_SIZE];
    char key[MAX_MANIFEST_FILE_SIZE];
    char value[MAX_MANIFEST_FILE_SIZE];
    char *dep_parts[3];  /* For dependency parsing */
    int i;
    
    while (fgets(line, sizeof(line), file) != NULL) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        /* Remove trailing newlines */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        
        /* Trim leading/trailing whitespace from the line */
        char *start = line;
        while (*start && isspace(*start)) {
            start++;
        }
        
        if (*start == '\0') {
            continue;  /* Empty line after trimming */
        }
        
        /* Split into key-value pairs */
        if (sscanf(start, "%[^:]: %[^\n]", key, value) != 2) {
            LOG_WARNING("Invalid line in manifest: %s", line);
            continue;
        }
        
        /* Trim spaces from key and value */
        char *k = key;
        char *v = value;
        
        /* Parse based on key */
        if (strcmp(k, "name") == 0) {
            strncpy(manifest->name, v, MAX_MODULE_NAME - 1);
        } else if (strcmp(k, "version") == 0) {
            strncpy(manifest->version_str, v, MAX_MODULE_VERSION - 1);
            module_parse_version(v, &manifest->version);
        } else if (strcmp(k, "author") == 0) {
            strncpy(manifest->author, v, MAX_MODULE_AUTHOR - 1);
        } else if (strcmp(k, "description") == 0) {
            strncpy(manifest->description, v, MAX_MODULE_DESCRIPTION - 1);
        } else if (strcmp(k, "type") == 0) {
            manifest->type = module_type_from_string(v);
        } else if (strcmp(k, "library") == 0) {
            strncpy(manifest->library_path, v, MAX_MODULE_PATH - 1);
        } else if (strcmp(k, "api_version") == 0) {
            manifest->api_version = atoi(v);
        } else if (strcmp(k, "auto_initialize") == 0) {
            manifest->auto_initialize = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
        } else if (strcmp(k, "auto_activate") == 0) {
            manifest->auto_activate = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
        } else if (strcmp(k, "capabilities") == 0) {
            manifest->capabilities = strtoul(v, NULL, 16);  /* Parse as hex */
        } else if (strncmp(k, "dependency.", 11) == 0) {
            if (manifest->num_dependencies >= MAX_DEPENDENCIES) {
                LOG_WARNING("Too many dependencies in manifest: %s", filename);
                continue;
            }
            
            /* Parse dependency string: module_name[: min_version[: max_version]] */
            char *dep_str = strdup(v);
            if (dep_str == NULL) {
                continue;
            }
            
            /* Initialize parts to NULL */
            for (i = 0; i < 3; i++) {
                dep_parts[i] = NULL;
            }
            
            /* Split by colon */
            dep_parts[0] = dep_str;
            char *ptr = dep_str;
            i = 1;
            
            while (*ptr && i < 3) {
                if (*ptr == ':') {
                    *ptr = '\0';
                    ptr++;
                    dep_parts[i++] = ptr;
                } else {
                    ptr++;
                }
            }
            
            /* Set up dependency */
            struct module_dependency *dep = &manifest->dependencies[manifest->num_dependencies];
            
            if (dep_parts[0]) {
                /* Extract name and optional flags */
                bool optional = false;
                bool exact = false;
                
                /* Check for flags in square brackets */
                char *flags = strchr(dep_parts[0], '[');
                if (flags) {
                    *flags = '\0';
                    flags++;
                    
                    char *end = strchr(flags, ']');
                    if (end) {
                        *end = '\0';
                        
                        /* Parse flags */
                        if (strstr(flags, "optional")) {
                            optional = true;
                        }
                        if (strstr(flags, "exact")) {
                            exact = true;
                        }
                    }
                }
                
                /* Trim spaces */
                char *name = dep_parts[0];
                while (*name && isspace(*name)) {
                    name++;
                }
                
                char *name_end = name + strlen(name) - 1;
                while (name_end > name && isspace(*name_end)) {
                    *name_end-- = '\0';
                }
                
                strncpy(dep->name, name, MAX_DEPENDENCY_NAME - 1);
                dep->optional = optional;
                dep->exact_match = exact;
                dep->type = module_type_from_string(name);
            }
            
            /* Store version strings */
            if (dep_parts[1]) {
                strncpy(dep->min_version_str, dep_parts[1], sizeof(dep->min_version_str) - 1);
            }
            
            /* Parse max version if provided */
            if (dep_parts[2]) {
                strncpy(dep->max_version_str, dep_parts[2], sizeof(dep->max_version_str) - 1);
            }
            
            free(dep_str);
            manifest->num_dependencies++;
        }
    }
    
    fclose(file);
    
    /* Validate the manifest */
    if (!module_validate_manifest(manifest)) {
        LOG_ERROR("Invalid manifest: %s", filename);
        return MODULE_STATUS_INVALID_MANIFEST;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Validate a module manifest
 * 
 * Checks that a manifest structure is valid and correctly formed.
 * 
 * @param manifest Manifest to validate
 * @return true if valid, false otherwise
 */
bool module_validate_manifest(const struct module_manifest *manifest) {
    if (manifest == NULL) {
        LOG_ERROR("NULL manifest pointer");
        return false;
    }
    
    /* Check required fields */
    if (manifest->name[0] == '\0') {
        LOG_ERROR("Module name cannot be empty");
        return false;
    }
    
    if (manifest->version_str[0] == '\0') {
        LOG_ERROR("Module version cannot be empty");
        return false;
    }
    
    if (manifest->type <= MODULE_TYPE_UNKNOWN || manifest->type >= MODULE_TYPE_MAX) {
        LOG_ERROR("Invalid module type: %d", manifest->type);
        return false;
    }
    
    if (manifest->library_path[0] == '\0') {
        LOG_ERROR("Module library path cannot be empty");
        return false;
    }
    
    /* API version must be valid */
    if (manifest->api_version <= 0) {
        LOG_ERROR("Invalid API version: %d", manifest->api_version);
        return false;
    }
    
    return true;
}

/**
 * Initialize the module system
 * 
 * Sets up the global module registry and prepares it for module registration.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_initialize(void) {
    /* Check if already initialized */
    if (global_module_registry != NULL) {
        LOG_WARNING("Module system already initialized");
        return MODULE_STATUS_ALREADY_INITIALIZED;
    }
    
    /* Allocate memory for the registry */
    global_module_registry = (struct module_registry *)mymalloc(sizeof(struct module_registry));
    if (global_module_registry == NULL) {
        LOG_ERROR("Failed to allocate memory for module registry");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize all registry fields to zero */
    memset(global_module_registry, 0, sizeof(struct module_registry));
    
    /* Initialize type mapping */
    for (int i = 0; i < MODULE_TYPE_MAX; i++) {
        global_module_registry->active_modules[i].type = MODULE_TYPE_UNKNOWN;
        global_module_registry->active_modules[i].module_index = -1;
    }
    
    /* Default configuration */
    global_module_registry->discovery_enabled = false;
    
    /* Initialize the module callback system */
    int status = module_callback_system_initialize();
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize module callback system");
        myfree(global_module_registry);
        global_module_registry = NULL;
        return status;
    }
    
    LOG_INFO("Module system initialized");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Configure the module system
 * 
 * Sets up the module system with the given parameters.
 * 
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_configure(struct params *params) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    if (params == NULL) {
        LOG_WARNING("NULL parameters pointer, using default configuration");
        /* Configure with defaults instead of failing */
        global_module_registry->discovery_enabled = false;
        
        /* Add default module directory */
        char default_path[MAX_MODULE_PATH];
        snprintf(default_path, MAX_MODULE_PATH, "./%s", DEFAULT_MODULE_DIR);
        module_add_search_path(default_path);
        
        LOG_INFO("Module system configured with defaults: %d search paths, discovery disabled",
                 global_module_registry->num_module_paths);
                 
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Configure discovery - default to false if not set */
    global_module_registry->discovery_enabled = params->runtime.EnableModuleDiscovery != 0;
    
    /* Add module directory if specified */
    if (params->runtime.ModuleDir[0] != '\0') {
        module_add_search_path(params->runtime.ModuleDir);
    } else {
        /* Add default directory relative to binary */
        char default_path[MAX_MODULE_PATH];
        snprintf(default_path, MAX_MODULE_PATH, "./%s", DEFAULT_MODULE_DIR);
        module_add_search_path(default_path);
    }
    
    /* Add additional module paths if specified */
    if (params->runtime.NumModulePaths > 0) {
        for (int i = 0; i < params->runtime.NumModulePaths && i < MAX_MODULE_PATHS; i++) {
            if (params->runtime.ModulePaths[i][0] != '\0') {
                module_add_search_path(params->runtime.ModulePaths[i]);
            }
        }
    }
    
    LOG_INFO("Module system configured: %d search paths, discovery %s",
             global_module_registry->num_module_paths,
             global_module_registry->discovery_enabled ? "enabled" : "disabled");
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Add a module search path
 * 
 * Adds a directory to the list of places to search for modules.
 * 
 * @param path Directory path to add
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_add_search_path(const char *path) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    if (path == NULL || path[0] == '\0') {
        LOG_ERROR("Empty module path");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if we have room for another path */
    if (global_module_registry->num_module_paths >= MAX_MODULE_PATHS) {
        LOG_ERROR("Too many module paths (max %d)", MAX_MODULE_PATHS);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if directory exists */
    if (!is_directory(path)) {
        LOG_WARNING("Module path is not a directory: %s", path);
        /* Continue anyway - directory might be created later */
    }
    
    /* Check if path is already in the list */
    for (int i = 0; i < global_module_registry->num_module_paths; i++) {
        if (strcmp(global_module_registry->module_paths[i], path) == 0) {
            LOG_INFO("Module path already added: %s", path);
            return MODULE_STATUS_SUCCESS;
        }
    }
    
    /* Add the path */
    strncpy(global_module_registry->module_paths[global_module_registry->num_module_paths],
            path, MAX_MODULE_PATH - 1);
    global_module_registry->num_module_paths++;
    
    LOG_INFO("Added module search path: %s", path);
    return MODULE_STATUS_SUCCESS;
}

/**
 * Find a module by name
 * 
 * Searches the registry for a module with the given name.
 * 
 * @param name Name of the module to find
 * @return Module index if found, -1 if not found
 */
int module_find_by_name(const char *name) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return -1;
    }
    
    if (name == NULL) {
        LOG_ERROR("NULL module name");
        return -1;
    }
    
    /* Search for the module */
    for (int i = 0; i < global_module_registry->num_modules; i++) {
        if (global_module_registry->modules[i].module != NULL &&
            strcmp(global_module_registry->modules[i].module->name, name) == 0) {
            return i;
        }
    }
    
    return -1;  /* Not found */
}

/**
 * Check and resolve module dependencies
 * 
 * Validates that all dependencies of a module are satisfied.
 * 
 * @param manifest Manifest containing the dependencies
 * @return MODULE_STATUS_SUCCESS if all dependencies are met, error code otherwise
 */
int module_check_dependencies(const struct module_manifest *manifest) {
    if (manifest == NULL) {
        LOG_ERROR("NULL manifest pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    if (manifest->num_dependencies == 0) {
        return MODULE_STATUS_SUCCESS;  /* No dependencies to check */
    }
    
    /* Check each dependency */
    for (int i = 0; i < manifest->num_dependencies; i++) {
        const struct module_dependency *dep = &manifest->dependencies[i];
        
        /* Find module by name */
        int module_idx = module_find_by_name(dep->name);
        if (module_idx < 0) {
            /* Dependency not found */
            if (dep->optional) {
                LOG_INFO("Optional dependency not found: %s", dep->name);
                continue;
            } else {
                LOG_ERROR("Required dependency not found: %s", dep->name);
                return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
            }
        }
        
        /* Get the module */
        struct base_module *module = global_module_registry->modules[module_idx].module;
        if (module == NULL) {
            LOG_ERROR("NULL module pointer for %s", dep->name);
            return MODULE_STATUS_ERROR;
        }
        
        /* Check module type */
        if (dep->type != MODULE_TYPE_UNKNOWN && module->type != dep->type) {
            LOG_ERROR("Dependency %s has wrong type: expected %s, got %s",
                     dep->name, module_type_name(dep->type), module_type_name(module->type));
            return MODULE_STATUS_DEPENDENCY_CONFLICT;
        }
        
        /* Check version compatibility using string-based approach */
        if (dep->min_version_str[0] != '\0') {
            struct module_version dep_min_version;
            struct module_version module_version;
            
            /* Parse both versions */
            if (module_parse_version(dep->min_version_str, &dep_min_version) == MODULE_STATUS_SUCCESS &&
                module_parse_version(module->version, &module_version) == MODULE_STATUS_SUCCESS) {
                
                /* Check minimum version requirement */
                if (module_compare_versions(&module_version, &dep_min_version) < 0) {
                    LOG_ERROR("Dependency %s version conflict: %s has version %s, but minimum %s is required",
                             dep->name, module->name, module->version, dep->min_version_str);
                    return MODULE_STATUS_DEPENDENCY_CONFLICT;
                }
                
                /* Check maximum version constraint if specified */
                if (dep->max_version_str[0] != '\0') {
                    struct module_version dep_max_version;
                    if (module_parse_version(dep->max_version_str, &dep_max_version) == MODULE_STATUS_SUCCESS) {
                        if (module_compare_versions(&module_version, &dep_max_version) > 0) {
                            LOG_ERROR("Dependency %s version conflict: %s has version %s, but maximum %s is allowed",
                                     dep->name, module->name, module->version, dep->max_version_str);
                            return MODULE_STATUS_DEPENDENCY_CONFLICT;
                        }
                    }
                }
                
                /* Check for exact match if required */
                if (dep->exact_match && strcmp(module->version, dep->min_version_str) != 0) {
                    LOG_ERROR("Dependency %s version conflict: %s has version %s, but exact version %s is required",
                             dep->name, module->name, module->version, dep->min_version_str);
                    return MODULE_STATUS_DEPENDENCY_CONFLICT;
                }
            }
        }
        
        LOG_DEBUG("Dependency %s satisfied by module %s version %s",
                 dep->name, module->name, module->version);
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up the module system
 * 
 * Releases resources used by the module system and unregisters all modules.
 * 
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_system_cleanup(void) {
    if (global_module_registry == NULL) {
        LOG_WARNING("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Cleanup all initialized modules */
    for (int i = 0; i < global_module_registry->num_modules; i++) {
        if (global_module_registry->modules[i].initialized) {
            module_cleanup(i);
        }
        
        /* Free function registry if allocated */
        struct base_module *module = global_module_registry->modules[i].module;
        if (module != NULL) {
            if (module->function_registry != NULL) {
                myfree(module->function_registry);
                module->function_registry = NULL;
            }
            
            if (module->dependencies != NULL) {
                myfree(module->dependencies);
                module->dependencies = NULL;
                module->num_dependencies = 0;
            }
        }
        
        /* If this is a dynamically loaded module, unload it */
        if (global_module_registry->modules[i].dynamic && 
            global_module_registry->modules[i].handle != NULL) {
            dlclose(global_module_registry->modules[i].handle);
            global_module_registry->modules[i].handle = NULL;
        }
    }
    
    /* Clean up the module callback system */
    module_callback_system_cleanup();
    
    /* Free the registry */
    myfree(global_module_registry);
    global_module_registry = NULL;
    
    LOG_INFO("Module system cleaned up");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Discover modules in search paths
 * 
 * Searches the configured paths for module manifests and loads them.
 * 
 * @param params Pointer to the global parameters structure
 * @return Number of modules discovered, or negative error code on failure
 */
int module_discover(struct params *params) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    if (!global_module_registry->discovery_enabled) {
        LOG_DEBUG("Module discovery is disabled");
        return 0;
    }
    
    if (params == NULL) {
        LOG_ERROR("NULL parameters pointer for module discovery");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if we have any search paths configured */
    if (global_module_registry->num_module_paths <= 0) {
        LOG_WARNING("No module search paths configured, skipping discovery");
        return 0;
    }
    
    int modules_discovered = 0;
    
    /* Search each path for modules */
    for (int i = 0; i < global_module_registry->num_module_paths; i++) {
        /* Skip invalid paths */
        if (i >= MAX_MODULE_PATHS) {
            LOG_WARNING("Module path index out of bounds: %d", i);
            break;
        }
        
        const char *path = global_module_registry->module_paths[i];
        
        /* Skip empty paths */
        if (path == NULL || path[0] == '\0') {
            continue;
        }
        
        LOG_DEBUG("Searching for modules in: %s", path);
        
        /* Check if directory exists */
        if (!is_directory(path)) {
            LOG_WARNING("Module path is not a directory: %s", path);
            continue;
        }
        
        DIR *dir = opendir(path);
        if (dir == NULL) {
            LOG_WARNING("Failed to open module directory: %s", path);
            continue;
        }
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            /* Skip hidden files and special directories */
            if (entry->d_name[0] == '.') {
                continue;
            }
            
            /* Check if this is a directory */
            char module_path[MAX_MODULE_PATH];
            path_join(module_path, sizeof(module_path), path, entry->d_name);
            
            if (!is_directory(module_path)) {
                continue;
            }
            
            /* Look for a manifest file */
            char manifest_path[MAX_MODULE_PATH];
            path_join(manifest_path, sizeof(manifest_path), module_path, "module.manifest");
            
            if (!file_exists(manifest_path)) {
                continue;
            }
            
            LOG_INFO("Found module manifest: %s", manifest_path);
            
            /* Load the manifest */
            struct module_manifest manifest;
            int status = module_load_manifest(manifest_path, &manifest);
            if (status != MODULE_STATUS_SUCCESS) {
                LOG_WARNING("Failed to load manifest: %s, status = %d", manifest_path, status);
                continue;
            }
            
            /* Update library path to be relative to the module directory */
            char full_library_path[MAX_MODULE_PATH];
            path_join(full_library_path, sizeof(full_library_path), module_path, manifest.library_path);
            strncpy(manifest.library_path, full_library_path, MAX_MODULE_PATH - 1);
            
            /* Check if module with same name already exists */
            if (module_find_by_name(manifest.name) >= 0) {
                LOG_WARNING("Module with name '%s' already exists, skipping", manifest.name);
                continue;
            }
            
            /* Check dependencies */
            status = module_check_dependencies(&manifest);
            if (status != MODULE_STATUS_SUCCESS) {
                LOG_WARNING("Dependency check failed for module '%s', status = %d", manifest.name, status);
                continue;
            }
            
            /* Load the module */
            LOG_INFO("Loading module '%s' from %s", manifest.name, manifest.library_path);
            
            /* Attempt to load the module */
            int module_id = module_load_from_manifest(&manifest, params);
            if (module_id >= 0) {
                LOG_INFO("Successfully loaded module '%s' with ID %d", manifest.name, module_id);
                modules_discovered++;
            } else {
                LOG_ERROR("Failed to load module '%s', status = %d", manifest.name, module_id);
            }
        }
        
        closedir(dir);
    }
    
    LOG_INFO("Module discovery complete: %d modules found", modules_discovered);
    return modules_discovered;
}

/**
 * Dynamic library loading functionality
 * 
 * Platform-independent functions for loading dynamic libraries
 */

/**
 * Load a dynamic library
 * 
 * @param library_path Path to the library file
 * @return Handle to the loaded library, or NULL on failure
 */
static module_library_handle_t load_library(const char *library_path) {
    if (library_path == NULL || library_path[0] == '\0') {
        LOG_ERROR("Empty library path");
        return NULL;
    }
    
    /* Check if library file exists */
    if (!file_exists(library_path)) {
        LOG_ERROR("Library file not found: %s", library_path);
        return NULL;
    }
    
    /* Load the library */
    module_library_handle_t handle = dlopen(library_path, RTLD_NOW);
    if (handle == NULL) {
        LOG_ERROR("Failed to load library: %s", dlerror());
        return NULL;
    }
    
    return handle;
}

/**
 * Get a symbol from a loaded library
 * 
 * @param handle Handle to the loaded library
 * @param symbol_name Name of the symbol to get
 * @return Pointer to the symbol, or NULL on failure
 */
static void *get_library_symbol(module_library_handle_t handle, const char *symbol_name) {
    if (handle == NULL) {
        LOG_ERROR("NULL library handle");
        return NULL;
    }
    
    if (symbol_name == NULL || symbol_name[0] == '\0') {
        LOG_ERROR("Empty symbol name");
        return NULL;
    }
    
    void *symbol = dlsym(handle, symbol_name);
    if (symbol == NULL) {
        LOG_ERROR("Failed to get symbol '%s': %s", symbol_name, dlerror());
        return NULL;
    }
    
    return symbol;
}

/**
 * Unload a dynamic library
 * 
 * @param handle Handle to the loaded library
 * @return 0 on success, error code on failure
 */
static int unload_library(module_library_handle_t handle) {
    if (handle == NULL) {
        LOG_ERROR("NULL library handle");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    int status = dlclose(handle);
    if (status != 0) {
        LOG_ERROR("Failed to unload library: %s", dlerror());
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Load a module from a manifest
 * 
 * Loads a module based on its manifest information.
 * 
 * @param manifest Manifest describing the module
 * @param params Pointer to the global parameters structure
 * @return Module ID if successful, negative error code on failure
 */
int module_load_from_manifest(const struct module_manifest *manifest, struct params *params) {
    if (manifest == NULL) {
        LOG_ERROR("NULL manifest pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    if (params == NULL) {
        LOG_ERROR("NULL parameters pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if library file exists */
    if (!file_exists(manifest->library_path)) {
        LOG_ERROR("Module library not found: %s", manifest->library_path);
        return MODULE_STATUS_MODULE_NOT_FOUND;
    }
    
    /* Open the library */
    module_library_handle_t handle = load_library(manifest->library_path);
    if (handle == NULL) {
        return MODULE_STATUS_MODULE_LOADING_FAILED;
    }
    
    /* Get module creation function */
    typedef struct base_module *(*create_module_func_t)(void);
    create_module_func_t create_module = (create_module_func_t)get_library_symbol(handle, "create_module");
    
    if (create_module == NULL) {
        unload_library(handle);
        return MODULE_STATUS_MODULE_LOADING_FAILED;
    }
    
    /* Create the module */
    struct base_module *module = create_module();
    if (module == NULL) {
        LOG_ERROR("Failed to create module from library");
        unload_library(handle);
        return MODULE_STATUS_MODULE_LOADING_FAILED;
    }
    
    /* Check module type */
    if (module->type != manifest->type) {
        LOG_ERROR("Module type mismatch: manifest has type %s, module reports type %s",
                 module_type_name(manifest->type), module_type_name(module->type));
        unload_library(handle);
        /* We can't call free on the module as it's managed by the library */
        return MODULE_STATUS_INVALID_MANIFEST;
    }
    
    /* Verify name and version */
    if (strcmp(module->name, manifest->name) != 0) {
        LOG_WARNING("Module name mismatch: manifest has name '%s', module reports name '%s'",
                   manifest->name, module->name);
        /* Not a critical error, just a warning */
    }
    
    if (strcmp(module->version, manifest->version_str) != 0) {
        LOG_WARNING("Module version mismatch: manifest has version '%s', module reports version '%s'",
                   manifest->version_str, module->version);
        /* Not a critical error, just a warning */
    }
    
    /* Create and copy the manifest */
    struct module_manifest *mod_manifest = (struct module_manifest *)mymalloc(sizeof(struct module_manifest));
    if (mod_manifest == NULL) {
        LOG_ERROR("Failed to allocate memory for module manifest");
        unload_library(handle);
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    memcpy(mod_manifest, manifest, sizeof(struct module_manifest));
    module->manifest = mod_manifest;
    
    /* Register the module */
    int status = module_register(module);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register module, status = %d", status);
        myfree(mod_manifest);
        unload_library(handle);
        return status;
    }
    
    /* Get the module ID */
    int module_id = module->module_id;
    
    /* Set as dynamic module */
    global_module_registry->modules[module_id].dynamic = true;
    global_module_registry->modules[module_id].handle = handle;
    strncpy(global_module_registry->modules[module_id].path, manifest->library_path, MAX_MODULE_PATH - 1);
    
    /* Initialize if auto-initialize is set */
    if (manifest->auto_initialize) {
        status = module_initialize(module_id, params);
        if (status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Failed to initialize module '%s', status = %d", module->name, status);
            module_unregister(module_id);
            return status;
        }
    }
    
    /* Activate if auto-activate is set */
    if (manifest->auto_activate && global_module_registry->modules[module_id].initialized) {
        status = module_set_active(module_id);
        if (status != MODULE_STATUS_SUCCESS) {
            LOG_ERROR("Failed to activate module '%s', status = %d", module->name, status);
            /* Continue anyway - module is still loaded */
        }
    }
    
    LOG_INFO("Successfully loaded module '%s' from %s", module->name, manifest->library_path);
    return module_id;
}

/**
 * Register a module with the system
 * 
 * Adds a module to the global registry and assigns it a unique ID.
 * 
 * @param module Pointer to the module interface to register
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_register(struct base_module *module) {
    if (global_module_registry == NULL) {
        /* Auto-initialize if not done already */
        int status = module_system_initialize();
        if (status != MODULE_STATUS_SUCCESS) {
            return status;
        }
    }

    /* Validate module */
    if (!module_validate(module)) {
        LOG_ERROR("Invalid module interface provided");
        return MODULE_STATUS_INVALID_ARGS;
    }

    /* Check for space in registry */
    if (global_module_registry->num_modules >= MAX_MODULES) {
        LOG_ERROR("Module registry is full (max %d modules)", MAX_MODULES);
        return MODULE_STATUS_ERROR;
    }

    /* Check if module with the same name already exists */
    for (int i = 0; i < global_module_registry->num_modules; i++) {
        if (global_module_registry->modules[i].module != NULL &&
            strcmp(global_module_registry->modules[i].module->name, module->name) == 0) {
            LOG_ERROR("Module with name '%s' already registered", module->name);
            return MODULE_STATUS_ERROR;
        }
    }

    /* Find an available slot */
    int module_id = global_module_registry->num_modules;

    /* Initialize module callback fields */
    module->function_registry = NULL;
    module->dependencies = NULL;
    module->num_dependencies = 0;

    /* Copy the module interface */
    global_module_registry->modules[module_id].module = module;
    global_module_registry->modules[module_id].module_data = NULL;
    global_module_registry->modules[module_id].initialized = false;
    global_module_registry->modules[module_id].active = false;
    global_module_registry->modules[module_id].dynamic = false;
    global_module_registry->modules[module_id].handle = NULL;
    global_module_registry->modules[module_id].path[0] = '\0';

    /* Assign ID to module */
    module->module_id = module_id;
    
    /* Increment module count */
    global_module_registry->num_modules++;
    
    LOG_INFO("Registered module '%s' (type %d) with ID %d", 
             module->name, module->type, module_id);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Unregister a module from the system
 * 
 * Removes a module from the global registry.
 * 
 * @param module_id ID of the module to unregister
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_unregister(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Clean up module if initialized */
    if (global_module_registry->modules[module_id].initialized) {
        module_cleanup(module_id);
    }
    
    /* If the module is active, deactivate it */
    if (global_module_registry->modules[module_id].active) {
        enum module_type type = global_module_registry->modules[module_id].module->type;
        
        /* Find and remove the active module entry */
        for (int i = 0; i < global_module_registry->num_active_types; i++) {
            if (global_module_registry->active_modules[i].type == type) {
                /* Shift remaining entries down */
                for (int j = i; j < global_module_registry->num_active_types - 1; j++) {
                    global_module_registry->active_modules[j] = global_module_registry->active_modules[j + 1];
                }
                global_module_registry->num_active_types--;
                break;
            }
        }
    }
    
    /* Remove the module entry */
    struct base_module *module = global_module_registry->modules[module_id].module;
    const char *module_name = module->name;
    enum module_type module_type = module->type;
    
    /* Clear the module entry */
    global_module_registry->modules[module_id].module = NULL;
    global_module_registry->modules[module_id].module_data = NULL;
    global_module_registry->modules[module_id].initialized = false;
    global_module_registry->modules[module_id].active = false;
    
    /* We don't reduce num_modules or reorganize the array to keep IDs stable */
    
    LOG_INFO("Unregistered module '%s' (type %d) with ID %d", 
             module_name, module_type, module_id);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Initialize a module
 * 
 * Calls the initialize function of a registered module.
 * 
 * @param module_id ID of the module to initialize
 * @param params Pointer to the global parameters structure
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_initialize(int module_id, struct params *params) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if already initialized */
    if (global_module_registry->modules[module_id].initialized) {
        LOG_WARNING("Module ID %d already initialized", module_id);
        return MODULE_STATUS_ALREADY_INITIALIZED;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("Module ID %d has NULL interface", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if initialization function exists */
    if (module->initialize == NULL) {
        LOG_ERROR("Module '%s' (ID %d) has no initialize function", 
                 module->name, module_id);
        return MODULE_STATUS_NOT_IMPLEMENTED;
    }
    
    /* Call module initialize function */
    void *module_data = NULL;
    int status = module->initialize(params, &module_data);
    
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to initialize module '%s' (ID %d): %d", 
                 module->name, module_id, status);
        return status;
    }
    
    /* Store module data */
    global_module_registry->modules[module_id].module_data = module_data;
    global_module_registry->modules[module_id].initialized = true;
    
    LOG_INFO("Initialized module '%s' (ID %d)", module->name, module_id);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up a module
 * 
 * Calls the cleanup function of a registered module.
 * 
 * @param module_id ID of the module to clean up
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_cleanup(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if initialized */
    if (!global_module_registry->modules[module_id].initialized) {
        LOG_WARNING("Module ID %d not initialized", module_id);
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("Module ID %d has NULL interface", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if cleanup function exists */
    if (module->cleanup == NULL) {
        LOG_WARNING("Module '%s' (ID %d) has no cleanup function", 
                   module->name, module_id);
        /* Not having a cleanup function is acceptable */
        global_module_registry->modules[module_id].initialized = false;
        global_module_registry->modules[module_id].module_data = NULL;
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Call module cleanup function */
    void *module_data = global_module_registry->modules[module_id].module_data;
    int status = module->cleanup(module_data);
    
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to clean up module '%s' (ID %d): %d", 
                 module->name, module_id, status);
        return status;
    }
    
    /* Clear module data */
    global_module_registry->modules[module_id].module_data = NULL;
    global_module_registry->modules[module_id].initialized = false;
    
    LOG_INFO("Cleaned up module '%s' (ID %d)", module->name, module_id);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get a module by ID
 * 
 * Retrieves a pointer to a module's interface and data.
 * 
 * @param module_id ID of the module to retrieve
 * @param module Output pointer to the module interface
 * @param module_data Output pointer to the module's data
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get(int module_id, struct base_module **module, void **module_data) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if module exists */
    if (global_module_registry->modules[module_id].module == NULL) {
        LOG_ERROR("No module registered at ID %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Set output pointers */
    if (module != NULL) {
        *module = global_module_registry->modules[module_id].module;
    }
    
    if (module_data != NULL) {
        *module_data = global_module_registry->modules[module_id].module_data;
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get active module by type
 * 
 * Retrieves the active module of a specific type.
 * 
 * @param type Type of module to retrieve
 * @param module Output pointer to the module interface
 * @param module_data Output pointer to the module's data
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_get_active_by_type(enum module_type type, struct base_module **module, void **module_data) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Find active module of the requested type */
    for (int i = 0; i < global_module_registry->num_active_types; i++) {
        if (global_module_registry->active_modules[i].type == type) {
            int module_index = global_module_registry->active_modules[i].module_index;
            
            /* Set output pointers */
            if (module != NULL) {
                *module = global_module_registry->modules[module_index].module;
            }
            
            if (module_data != NULL) {
                *module_data = global_module_registry->modules[module_index].module_data;
            }
            
            return MODULE_STATUS_SUCCESS;
        }
    }
    
    /* During Phase 2.5-2.6 development, we're less verbose */
    static bool already_logged_type_errors[MODULE_TYPE_MAX] = {false};
    
    if (!already_logged_type_errors[type]) {
        LOG_DEBUG("No active module found for type %d (%s)", type, module_type_name(type));
        already_logged_type_errors[type] = true;
    }
    
    return MODULE_STATUS_ERROR;
}

/**
 * Validate a base module
 * 
 * Checks that a module interface is valid and properly formed.
 * 
 * @param module Pointer to the module interface to validate
 * @return true if the module is valid, false otherwise
 */
bool module_validate(const struct base_module *module) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer");
        return false;
    }

    /* Check required fields */
    if (module->name[0] == '\0') {
        LOG_ERROR("Module name cannot be empty");
        return false;
    }

    if (module->version[0] == '\0') {
        LOG_ERROR("Module version cannot be empty");
        return false;
    }

    if (module->type <= MODULE_TYPE_UNKNOWN || module->type >= MODULE_TYPE_MAX) {
        LOG_ERROR("Invalid module type: %d", module->type);
        return false;
    }

    /* Check required functions */
    if (module->initialize == NULL) {
        LOG_ERROR("Module '%s' missing initialize function", module->name);
        return false;
    }

    /* cleanup can be NULL if the module doesn't need cleanup */

    /* Validate dependency array if present */
    if (module->dependencies != NULL && module->num_dependencies <= 0) {
        LOG_WARNING("Module '%s' has dependencies array but num_dependencies is %d",
                  module->name, module->num_dependencies);
    }

    /* Validate function registry if present */
    if (module->function_registry != NULL && 
        (module->function_registry->num_functions < 0 || 
         module->function_registry->num_functions > MAX_MODULE_FUNCTIONS)) {
        LOG_ERROR("Module '%s' has invalid function registry (num_functions: %d)",
                module->name, module->function_registry->num_functions);
        return false;
    }

    return true;
}

/**
 * Validate module runtime dependencies
 * 
 * Checks that all dependencies of a module are satisfied at runtime.
 * 
 * @param module_id ID of the module to validate
 * @return MODULE_STATUS_SUCCESS if all dependencies are met, error code otherwise
 */
int module_validate_runtime_dependencies(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("NULL module pointer for ID %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Skip if no dependencies */
    if (module->dependencies == NULL || module->num_dependencies == 0) {
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Check each dependency */
    for (int i = 0; i < module->num_dependencies; i++) {
        const module_dependency_t *dep = &module->dependencies[i];
        
        /* If this is a type-based dependency, check for active module of this type */
        if (dep->type != MODULE_TYPE_UNKNOWN && dep->name[0] == '\0') {
            /* Find active module of the specified type */
            int active_index = -1;
            for (int j = 0; j < global_module_registry->num_active_types; j++) {
                if (global_module_registry->active_modules[j].type == dep->type) {
                    active_index = global_module_registry->active_modules[j].module_index;
                    break;
                }
            }
            
            if (active_index < 0) {
                if (dep->optional) {
                    LOG_INFO("Optional dependency on type %s not satisfied - no active module",
                             module_type_name(dep->type));
                    continue;
                } else {
                    LOG_ERROR("Required dependency on type %s not satisfied - no active module",
                             module_type_name(dep->type));
                    return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
                }
            }
            
            /* Get the active module */
            struct base_module *active_module = global_module_registry->modules[active_index].module;
            if (active_module == NULL) {
                LOG_ERROR("NULL module pointer for active module of type %s",
                         module_type_name(dep->type));
                return MODULE_STATUS_ERROR;
            }
            
            /* Check version compatibility using string-based approach */
            if (dep->min_version_str[0] != '\0') {
                struct module_version dep_min_version;
                struct module_version active_version;
                
                /* Parse both versions */
                if (module_parse_version(dep->min_version_str, &dep_min_version) == MODULE_STATUS_SUCCESS &&
                    module_parse_version(active_module->version, &active_version) == MODULE_STATUS_SUCCESS) {
                    
                    /* Check minimum version requirement */
                    if (module_compare_versions(&active_version, &dep_min_version) < 0) {
                        LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but minimum %s is required",
                                 module_type_name(dep->type), active_module->name, 
                                 active_module->version, dep->min_version_str);
                        return MODULE_STATUS_DEPENDENCY_CONFLICT;
                    }
                    
                    /* Check maximum version constraint if specified */
                    if (dep->max_version_str[0] != '\0') {
                        struct module_version dep_max_version;
                        if (module_parse_version(dep->max_version_str, &dep_max_version) == MODULE_STATUS_SUCCESS) {
                            if (module_compare_versions(&active_version, &dep_max_version) > 0) {
                                LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but maximum %s is allowed",
                                         module_type_name(dep->type), active_module->name, 
                                         active_module->version, dep->max_version_str);
                                return MODULE_STATUS_DEPENDENCY_CONFLICT;
                            }
                        }
                    }
                    
                    /* Check for exact match if required */
                    if (dep->exact_match && strcmp(active_module->version, dep->min_version_str) != 0) {
                        LOG_ERROR("Dependency on type %s version conflict: %s has version %s, but exact version %s is required",
                                 module_type_name(dep->type), active_module->name, 
                                 active_module->version, dep->min_version_str);
                        return MODULE_STATUS_DEPENDENCY_CONFLICT;
                    }
                }
            }
            
            LOG_DEBUG("Dependency on type %s satisfied by active module %s version %s",
                     module_type_name(dep->type), active_module->name, active_module->version);
        }
        /* If this is a name-based dependency */
        else if (dep->name[0] != '\0') {
            /* Find module by name */
            int module_idx = module_find_by_name(dep->name);
            if (module_idx < 0) {
                if (dep->optional) {
                    LOG_INFO("Optional dependency on named module %s not satisfied - module not found",
                             dep->name);
                    continue;
                } else {
                    LOG_ERROR("Required dependency on named module %s not satisfied - module not found",
                             dep->name);
                    return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
                }
            }
            
            /* Check if dependency is active */
            if (!global_module_registry->modules[module_idx].active) {
                if (dep->optional) {
                    LOG_INFO("Optional dependency on named module %s not satisfied - module not active",
                             dep->name);
                    continue;
                } else {
                    LOG_ERROR("Required dependency on named module %s not satisfied - module not active",
                             dep->name);
                    return MODULE_STATUS_DEPENDENCY_NOT_FOUND;
                }
            }
            
            /* Check type if specified */
            if (dep->type != MODULE_TYPE_UNKNOWN) {
                struct base_module *dep_module = global_module_registry->modules[module_idx].module;
                if (dep_module->type != dep->type) {
                    LOG_ERROR("Dependency on named module %s has wrong type: expected %s, got %s",
                             dep->name, module_type_name(dep->type), module_type_name(dep_module->type));
                    return MODULE_STATUS_DEPENDENCY_CONFLICT;
                }
            }
            
            LOG_DEBUG("Dependency on named module %s satisfied", dep->name);
        }
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Set a module as active
 * 
 * Marks a module as the active implementation for its type.
 * 
 * @param module_id ID of the module to activate
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_set_active(int module_id) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if module exists */
    if (global_module_registry->modules[module_id].module == NULL) {
        LOG_ERROR("No module registered at ID %d", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if the module is initialized */
    if (!global_module_registry->modules[module_id].initialized) {
        LOG_ERROR("Module ID %d not initialized", module_id);
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate runtime dependencies */
    int status = module_validate_runtime_dependencies(module_id);
    if (status != MODULE_STATUS_SUCCESS) {
        LOG_ERROR("Failed to validate runtime dependencies for module ID %d: %d", 
                 module_id, status);
        return status;
    }
    
    struct base_module *module = global_module_registry->modules[module_id].module;
    enum module_type type = module->type;
    
    /* Check if there's already an active module of this type */
    int existing_index = -1;
    for (int i = 0; i < global_module_registry->num_active_types; i++) {
        if (global_module_registry->active_modules[i].type == type) {
            existing_index = i;
            break;
        }
    }
    
    if (existing_index >= 0) {
        /* Deactivate the existing module */
        int old_module_index = global_module_registry->active_modules[existing_index].module_index;
        global_module_registry->modules[old_module_index].active = false;
        
        /* Replace with the new module */
        global_module_registry->active_modules[existing_index].module_index = module_id;
        
        LOG_INFO("Replaced active module for type %d with '%s' (ID %d)",
                type, module->name, module_id);
    } else {
        /* Add as a new active module */
        if (global_module_registry->num_active_types >= MODULE_TYPE_MAX) {
            LOG_ERROR("Too many active module types");
            return MODULE_STATUS_ERROR;
        }
        
        int index = global_module_registry->num_active_types;
        global_module_registry->active_modules[index].type = type;
        global_module_registry->active_modules[index].module_index = module_id;
        global_module_registry->num_active_types++;
        
        LOG_INFO("Set module '%s' (ID %d) as active for type %d",
                module->name, module_id, type);
    }
    
    /* Mark the module as active */
    global_module_registry->modules[module_id].active = true;
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get the last error from a module
 * 
 * Retrieves the last error code and message from a module.
 * 
 * @param module_id ID of the module to get the error from
 * @param error_message Buffer to store the error message
 * @param max_length Maximum length of the error message buffer
 * @return Last error code
 */
int module_get_last_error(int module_id, char *error_message, size_t max_length) {
    if (global_module_registry == NULL) {
        LOG_ERROR("Module system not initialized");
        if (error_message != NULL && max_length > 0) {
            strncpy(error_message, "Module system not initialized", max_length - 1);
            error_message[max_length - 1] = '\0';
        }
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate module ID */
    if (module_id < 0 || module_id >= global_module_registry->num_modules) {
        LOG_ERROR("Invalid module ID: %d", module_id);
        if (error_message != NULL && max_length > 0) {
            snprintf(error_message, max_length, "Invalid module ID: %d", module_id);
        }
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check if module exists */
    struct base_module *module = global_module_registry->modules[module_id].module;
    if (module == NULL) {
        LOG_ERROR("No module registered at ID %d", module_id);
        if (error_message != NULL && max_length > 0) {
            snprintf(error_message, max_length, "No module registered at ID %d", module_id);
        }
        return MODULE_STATUS_ERROR;
    }
    
    /* Copy error message */
    if (error_message != NULL && max_length > 0) {
        strncpy(error_message, module->error_message, max_length - 1);
        error_message[max_length - 1] = '\0';
    }
    
    return module->last_error;
}

/**
 * Set error information in a module
 * 
 * Updates the error state of a module.
 * 
 * @param module Pointer to the module interface
 * @param error_code Error code to set
 * @param error_message Error message to set
 */
void module_set_error(struct base_module *module, int error_code, const char *error_message) {
    if (module == NULL) {
        LOG_ERROR("NULL module pointer");
        return;
    }
    
    module->last_error = error_code;
    
    if (error_message != NULL) {
        strncpy(module->error_message, error_message, MAX_ERROR_MESSAGE - 1);
        module->error_message[MAX_ERROR_MESSAGE - 1] = '\0';
    } else {
        module->error_message[0] = '\0';
    }
}
