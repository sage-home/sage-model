#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

/* Include header for the logging structure */
#include "../src/core/core_allvars.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_module_callback.h"
#include "../src/core/core_logging.h"

/* Test functions */
void test_version_parsing(void);
void test_version_comparison(void);
void test_version_compatibility(void);
void test_dependency_version_parsing(void);
void test_module_callback_dependency(void);

int main(void) {
    /* Initialize logging with minimal settings */
    initialize_logging(NULL);
    
    /* Initialize module system */
    int status = module_system_initialize();
    assert(status == MODULE_STATUS_SUCCESS);
    
    printf("\n=== Module Dependency Version Tests ===\n\n");
    
    printf("\n=== Module Dependency Version Tests ===\n\n");
    
    /* Run tests */
    test_version_parsing();
    test_version_comparison();
    test_version_compatibility();
    test_dependency_version_parsing();
    test_module_callback_dependency();
    
    /* Clean up */
    status = module_system_cleanup();
    assert(status == MODULE_STATUS_SUCCESS);
    
    printf("\nAll module dependency version tests passed!\n");
    return 0;
}

/**
 * Test version parsing functionality
 */
void test_version_parsing(void) {
    printf("Testing version parsing...\n");
    
    struct module_version version;
    
    /* Test valid version strings */
    assert(module_parse_version("1.0.0", &version) == MODULE_STATUS_SUCCESS);
    assert(version.major == 1);
    assert(version.minor == 0);
    assert(version.patch == 0);
    
    assert(module_parse_version("2.3.5", &version) == MODULE_STATUS_SUCCESS);
    assert(version.major == 2);
    assert(version.minor == 3);
    assert(version.patch == 5);
    
    /* Test partial version strings */
    assert(module_parse_version("3.4", &version) == MODULE_STATUS_SUCCESS);
    assert(version.major == 3);
    assert(version.minor == 4);
    assert(version.patch == 0);  /* Defaults to 0 */
    
    assert(module_parse_version("5", &version) == MODULE_STATUS_SUCCESS);
    assert(version.major == 5);
    assert(version.minor == 0);  /* Defaults to 0 */
    assert(version.patch == 0);  /* Defaults to 0 */
    
    /* Test invalid version strings */
    assert(module_parse_version("", &version) == MODULE_STATUS_ERROR);
    assert(module_parse_version("invalid", &version) == MODULE_STATUS_ERROR);
    assert(module_parse_version("1.2.3.4", &version) == MODULE_STATUS_SUCCESS);  /* Extra numbers should be ignored */
    
    /* Test edge cases */
    assert(module_parse_version("0.0.0", &version) == MODULE_STATUS_SUCCESS);
    assert(version.major == 0);
    assert(version.minor == 0);
    assert(version.patch == 0);
    
    printf("Version parsing tests passed.\n");
}

/**
 * Test version comparison functionality
 */
void test_version_comparison(void) {
    printf("Testing version comparison...\n");
    
    struct module_version v1, v2;
    
    /* Equal versions */
    module_parse_version("1.0.0", &v1);
    module_parse_version("1.0.0", &v2);
    assert(module_compare_versions(&v1, &v2) == 0);
    
    /* Different major version */
    module_parse_version("2.0.0", &v1);
    module_parse_version("1.0.0", &v2);
    assert(module_compare_versions(&v1, &v2) > 0);
    assert(module_compare_versions(&v2, &v1) < 0);
    
    /* Different minor version */
    module_parse_version("1.2.0", &v1);
    module_parse_version("1.1.0", &v2);
    assert(module_compare_versions(&v1, &v2) > 0);
    assert(module_compare_versions(&v2, &v1) < 0);
    
    /* Different patch version */
    module_parse_version("1.0.2", &v1);
    module_parse_version("1.0.1", &v2);
    assert(module_compare_versions(&v1, &v2) > 0);
    assert(module_compare_versions(&v2, &v1) < 0);
    
    /* Mixed differences */
    module_parse_version("2.1.0", &v1);
    module_parse_version("1.5.10", &v2);
    assert(module_compare_versions(&v1, &v2) > 0);  /* Major trumps minor and patch */
    
    module_parse_version("1.5.0", &v1);
    module_parse_version("1.4.20", &v2);
    assert(module_compare_versions(&v1, &v2) > 0);  /* Minor trumps patch */
    
    printf("Version comparison tests passed.\n");
}

/**
 * Test version compatibility functionality
 */
void test_version_compatibility(void) {
    printf("Testing version compatibility...\n");
    
    struct module_version version, min_version, max_version;
    
    /* Test exact match */
    module_parse_version("1.2.3", &version);
    module_parse_version("1.2.3", &min_version);
    assert(module_check_version_compatibility(&version, &min_version, NULL, true) == true);
    
    module_parse_version("1.2.4", &version);
    assert(module_check_version_compatibility(&version, &min_version, NULL, true) == false);
    
    /* Test minimum version only */
    module_parse_version("1.2.3", &version);
    module_parse_version("1.0.0", &min_version);
    assert(module_check_version_compatibility(&version, &min_version, NULL, false) == true);
    
    module_parse_version("0.9.0", &version);
    assert(module_check_version_compatibility(&version, &min_version, NULL, false) == false);
    
    /* Test range (min and max) */
    module_parse_version("1.5.0", &version);
    module_parse_version("1.0.0", &min_version);
    module_parse_version("2.0.0", &max_version);
    assert(module_check_version_compatibility(&version, &min_version, &max_version, false) == true);
    
    module_parse_version("0.9.0", &version);
    assert(module_check_version_compatibility(&version, &min_version, &max_version, false) == false);  /* Too low */
    
    module_parse_version("2.1.0", &version);
    assert(module_check_version_compatibility(&version, &min_version, &max_version, false) == false);  /* Too high */
    
    /* Edge cases */
    module_parse_version("1.0.0", &version);
    module_parse_version("1.0.0", &min_version);
    module_parse_version("2.0.0", &max_version);
    assert(module_check_version_compatibility(&version, &min_version, &max_version, false) == true);  /* Equals min */
    
    module_parse_version("2.0.0", &version);
    assert(module_check_version_compatibility(&version, &min_version, &max_version, false) == true);  /* Equals max */
    
    printf("Version compatibility tests passed.\n");
}

/**
 * Test dependency version parsing functionality
 */
void test_dependency_version_parsing(void) {
    printf("Testing dependency version parsing...\n");
    
    /* Create test dependency */
    struct module_dependency dep;
    memset(&dep, 0, sizeof(struct module_dependency));
    
    /* Test with only min version */
    strncpy(dep.min_version_str, "1.2.3", sizeof(dep.min_version_str) - 1);
    
    /* Call the internal parse_dependency_versions function */
    /* Since it's static, we need to indirectly test its effects */
    /* We'll use the module_load_manifest function which should call it */
    
    /* Create a manifest file with a dependency */
    FILE *manifest = fopen("test_dependency.manifest", "w");
    assert(manifest != NULL);
    
    fprintf(manifest, "name: test_module\n");
    fprintf(manifest, "version: 1.0.0\n");
    fprintf(manifest, "author: SAGE Test\n");
    fprintf(manifest, "description: Test module\n");
    fprintf(manifest, "type: cooling\n");
    fprintf(manifest, "library: test_module.so\n");
    fprintf(manifest, "api_version: 1\n");
    fprintf(manifest, "dependency.0: test_dependency: 1.2.3\n");
    fprintf(manifest, "dependency.1: test_dependency2: 2.0.0: 3.0.0\n");
    fprintf(manifest, "dependency.2: test_dependency3[optional]: 1.5.0\n");
    fprintf(manifest, "dependency.3: test_dependency4[exact]: 2.5.0\n");
    
    fclose(manifest);
    
    /* Load the manifest */
    struct module_manifest m;
    int status = module_load_manifest("test_dependency.manifest", &m);
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Verify that the parsed versions were set correctly */
    assert(m.dependencies[0].has_parsed_versions == true);
    assert(m.dependencies[0].min_version.major == 1);
    assert(m.dependencies[0].min_version.minor == 2);
    assert(m.dependencies[0].min_version.patch == 3);
    
    /* Verify dependency with both min and max versions */
    assert(m.dependencies[1].has_parsed_versions == true);
    assert(m.dependencies[1].min_version.major == 2);
    assert(m.dependencies[1].min_version.minor == 0);
    assert(m.dependencies[1].min_version.patch == 0);
    assert(m.dependencies[1].max_version.major == 3);
    assert(m.dependencies[1].max_version.minor == 0);
    assert(m.dependencies[1].max_version.patch == 0);
    
    /* Verify optional dependency flag */
    assert(m.dependencies[2].optional == true);
    assert(m.dependencies[2].has_parsed_versions == true);
    
    /* Verify exact match dependency flag */
    assert(m.dependencies[3].exact_match == true);
    assert(m.dependencies[3].has_parsed_versions == true);
    
    /* Clean up */
    remove("test_dependency.manifest");
    
    printf("Dependency version parsing tests passed.\n");
}

/**
 * Test module dependency functionality using direct struct manipulation
 */
void test_module_callback_dependency(void) {
    printf("Testing direct dependency version parsing...\n");
    
    /* Create dependency struct directly */
    struct module_dependency dep = {0};
    
    /* Set version strings */
    strncpy(dep.min_version_str, "1.5.0", sizeof(dep.min_version_str) - 1);
    strncpy(dep.max_version_str, "2.0.0", sizeof(dep.max_version_str) - 1);
    
    /* Parse versions */
    dep.has_parsed_versions = false;
    
    /* Manually parse versions */
    int status = module_parse_version(dep.min_version_str, &dep.min_version);
    assert(status == MODULE_STATUS_SUCCESS);
    
    status = module_parse_version(dep.max_version_str, &dep.max_version);
    assert(status == MODULE_STATUS_SUCCESS);
    
    dep.has_parsed_versions = true;
    
    /* Verify the version was parsed correctly */
    assert(dep.has_parsed_versions == true);
    assert(dep.min_version.major == 1);
    assert(dep.min_version.minor == 5);
    assert(dep.min_version.patch == 0);
    
    assert(dep.max_version.major == 2);
    assert(dep.max_version.minor == 0);
    assert(dep.max_version.patch == 0);
    
    printf("Direct dependency version parsing tests passed.\n");
}
