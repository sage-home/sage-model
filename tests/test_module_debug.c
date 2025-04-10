#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../src/core/core_module_system.h"
#include "../src/core/core_logging.h"
#include "../src/core/core_module_debug.h"

/* Constants for testing */
#define TEST_LOG_FILE "./test_module_debug.log"

/* Function prototypes */
void test_module_debug_init(void);
void test_module_trace_logging(void);
void test_module_trace_retrieval(void);
void cleanup_test_files(void);

/* Main test function */
int main(void) {
    /* Initialize logging */
    initialize_logging(NULL);
    
    printf("\n=== Module Debug System Tests ===\n\n");
    
    /* Initialize the module system */
    int status = module_system_initialize();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Run tests */
    test_module_debug_init();
    test_module_trace_logging();
    test_module_trace_retrieval();
    
    /* Clean up the module system */
    status = module_system_cleanup();
    assert(status == MODULE_STATUS_SUCCESS);
    
    /* Clean up test files */
    cleanup_test_files();
    
    printf("\nAll tests passed!\n");
    return 0;
}

/* Test module debug system initialization */
void test_module_debug_init(void) {
    printf("Testing module debug system initialization...\n");
    
    /* Create debug configuration */
    module_trace_config_t config = {
        .enabled = true,
        .min_level = TRACE_LEVEL_DEBUG,
        .log_to_console = true,
        .log_to_file = true,
        .circular_buffer = true,
        .buffer_size = 100
    };
    
    /* Set log file path */
    strcpy(config.log_file, TEST_LOG_FILE);
    
    /* Initialize the debug system */
    bool result = module_debug_init(&config);
    assert(result);
    
    /* Test disabling/enabling tracing */
    result = module_trace_set_enabled(false);
    assert(result);
    assert(!module_trace_is_enabled());
    
    result = module_trace_set_enabled(true);
    assert(result);
    assert(module_trace_is_enabled());
    
    /* Test changing minimum level */
    result = module_trace_set_min_level(TRACE_LEVEL_WARNING);
    assert(result);
    
    printf("Module debug initialization tests passed.\n");
}

/* Test module trace logging */
void test_module_trace_logging(void) {
    printf("\nTesting module trace logging...\n");
    
    /* Set trace level back to debug for testing */
    bool result = module_trace_set_min_level(TRACE_LEVEL_DEBUG);
    assert(result);
    
    /* Add some trace entries */
    result = MODULE_TRACE_DEBUG(0, "Debug message");
    assert(result);
    
    result = MODULE_TRACE_INFO(1, "Info message with data: %d", 42);
    assert(result);
    
    result = MODULE_TRACE_WARNING(2, "Warning message");
    assert(result);
    
    result = MODULE_TRACE_ERROR(-1, "Error message");
    assert(result);
    
    /* Test function tracing */
    MODULE_TRACE_ENTER(0);
    MODULE_TRACE_EXIT(0);
    MODULE_TRACE_EXIT_STATUS(1, 42);
    
    printf("Module trace logging tests passed.\n");
}

/* Test module trace retrieval */
void test_module_trace_retrieval(void) {
    printf("\nTesting module trace retrieval...\n");
    
    /* Clear log first */
    bool result = module_trace_clear_log();
    assert(result);
    
    /* Add known entries */
    MODULE_TRACE_INFO(0, "Test message 1");
    MODULE_TRACE_INFO(0, "Test message 2");
    MODULE_TRACE_INFO(0, "Test message 3");
    
    /* Retrieve the log */
    module_trace_entry_t entries[10];
    int num_entries = 0;
    
    result = module_trace_get_log(entries, 10, &num_entries);
    assert(result);
    assert(num_entries == 3);
    
    /* Format and print entries */
    for (int i = 0; i < num_entries; i++) {
        char output[MAX_TRACE_ENTRY_LENGTH * 2];
        result = module_trace_format_entry(&entries[i], output, sizeof(output));
        assert(result);
        printf("Entry %d: %s\n", i, output);
    }
    
    /* Write log to file */
    result = module_trace_write_to_file(TEST_LOG_FILE);
    assert(result);
    
    /* Check that the file exists */
    struct stat st;
    int stat_result = stat(TEST_LOG_FILE, &st);
    assert(stat_result == 0);
    assert(st.st_size > 0);
    
    printf("Module trace retrieval tests passed.\n");
    
    /* Cleanup debug system */
    result = module_debug_cleanup();
    assert(result);
}

/* Clean up test files */
void cleanup_test_files(void) {
    /* Remove the log file */
    unlink(TEST_LOG_FILE);
}