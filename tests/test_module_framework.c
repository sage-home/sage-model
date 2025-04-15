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
#include "test_module_template_validation.h"

/* Function prototypes */
void test_module_template_generation(void);
void test_module_validation(void);
void cleanup_test_files(void);
void test_minimal_module_template(void);
void test_full_module_template(void);
void test_mixed_module_template(void);
void create_test_dirs(void);

/* Main test function */
int main(int argc, char *argv[]) {
    /* Check for the no-cleanup option */
    bool skip_cleanup = false;
    if (argc > 1 && strcmp(argv[1], "--no-cleanup") == 0) {
        skip_cleanup = true;
    }
    
    /* Initialize logging */
    initialize_logging(NULL);
    
    printf("\n=== Module Development Framework Tests ===\n\n");
    
    /* Initialize the dynamic library system */
    dl_error_t dl_result = dynamic_library_system_initialize();
    assert(dl_result == DL_SUCCESS);
    
    /* Initialize the module system */
    int status = module_system_initialize();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Create test directories */
    create_test_dirs();
    
    /* Run tests */
    test_module_template_generation(); /* Original basic test */
    test_module_validation();          /* Original validation test */
    
    /* Enhanced template tests with content validation */
    test_minimal_module_template();
    test_full_module_template();
    test_mixed_module_template();
    
    /* Clean up the module system */
    status = module_system_cleanup();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Clean up the dynamic library system */
    dl_result = dynamic_library_system_cleanup();
    assert(dl_result == DL_SUCCESS);
    
    /* Clean up test files unless skipped */
    if (!skip_cleanup) {
        cleanup_test_files();
    } else {
        printf("\nSkipping cleanup of test files.\n");
    }
    
    printf("\nAll tests passed!\n");
    return 0;
}

/* Create test directories */
void create_test_dirs(void) {
    struct stat st = {0};
    
    /* Create base output directory if it doesn't exist */
    if (stat(TEST_BASE_DIR, &st) == -1) {
        mkdir(TEST_BASE_DIR, 0755);
    }
    
    /* Create minimal output directory */
    if (stat(TEST_MIN_DIR, &st) == -1) {
        mkdir(TEST_MIN_DIR, 0755);
    }
    
    /* Create full output directory */
    if (stat(TEST_FULL_DIR, &st) == -1) {
        mkdir(TEST_FULL_DIR, 0755);
    }
    
    /* Create mixed output directory */
    if (stat(TEST_MIXED_DIR, &st) == -1) {
        mkdir(TEST_MIXED_DIR, 0755);
    }
}

/* Test module template generation (original basic test) */
void test_module_template_generation(void) {
    printf("Testing basic module template generation...\n");
    
    /* Initialize template parameters */
    struct module_template_params params;
    int result = module_template_params_init(&params);
    assert(result == 0);
    
    /* Set template parameters */
    strcpy(params.module_name, TEST_MODULE_COOLING);
    strcpy(params.module_prefix, TEST_MODULE_COOLING_PREFIX);
    strcpy(params.author, "SAGE Test Framework");
    strcpy(params.email, "test@example.com");
    strcpy(params.description, "Test cooling module for SAGE");
    strcpy(params.version, "1.0.0");
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
    strcpy(params.output_dir, TEST_BASE_DIR);
    
    /* Generate the template */
    result = module_generate_template(&params);
    assert(result == 0);
    
    /* Verify that the template files were created */
    char path[PATH_MAX];
    
    /* Check header file */
    snprintf(path, sizeof(path), "%s/%s.h", TEST_BASE_DIR, TEST_MODULE_COOLING);
    assert(access(path, F_OK) == 0);
    
    /* Check implementation file */
    snprintf(path, sizeof(path), "%s/%s.c", TEST_BASE_DIR, TEST_MODULE_COOLING);
    assert(access(path, F_OK) == 0);
    
    /* Check manifest file */
    snprintf(path, sizeof(path), "%s/%s.manifest", TEST_BASE_DIR, TEST_MODULE_COOLING);
    assert(access(path, F_OK) == 0);
    
    /* Check Makefile */
    snprintf(path, sizeof(path), "%s/Makefile", TEST_BASE_DIR);
    assert(access(path, F_OK) == 0);
    
    /* Check README file */
    snprintf(path, sizeof(path), "%s/README.md", TEST_BASE_DIR);
    assert(access(path, F_OK) == 0);
    
    /* Check test file */
    snprintf(path, sizeof(path), "%s/test_%s.c", TEST_BASE_DIR, TEST_MODULE_COOLING);
    assert(access(path, F_OK) == 0);
    
    printf("Basic module template generation tests passed.\n");
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

/* Test minimal module template */
void test_minimal_module_template(void) {
    printf("\nTesting minimal module template generation with content validation...\n");
    
    /* Setup minimal template parameters */
    struct module_template_params params;
    setup_minimal_template_params(&params, TEST_MIN_DIR, MODULE_TYPE_COOLING);
    
    /* Generate the template */
    int result = module_generate_template(&params);
    assert(result == 0);
    
    /* Validate all generated files */
    bool valid = validate_all_module_files(&params);
    assert(valid);
    
    printf("Minimal module template generation tests passed.\n");
}

/* Test full module template */
void test_full_module_template(void) {
    printf("\nTesting full module template generation with content validation...\n");
    
    /* Setup full template parameters */
    struct module_template_params params;
    setup_full_template_params(&params, TEST_FULL_DIR, MODULE_TYPE_STAR_FORMATION);
    
    /* Generate the template */
    int result = module_generate_template(&params);
    assert(result == 0);
    
    /* Validate all generated files */
    bool valid = validate_all_module_files(&params);
    assert(valid);
    
    printf("Full module template generation tests passed.\n");
}

/* Test mixed module template */
void test_mixed_module_template(void) {
    printf("\nTesting mixed module template generation with content validation...\n");
    
    /* Setup mixed template parameters */
    struct module_template_params params;
    setup_mixed_template_params(&params, TEST_MIXED_DIR, MODULE_TYPE_FEEDBACK);
    
    /* Generate the template */
    int result = module_generate_template(&params);
    assert(result == 0);
    
    /* Validate all generated files */
    bool valid = validate_all_module_files(&params);
    assert(valid);
    
    printf("Mixed module template generation tests passed.\n");
}

/* Clean up test files */
void cleanup_test_files(void) {
    /* Remove generated files */
    char command[PATH_MAX];
    snprintf(command, sizeof(command), "rm -rf %s", TEST_BASE_DIR);
    system(command);
}
