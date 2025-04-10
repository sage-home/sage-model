#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../src/core/core_module_system.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_module_template.h"
#include "../src/core/core_module_validation.h"
#include "../src/core/core_dynamic_library.h"

/* Constants for testing */
#define TEST_OUTPUT_DIR "./test_module_output"
#define TEST_MODULE_NAME "test_cooling_module"
#define TEST_MODULE_PREFIX "tcm"
#define TEST_MODULE_AUTHOR "SAGE Test Framework"
#define TEST_MODULE_EMAIL "test@example.com"
#define TEST_MODULE_DESCRIPTION "Test cooling module for SAGE"
#define TEST_MODULE_VERSION "1.0.0"

/* Function prototypes */
void test_module_template_generation(void);
void test_module_validation(void);
void cleanup_test_files(void);

/* Main test function */
int main(void) {
    /* Initialize logging */
    initialize_logging(NULL);
    
    printf("\n=== Module Development Framework Tests ===\n\n");
    
    /* Initialize the dynamic library system */
    dl_error_t dl_result = dynamic_library_system_initialize();
    assert(dl_result == DL_SUCCESS);
    
    /* Initialize the module system */
    int status = module_system_initialize();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Run tests */
    test_module_template_generation();
    test_module_validation();
    
    /* Clean up the module system */
    status = module_system_cleanup();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Clean up the dynamic library system */
    dl_result = dynamic_library_system_cleanup();
    assert(dl_result == DL_SUCCESS);
    
    /* Clean up test files */
    cleanup_test_files();
    
    printf("\nAll tests passed!\n");
    return 0;
}

/* Test module template generation */
void test_module_template_generation(void) {
    printf("Testing module template generation...\n");
    
    /* Create the test output directory if it doesn't exist */
    struct stat st = {0};
    if (stat(TEST_OUTPUT_DIR, &st) == -1) {
        mkdir(TEST_OUTPUT_DIR, 0755);
    }
    
    /* Initialize template parameters */
    struct module_template_params params;
    int result = module_template_params_init(&params);
    assert(result == 0);
    
    /* Set template parameters */
    strcpy(params.module_name, TEST_MODULE_NAME);
    strcpy(params.module_prefix, TEST_MODULE_PREFIX);
    strcpy(params.author, TEST_MODULE_AUTHOR);
    strcpy(params.email, TEST_MODULE_EMAIL);
    strcpy(params.description, TEST_MODULE_DESCRIPTION);
    strcpy(params.version, TEST_MODULE_VERSION);
    params.type = MODULE_TYPE_COOLING;
    
    /* Set template features */
    params.include_galaxy_extension = true;
    params.include_event_handler = true;
    params.include_callback_registration = true;
    params.include_manifest = true;
    params.include_makefile = true;
    params.include_test_file = true;
    params.include_readme = true;
    
    /* Set output directory */
    strcpy(params.output_dir, TEST_OUTPUT_DIR);
    
    /* Generate the template */
    result = module_generate_template(&params);
    assert(result == 0);
    
    /* Verify that the template files were created */
    char path[PATH_MAX];
    
    /* Check header file */
    snprintf(path, sizeof(path), "%s/%s.h", TEST_OUTPUT_DIR, TEST_MODULE_NAME);
    assert(access(path, F_OK) == 0);
    
    /* Check implementation file */
    snprintf(path, sizeof(path), "%s/%s.c", TEST_OUTPUT_DIR, TEST_MODULE_NAME);
    assert(access(path, F_OK) == 0);
    
    /* Check manifest file */
    snprintf(path, sizeof(path), "%s/%s.manifest", TEST_OUTPUT_DIR, TEST_MODULE_NAME);
    assert(access(path, F_OK) == 0);
    
    /* Check Makefile */
    snprintf(path, sizeof(path), "%s/Makefile", TEST_OUTPUT_DIR);
    assert(access(path, F_OK) == 0);
    
    /* Check README file */
    snprintf(path, sizeof(path), "%s/README.md", TEST_OUTPUT_DIR);
    assert(access(path, F_OK) == 0);
    
    /* Check test file */
    snprintf(path, sizeof(path), "%s/test_%s.c", TEST_OUTPUT_DIR, TEST_MODULE_NAME);
    assert(access(path, F_OK) == 0);
    
    printf("Module template generation tests passed.\n");
}

/* Test module validation */
void test_module_validation(void) {
    printf("\nTesting module validation...\n");
    
    /* Initialize validation options */
    module_validation_options_t options;
    bool result = module_validation_options_init(&options);
    assert(result);
    
    /* Initialize validation result */
    module_validation_result_t validation_result;
    result = module_validation_result_init(&validation_result);
    assert(result);
    
    /* Create a test issue */
    result = module_validation_add_issue(
        &validation_result,
        VALIDATION_WARNING,
        "Test validation warning",
        "test_component",
        "test_file.c",
        42
    );
    assert(result);
    
    /* Create another test issue */
    result = module_validation_add_issue(
        &validation_result,
        VALIDATION_ERROR,
        "Test validation error",
        "test_component",
        "test_file.c",
        43
    );
    assert(result);
    
    /* Verify the issues were added */
    assert(validation_result.num_issues == 2);
    assert(validation_result.warning_count == 1);
    assert(validation_result.error_count == 1);
    
    /* Format the validation result */
    char output[4096];
    result = module_validation_result_format(&validation_result, output, sizeof(output));
    assert(result);
    
    printf("Validation result format output:\n%s\n", output);
    
    /* Test validation result has errors */
    result = module_validation_has_errors(&validation_result, &options);
    assert(result);
    
    /* Test strict mode */
    options.strict = false;
    module_validation_result_init(&validation_result);
    module_validation_add_issue(
        &validation_result,
        VALIDATION_WARNING,
        "Test warning only",
        "test_component",
        "test_file.c",
        42
    );
    
    /* Should not have errors in non-strict mode */
    result = module_validation_has_errors(&validation_result, &options);
    assert(!result);
    
    /* Should have errors in strict mode */
    options.strict = true;
    result = module_validation_has_errors(&validation_result, &options);
    assert(result);
    
    printf("Module validation tests passed.\n");
}

/* Clean up test files */
void cleanup_test_files(void) {
    /* Remove generated files */
    char command[PATH_MAX];
    snprintf(command, sizeof(command), "rm -rf %s", TEST_OUTPUT_DIR);
    system(command);
}
