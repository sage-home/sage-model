#include "test_invalid_module.h"

/* Global storage for created modules to facilitate cleanup */
struct base_module* test_modules[16];
int test_module_count = 0;

/* Initialize the test helper system */
void test_invalid_module_init(void) {
    memset(test_modules, 0, sizeof(test_modules));
    test_module_count = 0;
}

/* Cleanup the test helper system */
void test_invalid_module_cleanup(void) {
    for (int i = 0; i < test_module_count; i++) {
        if (test_modules[i] != NULL) {
            free(test_modules[i]);
            test_modules[i] = NULL;
        }
    }
    test_module_count = 0;
}

/* Helper to track created modules for cleanup */
void track_test_module(struct base_module* module) {
    if (test_module_count < (int)(sizeof(test_modules) / sizeof(test_modules[0]))) {
        test_modules[test_module_count++] = module;
    } else {
        fprintf(stderr, "Too many test modules created\n");
    }
}

/* Dummy initialize function for test modules */
int dummy_initialize(struct params *params, void **module_data) {
    (void)params;  /* Explicitly mark as unused */
    *module_data = malloc(1);  /* Minimal allocation */
    return MODULE_STATUS_SUCCESS;
}

/* Dummy cleanup function for test modules */
int dummy_cleanup(void *module_data) {
    free(module_data);
    return MODULE_STATUS_SUCCESS;
}

/* Dummy cooling function for test modules */
double dummy_calculate_cooling(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data) {
    (void)gal_idx;  /* Unused */
    (void)dt;       /* Unused */
    (void)galaxies; /* Unused */
    (void)module_data; /* Unused */
    return 0.0;
}

/* Dummy cooling rate function for test modules */
double dummy_get_cooling_rate(int gal_idx, struct GALAXY *galaxies, void *module_data) {
    (void)gal_idx;  /* Unused */
    (void)galaxies; /* Unused */
    (void)module_data; /* Unused */
    return 0.0;
}

/* Dummy star formation function for test modules */
double dummy_form_stars(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data) {
    (void)gal_idx;  /* Unused */
    (void)dt;       /* Unused */
    (void)galaxies; /* Unused */
    (void)module_data; /* Unused */
    return 0.0;
}

/* Create a minimal valid module */
struct base_module* create_minimal_valid_module(void) {
    struct base_module* module = (struct base_module*)malloc(sizeof(struct base_module));
    if (!module) return NULL;
    
    /* Initialize with zeros */
    memset(module, 0, sizeof(struct base_module));
    
    /* Set required fields */
    strcpy(module->name, "ValidTestModule");
    strcpy(module->version, "1.0.0");
    strcpy(module->author, "SAGE Test Framework");
    module->type = MODULE_TYPE_MISC;
    module->initialize = dummy_initialize;
    module->cleanup = dummy_cleanup;
    
    track_test_module(module);
    return module;
}

/* Create a fully valid module */
struct base_module* create_valid_module(void) {
    struct base_module* module = create_minimal_valid_module();
    if (!module) return NULL;
    
    /* Add additional valid fields */
    module->configure = NULL;  /* Optional */
    module->last_error = 0;
    strcpy(module->error_message, "");
    
    return module;
}

/* Create a module with empty name */
struct base_module* create_module_with_empty_name(void) {
    struct base_module* module = create_minimal_valid_module();
    if (!module) return NULL;
    
    /* Make the name empty to cause validation failure */
    module->name[0] = '\0';
    
    return module;
}

/* Create a module with empty version */
struct base_module* create_module_with_empty_version(void) {
    struct base_module* module = create_minimal_valid_module();
    if (!module) return NULL;
    
    /* Make the version empty to cause validation failure */
    module->version[0] = '\0';
    
    return module;
}

/* Create a module with invalid type */
struct base_module* create_module_with_invalid_type(void) {
    struct base_module* module = create_minimal_valid_module();
    if (!module) return NULL;
    
    /* Set an invalid module type */
    module->type = MODULE_TYPE_MAX + 1;
    
    return module;
}

/* Create a module with missing initialize function */
struct base_module* create_module_with_missing_initialize(void) {
    struct base_module* module = create_minimal_valid_module();
    if (!module) return NULL;
    
    /* Remove initialize function */
    module->initialize = NULL;
    
    return module;
}

/* Create a module with missing cleanup function */
struct base_module* create_module_with_missing_cleanup(void) {
    struct base_module* module = create_minimal_valid_module();
    if (!module) return NULL;
    
    /* Remove cleanup function */
    module->cleanup = NULL;
    
    return module;
}

/* Create a cooling module missing calculate_cooling function */
struct cooling_module* create_cooling_module_missing_calculate_cooling(void) {
    struct cooling_module* module = (struct cooling_module*)malloc(sizeof(struct cooling_module));
    if (!module) return NULL;
    
    /* Initialize with zeros */
    memset(module, 0, sizeof(struct cooling_module));
    
    /* Set up as a valid base module */
    strcpy(module->base.name, "InvalidCoolingModule");
    strcpy(module->base.version, "1.0.0");
    strcpy(module->base.author, "SAGE Test Framework");
    module->base.type = MODULE_TYPE_COOLING;
    module->base.initialize = dummy_initialize;
    module->base.cleanup = dummy_cleanup;
    
    /* Missing calculate_cooling to cause validation failure */
    module->calculate_cooling = NULL;
    module->get_cooling_rate = dummy_get_cooling_rate;
    
    track_test_module((struct base_module*)module);
    return module;
}

/* Create a star formation module missing form_stars function */
struct star_formation_module* create_star_formation_module_missing_form_stars(void) {
    struct star_formation_module* module = (struct star_formation_module*)malloc(sizeof(struct star_formation_module));
    if (!module) return NULL;
    
    /* Initialize with zeros */
    memset(module, 0, sizeof(struct star_formation_module));
    
    /* Set up as a valid base module */
    strcpy(module->base.name, "InvalidStarFormationModule");
    strcpy(module->base.version, "1.0.0");
    strcpy(module->base.author, "SAGE Test Framework");
    module->base.type = MODULE_TYPE_STAR_FORMATION;
    module->base.initialize = dummy_initialize;
    module->base.cleanup = dummy_cleanup;
    
    /* Missing form_stars to cause validation failure */
    module->form_stars = NULL;
    
    track_test_module((struct base_module*)module);
    return module;
}

/* Create a valid manifest */
struct module_manifest* create_valid_manifest(void) {
    struct module_manifest* manifest = (struct module_manifest*)malloc(sizeof(struct module_manifest));
    if (!manifest) return NULL;
    
    /* Initialize with zeros */
    memset(manifest, 0, sizeof(struct module_manifest));
    
    /* Set required fields */
    strcpy(manifest->name, "ValidTestManifest");
    strcpy(manifest->version_str, "1.0.0");
    manifest->type = MODULE_TYPE_MISC;
    strcpy(manifest->library_path, "/path/to/library.so");
    manifest->api_version = CORE_API_VERSION;
    
    track_test_module((struct base_module*)manifest);
    return manifest;
}

/* Create a manifest with empty name */
struct module_manifest* create_manifest_with_empty_name(void) {
    struct module_manifest* manifest = create_valid_manifest();
    if (!manifest) return NULL;
    
    /* Make name empty to cause validation failure */
    manifest->name[0] = '\0';
    
    return manifest;
}

/* Create a manifest with empty version */
struct module_manifest* create_manifest_with_empty_version(void) {
    struct module_manifest* manifest = create_valid_manifest();
    if (!manifest) return NULL;
    
    /* Make version empty to cause validation failure */
    manifest->version_str[0] = '\0';
    
    return manifest;
}

/* Create a manifest with invalid type */
struct module_manifest* create_manifest_with_invalid_type(void) {
    struct module_manifest* manifest = create_valid_manifest();
    if (!manifest) return NULL;
    
    /* Set invalid type to cause validation failure */
    manifest->type = MODULE_TYPE_MAX + 1;
    
    return manifest;
}

/* Create a manifest with empty library path */
struct module_manifest* create_manifest_with_empty_library_path(void) {
    struct module_manifest* manifest = create_valid_manifest();
    if (!manifest) return NULL;
    
    /* Make library path empty to cause validation failure */
    manifest->library_path[0] = '\0';
    
    return manifest;
}

/* Create a manifest with invalid API version */
struct module_manifest* create_manifest_with_invalid_api_version(void) {
    struct module_manifest* manifest = create_valid_manifest();
    if (!manifest) return NULL;
    
    /* Set invalid API version to cause validation failure */
    manifest->api_version = 0;
    
    return manifest;
}

/* Create a valid dependency */
struct module_dependency* create_valid_dependency(void) {
    struct module_dependency* dependency = (struct module_dependency*)malloc(sizeof(struct module_dependency));
    if (!dependency) return NULL;
    
    /* Initialize with zeros */
    memset(dependency, 0, sizeof(struct module_dependency));
    
    /* Set required fields */
    strcpy(dependency->name, "ValidDependency");
    dependency->type = MODULE_TYPE_MISC;
    dependency->optional = false;
    
    track_test_module((struct base_module*)dependency);
    return dependency;
}

/* Create a dependency with no name or type */
struct module_dependency* create_dependency_with_no_name_or_type(void) {
    struct module_dependency* dependency = create_valid_dependency();
    if (!dependency) return NULL;
    
    /* Remove name and set invalid type to cause validation failure */
    dependency->name[0] = '\0';
    dependency->type = MODULE_TYPE_UNKNOWN;
    
    return dependency;
}

/* Create a dependency with invalid version constraints */
struct module_dependency* create_dependency_with_invalid_version_constraints(void) {
    struct module_dependency* dependency = create_valid_dependency();
    if (!dependency) return NULL;
    
    /* Set min version > max version to cause validation failure */
    dependency->has_parsed_versions = true;
    strcpy(dependency->min_version_str, "2.0.0");
    strcpy(dependency->max_version_str, "1.0.0");
    dependency->min_version.major = 2;
    dependency->max_version.major = 1;
    
    return dependency;
}

/* Helper to verify validation errors for a given module */
bool verify_validation_error(struct base_module* module, 
                            const char* expected_error_substr,
                            const char* component) {
    module_validation_result_t result;
    module_validation_options_t options;
    
    module_validation_result_init(&result);
    module_validation_options_init(&options);
    
    /* Validate the module */
    module_validate_interface(module, &result, &options);
    
    /* Check if expected error was found */
    bool found_error = false;
    for (int i = 0; i < result.num_issues; i++) {
        if (result.issues[i].severity == VALIDATION_ERROR &&
            strstr(result.issues[i].message, expected_error_substr) != NULL &&
            (component == NULL || strcmp(result.issues[i].component, component) == 0)) {
            found_error = true;
            break;
        }
    }
    
    return found_error;
}

/* Functions for dependency testing - these will be implemented in the test file 
   since they require access to the module system state */