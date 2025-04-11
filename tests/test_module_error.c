/**
 * @file test_module_error.c
 * @brief Test suite for the module error handling system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/core/core_module_error.h"
#include "../src/core/core_module_system.h"
#include "../src/core/core_module_callback.h"
#include "../src/core/core_module_diagnostics.h"
#include "../src/core/core_logging.h"
#include "../src/core/macros.h"

/* Define test status codes */
#define TEST_SUCCESS 0
#define TEST_FAILURE 1

/* Global test counter */
static int tests_run = 0;
static int tests_failed = 0;

/* Mock module for testing */
static struct base_module *create_test_module(const char *name) {
    struct base_module *module = (struct base_module *)calloc(1, sizeof(struct base_module));
    assert(module != NULL);
    
    strncpy(module->name, name, MAX_MODULE_NAME - 1);
    module->module_id = 123;
    module->type = MODULE_TYPE_COOLING;
    
    return module;
}

/* Clean up a test module */
static void cleanup_test_module(struct base_module *module) {
    if (module == NULL) {
        return;
    }
    
    /* Clean up error context if it exists */
    if (module->error_context != NULL) {
        module_error_context_cleanup(module->error_context);
    }
    
    free(module);
}

/* Simplified assertion with message */
static void assert_condition(int condition, const char *message) {
    if (!condition) {
        printf("FAILURE: %s\n", message);
        tests_failed++;
        return;
    }
}

/* Test error context initialization and cleanup */
static int test_error_context_init_cleanup() {
    printf("Testing error context initialization and cleanup...\n");
    
    struct module_error_context *context = NULL;
    
    /* Test initialization */
    int status = module_error_context_init(&context);
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error context initialization failed");
    assert_condition(context != NULL, "Error context should not be NULL after initialization");
    assert_condition(context->error_count == 0, "New error context should have zero errors");
    assert_condition(context->current_index == 0, "New error context should have index 0");
    assert_condition(context->overflow == false, "New error context should not have overflow");
    
    /* Test cleanup */
    status = module_error_context_cleanup(context);
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error context cleanup failed");
    
    printf("PASSED: Error context initialization and cleanup\n");
    tests_run++;
    return TEST_SUCCESS;
}

/* Test error recording and retrieval */
static int test_error_recording() {
    printf("Testing error recording and retrieval...\n");
    
    struct base_module *module = create_test_module("TestModule");
    
    /* Record an error */
    int status = module_record_error(module, MODULE_STATUS_ERROR, LOG_LEVEL_ERROR,
                                    "test_file.c", 123, "test_function",
                                    "Test error message");
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error recording failed");
    assert_condition(module->error_context != NULL, "Error context should be created");
    assert_condition(module->error_context->error_count == 1, "Error count should be 1");
    
    /* Verify the last_error fields (backwards compatibility) */
    assert_condition(module->last_error == MODULE_STATUS_ERROR, "last_error field not updated");
    assert_condition(strcmp(module->error_message, "Test error message") == 0, 
                    "error_message field not updated");
    
    /* Retrieve the latest error */
    module_error_info_t error;
    status = module_get_latest_error(module, &error);
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error retrieval failed");
    assert_condition(error.code == MODULE_STATUS_ERROR, "Error code doesn't match");
    assert_condition(error.severity == LOG_LEVEL_ERROR, "Error severity doesn't match");
    assert_condition(strcmp(error.message, "Test error message") == 0, "Error message doesn't match");
    assert_condition(strcmp(error.file, "test_file.c") == 0, "Error file doesn't match");
    assert_condition(error.line == 123, "Error line doesn't match");
    assert_condition(strcmp(error.function, "test_function") == 0, "Error function doesn't match");
    
    /* Test the enhanced set_error function */
    module_set_error_ex(module, MODULE_STATUS_INVALID_ARGS, LOG_LEVEL_WARNING,
                       "other_file.c", 456, "other_function", "Another error");
    
    /* Verify the update */
    status = module_get_latest_error(module, &error);
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error retrieval failed after set_error_ex");
    assert_condition(error.code == MODULE_STATUS_INVALID_ARGS, "New error code doesn't match");
    assert_condition(error.severity == LOG_LEVEL_WARNING, "New error severity doesn't match");
    assert_condition(strcmp(error.message, "Another error") == 0, "New error message doesn't match");
    
    /* Clean up */
    cleanup_test_module(module);
    
    printf("PASSED: Error recording and retrieval\n");
    tests_run++;
    return TEST_SUCCESS;
}

/* Test error history circular buffer */
static int test_error_history_circular_buffer() {
    printf("Testing error history circular buffer...\n");
    
    struct base_module *module = create_test_module("BufferTest");
    
    /* Record more errors than the buffer can hold */
    for (int i = 0; i < MAX_ERROR_HISTORY + 5; i++) {
        char message[128];
        snprintf(message, sizeof(message), "Error %d", i);
        
        int status = module_record_error(module, i, LOG_LEVEL_ERROR,
                                        "buffer_test.c", i, "test_function", message);
        assert_condition(status == MODULE_STATUS_SUCCESS, "Error recording failed in loop");
    }
    
    /* Verify buffer overflow flag */
    assert_condition(module->error_context->overflow == true, "Buffer overflow flag not set");
    assert_condition(module->error_context->error_count == MAX_ERROR_HISTORY + 5, 
                    "Error count incorrect");
    
    /* Retrieve the error history */
    module_error_info_t errors[MAX_ERROR_HISTORY];
    int num_errors = 0;
    int status = module_get_error_history(module, errors, MAX_ERROR_HISTORY, &num_errors);
    
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error history retrieval failed");
    assert_condition(num_errors == MAX_ERROR_HISTORY, "Should return MAX_ERROR_HISTORY errors");
    
    /* Verify that we have the most recent errors (not the oldest) */
    for (int i = 0; i < num_errors; i++) {
        char expected[128];
        int error_num = MAX_ERROR_HISTORY + 5 - num_errors + i;
        snprintf(expected, sizeof(expected), "Error %d", error_num);
        
        assert_condition(errors[i].code == error_num, "Error code in history incorrect");
        assert_condition(strcmp(errors[i].message, expected) == 0, 
                        "Error message in history incorrect");
    }
    
    /* Clean up */
    cleanup_test_module(module);
    
    printf("PASSED: Error history circular buffer\n");
    tests_run++;
    return TEST_SUCCESS;
}

/* Test error formatting */
static int test_error_formatting() {
    printf("Testing error formatting...\n");
    
    /* Create an error */
    module_error_info_t error;
    memset(&error, 0, sizeof(error));
    
    error.code = MODULE_STATUS_ERROR;
    error.severity = LOG_LEVEL_ERROR;
    strncpy(error.message, "Test error for formatting", sizeof(error.message) - 1);
    strncpy(error.file, "format_test.c", sizeof(error.file) - 1);
    error.line = 42;
    strncpy(error.function, "test_format", sizeof(error.function) - 1);
    error.timestamp = 1617812345.0;  /* Fixed timestamp for testing */
    error.call_stack_depth = 3;
    error.caller_module_id = 456;
    
    /* Format the error */
    char buffer[512];
    int status = module_format_error(&error, buffer, sizeof(buffer));
    
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error formatting failed");
    
    /* Check that the formatted string contains the key information */
    assert_condition(strstr(buffer, "ERROR") != NULL, "Missing severity in formatted output");
    assert_condition(strstr(buffer, "Test error for formatting") != NULL, 
                    "Missing message in formatted output");
    assert_condition(strstr(buffer, "format_test.c:42") != NULL, 
                    "Missing file/line in formatted output");
    assert_condition(strstr(buffer, "test_format") != NULL, 
                    "Missing function in formatted output");
    assert_condition(strstr(buffer, "Call stack depth: 3") != NULL, 
                    "Missing call stack depth in formatted output");
    
    printf("PASSED: Error formatting\n");
    tests_run++;
    return TEST_SUCCESS;
}

/* Test diagnostic utilities - Simplified test focusing on the options */
static int test_diagnostic_utilities() {
    printf("Testing diagnostic utilities...\n");
    
    /* Initialize diagnostic options */
    module_diagnostic_options_t options;
    int status = module_diagnostic_options_init(&options);
    assert_condition(status == MODULE_STATUS_SUCCESS, "Diagnostic options initialization failed");
    
    /* Verify default options */
    assert_condition(options.include_timestamps == true, "Timestamps should be enabled by default");
    assert_condition(options.include_file_info == true, "File info should be enabled by default");
    assert_condition(options.include_call_stack == true, "Call stack should be enabled by default");
    assert_condition(options.verbose == false, "Verbose mode should be disabled by default");
    assert_condition(options.max_errors > 0, "Max errors should be positive");
    
    /* Create a test module with errors */
    struct base_module *module = create_test_module("DiagnosticsTest");
    module_record_error(module, MODULE_STATUS_ERROR, LOG_LEVEL_ERROR,
                       "diag_test.c", 100, "diag_function", "Diagnostic test error");
    
    /* Format an individual error */
    module_error_info_t error;
    status = module_get_latest_error(module, &error);
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error retrieval failed");
    
    char error_buffer[512];
    status = module_format_error(&error, error_buffer, sizeof(error_buffer));
    assert_condition(status == MODULE_STATUS_SUCCESS, "Error formatting failed");
    assert_condition(strstr(error_buffer, "Diagnostic test error") != NULL, 
                   "Missing error message in formatted output");
    
    /* Clean up */
    cleanup_test_module(module);
    
    printf("PASSED: Diagnostic utilities\n");
    tests_run++;
    return TEST_SUCCESS;
}

/* Main test function */
int main(int argc, char *argv[]) {
    (void)argc; /* Suppress unused parameter warning */
    (void)argv; /* Suppress unused parameter warning */
    
    printf("=== Module Error System Test Suite ===\n\n");
    
    /* Initialize logging system */
    initialize_logging(NULL);
    
    /* Run the tests */
    test_error_context_init_cleanup();
    test_error_recording();
    test_error_history_circular_buffer();
    test_error_formatting();
    test_diagnostic_utilities();
    
    /* Print test summary */
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\nAll Module Error System tests passed successfully!\n");
        return TEST_SUCCESS;
    } else {
        printf("\nSome tests failed. Please review the output.\n");
        return TEST_FAILURE;
    }
}