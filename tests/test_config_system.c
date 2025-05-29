/**
 * Test suite for SAGE Config System
 * 
 * Tests cover:
 * - Configuration initialization and parsing
 * - JSON validation and error handling
 * - Value retrieval (boolean, integer, double, string)
 * - Nested path access and complex structures
 * - Array and object handling
 * - Type conversion and mismatch handling
 * - Module configuration extraction
 * 
 * This test validates the complete functionality of the JSON configuration system
 * which is critical for module configuration and runtime modularity.
 * 
 * Note: This test intentionally generates error messages during malformed JSON
 * tests to validate error handling. These error messages are expected and do not
 * indicate test failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h> /* For PRId64 format specifier */
#include <math.h>     /* For fabs in double comparison */

#include "core/core_config_system.h"
#include "core/core_logging.h" // Needed for core functionality, not for test output

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions with enhanced reporting
#define TEST_ASSERT(condition, message, ...) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: "); \
        printf(message, ##__VA_ARGS__); \
        printf("\n  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("PASS: "); \
        printf(message, ##__VA_ARGS__); \
        printf("\n"); \
    } \
} while(0)

// Test fixtures (declared as per plan, but not heavily used with global config)
static struct test_context {
    struct config_object *test_config;    // Example: could hold a specific loaded config if not using global
    struct config_object *module_config;  // Example: for isolated module config tests
    struct config_object *invalid_config; // Example: for specific invalid config object tests
    int initialized;
} test_ctx;

// Setup/teardown functions
/**
 * Setup test environment
 * 
 * Creates test directories, initializes the configuration system,
 * and prepares the test context.
 */
static void setup_test_fixtures() {
    // Create test directories if they don't exist
    system("mkdir -p tests/test_output");
    
    // Initialize the test context (though not strictly used with global_config)
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize the logging system (still needed by the config system)
    logging_init(LOG_LEVEL_INFO, NULL); 
    
    // Initialize the configuration system
    int result = config_system_initialize();
    if (result != 0) {
        printf("ERROR: Failed to initialize configuration system in test setup\n");
        // This is a fatal error for the test suite
        exit(EXIT_FAILURE); 
    } else {
        test_ctx.initialized = 1;
        printf("Configuration system initialized successfully for tests\n");
    }
}

/**
 * Teardown test environment
 * 
 * Cleans up the configuration system and releases resources.
 */
static void teardown_test_fixtures() {
    // Clean up the configuration system
    if (test_ctx.initialized) {
        config_system_cleanup();
        printf("Configuration system cleaned up after tests\n");
    }
}

/**
 * Helper function to write test JSON content to files
 *
 * @param filename_suffix Suffix to append to the test filename
 * @param json_content JSON content to write to the file
 * @return 0 on success, -1 on failure
 */
static int write_test_json_file(const char *filename_suffix, const char *json_content) {
    char filename[256];
    snprintf(filename, sizeof(filename), "tests/test_output/test_config_%s.json", filename_suffix);
    
    // Make sure directory exists
    // system("mkdir -p tests/test_output"); // Already done in setup_test_fixtures

    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        printf("ERROR: Failed to open file for writing: %s\n", filename);
        return -1;
    }
    
    size_t json_len = strlen(json_content);
    size_t written = fwrite(json_content, 1, json_len, f);
    fclose(f);
    
    if (written != json_len) {
        printf("ERROR: Failed to write complete JSON to file '%s' (wrote %zu of %zu bytes)\n", 
               filename, written, json_len);
        return -1;
    }
    
    return 0;
}


/**
 * Test: Config Initialization and Parsing
 * 
 * Validates the initialization of the configuration system and its ability
 * to parse both valid and invalid JSON. Tests:
 * - Configuration initialization state
 * - Valid JSON parsing
 * - Invalid JSON detection (unclosed objects, syntax errors)
 * - Configuration saving and reloading
 */
static void test_config_init_and_parse(void) {
    printf("\n=== Testing configuration initialization and parsing ===\n");
    
    TEST_ASSERT(global_config != NULL, "global_config should be initialized by setup");
    if (!global_config) return; // Stop test if global_config is NULL
    TEST_ASSERT(global_config->root != NULL, "global_config->root should be allocated by setup (default config)");
    
    const char *valid_json_content = "{\"name\":\"test_init_parse\",\"value\":42}";
    int write_res = write_test_json_file("init_parse_valid", valid_json_content);
    TEST_ASSERT(write_res == 0, "write_test_json_file for valid JSON should succeed");

    int result = config_load_file("tests/test_output/test_config_init_parse_valid.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed with valid JSON (result=%d, expected=0)", result);
    
    const char *invalid_json_content_unclosed_obj = "{\"name\": \"test\", \"unclosed_object\": {"; 
    write_res = write_test_json_file("init_parse_invalid_unclosed_obj", invalid_json_content_unclosed_obj);
    TEST_ASSERT(write_res == 0, "write_test_json_file for invalid JSON (unclosed obj) should succeed");
    
    result = config_load_file("tests/test_output/test_config_init_parse_invalid_unclosed_obj.json");
    TEST_ASSERT(result != 0, "config_load_file should fail with invalid JSON (unclosed object) (result=%d, expected!=0)", result);

    const char *invalid_json_syntax_unquoted_key = "{name_no_quotes: test_val}";
    write_res = write_test_json_file("init_parse_invalid_syntax_unquoted_key", invalid_json_syntax_unquoted_key);
    TEST_ASSERT(write_res == 0, "write_test_json_file for invalid syntax JSON (unquoted key) should succeed");
    result = config_load_file("tests/test_output/test_config_init_parse_invalid_syntax_unquoted_key.json");
    TEST_ASSERT(result != 0, "config_load_file should fail with invalid JSON (unquoted key) (result=%d, expected!=0)", result);
    
    // Test save and reload (after loading a valid config)
    write_res = write_test_json_file("init_parse_tosave", valid_json_content); // use the known valid one
    TEST_ASSERT(write_res == 0, "write_test_json_file for to_save JSON should succeed");
    result = config_load_file("tests/test_output/test_config_init_parse_tosave.json");
    TEST_ASSERT(result == 0, "config_load_file before save should succeed (result=%d, expected=0)", result);

    if (result == 0) { // Only proceed if load was successful
        result = config_save_file("tests/test_output/saved_config.json", true);
        TEST_ASSERT(result == 0, "config_save_file should succeed (result=%d, expected=0)", result);
        
        // It's better to re-initialize the config system or load into a fresh context
        // to ensure the loaded data is purely from the file.
        // For now, we'll reload into the global one.
        config_system_cleanup();    // Clean current global config
        config_system_initialize(); // Re-initialize to a fresh default state

        result = config_load_file("tests/test_output/saved_config.json");
        TEST_ASSERT(result == 0, "config_load_file should succeed with saved JSON (result=%d, expected=0)", result);
        if (result == 0) {
            const char *name_after_reload = config_get_string("name", "default");
            TEST_ASSERT(name_after_reload != NULL && strcmp(name_after_reload, "test_init_parse") == 0, 
                       "Name should be 'test_init_parse' after reload (got '%s')", 
                       name_after_reload ? name_after_reload : "NULL");
                       
            int value_after_reload = config_get_integer("value", -1);
            TEST_ASSERT(value_after_reload == 42, 
                       "Value should be 42 after reload (got %d)", value_after_reload);
        }
    }
}

/**
 * Test: Basic Value Retrieval
 * 
 * Tests the basic config value retrieval functions for different data types:
 * - String retrieval (config_get_string)
 * - Integer retrieval (config_get_integer)
 * - Double retrieval (config_get_double) 
 * - Boolean retrieval (config_get_boolean)
 * - Defaults when keys don't exist
 * - Direct value access (config_get_value)
 */
static void test_basic_value_retrieval(void) {
    printf("\n=== Testing basic value retrieval ===\n");
    
    const char *json_content = "{"
           "\"string_value\": \"test string\","
           "\"int_value\": 42,"
           "\"double_value\": 3.14159,"
           "\"bool_value\": true,"
           "\"another_bool_false\": false,"
           "\"null_value\": null,"
           "\"nested\": {"
               "\"key1\": \"value1\","
               "\"key2\": 123"
           "}"
           "}";
    int write_res = write_test_json_file("basic_retrieval", json_content);
    TEST_ASSERT(write_res == 0, "write_test_json_file for basic_retrieval should succeed");
    
    int result = config_load_file("tests/test_output/test_config_basic_retrieval.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for retrieval test (result=%d, expected=0)", result);
    if (result != 0) return; // Don't proceed if load failed
    
    const char *str_val = config_get_string("string_value", "default");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "test string") == 0, 
               "config_get_string should return the correct string (got '%s', expected 'test string')", 
               str_val ? str_val : "NULL");
    
    str_val = config_get_string("nonexistent_string", "default_str");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "default_str") == 0, 
               "config_get_string should return default for nonexistent key (got '%s', expected 'default_str')", 
               str_val ? str_val : "NULL");
    
    int int_val = config_get_integer("int_value", -1);
    TEST_ASSERT(int_val == 42, "config_get_integer should return the correct integer (got %d, expected 42)", int_val);
    
    int_val = config_get_integer("nonexistent_int", -1);
    TEST_ASSERT(int_val == -1, "config_get_integer should return default for nonexistent key (got %d, expected -1)", int_val);
    
    double double_val = config_get_double("double_value", -1.0);
    TEST_ASSERT(fabs(double_val - 3.14159) < 1e-5, 
               "config_get_double should return the correct double (got %.6f, expected 3.14159, diff=%.6f)", 
               double_val, fabs(double_val - 3.14159));
    
    double_val = config_get_double("nonexistent_double", -1.0);
    TEST_ASSERT(fabs(double_val - (-1.0)) < 1e-9, 
               "config_get_double should return default for nonexistent key (got %.6f, expected -1.0)", double_val);
    
    bool bool_val = config_get_boolean("bool_value", false);
    TEST_ASSERT(bool_val == true, 
               "config_get_boolean should return the correct boolean (got %s, expected true)", 
               bool_val ? "true" : "false");

    bool_val = config_get_boolean("another_bool_false", true);
    TEST_ASSERT(bool_val == false, 
               "config_get_boolean should return the correct boolean (got %s, expected false)", 
               bool_val ? "true" : "false");
    
    bool_val = config_get_boolean("nonexistent_bool", false);
    TEST_ASSERT(bool_val == false, 
               "config_get_boolean should return default for nonexistent key (got %s, expected false)", 
               bool_val ? "true" : "false");
    
    bool_val = config_get_boolean("int_value", false); 
    TEST_ASSERT(bool_val == true, 
               "config_get_boolean should convert non-zero integer to true (got %s, expected true)", 
               bool_val ? "true" : "false");
    
    const struct config_value *value = config_get_value("nested.key1");
    TEST_ASSERT(value != NULL, "config_get_value should return value for nested path");
    if (value) {
        TEST_ASSERT(value->type == CONFIG_VALUE_STRING, 
                   "config_get_value should return correct type (got %d, expected %d [CONFIG_VALUE_STRING])", 
                   value->type, CONFIG_VALUE_STRING);
                   
        if(value->type == CONFIG_VALUE_STRING) { // Further guard for string access
             TEST_ASSERT(value->u.string != NULL && strcmp(value->u.string, "value1") == 0, 
                        "config_get_value should return correct string value (got '%s', expected 'value1')", 
                        value->u.string ? value->u.string : "NULL");
        }
    }
}

/**
 * Test: Nested Value Access with Paths
 * 
 * Tests access to configuration values using dot-notation paths:
 * - Deep nested structure traversal
 * - Array element access by index
 * - Handling of missing nested paths
 * - Access to objects within arrays
 * 
 * This test validates that the config system can correctly navigate
 * complex nested hierarchies and provide intuitive access patterns.
 */
static void test_nested_value_access(void) {
    printf("\n=== Testing nested value access with paths ===\n");
    
    const char *json_content = "{"
           "\"level1\": {"
               "\"level2\": {"
                   "\"level3\": {"
                       "\"string_value\": \"deeply nested\","
                       "\"int_value\": 42"
                   "},"
                   "\"array\": [1, 2, 3, 4, 5]"
               "}"
           "},"
           "\"array_of_objects\": ["
               "{\"name\": \"item1\", \"value\": 1},"
               "{\"name\": \"item2\", \"value\": 2},"
               "{\"name\": \"item3\", \"value\": 3}"
           "]"
           "}";
    int write_res = write_test_json_file("nested_access", json_content);
    TEST_ASSERT(write_res == 0, "write_test_json_file for nested_access should succeed");

    int result = config_load_file("tests/test_output/test_config_nested_access.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for nested test");
    if (result != 0) return;
    
    const char *str_val = config_get_string("level1.level2.level3.string_value", "default");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "deeply nested") == 0, "Should retrieve deeply nested string value");
    
    int int_val = config_get_integer("level1.level2.level3.int_value", -1);
    TEST_ASSERT(int_val == 42, "Should retrieve deeply nested integer value");
    
    int array_size = config_get_array_size("level1.level2.array");
    TEST_ASSERT(array_size == 5, "Should get correct array size");
    
    const struct config_value *array_element = config_get_array_element("level1.level2.array", 2);
    TEST_ASSERT(array_element != NULL, "Should retrieve array element");
    if (array_element) {
        TEST_ASSERT(array_element->type == CONFIG_VALUE_INTEGER, "Array element should have correct type (integer)");
        TEST_ASSERT(array_element->u.integer == 3, "Array element should have correct value (3)");
    }
    
    array_size = config_get_array_size("array_of_objects");
    TEST_ASSERT(array_size == 3, "Should get correct array of objects size");
    
    array_element = config_get_array_element("array_of_objects", 1);
    TEST_ASSERT(array_element != NULL, "Should retrieve array of objects element");
    if (array_element) {
        TEST_ASSERT(array_element->type == CONFIG_VALUE_OBJECT, "Array element should be an object");
    }
    
    str_val = config_get_string("level1.nonexistent.key", "default_invalid");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "default_invalid") == 0, "Should return default for invalid path");
    
    array_element = config_get_array_element("level1.level2.array", 10);
    TEST_ASSERT(array_element == NULL, "Should return NULL for out of bounds array access");
}

/**
 * Test: Setting Configuration Values
 */
static void test_setting_values(void) {
    LOG_INFO("\n=== Testing setting configuration values ===\n");
    
    const char *json_content = "{"
           "\"existing_string\": \"original value\","
           "\"existing_int\": 123,"
           "\"existing_double\": 3.14,"
           "\"existing_bool\": false,"
           "\"existing_object\": {\"key\": \"value\"}"
           "}";
    int write_res = write_test_json_file("setting_values", json_content);
    TEST_ASSERT(write_res == 0, "write_test_json_file for setting_values should succeed");

    int result = config_load_file("tests/test_output/test_config_setting_values.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for set test");
    if (result != 0) return;
    
    result = config_set_string("new_string", "new value");
    TEST_ASSERT(result == 0, "config_set_string should succeed for new_string");
    const char *str_val = config_get_string("new_string", "default");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "new value") == 0, "New string value should be set correctly");
    
    result = config_set_string("existing_string", "updated value");
    TEST_ASSERT(result == 0, "config_set_string should succeed for existing_string");
    str_val = config_get_string("existing_string", "default");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "updated value") == 0, "Existing string value should be updated");
    
    result = config_set_integer("new_int", 456);
    TEST_ASSERT(result == 0, "config_set_integer should succeed for new_int");
    int int_val = config_get_integer("new_int", -1);
    TEST_ASSERT(int_val == 456, "New integer value should be set correctly");
    
    result = config_set_string("nested.path.to.value", "nested value");
    TEST_ASSERT(result == 0, "config_set_string should succeed for nested path");
    str_val = config_get_string("nested.path.to.value", "default");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "nested value") == 0, "Nested string value should be set correctly");

    result = config_set_double("new_double", 1.2345);
    TEST_ASSERT(result == 0, "config_set_double should succeed");
    double dbl_val = config_get_double("new_double", -1.0);
    TEST_ASSERT(fabs(dbl_val - 1.2345) < 1e-5, "New double value should be set correctly");

    result = config_set_boolean("new_bool", true);
    TEST_ASSERT(result == 0, "config_set_boolean should succeed");
    bool bool_val = config_get_boolean("new_bool", false);
    TEST_ASSERT(bool_val == true, "New boolean value should be set correctly");
}

/**
 * Test: Malformed JSON Handling
 * 
 * Tests detection and rejection of various forms of malformed JSON:
 * - Unclosed objects ('{')
 * - Missing values after colon ('{"key": }')
 * - Unclosed string values ('{"key": "unclosed string')
 * - Extra commas in objects ('{"key": true,}')
 * - Invalid JSON literals ('{"key": invalid_json_literal}')
 * - Unclosed arrays ('[1, 2, 3')
 * - Extra commas in arrays ('{"array": [1, 2, 3,]}')
 *
 * Note: This test intentionally generates error messages during parsing.
 * These error messages are expected and validate the error handling functionality.
 */
static void test_malformed_json(void) {
    printf("\n=== Testing malformed JSON handling ===\n");
    
    const char *test_cases[][2] = {
        {"unclosed_object", "{"}, // Test case 0
        {"missing_value_after_colon", "{\"key\": }"}, // Test case 1
        {"unclosed_string_val", "{\"key\": \"unclosed string"}, // Test case 2
        {"extra_comma_object_end", "{\"key\": true,}"}, // Test case 3
        {"invalid_literal", "{\"key\": invalid_json_literal}"}, // Test case 4
        {"unclosed_array_val", "[1, 2, 3"}, // Test case 5
        {"extra_comma_array_end", "{\"array\": [1, 2, 3,]}"} // Test case 6
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        char suffix[64];
        snprintf(suffix, sizeof(suffix), "malformed_%s", test_cases[i][0]);
        int write_res = write_test_json_file(suffix, test_cases[i][1]);
        TEST_ASSERT(write_res == 0, "write_test_json_file for malformed JSON should succeed");
        
        char filename[256];
        snprintf(filename, sizeof(filename), "tests/test_output/test_config_%s.json", suffix);
        int result = config_load_file(filename);
        
        // Enhanced error reporting for malformed JSON test cases
        if (result == 0) {
            printf("WARNING: Malformed JSON was accepted: '%s'\n", test_cases[i][1]);
        }
        
        TEST_ASSERT(result != 0, "config_load_file should fail with malformed JSON '%s' (result=%d, expected!=0)", 
                   test_cases[i][1], result);
    }
}

/**
 * Test: Missing Values and Default Handling
 * 
 * Tests handling of missing configuration values and default fallbacks:
 * - Missing keys with string defaults
 * - Missing keys with integer defaults
 * - Missing keys with double defaults
 * - Missing keys with boolean defaults
 * - Missing nested keys with defaults
 * 
 * This ensures the config system gracefully handles attempts to access
 * non-existent configuration values by returning appropriate defaults.
 */
static void test_missing_values(void) {
    printf("\n=== Testing missing values and default handling ===\n");
    
    const char *test_json = "{"
        "\"string_value\": \"test\","
        "\"int_value\": 42,"
        "\"double_value\": 3.14,"
        "\"bool_value\": true,"
        "\"empty_object\": {}"
    "}";
    
    int write_res = write_test_json_file("missing_values", test_json);
    TEST_ASSERT(write_res == 0, "write_test_json_file for missing_values should succeed");
    
    int result = config_load_file("tests/test_output/test_config_missing_values.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for defaults test");
    if (result != 0) return;
    
    const char *str_val = config_get_string("nonexistent_str", "default string");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "default string") == 0, "Should return default string for missing key");
    
    int int_val = config_get_integer("nonexistent_int", 123);
    TEST_ASSERT(int_val == 123, "Should return default integer for missing key");
    
    double double_val = config_get_double("nonexistent_dbl", 1.23);
    TEST_ASSERT(fabs(double_val - 1.23) < 1e-9, "Should return default double for missing key");
    
    bool bool_val = config_get_boolean("nonexistent_bool", true);
    TEST_ASSERT(bool_val == true, "Should return default boolean for missing key");
    
    str_val = config_get_string("empty_object.nonexistent_nested", "default nested");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "default nested") == 0, "Should return default for missing nested key");
    
    str_val = config_get_string("level1.level2.nonexistent_deep", "deep default");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "deep default") == 0, "Should return default for missing deep path");
}

/**
 * Test: Type Mismatches
 * 
 * Tests handling of type mismatches when retrieving configuration values:
 * - Requesting string from non-string values
 * - Requesting integer from non-integer values
 * - Requesting boolean from non-boolean values
 * - Requesting array properties from non-array values
 * 
 * This test ensures the config system gracefully handles type mismatches
 * with appropriate conversion or default fallback behavior.
 */
static void test_type_mismatches(void) {
    printf("\n=== Testing type mismatch handling ===\n");
    
    const char *test_json = "{"
        "\"string_value\": \"test\","
        "\"int_value\": 42,"
        "\"double_value\": 3.14,"
        "\"bool_value\": true,"
        "\"object_value\": {\"key\": \"value\"},"
        "\"array_value\": [1, 2, 3]"
    "}";
    
    int write_res = write_test_json_file("type_mismatches", test_json);
    TEST_ASSERT(write_res == 0, "write_test_json_file for type_mismatches should succeed");
    
    int result = config_load_file("tests/test_output/test_config_type_mismatches.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for type test");
    if (result != 0) return;
    
    const char *str_val = config_get_string("int_value", "default_str_from_int");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "default_str_from_int") == 0, "Should return default when getting string from int");
    
    str_val = config_get_string("bool_value", "default_str_from_bool");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "default_str_from_bool") == 0, "Should return default when getting string from bool");
    
    str_val = config_get_string("object_value", "default_str_from_obj");
    TEST_ASSERT(str_val != NULL && strcmp(str_val, "default_str_from_obj") == 0, "Should return default when getting string from object");
    
    int int_val = config_get_integer("string_value", -1); 
    TEST_ASSERT(int_val == 0, "Should convert string 'test' to 0 for int (atoi behavior)");
    
    int_val = config_get_integer("bool_value", -1); 
    TEST_ASSERT(int_val == 1, "Should convert true to 1 for int");
    
    int_val = config_get_integer("object_value", -1);
    TEST_ASSERT(int_val == -1, "Should return default when getting int from object");
    
    int array_size = config_get_array_size("string_value");
    TEST_ASSERT(array_size == -1, "Array size should be -1 for non-array");
    
    const struct config_value *array_element = config_get_array_element("string_value", 0);
    TEST_ASSERT(array_element == NULL, "Array element should be NULL for non-array");
}

/**
 * Test: Module Configuration
 */
static void test_module_configuration(void) {
    LOG_INFO("\n=== Testing module configuration extraction ===\n");
    
    const char *test_json = "{"
        "\"modules\": {"
            "\"discovery_enabled\": true,"
            "\"search_paths\": [\"./src/physics\"],"
            "\"instances\": ["
                "{"
                    "\"name\": \"cooling_module\","
                    "\"enabled\": true,"
                    "\"parameters\": {"
                        "\"cooling_rate\": 1.5,"
                        "\"minimum_temperature\": 10000,"
                        "\"maximum_temperature\": 1e8"
                    "}"
                "},"
                "{"
                    "\"name\": \"infall_module\","
                    "\"enabled\": true,"
                    "\"parameters\": {"
                        "\"infall_timescale\": 2.5,"
                        "\"scaling_factor\": 0.75"
                    "}"
                "}"
            "]"
        "},"
        "\"simulation\": {"
            "\"cosmology\": {"
                "\"omega_m\": 0.3,"
                "\"omega_lambda\": 0.7,"
                "\"hubble\": 0.7"
            "}"
        "}"
    "}";
    
    int write_res = write_test_json_file("module_config", test_json);
    TEST_ASSERT(write_res == 0, "write_test_json_file for module_config should succeed");
    
    int result = config_load_file("tests/test_output/test_config_module_config.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for modules test");
    if (result != 0) return;
    
    bool discovery_enabled = config_get_boolean("modules.discovery_enabled", false);
    TEST_ASSERT(discovery_enabled == true, "Should retrieve modules.discovery_enabled correctly");
    
    int array_size = config_get_array_size("modules.search_paths");
    TEST_ASSERT(array_size == 1, "Should get correct modules.search_paths array size");
    
    const struct config_value *array_element = config_get_array_element("modules.search_paths", 0);
    TEST_ASSERT(array_element != NULL, "Should retrieve search path element");
    if (array_element) {
        TEST_ASSERT(array_element->type == CONFIG_VALUE_STRING, "Search path element should be string");
        if(array_element->type == CONFIG_VALUE_STRING && array_element->u.string != NULL)
            TEST_ASSERT(strcmp(array_element->u.string, "./src/physics") == 0, "Search path should match expected value");
    }
    
    array_size = config_get_array_size("modules.instances");
    TEST_ASSERT(array_size == 2, "Should get correct modules.instances array size");
    
    const struct config_value *module_instance = config_get_array_element("modules.instances", 0);
    TEST_ASSERT(module_instance != NULL, "Should retrieve first module instance");
    if (module_instance && module_instance->type == CONFIG_VALUE_OBJECT) {
        const struct config_value *module_name_val = NULL;
        for (int i = 0; i < module_instance->u.object->count; i++) {
            if (strcmp(module_instance->u.object->entries[i].key, "name") == 0) {
                module_name_val = &module_instance->u.object->entries[i].value;
                break;
            }
        }
        TEST_ASSERT(module_name_val != NULL, "Should find module name in object");
        if (module_name_val && module_name_val->type == CONFIG_VALUE_STRING && module_name_val->u.string != NULL) {
            TEST_ASSERT(strcmp(module_name_val->u.string, "cooling_module") == 0, "Module name should match expected value");
        }
        
        const struct config_value *module_params = NULL;
        for (int i = 0; i < module_instance->u.object->count; i++) {
            if (strcmp(module_instance->u.object->entries[i].key, "parameters") == 0) {
                module_params = &module_instance->u.object->entries[i].value;
                break;
            }
        }
        TEST_ASSERT(module_params != NULL, "Should find module parameters");
        if (module_params) {
            TEST_ASSERT(module_params->type == CONFIG_VALUE_OBJECT, "Module parameters should be an object");
        }
    }
    
    double omega_m = config_get_double("simulation.cosmology.omega_m", 0.0);
    TEST_ASSERT(fabs(omega_m - 0.3) < 1e-9, "Should retrieve simulation.cosmology.omega_m correctly");
    
    double hubble = config_get_double("simulation.cosmology.hubble", 0.0);
    TEST_ASSERT(fabs(hubble - 0.7) < 1e-9, "Should retrieve simulation.cosmology.hubble correctly");
}

/**
 * Test: Array Handling
 */
static void test_array_handling(void) {
    LOG_INFO("\n=== Testing array handling ===\n");
    
    const char *test_json = "{"
        "\"simple_array\": [1, 2, 3, 4, 5],"
        "\"string_array\": [\"one\", \"two\", \"three\"],"
        "\"mixed_array\": [1, \"two\", true, null, 3.14],"
        "\"nested_arrays\": ["
            "[1, 2, 3],"
            "[4, 5, 6],"
            "[7, 8, 9]"
        "],"
        "\"array_of_objects\": ["
            "{\"id\": 1, \"name\": \"item1\"},"
            "{\"id\": 2, \"name\": \"item2\"},"
            "{\"id\": 3, \"name\": \"item3\"}"
        "]"
    "}";
    
    int write_res = write_test_json_file("array_handling", test_json);
    TEST_ASSERT(write_res == 0, "write_test_json_file for array_handling should succeed");
    
    int result = config_load_file("tests/test_output/test_config_array_handling.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for array test");
    if (result != 0) return;
    
    int array_size = config_get_array_size("simple_array");
    TEST_ASSERT(array_size == 5, "Should get correct simple_array size");
    
    for (int i = 0; i < array_size; i++) {
        const struct config_value *element = config_get_array_element("simple_array", i);
        TEST_ASSERT(element != NULL, "Should retrieve simple array element");
        if (element) {
            TEST_ASSERT(element->type == CONFIG_VALUE_INTEGER, "Simple array element should be integer");
            TEST_ASSERT(element->u.integer == i + 1, "Simple array element should have correct value");
        }
    }
    
    array_size = config_get_array_size("string_array");
    TEST_ASSERT(array_size == 3, "Should get correct string_array size");
    
    const char *expected_strings[] = {"one", "two", "three"};
    for (int i = 0; i < array_size; i++) {
        const struct config_value *element = config_get_array_element("string_array", i);
        TEST_ASSERT(element != NULL, "Should retrieve string array element");
        if (element && element->type == CONFIG_VALUE_STRING) {
            TEST_ASSERT(element->u.string != NULL && strcmp(element->u.string, expected_strings[i]) == 0, "String array element should have correct value");
        } else if (element) {
            TEST_ASSERT(false, "String array element has wrong type");
        }
    }
    
    array_size = config_get_array_size("mixed_array");
    TEST_ASSERT(array_size == 5, "Should get correct mixed_array size");
    
    const struct config_value *element = config_get_array_element("mixed_array", 0);
    TEST_ASSERT(element != NULL && element->type == CONFIG_VALUE_INTEGER, "First mixed array element should be integer");
    
    element = config_get_array_element("mixed_array", 1);
    TEST_ASSERT(element != NULL && element->type == CONFIG_VALUE_STRING, "Second mixed array element should be string");
    
    element = config_get_array_element("mixed_array", 2);
    TEST_ASSERT(element != NULL && element->type == CONFIG_VALUE_BOOLEAN, "Third mixed array element should be boolean");
    
    array_size = config_get_array_size("array_of_objects");
    TEST_ASSERT(array_size == 3, "Should get correct array_of_objects size");
    
    for (int i = 0; i < array_size; i++) {
        element = config_get_array_element("array_of_objects", i);
        TEST_ASSERT(element != NULL, "Should retrieve object array element");
        if (element && element->type == CONFIG_VALUE_OBJECT) {
            const struct config_value *id_value = NULL;
            for (int j = 0; j < element->u.object->count; j++) {
                if (strcmp(element->u.object->entries[j].key, "id") == 0) {
                    id_value = &element->u.object->entries[j].value;
                    break;
                }
            }
            TEST_ASSERT(id_value != NULL, "Should find id field in object");
            if (id_value && id_value->type == CONFIG_VALUE_INTEGER) {
                TEST_ASSERT(id_value->u.integer == i + 1, "Id field should have correct value");
            } else if (id_value) {
                 TEST_ASSERT(false, "Id field has wrong type");
            }
        } else if (element) {
            TEST_ASSERT(false, "Object array element has wrong type");
        }
    }
}

/**
 * Test: Complex Nested Structures
 * 
 * Tests handling of complex nested hierarchical structures that mirror
 * real-world SAGE configurations:
 * - Deeply nested simulation parameters
 * - Module configuration with parameters
 * - Array and nested array handling
 * - Multi-level path traversal
 * 
 * This test validates that the configuration system can handle the
 * complex configurations required for SAGE's modular architecture.
 */
static void test_complex_structures(void) {
    printf("\n=== Testing complex nested structures ===\n");
    
    const char *test_json = "{"
        "\"simulation\": {"
            "\"cosmology\": {"
                "\"omega_m\": 0.3,"
                "\"omega_lambda\": 0.7,"
                "\"hubble\": 0.7"
            "},"
            "\"output\": {"
                "\"snapshots\": [0, 1, 21, 63],"
                "\"format\": \"HDF5\","
                "\"properties\": [\"StellarMass\", \"ColdGas\", \"HotGas\"]"
            "}"
        "},"
        "\"modules\": {"
            "\"discovery_enabled\": true,"
            "\"search_paths\": [\"./src/physics\"],"
            "\"instances\": ["
                "{"
                    "\"name\": \"cooling_module\","
                    "\"enabled\": true,"
                    "\"parameters\": {"
                        "\"cooling_rate\": 1.5,"
                        "\"minimum_temperature\": 10000"
                    "}"
                "}"
            "]"
        "}"
    "}";
    
    int write_res = write_test_json_file("complex_structures", test_json);
    TEST_ASSERT(write_res == 0, "write_test_json_file for complex_structures should succeed");
    
    int result = config_load_file("tests/test_output/test_config_complex_structures.json");
    TEST_ASSERT(result == 0, "config_load_file should succeed for complex test");
    if (result != 0) return;
    
    double omega_m = config_get_double("simulation.cosmology.omega_m", 0.0);
    TEST_ASSERT(fabs(omega_m - 0.3) < 1e-9, "Should retrieve deeply nested double value");
    
    const char *format = config_get_string("simulation.output.format", "default");
    TEST_ASSERT(format != NULL && strcmp(format, "HDF5") == 0, "Should retrieve deeply nested string value");
    
    int array_size = config_get_array_size("simulation.output.snapshots");
    TEST_ASSERT(array_size == 4, "Should get correct snapshots array size");
    
    const struct config_value *element = config_get_array_element("simulation.output.snapshots", 2);
    TEST_ASSERT(element != NULL, "Should retrieve snapshots array element");
    if (element) {
        TEST_ASSERT(element->type == CONFIG_VALUE_INTEGER, "Snapshots array element should be integer");
        TEST_ASSERT(element->u.integer == 21, "Snapshots array element should have correct value");
    }
    
    array_size = config_get_array_size("simulation.output.properties");
    TEST_ASSERT(array_size == 3, "Should get correct properties array size");
    
    element = config_get_array_element("simulation.output.properties", 0);
    TEST_ASSERT(element != NULL, "Should retrieve properties array element");
    if (element && element->type == CONFIG_VALUE_STRING) {
        TEST_ASSERT(element->u.string != NULL && strcmp(element->u.string, "StellarMass") == 0, "Properties array element should have correct value");
    } else if (element) {
        TEST_ASSERT(false, "Properties array element has wrong type");
    }
}

//=============================================================================
// Test Runner
//=============================================================================

/**
 * Main test runner
 * 
 * Executes all test cases for the configuration system and reports results.
 */
int main(int argc, char **argv) {
    (void)argc; // Avoid unused parameter warning
    (void)argv; // Avoid unused parameter warning
    
    printf("\n========================================\n");
    printf("Starting tests for test_config_system\n");
    printf("========================================\n\n");
    
    // Setup test fixtures
    setup_test_fixtures();
    
    // Run all tests
    test_config_init_and_parse();
    test_basic_value_retrieval();
    test_nested_value_access();
    test_setting_values();
    test_malformed_json();
    test_missing_values();
    test_type_mismatches();
    test_module_configuration();
    test_array_handling();
    test_complex_structures();
    
    // Teardown test fixtures
    teardown_test_fixtures();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_config_system:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}