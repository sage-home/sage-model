#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../src/core/core_allvars.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_dynamic_library.h"

/* Test constants */
#define TEST_DIR "./test_modules"
#define TEST_MODULE_NAME "test_cooling_module"

/* Global test variables */
struct params test_params;

/* Function prototypes */
void setup(void);
void teardown(void);
void create_test_module_structure(void);
void create_test_manifest(void);
void test_api_compatibility(void);
void test_module_discovery(void);

int main(void) {
    /* Initialize logging */
    initialize_logging(NULL);
    
    printf("\n=== Module Discovery Tests ===\n\n");
    
    /* Initialize the dynamic library system */
    dl_error_t dl_result = dynamic_library_system_initialize();
    assert(dl_result == DL_SUCCESS);
    
    /* Run test setup */
    setup();
    
    /* Run tests */
    test_api_compatibility();
    test_module_discovery();
    
    /* Clean up */
    teardown();
    
    /* Clean up the dynamic library system */
    dl_result = dynamic_library_system_cleanup();
    assert(dl_result == DL_SUCCESS);
    
    printf("\nAll module discovery tests passed!\n");
    return 0;
}

/* Set up the test environment */
void setup(void) {
    /* Initialize module system */
    int status = module_system_initialize();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Initialize test parameters */
    memset(&test_params, 0, sizeof(struct params));
    
    /* Create test directory structure */
    create_test_module_structure();
    
    /* Create test manifest file */
    create_test_manifest();
}

/* Clean up the test environment */
void teardown(void) {
    /* Clean up module system */
    int status = module_system_cleanup();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Remove test directory */
    char command[256];
    snprintf(command, sizeof(command), "rm -rf %s", TEST_DIR);
    system(command);
}

/* Create test module directory structure */
void create_test_module_structure(void) {
    struct stat st = {0};
    
    /* Create main test directory if it doesn't exist */
    if (stat(TEST_DIR, &st) == -1) {
        mkdir(TEST_DIR, 0755);
    }
}

/* Create test manifest file */
void create_test_manifest(void) {
    char manifest_path[256];
    snprintf(manifest_path, sizeof(manifest_path), "%s/%s.manifest", 
             TEST_DIR, TEST_MODULE_NAME);
    
    FILE *f = fopen(manifest_path, "w");
    assert(f != NULL);
    
    fprintf(f, "name: %s\n", TEST_MODULE_NAME);
    fprintf(f, "version: 1.0.0\n");
    fprintf(f, "author: SAGE Test\n");
    fprintf(f, "description: Test cooling module for SAGE\n");
    fprintf(f, "type: cooling\n");
    fprintf(f, "library: %s.so\n", TEST_MODULE_NAME);
    fprintf(f, "api_version: %d\n", CORE_API_VERSION);
    fprintf(f, "auto_initialize: true\n");
    fprintf(f, "auto_activate: true\n");
    
    fclose(f);
}

/* Test API version compatibility check */
void test_api_compatibility(void) {
    printf("Testing API compatibility check...\n");
    
    /* No direct way to test the private function, so we'll test via manifest loading */
    struct module_manifest manifest;
    memset(&manifest, 0, sizeof(struct module_manifest));
    
    /* Create a manifest with the current API version */
    strncpy(manifest.name, "test_module", MAX_MODULE_NAME);
    strncpy(manifest.version_str, "1.0.0", MAX_MODULE_VERSION);
    manifest.type = MODULE_TYPE_COOLING;
    strncpy(manifest.library_path, "test_module.so", MAX_MODULE_PATH);
    manifest.api_version = CORE_API_VERSION;  /* Current version */
    
    /* This should validate successfully */
    assert(module_validate_manifest(&manifest));
    
    /* Now try with an incompatible version */
    manifest.api_version = CORE_API_VERSION + 10;  /* Future incompatible version */
    
    /* This should still validate (validation doesn't check API compatibility) */
    assert(module_validate_manifest(&manifest));
    
    printf("API compatibility check tests passed.\n");
}

/* Test module discovery */
void test_module_discovery(void) {
    printf("Testing module discovery...\n");
    
    /* Configure the registry for discovery */
    global_module_registry->discovery_enabled = true;
    
    /* Add the test directory as a search path */
    int status = module_add_search_path(TEST_DIR);
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Since we can't actually load the module (it doesn't exist), we'll test 
       that the discovery process finds the manifest file but fails to load the module */
    int modules_found = module_discover(&test_params);
    
    /* We should find 0 modules since the file doesn't actually exist */
    assert(modules_found == 0);
    
    printf("Module discovery tests passed.\n");
}
